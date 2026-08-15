# Contest 2026 team 094 TF-card test app
if(CONFIG_LVX_USE_DEMO_CONTEST2026_094_TF_TEST)
  nuttx_add_application(
    NAME tf_test
    SRCS tf_test_main.c
    STACKSIZE 8192)
endif()
