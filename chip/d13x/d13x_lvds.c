/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_lvds.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * D13x single-link LVDS setup derived from Luban-Lite and the corrected
 * lladlam NuttX D13x implementation.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdint.h>

#include <nuttx/arch.h>

#include "riscv_internal.h"

#include "chip.h"
#include "d13x_lvds.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define D13X_BIT(n)                 (1u << (n))

#define LVDS_CTL                    0x00u
#define LVDS_0_SWAP                 0x20u
#define LVDS_1_SWAP                 0x24u
#define LVDS_0_POL_CTL              0x28u
#define LVDS_1_POL_CTL              0x2cu
#define LVDS_0_PHY_CTL              0x30u
#define LVDS_1_PHY_CTL              0x34u

#define LVDS_CTL_MODE(x)            (((x) & 0x3u) << 8)
#define LVDS_CTL_LINK(x)            (((x) & 0x3u) << 4)
#define LVDS_CTL_SYNC_MODE          D13X_BIT(1)
#define LVDS_CTL_ENABLE             D13X_BIT(0)

#define LVDS_LINES                  0x43210u
#define LVDS_PHY                    0xfau

#define CMU_PLL_FRA2_GEN            (CMU_BASE + 0x0028u)
#define CMU_PLL_FRA2_SDM            (CMU_BASE + 0x0088u)
#define CMU_CLK_DISP                (CMU_BASE + 0x0220u)
#define CMU_CLK_LVDS                (CMU_BASE + 0x0884u)

#define PLL_ENABLE                  D13X_BIT(16)
#define PLL_OUT_SYS                 D13X_BIT(18)
#define PLL_FACTOR_M_ENABLE         D13X_BIT(19)
#define PLL_OUT_MUX                 D13X_BIT(20)

#define MOD_RESET_DEASSERT          D13X_BIT(13)
#define MOD_BUS_ENABLE              D13X_BIT(12)
#define MOD_CLOCK_ENABLE            D13X_BIT(8)

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void d13x_lvds_write(uint32_t offset, uint32_t value)
{
  putreg32(value, LVDS_BASE + offset);
}

static int d13x_lvds_clock_config(uint32_t pixel_clock)
{
  uint32_t reg;

  if (pixel_clock != 52000000u)
    {
      return -EINVAL;
    }

  /* Configure FRA2 for the 364 MHz LVDS serial clock. These are the direct
   * Luban-Lite SDM parameters: P=1, N=120, M=3, step=360, selector=3.
   */

  reg = getreg32(CMU_PLL_FRA2_GEN);
  reg &= ~PLL_OUT_MUX;
  putreg32(reg, CMU_PLL_FRA2_GEN);

  reg &= ~(0xffffu | PLL_FACTOR_M_ENABLE | (0x1fu << 24));
  reg |= PLL_FACTOR_M_ENABLE | (120u << 8) | (3u << 4) | 1u;
  putreg32(reg, CMU_PLL_FRA2_GEN);
  putreg32(D13X_BIT(31) | (2u << 29) | (360u << 20) | (3u << 17),
           CMU_PLL_FRA2_SDM);

  reg = getreg32(CMU_PLL_FRA2_GEN);
  reg |= PLL_ENABLE | PLL_OUT_SYS;
  putreg32(reg, CMU_PLL_FRA2_GEN);
  up_udelay(200);

  reg = getreg32(CMU_PLL_FRA2_GEN);
  reg |= PLL_OUT_MUX;
  putreg32(reg, CMU_PLL_FRA2_GEN);

  /* SCLK = FRA2 / 1 and PIXCLK = SCLK / 7. */

  reg = getreg32(CMU_CLK_DISP);
  reg &= ~((0x7u << 0) | (0x1fu << 4) |
           (0x3u << 10) | (0x3u << 12));
  reg |= 6u << 4;
  putreg32(reg, CMU_CLK_DISP);

  reg = getreg32(CMU_CLK_LVDS);
  reg |= MOD_RESET_DEASSERT | MOD_BUS_ENABLE | MOD_CLOCK_ENABLE;
  putreg32(reg, CMU_CLK_LVDS);
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int d13x_lvds_initialize(uint32_t pixel_clock)
{
  int ret;

  ret = d13x_lvds_clock_config(pixel_clock);
  if (ret < 0)
    {
      return ret;
    }

  d13x_lvds_disable();
  d13x_lvds_write(LVDS_0_SWAP, LVDS_LINES);
  d13x_lvds_write(LVDS_1_SWAP, LVDS_LINES);
  d13x_lvds_write(LVDS_0_POL_CTL, 0);
  d13x_lvds_write(LVDS_1_POL_CTL, 0);
  d13x_lvds_write(LVDS_0_PHY_CTL, LVDS_PHY);
  d13x_lvds_write(LVDS_1_PHY_CTL, LVDS_PHY);
  d13x_lvds_write(LVDS_CTL, LVDS_CTL_MODE(0) | LVDS_CTL_LINK(0) |
                            LVDS_CTL_SYNC_MODE);
  return OK;
}

void d13x_lvds_enable(void)
{
  uint32_t reg;

  reg = getreg32(CMU_CLK_LVDS);
  reg |= MOD_RESET_DEASSERT | MOD_BUS_ENABLE | MOD_CLOCK_ENABLE;
  putreg32(reg, CMU_CLK_LVDS);

  reg = getreg32(LVDS_BASE + LVDS_CTL);
  putreg32(reg | LVDS_CTL_ENABLE, LVDS_BASE + LVDS_CTL);
}

void d13x_lvds_disable(void)
{
  uint32_t reg = getreg32(LVDS_BASE + LVDS_CTL);

  putreg32(reg & ~LVDS_CTL_ENABLE, LVDS_BASE + LVDS_CTL);
}
