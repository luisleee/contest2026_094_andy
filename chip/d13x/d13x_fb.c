/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_fb.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 1024x600 RGB565 framebuffer for the demo88-nor J18 LVDS panel.
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
#include <nuttx/video/fb.h>

#include "chip.h"
#include "d13x_de.h"
#include "d13x_lvds.h"
#include "d13x_panel.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define D13X_FB_WIDTH               1024u
#define D13X_FB_HEIGHT              600u
#define D13X_FB_BPP                 16u
#define D13X_FB_STRIDE              (D13X_FB_WIDTH * 2u)
#define D13X_FB_SIZE                (D13X_FB_STRIDE * D13X_FB_HEIGHT)
#define D13X_FB_ADDRESS             PSRAM_BASE

#define D13X_PANEL_HBP              160u
#define D13X_PANEL_HFP              160u
#define D13X_PANEL_HSYNC            20u
#define D13X_PANEL_VBP              12u
#define D13X_PANEL_VFP              20u
#define D13X_PANEL_VSYNC            2u
#define D13X_PANEL_PIXEL_CLOCK      52000000u

#define THEAD_MHCR_DCACHE_ENABLE    (1u << 1)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct d13x_fb_state_s
{
  bool initialized;
  bool power_on;
  uint8_t *memory;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int d13x_fb_getvideoinfo(struct fb_vtable_s *vtable,
                                struct fb_videoinfo_s *vinfo);
static int d13x_fb_getplaneinfo(struct fb_vtable_s *vtable, int planeno,
                                struct fb_planeinfo_s *pinfo);
#ifdef CONFIG_FB_UPDATE
static int d13x_fb_updatearea(struct fb_vtable_s *vtable,
                              const struct fb_area_s *area);
#endif
static int d13x_fb_getpower(struct fb_vtable_s *vtable);
static int d13x_fb_setpower(struct fb_vtable_s *vtable, int power);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct d13x_fb_state_s g_d13x_fb;

static struct fb_vtable_s g_d13x_fb_vtable =
{
  .getvideoinfo = d13x_fb_getvideoinfo,
  .getplaneinfo = d13x_fb_getplaneinfo,
#ifdef CONFIG_FB_UPDATE
  .updatearea = d13x_fb_updatearea,
#endif
  .getpower = d13x_fb_getpower,
  .setpower = d13x_fb_setpower
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void d13x_fb_clean_cache(void)
{
  uint32_t mhcr;

  /* The E907 may inherit an enabled write-back cache from tinySPL. The
   * current custom arch has no D-cache hooks, so use T-Head's dcache.call.
   */

  __asm__ __volatile__ ("csrr %0, 0x7c1" : "=r"(mhcr));
  if ((mhcr & THEAD_MHCR_DCACHE_ENABLE) != 0)
    {
      __asm__ __volatile__ ("fence rw, rw" ::: "memory");
      __asm__ __volatile__ (".long 0x0010000b" ::: "memory");
      __asm__ __volatile__ ("fence rw, rw" ::: "memory");
    }
}

static int d13x_fb_getvideoinfo(struct fb_vtable_s *vtable,
                                struct fb_videoinfo_s *vinfo)
{
  (void)vtable;

  if (vinfo == NULL)
    {
      return -EINVAL;
    }

  vinfo->fmt = FB_FMT_RGB16_565;
  vinfo->xres = D13X_FB_WIDTH;
  vinfo->yres = D13X_FB_HEIGHT;
  vinfo->nplanes = 1;
  return OK;
}

static int d13x_fb_getplaneinfo(struct fb_vtable_s *vtable, int planeno,
                                struct fb_planeinfo_s *pinfo)
{
  (void)vtable;

  if (planeno != 0 || pinfo == NULL || !g_d13x_fb.initialized)
    {
      return -EINVAL;
    }

  pinfo->fbmem = g_d13x_fb.memory;
  pinfo->fblen = D13X_FB_SIZE;
  pinfo->stride = D13X_FB_STRIDE;
  pinfo->display = 0;
  pinfo->bpp = D13X_FB_BPP;
  pinfo->xres_virtual = D13X_FB_WIDTH;
  pinfo->yres_virtual = D13X_FB_HEIGHT;
  pinfo->xoffset = 0;
  pinfo->yoffset = 0;
  return OK;
}

#ifdef CONFIG_FB_UPDATE
static int d13x_fb_updatearea(struct fb_vtable_s *vtable,
                              const struct fb_area_s *area)
{
  (void)vtable;

  if (area == NULL || area->w == 0 || area->h == 0 ||
      (uint32_t)area->x + area->w > D13X_FB_WIDTH ||
      (uint32_t)area->y + area->h > D13X_FB_HEIGHT)
    {
      return -EINVAL;
    }

  d13x_fb_clean_cache();
  return OK;
}
#endif

static int d13x_fb_getpower(struct fb_vtable_s *vtable)
{
  (void)vtable;
  return g_d13x_fb.power_on ? 1 : 0;
}

static int d13x_fb_setpower(struct fb_vtable_s *vtable, int power)
{
  (void)vtable;

  if (!g_d13x_fb.initialized)
    {
      return -ENODEV;
    }

  if (power > 0 && !g_d13x_fb.power_on)
    {
      d13x_lvds_enable();
      d13x_de_enable();
      up_mdelay(20);
      d13x_panel_enable();
      g_d13x_fb.power_on = true;
    }
  else if (power <= 0 && g_d13x_fb.power_on)
    {
      d13x_panel_disable();
      d13x_de_disable();
      d13x_lvds_disable();
      g_d13x_fb.power_on = false;
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int up_fbinitialize(int display)
{
  int ret;

  if (display != 0)
    {
      return -ENODEV;
    }

  if (g_d13x_fb.initialized)
    {
      return OK;
    }

  g_d13x_fb.memory = (uint8_t *)(uintptr_t)D13X_FB_ADDRESS;
  memset(g_d13x_fb.memory, 0, D13X_FB_SIZE);
  d13x_fb_clean_cache();

  ret = d13x_panel_initialize();
  if (ret < 0)
    {
      goto errout;
    }

  ret = d13x_lvds_initialize(D13X_PANEL_PIXEL_CLOCK);
  if (ret < 0)
    {
      goto errout;
    }

  ret = d13x_de_initialize(D13X_FB_WIDTH, D13X_FB_HEIGHT,
                           D13X_PANEL_HFP, D13X_PANEL_HBP,
                           D13X_PANEL_HSYNC, D13X_PANEL_VFP,
                           D13X_PANEL_VBP, D13X_PANEL_VSYNC,
                           D13X_FB_STRIDE);
  if (ret < 0)
    {
      goto errout;
    }

  d13x_de_set_framebuffer((uintptr_t)g_d13x_fb.memory);
  d13x_lvds_enable();
  d13x_de_enable();
  up_mdelay(20);
  d13x_panel_enable();
  g_d13x_fb.power_on = true;
  g_d13x_fb.initialized = true;
  return OK;

errout:
  d13x_panel_disable();
  d13x_de_disable();
  d13x_lvds_disable();
  memset(&g_d13x_fb, 0, sizeof(g_d13x_fb));
  return ret;
}

struct fb_vtable_s *up_fbgetvplane(int display, int vplane)
{
  if (display != 0 || vplane != 0 || !g_d13x_fb.initialized)
    {
      return NULL;
    }

  return &g_d13x_fb_vtable;
}

void up_fbuninitialize(int display)
{
  if (display != 0 || !g_d13x_fb.initialized)
    {
      return;
    }

  d13x_panel_disable();
  d13x_de_disable();
  d13x_lvds_disable();
  memset(&g_d13x_fb, 0, sizeof(g_d13x_fb));
}
