/****************************************************************************
 * contest2026_094_andy/chip/d13x/include/aic_soc.h
 *
 * Minimal D13X SoC header for early bring-up. Base addresses and SRAM
 * placement are taken from the local D13X datasheet and user manual.
 ****************************************************************************/

#ifndef __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_AIC_SOC_H
#define __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_AIC_SOC_H

#include <arch/irq.h>

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

#define CLOCK_120M             120000000UL
#define CLOCK_100M             100000000UL
#define CLOCK_72M              72000000UL
#define CLOCK_60M              60000000UL
#define CLOCK_50M              50000000UL
#define CLOCK_36M              36000000UL
#define CLOCK_30M              30000000UL
#define CLOCK_AUDIO            24576000UL
#define CLOCK_24M              24000000UL
#define CLOCK_12M              12000000UL
#define CLOCK_4M               4000000UL
#define CLOCK_1M               1000000UL
#define CLOCK_32K              32768UL

#ifndef AIC_DMA_CH_NUM
#  define AIC_DMA_CH_NUM       8
#endif

#ifndef AIC_DMA_ALIGN_SIZE
#  define AIC_DMA_ALIGN_SIZE   4
#endif

#define CPU_BASE               0x20000000UL
#define E907_CORET_BASE        (CPU_BASE + 0x00004000UL)
#define BROM_BASE              0x30000000UL
#define SRAM0_BASE             0x30040000UL
#define SRAM1_BASE             0x3ff00000UL
#define SRAM_BANK_SIZE         0x00100000UL
#define SRAM_BOOTRSV_SIZE      0x00004000UL
#define SRAM_BOOT_LOAD_BASE    (SRAM0_BASE + SRAM_BOOTRSV_SIZE)
#define FLASH_XIP_BASE         0x60000000UL

#define E907_CLIC_BASE         0x20800000UL

#define DMA_BASE               0x10000000UL
#define CE_BASE                0x10020000UL
#define USB_DEV_BASE           0x10200000UL
#define USB_HOST_BASE          0x10210000UL
#define EMAC_BASE              0x10280000UL
#define XSPI_BASE              0x10300000UL
#define SPI0_BASE              0x10400000UL
#define SPI1_BASE              0x10410000UL
#define SPI2_BASE              0x10420000UL
#define SPI3_BASE              0x10430000UL
#define QSPI0_BASE             SPI0_BASE
#define QSPI1_BASE             SPI1_BASE
#define QSPI2_BASE             SPI2_BASE
#define QSPI3_BASE             SPI3_BASE
#define SDMC0_BASE             0x10440000UL
#define SDMC1_BASE             0x10450000UL
#define AHBCFG_BASE            0x104fe000UL
#define CORDIC_BASE            0x10700000UL
#define HCL_BASE               0x10710000UL
#define PBUS_BASE              0x107f0000UL
#define SYSCFG_BASE            0x18000000UL
#define CMU_BASE               0x18020000UL
#define SPI_ENC_BASE           0x18100000UL
#define PWMCS_BASE             0x18200000UL
#define PSADC_BASE             0x18210000UL
#define AXICFG_BASE            0x184fe000UL
#define MTOP_BASE              0x184ff000UL
#define I2S_BASE               0x18600000UL
#define AUDIO_BASE             0x18610000UL
#define GPIO_BASE              0x18700000UL
#define UART0_BASE             0x18710000UL
#define UART1_BASE             0x18711000UL
#define UART2_BASE             0x18712000UL
#define UART3_BASE             0x18713000UL
#define UART4_BASE             0x18714000UL
#define UART5_BASE             0x18715000UL
#define UART6_BASE             0x18716000UL
#define UART7_BASE             0x18717000UL
#define UART_BASE(n)           (UART0_BASE + ((n) * 0x1000UL))
#define LCD_BASE               0x18800000UL
#define LVDS_BASE              0x18810000UL
#define MIPI_DSI_BASE          0x18820000UL
#define DE_BASE                0x18a00000UL
#define WDT_BASE               0x19000000UL
#define WRI_BASE               0x1900f000UL
#define SID_BASE               0x19010000UL
#define RTC_BASE               0x19030000UL
#define GTC_BASE               0x19050000UL
#define I2C0_BASE              0x19220000UL
#define I2C1_BASE              0x19221000UL
#define I2C2_BASE              0x19222000UL
#define CAN0_BASE              0x19230000UL
#define CAN1_BASE              0x19231000UL
#define PWM_BASE               0x19240000UL
#define ADCIM_BASE             0x19250000UL
#define GPAI_BASE              0x19251000UL
#define RTP_BASE               0x19252000UL
#define TSEN_BASE              0x19253000UL
#define THS_BASE               TSEN_BASE
#define CIR_BASE               0x19260000UL
#define PSRAM_BASE             0x40000000UL
#define PSRAM_SIZE             0x00800000UL

#define D13X_IRQN_PERI_BASE    16U
#define D13X_IRQN_CPU_TIMER    7U
#define D13X_IRQN_DMA          32U
#define D13X_IRQN_QSPI2        42U
#define D13X_IRQN_QSPI3        43U
#define D13X_IRQN_QSPI0        44U
#define D13X_IRQN_QSPI1        45U
#define D13X_IRQN_SDMC1        47U
#define D13X_IRQN_RTC          50U
#define D13X_IRQN_WDT          64U
#define D13X_IRQN_GPIOA        68U
#define D13X_IRQN_GPIOD        71U
#define D13X_IRQN_UART0        76U
#define D13X_IRQN_UART1        77U
#define D13X_IRQN_UART2        78U
#define D13X_IRQN_UART3        79U
#define D13X_IRQN_UART4        80U
#define D13X_IRQN_UART5        81U
#define D13X_IRQN_UART6        82U
#define D13X_IRQN_UART7        83U
#define D13X_IRQN_I2C0         84U
#define D13X_IRQN_I2C1         85U
#define D13X_IRQN_I2C2         86U
#define D13X_IRQN_LAST         D13X_IRQN_I2C2

#ifndef __ASSEMBLY__
typedef enum IRQn
{
  NMI_EXPn                 = -2,
  Supervisor_Software_IRQn = 1U,
  Machine_Software_IRQn    = 3U,
  User_Timer_IRQn          = 4U,
  Supervisor_Timer_IRQn    = 5U,
  CORET_IRQn               = D13X_IRQN_CPU_TIMER,
  Supervisor_External_IRQn = 9U,
  Machine_External_IRQn    = 11U,

  QSPI0_IRQn               = D13X_IRQN_QSPI0,
  QSPI1_IRQn               = D13X_IRQN_QSPI1,
  QSPI2_IRQn               = D13X_IRQN_QSPI2,
  QSPI3_IRQn               = D13X_IRQN_QSPI3,
  SDMC1_IRQn               = D13X_IRQN_SDMC1,
  RTC_IRQn                 = D13X_IRQN_RTC,
  WDT_IRQn                 = D13X_IRQN_WDT,
  DMA_IRQn                 = RISCV_IRQ_ASYNC + D13X_IRQN_DMA,
  GPIOA_IRQn               = RISCV_IRQ_ASYNC + D13X_IRQN_GPIOA,
  GPIOD_IRQn               = RISCV_IRQ_ASYNC + D13X_IRQN_GPIOD,
  UART0_IRQn               = RISCV_IRQ_ASYNC + D13X_IRQN_UART0,
  UART1_IRQn               = RISCV_IRQ_ASYNC + D13X_IRQN_UART1,
  UART2_IRQn               = RISCV_IRQ_ASYNC + D13X_IRQN_UART2,
  UART3_IRQn               = RISCV_IRQ_ASYNC + D13X_IRQN_UART3,
  UART4_IRQn               = RISCV_IRQ_ASYNC + D13X_IRQN_UART4,
  UART5_IRQn               = RISCV_IRQ_ASYNC + D13X_IRQN_UART5,
  UART6_IRQn               = RISCV_IRQ_ASYNC + D13X_IRQN_UART6,
  UART7_IRQn               = RISCV_IRQ_ASYNC + D13X_IRQN_UART7,
  I2C0_IRQn                = RISCV_IRQ_ASYNC + D13X_IRQN_I2C0,
  I2C1_IRQn                = RISCV_IRQ_ASYNC + D13X_IRQN_I2C1,
  I2C2_IRQn                = RISCV_IRQ_ASYNC + D13X_IRQN_I2C2,

  MAX_IRQn
} IRQn_Type;

#  define UART_IRQn(id) (UART0_IRQn + (id))
#endif

#ifdef __cplusplus
}
#endif

#endif /* __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_AIC_SOC_H */
