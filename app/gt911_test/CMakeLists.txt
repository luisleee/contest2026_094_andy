# Contest 2026 team 094 GT911 test app
if(CONFIG_LVX_USE_DEMO_CONTEST2026_094_GT911_TEST)
  nuttx_add_application(
    NAME gt911_test
    SRCS gt911_test_main.c
    STACKSIZE 4096)
endif()
