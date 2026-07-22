/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_probe.c
 ****************************************************************************/

#include <nuttx/config.h>

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>

#include "aic_soc.h"

#define D13X_PROBE_UART_DELAY 50000U

struct tcb_s;

static void d13x_probe_putc(int ch)
{
  volatile uint32_t delay;

  *(volatile uint32_t *)(UART0_BASE + 0x00) = (uint32_t)ch;

  for (delay = 0; delay < D13X_PROBE_UART_DELAY; delay++)
    {
      asm volatile ("nop");
    }
}

static void d13x_probe_puts(const char *str)
{
  while (*str != '\0')
    {
      d13x_probe_putc(*str++);
    }
}

static void d13x_probe_puthex2(unsigned int value)
{
  static const char hex[] = "0123456789abcdef";

  d13x_probe_putc(hex[(value >> 4) & 0x0f]);
  d13x_probe_putc(hex[value & 0x0f]);
}

void __real_umm_initialize(void *heap_start, size_t heap_size);
void *__real_zalloc(size_t size);
int __real_group_initialize(struct tcb_s *tcb, int ttype, size_t heapsize);
void __real_up_initial_state(struct tcb_s *tcb);
int __real_tls_init_info(struct tcb_s *tcb);
void __real_group_postinitialize(struct tcb_s *tcb);
void __real_instrument_initialize(void);
void __real_fs_initialize(void);
void __real_irq_initialize(void);
void __real_clock_initialize(void);
void __real_up_initialize(void);
void *__real_mm_zalloc(void *heap, size_t size);
void *__real_mm_malloc(void *heap, size_t size);
int __real_nxrmutex_lock(void *rmutex);
int __real_nxrmutex_unlock(void *rmutex);
size_t __real_mm_heapfree(void *heap);
size_t __real_mm_heapfree_largest(void *heap);
void __real_mm_notify_pressure(size_t free_size, size_t largest_free_size);
void __real_nx_bringup(void);
int __real_nxthread_create(const char *name, int ttype, int priority,
                           void *stack_addr, int stack_size, main_t entry,
                           char * const argv[], char * const envp[]);
void __real_nxtask_activate(struct tcb_s *tcb);
void __real_nxsched_resume_scheduler(struct tcb_s *tcb);
void __real_up_switch_context(struct tcb_s *tcb, struct tcb_s *rtcb);
void __real_nxtask_start(void);
int __real_riscv_swint(int irq, void *context, void *arg);
uintreg_t *__real_riscv_doirq(int irq, uintreg_t *regs);
int __real_nsh_main(int argc, char **argv);
void __real_nsh_initialize(void);
int __real_nsh_consolemain(int argc, char **argv);
int __real_nsh_session(void *pstate, int login, int argc, char **argv);
int __real_open(const char *path, int oflags, ...);
ssize_t __real_read(int fd, void *buf, size_t nbytes);
ssize_t __real_write(int fd, const void *buf, size_t nbytes);

void __wrap_umm_initialize(void *heap_start, size_t heap_size)
{
  d13x_probe_puts("M0\r\n");
  __real_umm_initialize(heap_start, heap_size);
  d13x_probe_puts("M1\r\n");
}

void *__wrap_zalloc(size_t size)
{
  void *ret;

  d13x_probe_puts("Z0\r\n");
  ret = __real_zalloc(size);
  d13x_probe_puts("Z1\r\n");
  return ret;
}

int __wrap_group_initialize(struct tcb_s *tcb, int ttype, size_t heapsize)
{
  int ret;

  d13x_probe_puts("G0\r\n");
  ret = __real_group_initialize(tcb, ttype, heapsize);
  d13x_probe_puts("G1\r\n");
  return ret;
}

void __wrap_up_initial_state(struct tcb_s *tcb)
{
  d13x_probe_puts("U0\r\n");
  __real_up_initial_state(tcb);
  d13x_probe_puts("U1\r\n");
}

int __wrap_tls_init_info(struct tcb_s *tcb)
{
  int ret;

  d13x_probe_puts("L0\r\n");
  ret = __real_tls_init_info(tcb);
  d13x_probe_puts("L1\r\n");
  return ret;
}

void __wrap_group_postinitialize(struct tcb_s *tcb)
{
  d13x_probe_puts("P0\r\n");
  __real_group_postinitialize(tcb);
  d13x_probe_puts("P1\r\n");
}

void __wrap_instrument_initialize(void)
{
  d13x_probe_puts("N0\r\n");
  __real_instrument_initialize();
  d13x_probe_puts("N1\r\n");
}

void __wrap_fs_initialize(void)
{
  d13x_probe_puts("F0\r\n");
  __real_fs_initialize();
  d13x_probe_puts("F1\r\n");
}

void __wrap_irq_initialize(void)
{
  d13x_probe_puts("Q0\r\n");
  __real_irq_initialize();
  d13x_probe_puts("Q1\r\n");
}

void __wrap_clock_initialize(void)
{
  d13x_probe_puts("K0\r\n");
  __real_clock_initialize();
  d13x_probe_puts("K1\r\n");
}

void __wrap_up_initialize(void)
{
  d13x_probe_puts("A0\r\n");
  __real_up_initialize();
  d13x_probe_puts("A1\r\n");
}

void *__wrap_mm_zalloc(void *heap, size_t size)
{
  void *ret;

  d13x_probe_puts("Y0\r\n");
  ret = __real_mm_zalloc(heap, size);
  d13x_probe_puts("Y1\r\n");
  return ret;
}

void *__wrap_mm_malloc(void *heap, size_t size)
{
  void *ret;

  d13x_probe_puts("X0\r\n");
  ret = __real_mm_malloc(heap, size);
  d13x_probe_puts("X1\r\n");
  return ret;
}

int __wrap_nxrmutex_lock(void *rmutex)
{
  int ret;

  d13x_probe_puts("R0\r\n");
  ret = __real_nxrmutex_lock(rmutex);
  d13x_probe_puts("R1\r\n");
  return ret;
}

int __wrap_nxrmutex_unlock(void *rmutex)
{
  int ret;

  d13x_probe_puts("O0\r\n");
  ret = __real_nxrmutex_unlock(rmutex);
  d13x_probe_puts("O1\r\n");
  return ret;
}

size_t __wrap_mm_heapfree(void *heap)
{
  size_t ret;

  d13x_probe_puts("E0\r\n");
  ret = __real_mm_heapfree(heap);
  d13x_probe_puts("E1\r\n");
  return ret;
}

size_t __wrap_mm_heapfree_largest(void *heap)
{
  size_t ret;

  d13x_probe_puts("B0\r\n");
  ret = __real_mm_heapfree_largest(heap);
  d13x_probe_puts("B1\r\n");
  return ret;
}

void __wrap_mm_notify_pressure(size_t free_size, size_t largest_free_size)
{
  d13x_probe_puts("D0\r\n");
  __real_mm_notify_pressure(free_size, largest_free_size);
  d13x_probe_puts("D1\r\n");
}

void __wrap_nx_bringup(void)
{
  d13x_probe_puts("V0\r\n");
  __real_nx_bringup();
  d13x_probe_puts("V1\r\n");
}

int __wrap_nxthread_create(const char *name, int ttype, int priority,
                           void *stack_addr, int stack_size, main_t entry,
                           char * const argv[], char * const envp[])
{
  int ret;

  d13x_probe_puts("W0\r\n");
  ret = __real_nxthread_create(name, ttype, priority, stack_addr,
                               stack_size, entry, argv, envp);
  d13x_probe_puts("W1\r\n");
  return ret;
}

void __wrap_nxtask_activate(struct tcb_s *tcb)
{
  d13x_probe_puts("J0\r\n");
  __real_nxtask_activate(tcb);
  d13x_probe_puts("J1\r\n");
}

void __wrap_nxsched_resume_scheduler(struct tcb_s *tcb)
{
  d13x_probe_puts("C0\r\n");
  __real_nxsched_resume_scheduler(tcb);
  d13x_probe_puts("C1\r\n");
}

void __wrap_up_switch_context(struct tcb_s *tcb, struct tcb_s *rtcb)
{
  d13x_probe_puts("S0\r\n");
  __real_up_switch_context(tcb, rtcb);
  d13x_probe_puts("S1\r\n");
}

void __wrap_nxtask_start(void)
{
  d13x_probe_puts("AA0\r\n");
  __real_nxtask_start();
  d13x_probe_puts("AA1\r\n");
}

int __wrap_riscv_swint(int irq, void *context, void *arg)
{
  int ret;

  d13x_probe_puts("SW0\r\n");
  ret = __real_riscv_swint(irq, context, arg);
  d13x_probe_puts("SW1\r\n");
  return ret;
}

uintreg_t *__wrap_riscv_doirq(int irq, uintreg_t *regs)
{
  uintreg_t *ret;

  d13x_probe_puts("DR0\r\n");
  d13x_probe_putc('d');
  d13x_probe_puthex2((unsigned int)irq);
  d13x_probe_puts("\r\n");
  ret = __real_riscv_doirq(irq, regs);
  d13x_probe_puts("DR1\r\n");
  return ret;
}

int __wrap_nsh_main(int argc, char **argv)
{
  int ret;

  d13x_probe_puts("NM0\r\n");
  ret = __real_nsh_main(argc, argv);
  d13x_probe_puts("NM1\r\n");
  return ret;
}

void __wrap_nsh_initialize(void)
{
  d13x_probe_puts("NI0\r\n");
  __real_nsh_initialize();
  d13x_probe_puts("NI1\r\n");
}

int __wrap_nsh_consolemain(int argc, char **argv)
{
  int ret;

  d13x_probe_puts("NC0\r\n");
  ret = __real_nsh_consolemain(argc, argv);
  d13x_probe_puts("NC1\r\n");
  return ret;
}

int __wrap_nsh_session(void *pstate, int login, int argc, char **argv)
{
  int ret;

  d13x_probe_puts("NS0\r\n");
  ret = __real_nsh_session(pstate, login, argc, argv);
  d13x_probe_puts("NS1\r\n");
  return ret;
}

int __wrap_open(const char *path, int oflags, ...)
{
  mode_t mode = 0;
  int ret;

  if ((oflags & O_CREAT) != 0)
    {
      va_list ap;

      va_start(ap, oflags);
      mode = (mode_t)va_arg(ap, int);
      va_end(ap);
    }

  d13x_probe_puts("OP0\r\n");
  ret = __real_open(path, oflags, mode);
  d13x_probe_puts("OP1\r\n");
  return ret;
}

ssize_t __wrap_read(int fd, void *buf, size_t nbytes)
{
  ssize_t ret;

  d13x_probe_puts("RD0\r\n");
  ret = __real_read(fd, buf, nbytes);
  d13x_probe_puts("RD1\r\n");
  return ret;
}

ssize_t __wrap_write(int fd, const void *buf, size_t nbytes)
{
  ssize_t ret;

  d13x_probe_puts("WR0\r\n");
  ret = __real_write(fd, buf, nbytes);
  d13x_probe_puts("WR1\r\n");
  return ret;
}
