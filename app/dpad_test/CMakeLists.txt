# Contest 2026 team 094 direction-key test app
if(CONFIG_LVX_USE_DEMO_CONTEST2026_094_DPAD_TEST)
  nuttx_add_application(
    NAME dpad_test
    SRCS dpad_test_main.c
    STACKSIZE 2048)
endif()
