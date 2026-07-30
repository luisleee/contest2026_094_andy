/****************************************************************************
 * contest2026_094_andy/board/d13x/demo88-nor/src/d13x_keyadc.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <nuttx/arch.h>
#include <nuttx/input/buttons.h>
#include <nuttx/irq.h>
#include <nuttx/kthread.h>
#include <nuttx/semaphore.h>
#include <nuttx/signal.h>
#include <nuttx/spinlock.h>

#include <arch/chip/chip.h>

#include "board.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define D13X_KEYADC_CHANNEL             2
#define D13X_KEYADC_SUPPORTED           0x0f
#define D13X_KEYADC_POLL_US             10000
#define D13X_KEYADC_STACKSIZE           2048
#define D13X_KEYADC_STABLE_SAMPLES      3
#define D13X_KEYADC_INIT_RETRIES         3
#define D13X_KEYADC_CONVERSION_TIMEOUT  1000
#define D13X_KEYADC_CAL_SAMPLES         6

#define D13X_KEYADC_UP_MAX              736
#define D13X_KEYADC_DOWN_MAX            1309
#define D13X_KEYADC_LEFT_MAX            1932
#define D13X_KEYADC_RIGHT_MAX           2702

#define D13X_CMU_ADCIM                  (CMU_BASE + 0x09a0)
#define D13X_CMU_GPAI                   (CMU_BASE + 0x09a4)
#define D13X_CMU_DIV_MASK               0x1f
#define D13X_CMU_ADCIM_DIV              24
#define D13X_CMU_MOD_CLK_EN             (1u << 8)
#define D13X_CMU_BUS_CLK_EN             (1u << 12)
#define D13X_CMU_RESET_RELEASE          (1u << 13)

#define D13X_ADCIM_CALCSR               (ADCIM_BASE + 0x004)
#define D13X_ADCIM_CAL_START            0x08002f03
#define D13X_ADCIM_CAL_BUSY             (1u << 0)
#define D13X_ADCIM_CAL_SHIFT            16
#define D13X_ADCIM_CAL_MASK             0x0fff
#define D13X_ADCIM_CAL_STANDARD         0x0800
#define D13X_ADCIM_CAL_OFFSET           0x0028

#define D13X_GPAI_MCR                   (GPAI_BASE + 0x000)
#define D13X_GPAI_INTR                  (GPAI_BASE + 0x004)
#define D13X_GPAI_CH_BASE(ch)           (GPAI_BASE + 0x100 + (ch) * 0x40)
#define D13X_GPAI_CH_CR(ch)             (D13X_GPAI_CH_BASE(ch) + 0x00)
#define D13X_GPAI_CH_INT(ch)            (D13X_GPAI_CH_BASE(ch) + 0x04)
#define D13X_GPAI_CH_FCR(ch)            (D13X_GPAI_CH_BASE(ch) + 0x20)
#define D13X_GPAI_CH_DATA(ch)           (D13X_GPAI_CH_BASE(ch) + 0x24)

#define D13X_GPAI_MCR_ENABLE            (1u << 0)
#define D13X_GPAI_MCR_CH_ENABLE(ch)     (1u << (8 + (ch)))
#define D13X_GPAI_INTR_CH_ENABLE(ch)    (1u << (ch))
#define D13X_GPAI_CR_AVG_8              (3u << 24)
#define D13X_GPAI_CR_ADC_ACQ            (0x2fu << 8)
#define D13X_GPAI_CR_SINGLE             (1u << 0)
#define D13X_GPAI_INT_DRDY_ENABLE       (1u << 0)
#define D13X_GPAI_INT_FIFOERR_ENABLE    (1u << 1)
#define D13X_GPAI_INT_DRDY_FLAG         (1u << 16)
#define D13X_GPAI_FCR_COUNT_SHIFT       24
#define D13X_GPAI_FCR_COUNT_MASK        (0x7fu << 24)
#define D13X_GPAI_FCR_THRESHOLD         (1u << 8)
#define D13X_GPAI_FCR_FLUSH             (1u << 0)
#define D13X_GPAI_DATA_MASK             0x0fff

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct d13x_keyadc_s
{
  struct btn_lowerhalf_s lower;
  sem_t waitsem;
  btn_handler_t handler;
  FAR void *arg;
  volatile btn_buttonset_t buttons;
  volatile uint16_t raw;
  uint16_t calibration;
  bool enabled;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static btn_buttonset_t
  d13x_keyadc_supported(FAR const struct btn_lowerhalf_s *lower);
static btn_buttonset_t
  d13x_keyadc_buttons(FAR const struct btn_lowerhalf_s *lower);
static void d13x_keyadc_enable(FAR const struct btn_lowerhalf_s *lower,
                              btn_buttonset_t press,
                              btn_buttonset_t release,
                              btn_handler_t handler, FAR void *arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct d13x_keyadc_s g_keyadc =
{
  .lower =
    {
      d13x_keyadc_supported,
      d13x_keyadc_buttons,
      d13x_keyadc_enable,
      NULL
    },
  .raw = D13X_GPAI_DATA_MASK,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline uint32_t d13x_keyadc_getreg(uintptr_t address)
{
  return *(FAR volatile uint32_t *)address;
}

static inline void d13x_keyadc_putreg(uint32_t value, uintptr_t address)
{
  *(FAR volatile uint32_t *)address = value;
}

static int d13x_keyadc_calibrate(FAR uint16_t *calibration)
{
  uint32_t values[D13X_KEYADC_CAL_SAMPLES];
  uint32_t value;
  uint32_t total = 0;
  uint32_t minimum = UINT32_MAX;
  uint32_t maximum = 0;
  unsigned int sample;
  unsigned int timeout;

  for (sample = 0; sample < D13X_KEYADC_CAL_SAMPLES; sample++)
    {
      d13x_keyadc_putreg(D13X_ADCIM_CAL_START, D13X_ADCIM_CALCSR);
      for (timeout = 0; timeout < D13X_KEYADC_CONVERSION_TIMEOUT; timeout++)
        {
          value = d13x_keyadc_getreg(D13X_ADCIM_CALCSR);
          if ((value & D13X_ADCIM_CAL_BUSY) == 0)
            {
              break;
            }

          up_udelay(1);
        }

      if (timeout >= D13X_KEYADC_CONVERSION_TIMEOUT)
        {
          syslog(LOG_ERR,
                 "[D13X] ADCIM calibration timeout: sample=%u "
                 "cmu=%08lx calcsr=%08lx\n",
                 sample, (unsigned long)d13x_keyadc_getreg(D13X_CMU_ADCIM),
                 (unsigned long)d13x_keyadc_getreg(D13X_ADCIM_CALCSR));
          return -ETIMEDOUT;
        }

      values[sample] = (value >> D13X_ADCIM_CAL_SHIFT) &
                       D13X_ADCIM_CAL_MASK;
      total += values[sample];
      if (values[sample] < minimum)
        {
          minimum = values[sample];
        }

      if (values[sample] > maximum)
        {
          maximum = values[sample];
        }
    }

  *calibration = (total - minimum - maximum) /
                 (D13X_KEYADC_CAL_SAMPLES - 2);
  return OK;
}

static int d13x_keyadc_hardware_initialize(FAR struct d13x_keyadc_s *dev)
{
  uint32_t reg;
  int ret;

  reg = d13x_keyadc_getreg(D13X_CMU_ADCIM);
  reg &= ~D13X_CMU_DIV_MASK;
  reg |= D13X_CMU_ADCIM_DIV | D13X_CMU_MOD_CLK_EN |
         D13X_CMU_BUS_CLK_EN | D13X_CMU_RESET_RELEASE;
  d13x_keyadc_putreg(reg, D13X_CMU_ADCIM);

  reg = d13x_keyadc_getreg(D13X_CMU_GPAI);
  reg |= D13X_CMU_BUS_CLK_EN | D13X_CMU_RESET_RELEASE;
  d13x_keyadc_putreg(reg, D13X_CMU_GPAI);

  up_udelay(10);

  ret = d13x_keyadc_calibrate(&dev->calibration);
  if (ret < 0)
    {
      return ret;
    }

  reg = d13x_keyadc_getreg(D13X_GPAI_MCR);
  reg |= D13X_GPAI_MCR_ENABLE |
         D13X_GPAI_MCR_CH_ENABLE(D13X_KEYADC_CHANNEL);
  d13x_keyadc_putreg(reg, D13X_GPAI_MCR);

  d13x_keyadc_putreg(D13X_GPAI_FCR_THRESHOLD,
                     D13X_GPAI_CH_FCR(D13X_KEYADC_CHANNEL));
  d13x_keyadc_putreg(D13X_GPAI_CR_AVG_8 | D13X_GPAI_CR_ADC_ACQ,
                     D13X_GPAI_CH_CR(D13X_KEYADC_CHANNEL));
  d13x_keyadc_putreg(D13X_GPAI_INT_DRDY_ENABLE |
                     D13X_GPAI_INT_FIFOERR_ENABLE,
                     D13X_GPAI_CH_INT(D13X_KEYADC_CHANNEL));

  reg = d13x_keyadc_getreg(D13X_GPAI_INTR);
  reg |= D13X_GPAI_INTR_CH_ENABLE(D13X_KEYADC_CHANNEL);
  d13x_keyadc_putreg(reg, D13X_GPAI_INTR);

  return OK;
}

static int d13x_keyadc_sample(FAR struct d13x_keyadc_s *dev,
                             FAR uint16_t *sample)
{
  uint32_t count;
  uint32_t raw;
  uint32_t reg;
  uint32_t total = 0;
  int calibrated;
  unsigned int i;
  unsigned int timeout;

  reg = d13x_keyadc_getreg(D13X_GPAI_CH_INT(D13X_KEYADC_CHANNEL));
  d13x_keyadc_putreg(reg, D13X_GPAI_CH_INT(D13X_KEYADC_CHANNEL));

  reg = d13x_keyadc_getreg(D13X_GPAI_CH_CR(D13X_KEYADC_CHANNEL));
  reg &= ~((3u << 24) | (0xffu << 8));
  reg |= D13X_GPAI_CR_AVG_8 | D13X_GPAI_CR_ADC_ACQ;
  d13x_keyadc_putreg(reg, D13X_GPAI_CH_CR(D13X_KEYADC_CHANNEL));

  reg = d13x_keyadc_getreg(D13X_GPAI_CH_CR(D13X_KEYADC_CHANNEL));
  d13x_keyadc_putreg(reg | D13X_GPAI_CR_SINGLE,
                     D13X_GPAI_CH_CR(D13X_KEYADC_CHANNEL));

  for (timeout = 0; timeout < D13X_KEYADC_CONVERSION_TIMEOUT; timeout++)
    {
      reg = d13x_keyadc_getreg(D13X_GPAI_CH_INT(D13X_KEYADC_CHANNEL));
      if ((reg & D13X_GPAI_INT_DRDY_FLAG) != 0)
        {
          break;
        }

      up_udelay(1);
    }

  if (timeout >= D13X_KEYADC_CONVERSION_TIMEOUT)
    {
      return -ETIMEDOUT;
    }

  count = (d13x_keyadc_getreg(D13X_GPAI_CH_FCR(D13X_KEYADC_CHANNEL)) &
           D13X_GPAI_FCR_COUNT_MASK) >> D13X_GPAI_FCR_COUNT_SHIFT;
  if (count == 0 || count > 8)
    {
      d13x_keyadc_putreg(reg,
                         D13X_GPAI_CH_INT(D13X_KEYADC_CHANNEL));
      reg = d13x_keyadc_getreg(D13X_GPAI_CH_FCR(D13X_KEYADC_CHANNEL));
      d13x_keyadc_putreg(reg | D13X_GPAI_FCR_FLUSH,
                         D13X_GPAI_CH_FCR(D13X_KEYADC_CHANNEL));
      return -EIO;
    }

  for (i = 0; i < count; i++)
    {
      total += d13x_keyadc_getreg(
                 D13X_GPAI_CH_DATA(D13X_KEYADC_CHANNEL)) &
               D13X_GPAI_DATA_MASK;
    }

  d13x_keyadc_putreg(reg, D13X_GPAI_CH_INT(D13X_KEYADC_CHANNEL));
  raw = total / count;
  calibrated = (int)raw + D13X_ADCIM_CAL_STANDARD - dev->calibration +
               D13X_ADCIM_CAL_OFFSET;
  if (calibrated < 0)
    {
      calibrated = 0;
    }
  else if (calibrated > D13X_GPAI_DATA_MASK)
    {
      calibrated = D13X_GPAI_DATA_MASK;
    }

  *sample = calibrated;
  return OK;
}

static btn_buttonset_t d13x_keyadc_classify(uint16_t raw)
{
  if (raw <= D13X_KEYADC_UP_MAX)
    {
      return DPAD_UP_BIT;
    }
  else if (raw <= D13X_KEYADC_DOWN_MAX)
    {
      return DPAD_DOWN_BIT;
    }
  else if (raw <= D13X_KEYADC_LEFT_MAX)
    {
      return DPAD_LEFT_BIT;
    }
  else if (raw <= D13X_KEYADC_RIGHT_MAX)
    {
      return DPAD_RIGHT_BIT;
    }

  return 0;
}

static int d13x_keyadc_worker(int argc, FAR char *argv[])
{
  FAR struct d13x_keyadc_s *dev = &g_keyadc;
  btn_buttonset_t candidate = 0;
  btn_buttonset_t buttons;
  btn_handler_t handler;
  FAR void *arg;
  irqstate_t flags;
  uint16_t raw;
  unsigned int stable = 0;
  int ret;

  (void)argc;
  (void)argv;

  for (; ; )
    {
      flags = enter_critical_section();
      if (!dev->enabled)
        {
          leave_critical_section(flags);
          nxsem_wait_uninterruptible(&dev->waitsem);
          candidate = dev->buttons;
          stable = 0;
          continue;
        }

      leave_critical_section(flags);
      ret = d13x_keyadc_sample(dev, &raw);
      if (ret >= 0)
        {
          dev->raw = raw;
          buttons = d13x_keyadc_classify(raw);
          if (buttons == candidate)
            {
              if (stable < D13X_KEYADC_STABLE_SAMPLES)
                {
                  stable++;
                }
            }
          else
            {
              candidate = buttons;
              stable = 1;
            }

          if (stable == D13X_KEYADC_STABLE_SAMPLES &&
              candidate != dev->buttons)
            {
              dev->buttons = candidate;
              flags = enter_critical_section();
              handler = dev->enabled ? dev->handler : NULL;
              arg = dev->arg;
              leave_critical_section(flags);
              if (handler != NULL)
                {
                  handler(&dev->lower, arg);
                }
            }
        }

      nxsig_usleep(D13X_KEYADC_POLL_US);
    }

  return OK;
}

static btn_buttonset_t
d13x_keyadc_supported(FAR const struct btn_lowerhalf_s *lower)
{
  (void)lower;
  return D13X_KEYADC_SUPPORTED;
}

static btn_buttonset_t
d13x_keyadc_buttons(FAR const struct btn_lowerhalf_s *lower)
{
  FAR const struct d13x_keyadc_s *dev =
    (FAR const struct d13x_keyadc_s *)lower;

  return dev->buttons;
}

static void d13x_keyadc_enable(FAR const struct btn_lowerhalf_s *lower,
                              btn_buttonset_t press,
                              btn_buttonset_t release,
                              btn_handler_t handler, FAR void *arg)
{
  FAR struct d13x_keyadc_s *dev = (FAR struct d13x_keyadc_s *)lower;
  irqstate_t flags;
  bool enable;

  enable = handler != NULL && ((press | release) &
                               D13X_KEYADC_SUPPORTED) != 0;
  flags = enter_critical_section();
  dev->handler = enable ? handler : NULL;
  dev->arg = enable ? arg : NULL;
  dev->enabled = enable;
  leave_critical_section(flags);

  if (enable)
    {
      nxsem_post(&dev->waitsem);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int d13x_keyadc_register(FAR const char *devpath)
{
  FAR struct d13x_keyadc_s *dev = &g_keyadc;
  uint16_t raw;
  unsigned int attempt;
  int ret;

  if (devpath == NULL)
    {
      return -EINVAL;
    }

  ret = d13x_keyadc_hardware_initialize(dev);
  if (ret < 0)
    {
      syslog(LOG_ERR,
             "[D13X] GPAI2 initial conversion failed: %d "
             "cmu=%08lx mcr=%08lx intr=%08lx cr=%08lx "
             "chint=%08lx fcr=%08lx\n",
             ret, (unsigned long)d13x_keyadc_getreg(D13X_CMU_GPAI),
             (unsigned long)d13x_keyadc_getreg(D13X_GPAI_MCR),
             (unsigned long)d13x_keyadc_getreg(D13X_GPAI_INTR),
             (unsigned long)d13x_keyadc_getreg(
               D13X_GPAI_CH_CR(D13X_KEYADC_CHANNEL)),
             (unsigned long)d13x_keyadc_getreg(
               D13X_GPAI_CH_INT(D13X_KEYADC_CHANNEL)),
             (unsigned long)d13x_keyadc_getreg(
               D13X_GPAI_CH_FCR(D13X_KEYADC_CHANNEL)));
      return ret;
    }

  for (attempt = 0; attempt < D13X_KEYADC_INIT_RETRIES; attempt++)
    {
      ret = d13x_keyadc_sample(dev, &raw);
      if (ret >= 0)
        {
          break;
        }

      up_udelay(100);
    }

  if (ret < 0)
    {
      return ret;
    }

  dev->raw = raw;
  dev->buttons = d13x_keyadc_classify(raw);
  syslog(LOG_INFO, "[D13X] GPAI2 calibration=%u initial=%u\n",
         dev->calibration, raw);

  nxsem_init(&dev->waitsem, 0, 0);
  ret = kthread_create("dpad_poll", SCHED_PRIORITY_DEFAULT,
                       D13X_KEYADC_STACKSIZE, d13x_keyadc_worker, NULL);
  if (ret < 0)
    {
      nxsem_destroy(&dev->waitsem);
      return ret;
    }

  ret = btn_register(devpath, &dev->lower);
  if (ret < 0)
    {
      syslog(LOG_ERR, "[D13X] failed to bind key ADC upper half: %d\n",
             ret);
    }

  return ret;
}

int d13x_keyadc_last_raw(FAR uint16_t *raw)
{
  if (raw == NULL)
    {
      return -EINVAL;
    }

  *raw = g_keyadc.raw;
  return OK;
}
