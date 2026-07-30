# Contest 2026 team 094 RTC test
if(CONFIG_LVX_USE_DEMO_CONTEST2026_094_RTC_TEST)
  nuttx_add_application(
    NAME rtc_test
    SRCS rtc_test_main.c
    STACKSIZE 2048)
endif()
