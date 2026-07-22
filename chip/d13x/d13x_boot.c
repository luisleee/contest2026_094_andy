/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_boot.c
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>

#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/timers/arch_alarm.h>

#include <arch/irq.h>

#include "aic_soc.h"
#include "riscv_mtimer.h"

#define D13X_MTIMECMP          (E907_CORET_BASE + 0x0000UL)
#define D13X_MTIME             (E907_CORET_BASE + 0x7ff8UL)

#define D13X_GTC_CLOCK_REG     (CMU_BASE + 0x090cUL)
#define D13X_GTC_FREQUENCY     4000000UL

void up_allocate_heap(void **heap_start, size_t *heap_size)
{
  extern uintptr_t _sheap;
  extern uintptr_t _eheap;
  *heap_start = (void *)&_sheap;
  *heap_size = (size_t)((uintptr_t)&_eheap - (uintptr_t)&_sheap);
}

void up_timer_initialize(void)
{
  struct oneshot_lowerhalf_s *lower;

  putreg32(0x3100, D13X_GTC_CLOCK_REG);
  putreg32(1, GTC_BASE);

  lower = riscv_mtimer_initialize(D13X_MTIME, D13X_MTIMECMP,
                                  RISCV_IRQ_MTIMER,
                                  D13X_GTC_FREQUENCY);

  DEBUGASSERT(lower != NULL);
  up_alarm_set_lowerhalf(lower);
}
