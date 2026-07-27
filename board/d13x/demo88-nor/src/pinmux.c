/****************************************************************************
 * contest2026_094_andy/board/d13x/demo88-nor/src/pinmux.c
 ****************************************************************************/

#include <stdint.h>

#include <aic_core.h>
#include <aic_hal.h>
#include "board.h"
#include <aic_utils.h>

struct aic_pinmux aic_pinmux_config[] =
{
#ifdef CONFIG_D13X_I2C2
  /* Onboard GT911 touch bus. PA.10/PA.11 are configured explicitly by the
   * board touch initialization when CONFIG_D13X_TOUCH_GT911 is enabled.
   */

  {4, PIN_PULL_DIS, 3, "PA.8"},
  {4, PIN_PULL_DIS, 3, "PA.9"},
#endif
#ifdef CONFIG_D13X_PWM1
  /* Onboard buzzer: PE.11 is PWM1_A on mux function 3. */

  {3, PIN_PULL_DIS, 3, "PE.11"},
#endif
#ifdef CONFIG_D13X_DISPLAY
  /* J18 single-link LVDS. J3 MIPI uses the same pins with function 4. */

  {3, PIN_PULL_DIS, 3, "PD.18"},
  {3, PIN_PULL_DIS, 3, "PD.19"},
  {3, PIN_PULL_DIS, 3, "PD.20"},
  {3, PIN_PULL_DIS, 3, "PD.21"},
  {3, PIN_PULL_DIS, 3, "PD.22"},
  {3, PIN_PULL_DIS, 3, "PD.23"},
  {3, PIN_PULL_DIS, 3, "PD.24"},
  {3, PIN_PULL_DIS, 3, "PD.25"},
  {3, PIN_PULL_DIS, 3, "PD.26"},
  {3, PIN_PULL_DIS, 3, "PD.27"},
#endif
};

uint32_t aic_pinmux_config_size = ARRAY_SIZE(aic_pinmux_config);
