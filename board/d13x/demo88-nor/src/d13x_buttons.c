/****************************************************************************
 * contest2026_094_andy/board/d13x/demo88-nor/src/d13x_buttons.c
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
#include <syslog.h>

#include <nuttx/arch.h>
#include <nuttx/board.h>
#include <nuttx/clock.h>
#include <nuttx/irq.h>
#include <nuttx/semaphore.h>
#include <nuttx/signal.h>
#include <nuttx/spinlock.h>

#include <arch/chip/irq.h>

#include <aic_hal_gpio.h>

#include "board.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define D13X_WAKEUP_GPIO_GROUP  3
#define D13X_WAKEUP_GPIO_PIN    15
#define D13X_WAKEUP_GPIO_MASK   (1u << D13X_WAKEUP_GPIO_PIN)
#define D13X_WAKEUP_GPIO_FUNC   1
#define D13X_WAKEUP_RELEASE_MS  5000
#define D13X_WAKEUP_POLL_US     10000

/****************************************************************************
 * Private Data
 ****************************************************************************/

static xcpt_t g_wakeup_handler;
static FAR void *g_wakeup_arg;
#ifdef CONFIG_D13X_WAKEUP_PM
static sem_t g_wakeup_pm_sem = SEM_INITIALIZER(0);
static bool g_wakeup_pm_armed;
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int d13x_wakeup_interrupt(int irq, FAR void *context, FAR void *arg)
{
  unsigned int enabled;
  unsigned int status;
  xcpt_t handler;

  (void)arg;

  if (hal_gpio_group_get_irq_stat(D13X_WAKEUP_GPIO_GROUP, &status) >= 0 &&
      hal_gpio_group_get_irq_en(D13X_WAKEUP_GPIO_GROUP, &enabled) >= 0 &&
      (status & enabled & D13X_WAKEUP_GPIO_MASK) != 0)
    {
      hal_gpio_clr_irq_stat(D13X_WAKEUP_GPIO_GROUP,
                            D13X_WAKEUP_GPIO_PIN);
      handler = g_wakeup_handler;
      if (handler != NULL)
        {
          return handler(irq, context, g_wakeup_arg);
        }
    }

  return OK;
}

#ifdef CONFIG_D13X_WAKEUP_PM
static int d13x_wakeup_pm_interrupt(int irq, FAR void *context,
                                    FAR void *arg)
{
  (void)irq;
  (void)context;
  (void)arg;

  nxsem_post(&g_wakeup_pm_sem);
  return OK;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

uint32_t board_button_initialize(void)
{
  int ret;

  hal_gpio_set_func(D13X_WAKEUP_GPIO_GROUP, D13X_WAKEUP_GPIO_PIN,
                    D13X_WAKEUP_GPIO_FUNC);
  hal_gpio_set_bias_pull(D13X_WAKEUP_GPIO_GROUP, D13X_WAKEUP_GPIO_PIN,
                         PIN_PULL_UP);
  hal_gpio_direction_input(D13X_WAKEUP_GPIO_GROUP, D13X_WAKEUP_GPIO_PIN);
  hal_gpio_set_irq_mode(D13X_WAKEUP_GPIO_GROUP, D13X_WAKEUP_GPIO_PIN,
                        PIN_IRQ_MODE_EDGE_BOTH);
  hal_gpio_disable_irq(D13X_WAKEUP_GPIO_GROUP, D13X_WAKEUP_GPIO_PIN);
  hal_gpio_clr_irq_stat(D13X_WAKEUP_GPIO_GROUP, D13X_WAKEUP_GPIO_PIN);
  up_disable_irq(D13X_IRQ_GPIOD);

  ret = irq_attach(D13X_IRQ_GPIOD, d13x_wakeup_interrupt, NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "[D13X] failed to attach WAKEUP IRQ: %d\n", ret);
      return 0;
    }

  return NUM_BUTTONS;
}

uint32_t board_buttons(void)
{
  unsigned int value;

  if (hal_gpio_get_value(D13X_WAKEUP_GPIO_GROUP, D13X_WAKEUP_GPIO_PIN,
                         &value) < 0)
    {
      return 0;
    }

  return value == 0 ? BUTTON_WAKEUP_BIT : 0;
}

#ifdef CONFIG_ARCH_IRQBUTTONS
int board_button_irq(int id, xcpt_t irqhandler, FAR void *arg)
{
  int ret = OK;

  if (id != BUTTON_WAKEUP)
    {
      return -EINVAL;
    }

#ifdef CONFIG_D13X_WAKEUP_PM
  if (g_wakeup_pm_armed)
    {
      return -EBUSY;
    }
#endif

  hal_gpio_disable_irq(D13X_WAKEUP_GPIO_GROUP, D13X_WAKEUP_GPIO_PIN);
  up_disable_irq(D13X_IRQ_GPIOD);
  hal_gpio_clr_irq_stat(D13X_WAKEUP_GPIO_GROUP, D13X_WAKEUP_GPIO_PIN);
  g_wakeup_handler = NULL;
  g_wakeup_arg = NULL;

  if (irqhandler != NULL)
    {
      g_wakeup_arg = arg;
      g_wakeup_handler = irqhandler;
      ret = hal_gpio_enable_irq(D13X_WAKEUP_GPIO_GROUP,
                                D13X_WAKEUP_GPIO_PIN);
      if (ret >= 0)
        {
          up_enable_irq(D13X_IRQ_GPIOD);
        }
      else
        {
          g_wakeup_handler = NULL;
          g_wakeup_arg = NULL;
        }
    }

  return ret;
}
#endif

#ifdef CONFIG_D13X_WAKEUP_PM
int d13x_wakeup_pm_arm(void)
{
  irqstate_t flags;
  unsigned int value;
  unsigned int elapsed;
  int ret;

  for (elapsed = 0; elapsed < D13X_WAKEUP_RELEASE_MS;
       elapsed += D13X_WAKEUP_POLL_US / 1000)
    {
      if ((board_buttons() & BUTTON_WAKEUP_BIT) == 0)
        {
          break;
        }

      nxsig_usleep(D13X_WAKEUP_POLL_US);
    }

  if ((board_buttons() & BUTTON_WAKEUP_BIT) != 0)
    {
      return -EBUSY;
    }

  flags = enter_critical_section();
  if (g_wakeup_handler != NULL || g_wakeup_pm_armed)
    {
      leave_critical_section(flags);
      return -EBUSY;
    }

  hal_gpio_disable_irq(D13X_WAKEUP_GPIO_GROUP, D13X_WAKEUP_GPIO_PIN);
  up_disable_irq(D13X_IRQ_GPIOD);
  hal_gpio_clr_irq_stat(D13X_WAKEUP_GPIO_GROUP, D13X_WAKEUP_GPIO_PIN);
  nxsem_reset(&g_wakeup_pm_sem, 0);
  ret = hal_gpio_set_irq_mode(D13X_WAKEUP_GPIO_GROUP,
                              D13X_WAKEUP_GPIO_PIN,
                              PIN_IRQ_MODE_EDGE_FALLING);
  if (ret >= 0)
    {
      g_wakeup_arg = NULL;
      g_wakeup_handler = d13x_wakeup_pm_interrupt;
      g_wakeup_pm_armed = true;
      ret = hal_gpio_enable_irq(D13X_WAKEUP_GPIO_GROUP,
                                D13X_WAKEUP_GPIO_PIN);
      if (ret >= 0)
        {
          up_enable_irq(D13X_IRQ_GPIOD);
          ret = hal_gpio_get_value(D13X_WAKEUP_GPIO_GROUP,
                                   D13X_WAKEUP_GPIO_PIN, &value);
          if (ret >= 0 && value == 0)
            {
              nxsem_post(&g_wakeup_pm_sem);
            }
        }
      else
        {
          g_wakeup_handler = NULL;
          g_wakeup_pm_armed = false;
        }
    }

  if (ret < 0)
    {
      hal_gpio_disable_irq(D13X_WAKEUP_GPIO_GROUP,
                           D13X_WAKEUP_GPIO_PIN);
      up_disable_irq(D13X_IRQ_GPIOD);
      hal_gpio_clr_irq_stat(D13X_WAKEUP_GPIO_GROUP,
                            D13X_WAKEUP_GPIO_PIN);
      g_wakeup_handler = NULL;
      g_wakeup_arg = NULL;
      g_wakeup_pm_armed = false;
      hal_gpio_set_irq_mode(D13X_WAKEUP_GPIO_GROUP,
                            D13X_WAKEUP_GPIO_PIN,
                            PIN_IRQ_MODE_EDGE_BOTH);
    }

  leave_critical_section(flags);
  return ret;
}

int d13x_wakeup_pm_wait(uint32_t timeout_ms)
{
  if (!g_wakeup_pm_armed || timeout_ms == 0)
    {
      return -EINVAL;
    }

  return nxsem_tickwait_uninterruptible(&g_wakeup_pm_sem,
                                        MSEC2TICK(timeout_ms));
}

void d13x_wakeup_pm_disarm(void)
{
  irqstate_t flags;

  flags = enter_critical_section();
  hal_gpio_disable_irq(D13X_WAKEUP_GPIO_GROUP, D13X_WAKEUP_GPIO_PIN);
  up_disable_irq(D13X_IRQ_GPIOD);
  hal_gpio_clr_irq_stat(D13X_WAKEUP_GPIO_GROUP, D13X_WAKEUP_GPIO_PIN);
  if (g_wakeup_pm_armed)
    {
      g_wakeup_handler = NULL;
      g_wakeup_arg = NULL;
      g_wakeup_pm_armed = false;
    }

  hal_gpio_set_irq_mode(D13X_WAKEUP_GPIO_GROUP, D13X_WAKEUP_GPIO_PIN,
                        PIN_IRQ_MODE_EDGE_BOTH);
  leave_critical_section(flags);
}
#endif
