/****************************************************************************
 * contest2026_094_andy/app/lvgl_test/lvgl_test_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <lvgl/lvgl.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LVGL_TEST_DEFAULT_SECONDS   30u
#define LVGL_TEST_MIN_SECONDS       5u
#define LVGL_TEST_MAX_SECONDS       300u
#define LVGL_TEST_UPDATE_MS         50u

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct lvgl_test_ui_s
{
  lv_obj_t *progress;
  lv_obj_t *progress_label;
  lv_obj_t *moving_block;
  lv_obj_t *frame_label;
  lv_obj_t *event_label;
  lv_obj_t *button_label;
  lv_obj_t *slider;
  lv_obj_t *slider_label;
  lv_obj_t *touch_switch;
  lv_obj_t *switch_label;
  lv_obj_t *drag_zone;
  lv_obj_t *drag_block;
  uint32_t button_count;
  uint32_t slider_events;
  uint32_t switch_events;
  uint32_t drag_events;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const uint32_t g_swatch_colors[] =
{
  0xf04444u,
  0xffc145u,
  0x3ac47du,
  0x36b9ccu,
  0x3978e8u,
  0xb45bd6u
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint64_t lvgl_test_milliseconds(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static unsigned int lvgl_test_duration(int argc, char *argv[])
{
  unsigned long value;
  char *endptr;

  if (argc < 2)
    {
      return LVGL_TEST_DEFAULT_SECONDS;
    }

  errno = 0;
  value = strtoul(argv[1], &endptr, 10);
  if (errno != 0 || endptr == argv[1] || *endptr != '\0' ||
      value < LVGL_TEST_MIN_SECONDS || value > LVGL_TEST_MAX_SECONDS)
    {
      return 0;
    }

  return (unsigned int)value;
}

static lv_obj_t *lvgl_test_panel(lv_obj_t *parent, int32_t x, int32_t y,
                                 int32_t width, int32_t height,
                                 uint32_t color)
{
  lv_obj_t *panel;

  panel = lv_obj_create(parent);
  lv_obj_set_pos(panel, x, y);
  lv_obj_set_size(panel, width, height);
  lv_obj_set_style_radius(panel, 4, 0);
  lv_obj_set_style_border_width(panel, 0, 0);
  lv_obj_set_style_bg_color(panel, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(panel, 0, 0);
  lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  return panel;
}

static void lvgl_test_button_event(lv_event_t *event)
{
  struct lvgl_test_ui_s *ui = lv_event_get_user_data(event);

  ui->button_count++;
  lv_label_set_text_fmt(ui->button_label, "Clicked %lu",
                        (unsigned long)ui->button_count);
  lv_label_set_text_fmt(ui->event_label, "Button event #%lu",
                        (unsigned long)ui->button_count);
}

static void lvgl_test_slider_event(lv_event_t *event)
{
  struct lvgl_test_ui_s *ui = lv_event_get_user_data(event);
  int32_t value = lv_slider_get_value(ui->slider);

  ui->slider_events++;
  lv_label_set_text_fmt(ui->slider_label, "Slider %ld",
                        (long)value);
  lv_label_set_text_fmt(ui->event_label, "Slider value %ld",
                        (long)value);
}

static void lvgl_test_switch_event(lv_event_t *event)
{
  struct lvgl_test_ui_s *ui = lv_event_get_user_data(event);
  bool checked = lv_obj_has_state(ui->touch_switch, LV_STATE_CHECKED);

  ui->switch_events++;
  lv_label_set_text(ui->switch_label, checked ? "Switch ON" : "Switch OFF");
  lv_label_set_text(ui->event_label,
                    checked ? "Switch enabled" : "Switch disabled");
}

static void lvgl_test_drag_event(lv_event_t *event)
{
  struct lvgl_test_ui_s *ui = lv_event_get_user_data(event);
  lv_indev_t *indev = lv_indev_active();
  lv_point_t point;
  lv_area_t area;
  int32_t max_x;
  int32_t max_y;
  int32_t x;
  int32_t y;

  if (indev == NULL)
    {
      return;
    }

  lv_indev_get_point(indev, &point);
  lv_obj_get_coords(ui->drag_zone, &area);
  max_x = lv_obj_get_width(ui->drag_zone) -
          lv_obj_get_width(ui->drag_block);
  max_y = lv_obj_get_height(ui->drag_zone) -
          lv_obj_get_height(ui->drag_block);
  x = point.x - area.x1 - lv_obj_get_width(ui->drag_block) / 2;
  y = point.y - area.y1 - lv_obj_get_height(ui->drag_block) / 2;

  if (x < 0)
    {
      x = 0;
    }
  else if (x > max_x)
    {
      x = max_x;
    }

  if (y < 0)
    {
      y = 0;
    }
  else if (y > max_y)
    {
      y = max_y;
    }

  lv_obj_set_pos(ui->drag_block, x, y);
  ui->drag_events++;
  lv_label_set_text_fmt(ui->event_label, "Drag x=%ld y=%ld",
                        (long)point.x, (long)point.y);
}

static void lvgl_test_create_ui(struct lvgl_test_ui_s *ui)
{
  lv_obj_t *screen;
  lv_obj_t *header;
  lv_obj_t *title;
  lv_obj_t *subtitle;
  lv_obj_t *section;
  lv_obj_t *label;
  lv_obj_t *swatch;
  lv_obj_t *button;
  unsigned int i;

  memset(ui, 0, sizeof(*ui));

  screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(screen, lv_color_hex(0xf4f7fa), 0);

  header = lvgl_test_panel(screen, 0, 0, 1024, 96, 0x182630);
  title = lv_label_create(header);
  lv_label_set_text(title, "LVGL Touch Input");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_set_pos(title, 32, 18);

  subtitle = lv_label_create(header);
  lv_label_set_text(subtitle,
                    "demo88-nor  |  /dev/fb0  |  /dev/input0");
  lv_obj_set_style_text_color(subtitle, lv_color_hex(0xa9bbc7), 0);
  lv_obj_set_pos(subtitle, 34, 58);

  ui->event_label = lv_label_create(header);
  lv_label_set_text(ui->event_label, "Touch ready");
  lv_obj_set_style_text_color(ui->event_label, lv_color_hex(0x61d6a3), 0);
  lv_obj_align(ui->event_label, LV_ALIGN_RIGHT_MID, -32, 0);

  section = lvgl_test_panel(screen, 32, 116, 960, 92, 0x1c2d37);
  label = lv_label_create(section);
  lv_label_set_text(label, "Display");
  lv_obj_set_pos(label, 18, 12);

  for (i = 0; i < sizeof(g_swatch_colors) / sizeof(g_swatch_colors[0]); i++)
    {
      swatch = lvgl_test_panel(section, 112 + (int32_t)i * 136, 20,
                               112, 52, g_swatch_colors[i]);
      lv_obj_set_style_border_width(swatch, 2, 0);
      lv_obj_set_style_border_color(swatch, lv_color_hex(0xe4edf2), 0);
      lv_obj_set_style_border_opa(swatch, LV_OPA_COVER, 0);
    }

  section = lvgl_test_panel(screen, 32, 228, 960, 254, 0x1c2d37);
  label = lv_label_create(section);
  lv_label_set_text(label, "Controls");
  lv_obj_set_pos(label, 18, 14);

  button = lv_button_create(section);
  lv_obj_set_pos(button, 22, 50);
  lv_obj_set_size(button, 174, 64);
  lv_obj_set_style_radius(button, 4, 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x3978e8), 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x2e5fb8),
                            LV_STATE_PRESSED);
  ui->button_label = lv_label_create(button);
  lv_label_set_text(ui->button_label, "Button");
  lv_obj_center(ui->button_label);
  lv_obj_add_event_cb(button, lvgl_test_button_event, LV_EVENT_CLICKED, ui);

  ui->slider = lv_slider_create(section);
  lv_obj_set_pos(ui->slider, 232, 66);
  lv_obj_set_size(ui->slider, 330, 28);
  lv_slider_set_range(ui->slider, 0, 100);
  lv_slider_set_value(ui->slider, 50, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(ui->slider, lv_color_hex(0x334750),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui->slider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui->slider, lv_color_hex(0x36b9cc),
                            LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(ui->slider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(ui->slider, lv_color_hex(0xf4f7fa),
                            LV_PART_KNOB);
  lv_obj_set_style_bg_opa(ui->slider, LV_OPA_COVER, LV_PART_KNOB);
  ui->slider_label = lv_label_create(section);
  lv_label_set_text(ui->slider_label, "Slider 50");
  lv_obj_set_pos(ui->slider_label, 232, 108);
  lv_obj_add_event_cb(ui->slider, lvgl_test_slider_event,
                      LV_EVENT_VALUE_CHANGED, ui);

  ui->touch_switch = lv_switch_create(section);
  lv_obj_set_pos(ui->touch_switch, 622, 54);
  lv_obj_set_size(ui->touch_switch, 84, 48);
  lv_obj_set_style_bg_color(ui->touch_switch, lv_color_hex(0x334750),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui->touch_switch, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui->touch_switch, lv_color_hex(0x3ac47d),
                            LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_set_style_bg_opa(ui->touch_switch, LV_OPA_COVER,
                          LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(ui->touch_switch, lv_color_hex(0xf4f7fa),
                            LV_PART_KNOB);
  lv_obj_set_style_bg_opa(ui->touch_switch, LV_OPA_COVER, LV_PART_KNOB);
  ui->switch_label = lv_label_create(section);
  lv_label_set_text(ui->switch_label, "Switch OFF");
  lv_obj_set_pos(ui->switch_label, 730, 70);
  lv_obj_add_event_cb(ui->touch_switch, lvgl_test_switch_event,
                      LV_EVENT_VALUE_CHANGED, ui);

  ui->drag_zone = lvgl_test_panel(section, 22, 148, 916, 82, 0x142129);
  lv_obj_set_style_border_width(ui->drag_zone, 2, 0);
  lv_obj_set_style_border_color(ui->drag_zone, lv_color_hex(0x536b78), 0);
  lv_obj_set_style_border_opa(ui->drag_zone, LV_OPA_COVER, 0);
  ui->drag_block = lvgl_test_panel(ui->drag_zone, 10, 10, 58, 58, 0xffc145);
  lv_obj_add_flag(ui->drag_block, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ui->drag_block, lvgl_test_drag_event,
                      LV_EVENT_PRESSING, ui);
  label = lv_label_create(ui->drag_block);
  lv_label_set_text(label, "Drag");
  lv_obj_set_style_text_color(label, lv_color_hex(0x101820), 0);
  lv_obj_center(label);

  section = lvgl_test_panel(screen, 32, 502, 960, 70, 0x182630);
  ui->progress = lv_bar_create(section);
  lv_obj_set_pos(ui->progress, 18, 20);
  lv_obj_set_size(ui->progress, 620, 30);
  lv_bar_set_range(ui->progress, 0, 100);
  lv_obj_set_style_bg_color(ui->progress, lv_color_hex(0x334750),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui->progress, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui->progress, lv_color_hex(0x36b9cc),
                            LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(ui->progress, LV_OPA_COVER,
                          LV_PART_INDICATOR);

  ui->progress_label = lv_label_create(section);
  lv_obj_set_pos(ui->progress_label, 658, 27);
  lv_label_set_text(ui->progress_label, "0%");

  ui->moving_block = lvgl_test_panel(section, 18, 54, 48, 6, 0xffc145);

  ui->frame_label = lv_label_create(section);
  lv_obj_align(ui->frame_label, LV_ALIGN_RIGHT_MID, -18, 0);
  lv_label_set_text(ui->frame_label, "frame 0");
}

static void lvgl_test_update(struct lvgl_test_ui_s *ui,
                             uint64_t elapsed_ms, uint32_t frame)
{
  uint32_t cycle;
  uint32_t value;
  int32_t x;

  cycle = (uint32_t)((elapsed_ms / LVGL_TEST_UPDATE_MS) % 200u);
  value = cycle <= 100u ? cycle : 200u - cycle;
  lv_bar_set_value(ui->progress, value, LV_ANIM_OFF);
  lv_label_set_text_fmt(ui->progress_label, "%lu%%",
                        (unsigned long)value);

  x = 18 + (int32_t)((elapsed_ms / 4u) % 570u);
  lv_obj_set_x(ui->moving_block, x);
  lv_label_set_text_fmt(ui->frame_label, "frame %lu",
                        (unsigned long)frame);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, char *argv[])
{
  struct lvgl_test_ui_s ui;
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;
  unsigned int duration;
  uint64_t start_ms;
  uint64_t now_ms;
  uint64_t elapsed_ms;
  uint64_t last_update_ms;
  uint32_t frame = 0;
  uint32_t delay;

  duration = lvgl_test_duration(argc, argv);
  if (duration == 0)
    {
      printf("Usage: lvgl_test [seconds: %u..%u]\n",
             LVGL_TEST_MIN_SECONDS, LVGL_TEST_MAX_SECONDS);
      return 1;
    }

  if (lv_is_initialized())
    {
      printf("lvgl_test: LVGL is already initialized\n");
      return 1;
    }

  lv_init();
  lv_nuttx_dsc_init(&info);
  info.input_path = "/dev/input0";
  lv_nuttx_init(&info, &result);
  if (result.disp == NULL)
    {
      printf("lvgl_test: failed to initialize /dev/fb0\n");
      lv_deinit();
      return 1;
    }

  if (result.indev == NULL)
    {
      printf("lvgl_test: failed to initialize /dev/input0\n");
      lv_nuttx_deinit(&result);
      lv_deinit();
      return 1;
    }

  lv_indev_set_display(result.indev, result.disp);

  if (lv_display_get_horizontal_resolution(result.disp) != 1024 ||
      lv_display_get_vertical_resolution(result.disp) != 600 ||
      lv_display_get_color_format(result.disp) != LV_COLOR_FORMAT_RGB565)
    {
      printf("lvgl_test: unexpected display configuration\n");
      lv_nuttx_deinit(&result);
      lv_deinit();
      return 1;
    }

  printf("lvgl_test: LVGL %u.%u.%u, 1024x600 RGB565, /dev/input0, "
         "duration=%u seconds\n", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
         LVGL_VERSION_PATCH, duration);

  lvgl_test_create_ui(&ui);
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(result.disp);

  start_ms = lvgl_test_milliseconds();
  last_update_ms = start_ms;
  do
    {
      now_ms = lvgl_test_milliseconds();
      elapsed_ms = now_ms - start_ms;
      if (now_ms - last_update_ms >= LVGL_TEST_UPDATE_MS)
        {
          last_update_ms = now_ms;
          lvgl_test_update(&ui, elapsed_ms, ++frame);
        }

      delay = lv_timer_handler();
      if (delay == 0 || delay > 20)
        {
          delay = 20;
        }

      usleep(delay * 1000u);
    }
  while (elapsed_ms < (uint64_t)duration * 1000u);

  lv_label_set_text(ui.frame_label, "PASS - animation complete");
  lv_label_set_text_fmt(ui.event_label,
                        "Events B=%lu S=%lu W=%lu D=%lu",
                        (unsigned long)ui.button_count,
                        (unsigned long)ui.slider_events,
                        (unsigned long)ui.switch_events,
                        (unsigned long)ui.drag_events);
  lv_obj_set_style_bg_color(ui.moving_block, lv_color_hex(0x3ac47d), 0);
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(result.disp);

  lv_nuttx_deinit(&result);
  lv_deinit();
  printf("lvgl_test: touch events button=%lu slider=%lu switch=%lu "
         "drag=%lu\n", (unsigned long)ui.button_count,
         (unsigned long)ui.slider_events,
         (unsigned long)ui.switch_events,
         (unsigned long)ui.drag_events);
  printf("lvgl_test: completed; final frame remains on the panel\n");
  return 0;
}
