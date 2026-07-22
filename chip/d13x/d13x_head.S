/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_head.S
 *
 * Minimal D13X reset entry. This follows the NuttX RISC-V bring-up pattern:
 * set gp/sp, install the common trap vector, then branch into C startup.
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/irq.h>

#include "riscv_internal.h"

  .extern __trap_vec

  .section .text.entry
  .align 2
  .global __start

__start:
  csrci   CSR_MSTATUS, 8
  csrw    CSR_MIE, zero
  csrw    CSR_MIP, zero

.option push
.option norelax
  la      gp, __global_pointer$
.option pop

  csrr    a0, CSR_MHARTID
  la      sp, _estack

  csrr    t0, 0x7c0
  li      t1, ((1 << 15) | (1 << 22))
  or      t0, t0, t1
  csrw    0x7c0, t0

  csrr    t0, 0x7e1
  li      t1, ~((1 << 16) | (1 << 17))
  and     t0, t0, t1
  csrw    0x7e1, t0

  la      t0, __trap_vec
  ori     t0, t0, 2
  csrw    CSR_MTVEC, t0

  la      t0, __trap_vec
  csrw    0x307, t0

  j       d13x_start

  .global _init
  .global _fini

_init:
_fini:
  ret
