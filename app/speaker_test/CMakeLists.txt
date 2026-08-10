# Contest 2026 team 094 speaker test app
if(CONFIG_LVX_USE_DEMO_CONTEST2026_094_SPEAKER_TEST)
  nuttx_add_application(
    NAME speaker_test
    SRCS speaker_test_main.c
    STACKSIZE 4096)
endif()
