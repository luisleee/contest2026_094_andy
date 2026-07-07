/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_head.S
 *
 * Minimal reset entry for the custom D13X chip stub. This only provides the
 * symbols required by the linker and transfers control to nx_start().
 ****************************************************************************/

#include <nuttx/config.h>

  .section .text
  .align 2
  .global __start

__start:
  la      sp, _estack

  la      a0, _sbss
  la      a1, _ebss
1:
  bgeu    a0, a1, 2f
  sw      zero, 0(a0)
  addi    a0, a0, 4
  j       1b

2:
  call    nx_start

3:
  j       3b

  .global _init
  .global _fini

_init:
_fini:
  ret
