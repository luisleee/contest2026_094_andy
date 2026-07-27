/****************************************************************************
 * Contest 2026 demo88-nor board - boot hooks
 ****************************************************************************/

#include <stdint.h>
#include <syslog.h>

#include <nuttx/board.h>
#include <arch/chip/chip.h>

#define D13X_BOARD_UART_DELAY 50000U

extern void d13x_board_initialize(void);
extern void d13x_chip_late_boot(void);
extern int d13x_board_bringup(void);

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
  board_boot_puts("[D13X] board early\r\n");
  /*
   * TinySPL has already configured the clocks and UART0 pins well enough for
   * NuttX bring-up.  Reprogramming sysclk/pinmux here can silence UART0 or
   * hang before NSH starts, so keep board-early inert until the OS boots.
   */
  board_boot_puts("[D13X] board early ok\r\n");
}
#endif

#ifdef CONFIG_BOARD_LATE_INITIALIZE
void board_late_initialize(void)
{
  int ret;

  board_boot_puts("[D13X] board late\r\n");
  d13x_chip_late_boot();

  ret = d13x_board_bringup();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[D13X] board bringup failed: %d\n", ret);
    }

  board_boot_puts("[D13X] board late ok\r\n");
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
