/****************************************************************************
 * Contest 2026 demo88-nor board - boot stub
 ****************************************************************************/

#include <nuttx/board.h>
#include <arch/chip/chip.h>

extern void d13x_board_initialize(void);

#ifdef CONFIG_BOARD_EARLY_INITIALIZE
void board_early_initialize(void)
{
  d13x_board_initialize();
}
#endif

void openvela_board_initialize(void)
{
  /* Placeholder: no hardware initialization. */
}
