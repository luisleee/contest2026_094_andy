/****************************************************************************
 * contest2026_094_andy/chip/d13x/include/irq.h
 ****************************************************************************/

#ifndef __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_IRQ_H
#define __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_IRQ_H

#define D13X_IRQ_PERI_START   (RISCV_IRQ_ASYNC + 16)

#define D13X_IRQ_DMA          (D13X_IRQ_PERI_START + 16)
#define D13X_IRQ_GMAC0        (D13X_IRQ_PERI_START + 23) /* Raw IRQ 39 */
#define D13X_IRQ_SDMC1        (D13X_IRQ_PERI_START + 31) /* Raw IRQ 47 */
#define D13X_IRQ_RTC          (D13X_IRQ_PERI_START + 34) /* Raw IRQ 50 */
#define D13X_IRQ_WDT          (D13X_IRQ_PERI_START + 48) /* Raw IRQ 64 */
#define D13X_IRQ_GPIOA        (D13X_IRQ_PERI_START + 52) /* Raw IRQ 68 */
#define D13X_IRQ_GPIOD        (D13X_IRQ_PERI_START + 55) /* Raw IRQ 71 */
#define D13X_IRQ_UART0        (D13X_IRQ_PERI_START + 60)
#define D13X_IRQ_UART1        (D13X_IRQ_PERI_START + 61)
#define D13X_IRQ_UART2        (D13X_IRQ_PERI_START + 62)
#define D13X_IRQ_UART3        (D13X_IRQ_PERI_START + 63)
#define D13X_IRQ_UART4        (D13X_IRQ_PERI_START + 64)
#define D13X_IRQ_UART5        (D13X_IRQ_PERI_START + 65)
#define D13X_IRQ_UART6        (D13X_IRQ_PERI_START + 66)
#define D13X_IRQ_UART7        (D13X_IRQ_PERI_START + 67)
#define D13X_IRQ_I2C0         (D13X_IRQ_PERI_START + 68)
#define D13X_IRQ_I2C1         (D13X_IRQ_PERI_START + 69)
#define D13X_IRQ_I2C2         (D13X_IRQ_PERI_START + 70)

#define NR_IRQS               (D13X_IRQ_I2C2 + 1)

#endif /* __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_IRQ_H */
