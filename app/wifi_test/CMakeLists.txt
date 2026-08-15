# Contest 2026 team 094 SDIO Wi-Fi probe app
if(CONFIG_LVX_USE_DEMO_CONTEST2026_094_WIFI_TEST)
  nuttx_add_application(
    NAME wifi_test
    SRCS wifi_test_main.c
    STACKSIZE 3072)
endif()
