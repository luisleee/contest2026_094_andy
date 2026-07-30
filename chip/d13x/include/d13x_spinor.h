/****************************************************************************
 * contest2026_094_andy/chip/d13x/include/d13x_spinor.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_D13X_SPINOR_H
#define __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_D13X_SPINOR_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define D13X_SPINOR_NAME_MAX  32

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct d13x_spinor_info_s
{
  char name[D13X_SPINOR_NAME_MAX];
  uint8_t manufacturer_id;
  uint8_t memory_type_id;
  uint8_t capacity_id;
  uint32_t capacity;
  uint32_t erase_size;
  uint32_t bus_frequency;
  bool sfdp_available;
  uint8_t sfdp_major;
  uint8_t sfdp_minor;
  uint32_t sfdp_capacity;
};

struct d13x_spinor_partition_s
{
  char name[D13X_SPINOR_NAME_MAX];
  uint32_t start;
  uint32_t size;
  uint32_t erase_size;
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#if defined(CONFIG_USING_SFUD) && defined(CONFIG_AIC_SPINOR_DRV)
int d13x_spinor_initialize(void);
int d13x_spinor_get_info(FAR struct d13x_spinor_info_s *info);
uint32_t d13x_spinor_get_partition_count(void);
int d13x_spinor_get_partition(uint32_t index,
                              FAR struct d13x_spinor_partition_s *partition);
int d13x_spinor_read(uint32_t offset, FAR uint8_t *buffer, uint32_t length);
#endif

#endif /* __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_D13X_SPINOR_H */
