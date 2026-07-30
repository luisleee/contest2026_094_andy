/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_spinor.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <aic_mtd.h>
#include <sfud.h>

#include "d13x_spinor.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define D13X_SPINOR_MTD_NAME  "/dev/nor0"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static FAR sfud_flash *g_d13x_spinor;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void d13x_spinor_copy_name(FAR char *destination,
                                  FAR const char *source)
{
  if (source == NULL)
    {
      source = "unknown";
    }

  strncpy(destination, source, D13X_SPINOR_NAME_MAX - 1);
  destination[D13X_SPINOR_NAME_MAX - 1] = '\0';
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int d13x_spinor_initialize(void)
{
  extern FAR sfud_flash *spinor_init(unsigned int spi_bus);

  g_d13x_spinor = spinor_init(0);
  return g_d13x_spinor != NULL ? OK : -ENODEV;
}

int d13x_spinor_get_info(FAR struct d13x_spinor_info_s *info)
{
  if (info == NULL)
    {
      return -EINVAL;
    }

  if (g_d13x_spinor == NULL || !g_d13x_spinor->init_ok)
    {
      return -ENODEV;
    }

  memset(info, 0, sizeof(*info));
  d13x_spinor_copy_name(info->name, g_d13x_spinor->chip.name);
  info->manufacturer_id = g_d13x_spinor->chip.mf_id;
  info->memory_type_id = g_d13x_spinor->chip.type_id;
  info->capacity_id = g_d13x_spinor->chip.capacity_id;
  info->capacity = g_d13x_spinor->chip.capacity;
  info->erase_size = g_d13x_spinor->chip.erase_gran;

#ifdef CONFIG_SFUD_USING_SFDP
  info->bus_frequency = g_d13x_spinor->bus_hz;
  info->sfdp_available = g_d13x_spinor->sfdp.available;
  info->sfdp_major = g_d13x_spinor->sfdp.major_rev;
  info->sfdp_minor = g_d13x_spinor->sfdp.minor_rev;
  info->sfdp_capacity = g_d13x_spinor->sfdp.capacity;
#endif

  return OK;
}

uint32_t d13x_spinor_get_partition_count(void)
{
  return mtd_get_device_count();
}

int d13x_spinor_get_partition(uint32_t index,
                              FAR struct d13x_spinor_partition_s *partition)
{
  FAR struct mtd_dev *device;

  if (partition == NULL)
    {
      return -EINVAL;
    }

  device = mtd_get_device_by_id(index);
  if (device == NULL)
    {
      return -ENOENT;
    }

  memset(partition, 0, sizeof(*partition));
  d13x_spinor_copy_name(partition->name, device->name);
  partition->start = device->start;
  partition->size = device->size;
  partition->erase_size = device->erasesize;
  return OK;
}

int d13x_spinor_read(uint32_t offset, FAR uint8_t *buffer, uint32_t length)
{
  FAR struct mtd_dev *device;
  int ret;

  if (buffer == NULL || length == 0)
    {
      return -EINVAL;
    }

  device = mtd_get_device(D13X_SPINOR_MTD_NAME);
  if (device == NULL)
    {
      return -ENODEV;
    }

  if (offset >= device->size || length > device->size - offset)
    {
      return -ERANGE;
    }

  ret = mtd_read(device, offset, buffer, length);
  return ret == SFUD_SUCCESS ? OK : -EIO;
}
