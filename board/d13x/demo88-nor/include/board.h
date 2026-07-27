/*
 * Copyright (c) 2026, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CONTEST2026_094_ANDY_BOARD_D13X_DEMO88_NOR_BOARD_H
#define __CONTEST2026_094_ANDY_BOARD_D13X_DEMO88_NOR_BOARD_H

#include <nuttx/config.h>

#include <stdbool.h>
#include <stdint.h>

#if defined(KERNEL_BAREMETAL)
void aic_hw_board_init(void);
#endif

#ifdef CONFIG_D13X_TOUCH_GT911
#include <nuttx/i2c/i2c_master.h>

int d13x_gt911_reset(void);
int d13x_gt911_configure(FAR struct i2c_master_s *i2c,
                         uint8_t *address);
int d13x_gt911_interrupt_level(bool *high);
#ifdef CONFIG_D13X_GT911_INPUT
int d13x_gt911_input_register(FAR struct i2c_master_s *i2c,
                              uint8_t address, FAR const char *devpath);
#endif
#endif

#endif /* __CONTEST2026_094_ANDY_BOARD_D13X_DEMO88_NOR_BOARD_H */
