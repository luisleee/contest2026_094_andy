#
# Licensed under the Apache License, Version 2.0
#

include $(TOPDIR)/Make.defs

ifneq ($(CONFIG_ARTINCHIP_BINARY_ONLY),y)

AIC_CHIP_DIR = $(TOPDIR)/../vendor/artinchip/chips/artinchips
AIC_HAL_DIR  = $(AIC_CHIP_DIR)/hal
AIC_HAL_INCDIR = $(AIC_HAL_DIR)/include/hal

CFLAGS += -I$(AIC_CHIP_DIR)/include
CFLAGS += -I$(AIC_CHIP_DIR)/include/hal
CFLAGS += -I$(AIC_CHIP_DIR)/include/uapi
CFLAGS += -I$(AIC_CHIP_DIR)/common/include
CFLAGS += -I$(AIC_HAL_INCDIR)
CFLAGS += -I$(AIC_HAL_DIR)
CFLAGS += -I$(AIC_HAL_DIR)/gpio
CFLAGS += -I$(AIC_HAL_DIR)/timer
CFLAGS += -I$(TOPDIR)/../vendor/artinchip/boards/d13x/demo88-nor/include

CSRCS = board_boot.c pinmux.c sys_clk.c
endif

ifeq ($(CONFIG_ARTINCHIP_BINARY_ONLY),y)
all: libboard$(LIBEXT)
context:
depend:
clean:
	$(call DELFILE, libboard$(LIBEXT))
distclean: clean
	$(call DELFILE, Make.dep)
	$(call DELFILE, .depend)

libboard$(LIBEXT):
	@echo "No binary-only libboard is provided for d13x/demo88-nor"
	false
else
include $(TOPDIR)/boards/Board.mk
endif
