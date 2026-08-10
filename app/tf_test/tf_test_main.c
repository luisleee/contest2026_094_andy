/****************************************************************************
 * contest2026_094_andy/app/tf_test/tf_test_main.c
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statfs.h>

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define TF_TEST_DEVICE     "/dev/mmcsd1"
#define TF_TEST_MOUNTPOINT "/sdcard"
#define TF_TEST_PATH       "/sdcard/tf_test.bin"
#define TF_TEST_SIZE       4096u

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint8_t tf_test_pattern(uint32_t offset)
{
  return (uint8_t)(((offset * 37u) + (offset >> 3) + 0x5au) & 0xffu);
}

static int tf_test_write_exact(int fd, FAR const uint8_t *buffer,
                               size_t length)
{
  size_t done = 0;

  while (done < length)
    {
      ssize_t written = write(fd, buffer + done, length - done);

      if (written < 0)
        {
          return -errno;
        }

      if (written == 0)
        {
          return -EIO;
        }

      done += written;
    }

  return OK;
}

static int tf_test_read_exact(int fd, FAR uint8_t *buffer, size_t length)
{
  size_t done = 0;

  while (done < length)
    {
      ssize_t received = read(fd, buffer + done, length - done);

      if (received < 0)
        {
          return -errno;
        }

      if (received == 0)
        {
          return -ENODATA;
        }

      done += received;
    }

  return OK;
}

static int tf_test_info(void)
{
  struct stat device_stat;
  struct statfs filesystem;
  uint64_t available_bytes;
  uint64_t total_bytes;

  if (stat(TF_TEST_DEVICE, &device_stat) < 0)
    {
      printf("tf_test: stat %s failed: %s\n",
             TF_TEST_DEVICE, strerror(errno));
      return 1;
    }

  if (!S_ISBLK(device_stat.st_mode))
    {
      printf("tf_test: %s is not a block device\n", TF_TEST_DEVICE);
      return 1;
    }

  printf("Device: %s (block device)\n", TF_TEST_DEVICE);

  if (statfs(TF_TEST_MOUNTPOINT, &filesystem) < 0)
    {
      if (errno == ENOENT)
        {
          printf("Filesystem: not mounted; run 'tf_test mount' for capacity "
                 "information\n");
          return 0;
        }

      printf("tf_test: statfs %s failed: %s\n",
             TF_TEST_MOUNTPOINT, strerror(errno));
      return 1;
    }

  total_bytes = (uint64_t)filesystem.f_blocks * filesystem.f_bsize;
  available_bytes = (uint64_t)filesystem.f_bavail * filesystem.f_bsize;
  printf("Filesystem: %s at %s\n",
         filesystem.f_type == FATFS_SUPER_MAGIC ? "fatfs" : "unknown",
         TF_TEST_MOUNTPOINT);
  printf("Capacity: %llu bytes, available: %llu bytes, block: %lu bytes\n",
         (unsigned long long)total_bytes,
         (unsigned long long)available_bytes,
         (unsigned long)filesystem.f_bsize);
  return 0;
}

static int tf_test_mount(void)
{
  int ret;

  ret = mount(TF_TEST_DEVICE, TF_TEST_MOUNTPOINT, "fatfs",
              MS_NOSUID | MS_SYNCHRONOUS, NULL);
  if (ret < 0)
    {
      if (errno == EBUSY)
        {
          printf("tf_test: %s is already mounted\n", TF_TEST_MOUNTPOINT);
          return 0;
        }

      printf("tf_test: mount %s at %s failed: %s\n",
             TF_TEST_DEVICE, TF_TEST_MOUNTPOINT, strerror(errno));
      printf("tf_test: card must contain a supported FAT filesystem; "
             "no format was attempted\n");
      return 1;
    }

  printf("tf_test: mounted %s at %s as fatfs\n",
         TF_TEST_DEVICE, TF_TEST_MOUNTPOINT);
  return 0;
}

static int tf_test_umount(void)
{
  if (umount(TF_TEST_MOUNTPOINT) < 0)
    {
      printf("tf_test: umount %s failed: %s\n",
             TF_TEST_MOUNTPOINT, strerror(errno));
      return 1;
    }

  printf("tf_test: unmounted %s\n", TF_TEST_MOUNTPOINT);
  return 0;
}

static int tf_test_write(void)
{
  FAR uint8_t *buffer;
  uint32_t offset;
  int fd;
  int ret;

  buffer = malloc(TF_TEST_SIZE);
  if (buffer == NULL)
    {
      printf("tf_test: buffer allocation failed\n");
      return 1;
    }

  for (offset = 0; offset < TF_TEST_SIZE; offset++)
    {
      buffer[offset] = tf_test_pattern(offset);
    }

  fd = open(TF_TEST_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
    {
      printf("tf_test: open %s failed: %s\n", TF_TEST_PATH, strerror(errno));
      free(buffer);
      return 1;
    }

  ret = tf_test_write_exact(fd, buffer, TF_TEST_SIZE);
  if (ret == OK && fsync(fd) < 0)
    {
      ret = -errno;
    }

  if (close(fd) < 0 && ret == OK)
    {
      ret = -errno;
    }

  free(buffer);
  if (ret < 0)
    {
      printf("tf_test: write failed: %s\n", strerror(-ret));
      return 1;
    }

  sync();
  printf("tf_test: wrote and synced %u bytes to %s\n",
         TF_TEST_SIZE, TF_TEST_PATH);
  return 0;
}

static int tf_test_check(void)
{
  FAR uint8_t *buffer;
  uint8_t extra;
  uint32_t offset;
  ssize_t received;
  int fd;
  int ret;

  buffer = malloc(TF_TEST_SIZE);
  if (buffer == NULL)
    {
      printf("tf_test: buffer allocation failed\n");
      return 1;
    }

  fd = open(TF_TEST_PATH, O_RDONLY);
  if (fd < 0)
    {
      printf("tf_test: open %s failed: %s\n", TF_TEST_PATH, strerror(errno));
      free(buffer);
      return 1;
    }

  ret = tf_test_read_exact(fd, buffer, TF_TEST_SIZE);
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
      printf("tf_test: read failed: %s\n", strerror(-ret));
      free(buffer);
      return 1;
    }

  for (offset = 0; offset < TF_TEST_SIZE; offset++)
    {
      if (buffer[offset] != tf_test_pattern(offset))
        {
          printf("tf_test: FAIL; mismatch at byte %lu\n",
                 (unsigned long)offset);
          free(buffer);
          return 1;
        }
    }

  free(buffer);
  printf("tf_test: PASS; verified %u bytes in %s\n",
         TF_TEST_SIZE, TF_TEST_PATH);
  return 0;
}

static int tf_test_clear(void)
{
  if (unlink(TF_TEST_PATH) < 0 && errno != ENOENT)
    {
      printf("tf_test: unlink %s failed: %s\n",
             TF_TEST_PATH, strerror(errno));
      return 1;
    }

  sync();
  printf("tf_test: removed %s\n", TF_TEST_PATH);
  return 0;
}

static void tf_test_usage(void)
{
  printf("Usage: tf_test info|mount|write|check|clear|umount\n");
  printf("No command formats the card; writes are limited to %s.\n",
         TF_TEST_PATH);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  if (argc != 2)
    {
      tf_test_usage();
      return 1;
    }

  if (strcmp(argv[1], "info") == 0)
    {
      return tf_test_info();
    }

  if (strcmp(argv[1], "mount") == 0)
    {
      return tf_test_mount();
    }

  if (strcmp(argv[1], "write") == 0)
    {
      return tf_test_write();
    }

  if (strcmp(argv[1], "check") == 0)
    {
      return tf_test_check();
    }

  if (strcmp(argv[1], "clear") == 0)
    {
      return tf_test_clear();
    }

  if (strcmp(argv[1], "umount") == 0)
    {
      return tf_test_umount();
    }

  tf_test_usage();
  return 1;
}
