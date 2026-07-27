# Contest 2026 demo88-nor board - src
target_sources(board PRIVATE board_boot.c board_bringup.c pinmux.c sys_clk.c)
target_include_directories(board PRIVATE ${NUTTX_BOARD_ABS_DIR}/include)

if(CONFIG_D13X_TOUCH_GT911)
  target_sources(board PRIVATE gt911_board.c)
endif()

if(CONFIG_D13X_GT911_INPUT)
  target_sources(board PRIVATE gt911_input.c)
endif()
