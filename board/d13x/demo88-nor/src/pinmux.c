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
#ifdef CONFIG_AIC_USING_UART1
  {5, PIN_PULL_DIS, 3, "PA.2"},
  {5, PIN_PULL_UP,  3, "PA.3"},
#endif
#ifdef CONFIG_AIC_USING_QSPI0
  {2, PIN_PULL_UP, 3, "PB.0"},
  {2, PIN_PULL_UP, 3, "PB.1"},
  {2, PIN_PULL_UP, 3, "PB.2"},
  {2, PIN_PULL_UP, 3, "PB.3"},
  {2, PIN_PULL_UP, 3, "PB.4"},
  {2, PIN_PULL_UP, 3, "PB.5"},
#endif
};

uint32_t aic_pinmux_config_size = ARRAY_SIZE(aic_pinmux_config);
