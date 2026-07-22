/****************************************************************************
 * contest2026_094_andy/chip/d13x/include/chip.h
 ****************************************************************************/

#ifndef __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_CHIP_H
#define __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_CHIP_H

#include <arch/chip/aic_soc.h>

#ifndef NR_IRQS
#  define NR_IRQS MAX_IRQn
#endif

#define CACHE_LINE_SIZE 32
#define GPIO_MAX_PINS   192
#define GPIO_GROUP_MAX  6

#define D13X_CHIP_NAME "artinchip-d13x"

#endif /* __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_CHIP_H */
