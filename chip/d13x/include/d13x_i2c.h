/****************************************************************************
 * contest2026_094_andy/chip/d13x/include/d13x_i2c.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_D13X_I2C_H
#define __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_D13X_I2C_H

#include <nuttx/config.h>

struct i2c_master_s;

FAR struct i2c_master_s *d13x_i2cbus_initialize(int bus);

#endif /* __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_D13X_I2C_H */
