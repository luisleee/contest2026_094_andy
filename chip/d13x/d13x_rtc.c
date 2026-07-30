/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_rtc.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include <nuttx/arch.h>
#include <nuttx/clock.h>
#include <nuttx/irq.h>
#include <nuttx/mutex.h>
#include <nuttx/semaphore.h>
#include <nuttx/spinlock.h>

#include <arch/chip/irq.h>

#include <aic_core.h>
#include <aic_io.h>
#include <hal_rtc.h>

#include "d13x_rtc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define D13X_RTC_REG_CTL       0x000u
#define D13X_RTC_REG_INIT      0x004u
#define D13X_RTC_REG_IRQ_EN    0x008u
#define D13X_RTC_REG_IRQ_STA   0x00cu
#define D13X_RTC_REG_TIME0     0x020u
#define D13X_RTC_REG_TCNT      0x800u
#define D13X_RTC_REG_WR_KEY    0x0fcu
#define D13X_RTC_REG_VERSION   0x8fcu

#define D13X_RTC_WRITE_KEY     0xacu
#define D13X_RTC_CTL_ALARM_EN  (1u << 2)
#define D13X_RTC_INIT_PENDING  (1u << 0)

#define D13X_RTC_INIT_POLL_US  100u
#define D13X_RTC_INIT_TIMEOUT_US  100000u

/****************************************************************************
 * Private Data
 ****************************************************************************/

volatile bool g_rtc_enabled;

static sem_t g_d13x_rtc_alarm_sem = SEM_INITIALIZER(0);
static mutex_t g_d13x_rtc_alarm_lock = NXMUTEX_INITIALIZER;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void d13x_rtc_writeb(uint8_t value, uint32_t offset)
{
  writeb(D13X_RTC_WRITE_KEY, RTC_BASE + D13X_RTC_REG_WR_KEY);
  writeb(value, RTC_BASE + offset);
  writeb(0, RTC_BASE + D13X_RTC_REG_WR_KEY);
}

static uint32_t d13x_rtc_read_keep32(uint32_t offset)
{
  return (uint32_t)readb(RTC_BASE + offset) |
         (uint32_t)readb(RTC_BASE + offset + 4u) << 8 |
         (uint32_t)readb(RTC_BASE + offset + 8u) << 16 |
         (uint32_t)readb(RTC_BASE + offset + 12u) << 24;
}

static int d13x_rtc_wait_settime(uint32_t expected)
{
  uint32_t elapsed;
  uint32_t counter;
  uint8_t init;

  for (elapsed = 0; elapsed < D13X_RTC_INIT_TIMEOUT_US;
       elapsed += D13X_RTC_INIT_POLL_US)
    {
      init = readb(RTC_BASE + D13X_RTC_REG_INIT);
      counter = readl(RTC_BASE + D13X_RTC_REG_TCNT);
      if ((init & D13X_RTC_INIT_PENDING) == 0 &&
          (counter == expected ||
           (expected < UINT32_MAX && counter == expected + 1u)))
        {
          return OK;
        }

      up_udelay(D13X_RTC_INIT_POLL_US);
    }

  return (init & D13X_RTC_INIT_PENDING) != 0 ? -ETIMEDOUT : -EIO;
}

static void d13x_rtc_cancel_alarm(void)
{
  uint8_t status;
  uint8_t value;

  value = readb(RTC_BASE + D13X_RTC_REG_CTL);
  value &= ~D13X_RTC_CTL_ALARM_EN;
  d13x_rtc_writeb(value, D13X_RTC_REG_CTL);
  d13x_rtc_writeb(0, D13X_RTC_REG_IRQ_EN);

  status = readb(RTC_BASE + D13X_RTC_REG_IRQ_STA);
  if (status != 0)
    {
      d13x_rtc_writeb(status, D13X_RTC_REG_IRQ_STA);
    }
}

static int d13x_rtc_alarm_callback(void)
{
  nxsem_post(&g_d13x_rtc_alarm_sem);
  return OK;
}

static int d13x_rtc_interrupt(int irq, FAR void *context, FAR void *arg)
{
  (void)context;

  hal_rtc_irq(irq, arg);
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int up_rtc_initialize(void)
{
  int ret;

  g_rtc_enabled = false;
  ret = hal_rtc_init();
  if (ret < 0)
    {
      return -EIO;
    }

  ret = hal_rtc_register_callback(d13x_rtc_alarm_callback);
  if (ret < 0)
    {
      return -EIO;
    }

  d13x_rtc_cancel_alarm();
  ret = irq_attach(D13X_IRQ_RTC, d13x_rtc_interrupt, NULL);
  if (ret < 0)
    {
      return ret;
    }

  up_enable_irq(D13X_IRQ_RTC);
  g_rtc_enabled = true;
  return OK;
}

time_t up_rtc_time(void)
{
  u32 seconds;

  hal_rtc_read_time(&seconds);
  return (time_t)seconds;
}

int up_rtc_settime(FAR const struct timespec *time)
{
  if (time == NULL || time->tv_sec < 0 ||
      (uint64_t)time->tv_sec > UINT32_MAX)
    {
      return -EINVAL;
    }

  hal_rtc_set_time((uint32_t)time->tv_sec);
  return d13x_rtc_wait_settime((uint32_t)time->tv_sec);
}

int d13x_rtc_get_version(FAR uint32_t *version)
{
  u32 value;

  if (version == NULL)
    {
      return -EINVAL;
    }

  value = readl(RTC_BASE + D13X_RTC_REG_VERSION);
  *version = (uint32_t)value;
  return OK;
}

int d13x_rtc_get_state(FAR struct d13x_rtc_state_s *state)
{
  if (state == NULL)
    {
      return -EINVAL;
    }

  state->version = readl(RTC_BASE + D13X_RTC_REG_VERSION);
  state->counter = readl(RTC_BASE + D13X_RTC_REG_TCNT);
  state->time_set = d13x_rtc_read_keep32(D13X_RTC_REG_TIME0);
  state->control = readb(RTC_BASE + D13X_RTC_REG_CTL);
  state->init = readb(RTC_BASE + D13X_RTC_REG_INIT);
  state->irq_enable = readb(RTC_BASE + D13X_RTC_REG_IRQ_EN);
  state->irq_status = readb(RTC_BASE + D13X_RTC_REG_IRQ_STA);
  return OK;
}

int d13x_rtc_alarm_wait(uint32_t delay_seconds, uint32_t timeout_ms)
{
  irqstate_t flags;
  u32 current;
  int ret;

  if (!g_rtc_enabled || delay_seconds == 0 || timeout_ms == 0 ||
      delay_seconds > UINT32_MAX / 1000u)
    {
      return -EINVAL;
    }

  ret = nxmutex_lock(&g_d13x_rtc_alarm_lock);
  if (ret < 0)
    {
      return ret;
    }

  nxsem_reset(&g_d13x_rtc_alarm_sem, 0);
  flags = enter_critical_section();
  d13x_rtc_cancel_alarm();
  hal_rtc_read_time(&current);
  if (current > UINT32_MAX - delay_seconds)
    {
      leave_critical_section(flags);
      nxmutex_unlock(&g_d13x_rtc_alarm_lock);
      return -ERANGE;
    }

  hal_rtc_set_alarm(current + delay_seconds);
  leave_critical_section(flags);

  ret = nxsem_tickwait_uninterruptible(&g_d13x_rtc_alarm_sem,
                                        MSEC2TICK(timeout_ms));

  flags = enter_critical_section();
  d13x_rtc_cancel_alarm();
  leave_critical_section(flags);
  nxmutex_unlock(&g_d13x_rtc_alarm_lock);
  return ret;
}
