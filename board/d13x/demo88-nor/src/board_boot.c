/****************************************************************************
 * Contest 2026 demo88-nor board - boot hooks
 ****************************************************************************/

#include <stdint.h>

#include <nuttx/board.h>
#include <arch/chip/chip.h>

#define D13X_BOARD_UART_DELAY 50000U

extern void d13x_board_initialize(void);
extern void d13x_chip_late_boot(void);

static void board_boot_putc(int ch)
{
  volatile uint32_t delay;

  *(volatile uint32_t *)(UART0_BASE + 0x00) = (uint32_t)ch;

  for (delay = 0; delay < D13X_BOARD_UART_DELAY; delay++)
    {
      asm volatile ("nop");
    }
}

static void board_boot_puts(const char *str)
{
  while (*str != '\0')
    {
      board_boot_putc(*str++);
    }
}

#ifdef CONFIG_BOARD_EARLY_INITIALIZE
void board_early_initialize(void)
{
  board_boot_puts("[D13X] board early\n");
  /*
   * TinySPL has already configured the clocks and UART0 pins well enough for
   * NuttX bring-up.  Reprogramming sysclk/pinmux here can silence UART0 or
   * hang before NSH starts, so keep board-early inert until the OS boots.
   */
  board_boot_puts("[D13X] board early ok\n");
}
#endif

#ifdef CONFIG_BOARD_LATE_INITIALIZE
void board_late_initialize(void)
{
  board_boot_puts("[D13X] board late\n");
  d13x_chip_late_boot();
  board_boot_puts("[D13X] board late ok\n");
}
#endif

int board_app_initialize(uintptr_t arg)
{
  (void)arg;
  return 0;
}

void openvela_board_initialize(void)
{
  /* Reserved for OpenVela-specific board glue if the contest needs it. */
}
