# Contest 2026 team 094 power-management wake test
if(CONFIG_LVX_USE_DEMO_CONTEST2026_094_PM_TEST)
  nuttx_add_application(
    NAME pm_test
    SRCS pm_test_main.c
    STACKSIZE 2048)
endif()
