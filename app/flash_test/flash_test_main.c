/****************************************************************************
 * contest2026_094_andy/app/flash_test/flash_test_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arch/chip/d13x_spinor.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define FLASH_TEST_DEVICE              "/dev/nor0"
#define FLASH_TEST_READ_DEFAULT_LEN    64u
#define FLASH_TEST_READ_MAX_LEN        256u
#define FLASH_TEST_VERIFY_DEFAULT_LEN  65536u
#define FLASH_TEST_VERIFY_MAX_LEN      1048576u
#define FLASH_TEST_BUFFER_SIZE         256u
#define FLASH_TEST_FS_PATH             "/data/spi_test.bin"
#define FLASH_TEST_FS_SIZE             1024u

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int flash_test_parse_u32(FAR const char *text, uint32_t maximum,
                                FAR uint32_t *value)
{
  FAR char *endptr;
  unsigned long parsed;

  errno = 0;
  parsed = strtoul(text, &endptr, 0);
  if (errno != 0 || *text == '\0' || *endptr != '\0' || parsed > maximum)
    {
      return -EINVAL;
    }

  *value = parsed;
  return OK;
}

static int flash_test_get_info(FAR struct d13x_spinor_info_s *info)
{
  int ret;

  ret = d13x_spinor_get_info(info);
  if (ret < 0)
    {
      printf("flash_test: SPI NOR is unavailable: %s\n", strerror(-ret));
      return ret;
    }

  return OK;
}

static int flash_test_info(void)
{
  struct d13x_spinor_partition_s partition;
  struct d13x_spinor_partition_s whole;
  struct d13x_spinor_info_s info;
  bool found_whole = false;
  uint32_t count;
  uint32_t index;
  int ret;

  ret = flash_test_get_info(&info);
  if (ret < 0)
    {
      return 1;
    }

  printf("flash_test: chip=%s jedec=0x%02x%02x%02x capacity=%lu "
         "erase=%lu bus=%lu Hz\n",
         info.name, info.manufacturer_id, info.memory_type_id,
         info.capacity_id, (unsigned long)info.capacity,
         (unsigned long)info.erase_size,
         (unsigned long)info.bus_frequency);
  printf("flash_test: SFDP=%s revision=%u.%u capacity=%lu\n",
         info.sfdp_available ? "yes" : "no", info.sfdp_major,
         info.sfdp_minor, (unsigned long)info.sfdp_capacity);

  count = d13x_spinor_get_partition_count();
  printf("flash_test: MTD devices=%lu\n", (unsigned long)count);
  for (index = 0; index < count; index++)
    {
      if (d13x_spinor_get_partition(index, &partition) == OK)
        {
          printf("  [%lu] %-12s start=0x%08lx size=0x%08lx erase=%lu\n",
                 (unsigned long)index, partition.name,
                 (unsigned long)partition.start,
                 (unsigned long)partition.size,
                 (unsigned long)partition.erase_size);
          if (strcmp(partition.name, FLASH_TEST_DEVICE) == 0)
            {
              whole = partition;
              found_whole = true;
            }
        }
    }

  if (!found_whole)
    {
      printf("flash_test: FAIL; whole-flash MTD object is missing\n");
      return 1;
    }

  if (info.capacity != whole.size || info.erase_size != whole.erase_size)
    {
      printf("flash_test: FAIL; SFUD and MTD capacities differ\n");
      return 1;
    }

  printf("flash_test: PASS; SPI NOR and MTD geometry agree\n");
  return 0;
}

static int flash_test_validate_range(uint32_t offset, uint32_t length,
                                     uint32_t maximum_length)
{
  struct d13x_spinor_info_s info;
  int ret;

  if (length == 0 || length > maximum_length)
    {
      return -EINVAL;
    }

  ret = flash_test_get_info(&info);
  if (ret < 0)
    {
      return ret;
    }

  if (offset >= info.capacity || length > info.capacity - offset)
    {
      return -ERANGE;
    }

  return OK;
}

static int flash_test_read(uint32_t offset, uint32_t length)
{
  uint8_t buffer[FLASH_TEST_READ_MAX_LEN];
  uint32_t line;
  uint32_t byte;
  int ret;

  ret = flash_test_validate_range(offset, length, FLASH_TEST_READ_MAX_LEN);
  if (ret < 0)
    {
      printf("flash_test: invalid read range: %s\n", strerror(-ret));
      return 1;
    }

  ret = d13x_spinor_read(offset, buffer, length);
  if (ret < 0)
    {
      printf("flash_test: read failed: %s\n", strerror(-ret));
      return 1;
    }

  for (line = 0; line < length; line += 16u)
    {
      printf("%08lx:", (unsigned long)(offset + line));
      for (byte = 0; byte < 16u && line + byte < length; byte++)
        {
          printf(" %02x", buffer[line + byte]);
        }

      printf("\n");
    }

  printf("flash_test: PASS; read %lu bytes at 0x%08lx\n",
         (unsigned long)length, (unsigned long)offset);
  return 0;
}

static uint32_t flash_test_crc32_update(uint32_t crc,
                                        FAR const uint8_t *buffer,
                                        uint32_t length)
{
  uint32_t mask;
  unsigned int bit;

  while (length-- > 0)
    {
      crc ^= *buffer++;
      for (bit = 0; bit < 8; bit++)
        {
          mask = (uint32_t)-(int32_t)(crc & 1u);
          crc = (crc >> 1) ^ (0xedb88320u & mask);
        }
    }

  return crc;
}

static int flash_test_checksum(uint32_t offset, uint32_t length,
                               FAR uint32_t *checksum)
{
  uint8_t buffer[FLASH_TEST_BUFFER_SIZE];
  uint32_t chunk;
  uint32_t crc = UINT32_MAX;
  int ret;

  while (length > 0)
    {
      chunk = length > sizeof(buffer) ? sizeof(buffer) : length;
      ret = d13x_spinor_read(offset, buffer, chunk);
      if (ret < 0)
        {
          return ret;
        }

      crc = flash_test_crc32_update(crc, buffer, chunk);
      offset += chunk;
      length -= chunk;
    }

  *checksum = ~crc;
  return OK;
}

static int flash_test_verify(uint32_t offset, uint32_t length)
{
  uint32_t first;
  uint32_t second;
  int ret;

  ret = flash_test_validate_range(offset, length, FLASH_TEST_VERIFY_MAX_LEN);
  if (ret < 0)
    {
      printf("flash_test: invalid verify range: %s\n", strerror(-ret));
      return 1;
    }

  ret = flash_test_checksum(offset, length, &first);
  if (ret == OK)
    {
      ret = flash_test_checksum(offset, length, &second);
    }

  if (ret < 0)
    {
      printf("flash_test: verify read failed: %s\n", strerror(-ret));
      return 1;
    }

  printf("flash_test: pass1=0x%08lx pass2=0x%08lx range=0x%08lx+%lu\n",
         (unsigned long)first, (unsigned long)second,
         (unsigned long)offset, (unsigned long)length);
  if (first != second)
    {
      printf("flash_test: FAIL; repeated reads differ\n");
      return 1;
    }

  printf("flash_test: PASS; repeated read CRC32 values agree\n");
  return 0;
}

static uint8_t flash_test_fs_pattern(uint32_t offset)
{
  return (uint8_t)((offset * 37u + 0x5au) & 0xffu);
}

static int flash_test_fs_write_exact(int fd, FAR const uint8_t *buffer,
                                     uint32_t length)
{
  ssize_t written;

  while (length > 0)
    {
      written = write(fd, buffer, length);
      if (written < 0)
        {
          return -errno;
        }

      if (written == 0)
        {
          return -EIO;
        }

      buffer += written;
      length -= written;
    }

  return OK;
}

static int flash_test_fs_read_exact(int fd, FAR uint8_t *buffer,
                                    uint32_t length)
{
  ssize_t received;

  while (length > 0)
    {
      received = read(fd, buffer, length);
      if (received < 0)
        {
          return -errno;
        }

      if (received == 0)
        {
          return -EIO;
        }

      buffer += received;
      length -= received;
    }

  return OK;
}

static int flash_test_fs_write(void)
{
  uint8_t buffer[FLASH_TEST_FS_SIZE];
  uint32_t offset;
  int fd;
  int ret;

  for (offset = 0; offset < sizeof(buffer); offset++)
    {
      buffer[offset] = flash_test_fs_pattern(offset);
    }

  fd = open(FLASH_TEST_FS_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
    {
      printf("flash_test: open %s failed: %s\n",
             FLASH_TEST_FS_PATH, strerror(errno));
      return 1;
    }

  ret = flash_test_fs_write_exact(fd, buffer, sizeof(buffer));
  if (ret == OK && fsync(fd) < 0)
    {
      ret = -errno;
    }

  close(fd);
  if (ret < 0)
    {
      printf("flash_test: filesystem write failed: %s\n", strerror(-ret));
      return 1;
    }

  printf("flash_test: wrote and synced %u bytes to %s\n",
         FLASH_TEST_FS_SIZE, FLASH_TEST_FS_PATH);
  printf("flash_test: run 'flash_test fs check' now and after reboot\n");
  return 0;
}

static int flash_test_fs_check(void)
{
  uint8_t buffer[FLASH_TEST_FS_SIZE];
  uint8_t extra;
  uint32_t offset;
  ssize_t received;
  int fd;
  int ret;

  fd = open(FLASH_TEST_FS_PATH, O_RDONLY);
  if (fd < 0)
    {
      printf("flash_test: open %s failed: %s\n",
             FLASH_TEST_FS_PATH, strerror(errno));
      return 1;
    }

  ret = flash_test_fs_read_exact(fd, buffer, sizeof(buffer));
  if (ret == OK)
    {
      received = read(fd, &extra, 1);
      if (received < 0)
        {
          ret = -errno;
        }
      else if (received != 0)
        {
          ret = -EFBIG;
        }
    }

  close(fd);
  if (ret < 0)
    {
      printf("flash_test: filesystem read failed: %s\n", strerror(-ret));
      return 1;
    }

  for (offset = 0; offset < sizeof(buffer); offset++)
    {
      if (buffer[offset] != flash_test_fs_pattern(offset))
        {
          printf("flash_test: FAIL; data mismatch at byte %lu\n",
                 (unsigned long)offset);
          return 1;
        }
    }

  printf("flash_test: PASS; %s retained %u verified bytes\n",
         FLASH_TEST_FS_PATH, FLASH_TEST_FS_SIZE);
  return 0;
}

static int flash_test_fs_clear(void)
{
  if (unlink(FLASH_TEST_FS_PATH) < 0 && errno != ENOENT)
    {
      printf("flash_test: unlink %s failed: %s\n",
             FLASH_TEST_FS_PATH, strerror(errno));
      return 1;
    }

  sync();
  printf("flash_test: removed %s\n", FLASH_TEST_FS_PATH);
  return 0;
}

static void flash_test_usage(void)
{
  printf("Usage: flash_test info\n");
  printf("       flash_test read [offset [length 1..256]]\n");
  printf("       flash_test verify [offset [length 1..1048576]]\n");
  printf("       flash_test fs write|check|clear\n");
  printf("Raw flash commands are read-only; fs writes only under /data.\n");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  uint32_t offset = 0;
  uint32_t length;

  if (argc == 2 && strcmp(argv[1], "info") == 0)
    {
      return flash_test_info();
    }

  if (argc >= 2 && strcmp(argv[1], "read") == 0)
    {
      length = FLASH_TEST_READ_DEFAULT_LEN;
      if (argc > 4 ||
          (argc >= 3 && flash_test_parse_u32(argv[2], UINT32_MAX,
                                             &offset) < 0) ||
          (argc == 4 &&
           flash_test_parse_u32(argv[3], FLASH_TEST_READ_MAX_LEN,
                                &length) < 0))
        {
          flash_test_usage();
          return 1;
        }

      return flash_test_read(offset, length);
    }

  if (argc >= 2 && strcmp(argv[1], "verify") == 0)
    {
      length = FLASH_TEST_VERIFY_DEFAULT_LEN;
      if (argc > 4 ||
          (argc >= 3 && flash_test_parse_u32(argv[2], UINT32_MAX,
                                             &offset) < 0) ||
          (argc == 4 &&
           flash_test_parse_u32(argv[3], FLASH_TEST_VERIFY_MAX_LEN,
                                &length) < 0))
        {
          flash_test_usage();
          return 1;
        }

      return flash_test_verify(offset, length);
    }

  if (argc == 3 && strcmp(argv[1], "fs") == 0)
    {
      if (strcmp(argv[2], "write") == 0)
        {
          return flash_test_fs_write();
        }

      if (strcmp(argv[2], "check") == 0)
        {
          return flash_test_fs_check();
        }

      if (strcmp(argv[2], "clear") == 0)
        {
          return flash_test_fs_clear();
        }
    }

  flash_test_usage();
  return 1;
}
