/****************************************************************************
 * contest2026_094_andy/app/fb_test/fb_test_main.c
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <nuttx/video/fb.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define FB_TEST_DEVICE             "/dev/fb0"
#define FB_TEST_WIDTH              1024u
#define FB_TEST_HEIGHT             600u
#define FB_TEST_BPP                16u

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const uint16_t g_fb_test_colors[] =
{
  0xf800u, /* Red */
  0xffe0u, /* Yellow */
  0x07e0u, /* Green */
  0x07ffu, /* Cyan */
  0x001fu, /* Blue */
  0xf81fu, /* Magenta */
  0xffffu, /* White */
  0x0000u  /* Black */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void fb_test_draw_bars(uint8_t *framebuffer, uint32_t stride)
{
  unsigned int color_count = sizeof(g_fb_test_colors) /
                             sizeof(g_fb_test_colors[0]);
  uint32_t x;
  uint32_t y;

  for (y = 0; y < FB_TEST_HEIGHT; y++)
    {
      uint16_t *row = (uint16_t *)(framebuffer + y * stride);

      for (x = 0; x < FB_TEST_WIDTH; x++)
        {
          unsigned int index = (x * color_count) / FB_TEST_WIDTH;

          row[x] = g_fb_test_colors[index];
        }
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, char *argv[])
{
  struct fb_videoinfo_s video;
  struct fb_planeinfo_s plane;
  struct fb_area_s area;
  int fd;
  int ret;

  (void)argc;
  (void)argv;

  fd = open(FB_TEST_DEVICE, O_RDWR);
  if (fd < 0)
    {
      printf("fb_test: open %s failed: %s\n",
             FB_TEST_DEVICE, strerror(errno));
      return 1;
    }

  memset(&video, 0, sizeof(video));
  ret = ioctl(fd, FBIOGET_VIDEOINFO,
              (unsigned long)((uintptr_t)&video));
  if (ret < 0)
    {
      printf("fb_test: FBIOGET_VIDEOINFO failed: %s\n", strerror(errno));
      goto errout;
    }

  memset(&plane, 0, sizeof(plane));
  plane.display = 0;
  ret = ioctl(fd, FBIOGET_PLANEINFO,
              (unsigned long)((uintptr_t)&plane));
  if (ret < 0)
    {
      printf("fb_test: FBIOGET_PLANEINFO failed: %s\n", strerror(errno));
      goto errout;
    }

  if (video.fmt != FB_FMT_RGB16_565 || video.xres != FB_TEST_WIDTH ||
      video.yres != FB_TEST_HEIGHT || plane.bpp != FB_TEST_BPP ||
      plane.fbmem == NULL || plane.stride < FB_TEST_WIDTH * 2u ||
      plane.fblen < plane.stride * FB_TEST_HEIGHT)
    {
      printf("fb_test: unexpected framebuffer: fmt=%u %ux%u bpp=%u "
             "stride=%u len=%lu addr=%p\n",
             video.fmt, video.xres, video.yres, plane.bpp, plane.stride,
             (unsigned long)plane.fblen, plane.fbmem);
      goto errout;
    }

  fb_test_draw_bars((uint8_t *)plane.fbmem, plane.stride);

  area.x = 0;
  area.y = 0;
  area.w = FB_TEST_WIDTH;
  area.h = FB_TEST_HEIGHT;
  ret = ioctl(fd, FBIO_UPDATE, (unsigned long)((uintptr_t)&area));
  if (ret < 0)
    {
      printf("fb_test: FBIO_UPDATE failed: %s\n", strerror(errno));
      goto errout;
    }

  printf("Framebuffer color bars: RGB565 %ux%u, addr=%p, stride=%u\n",
         video.xres, video.yres, plane.fbmem, plane.stride);
  close(fd);
  return 0;

errout:
  close(fd);
  return 1;
}
