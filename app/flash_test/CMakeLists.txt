# Contest 2026 team 094 SPI NOR read-only test
if(CONFIG_LVX_USE_DEMO_CONTEST2026_094_FLASH_TEST)
  nuttx_add_application(
    NAME flash_test
    SRCS flash_test_main.c
    STACKSIZE 3072)
endif()
