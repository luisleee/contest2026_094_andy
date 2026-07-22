/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_irq.c
 ****************************************************************************/

#include <nuttx/config.h>

#include <stddef.h>
#include <stdint.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/sched.h>

#include <core_rv32.h>

#include "riscv_internal.h"

#include "aic_soc.h"
#include "chip.h"

#define D13X_MCAUSE_CODE_MASK 0x3ffU
#define D13X_CSR_MXSTATUS     0x7c0
#define D13X_CSR_MEXSTATUS    0x7e1
#define D13X_CSR_MTVT         0x307
#define D13X_MXSTATUS_MM      (1u << 15)
#define D13X_MXSTATUS_ISAEE   (1u << 22)
#define D13X_MEXSTATUS_SPUSHEN  (1u << 16)
#define D13X_MEXSTATUS_SPSWAPEN (1u << 17)

static void d13x_raw_uart_putc(int ch)
{
  *(volatile uint32_t *)(UART0_BASE + 0x00) = (uint32_t)ch;
}

static void d13x_raw_uart_puts(const char *str)
{
  while (*str != '\0')
    {
      d13x_raw_uart_putc(*str++);
    }
}

static void d13x_raw_uart_puthex2(unsigned int value)
{
  static const char hex[] = "0123456789abcdef";

  d13x_raw_uart_putc(hex[(value >> 4) & 0x0f]);
  d13x_raw_uart_putc(hex[value & 0x0f]);
}

static void d13x_raw_uart_puthex32(uint32_t value)
{
  d13x_raw_uart_puthex2((value >> 24) & 0xff);
  d13x_raw_uart_puthex2((value >> 16) & 0xff);
  d13x_raw_uart_puthex2((value >> 8) & 0xff);
  d13x_raw_uart_puthex2(value & 0xff);
}

static void d13x_dump_exception(int irq, uintreg_t *regs)
{
  d13x_raw_uart_puts("\r\n[D13X] exception irq=");
  d13x_raw_uart_puthex2((unsigned int)irq);

  if (regs != NULL)
    {
      d13x_raw_uart_puts(" epc=");
      d13x_raw_uart_puthex32((uint32_t)regs[REG_EPC]);
      d13x_raw_uart_puts(" ra=");
      d13x_raw_uart_puthex32((uint32_t)regs[REG_X1]);
      d13x_raw_uart_puts(" sp=");
      d13x_raw_uart_puthex32((uint32_t)regs[REG_X2]);
    }

  d13x_raw_uart_puts(" tv=");
  d13x_raw_uart_puthex32((uint32_t)READ_CSR(CSR_TVAL));

  if (g_running_task != NULL)
    {
      d13x_raw_uart_puts(" tcb=");
      d13x_raw_uart_puthex32((uint32_t)g_running_task);
      d13x_raw_uart_puts(" xr=");
      d13x_raw_uart_puthex32((uint32_t)g_running_task->xcp.regs);
      d13x_raw_uart_puts(" sb=");
      d13x_raw_uart_puthex32((uint32_t)g_running_task->stack_base_ptr);
      d13x_raw_uart_puts(" ss=");
      d13x_raw_uart_puthex32((uint32_t)g_running_task->adj_stack_size);
    }

  d13x_raw_uart_puts("\r\n");
}

static void d13x_e907_prepare_traps(void)
{
  uintreg_t mxstatus;
  uintreg_t mexstatus;

  mxstatus = READ_CSR(D13X_CSR_MXSTATUS);
  mxstatus |= D13X_MXSTATUS_MM | D13X_MXSTATUS_ISAEE;
  WRITE_CSR(D13X_CSR_MXSTATUS, mxstatus);

  /* The ArtInChip boot chain may leave E907 automatic stack push/swap
   * enabled.  NuttX saves its own software exception frame, so keep these
   * hardware stack extensions disabled before CLIC interrupts are enabled.
   */

  mexstatus = READ_CSR(D13X_CSR_MEXSTATUS);
  mexstatus &= ~(D13X_MEXSTATUS_SPUSHEN | D13X_MEXSTATUS_SPSWAPEN);
  WRITE_CSR(D13X_CSR_MEXSTATUS, mexstatus);
}

extern uint32_t __trap_vec[];

void up_irqinitialize(void)
{
  int id;

  up_irq_save();
  d13x_e907_prepare_traps();

  WRITE_CSR(CSR_MTVEC, (uintptr_t)__trap_vec | 2U);
  WRITE_CSR(D13X_CSR_MTVT, (uintptr_t)__trap_vec);

  CLIC->CLICCFG = (((CLIC->CLICINFO & CLIC_INFO_CLICINTCTLBITS_Msk) >>
                    CLIC_INFO_CLICINTCTLBITS_Pos)
                   << CLIC_CLICCFG_NLBIT_Pos);
  for (id = 0; id <= D13X_IRQN_LAST; id++)
    {
      csi_vic_disable_irq(id);
      CLIC->CLICINT[id].IP = 0;
      CLIC->CLICINT[id].ATTR = 0;
      csi_vic_set_prio(id, 0);
    }

  CLIC->CLICINT[Machine_Software_IRQn].ATTR = 0x2;
  csi_vic_set_threshold(0);

#if defined(CONFIG_STACK_COLORATION) && CONFIG_ARCH_INTERRUPTSTACK > 15
  {
    size_t intstack_size = (CONFIG_ARCH_INTERRUPTSTACK & ~15);
    riscv_stack_color(g_intstackalloc, intstack_size);
  }
#endif

  riscv_exception_attach();

#ifndef CONFIG_SUPPRESS_INTERRUPTS
  riscv_color_intstack();
  up_irq_enable();
#endif
}

void up_enable_irq(int irq)
{
  int extirq = irq - RISCV_IRQ_ASYNC;

  if (irq == RISCV_IRQ_MSOFT)
    {
      SET_CSR(CSR_MIE, MIE_MSIE);
    }
  else if (irq == RISCV_IRQ_MTIMER)
    {
      SET_CSR(CSR_MIE, MIE_MTIE);
    }
  else if (irq == RISCV_IRQ_MEXT)
    {
      SET_CSR(CSR_MIE, MIE_MEIE);
    }

  if (irq >= RISCV_IRQ_ASYNC &&
      extirq >= 0 && extirq <= D13X_IRQN_LAST)
    {
      csi_vic_set_prio(extirq, UINT8_MAX);
      csi_vic_enable_irq(extirq);
    }
}

void up_disable_irq(int irq)
{
  int extirq = irq - RISCV_IRQ_ASYNC;

  if (irq == RISCV_IRQ_MSOFT)
    {
      CLEAR_CSR(CSR_MIE, MIE_MSIE);
    }
  else if (irq == RISCV_IRQ_MTIMER)
    {
      CLEAR_CSR(CSR_MIE, MIE_MTIE);
    }
  else if (irq == RISCV_IRQ_MEXT)
    {
      CLEAR_CSR(CSR_MIE, MIE_MEIE);
    }

  if (irq >= RISCV_IRQ_ASYNC &&
      extirq >= 0 && extirq <= D13X_IRQN_LAST)
    {
      csi_vic_clear_pending_irq(extirq);
      csi_vic_disable_irq(extirq);
    }
}

irqstate_t up_irq_enable(void)
{
  irqstate_t oldstat;

  /* Keep machine external interrupts globally reachable. Timer/software
   * sources are enabled individually by up_enable_irq().
   */

  SET_CSR(CSR_MIE, MIE_MEIE);

  oldstat = READ_AND_SET_CSR(CSR_MSTATUS, MSTATUS_MIE);
  return oldstat;
}

void *riscv_dispatch_irq(uintreg_t vector, uintreg_t *regs)
{
  int irq = (int)(vector & D13X_MCAUSE_CODE_MASK);

  if ((vector & RISCV_IRQ_BIT) != 0)
    {
      irq += RISCV_IRQ_ASYNC;
    }

  if (irq >= 0 && irq < NR_IRQS)
    {
      if (irq <= RISCV_MAX_EXCEPTION && irq != RISCV_IRQ_ECALLM &&
          regs != NULL)
        {
          d13x_dump_exception(irq, regs);
        }

      regs = riscv_doirq(irq, regs);
    }

  return regs;
}
