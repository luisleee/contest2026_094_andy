/****************************************************************************
 * contest2026_094_andy/chip/d13x/include/d13x_dma_compat.h
 ****************************************************************************/

#ifndef __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_D13X_DMA_COMPAT_H
#define __CONTEST2026_094_ANDY_CHIP_D13X_INCLUDE_D13X_DMA_COMPAT_H

#ifndef __ASSEMBLY__

#  include <stdint.h>

#  include <nuttx/mutex.h>
#  include <nuttx/spinlock.h>

#ifndef CACHE_LINE_SIZE
#  define CACHE_LINE_SIZE 32
#endif

#define CACHE_ADDR_MASK (CACHE_LINE_SIZE - 1)
#define CACHE_ALIGN_UP(p) \
  (void *)((((uintptr_t)(p)) + CACHE_ADDR_MASK) & \
           ~(uintptr_t)CACHE_ADDR_MASK)
#define CACHE_ALIGN_DOWN(p) \
  (void *)(((uintptr_t)(p)) & ~(uintptr_t)CACHE_ADDR_MASK)

#endif
#endif
