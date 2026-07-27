/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_pwm.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * D13x PWM1 lower-half for the demo88-nor onboard buzzer. The register
 * layout and clock fields follow the D13x user manual and Luban-Lite port.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <nuttx/arch.h>
#include <nuttx/timers/pwm.h>

#include "riscv_internal.h"

#include "chip.h"
#include "include/d13x_pwm.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define D13X_PWM_CHANNEL             1u

#define D13X_PWM_CMU_REG             (CMU_BASE + 0x0990u)
#define D13X_PWM_CMU_DIV_MASK        0x1fu
#define D13X_PWM_CMU_DIV_48MHZ       24u
#define D13X_PWM_CMU_MODULE_CLOCK    (1u << 8)
#define D13X_PWM_CMU_BUS_CLOCK       (1u << 12)
#define D13X_PWM_CMU_RESET_RELEASE   (1u << 13)

#define D13X_PWM_CLOCK               48000000u
#define D13X_PWM_MIN_FREQUENCY       20u
#define D13X_PWM_MAX_FREQUENCY       20000u

#define D13X_PWM_CTL                 0x000u
#define D13X_PWM_MCTL                0x004u
#define D13X_PWM_CKCTL               0x008u
#define D13X_PWM_CH_BASE(n)          (0x300u + ((n) * 0x100u))
#define D13X_PWM_TBCTL(n)            (D13X_PWM_CH_BASE(n) + 0x000u)
#define D13X_PWM_TBCTR(n)            (D13X_PWM_CH_BASE(n) + 0x010u)
#define D13X_PWM_TBPRD(n)            (D13X_PWM_CH_BASE(n) + 0x014u)
#define D13X_PWM_CMPCTL(n)           (D13X_PWM_CH_BASE(n) + 0x018u)
#define D13X_PWM_CMPA(n)             (D13X_PWM_CH_BASE(n) + 0x020u)
#define D13X_PWM_CMPB(n)             (D13X_PWM_CH_BASE(n) + 0x024u)
#define D13X_PWM_AQCTLA(n)           (D13X_PWM_CH_BASE(n) + 0x028u)
#define D13X_PWM_AQCTLB(n)           (D13X_PWM_CH_BASE(n) + 0x02cu)

#define D13X_PWM_MODULE_ENABLE       (1u << 0)
#define D13X_PWM_CHANNEL_BIT         (1u << D13X_PWM_CHANNEL)
#define D13X_PWM_TB_DIV_SHIFT        16u
#define D13X_PWM_TB_DIV_MAX          4096u
#define D13X_PWM_PERIOD_MAX          65536u
#define D13X_PWM_TB_IMMEDIATE        (1u << 3)
#define D13X_PWM_CMP_IMMEDIATE       ((1u << 6) | (1u << 4))
#define D13X_PWM_ACTION_CAU_LOW      (1u << 4)
#define D13X_PWM_ACTION_ZRO_HIGH     2u
#define D13X_PWM_ACTION_INIT_HIGH    (1u << 16)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct d13x_pwm_lowerhalf_s
{
  struct pwm_lowerhalf_s lower;
  bool registered;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int d13x_pwm_setup(struct pwm_lowerhalf_s *dev);
static int d13x_pwm_shutdown(struct pwm_lowerhalf_s *dev);
#ifdef CONFIG_PWM_PULSECOUNT
static int d13x_pwm_start(struct pwm_lowerhalf_s *dev,
                          const struct pwm_info_s *info, void *handle);
#else
static int d13x_pwm_start(struct pwm_lowerhalf_s *dev,
                          const struct pwm_info_s *info);
#endif
static int d13x_pwm_stop(struct pwm_lowerhalf_s *dev);
static int d13x_pwm_ioctl(struct pwm_lowerhalf_s *dev, int cmd,
                          unsigned long arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct pwm_ops_s g_d13x_pwm_ops =
{
  .setup = d13x_pwm_setup,
  .shutdown = d13x_pwm_shutdown,
  .start = d13x_pwm_start,
  .stop = d13x_pwm_stop,
  .ioctl = d13x_pwm_ioctl,
};

static struct d13x_pwm_lowerhalf_s g_d13x_pwm1 =
{
  .lower =
  {
    .ops = &g_d13x_pwm_ops
  }
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint32_t d13x_pwm_getreg(uint32_t offset)
{
  return getreg32(PWM_BASE + offset);
}

static void d13x_pwm_putreg(uint32_t offset, uint32_t value)
{
  putreg32(value, PWM_BASE + offset);
}

static void d13x_pwm_modifyreg(uint32_t offset, uint32_t clearbits,
                               uint32_t setbits)
{
  uint32_t value = d13x_pwm_getreg(offset);

  value &= ~clearbits;
  value |= setbits;
  d13x_pwm_putreg(offset, value);
}

static int d13x_pwm_setup(struct pwm_lowerhalf_s *dev)
{
  uint32_t value;

  (void)dev;

  value = getreg32(D13X_PWM_CMU_REG);
  value &= ~D13X_PWM_CMU_DIV_MASK;
  value |= D13X_PWM_CMU_DIV_48MHZ | D13X_PWM_CMU_MODULE_CLOCK |
           D13X_PWM_CMU_BUS_CLOCK | D13X_PWM_CMU_RESET_RELEASE;
  putreg32(value, D13X_PWM_CMU_REG);

  d13x_pwm_modifyreg(D13X_PWM_CTL, 0, D13X_PWM_MODULE_ENABLE);
  d13x_pwm_modifyreg(D13X_PWM_CKCTL, 0, D13X_PWM_CHANNEL_BIT);
  d13x_pwm_modifyreg(D13X_PWM_MCTL, D13X_PWM_CHANNEL_BIT, 0);
  d13x_pwm_putreg(D13X_PWM_AQCTLA(D13X_PWM_CHANNEL), 0);
  d13x_pwm_putreg(D13X_PWM_AQCTLB(D13X_PWM_CHANNEL), 0);
  return OK;
}

static int d13x_pwm_shutdown(struct pwm_lowerhalf_s *dev)
{
  d13x_pwm_stop(dev);
  d13x_pwm_modifyreg(D13X_PWM_CKCTL, D13X_PWM_CHANNEL_BIT, 0);
  return OK;
}

#ifdef CONFIG_PWM_PULSECOUNT
static int d13x_pwm_start(struct pwm_lowerhalf_s *dev,
                          const struct pwm_info_s *info, void *handle)
#else
static int d13x_pwm_start(struct pwm_lowerhalf_s *dev,
                          const struct pwm_info_s *info)
#endif
{
  uint64_t clocks_per_period;
  uint64_t compare;
  uint32_t action;
  uint32_t divider;
  uint32_t period;

  (void)dev;
#ifdef CONFIG_PWM_PULSECOUNT
  (void)handle;

  if (info->count != 0)
    {
      return -ENOTSUP;
    }
#endif

  if (info->frequency < D13X_PWM_MIN_FREQUENCY ||
      info->frequency > D13X_PWM_MAX_FREQUENCY || info->duty > 0xffffu)
    {
      return -ERANGE;
    }

  clocks_per_period = ((uint64_t)D13X_PWM_CLOCK +
                       (info->frequency / 2u)) / info->frequency;
  divider = (uint32_t)((clocks_per_period + D13X_PWM_PERIOD_MAX - 1u) /
                       D13X_PWM_PERIOD_MAX);
  if (divider == 0)
    {
      divider = 1;
    }

  if (divider > D13X_PWM_TB_DIV_MAX)
    {
      return -ERANGE;
    }

  period = (uint32_t)((clocks_per_period + (divider / 2u)) / divider);
  if (period < 2u || period > D13X_PWM_PERIOD_MAX)
    {
      return -ERANGE;
    }

  d13x_pwm_modifyreg(D13X_PWM_MCTL, D13X_PWM_CHANNEL_BIT, 0);
  d13x_pwm_putreg(D13X_PWM_TBCTL(D13X_PWM_CHANNEL),
                  ((divider - 1u) << D13X_PWM_TB_DIV_SHIFT) |
                  D13X_PWM_TB_IMMEDIATE);
  d13x_pwm_putreg(D13X_PWM_TBCTR(D13X_PWM_CHANNEL), 0);
  d13x_pwm_putreg(D13X_PWM_TBPRD(D13X_PWM_CHANNEL), period - 1u);
  d13x_pwm_putreg(D13X_PWM_CMPCTL(D13X_PWM_CHANNEL),
                  D13X_PWM_CMP_IMMEDIATE);

  if (info->duty == 0)
    {
      compare = 0;
      action = 0;
    }
  else if (info->duty == 0xffffu)
    {
      compare = period;
      action = D13X_PWM_ACTION_INIT_HIGH;
    }
  else
    {
      compare = ((uint64_t)period * info->duty + 0x8000u) >> 16;
      if (compare == 0)
        {
          compare = 1;
        }

      action = D13X_PWM_ACTION_ZRO_HIGH | D13X_PWM_ACTION_CAU_LOW;
    }

  d13x_pwm_putreg(D13X_PWM_CMPA(D13X_PWM_CHANNEL), (uint32_t)compare);
  d13x_pwm_putreg(D13X_PWM_CMPB(D13X_PWM_CHANNEL), 0);
  d13x_pwm_putreg(D13X_PWM_AQCTLA(D13X_PWM_CHANNEL), action);
  d13x_pwm_putreg(D13X_PWM_AQCTLB(D13X_PWM_CHANNEL), 0);
  d13x_pwm_modifyreg(D13X_PWM_MCTL, 0, D13X_PWM_CHANNEL_BIT);
  return OK;
}

static int d13x_pwm_stop(struct pwm_lowerhalf_s *dev)
{
  (void)dev;

  d13x_pwm_modifyreg(D13X_PWM_MCTL, D13X_PWM_CHANNEL_BIT, 0);
  d13x_pwm_putreg(D13X_PWM_AQCTLA(D13X_PWM_CHANNEL), 0);
  d13x_pwm_putreg(D13X_PWM_AQCTLB(D13X_PWM_CHANNEL), 0);
  return OK;
}

static int d13x_pwm_ioctl(struct pwm_lowerhalf_s *dev, int cmd,
                          unsigned long arg)
{
  (void)dev;
  (void)cmd;
  (void)arg;
  return -ENOTTY;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int d13x_pwm1_initialize(const char *devpath)
{
  int ret;

  if (g_d13x_pwm1.registered)
    {
      return OK;
    }

  ret = pwm_register(devpath, &g_d13x_pwm1.lower);
  if (ret >= 0)
    {
      g_d13x_pwm1.registered = true;
    }

  return ret;
}
