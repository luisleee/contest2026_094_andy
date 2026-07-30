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

#define BUTTON_WAKEUP      0
#define NUM_BUTTONS        1
#define BUTTON_WAKEUP_BIT  (1u << BUTTON_WAKEUP)

#define DPAD_UP             0
#define DPAD_DOWN           1
#define DPAD_LEFT           2
#define DPAD_RIGHT          3
#define DPAD_UP_BIT         (1u << DPAD_UP)
#define DPAD_DOWN_BIT       (1u << DPAD_DOWN)
#define DPAD_LEFT_BIT       (1u << DPAD_LEFT)
#define DPAD_RIGHT_BIT      (1u << DPAD_RIGHT)

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

#ifdef CONFIG_D13X_KEYADC
int d13x_keyadc_register(FAR const char *devpath);
int d13x_keyadc_last_raw(FAR uint16_t *raw);
#endif

#ifdef CONFIG_D13X_WAKEUP_PM
int d13x_wakeup_pm_arm(void);
int d13x_wakeup_pm_wait(uint32_t timeout_ms);
void d13x_wakeup_pm_disarm(void);
#endif

#endif /* __CONTEST2026_094_ANDY_BOARD_D13X_DEMO88_NOR_BOARD_H */
