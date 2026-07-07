# Minimal custom chip glue for the contest D13X port.
#
# This is intentionally small: it only makes the custom chip discoverable by
# the CMake build and provides stub sources that can be replaced incrementally.

set(SRCS
    d13x_head.S
    d13x_irq.c
    d13x_start.c
    d13x_boot.c)

target_sources(arch PRIVATE ${SRCS})

target_include_directories(arch PRIVATE
                           ${NUTTX_CHIP_ABS_DIR}
                           ${NUTTX_CHIP_ABS_DIR}/include)
