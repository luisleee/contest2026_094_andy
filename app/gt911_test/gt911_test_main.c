/****************************************************************************
 * contest2026_094_andy/app/gt911_test/gt911_test_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <nuttx/i2c/i2c_master.h>
#include <nuttx/video/fb.h>

#include <arch/board/board.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define GT911_I2C_DEVICE              "/dev/i2c2"
#define GT911_I2C_FREQUENCY           400000

#define GT911_CONFIG_REG              0x8047
#define GT911_CONFIG_SIZE             186
#define GT911_CONFIG_DATA_SIZE        (GT911_CONFIG_SIZE - 2)
#define GT911_PRODUCT_ID_REG          0x8140
#define GT911_VERSION_INFO_SIZE       11
#define GT911_TOUCH_STATUS_REG        0x814e
#define GT911_POINT_DATA_REG          0x814f

#define GT911_STATUS_READY            (1u << 7)
#define GT911_STATUS_POINT_MASK       0x0f
#define GT911_MAX_TOUCHES             5
#define GT911_POINT_SIZE              8

#define GT911_DEFAULT_SECONDS         15u
#define GT911_MIN_SECONDS             1u
#define GT911_MAX_SECONDS             300u
#define GT911_POLL_INTERVAL_US        10000u

#define GT911_FB_DEVICE               "/dev/fb0"
#define GT911_FB_WIDTH                1024u
#define GT911_FB_HEIGHT               600u
#define GT911_FB_BPP                  16u

#define GT911_TARGET_TOP_LEFT         (1u << 0)
#define GT911_TARGET_TOP_RIGHT        (1u << 1)
#define GT911_TARGET_CENTER           (1u << 2)
#define GT911_TARGET_BOTTOM_LEFT      (1u << 3)
#define GT911_TARGET_BOTTOM_RIGHT     (1u << 4)
#define GT911_TARGET_ALL              0x1fu

#define GT911_COLOR_BACKGROUND        0x0841u
#define GT911_COLOR_GRID              0x4208u
#define GT911_COLOR_FOREGROUND        0xffffu
#define GT911_TARGET_SIZE             52u
#define GT911_TARGET_BORDER           4u

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct gt911_info_s
{
  uint8_t address;
  uint8_t version[GT911_VERSION_INFO_SIZE];
  uint8_t config[GT911_CONFIG_SIZE];
  bool config_valid;
};

struct gt911_point_s
{
  uint8_t id;
  uint16_t x;
  uint16_t y;
  uint16_t size;
};

struct gt911_visual_s
{
  int fd;
  uint8_t *framebuffer;
  uint32_t stride;
  bool active;
};

struct gt911_stats_s
{
  struct gt911_point_s last[16];
  uint32_t ready_frames;
  uint32_t touch_frames;
  uint32_t samples;
  uint32_t down_events;
  uint32_t move_events;
  uint32_t up_events;
  uint16_t active_ids;
  uint16_t observed_ids;
  uint16_t min_x;
  uint16_t max_x;
  uint16_t min_y;
  uint16_t max_y;
  uint8_t max_points;
  uint8_t targets;
  bool have_range;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const uint16_t g_gt911_id_colors[GT911_MAX_TOUCHES] =
{
  0xf800u,
  0xffe0u,
  0x07e0u,
  0x07ffu,
  0xf81fu
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint16_t gt911_getle16(const uint8_t *buffer)
{
  return (uint16_t)buffer[0] | (uint16_t)buffer[1] << 8;
}

static uint64_t gt911_milliseconds(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000u +
         (uint64_t)ts.tv_nsec / 1000000u;
}

static int gt911_read_reg(int fd, uint8_t address, uint16_t reg,
                          uint8_t *buffer, size_t length)
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

  struct i2c_transfer_s transfer =
  {
    .msgv = messages,
    .msgc = 2,
  };

  int ret;

  ret = ioctl(fd, I2CIOC_TRANSFER,
              (unsigned long)((uintptr_t)&transfer));
  if (ret < 0)
    {
      return -errno;
    }

  return ret == 0 || ret == 2 ? OK : -EIO;
}

static int gt911_write_u8(int fd, uint8_t address, uint16_t reg,
                          uint8_t value)
{
  uint8_t buffer[3] =
  {
    reg >> 8,
    reg & 0xff,
    value
  };

  struct i2c_msg_s message =
  {
    .frequency = GT911_I2C_FREQUENCY,
    .addr = address,
    .flags = 0,
    .buffer = buffer,
    .length = sizeof(buffer),
  };

  struct i2c_transfer_s transfer =
  {
    .msgv = &message,
    .msgc = 1,
  };

  int ret;

  ret = ioctl(fd, I2CIOC_TRANSFER,
              (unsigned long)((uintptr_t)&transfer));
  if (ret < 0)
    {
      return -errno;
    }

  return ret == 0 || ret == 1 ? OK : -EIO;
}

static bool gt911_valid_product_id(const uint8_t *version)
{
  unsigned int i;

  for (i = 0; i < 4; i++)
    {
      if (version[i] != 0 && version[i] != 0xff)
        {
          return true;
        }
    }

  return false;
}

static int gt911_probe(int fd, struct gt911_info_s *info)
{
  static const uint8_t addresses[] =
  {
    0x5d, 0x14
  };

  int last_error = -ENODEV;
  unsigned int i;
  int ret;

  memset(info, 0, sizeof(*info));
  for (i = 0; i < sizeof(addresses) / sizeof(addresses[0]); i++)
    {
      memset(info->version, 0, sizeof(info->version));
      ret = gt911_read_reg(fd, addresses[i], GT911_PRODUCT_ID_REG,
                           info->version, sizeof(info->version));
      if (ret < 0)
        {
          last_error = ret;
          printf("GT911 no response at 0x%02x: %s\n",
                 addresses[i], strerror(-ret));
          continue;
        }

      if (!gt911_valid_product_id(info->version))
        {
          last_error = -ENODEV;
          printf("GT911 invalid product ID at 0x%02x\n", addresses[i]);
          continue;
        }

      info->address = addresses[i];
      ret = gt911_read_reg(fd, info->address, GT911_CONFIG_REG,
                           info->config, sizeof(info->config));
      info->config_valid = ret >= 0;
      return OK;
    }

  return last_error;
}

static void gt911_print_info(const struct gt911_info_s *info)
{
  char product_id[5];
  uint16_t sensor_x;
  uint16_t sensor_y;
  uint16_t config_x;
  uint16_t config_y;
  uint8_t checksum;
  uint8_t sum = 0;
  unsigned int i;
  bool int_high;

  for (i = 0; i < 4; i++)
    {
      product_id[i] = isprint(info->version[i]) ?
                      info->version[i] : '.';
    }

  product_id[4] = '\0';
  sensor_x = gt911_getle16(&info->version[6]);
  sensor_y = gt911_getle16(&info->version[8]);

  printf("GT911 found at 0x%02x, product ID: %s "
         "(%02x %02x %02x %02x)\n",
         info->address, product_id, info->version[0], info->version[1],
         info->version[2], info->version[3]);
  printf("Firmware: 0x%04x, sensor resolution: %ux%u, vendor: 0x%02x\n",
         gt911_getle16(&info->version[4]), sensor_x, sensor_y,
         info->version[10]);

  if (info->config_valid)
    {
      for (i = 0; i < GT911_CONFIG_DATA_SIZE; i++)
        {
          sum += info->config[i];
        }

      checksum = (uint8_t)(~sum + 1);
      config_x = gt911_getle16(&info->config[1]);
      config_y = gt911_getle16(&info->config[3]);
      printf("Config: version=0x%02x, output=%ux%u, max_touches=%u, "
             "module_switch1=0x%02x\n",
             info->config[0], config_x, config_y,
             info->config[5] & GT911_STATUS_POINT_MASK,
             info->config[6]);
      printf("Config checksum: stored=0x%02x, expected=0x%02x, "
             "fresh=0x%02x\n",
             info->config[GT911_CONFIG_DATA_SIZE], checksum,
             info->config[GT911_CONFIG_DATA_SIZE + 1]);
    }
  else
    {
      printf("Config: read failed\n");
    }

  if (d13x_gt911_interrupt_level(&int_high) >= 0)
    {
      printf("Interrupt: PA.11=%s\n", int_high ? "HIGH" : "LOW");
    }
}

static void gt911_decode_point(const uint8_t *buffer,
                               struct gt911_point_s *point)
{
  point->id = buffer[0] & GT911_STATUS_POINT_MASK;
  point->x = gt911_getle16(&buffer[1]);
  point->y = gt911_getle16(&buffer[3]);
  point->size = gt911_getle16(&buffer[5]);
}

static void gt911_visual_fill_rect(struct gt911_visual_s *visual,
                                   uint32_t x, uint32_t y,
                                   uint32_t width, uint32_t height,
                                   uint16_t color)
{
  uint32_t x_end = x + width;
  uint32_t y_end = y + height;
  uint32_t draw_x;
  uint32_t draw_y;

  if (x_end > GT911_FB_WIDTH)
    {
      x_end = GT911_FB_WIDTH;
    }

  if (y_end > GT911_FB_HEIGHT)
    {
      y_end = GT911_FB_HEIGHT;
    }

  for (draw_y = y; draw_y < y_end; draw_y++)
    {
      uint16_t *row = (uint16_t *)(visual->framebuffer +
                                   draw_y * visual->stride);

      for (draw_x = x; draw_x < x_end; draw_x++)
        {
          row[draw_x] = color;
        }
    }
}

static void gt911_visual_target(struct gt911_visual_s *visual,
                                uint32_t center_x, uint32_t center_y,
                                uint16_t color, bool filled)
{
  uint32_t x = center_x - GT911_TARGET_SIZE / 2u;
  uint32_t y = center_y - GT911_TARGET_SIZE / 2u;

  if (filled)
    {
      gt911_visual_fill_rect(visual, x, y, GT911_TARGET_SIZE,
                             GT911_TARGET_SIZE, color);
      return;
    }

  gt911_visual_fill_rect(visual, x, y, GT911_TARGET_SIZE,
                         GT911_TARGET_BORDER, color);
  gt911_visual_fill_rect(visual, x, y + GT911_TARGET_SIZE -
                         GT911_TARGET_BORDER, GT911_TARGET_SIZE,
                         GT911_TARGET_BORDER, color);
  gt911_visual_fill_rect(visual, x, y, GT911_TARGET_BORDER,
                         GT911_TARGET_SIZE, color);
  gt911_visual_fill_rect(visual, x + GT911_TARGET_SIZE -
                         GT911_TARGET_BORDER, y, GT911_TARGET_BORDER,
                         GT911_TARGET_SIZE, color);
}

static int gt911_visual_update(struct gt911_visual_s *visual)
{
  struct fb_area_s area;

  area.x = 0;
  area.y = 0;
  area.w = GT911_FB_WIDTH;
  area.h = GT911_FB_HEIGHT;
  if (ioctl(visual->fd, FBIO_UPDATE,
            (unsigned long)((uintptr_t)&area)) < 0)
    {
      return -errno;
    }

  return OK;
}

static int gt911_visual_open(struct gt911_visual_s *visual)
{
  struct fb_videoinfo_s video;
  struct fb_planeinfo_s plane;
  uint32_t x;
  uint32_t y;
  int ret;

  memset(visual, 0, sizeof(*visual));
  visual->fd = open(GT911_FB_DEVICE, O_RDWR);
  if (visual->fd < 0)
    {
      return -errno;
    }

  memset(&video, 0, sizeof(video));
  ret = ioctl(visual->fd, FBIOGET_VIDEOINFO,
              (unsigned long)((uintptr_t)&video));
  if (ret < 0)
    {
      ret = -errno;
      goto errout;
    }

  memset(&plane, 0, sizeof(plane));
  plane.display = 0;
  ret = ioctl(visual->fd, FBIOGET_PLANEINFO,
              (unsigned long)((uintptr_t)&plane));
  if (ret < 0)
    {
      ret = -errno;
      goto errout;
    }

  if (video.fmt != FB_FMT_RGB16_565 || video.xres != GT911_FB_WIDTH ||
      video.yres != GT911_FB_HEIGHT || plane.bpp != GT911_FB_BPP ||
      plane.fbmem == NULL || plane.stride < GT911_FB_WIDTH * 2u ||
      plane.fblen < plane.stride * GT911_FB_HEIGHT)
    {
      ret = -ENOTSUP;
      goto errout;
    }

  visual->framebuffer = (uint8_t *)plane.fbmem;
  visual->stride = plane.stride;
  visual->active = true;
  gt911_visual_fill_rect(visual, 0, 0, GT911_FB_WIDTH, GT911_FB_HEIGHT,
                         GT911_COLOR_BACKGROUND);

  for (x = GT911_FB_WIDTH / 4u; x < GT911_FB_WIDTH;
       x += GT911_FB_WIDTH / 4u)
    {
      gt911_visual_fill_rect(visual, x, 0, 2, GT911_FB_HEIGHT,
                             GT911_COLOR_GRID);
    }

  for (y = GT911_FB_HEIGHT / 4u; y < GT911_FB_HEIGHT;
       y += GT911_FB_HEIGHT / 4u)
    {
      gt911_visual_fill_rect(visual, 0, y, GT911_FB_WIDTH, 2,
                             GT911_COLOR_GRID);
    }

  gt911_visual_target(visual, GT911_TARGET_SIZE, GT911_TARGET_SIZE,
                      0xf800u, false);
  gt911_visual_target(visual, GT911_FB_WIDTH - GT911_TARGET_SIZE,
                      GT911_TARGET_SIZE, 0xffe0u, false);
  gt911_visual_target(visual, GT911_FB_WIDTH / 2u, GT911_FB_HEIGHT / 2u,
                      0xffffu, false);
  gt911_visual_target(visual, GT911_TARGET_SIZE,
                      GT911_FB_HEIGHT - GT911_TARGET_SIZE, 0x07e0u, false);
  gt911_visual_target(visual, GT911_FB_WIDTH - GT911_TARGET_SIZE,
                      GT911_FB_HEIGHT - GT911_TARGET_SIZE, 0x07ffu, false);

  ret = gt911_visual_update(visual);
  if (ret < 0)
    {
      goto errout;
    }

  return OK;

errout:
  close(visual->fd);
  memset(visual, 0, sizeof(*visual));
  visual->fd = -1;
  return ret;
}

static void gt911_visual_close(struct gt911_visual_s *visual)
{
  if (visual->fd >= 0)
    {
      close(visual->fd);
    }

  memset(visual, 0, sizeof(*visual));
  visual->fd = -1;
}

static void gt911_visual_point(struct gt911_visual_s *visual,
                               const struct gt911_point_s *point)
{
  int32_t radius = 8;
  int32_t draw_x;
  int32_t draw_y;
  int32_t x;
  int32_t y;
  uint16_t color;

  if (!visual->active || point->x >= GT911_FB_WIDTH ||
      point->y >= GT911_FB_HEIGHT)
    {
      return;
    }

  color = g_gt911_id_colors[point->id % GT911_MAX_TOUCHES];
  for (draw_y = -radius; draw_y <= radius; draw_y++)
    {
      for (draw_x = -radius; draw_x <= radius; draw_x++)
        {
          if (draw_x * draw_x + draw_y * draw_y <= radius * radius)
            {
              x = (int32_t)point->x + draw_x;
              y = (int32_t)point->y + draw_y;
              if (x >= 0 && x < GT911_FB_WIDTH &&
                  y >= 0 && y < GT911_FB_HEIGHT)
                {
                  uint16_t *row =
                    (uint16_t *)(visual->framebuffer + y * visual->stride);

                  row[x] = color;
                }
            }
        }
    }
}

static uint8_t gt911_target_for_point(const struct gt911_point_s *point)
{
  bool left = point->x < GT911_FB_WIDTH / 5u;
  bool right = point->x >= GT911_FB_WIDTH * 4u / 5u;
  bool top = point->y < GT911_FB_HEIGHT / 5u;
  bool bottom = point->y >= GT911_FB_HEIGHT * 4u / 5u;
  bool center_x = point->x >= GT911_FB_WIDTH * 2u / 5u &&
                  point->x < GT911_FB_WIDTH * 3u / 5u;
  bool center_y = point->y >= GT911_FB_HEIGHT * 2u / 5u &&
                  point->y < GT911_FB_HEIGHT * 3u / 5u;

  if (left && top)
    {
      return GT911_TARGET_TOP_LEFT;
    }

  if (right && top)
    {
      return GT911_TARGET_TOP_RIGHT;
    }

  if (center_x && center_y)
    {
      return GT911_TARGET_CENTER;
    }

  if (left && bottom)
    {
      return GT911_TARGET_BOTTOM_LEFT;
    }

  if (right && bottom)
    {
      return GT911_TARGET_BOTTOM_RIGHT;
    }

  return 0;
}

static void gt911_visual_mark_target(struct gt911_visual_s *visual,
                                     uint8_t target)
{
  if (!visual->active)
    {
      return;
    }

  if (target == GT911_TARGET_TOP_LEFT)
    {
      gt911_visual_target(visual, GT911_TARGET_SIZE, GT911_TARGET_SIZE,
                          0xf800u, true);
    }
  else if (target == GT911_TARGET_TOP_RIGHT)
    {
      gt911_visual_target(visual, GT911_FB_WIDTH - GT911_TARGET_SIZE,
                          GT911_TARGET_SIZE, 0xffe0u, true);
    }
  else if (target == GT911_TARGET_CENTER)
    {
      gt911_visual_target(visual, GT911_FB_WIDTH / 2u,
                          GT911_FB_HEIGHT / 2u, 0xffffu, true);
    }
  else if (target == GT911_TARGET_BOTTOM_LEFT)
    {
      gt911_visual_target(visual, GT911_TARGET_SIZE,
                          GT911_FB_HEIGHT - GT911_TARGET_SIZE,
                          0x07e0u, true);
    }
  else if (target == GT911_TARGET_BOTTOM_RIGHT)
    {
      gt911_visual_target(visual, GT911_FB_WIDTH - GT911_TARGET_SIZE,
                          GT911_FB_HEIGHT - GT911_TARGET_SIZE,
                          0x07ffu, true);
    }
}

static void gt911_stats_point(struct gt911_stats_s *stats,
                              const struct gt911_point_s *point)
{
  uint8_t target = gt911_target_for_point(point);

  stats->samples++;
  stats->observed_ids |= 1u << point->id;
  if (!stats->have_range)
    {
      stats->min_x = point->x;
      stats->max_x = point->x;
      stats->min_y = point->y;
      stats->max_y = point->y;
      stats->have_range = true;
    }
  else
    {
      if (point->x < stats->min_x)
        {
          stats->min_x = point->x;
        }

      if (point->x > stats->max_x)
        {
          stats->max_x = point->x;
        }

      if (point->y < stats->min_y)
        {
          stats->min_y = point->y;
        }

      if (point->y > stats->max_y)
        {
          stats->max_y = point->y;
        }
    }

  stats->targets |= target;
}

static void gt911_print_summary(const struct gt911_stats_s *stats)
{
  printf("Touch monitor complete: ready_frames=%lu, touch_frames=%lu, "
         "samples=%lu, max_points=%u\n",
         (unsigned long)stats->ready_frames,
         (unsigned long)stats->touch_frames,
         (unsigned long)stats->samples, stats->max_points);
  printf("Events: down=%lu move=%lu up=%lu, ids=0x%04x\n",
         (unsigned long)stats->down_events,
         (unsigned long)stats->move_events,
         (unsigned long)stats->up_events, stats->observed_ids);

  if (stats->have_range)
    {
      printf("Observed range: x=%u..%u y=%u..%u\n", stats->min_x,
             stats->max_x, stats->min_y, stats->max_y);
    }

  printf("Coverage: TL=%s TR=%s C=%s BL=%s BR=%s (%s)\n",
         (stats->targets & GT911_TARGET_TOP_LEFT) != 0 ? "yes" : "no",
         (stats->targets & GT911_TARGET_TOP_RIGHT) != 0 ? "yes" : "no",
         (stats->targets & GT911_TARGET_CENTER) != 0 ? "yes" : "no",
         (stats->targets & GT911_TARGET_BOTTOM_LEFT) != 0 ? "yes" : "no",
         (stats->targets & GT911_TARGET_BOTTOM_RIGHT) != 0 ? "yes" : "no",
         stats->targets == GT911_TARGET_ALL ? "PASS" : "incomplete");
}

static int gt911_monitor(int fd, const struct gt911_info_s *info,
                         unsigned int seconds, bool draw)
{
  uint8_t point_data[GT911_MAX_TOUCHES * GT911_POINT_SIZE];
  struct gt911_point_s current[GT911_MAX_TOUCHES];
  struct gt911_visual_s visual;
  struct gt911_stats_s stats;
  uint64_t next_wait_ms;
  uint64_t now_ms;
  uint64_t end_ms;
  uint16_t current_ids;
  uint16_t released_ids;
  uint16_t id_bit;
  uint8_t target;
  uint8_t status;
  uint8_t points;
  unsigned int id;
  unsigned int i;
  bool int_high;
  int clear_ret;
  int ret;

  memset(&stats, 0, sizeof(stats));
  memset(&visual, 0, sizeof(visual));
  visual.fd = -1;
  if (draw)
    {
      ret = gt911_visual_open(&visual);
      if (ret < 0)
        {
          printf("gt911_test: visual mode failed to open %s: %s\n",
                 GT911_FB_DEVICE, strerror(-ret));
          return ret;
        }
    }

  ret = gt911_write_u8(fd, info->address, GT911_TOUCH_STATUS_REG, 0);
  if (ret < 0)
    {
      printf("gt911_test: clear initial touch status failed: %s\n",
             strerror(-ret));
      goto out;
    }

  printf("Touch monitor: %u seconds, poll=%u ms, display=%s. "
         "Tap targets, drag, and try multiple fingers.\n",
         seconds, GT911_POLL_INTERVAL_US / 1000u,
         draw ? "on" : "off");
  now_ms = gt911_milliseconds();
  end_ms = now_ms + (uint64_t)seconds * 1000u;
  next_wait_ms = now_ms + 1000u;

  while ((now_ms = gt911_milliseconds()) < end_ms)
    {
      ret = gt911_read_reg(fd, info->address, GT911_TOUCH_STATUS_REG,
                           &status, sizeof(status));
      if (ret < 0)
        {
          printf("gt911_test: touch status read failed: %s\n",
                 strerror(-ret));
          goto out;
        }

      if ((status & GT911_STATUS_READY) == 0)
        {
          if (now_ms >= next_wait_ms)
            {
              if (d13x_gt911_interrupt_level(&int_high) >= 0)
                {
                  printf("WAIT status=0x%02x int=%s\n", status,
                         int_high ? "HIGH" : "LOW");
                }
              else
                {
                  printf("WAIT status=0x%02x int=unknown\n", status);
                }

              next_wait_ms = now_ms + 1000u;
            }

          usleep(GT911_POLL_INTERVAL_US);
          continue;
        }

      stats.ready_frames++;
      points = status & GT911_STATUS_POINT_MASK;
      if (points > GT911_MAX_TOUCHES)
        {
          printf("gt911_test: invalid touch count %u (status=0x%02x)\n",
                 points, status);
          ret = -EPROTO;
        }
      else if (points > 0)
        {
          ret = gt911_read_reg(fd, info->address, GT911_POINT_DATA_REG,
                               point_data, points * GT911_POINT_SIZE);
          if (ret < 0)
            {
              printf("gt911_test: point data read failed: %s\n",
                     strerror(-ret));
            }
        }
      else
        {
          ret = OK;
        }

      clear_ret = gt911_write_u8(fd, info->address,
                                 GT911_TOUCH_STATUS_REG, 0);
      if (clear_ret < 0)
        {
          printf("gt911_test: clear touch status failed: %s\n",
                 strerror(-clear_ret));
          ret = clear_ret;
          goto out;
        }

      if (ret < 0)
        {
          goto out;
        }

      current_ids = 0;
      for (i = 0; i < points; i++)
        {
          gt911_decode_point(&point_data[i * GT911_POINT_SIZE], &current[i]);
          id_bit = 1u << current[i].id;
          if ((current_ids & id_bit) != 0)
            {
              printf("gt911_test: duplicate track id %u\n", current[i].id);
              ret = -EPROTO;
              goto out;
            }

          current_ids |= id_bit;
        }

      if (points > 0)
        {
          printf("FRAME points=%u\n", points);
          stats.touch_frames++;
          if (points > stats.max_points)
            {
              stats.max_points = points;
            }
        }

      for (i = 0; i < points; i++)
        {
          id_bit = 1u << current[i].id;
          if ((stats.active_ids & id_bit) == 0)
            {
              printf("  DOWN id=%u x=%u y=%u size=%u\n", current[i].id,
                     current[i].x, current[i].y, current[i].size);
              stats.down_events++;
            }
          else
            {
              printf("  MOVE id=%u x=%u y=%u size=%u\n", current[i].id,
                     current[i].x, current[i].y, current[i].size);
              stats.move_events++;
            }

          stats.last[current[i].id] = current[i];
          target = gt911_target_for_point(&current[i]);
          gt911_stats_point(&stats, &current[i]);
          gt911_visual_point(&visual, &current[i]);
          gt911_visual_mark_target(&visual, target);
        }

      released_ids = stats.active_ids & ~current_ids;
      for (id = 0; id < 16; id++)
        {
          id_bit = 1u << id;
          if ((released_ids & id_bit) != 0)
            {
              printf("  UP id=%u x=%u y=%u\n", id, stats.last[id].x,
                     stats.last[id].y);
              stats.up_events++;
            }
        }

      stats.active_ids = current_ids;
      if (visual.active)
        {
          ret = gt911_visual_update(&visual);
          if (ret < 0)
            {
              printf("gt911_test: framebuffer update failed: %s\n",
                     strerror(-ret));
              goto out;
            }
        }

      usleep(GT911_POLL_INTERVAL_US);
    }

  gt911_print_summary(&stats);
  ret = OK;

out:
  gt911_visual_close(&visual);
  return ret;
}

static unsigned int gt911_duration(const char *argument)
{
  unsigned long value;
  char *endptr;

  if (argument == NULL)
    {
      return GT911_DEFAULT_SECONDS;
    }

  errno = 0;
  value = strtoul(argument, &endptr, 10);
  if (errno != 0 || endptr == argument || *endptr != '\0' ||
      value < GT911_MIN_SECONDS || value > GT911_MAX_SECONDS)
    {
      return 0;
    }

  return (unsigned int)value;
}

static void gt911_usage(void)
{
  printf("Usage:\n");
  printf("  gt911_test [info]\n");
  printf("  gt911_test touch [seconds: %u..%u]\n",
         GT911_MIN_SECONDS, GT911_MAX_SECONDS);
  printf("  gt911_test draw [seconds: %u..%u]\n",
         GT911_MIN_SECONDS, GT911_MAX_SECONDS);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, char *argv[])
{
  struct gt911_info_s info;
  unsigned int seconds = 0;
  bool monitor = false;
  bool draw = false;
  int fd;
  int ret;

  if (argc > 1)
    {
      if (strcmp(argv[1], "touch") == 0 ||
          strcmp(argv[1], "draw") == 0)
        {
          if (argc > 3)
            {
              gt911_usage();
              return 1;
            }

          monitor = true;
          draw = strcmp(argv[1], "draw") == 0;
          seconds = gt911_duration(argc == 3 ? argv[2] : NULL);
          if (seconds == 0)
            {
              gt911_usage();
              return 1;
            }
        }
      else if (strcmp(argv[1], "info") != 0 || argc != 2)
        {
          gt911_usage();
          return 1;
        }
    }

  fd = open(GT911_I2C_DEVICE, O_RDWR);
  if (fd < 0)
    {
      printf("gt911_test: open %s failed: %s\n",
             GT911_I2C_DEVICE, strerror(errno));
      return 1;
    }

  ret = gt911_probe(fd, &info);
  if (ret < 0)
    {
      close(fd);
      printf("gt911_test: probe failed (%s); GT911 reset/address "
             "selection may be required\n",
             strerror(-ret));
      return 1;
    }

  gt911_print_info(&info);
  if (monitor)
    {
      ret = gt911_monitor(fd, &info, seconds, draw);
    }

  close(fd);
  return ret < 0 ? 1 : 0;
}
