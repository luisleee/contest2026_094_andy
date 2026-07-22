# Contest 2026 demo88-nor board - src
target_sources(board PRIVATE board_boot.c pinmux.c sys_clk.c)
target_include_directories(board PRIVATE ${NUTTX_BOARD_ABS_DIR}/include)
