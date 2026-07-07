/****************************************************************************
 * contest2026_094_andy/chip/d13x/include/aic_soc.h
 *
 * Minimal D13X SoC header for build bring-up. The values here are only
 * enough to let board/chip code compile while the real port is being filled
 * in from the official D13X manual.
 ****************************************************************************/

#ifndef __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_AIC_SOC_H
#define __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_AIC_SOC_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef IHS_VALUE
#  define IHS_VALUE 24000000UL
#endif

#ifndef EHS_VALUE
#  define EHS_VALUE 24000000UL
#endif

#define BROM_BASE      0x30000000UL
#define SRAM_BASE      0x30040000UL
#define FLASH_XIP_BASE 0x60000000UL

#define DMA_BASE       0x10000000UL
#define XSPI_BASE      0x10300000UL
#define SPI0_BASE      0x10400000UL
#define SPI1_BASE      0x10410000UL
#define SPI2_BASE      0x10420000UL
#define SPI3_BASE      0x10430000UL
#define SDMC0_BASE     0x10440000UL
#define SDMC1_BASE     0x10450000UL
#define AHBCFG_BASE    0x104fe000UL
#define SYSCFG_BASE    0x18000000UL
#define CMU_BASE       0x18020000UL
#define GPIO_BASE      0x18700000UL
#define UART0_BASE     0x18710000UL
#define UART1_BASE     0x18711000UL
#define UART2_BASE     0x18712000UL
#define UART3_BASE     0x18713000UL
#define UART_BASE(n)   (UART0_BASE + ((n) * 0x1000UL))

#ifndef __ASSEMBLY__
typedef enum IRQn
{
  NMI_EXPn                 = -2,
  Supervisor_Software_IRQn = 1U,
  Machine_Software_IRQn    = 3U,
  User_Timer_IRQn          = 4U,
  Supervisor_Timer_IRQn    = 5U,
  CORET_IRQn               = 7U,
  Supervisor_External_IRQn = 9U,
  Machine_External_IRQn    = 11U,

  DMA_IRQn                 = 32U,
  UART0_IRQn               = 76U,
  UART1_IRQn               = 77U,
  UART2_IRQn               = 78U,
  UART3_IRQn               = 79U,

  MAX_IRQn
} IRQn_Type;

#  define UART_IRQn(id) (UART0_IRQn + (id))
#endif

#ifdef __cplusplus
}
#endif

#endif /* __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_AIC_SOC_H */
