# Contest 2026 team 094 LVGL recorder app
if(CONFIG_LVX_USE_DEMO_CONTEST2026_094_RECORDER_APP)
  nuttx_add_application(
    NAME recorder
    SRCS recorder_app_main.c
    STACKSIZE ${CONFIG_LVX_USE_DEMO_CONTEST2026_094_RECORDER_STACKSIZE}
    DEPENDS lvgl)
endif()
