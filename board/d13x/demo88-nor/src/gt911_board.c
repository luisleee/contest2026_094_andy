/****************************************************************************
 * contest2026_094_andy/board/d13x/demo88-nor/src/gt911_board.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <nuttx/arch.h>
#include <nuttx/i2c/i2c_master.h>

#include <aic_hal_gpio.h>

#include "board.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define GT911_GPIO_GROUP        0
#define GT911_RESET_PIN         10
#define GT911_INTERRUPT_PIN     11
#define GT911_GPIO_FUNCTION     1

#define GT911_I2C_FREQUENCY     400000
#define GT911_CONFIG_REG        0x8047
#define GT911_PRODUCT_ID_REG    0x8140
#define GT911_TOUCH_STATUS_REG  0x814e

#define GT911_CONFIG_SIZE       186
#define GT911_CONFIG_DATA_SIZE  (GT911_CONFIG_SIZE - 2)
#define GT911_CONFIG_INFO_SIZE  7

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* ArtInChip's GT911 configuration, adjusted to 1024x600 with five points.
 * The last two bytes are replaced with a calculated checksum and fresh flag.
 */

static const uint8_t g_gt911_config_template[GT911_CONFIG_SIZE] =
{
  0x6b, 0x00, 0x04, 0x58, 0x02, 0x05, 0x0d, 0x00,
  0x01, 0x0f, 0x28, 0x0f, 0x50, 0x32, 0x03, 0x05,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x8a, 0x2a, 0x0c, 0x45, 0x47,
  0x0c, 0x08, 0x00, 0x00, 0x00, 0x40, 0x03, 0x2c,
  0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x64, 0x32,
  0x00, 0x00, 0x00, 0x28, 0x64, 0x94, 0xd5, 0x02,
  0x07, 0x00, 0x00, 0x04, 0x95, 0x2c, 0x00, 0x8b,
  0x34, 0x00, 0x82, 0x3f, 0x00, 0x7d, 0x4c, 0x00,
  0x7a, 0x5b, 0x00, 0x7a, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x18, 0x16, 0x14, 0x12,
  0x10, 0x0e, 0x0c, 0x0a, 0x08, 0x06, 0x04, 0x02,
  0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x16, 0x18, 0x1c, 0x1d, 0x1e, 0x1f,
  0x20, 0x21, 0x22, 0x24, 0x13, 0x12, 0x10, 0x0f,
  0x0a, 0x08, 0x06, 0x04, 0x02, 0x00, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x79, 0x01
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int gt911_i2c_read(FAR struct i2c_master_s *i2c, uint8_t address,
                          uint16_t reg, uint8_t *buffer, size_t length)
{
  uint8_t regaddr[2] =
  {
    reg >> 8,
    reg & 0xff
  };

  struct i2c_msg_s messages[2] =
  {
    {
      .frequency = GT911_I2C_FREQUENCY,
      .addr = address,
      .flags = I2C_M_NOSTOP,
      .buffer = regaddr,
      .length = sizeof(regaddr),
    },
    {
      .frequency = GT911_I2C_FREQUENCY,
      .addr = address,
      .flags = I2C_M_READ,
      .buffer = buffer,
      .length = length,
    }
  };

  int ret;

  ret = I2C_TRANSFER(i2c, messages, 2);
  return ret == 2 ? OK : (ret < 0 ? ret : -EIO);
}

static int gt911_i2c_write(FAR struct i2c_master_s *i2c,
                           uint8_t address, uint16_t reg,
                           const uint8_t *buffer, size_t length)
{
  uint8_t transfer_buffer[2 + GT911_CONFIG_SIZE];
  struct i2c_msg_s message;
  int ret;

  if (length > GT911_CONFIG_SIZE)
    {
      return -E2BIG;
    }

  transfer_buffer[0] = reg >> 8;
  transfer_buffer[1] = reg & 0xff;
  memcpy(&transfer_buffer[2], buffer, length);

  message.frequency = GT911_I2C_FREQUENCY;
  message.addr = address;
  message.flags = 0;
  message.buffer = transfer_buffer;
  message.length = length + 2;

  ret = I2C_TRANSFER(i2c, &message, 1);
  return ret == 1 ? OK : (ret < 0 ? ret : -EIO);
}

static int gt911_probe(FAR struct i2c_master_s *i2c, uint8_t *address)
{
  static const uint8_t addresses[] =
  {
    0x5d, 0x14
  };

  uint8_t product_id[4];
  unsigned int i;
  int ret = -ENODEV;

  for (i = 0; i < sizeof(addresses) / sizeof(addresses[0]); i++)
    {
      ret = gt911_i2c_read(i2c, addresses[i], GT911_PRODUCT_ID_REG,
                           product_id, sizeof(product_id));
      if (ret >= 0 && memcmp(product_id, "\0\0\0\0", 4) != 0 &&
          memcmp(product_id, "\xff\xff\xff\xff", 4) != 0)
        {
          *address = addresses[i];
          return OK;
        }
    }

  return ret < 0 ? ret : -ENODEV;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int d13x_gt911_reset(void)
{
  int ret;

  ret = hal_gpio_set_func(GT911_GPIO_GROUP, GT911_RESET_PIN,
                          GT911_GPIO_FUNCTION);
  if (ret < 0)
    {
      return ret;
    }

  ret = hal_gpio_set_func(GT911_GPIO_GROUP, GT911_INTERRUPT_PIN,
                          GT911_GPIO_FUNCTION);
  if (ret < 0)
    {
      return ret;
    }

  hal_gpio_set_bias_pull(GT911_GPIO_GROUP, GT911_RESET_PIN, PIN_PULL_DIS);
  hal_gpio_set_bias_pull(GT911_GPIO_GROUP, GT911_INTERRUPT_PIN,
                         PIN_PULL_DIS);

  /* Match the demo88 factory sequence: reset first, then drive INT low to
   * select address 0x5d before reset is released.
   */

  hal_gpio_set_value(GT911_GPIO_GROUP, GT911_RESET_PIN, 0);
  hal_gpio_direction_output(GT911_GPIO_GROUP, GT911_RESET_PIN);
  up_mdelay(10);

  hal_gpio_set_value(GT911_GPIO_GROUP, GT911_INTERRUPT_PIN, 0);
  hal_gpio_direction_output(GT911_GPIO_GROUP, GT911_INTERRUPT_PIN);
  up_mdelay(2);

  hal_gpio_set_output(GT911_GPIO_GROUP, GT911_RESET_PIN);
  up_mdelay(5);
  hal_gpio_direction_input(GT911_GPIO_GROUP, GT911_RESET_PIN);

  /* Hold address selection through the controller's latch window. */

  hal_gpio_clr_output(GT911_GPIO_GROUP, GT911_INTERRUPT_PIN);
  up_mdelay(50);
  hal_gpio_direction_input(GT911_GPIO_GROUP, GT911_INTERRUPT_PIN);
  up_mdelay(100);
  return OK;
}

int d13x_gt911_configure(FAR struct i2c_master_s *i2c, uint8_t *address)
{
  uint8_t config[GT911_CONFIG_SIZE];
  uint8_t verify[GT911_CONFIG_INFO_SIZE];
  uint8_t status = 0;
  uint8_t sum = 0;
  unsigned int i;
  int ret;

  if (i2c == NULL || address == NULL)
    {
      return -EINVAL;
    }

  ret = gt911_probe(i2c, address);
  if (ret < 0)
    {
      return ret;
    }

  memcpy(config, g_gt911_config_template, sizeof(config));
  for (i = 0; i < GT911_CONFIG_DATA_SIZE; i++)
    {
      sum += config[i];
    }

  config[GT911_CONFIG_DATA_SIZE] = (uint8_t)(~sum + 1);
  config[GT911_CONFIG_DATA_SIZE + 1] = 1;

  ret = gt911_i2c_write(i2c, *address, GT911_CONFIG_REG,
                         config, sizeof(config));
  if (ret < 0)
    {
      return ret;
    }

  up_mdelay(20);
  ret = gt911_i2c_read(i2c, *address, GT911_CONFIG_REG,
                       verify, sizeof(verify));
  if (ret < 0)
    {
      return ret;
    }

  if (verify[1] != 0x00 || verify[2] != 0x04 ||
      verify[3] != 0x58 || verify[4] != 0x02 ||
      (verify[5] & 0x0f) != 5)
    {
      return -EIO;
    }

  return gt911_i2c_write(i2c, *address, GT911_TOUCH_STATUS_REG,
                          &status, sizeof(status));
}

int d13x_gt911_interrupt_level(bool *high)
{
  unsigned int value;
  int ret;

  if (high == NULL)
    {
      return -EINVAL;
    }

  ret = hal_gpio_get_value(GT911_GPIO_GROUP, GT911_INTERRUPT_PIN, &value);
  if (ret >= 0)
    {
      *high = value != 0;
    }

  return ret;
}
