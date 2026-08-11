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
#ifdef CONFIG_AIC_USING_QSPI0
  /* Onboard 16 MiB SPI NOR: QSPI0 WP/MISO/CS/HOLD/CLK/MOSI. */

  {2, PIN_PULL_UP, 3, "PB.0"},
  {2, PIN_PULL_UP, 3, "PB.1"},
  {2, PIN_PULL_UP, 3, "PB.2"},
  {2, PIN_PULL_UP, 3, "PB.3"},
  {2, PIN_PULL_UP, 3, "PB.4"},
  {2, PIN_PULL_UP, 3, "PB.5"},
#endif
#ifdef CONFIG_ARCH_BUTTONS
  /* Onboard WAKEUP key: PD.15 is active low and conflicts with I2S_MCLK. */

  {1, PIN_PULL_UP, 3, "PD.15"},
#endif
#ifdef CONFIG_D13X_KEYADC
  /* Four direction keys share GPAI2 through the PA.2 resistor ladder. */

  {2, PIN_PULL_DIS, 3, "PA.2"},
#endif
#ifdef CONFIG_D13X_SDMC1
  /* J5 TF card: SDMC1 D0..D3/CMD/CLK and card detect. */

  {2, PIN_PULL_UP, 3, "PC.0"},
  {2, PIN_PULL_UP, 3, "PC.1"},
  {2, PIN_PULL_UP, 3, "PC.2"},
  {2, PIN_PULL_UP, 3, "PC.3"},
  {2, PIN_PULL_UP, 3, "PC.4"},
  {2, PIN_PULL_UP, 3, "PC.5"},
  {2, PIN_PULL_UP, 3, "PC.6"},
#endif
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
#ifdef CONFIG_AIC_USING_AUDIO
  /* Onboard speaker: DSPK1 on PE.12 and active-low amplifier shutdown on
   * PD.10. The amplifier remains disabled until playback starts.
   */

  {5, PIN_PULL_DIS, 3, "PE.12"},
  {1, PIN_PULL_DIS, 3, CONFIG_AIC_AUDIO_PA_ENABLE_GPIO},
#ifdef CONFIG_AIC_AUDIO_DMIC
  /* Onboard PDM microphones: shared clock on PD.16 and data on PD.17. */

  {4, PIN_PULL_DIS, 3, "PD.16"},
  {4, PIN_PULL_DIS, 3, "PD.17"},
#endif
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
