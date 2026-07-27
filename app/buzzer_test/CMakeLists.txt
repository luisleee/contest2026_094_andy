# Contest 2026 team 094 buzzer test app
if(CONFIG_LVX_USE_DEMO_CONTEST2026_094_BUZZER_TEST)
  nuttx_add_application(
    NAME buzzer_test
    SRCS buzzer_test_main.c
    STACKSIZE 2048)
endif()
