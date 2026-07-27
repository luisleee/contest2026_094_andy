# Contest 2026 team 094 LVGL framebuffer test app
if(CONFIG_LVX_USE_DEMO_CONTEST2026_094_LVGL_TEST)
  nuttx_add_application(
    NAME lvgl_test
    SRCS lvgl_test_main.c
    STACKSIZE 16384
    DEPENDS lvgl)
endif()
