/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_sdmc.c
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/mount.h>
#include <sys/statfs.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <nuttx/fs/fs.h>
#include <nuttx/fs/partition.h>
#include <nuttx/mmcsd.h>
#include <nuttx/sdio.h>
#include <syslog.h>

#include <aic_hal_gpio.h>
#include "aic_sdio.h"
#include "d13x_sdmc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define D13X_SDMC1_SLOT  1
#define D13X_SDMC1_MINOR 0
#define D13X_SDMC1_DEVICE       "/dev/mmcsd0"
#define D13X_SDMC1_PART_PREFIX  "/dev/mmcsd0p"
#define D13X_SDMC1_MOUNTPOINT   "/sdcard"
#define D13X_SDMC1_MAX_PARTS    4
#define D13X_SDMC0_SLOT  0
#define D13X_SDMC0_WIFI_OCR_READY   (1ul << 31)
#define D13X_SDMC0_WIFI_OCR_MEMORY  (1ul << 27)
#define D13X_SDMC0_WIFI_OCR_IOFUNCS_SHIFT 28
#define D13X_SDMC0_WIFI_OCR_IOFUNCS_MASK  (0x07ul << 28)
#define D13X_SDMC0_WIFI_OCR_VDD_MASK      0x00ff8000ul
#define D13X_SDMC0_WIFI_READY_RETRIES     100
#define D13X_SDMC0_WIFI_CMD53_TIMEOUT_MS  1000
#define D13X_SDMC0_WIFI_R5_DATA_MASK      0x000000fful
#define D13X_SDMC0_WIFI_R5_ERR_MASK       0x0000c800ul
#define D13X_SDMC0_WIFI_R5_INV_MASK       0x00000300ul

/****************************************************************************
 * Private Data
 ****************************************************************************/

static FAR struct sdio_dev_s *g_sdmc1;
static FAR struct sdio_dev_s *g_sdmc0;
static uint32_t g_sdmc0_wifi_ocr;
static uint16_t g_sdmc0_wifi_rca;
static uint8_t g_sdmc0_wifi_funcs;
static bool g_sdmc0_wifi_memory;
static bool g_sdmc0_wifi_ready;
static bool g_sdmc0_wifi_selected;

struct d13x_sdmc1_partitions_s
{
  uint8_t count;
  char paths[D13X_SDMC1_MAX_PARTS][PATH_MAX];
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void d13x_sdmc1_partition_handler(FAR struct partition_s *part,
                                         FAR void *arg)
{
  FAR struct d13x_sdmc1_partitions_s *partitions = arg;
  char path[PATH_MAX];
  int ret;

  if (partitions->count >= D13X_SDMC1_MAX_PARTS || part->nblocks == 0)
    {
      return;
    }

  ret = snprintf(path, sizeof(path), "%s%u", D13X_SDMC1_PART_PREFIX,
                 (unsigned int)part->index + 1);
  if (ret < 0 || ret >= (int)sizeof(path))
    {
      return;
    }

  ret = register_blockpartition(path, 0660, D13X_SDMC1_DEVICE,
                                part->firstblock, part->nblocks);
  if (ret < 0 && ret != -EEXIST)
    {
      return;
    }

  strlcpy(partitions->paths[partitions->count], path,
          sizeof(partitions->paths[partitions->count]));
  partitions->count++;
}

static bool d13x_sdmc1_is_mounted(void)
{
  struct statfs filesystem;

  return statfs(D13X_SDMC1_MOUNTPOINT, &filesystem) == 0 &&
         filesystem.f_type == FATFS_SUPER_MAGIC;
}

static int d13x_sdmc1_mount_device(FAR const char *device)
{
  if (mount(device, D13X_SDMC1_MOUNTPOINT, "fatfs",
            MS_NOSUID | MS_SYNCHRONOUS, NULL) == OK)
    {
      return OK;
    }

  if (errno == EBUSY && d13x_sdmc1_is_mounted())
    {
      return OK;
    }

  return -errno;
}

#ifdef CONFIG_D13X_SDMC0_WIFI
static int d13x_sdmc0_wifi_gpio(FAR unsigned int *group,
                                FAR unsigned int *pin)
{
  int gpio;

  gpio = hal_gpio_name2pin(CONFIG_D13X_SDMC0_WIFI_POWER_GPIO);
  if (gpio < 0)
    {
      return -EINVAL;
    }

  *group = GPIO_GROUP(gpio);
  *pin = GPIO_GROUP_PIN(gpio);
  return OK;
}

static int d13x_sdmc0_wifi_sendcmd(FAR struct sdio_dev_s *dev,
                                   uint32_t cmd, uint32_t arg)
{
  int ret;

  ret = SDIO_SENDCMD(dev, cmd, arg);
  if (ret < 0)
    {
      return ret;
    }

  return SDIO_WAITRESPONSE(dev, cmd);
}

static void d13x_sdmc0_wifi_parse_ocr(
    FAR struct d13x_sdmc0_wifi_probe_s *result, uint32_t ocr)
{
  result->ocr = ocr;
  result->rca = g_sdmc0_wifi_rca;
  result->ready = (ocr & D13X_SDMC0_WIFI_OCR_READY) != 0;
  result->function_count =
      (uint8_t)((ocr & D13X_SDMC0_WIFI_OCR_IOFUNCS_MASK) >>
                D13X_SDMC0_WIFI_OCR_IOFUNCS_SHIFT);
  result->memory_present = (ocr & D13X_SDMC0_WIFI_OCR_MEMORY) != 0;
}

static void d13x_sdmc0_wifi_cache_result(
    FAR struct d13x_sdmc0_wifi_probe_s *result)
{
  result->ocr = g_sdmc0_wifi_ocr;
  result->rca = g_sdmc0_wifi_rca;
  result->function_count = g_sdmc0_wifi_funcs;
  result->memory_present = g_sdmc0_wifi_memory;
  result->ready = g_sdmc0_wifi_ready;
}

static int d13x_sdmc0_wifi_select(FAR struct d13x_sdmc0_wifi_probe_s *result)
{
  uint32_t response;
  int ret;

  if (g_sdmc0_wifi_selected)
    {
      if (result != NULL)
        {
          result->rca = g_sdmc0_wifi_rca;
        }

      return OK;
    }

  ret = d13x_sdmc0_wifi_sendcmd(g_sdmc0, SD_CMD3, 0);
  if (ret < 0)
    {
      return ret;
    }

  ret = SDIO_RECVR6(g_sdmc0, SD_CMD3, &response);
  if (ret < 0)
    {
      return ret;
    }

  g_sdmc0_wifi_rca = (uint16_t)(response >> 16);

  ret = d13x_sdmc0_wifi_sendcmd(g_sdmc0, MMCSD_CMD7S,
                                (uint32_t)g_sdmc0_wifi_rca << 16);
  if (ret < 0)
    {
      return ret;
    }

  ret = SDIO_RECVR1(g_sdmc0, MMCSD_CMD7S, &response);
  if (ret < 0)
    {
      return ret;
    }

  SDIO_CLOCK(g_sdmc0, CLOCK_SD_TRANSFER_1BIT);

  g_sdmc0_wifi_selected = true;
  if (result != NULL)
    {
      result->rca = g_sdmc0_wifi_rca;
    }

  return OK;
}

static int d13x_sdmc0_wifi_cmd52_read(uint8_t function, uint32_t address,
                                      FAR uint8_t *value)
{
  uint32_t response;
  uint32_t arg;
  int ret;

  if (value == NULL)
    {
      return -EINVAL;
    }

  arg = ((uint32_t)(function & 0x07) << 28) |
        ((address & 0x0001fffful) << 9);

  ret = d13x_sdmc0_wifi_sendcmd(g_sdmc0, SDIO_CMD52, arg);
  if (ret < 0)
    {
      return ret;
    }

  ret = SDIO_RECVR5(g_sdmc0, SDIO_CMD52, &response);
  if (ret < 0)
    {
      return ret;
    }

  if ((response & D13X_SDMC0_WIFI_R5_INV_MASK) != 0)
    {
      return -EINVAL;
    }

  if ((response & D13X_SDMC0_WIFI_R5_ERR_MASK) != 0)
    {
      return -EIO;
    }

  *value = (uint8_t)(response & D13X_SDMC0_WIFI_R5_DATA_MASK);
  return OK;
}

static int d13x_sdmc0_wifi_cmd52_write(uint8_t function, uint32_t address,
                                       uint8_t value)
{
  uint32_t response;
  uint32_t arg;
  int ret;

  arg = (1ul << 31) | ((uint32_t)(function & 0x07) << 28) |
        ((address & 0x0001fffful) << 9) | value;

  ret = d13x_sdmc0_wifi_sendcmd(g_sdmc0, SDIO_CMD52, arg);
  if (ret < 0)
    {
      return ret;
    }

  ret = SDIO_RECVR5(g_sdmc0, SDIO_CMD52, &response);
  if (ret < 0)
    {
      return ret;
    }

  if ((response & D13X_SDMC0_WIFI_R5_INV_MASK) != 0)
    {
      return -EINVAL;
    }

  if ((response & D13X_SDMC0_WIFI_R5_ERR_MASK) != 0)
    {
      return -EIO;
    }

  return OK;
}

static int d13x_sdmc0_wifi_check_r5(uint32_t response)
{
  if ((response & D13X_SDMC0_WIFI_R5_INV_MASK) != 0)
    {
      return -EINVAL;
    }

  if ((response & D13X_SDMC0_WIFI_R5_ERR_MASK) != 0)
    {
      return -EIO;
    }

  return OK;
}

static int d13x_sdmc0_wifi_ensure_ready(void)
{
  struct d13x_sdmc0_wifi_probe_s probe;
  int ret;

  if (!g_sdmc0_wifi_ready || !g_sdmc0_wifi_selected)
    {
      ret = d13x_sdmc0_wifi_probe(&probe);
      if (ret < 0)
        {
          return ret;
        }

      if (!probe.ready || probe.function_count == 0)
        {
          return -ENODEV;
        }
    }

  return OK;
}

static int d13x_sdmc0_wifi_cmd53(bool write, uint8_t function,
                                 uint32_t address, bool inc_addr,
                                 FAR uint8_t *buffer, size_t length)
{
  sdio_eventset_t event;
  uint32_t response;
  uint32_t cmd;
  uint32_t arg;
  uint16_t blocklen;
  uint16_t nblocks;
  int ret;

  if (buffer == NULL || length == 0 || (length & 3) != 0 ||
      function == 0 || function > g_sdmc0_wifi_funcs)
    {
      return -EINVAL;
    }

  if (length >= 512 && (length % 512) == 0)
    {
      blocklen = 512;
      nblocks = (uint16_t)(length / 512);
      if (nblocks == 0 || nblocks > 511)
        {
          return -EINVAL;
        }
    }
  else
    {
      if (length > 512)
        {
          return -EINVAL;
        }

      blocklen = (uint16_t)length;
      nblocks = 0;
    }

  arg = ((uint32_t)(function & 0x07) << 28) |
        ((address & 0x0001fffful) << 9);

  if (nblocks == 0)
    {
      arg |= blocklen;
    }
  else
    {
      arg |= (1ul << 27) | nblocks;
    }

  if (write)
    {
      arg |= 1ul << 31;
    }

  if (inc_addr)
    {
      arg |= 1ul << 26;
    }

  cmd = write ? SDIO_CMD53WR : SDIO_CMD53RD;

  SDIO_BLOCKSETUP(g_sdmc0, blocklen, nblocks == 0 ? 1 : nblocks);
  SDIO_WAITENABLE(g_sdmc0, SDIOWAIT_TRANSFERDONE | SDIOWAIT_TIMEOUT |
                  SDIOWAIT_ERROR, D13X_SDMC0_WIFI_CMD53_TIMEOUT_MS);

  if (write)
    {
      ret = SDIO_SENDSETUP(g_sdmc0, buffer, length);
    }
  else
    {
      ret = SDIO_RECVSETUP(g_sdmc0, buffer, length);
    }

  if (ret < 0)
    {
      SDIO_CANCEL(g_sdmc0);
      return ret;
    }

  ret = SDIO_SENDCMD(g_sdmc0, cmd, arg);
  if (ret < 0)
    {
      SDIO_CANCEL(g_sdmc0);
      return ret;
    }

  ret = SDIO_WAITRESPONSE(g_sdmc0, cmd);
  if (ret < 0)
    {
      SDIO_CANCEL(g_sdmc0);
      return ret;
    }

  ret = SDIO_RECVR5(g_sdmc0, cmd, &response);
  if (ret < 0)
    {
      SDIO_CANCEL(g_sdmc0);
      return ret;
    }

  event = SDIO_EVENTWAIT(g_sdmc0);
  if ((event & SDIOWAIT_TIMEOUT) != 0)
    {
      return -ETIMEDOUT;
    }

  if ((event & SDIOWAIT_ERROR) != 0)
    {
      return -EIO;
    }

  return d13x_sdmc0_wifi_check_r5(response);
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#ifdef CONFIG_D13X_SDMC0_WIFI
int d13x_sdmc0_wifi_power(bool enable)
{
  unsigned int group;
  unsigned int pin;
  int ret;

  ret = d13x_sdmc0_wifi_gpio(&group, &pin);
  if (ret < 0)
    {
      return ret;
    }

  hal_gpio_set_value(group, pin, enable ? 1 : 0);
  hal_gpio_direction_output(group, pin);
  if (!enable)
    {
      g_sdmc0_wifi_ready = false;
      g_sdmc0_wifi_selected = false;
      g_sdmc0_wifi_ocr = 0;
      g_sdmc0_wifi_rca = 0;
      g_sdmc0_wifi_funcs = 0;
      g_sdmc0_wifi_memory = false;
    }

  up_mdelay(10);

  return OK;
}

int d13x_sdmc0_wifi_probe(FAR struct d13x_sdmc0_wifi_probe_s *result)
{
  uint32_t ocr = 0;
  uint32_t vdd;
  int i;
  int ret;

  if (result == NULL)
    {
      return -EINVAL;
    }

  result->ocr = 0;
  result->rca = 0;
  result->function_count = 0;
  result->memory_present = false;
  result->ready = false;

  if (g_sdmc0_wifi_ready && g_sdmc0_wifi_selected)
    {
      d13x_sdmc0_wifi_cache_result(result);
      return OK;
    }

  ret = d13x_sdmc0_wifi_power(true);
  if (ret < 0)
    {
      return ret;
    }

  if (g_sdmc0 == NULL)
    {
      g_sdmc0 = sdio_initialize(D13X_SDMC0_SLOT);
      if (g_sdmc0 == NULL)
        {
          return -ENODEV;
        }
    }

  sdio_mediachange(g_sdmc0, true);
  SDIO_RESET(g_sdmc0);
  SDIO_CLOCK(g_sdmc0, CLOCK_IDMODE);
  g_sdmc0_wifi_ready = false;
  g_sdmc0_wifi_selected = false;
  g_sdmc0_wifi_ocr = 0;
  g_sdmc0_wifi_rca = 0;
  g_sdmc0_wifi_funcs = 0;
  g_sdmc0_wifi_memory = false;

  ret = d13x_sdmc0_wifi_sendcmd(g_sdmc0, MMCSD_CMD0, 0);
  if (ret < 0)
    {
      return ret;
    }

  up_mdelay(2);

  ret = d13x_sdmc0_wifi_sendcmd(g_sdmc0, SDIO_CMD5, 0);
  if (ret < 0)
    {
      return ret;
    }

  ret = SDIO_RECVR4(g_sdmc0, SDIO_CMD5, &ocr);
  if (ret < 0)
    {
      return ret;
    }

  d13x_sdmc0_wifi_parse_ocr(result, ocr);
  g_sdmc0_wifi_ocr = result->ocr;
  g_sdmc0_wifi_funcs = result->function_count;
  g_sdmc0_wifi_memory = result->memory_present;
  if (result->function_count == 0)
    {
      return OK;
    }

  if (result->ready)
    {
      g_sdmc0_wifi_ready = true;
      g_sdmc0_wifi_funcs = result->function_count;
      return d13x_sdmc0_wifi_select(result);
    }

  vdd = ocr & D13X_SDMC0_WIFI_OCR_VDD_MASK;
  if (vdd == 0)
    {
      vdd = D13X_SDMC0_WIFI_OCR_VDD_MASK;
    }

  for (i = 0; i < D13X_SDMC0_WIFI_READY_RETRIES; i++)
    {
      ret = d13x_sdmc0_wifi_sendcmd(g_sdmc0, SDIO_CMD5, vdd);
      if (ret < 0)
        {
          return ret;
        }

      ret = SDIO_RECVR4(g_sdmc0, SDIO_CMD5, &ocr);
      if (ret < 0)
        {
          return ret;
        }

      d13x_sdmc0_wifi_parse_ocr(result, ocr);
      g_sdmc0_wifi_ocr = result->ocr;
      g_sdmc0_wifi_funcs = result->function_count;
      g_sdmc0_wifi_memory = result->memory_present;
      if (result->ready)
        {
          g_sdmc0_wifi_ready = true;
          break;
        }

      up_mdelay(10);
    }

  if (!result->ready)
    {
      return OK;
    }

  return d13x_sdmc0_wifi_select(result);
}

int d13x_sdmc0_wifi_readb(uint8_t function, uint32_t address,
                          FAR uint8_t *value)
{
  int ret;

  if (value == NULL)
    {
      return -EINVAL;
    }

  ret = d13x_sdmc0_wifi_ensure_ready();
  if (ret < 0)
    {
      return ret;
    }

  if (function > g_sdmc0_wifi_funcs)
    {
      return -EINVAL;
    }

  return d13x_sdmc0_wifi_cmd52_read(function, address, value);
}

int d13x_sdmc0_wifi_writeb(uint8_t function, uint32_t address,
                           uint8_t value)
{
  int ret;

  ret = d13x_sdmc0_wifi_ensure_ready();
  if (ret < 0)
    {
      return ret;
    }

  if (function > g_sdmc0_wifi_funcs)
    {
      return -EINVAL;
    }

  return d13x_sdmc0_wifi_cmd52_write(function, address, value);
}

int d13x_sdmc0_wifi_read(uint8_t function, uint32_t address, bool inc_addr,
                         FAR uint8_t *buffer, size_t length)
{
  int ret;

  ret = d13x_sdmc0_wifi_ensure_ready();
  if (ret < 0)
    {
      return ret;
    }

  return d13x_sdmc0_wifi_cmd53(false, function, address, inc_addr,
                               buffer, length);
}

int d13x_sdmc0_wifi_write(uint8_t function, uint32_t address, bool inc_addr,
                          FAR const uint8_t *buffer, size_t length)
{
  int ret;

  ret = d13x_sdmc0_wifi_ensure_ready();
  if (ret < 0)
    {
      return ret;
    }

  return d13x_sdmc0_wifi_cmd53(true, function, address, inc_addr,
                               (FAR uint8_t *)buffer, length);
}
#endif

int d13x_sdmc1_initialize(void)
{
  int ret;

  if (g_sdmc1 != NULL)
    {
      return OK;
    }

  g_sdmc1 = sdio_initialize(D13X_SDMC1_SLOT);
  if (g_sdmc1 == NULL)
    {
      return -ENODEV;
    }

  /* The first milestone requires a card to be inserted at boot and does not
   * start the shared hotplug polling worker.
   */

  sdio_mediachange(g_sdmc1, true);
  ret = mmcsd_slotinitialize(D13X_SDMC1_MINOR, g_sdmc1);
  if (ret < 0)
    {
      g_sdmc1 = NULL;
      return ret;
    }

  return OK;
}

int d13x_sdmc1_reprobe(void)
{
  FAR struct inode *inode;
  int ret;

  ret = find_blockdriver("/dev/mmcsd0", 0, &inode);
  if (ret >= 0)
    {
      close_blockdriver(inode);
      return OK;
    }

  ret = d13x_sdmc1_initialize();
  if (ret < 0)
    {
      return ret;
    }

  ret = find_blockdriver("/dev/mmcsd0", 0, &inode);
  if (ret >= 0)
    {
      close_blockdriver(inode);
      return OK;
    }

  syslog(LOG_ERR, "[D13X] SDMC1 initialization did not register "
                  "/dev/mmcsd0: %d\n", ret);
  return ret;
}

int d13x_sdmc1_mount(void)
{
  struct d13x_sdmc1_partitions_s partitions;
  unsigned int index;
  int mount_ret;
  int ret;

  if (d13x_sdmc1_is_mounted())
    {
      return OK;
    }

  ret = d13x_sdmc1_reprobe();
  if (ret < 0)
    {
      return ret;
    }

  mount_ret = d13x_sdmc1_mount_device(D13X_SDMC1_DEVICE);
  if (mount_ret == OK)
    {
      return OK;
    }

  memset(&partitions, 0, sizeof(partitions));
  ret = parse_block_partition(D13X_SDMC1_DEVICE,
                              d13x_sdmc1_partition_handler,
                              &partitions);
  if (ret < 0 && ret != -EINVAL)
    {
      return ret;
    }

  ret = mount_ret;

  for (index = 0; index < partitions.count; index++)
    {
      ret = d13x_sdmc1_mount_device(partitions.paths[index]);
      if (ret == OK)
        {
          return OK;
        }
    }

  return ret;
}
