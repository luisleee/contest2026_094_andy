/****************************************************************************
 * contest2026_094_andy/chip/d13x/include/d13x_rtc.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_D13X_RTC_H
#define __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_D13X_RTC_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct d13x_rtc_state_s
{
  uint32_t version;
  uint32_t counter;
  uint32_t time_set;
  uint8_t control;
  uint8_t init;
  uint8_t irq_enable;
  uint8_t irq_status;
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef CONFIG_D13X_RTC
int d13x_rtc_get_version(FAR uint32_t *version);
int d13x_rtc_get_state(FAR struct d13x_rtc_state_s *state);
int d13x_rtc_alarm_wait(uint32_t delay_seconds, uint32_t timeout_ms);
#endif

#endif /* __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_D13X_RTC_H */
