/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_start.c
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>

#include <nuttx/arch.h>
#include <nuttx/init.h>
#include <sys/mount.h>
#include <syslog.h>

#include "riscv_internal.h"

#include "chip.h"

#ifdef CONFIG_USING_SFUD
#  include <sfud.h>
#endif

extern void aic_board_pinmux_init(void);
extern void aic_board_sysclk_init(void);
extern void up_putc(int ch);

static void d13x_boot_puts(const char *str)
{
  while (*str != '\0')
    {
      up_putc(*str++);
    }
}

void d13x_chip_early_boot(void)
{
  /* Reserved for earlier chip-only hooks if D13X later needs them. */
}

void d13x_chip_late_boot(void)
{
  int ret = 0;

#if defined(CONFIG_USING_SFUD) && defined(CONFIG_AIC_SPINOR_DRV)
  extern sfud_flash *spinor_init(unsigned int spi_bus);
  sfud_flash *sfud = spinor_init(0);

  if (sfud == NULL)
    {
      syslog(LOG_ERR, "Failed to probe spinor flash.\n");
    }
#endif

#ifdef CONFIG_FS_PROCFS
  ret = mount(NULL, "/proc", "procfs", 0, NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "Failed to mount procfs at /proc: %d\n", ret);
    }
#endif
}

void d13x_board_initialize(void)
{
  aic_board_sysclk_init();
  aic_board_pinmux_init();
}

void d13x_start(uint32_t mhartid)
{
  uint8_t *dest;

  d13x_boot_puts("\n[D13X] start\n");

#ifdef CONFIG_ARCH_FPU
  riscv_fpuconfig();
  d13x_boot_puts("[D13X] fpu ok\n");
#endif

  /* The contest bring-up is single-hart only. Hold any unexpected hart. */

  if (mhartid != 0)
    {
      for (;;)
        {
          asm volatile ("wfi");
        }
    }

  /* Images are loaded directly into SRAM, so only .bss needs clearing here. */

#ifndef CONFIG_ARCH_SKIP_ZERO_BSS
  for (dest = _sbss; dest < _ebss; )
    {
      *dest++ = 0;
    }
#endif

  d13x_boot_puts("[D13X] bss ok\n");

#ifdef USE_EARLYSERIALINIT
  riscv_earlyserialinit();
  d13x_boot_puts("[D13X] early serial ok\n");
#endif

  d13x_chip_early_boot();
  d13x_boot_puts("[D13X] nx_start\n");

  nx_start();

  for (;;)
    {
      asm volatile ("wfi");
    }
}

uintptr_t up_addrenv_va_to_pa(void *va)
{
  return (uintptr_t)va;
}

void *up_addrenv_pa_to_va(uintptr_t pa)
{
  return (void *)pa;
}
