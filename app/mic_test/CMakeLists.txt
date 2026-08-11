# Contest 2026 team 094 microphone test app
if(CONFIG_LVX_USE_DEMO_CONTEST2026_094_MIC_TEST)
  nuttx_add_application(
    NAME mic_test
    SRCS mic_test_main.c
    STACKSIZE 4096)
endif()
