# Contest 2026 team 094 button test app
if(CONFIG_LVX_USE_DEMO_CONTEST2026_094_BUTTON_TEST)
  nuttx_add_application(
    NAME button_test
    SRCS button_test_main.c
    STACKSIZE 2048)
endif()
