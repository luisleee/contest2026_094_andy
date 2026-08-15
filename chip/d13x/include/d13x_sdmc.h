/****************************************************************************
 * contest2026_094_andy/chip/d13x/include/d13x_sdmc.h
 ****************************************************************************/

#ifndef __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_D13X_SDMC_H
#define __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_D13X_SDMC_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

struct d13x_sdmc0_wifi_probe_s
{
  uint32_t ocr;
  uint16_t rca;
  uint8_t function_count;
  bool memory_present;
  bool ready;
};

int d13x_sdmc0_wifi_power(bool enable);
int d13x_sdmc0_wifi_probe(FAR struct d13x_sdmc0_wifi_probe_s *result);
int d13x_sdmc0_wifi_readb(uint8_t function, uint32_t address,
                          FAR uint8_t *value);
int d13x_sdmc0_wifi_writeb(uint8_t function, uint32_t address,
                           uint8_t value);
int d13x_sdmc0_wifi_read(uint8_t function, uint32_t address, bool inc_addr,
                         FAR uint8_t *buffer, size_t length);
int d13x_sdmc0_wifi_write(uint8_t function, uint32_t address, bool inc_addr,
                          FAR const uint8_t *buffer, size_t length);
int d13x_sdmc1_initialize(void);
int d13x_sdmc1_reprobe(void);
int d13x_sdmc1_mount(void);

#endif /* __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_D13X_SDMC_H */
