/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_panel.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * demo88-nor J18 panel/backlight enable control.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>

#include <nuttx/arch.h>

#include "riscv_internal.h"

#include "chip.h"
#include "d13x_panel.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define PANEL_GPIO_GROUP            4u
#define PANEL_ENABLE_PIN            13u

#define GPIO_GROUP_STRIDE           0x100u
#define GPIO_OUTPUT_CLEAR           0x10u
#define GPIO_OUTPUT_SET             0x14u
#define GPIO_PIN_CONFIG             0x80u
#define GPIO_PIN_STRIDE             0x04u

#define GPIO_FUNCTION_MASK          0x0fu
#define GPIO_DRIVE_MASK             (0x07u << 4)
#define GPIO_PULL_MASK              (0x03u << 8)
#define GPIO_DIRECTION_MASK         (0x03u << 16)
#define GPIO_FUNCTION_GPIO          1u
#define GPIO_DRIVE_LEVEL_3          (3u << 4)
#define GPIO_DIRECTION_OUTPUT       (2u << 16)

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uintptr_t d13x_panel_gpio_reg(uint32_t offset)
{
  return GPIO_BASE + PANEL_GPIO_GROUP * GPIO_GROUP_STRIDE + offset;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int d13x_panel_initialize(void)
{
  uintptr_t cfg;
  uint32_t reg;

  cfg = d13x_panel_gpio_reg(GPIO_PIN_CONFIG +
                            PANEL_ENABLE_PIN * GPIO_PIN_STRIDE);
  reg = getreg32(cfg);
  reg &= ~(GPIO_FUNCTION_MASK | GPIO_DRIVE_MASK | GPIO_PULL_MASK |
           GPIO_DIRECTION_MASK);
  reg |= GPIO_FUNCTION_GPIO | GPIO_DRIVE_LEVEL_3 | GPIO_DIRECTION_OUTPUT;
  putreg32(reg, cfg);

  /* Keep the panel/backlight off until LVDS timing is stable. */

  d13x_panel_disable();
  return 0;
}

void d13x_panel_enable(void)
{
  putreg32(1u << PANEL_ENABLE_PIN,
           d13x_panel_gpio_reg(GPIO_OUTPUT_SET));
}

void d13x_panel_disable(void)
{
  putreg32(1u << PANEL_ENABLE_PIN,
           d13x_panel_gpio_reg(GPIO_OUTPUT_CLEAR));
}
