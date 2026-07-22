/****************************************************************************
 * contest2026_094_andy/board/d13x/demo88-nor/src/sys_clk.c
 ****************************************************************************/

#include <aic_core.h>
#include <aic_hal.h>

#ifndef AIC_CLK_PLL_INT0_FREQ
#  define AIC_CLK_PLL_INT0_FREQ 480000000UL
#endif

#ifndef AIC_CLK_PLL_INT1_FREQ
#  define AIC_CLK_PLL_INT1_FREQ 1200000000UL
#endif

#ifndef AIC_CLK_PLL_FRA0_FREQ
#  define AIC_CLK_PLL_FRA0_FREQ 792000000UL
#endif

#ifndef AIC_CLK_PLL_FRA2_FREQ
#  define AIC_CLK_PLL_FRA2_FREQ 1188000000UL
#endif

#ifndef AIC_CLK_CPU_FREQ
#  define AIC_CLK_CPU_FREQ 480000000UL
#endif

#ifndef AIC_CLK_AXI0_FREQ
#  define AIC_CLK_AXI0_FREQ 200000000UL
#endif

#ifndef AIC_CLK_AHB0_FREQ
#  define AIC_CLK_AHB0_FREQ 200000000UL
#endif

#ifndef AIC_CLK_APB0_FREQ
#  define AIC_CLK_APB0_FREQ 100000000UL
#endif

struct aic_sysclk
{
  unsigned long freq;
  unsigned int  clk_id;
  unsigned int  parent_clk_id;
};

static const struct aic_sysclk g_d13x_sysclk_config[] =
{
  {AIC_CLK_PLL_INT0_FREQ, CLK_PLL_INT0, 0},
  {AIC_CLK_PLL_INT1_FREQ, CLK_PLL_INT1, 0},
  {AIC_CLK_PLL_FRA0_FREQ, CLK_PLL_FRA0, 0},
  {AIC_CLK_PLL_FRA2_FREQ, CLK_PLL_FRA2, 0},
  {AIC_CLK_CPU_FREQ,      CLK_CPU,      CLK_CPU_SRC1},
  {AIC_CLK_AXI0_FREQ,     CLK_AXI0,     CLK_AXI_AHB_SRC1},
  {AIC_CLK_AHB0_FREQ,     CLK_AHB0,     CLK_AXI_AHB_SRC1},
  {AIC_CLK_APB0_FREQ,     CLK_APB0,     CLK_APB0_SRC1},
};

void aic_board_sysclk_init(void)
{
  uint32_t i;

  for (i = 0; i < ARRAY_SIZE(g_d13x_sysclk_config); i++)
    {
      if (g_d13x_sysclk_config[i].parent_clk_id != 0)
        {
          hal_clk_set_freq(g_d13x_sysclk_config[i].parent_clk_id,
                           g_d13x_sysclk_config[i].freq);
          hal_clk_enable(g_d13x_sysclk_config[i].parent_clk_id);
          hal_clk_set_parent(g_d13x_sysclk_config[i].clk_id,
                             g_d13x_sysclk_config[i].parent_clk_id);
        }
      else
        {
          hal_clk_set_freq(g_d13x_sysclk_config[i].clk_id,
                           g_d13x_sysclk_config[i].freq);
          hal_clk_enable(g_d13x_sysclk_config[i].clk_id);
        }
    }

  hal_clk_enable_deassertrst_iter(CLK_GPIO);
  hal_clk_enable_deassertrst_iter(CLK_GTC);
}
