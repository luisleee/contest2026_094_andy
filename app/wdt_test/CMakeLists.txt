# Contest 2026 team 094 watchdog test
if(CONFIG_LVX_USE_DEMO_CONTEST2026_094_WDT_TEST)
  nuttx_add_application(
    NAME wdt_test
    SRCS wdt_test_main.c
    STACKSIZE 2048)
endif()
