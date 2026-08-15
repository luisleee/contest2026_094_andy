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
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nuttx/fs/fs.h>
#include <nuttx/fs/ioctl.h>
#include <nuttx/fs/partition.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define TF_TEST_DEVICE       "/dev/mmcsd0"
#define TF_TEST_PART_PREFIX  "/dev/mmcsd0p"
#define TF_TEST_MOUNTPOINT   "/sdcard"
#define TF_TEST_PATH         "/sdcard/tf_test.bin"
#define TF_TEST_SIZE         4096u
#define TF_TEST_MAX_PARTS    4
#define TF_TEST_SECTOR_DUMP  128u

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

extern int d13x_sdmc1_reprobe(void);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

struct tf_test_partitions_s
{
  uint8_t count;
  char paths[TF_TEST_MAX_PARTS][PATH_MAX];
};

static void tf_test_partition_handler(FAR struct partition_s *part,
                                      FAR void *arg)
{
  FAR struct tf_test_partitions_s *partitions = arg;
  char path[PATH_MAX];
  int ret;

  if (partitions->count >= TF_TEST_MAX_PARTS || part->nblocks == 0)
    {
      return;
    }

  ret = snprintf(path, sizeof(path), "%s%u", TF_TEST_PART_PREFIX,
                 (unsigned int)part->index + 1);
  if (ret < 0 || ret >= (int)sizeof(path))
    {
      return;
    }

  ret = register_blockpartition(path, 0660, TF_TEST_DEVICE,
                                part->firstblock, part->nblocks);
  if (ret < 0 && ret != -EEXIST)
    {
      printf("tf_test: register %s failed: %s\n", path, strerror(-ret));
      return;
    }

  strlcpy(partitions->paths[partitions->count], path,
          sizeof(partitions->paths[partitions->count]));
  partitions->count++;
}

static int tf_test_reprobe(void)
{
  return d13x_sdmc1_reprobe();
}

static int tf_test_scan_partitions(FAR struct tf_test_partitions_s *partitions)
{
  int ret;

  memset(partitions, 0, sizeof(*partitions));

  ret = parse_block_partition(TF_TEST_DEVICE, tf_test_partition_handler,
                              partitions);
  if (ret < 0)
    {
      return ret == -EINVAL ? OK : ret;
    }

  return OK;
}

static bool tf_test_is_mounted(void)
{
  struct statfs filesystem;

  return statfs(TF_TEST_MOUNTPOINT, &filesystem) == 0 &&
         filesystem.f_type == FATFS_SUPER_MAGIC;
}

static int tf_test_require_mounted(void)
{
  if (tf_test_is_mounted())
    {
      return OK;
    }

  printf("tf_test: %s is not a mounted FAT filesystem\n",
         TF_TEST_MOUNTPOINT);
  printf("tf_test: refusing to access an unmounted path; run "
         "'tf_test mount' first\n");
  return -ENODEV;
}

static int tf_test_mount_device(FAR const char *device, bool verbose)
{
  int ret;

  ret = mount(device, TF_TEST_MOUNTPOINT, "fatfs",
              MS_NOSUID | MS_SYNCHRONOUS, NULL);
  if (ret < 0)
    {
      if (errno == EBUSY && tf_test_is_mounted())
        {
          if (verbose)
            {
              printf("tf_test: %s is already mounted\n", TF_TEST_MOUNTPOINT);
            }

          return OK;
        }

      if (verbose)
        {
          printf("tf_test: mount %s at %s failed: %s\n",
                 device, TF_TEST_MOUNTPOINT, strerror(errno));
        }

      return -errno;
    }

  if (verbose)
    {
      printf("tf_test: mounted %s at %s as fatfs\n",
             device, TF_TEST_MOUNTPOINT);
    }

  return OK;
}

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
  struct tf_test_partitions_s partitions;
  struct stat device_stat;
  struct statfs filesystem;
  uint64_t available_bytes;
  uint64_t total_bytes;
  int part_ret;
  int ret;

  memset(&partitions, 0, sizeof(partitions));

  ret = tf_test_reprobe();
  if (ret < 0)
    {
      printf("tf_test: SD card is not ready: %s\n", strerror(-ret));
      return 1;
    }

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
  part_ret = tf_test_scan_partitions(&partitions);
  if (part_ret < 0)
    {
      printf("Partition: scan failed: %s; trying whole-card filesystem\n",
             strerror(-part_ret));
    }
  else if (partitions.count > 0)
    {
      printf("Partition: %s\n", partitions.paths[0]);
    }
  else
    {
      printf("Partition: none; trying whole-card filesystem\n");
    }

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

  if (filesystem.f_type != FATFS_SUPER_MAGIC)
    {
      printf("Filesystem: not mounted; run 'tf_test mount' for capacity "
             "information\n");
      return 0;
    }

  total_bytes = (uint64_t)filesystem.f_blocks * filesystem.f_bsize;
  available_bytes = (uint64_t)filesystem.f_bavail * filesystem.f_bsize;
  printf("Filesystem: fatfs at %s\n", TF_TEST_MOUNTPOINT);
  printf("Capacity: %llu bytes, available: %llu bytes, block: %lu bytes\n",
         (unsigned long long)total_bytes,
         (unsigned long long)available_bytes,
         (unsigned long)filesystem.f_bsize);
  return 0;
}

static int tf_test_mount(void)
{
  struct tf_test_partitions_s partitions;
  unsigned int i;
  int ret;

  ret = tf_test_reprobe();
  if (ret < 0)
    {
      printf("tf_test: SD card is not ready: %s\n", strerror(-ret));
      return 1;
    }

  ret = tf_test_mount_device(TF_TEST_DEVICE, true);
  if (ret == OK)
    {
      return 0;
    }

  ret = tf_test_scan_partitions(&partitions);
  if (ret < 0)
    {
      printf("tf_test: partition scan failed: %s\n", strerror(-ret));
      return 1;
    }

  for (i = 0; i < partitions.count; i++)
    {
      ret = tf_test_mount_device(partitions.paths[i], true);
      if (ret == OK)
        {
          return 0;
        }
    }

  printf("tf_test: card must contain a supported FAT filesystem; "
         "no format was attempted\n");
  if (partitions.count > 0)
    {
      printf("tf_test: tried %u partition(s) before whole-card fallback\n",
             (unsigned int)partitions.count);
    }

  return 1;
}

static void tf_test_dump_line(FAR const uint8_t *buffer, unsigned int offset,
                              unsigned int length)
{
  unsigned int i;

  printf("  %04x:", offset);
  for (i = 0; i < length; i++)
    {
      printf(" %02x", buffer[offset + i]);
    }

  printf("\n");
}

static uint16_t tf_test_le16(FAR const uint8_t *buffer)
{
  return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
}

static uint32_t tf_test_le32(FAR const uint8_t *buffer)
{
  return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) |
         ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 24);
}

static int tf_test_sector(void)
{
  FAR struct inode *inode;
  FAR uint8_t *sector;
  struct geometry geo;
  unsigned int dump;
  unsigned int i;
  int ret;

  ret = tf_test_reprobe();
  if (ret < 0)
    {
      printf("tf_test: SD card is not ready: %s\n", strerror(-ret));
      return 1;
    }

  ret = open_blockdriver(TF_TEST_DEVICE, MS_RDONLY, &inode);
  if (ret < 0)
    {
      printf("tf_test: open block %s failed: %s\n",
             TF_TEST_DEVICE, strerror(-ret));
      return 1;
    }

  memset(&geo, 0, sizeof(geo));
  ret = inode->u.i_bops->geometry(inode, &geo);
  if (ret < 0)
    {
      printf("tf_test: geometry failed: %s\n", strerror(-ret));
      close_blockdriver(inode);
      return 1;
    }

  printf("Geometry: available=%d changed=%d writable=%d sectors=%llu "
         "sector_size=%lu\n",
         geo.geo_available, geo.geo_mediachanged, geo.geo_writeenabled,
         (unsigned long long)geo.geo_nsectors,
         (unsigned long)geo.geo_sectorsize);

  if (!geo.geo_available || geo.geo_sectorsize == 0)
    {
      close_blockdriver(inode);
      return 1;
    }

  sector = malloc(geo.geo_sectorsize);
  if (sector == NULL)
    {
      printf("tf_test: sector buffer allocation failed\n");
      close_blockdriver(inode);
      return 1;
    }

  memset(sector, 0, geo.geo_sectorsize);
  ret = inode->u.i_bops->read(inode, sector, 0, 1);
  if (ret < 0)
    {
      printf("tf_test: read sector 0 failed: %s (%d)\n",
             strerror(-ret), ret);
      free(sector);
      close_blockdriver(inode);
      return 1;
    }

  printf("Read sector 0: %d block(s)\n", ret);
  if (ret != 1)
    {
      printf("tf_test: short block read\n");
      free(sector);
      close_blockdriver(inode);
      return 1;
    }

  dump = geo.geo_sectorsize < TF_TEST_SECTOR_DUMP ?
         geo.geo_sectorsize : TF_TEST_SECTOR_DUMP;
  for (i = 0; i < dump; i += 16)
    {
      tf_test_dump_line(sector, i, dump - i > 16 ? 16 : dump - i);
    }

  if (geo.geo_sectorsize >= 512)
    {
      printf("MBR partition table:\n");
      for (i = 0x1b0; i < 0x200; i += 16)
        {
          tf_test_dump_line(sector, i, 16);
        }

      printf("Signature: 0x%02x 0x%02x%s\n", sector[510], sector[511],
             sector[510] == 0x55 && sector[511] == 0xaa ? " (valid)" :
             " (invalid)");
      printf("BPB: bytes_per_sector=%u sectors_per_cluster=%u "
             "reserved=%u fats=%u total16=%u total32=%lu "
             "fat32_size=%lu fs=\"%.8s\"\n",
             tf_test_le16(&sector[11]), sector[13],
             tf_test_le16(&sector[14]), sector[16],
             tf_test_le16(&sector[19]),
             (unsigned long)tf_test_le32(&sector[32]),
             (unsigned long)tf_test_le32(&sector[36]),
             &sector[82]);
    }

  free(sector);
  close_blockdriver(inode);
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

  if (tf_test_require_mounted() < 0)
    {
      return 1;
    }

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

  if (tf_test_require_mounted() < 0)
    {
      return 1;
    }

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
  if (tf_test_require_mounted() < 0)
    {
      return 1;
    }

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
  printf("Usage: tf_test info|sector|mount|write|check|clear|umount\n");
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

  if (strcmp(argv[1], "sector") == 0)
    {
      return tf_test_sector();
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
