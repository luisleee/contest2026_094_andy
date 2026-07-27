/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_de.h
 ****************************************************************************/

#ifndef __CONTEST2026_094_ANDY_CHIP_D13X_D13X_DE_H
#define __CONTEST2026_094_ANDY_CHIP_D13X_D13X_DE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int d13x_de_initialize(uint32_t width, uint32_t height,
                       uint32_t hfp, uint32_t hbp, uint32_t hsync,
                       uint32_t vfp, uint32_t vbp, uint32_t vsync,
                       uint32_t stride);
void d13x_de_set_framebuffer(uintptr_t address);
void d13x_de_enable(void);
void d13x_de_disable(void);

#endif /* __CONTEST2026_094_ANDY_CHIP_D13X_D13X_DE_H */
