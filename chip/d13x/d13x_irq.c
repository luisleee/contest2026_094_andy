/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_irq.c
 ****************************************************************************/

#include <nuttx/config.h>

#include <nuttx/irq.h>

#include <arch/risc-v/src/common/riscv_internal.h>

#include "chip.h"

void up_irqinitialize(void)
{
  /* Minimal stub: real D13X CLIC/exception setup will be added later. */
}

void up_enable_irq(int irq)
{
  UNUSED(irq);
}

void up_disable_irq(int irq)
{
  UNUSED(irq);
}
