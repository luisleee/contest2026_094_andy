/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_dmic.c
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>

#include <nuttx/arch.h>
#include <nuttx/audio/audio.h>
#include <nuttx/irq.h>
#include <nuttx/kthread.h>
#include <nuttx/kmalloc.h>
#include <nuttx/queue.h>
#include <nuttx/semaphore.h>
#include <nuttx/signal.h>

#include <aic_drv_dma.h>
#include <aic_soc.h>
#include <drv_dma.h>
#include <hal_audio.h>
#include <hal_dma.h>

#define D13X_DMIC_DMA_CHANNEL       1
#define D13X_DMIC_BUFFER_BYTES      8192
#define D13X_DMIC_BUFFER_COUNT      2
#define D13X_DMIC_WORKER_PRIORITY   120
#define D13X_DMIC_WORKER_STACKSIZE  2048

#define D13X_DMA_CFG_SRC_DEV(n)     ((uint32_t)(n) << 0)
#define D13X_DMA_CFG_SRC_BURST(n)   ((uint32_t)(n) << 6)
#define D13X_DMA_CFG_SRC_ADDR(n)    ((uint32_t)(n) << 8)
#define D13X_DMA_CFG_SRC_WIDTH(n)   ((uint32_t)(n) << 9)
#define D13X_DMA_CFG_DST_DEV(n)     ((uint32_t)(n) << 16)
#define D13X_DMA_CFG_DST_BURST(n)   ((uint32_t)(n) << 22)
#define D13X_DMA_CFG_DST_ADDR(n)    ((uint32_t)(n) << 24)
#define D13X_DMA_CFG_DST_WIDTH(n)   ((uint32_t)(n) << 25)

#define D13X_AUDIO_RX_DMIC_IF_CTL   (AUDIO_BASE + 0x000)
#define D13X_AUDIO_RX_HPF1_2_CTL    (AUDIO_BASE + 0x004)
#define D13X_AUDIO_RX_DVC1_2_CTL    (AUDIO_BASE + 0x018)
#define D13X_AUDIO_DMIC_RXFIFO_CTL  (AUDIO_BASE + 0x030)
#define D13X_AUDIO_FIFO_INT_EN      (AUDIO_BASE + 0x038)
#define D13X_AUDIO_DMIC_RXFIFO_DATA (AUDIO_BASE + 0x040)
#define D13X_AUDIO_GLOBE_CTL        (AUDIO_BASE + 0x060)

#define D13X_DMIC_ADOUT_SHIFT_EN    (1u << 15)
#define D13X_DMIC_ADOUT_SHIFT_MASK  (7u << 12)
#define D13X_DMIC_ADOUT_SHIFT(n)    ((uint32_t)(n) << 12)
#define D13X_DMIC_DEC2_EN           (1u << 7)
#define D13X_DMIC_DEC1_EN           (1u << 6)
#define D13X_DMIC_DEC_MASK          (3u << 6)
#define D13X_DMIC_IF_EN             (1u << 4)
#define D13X_DMIC_FS_MASK           (7u << 1)
#define D13X_DMIC_FS(n)             ((uint32_t)(n) << 1)
#define D13X_DMIC_CLK_22579KHZ      (1u << 0)

#define D13X_DMIC_HPF2_EN           (1u << 1)
#define D13X_DMIC_HPF1_EN           (1u << 0)
#define D13X_DMIC_HPF_MASK          (3u << 0)

#define D13X_DMIC_DVC2_GAIN(n)      ((uint32_t)(n) << 24)
#define D13X_DMIC_DVC1_GAIN(n)      ((uint32_t)(n) << 16)
#define D13X_DMIC_DVC2_EN           (1u << 1)
#define D13X_DMIC_DVC1_EN           (1u << 0)
#define D13X_DMIC_DVC_MASK          (3u << 0)
#define D13X_DMIC_DVC_GAIN_MASK     (0xffffu << 16)

#define D13X_DMIC_RXFIFO_FLUSH      (1u << 31)
#define D13X_DMIC_RXFIFO_RXTH_MASK  (0xffu << 8)
#define D13X_DMIC_RXFIFO_RXTH(n)    ((uint32_t)(n) << 8)
#define D13X_DMIC_RXFIFO_CH1_EN     (1u << 1)
#define D13X_DMIC_RXFIFO_CH0_EN     (1u << 0)
#define D13X_DMIC_RXFIFO_CH_MASK    (3u << 0)

#define D13X_DMIC_DRQ_EN            (1u << 3)
#define D13X_AUDIO_RX_GLBEN         (1u << 0)

#define D13X_MHCR_DCACHE_ENABLE     (1u << 1)

struct d13x_dmic_s
{
  struct audio_lowerhalf_s dev;
  hal_audio_handle_t audio;
  struct aic_dma_chan_s dma;
  hal_dma_task_desc_t tasks[D13X_DMIC_BUFFER_COUNT]
    __attribute__((aligned(CACHE_LINE_SIZE)));
  struct dq_queue_s pendq;
  sem_t worker_sem;
  FAR struct ap_buffer_s *active;
  FAR uint8_t *alloc_addr;
  uint32_t samplerate;
  uint32_t buffers_completed;
  uint32_t peak_sequence;
  uint8_t peak_percent;
  uint8_t channels;
  uint8_t samplebits;
  uint8_t alloc_index;
  bool started;
  bool reserved;
  bool xrun;
  bool audio_ready;
  bool dma_ready;
  bool worker_wakeup;
  bool stopping;
  bool paused;
};

static struct d13x_dmic_s g_d13x_dmic;

static bool d13x_dmic_dcache_enabled(void)
{
  uint32_t mhcr;

  __asm__ __volatile__ ("csrr %0, 0x7c1" : "=r"(mhcr));
  return (mhcr & D13X_MHCR_DCACHE_ENABLE) != 0;
}

static void d13x_dmic_cache_clean_invalidate(uintptr_t address,
                                              size_t length)
{
  uintptr_t end;

  if (!d13x_dmic_dcache_enabled() || length == 0)
    {
      return;
    }

  end = (address + length + CACHE_LINE_SIZE - 1) &
        ~(uintptr_t)(CACHE_LINE_SIZE - 1);
  address &= ~(uintptr_t)(CACHE_LINE_SIZE - 1);

  __asm__ __volatile__ ("fence rw, rw" ::: "memory");
  while (address < end)
    {
      register uintptr_t cache_line __asm__("a5") = address;

      __asm__ __volatile__ (".long 0x02b7800b"
                            : : "r"(cache_line) : "memory");
      address += CACHE_LINE_SIZE;
    }

  __asm__ __volatile__ ("fence rw, rw" ::: "memory");
}

static void d13x_dmic_cache_invalidate(uintptr_t address, size_t length)
{
  uintptr_t end;

  if (!d13x_dmic_dcache_enabled() || length == 0)
    {
      return;
    }

  end = (address + length + CACHE_LINE_SIZE - 1) &
        ~(uintptr_t)(CACHE_LINE_SIZE - 1);
  address &= ~(uintptr_t)(CACHE_LINE_SIZE - 1);

  __asm__ __volatile__ ("fence rw, rw" ::: "memory");
  while (address < end)
    {
      register uintptr_t cache_line __asm__("a5") = address;

      __asm__ __volatile__ (".long 0x02a7800b"
                            : : "r"(cache_line) : "memory");
      address += CACHE_LINE_SIZE;
    }

  __asm__ __volatile__ ("fence rw, rw" ::: "memory");
}

static void d13x_dmic_kick_worker(FAR struct d13x_dmic_s *priv)
{
  irqstate_t flags;
  bool wakeup = false;

  flags = enter_critical_section();
  if (!priv->worker_wakeup)
    {
      priv->worker_wakeup = true;
      wakeup = true;
    }

  leave_critical_section(flags);
  if (wakeup)
    {
      nxsem_post(&priv->worker_sem);
    }
}

static int d13x_dmic_hw_configure(uint32_t samplerate, uint8_t channels)
{
  uint32_t rate;
  uint32_t value;

  switch (samplerate)
    {
      case 48000:
        rate = 0;
        break;
      case 32000:
        rate = 1;
        break;
      case 24000:
        rate = 2;
        break;
      case 16000:
        rate = 3;
        break;
      case 12000:
        rate = 4;
        break;
      case 8000:
        rate = 5;
        break;
      default:
        return -EINVAL;
    }

  value = readl(D13X_AUDIO_RX_DMIC_IF_CTL);
  value &= ~(D13X_DMIC_CLK_22579KHZ | D13X_DMIC_FS_MASK |
             D13X_DMIC_DEC_MASK);
  value |= D13X_DMIC_FS(rate) | D13X_DMIC_IF_EN | D13X_DMIC_DEC1_EN;
  if (channels == 2)
    {
      value |= D13X_DMIC_DEC2_EN;
    }

  writel(value, D13X_AUDIO_RX_DMIC_IF_CTL);

  value = readl(D13X_AUDIO_RX_HPF1_2_CTL);
  value &= ~D13X_DMIC_HPF_MASK;
  value |= D13X_DMIC_HPF1_EN;
  if (channels == 2)
    {
      value |= D13X_DMIC_HPF2_EN;
    }

  writel(value, D13X_AUDIO_RX_HPF1_2_CTL);

  value = readl(D13X_AUDIO_RX_DVC1_2_CTL);
  value &= ~(D13X_DMIC_DVC_GAIN_MASK | D13X_DMIC_DVC_MASK);
  value |= D13X_DMIC_DVC1_GAIN(0xa0) | D13X_DMIC_DVC1_EN;
  if (channels == 2)
    {
      value |= D13X_DMIC_DVC2_GAIN(0xa0) | D13X_DMIC_DVC2_EN;
    }

  writel(value, D13X_AUDIO_RX_DVC1_2_CTL);

  value = readl(D13X_AUDIO_DMIC_RXFIFO_CTL);
  value &= ~(D13X_DMIC_RXFIFO_RXTH_MASK | D13X_DMIC_RXFIFO_CH_MASK);
  value |= D13X_DMIC_RXFIFO_RXTH(8) | D13X_DMIC_RXFIFO_CH0_EN;
  if (channels == 2)
    {
      value |= D13X_DMIC_RXFIFO_CH1_EN;
    }

  writel(value, D13X_AUDIO_DMIC_RXFIFO_CTL);
  return OK;
}

static void d13x_dmic_hw_start(void)
{
  uint32_t value;

  value = readl(D13X_AUDIO_DMIC_RXFIFO_CTL);
  writel(value | D13X_DMIC_RXFIFO_FLUSH,
         D13X_AUDIO_DMIC_RXFIFO_CTL);

  value = readl(D13X_AUDIO_RX_DMIC_IF_CTL);
  value &= ~D13X_DMIC_ADOUT_SHIFT_MASK;
  value |= D13X_DMIC_ADOUT_SHIFT_EN | D13X_DMIC_ADOUT_SHIFT(3);
  writel(value, D13X_AUDIO_RX_DMIC_IF_CTL);

  value = readl(D13X_AUDIO_GLOBE_CTL);
  writel(value | D13X_AUDIO_RX_GLBEN, D13X_AUDIO_GLOBE_CTL);

  value = readl(D13X_AUDIO_FIFO_INT_EN);
  writel(value | D13X_DMIC_DRQ_EN, D13X_AUDIO_FIFO_INT_EN);
}

static void d13x_dmic_hw_stop(void)
{
  uint32_t value;

  value = readl(D13X_AUDIO_FIFO_INT_EN);
  writel(value & ~D13X_DMIC_DRQ_EN, D13X_AUDIO_FIFO_INT_EN);

  value = readl(D13X_AUDIO_GLOBE_CTL);
  writel(value & ~D13X_AUDIO_RX_GLBEN, D13X_AUDIO_GLOBE_CTL);

  value = readl(D13X_AUDIO_RX_DMIC_IF_CTL);
  writel(value & ~D13X_DMIC_IF_EN, D13X_AUDIO_RX_DMIC_IF_CTL);

  value = readl(D13X_AUDIO_DMIC_RXFIFO_CTL);
  writel(value | D13X_DMIC_RXFIFO_FLUSH,
         D13X_AUDIO_DMIC_RXFIFO_CTL);
}

static void d13x_dmic_complete(FAR struct d13x_dmic_s *priv,
                               FAR struct ap_buffer_s *apb, int status)
{
  if (apb == NULL)
    {
      return;
    }

  apb_free(apb);
  priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_DEQUEUE, apb, status);
}

static void d13x_dmic_finish(FAR struct d13x_dmic_s *priv,
                             FAR struct ap_buffer_s *apb, int status)
{
  irqstate_t flags;

  if (priv->dma_ready)
    {
      hal_dma_deinit(&priv->dma.hal);
    }

  if (priv->audio_ready)
    {
      hal_audio_deinit(&priv->audio);
    }

  while (apb != NULL)
    {
      apb->curbyte = 0;
      apb->nbytes = 0;
      d13x_dmic_complete(priv, apb, status);

      flags = enter_critical_section();
      apb = (FAR struct ap_buffer_s *)dq_remfirst(&priv->pendq);
      leave_critical_section(flags);
    }

  flags = enter_critical_section();
  priv->stopping = false;
  priv->audio_ready = false;
  priv->dma_ready = false;
  leave_critical_section(flags);

  if (status < 0)
    {
      priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_IOERR, NULL, status);
    }

  priv->dev.upper(priv->dev.priv, AUDIO_CALLBACK_COMPLETE, NULL, status);
}

static void d13x_dmic_prepare_buffer(FAR struct d13x_dmic_s *priv,
                                     FAR struct ap_buffer_s *apb)
{
  FAR uint32_t *source;
  FAR int16_t *sink;
  uint32_t peak = 0;
  uint32_t samples;
  uint32_t index;
  irqstate_t flags;

  d13x_dmic_cache_invalidate((uintptr_t)apb->samp, apb->nmaxbytes);
  apb->curbyte = 0;
  apb->nbytes = apb->nmaxbytes;

  if (priv->channels == 1)
    {
      source = (FAR uint32_t *)apb->samp;
      sink = (FAR int16_t *)apb->samp;
      samples = apb->nmaxbytes / sizeof(uint32_t);
      for (index = 0; index < samples; index++)
        {
          sink[index] = (int16_t)source[index];
        }

      apb->nbytes = samples * sizeof(int16_t);
    }

  sink = (FAR int16_t *)apb->samp;
  samples = apb->nbytes / sizeof(int16_t);
  for (index = 0; index < samples; index++)
    {
      uint32_t magnitude;

      magnitude = sink[index] < 0 ? -(int32_t)sink[index] : sink[index];
      if (magnitude > peak)
        {
          peak = magnitude;
        }
    }

  flags = enter_critical_section();
  priv->peak_percent = (uint8_t)((peak * 100u) / 32768u);
  if (priv->peak_percent == 0 && peak != 0)
    {
      priv->peak_percent = 1;
    }

  priv->peak_sequence++;
  leave_critical_section(flags);
}

static int d13x_dmic_start_dma(FAR struct d13x_dmic_s *priv)
{
  FAR hal_dma_handle_t *hdma = &priv->dma.hal;
  FAR hal_dma_task_desc_t *task = priv->tasks;
  uint32_t config;
  uint32_t value;
  unsigned int index;

  if (priv->alloc_addr == NULL ||
      priv->alloc_index != D13X_DMIC_BUFFER_COUNT)
    {
      return -EAGAIN;
    }

  d13x_dmic_cache_clean_invalidate(
    (uintptr_t)priv->alloc_addr,
    D13X_DMIC_BUFFER_BYTES * D13X_DMIC_BUFFER_COUNT);

  config = D13X_DMA_CFG_SRC_DEV(HAL_DMA_ID_AUDIO) |
           D13X_DMA_CFG_SRC_BURST(DMA_XFER_BURST_1) |
           D13X_DMA_CFG_SRC_ADDR(DMA_ADDR_FIXED_MODE) |
           D13X_DMA_CFG_SRC_WIDTH(DMA_DATA_WIDTH_4_BYTES) |
           D13X_DMA_CFG_DST_DEV(HAL_DMA_ID_SRAM) |
           D13X_DMA_CFG_DST_BURST(DMA_XFER_BURST_1) |
           D13X_DMA_CFG_DST_ADDR(DMA_ADDR_LINEAR_MODE) |
           D13X_DMA_CFG_DST_WIDTH(DMA_DATA_WIDTH_4_BYTES);

  memset(task, 0, sizeof(priv->tasks));
  for (index = 0; index < D13X_DMIC_BUFFER_COUNT; index++)
    {
      task[index].cfg.val = config;
      task[index].src = D13X_AUDIO_DMIC_RXFIFO_DATA;
      task[index].dst = (uint32_t)priv->alloc_addr +
                        index * D13X_DMIC_BUFFER_BYTES;
      task[index].len = D13X_DMIC_BUFFER_BYTES;
      task[index].delay = hdma->init.delay;
      task[index].next = (uint32_t)&task[
        (index + 1) % D13X_DMIC_BUFFER_COUNT];
    }

  d13x_dmic_cache_clean_invalidate((uintptr_t)task, sizeof(priv->tasks));

  writel(0, DMA_CH_EN_REG(hdma, D13X_DMIC_DMA_CHANNEL));
  writel(0, DMA_CH_PAUSE_REG(hdma, D13X_DMIC_DMA_CHANNEL));
  writel(DMA_MODE_DMA_SRC_MODE_MASK,
         DMA_MODE_REG(hdma, D13X_DMIC_DMA_CHANNEL));

  value = readl(DMA_IRQ_EN_REG(hdma));
  value &= ~DMA_IRQ_EN_CH_MASK(D13X_DMIC_DMA_CHANNEL);
  writel(value, DMA_IRQ_EN_REG(hdma));
  writel(DMA_IRQ_STA_CH_MASK(D13X_DMIC_DMA_CHANNEL),
         DMA_IRQ_STA_REG(hdma));

  syslog(LOG_INFO,
         "[D13X] DMIC DMA task0=%08lx cfg=%08lx dst=%08lx next=%08lx\n",
         (unsigned long)&task[0], (unsigned long)task[0].cfg.val,
         (unsigned long)task[0].dst, (unsigned long)task[0].next);
  syslog(LOG_INFO,
         "[D13X] DMIC DMA task1=%08lx cfg=%08lx dst=%08lx next=%08lx\n",
         (unsigned long)&task[1], (unsigned long)task[1].cfg.val,
         (unsigned long)task[1].dst, (unsigned long)task[1].next);

  writel((uint32_t)&task[0],
         DMA_CH_TASK_REG(hdma, D13X_DMIC_DMA_CHANNEL));
  writel(1, DMA_CH_EN_REG(hdma, D13X_DMIC_DMA_CHANNEL));
  hdma->errcode = DMA_ERROR_NONE;
  hdma->state = DMA_STATE_BUSY;
  return OK;
}

static hal_status_e d13x_dmic_poll_buffer(FAR struct d13x_dmic_s *priv)
{
  FAR hal_dma_handle_t *hdma = &priv->dma.hal;
  hal_status_e status;

  status = hal_dma_poll_for_transfer(hdma, DMA_COMPELE_TYPE_FULL_TASK, 1);
  if (status == HAL_OK)
    {
      writel(DMA_IRQ_STA_CH_MASK(D13X_DMIC_DMA_CHANNEL),
             DMA_BASE + DMA_IRQ_STA);
    }

  return status;
}

static int d13x_dmic_worker(int argc, FAR char *argv[])
{
  FAR struct d13x_dmic_s *priv = &g_d13x_dmic;
  FAR struct ap_buffer_s *apb;
  hal_status_e status;
  irqstate_t flags;
  bool started;
  bool stopping;

  UNUSED(argc);
  UNUSED(argv);

  for (;;)
    {
      nxsem_wait_uninterruptible(&priv->worker_sem);

      for (;;)
        {
          flags = enter_critical_section();
          priv->worker_wakeup = false;
          started = priv->started;
          stopping = priv->stopping;
          if (!started)
            {
              apb = NULL;
              if (stopping)
                {
                  apb = priv->active;
                  priv->active = NULL;
                  if (apb == NULL)
                    {
                      apb = (FAR struct ap_buffer_s *)
                            dq_remfirst(&priv->pendq);
                    }
                }
            }
          else if (priv->active == NULL)
            {
              apb = (FAR struct ap_buffer_s *)dq_remfirst(&priv->pendq);
              priv->active = apb;
            }
          else
            {
              apb = priv->active;
            }

          leave_critical_section(flags);

          if (!started)
            {
              if (stopping)
                {
                  d13x_dmic_finish(priv, apb, OK);
                }

              break;
            }

          if (apb == NULL)
            {
              flags = enter_critical_section();
              priv->xrun = true;
              leave_critical_section(flags);
              break;
            }

          status = hal_dma_get_state(&priv->dma.hal) == DMA_STATE_BUSY ?
                   HAL_OK : HAL_ERROR;

          while (status == HAL_OK || status == HAL_TIMEOUT)
            {
              status = d13x_dmic_poll_buffer(priv);

              flags = enter_critical_section();
              started = priv->started;
              leave_critical_section(flags);
              if (!started || status != HAL_TIMEOUT)
                {
                  break;
                }

              nxsig_usleep(1000);
            }

          flags = enter_critical_section();
          started = priv->started;
          priv->active = NULL;
          leave_critical_section(flags);

          if (status == HAL_OK && started)
            {
              d13x_dmic_prepare_buffer(priv, apb);
              priv->buffers_completed++;
              if (priv->buffers_completed == 1)
                {
                  syslog(LOG_INFO,
                         "[D13X] DMIC first DMA buffer captured\n");
                }

              d13x_dmic_complete(priv, apb, OK);
              continue;
            }

          if (!started)
            {
              d13x_dmic_finish(priv, apb, OK);
              break;
            }

          syslog(LOG_ERR, "[D13X] DMIC DMA failed: state=%u error=%u\n",
                 (unsigned int)hal_dma_get_state(&priv->dma.hal),
                 (unsigned int)hal_dma_get_error(&priv->dma.hal));
          syslog(LOG_ERR,
                 "[D13X] DMA irq=%08lx task=%08lx former=%08lx "
                 "pkg=%08lx src=%08lx dst=%08lx left=%08lx "
                 "cfg=%08lx mode=%08lx\n",
                 (unsigned long)readl(DMA_BASE + DMA_IRQ_STA),
                 (unsigned long)readl(DMA_BASE +
                                      DMA_CH_TASK(D13X_DMIC_DMA_CHANNEL)),
                 (unsigned long)readl(DMA_BASE +
                                      DMA_FDES_ADDR(D13X_DMIC_DMA_CHANNEL)),
                 (unsigned long)readl(DMA_BASE +
                                      DMA_PKG_NUM(D13X_DMIC_DMA_CHANNEL)),
                 (unsigned long)readl(DMA_BASE +
                                      DMA_SRC_ADDR(D13X_DMIC_DMA_CHANNEL)),
                 (unsigned long)readl(DMA_BASE +
                                      DMA_SINK_ADDR(D13X_DMIC_DMA_CHANNEL)),
                 (unsigned long)readl(DMA_BASE +
                                      DMA_BCNT_LEFT(D13X_DMIC_DMA_CHANNEL)),
                 (unsigned long)readl(DMA_BASE +
                                      DMA_CH_CFG(D13X_DMIC_DMA_CHANNEL)),
                 (unsigned long)readl(DMA_BASE +
                                      DMA_MODE(D13X_DMIC_DMA_CHANNEL)));
          d13x_dmic_hw_stop();
          flags = enter_critical_section();
          priv->started = false;
          priv->stopping = true;
          leave_critical_section(flags);
          hal_dma_abort(&priv->dma.hal);
          d13x_dmic_finish(priv, apb, -EIO);
          break;
        }
    }

  return OK;
}

static int d13x_dmic_getcaps(FAR struct audio_lowerhalf_s *dev, int type,
                             FAR struct audio_caps_s *caps)
{
  FAR struct d13x_dmic_s *priv = (FAR struct d13x_dmic_s *)dev;

  caps->ac_format.hw = 0;
  caps->ac_controls.w = 0;
  if (caps->ac_type == AUDIO_TYPE_QUERY &&
      caps->ac_subtype == AUDIO_TYPE_QUERY)
    {
      caps->ac_channels = 0x12;
      caps->ac_controls.b[0] = AUDIO_TYPE_INPUT;
      caps->ac_format.hw = 1 << (AUDIO_FMT_PCM - 1);
    }
  else if (caps->ac_type == AUDIO_TYPE_INPUT &&
           caps->ac_subtype == AUDIO_TYPE_QUERY)
    {
      caps->ac_channels = 0x12;
      caps->ac_controls.hw[0] = AUDIO_SAMP_RATE_8K | AUDIO_SAMP_RATE_16K |
                                AUDIO_SAMP_RATE_32K | AUDIO_SAMP_RATE_48K;
      caps->ac_controls.b[2] = priv->samplebits;
    }

  return caps->ac_len;
}

static int d13x_dmic_configure(FAR struct audio_lowerhalf_s *dev,
                               FAR const struct audio_caps_s *caps)
{
  FAR struct d13x_dmic_s *priv = (FAR struct d13x_dmic_s *)dev;
  uint32_t samplerate;
  uint8_t channels;
  uint8_t samplebits;

  if (caps->ac_type != AUDIO_TYPE_INPUT)
    {
      return -ENOTTY;
    }

  samplerate = caps->ac_controls.hw[0] |
               ((uint32_t)caps->ac_controls.b[3] << 16);
  channels = caps->ac_channels & 0x0f;
  samplebits = caps->ac_controls.b[2];
  if ((channels != 1 && channels != 2) || samplebits != 16 ||
      (samplerate != 8000 && samplerate != 12000 &&
       samplerate != 16000 && samplerate != 24000 &&
       samplerate != 32000 && samplerate != 48000))
    {
      return -EINVAL;
    }

  priv->samplerate = samplerate;
  priv->channels = channels;
  priv->samplebits = samplebits;
  return OK;
}

static int d13x_dmic_shutdown(FAR struct audio_lowerhalf_s *dev);

static int d13x_dmic_start(FAR struct audio_lowerhalf_s *dev)
{
  FAR struct d13x_dmic_s *priv = (FAR struct d13x_dmic_s *)dev;
  FAR hal_dma_handle_t *hdma = &priv->dma.hal;
  FAR struct ap_buffer_s *apb;
  irqstate_t flags;

  if (priv->started)
    {
      return OK;
    }

  hal_audio_handle_init(&priv->audio);
  priv->audio.init.samplebits = AUDIO_SAMPLEBITS_16BIT;
  priv->audio.init.samplerate = priv->samplerate;
  priv->audio.init.channel = priv->channels;
  if (hal_audio_init(&priv->audio) != HAL_OK)
    {
      syslog(LOG_ERR, "[D13X] DMIC AUDIO initialization failed\n");
      hal_audio_deinit(&priv->audio);
      return -EIO;
    }

  priv->audio_ready = true;
  if (d13x_dmic_hw_configure(priv->samplerate, priv->channels) < 0)
    {
      syslog(LOG_ERR, "[D13X] DMIC hardware configuration failed\n");
      hal_audio_deinit(&priv->audio);
      priv->audio_ready = false;
      return -EIO;
    }

  hdma->init.src_data_width = DMA_DATA_WIDTH_4_BYTES;
  hdma->init.snk_data_width = DMA_DATA_WIDTH_4_BYTES;
  hdma->work_mode = DMA_WORK_MODE_CYCLIC;
  hdma->cyclic_period_len = D13X_DMIC_BUFFER_BYTES;
  if (hal_dma_init(hdma) != HAL_OK)
    {
      syslog(LOG_ERR, "[D13X] DMIC DMA initialization failed: %u\n",
             (unsigned int)hal_dma_get_error(hdma));
      hal_audio_deinit(&priv->audio);
      priv->audio_ready = false;
      return -EIO;
    }

  priv->dma_ready = true;
  flags = enter_critical_section();
  apb = (FAR struct ap_buffer_s *)dq_remfirst(&priv->pendq);
  priv->active = apb;
  leave_critical_section(flags);
  if (apb == NULL || d13x_dmic_start_dma(priv) < 0)
    {
      flags = enter_critical_section();
      priv->active = NULL;
      if (apb != NULL)
        {
          dq_addfirst(&apb->dq_entry, &priv->pendq);
        }

      leave_critical_section(flags);
      hal_dma_deinit(hdma);
      hal_audio_deinit(&priv->audio);
      priv->dma_ready = false;
      priv->audio_ready = false;
      return apb == NULL ? -EAGAIN : -EIO;
    }

  priv->started = true;
  priv->paused = false;
  priv->xrun = false;
  priv->buffers_completed = 0;
  priv->peak_percent = 0;
  d13x_dmic_hw_start();
  d13x_dmic_kick_worker(priv);
  return OK;
}

#ifndef CONFIG_AUDIO_EXCLUDE_STOP
static int d13x_dmic_stop(FAR struct audio_lowerhalf_s *dev)
{
  FAR struct d13x_dmic_s *priv = (FAR struct d13x_dmic_s *)dev;
  irqstate_t flags;
  bool audio_ready;
  bool dma_ready;

  flags = enter_critical_section();
  if (priv->stopping)
    {
      leave_critical_section(flags);
      return OK;
    }

  if (!priv->started && !priv->audio_ready && !priv->dma_ready)
    {
      leave_critical_section(flags);
      return OK;
    }

  priv->started = false;
  audio_ready = priv->audio_ready;
  dma_ready = priv->dma_ready;
  priv->stopping = true;
  leave_critical_section(flags);

  if (audio_ready)
    {
      d13x_dmic_hw_stop();
    }

  if (dma_ready)
    {
      hal_dma_abort(&priv->dma.hal);
    }

  d13x_dmic_kick_worker(priv);
  return OK;
}
#endif

#ifndef CONFIG_AUDIO_EXCLUDE_PAUSE_RESUME
static int d13x_dmic_pause(FAR struct audio_lowerhalf_s *dev)
{
  FAR struct d13x_dmic_s *priv = (FAR struct d13x_dmic_s *)dev;
  FAR hal_dma_handle_t *hdma = &priv->dma.hal;
  irqstate_t flags;
  uint32_t value;

  flags = enter_critical_section();
  if (!priv->started || !priv->dma_ready)
    {
      leave_critical_section(flags);
      return -EIO;
    }

  if (priv->paused)
    {
      leave_critical_section(flags);
      return OK;
    }

  priv->paused = true;
  leave_critical_section(flags);

  writel(1, DMA_CH_PAUSE_REG(hdma, D13X_DMIC_DMA_CHANNEL));
  value = readl(D13X_AUDIO_FIFO_INT_EN);
  writel(value & ~D13X_DMIC_DRQ_EN, D13X_AUDIO_FIFO_INT_EN);
  value = readl(D13X_AUDIO_GLOBE_CTL);
  writel(value & ~D13X_AUDIO_RX_GLBEN, D13X_AUDIO_GLOBE_CTL);
  return OK;
}

static int d13x_dmic_resume(FAR struct audio_lowerhalf_s *dev)
{
  FAR struct d13x_dmic_s *priv = (FAR struct d13x_dmic_s *)dev;
  FAR hal_dma_handle_t *hdma = &priv->dma.hal;
  irqstate_t flags;
  uint32_t value;

  flags = enter_critical_section();
  if (!priv->started || !priv->dma_ready)
    {
      leave_critical_section(flags);
      return -EIO;
    }

  if (!priv->paused)
    {
      leave_critical_section(flags);
      return OK;
    }

  priv->paused = false;
  leave_critical_section(flags);

  writel(0, DMA_CH_PAUSE_REG(hdma, D13X_DMIC_DMA_CHANNEL));
  value = readl(D13X_AUDIO_GLOBE_CTL);
  writel(value | D13X_AUDIO_RX_GLBEN, D13X_AUDIO_GLOBE_CTL);
  value = readl(D13X_AUDIO_FIFO_INT_EN);
  writel(value | D13X_DMIC_DRQ_EN, D13X_AUDIO_FIFO_INT_EN);
  return OK;
}
#endif

static int d13x_dmic_shutdown(FAR struct audio_lowerhalf_s *dev)
{
#ifndef CONFIG_AUDIO_EXCLUDE_STOP
  return d13x_dmic_stop(dev);
#else
  return OK;
#endif
}

static int d13x_dmic_allocbuffer(FAR struct audio_lowerhalf_s *dev,
                                 FAR struct audio_buf_desc_s *bufdesc)
{
  FAR struct d13x_dmic_s *priv = (FAR struct d13x_dmic_s *)dev;
  FAR struct ap_buffer_s *apb;

  if (bufdesc->numbytes != D13X_DMIC_BUFFER_BYTES ||
      priv->alloc_index >= D13X_DMIC_BUFFER_COUNT)
    {
      return -EINVAL;
    }

  if (priv->alloc_addr == NULL)
    {
      priv->alloc_addr = kumm_memalign(32, D13X_DMIC_BUFFER_BYTES *
                                       D13X_DMIC_BUFFER_COUNT);
      if (priv->alloc_addr == NULL)
        {
          return -ENOMEM;
        }
    }

  apb = kumm_zalloc(sizeof(*apb));
  if (apb == NULL)
    {
      return -ENOMEM;
    }

  apb->i.channels = priv->channels;
  apb->crefs = 1;
  apb->nmaxbytes = D13X_DMIC_BUFFER_BYTES;
  apb->samp = priv->alloc_addr +
              priv->alloc_index * D13X_DMIC_BUFFER_BYTES;
  nxmutex_init(&apb->lock);
  priv->alloc_index++;
  *bufdesc->u.pbuffer = apb;
  return sizeof(*bufdesc);
}

static int d13x_dmic_freebuffer(FAR struct audio_lowerhalf_s *dev,
                                FAR struct audio_buf_desc_s *bufdesc)
{
  FAR struct d13x_dmic_s *priv = (FAR struct d13x_dmic_s *)dev;
  FAR struct ap_buffer_s *apb = bufdesc->u.buffer;

  if (apb == NULL)
    {
      return -EINVAL;
    }

  nxmutex_destroy(&apb->lock);
  kumm_free(apb);
  if (priv->alloc_index > 0)
    {
      priv->alloc_index--;
    }

  if (priv->alloc_index == 0)
    {
      kumm_free(priv->alloc_addr);
      priv->alloc_addr = NULL;
    }

  return sizeof(*bufdesc);
}

static int d13x_dmic_enqueuebuffer(FAR struct audio_lowerhalf_s *dev,
                                   FAR struct ap_buffer_s *apb)
{
  FAR struct d13x_dmic_s *priv = (FAR struct d13x_dmic_s *)dev;
  irqstate_t flags;
  bool started;
  bool stopping;

  apb_reference(apb);
  apb->curbyte = 0;
  apb->nbytes = apb->nmaxbytes;

  flags = enter_critical_section();
  stopping = priv->stopping;
  if (stopping)
    {
      leave_critical_section(flags);
      apb_free(apb);
      return -ESHUTDOWN;
    }

  dq_addlast(&apb->dq_entry, &priv->pendq);
  started = priv->started;
  priv->xrun = false;
  leave_critical_section(flags);
  if (started)
    {
      d13x_dmic_kick_worker(priv);
    }

  return OK;
}

static int d13x_dmic_ioctl(FAR struct audio_lowerhalf_s *dev, int cmd,
                           unsigned long arg)
{
  FAR struct ap_buffer_info_s *info;

  if (cmd == AUDIOIOC_GETBUFFERINFO)
    {
      info = (FAR struct ap_buffer_info_s *)arg;
      info->buffer_size = D13X_DMIC_BUFFER_BYTES;
      info->nbuffers = D13X_DMIC_BUFFER_COUNT;
      return OK;
    }

  return -ENOTTY;
}

static int d13x_dmic_reserve(FAR struct audio_lowerhalf_s *dev)
{
  FAR struct d13x_dmic_s *priv = (FAR struct d13x_dmic_s *)dev;
  irqstate_t flags;

  flags = enter_critical_section();
  if (priv->reserved)
    {
      leave_critical_section(flags);
      return -EBUSY;
    }

  priv->reserved = true;
  leave_critical_section(flags);
  return OK;
}

static int d13x_dmic_release(FAR struct audio_lowerhalf_s *dev)
{
  FAR struct d13x_dmic_s *priv = (FAR struct d13x_dmic_s *)dev;

  priv->reserved = false;
  return OK;
}

static const struct audio_ops_s g_d13x_dmic_ops =
{
  .getcaps = d13x_dmic_getcaps,
  .configure = d13x_dmic_configure,
  .shutdown = d13x_dmic_shutdown,
  .start = d13x_dmic_start,
#ifndef CONFIG_AUDIO_EXCLUDE_STOP
  .stop = d13x_dmic_stop,
#endif
#ifndef CONFIG_AUDIO_EXCLUDE_PAUSE_RESUME
  .pause = d13x_dmic_pause,
  .resume = d13x_dmic_resume,
#endif
  .allocbuffer = d13x_dmic_allocbuffer,
  .freebuffer = d13x_dmic_freebuffer,
  .enqueuebuffer = d13x_dmic_enqueuebuffer,
  .ioctl = d13x_dmic_ioctl,
  .reserve = d13x_dmic_reserve,
  .release = d13x_dmic_release,
};

FAR struct audio_lowerhalf_s *aic_dmic_initialize(void)
{
  FAR struct d13x_dmic_s *priv = &g_d13x_dmic;
  FAR hal_dma_handle_t *hdma;

  memset(priv, 0, sizeof(*priv));
  if (aic_dma_initialize() == NULL)
    {
      return NULL;
    }

  priv->dev.ops = &g_d13x_dmic_ops;
  priv->samplerate = 16000;
  priv->channels = 1;
  priv->samplebits = 16;
  dq_init(&priv->pendq);
  nxsem_init(&priv->worker_sem, 0, 0);

  hdma = &priv->dma.hal;
  hal_dma_handle_init(hdma);
  hdma->regbase = DMA_BASE;
  hdma->parent = priv;
  hdma->work_mode = DMA_WORK_MODE_NORMAL;
  hdma->init.channel_id = D13X_DMIC_DMA_CHANNEL;
  hdma->init.direction = DMA_DEVICE_TO_MEMORY;
  hdma->init.src_dev = HAL_DMA_ID_AUDIO;
  hdma->init.src_mode = DMA_MODE_HANDSHAKE;
  hdma->init.src_burst = DMA_XFER_BURST_1;
  hdma->init.src_addr_mode = DMA_ADDR_FIXED_MODE;
  hdma->init.src_data_width = DMA_DATA_WIDTH_4_BYTES;
  hdma->init.snk_dev = HAL_DMA_ID_SRAM;
  hdma->init.snk_mode = DMA_MODE_WAIT;
  hdma->init.snk_burst = DMA_XFER_BURST_1;
  hdma->init.snk_addr_mode = DMA_ADDR_LINEAR_MODE;
  hdma->init.snk_data_width = DMA_DATA_WIDTH_4_BYTES;
  hdma->init.flag = HAL_HANDLE_ALL_INIT_FLAG;
  if (aic_dma_chan_register(&priv->dma.chan) < 0)
    {
      nxsem_destroy(&priv->worker_sem);
      return NULL;
    }

  if (kthread_create("d13x_dmic", D13X_DMIC_WORKER_PRIORITY,
                     D13X_DMIC_WORKER_STACKSIZE, d13x_dmic_worker,
                     NULL) < 0)
    {
      aic_dma_chan_unregister(&priv->dma.chan);
      nxsem_destroy(&priv->worker_sem);
      return NULL;
    }

  return &priv->dev;
}

int d13x_dmic_get_peak(FAR uint32_t *sequence, FAR uint8_t *peak_percent)
{
  FAR struct d13x_dmic_s *priv = &g_d13x_dmic;
  irqstate_t flags;

  if (sequence == NULL || peak_percent == NULL)
    {
      return -EINVAL;
    }

  flags = enter_critical_section();
  *sequence = priv->peak_sequence;
  *peak_percent = priv->peak_percent;
  leave_critical_section(flags);
  return OK;
}
