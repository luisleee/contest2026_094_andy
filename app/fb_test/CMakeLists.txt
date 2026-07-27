# Contest 2026 team 094 framebuffer test app
if(CONFIG_LVX_USE_DEMO_CONTEST2026_094_FB_TEST)
  nuttx_add_application(
    NAME fb_test
    SRCS fb_test_main.c
    STACKSIZE 2048)
endif()
