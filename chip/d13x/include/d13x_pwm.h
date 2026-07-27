/****************************************************************************
 * contest2026_094_andy/chip/d13x/include/d13x_pwm.h
 ****************************************************************************/

#ifndef __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_D13X_PWM_H
#define __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_D13X_PWM_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef CONFIG_D13X_PWM1
int d13x_pwm1_initialize(const char *devpath);
#endif

#endif /* __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_D13X_PWM_H */
