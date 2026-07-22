# Contest 2026 team 094 demo88-nor board

add_subdirectory(src)

if(EXISTS ${NUTTX_BOARD_ABS_DIR}/scripts/ld.script)
  set_property(GLOBAL PROPERTY LD_SCRIPT
               ${NUTTX_BOARD_ABS_DIR}/scripts/ld.script)
endif()
