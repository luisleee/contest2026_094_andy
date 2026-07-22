/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_serial.c
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include <nuttx/irq.h>
#include <nuttx/kthread.h>
#include <nuttx/serial/serial.h>
#include <nuttx/signal.h>
#include <nuttx/spinlock.h>

#include "aic_soc.h"
#include "irq.h"

#define D13X_CONSOLE_UART_BASE       UART0_BASE
#define D13X_CONSOLE_UART_IRQ        D13X_IRQ_UART0
#define D13X_CONSOLE_UART_BAUD       115200UL
#define D13X_CONSOLE_UART_CLK        48000000UL
#define D13X_CONSOLE_UART_DIVISOR    26U
#define D13X_CONSOLE_UART_CMU_DIV    25U

#define D13X_CMU_UART0_REG           (CMU_BASE + 0x0840UL)
#define D13X_CMU_GPIO_REG            (CMU_BASE + 0x083cUL)
#define D13X_CMU_GATE_MOD            (1U << 8)
#define D13X_CMU_GATE_BUS            (1U << 12)
#define D13X_CMU_RSTN                (1U << 13)
#define D13X_CMU_DIV_SHIFT           0U
#define D13X_CMU_DIV_MASK            (0x1fU << D13X_CMU_DIV_SHIFT)

#define D13X_GPIO_GROUP_STRIDE       0x100UL
#define D13X_GPIO_PINCFG_BASE        0x080UL
#define D13X_GPIO_PINCFG_STRIDE      0x004UL
#define D13X_GPIO_GROUP_A            0U
#define D13X_GPIO_PA0                0U
#define D13X_GPIO_PA1                1U
#define D13X_PINMUX_UART_FUNC        5U
#define D13X_PINMUX_DRV_DEFAULT      3U
#define D13X_PINMUX_PULL_NONE        0U
#define D13X_PINMUX_PULL_UP          3U
#define D13X_PINMUX_FUNC_SHIFT       0U
#define D13X_PINMUX_DRV_SHIFT        4U
#define D13X_PINMUX_PULL_SHIFT       8U

#define D13X_UART_RBR_OFF            0x00UL
#define D13X_UART_THR_OFF            0x00UL
#define D13X_UART_DLL_OFF            0x00UL
#define D13X_UART_DLH_OFF            0x04UL
#define D13X_UART_IER_OFF            0x04UL
#define D13X_UART_IIR_OFF            0x08UL
#define D13X_UART_FCR_OFF            0x08UL
#define D13X_UART_LCR_OFF            0x0cUL
#define D13X_UART_MCR_OFF            0x10UL
#define D13X_UART_LSR_OFF            0x14UL
#define D13X_UART_USR_OFF            0x7cUL

#define D13X_GPIO_PA1_CFG            (GPIO_BASE + 0x084UL)
#define D13X_GPIO_PINMUX_MASK        0x0004037fU
#define D13X_GPIO_UART0_RX_CFG       0x00000035U
#define D13X_UART_MCR_FUNC_MASK      0x000000c0U

#define D13X_UART_IER_ERBFI          0x01U
#define D13X_UART_IER_ETBEI          0x02U

#define D13X_UART_IIR_NO_INT         0x01U
#define D13X_UART_IIR_THR_EMPTY      0x02U
#define D13X_UART_IIR_RECV_DATA      0x04U
#define D13X_UART_IIR_RECV_LINE      0x06U
#define D13X_UART_IIR_BUSY           0x07U
#define D13X_UART_IIR_CHAR_TIMEOUT   0x0cU

#define D13X_UART_FCR_FIFO_EN        0x01U
#define D13X_UART_FCR_RXRST          0x02U
#define D13X_UART_FCR_TXRST          0x04U

#define D13X_UART_LCR_DLAB           0x80U
#define D13X_UART_LCR_8N1            0x03U

#define D13X_UART_LSR_DR             0x01U
#define D13X_UART_LSR_THRE           0x20U
#define D13X_UART_LSR_TEMT           0x40U

#define D13X_UART_PUTC_TIMEOUT       1000000U
#define D13X_UART_TXBUFSIZE          256
#define D13X_UART_RXBUFSIZE          256
#define D13X_UART_DEBUG_DELAY        50000U
#define D13X_UART_RXPOLL_PRIORITY    100
#define D13X_UART_RXPOLL_STACKSIZE   1536
#define D13X_UART_RXPOLL_DELAY_US    1000

struct d13x_uart_s
{
  uintptr_t base;
  int irq;
  bool initialized;
  spinlock_t lock;
};

static int d13x_uart_setup(FAR struct uart_dev_s *dev);
static void d13x_uart_shutdown(FAR struct uart_dev_s *dev);
static int d13x_uart_attach(FAR struct uart_dev_s *dev);
static void d13x_uart_detach(FAR struct uart_dev_s *dev);
static int d13x_uart_interrupt(int irq, void *context, void *arg);
static int d13x_uart_ioctl(FAR struct file *filep, int cmd,
                           unsigned long arg);
static int d13x_uart_receive(FAR struct uart_dev_s *dev,
                             FAR unsigned int *status);
static void d13x_uart_rxint(FAR struct uart_dev_s *dev, bool enable);
static bool d13x_uart_rxavailable(FAR struct uart_dev_s *dev);
static void d13x_uart_send(FAR struct uart_dev_s *dev, int ch);
static void d13x_uart_txint(FAR struct uart_dev_s *dev, bool enable);
static bool d13x_uart_txready(FAR struct uart_dev_s *dev);
static bool d13x_uart_txempty(FAR struct uart_dev_s *dev);
static int d13x_uart_rxpoll(int argc, FAR char *argv[]);
void up_putc(int ch);

static const struct uart_ops_s g_d13x_uart_ops =
{
  .setup       = d13x_uart_setup,
  .shutdown    = d13x_uart_shutdown,
  .attach      = d13x_uart_attach,
  .detach      = d13x_uart_detach,
  .ioctl       = d13x_uart_ioctl,
  .receive     = d13x_uart_receive,
  .rxint       = d13x_uart_rxint,
  .rxavailable = d13x_uart_rxavailable,
  .send        = d13x_uart_send,
  .txint       = d13x_uart_txint,
  .txready     = d13x_uart_txready,
  .txempty     = d13x_uart_txempty,
};

static char g_d13x_uart0rxbuffer[D13X_UART_RXBUFSIZE];
static char g_d13x_uart0txbuffer[D13X_UART_TXBUFSIZE];
static bool g_d13x_uart_rxpoll_started;

static struct d13x_uart_s g_d13x_uart0priv =
{
  .base        = D13X_CONSOLE_UART_BASE,
  .irq         = D13X_CONSOLE_UART_IRQ,
  .initialized = false,
};

static struct uart_dev_s g_d13x_uart0port =
{
  .recv =
    {
      .size   = D13X_UART_RXBUFSIZE,
      .buffer = g_d13x_uart0rxbuffer,
    },
  .xmit =
    {
      .size   = D13X_UART_TXBUFSIZE,
      .buffer = g_d13x_uart0txbuffer,
    },
  .ops       = &g_d13x_uart_ops,
  .priv      = &g_d13x_uart0priv,
  .isconsole = true,
};

static inline uint32_t d13x_uart_getreg32(uintptr_t addr)
{
  return *(volatile uint32_t *)addr;
}

static inline void d13x_uart_putreg32(uint32_t value, uintptr_t addr)
{
  *(volatile uint32_t *)addr = value;
}

static void d13x_uart_hwinit(struct d13x_uart_s *priv)
{
  if (priv->initialized)
    {
      return;
    }

  /* tinySPL has already initialized UART0 for the same console.  Preserve
   * its clock, divisor, FIFO, and TX state. Explicitly claim PA1 for UART0
   * RX because the bootloader does not guarantee the receive pin mux.
   */

  d13x_uart_putreg32((d13x_uart_getreg32(D13X_GPIO_PA1_CFG) &
                      ~D13X_GPIO_PINMUX_MASK) |
                     D13X_GPIO_UART0_RX_CFG,
                     D13X_GPIO_PA1_CFG);
  d13x_uart_putreg32(d13x_uart_getreg32(priv->base + D13X_UART_MCR_OFF) &
                     ~D13X_UART_MCR_FUNC_MASK,
                     priv->base + D13X_UART_MCR_OFF);

  priv->initialized = true;
}

static void d13x_uart_lowputc(struct d13x_uart_s *priv, int ch)
{
  volatile uint32_t delay;

  /* tinySPL leaves UART0 usable, but the line-status readiness bits are not
   * reliable enough yet for the early console path.  Use a bounded pacing
   * delay instead of blocking the only debug channel on LSR_THRE.
   */

  d13x_uart_putreg32((uint32_t)ch, priv->base + D13X_UART_THR_OFF);

  for (delay = 0; delay < D13X_UART_DEBUG_DELAY; delay++)
    {
      asm volatile ("nop");
    }
}

static void d13x_uart_puts(const char *str)
{
  while (*str != '\0')
    {
      up_putc(*str++);
    }
}

static int d13x_uart_setup(FAR struct uart_dev_s *dev)
{
  FAR struct d13x_uart_s *priv = dev->priv;

  d13x_uart_hwinit(priv);
  return OK;
}

static void d13x_uart_shutdown(FAR struct uart_dev_s *dev)
{
  FAR struct d13x_uart_s *priv = dev->priv;

  d13x_uart_putreg32(0, priv->base + D13X_UART_IER_OFF);
  d13x_uart_detach(dev);
}

static int d13x_uart_attach(FAR struct uart_dev_s *dev)
{
  FAR struct d13x_uart_s *priv = dev->priv;
  int ret;

  ret = irq_attach(priv->irq, d13x_uart_interrupt, dev);
  if (ret == OK)
    {
      up_disable_irq(priv->irq);
      d13x_uart_putreg32(d13x_uart_getreg32(priv->base +
                           D13X_UART_IER_OFF) &
                           ~(D13X_UART_IER_ERBFI | D13X_UART_IER_ETBEI),
                           priv->base + D13X_UART_IER_OFF);
    }

  return ret;
}

static void d13x_uart_detach(FAR struct uart_dev_s *dev)
{
  FAR struct d13x_uart_s *priv = dev->priv;

  d13x_uart_putreg32(0, priv->base + D13X_UART_IER_OFF);
  up_disable_irq(priv->irq);
  irq_detach(priv->irq);
}

static int d13x_uart_interrupt(int irq, void *context, void *arg)
{
  FAR struct uart_dev_s *dev = arg;
  FAR struct d13x_uart_s *priv = dev->priv;
  uint32_t iir;
  int passes;

  (void)irq;
  (void)context;

  for (passes = 0; passes < 256; passes++)
    {
      iir = d13x_uart_getreg32(priv->base + D13X_UART_IIR_OFF);
      if ((iir & D13X_UART_IIR_NO_INT) != 0 &&
          (iir & 0x0fU) != D13X_UART_IIR_BUSY)
        {
          break;
        }

      switch (iir & 0x0fU)
        {
          case D13X_UART_IIR_RECV_DATA:
          case D13X_UART_IIR_CHAR_TIMEOUT:
            uart_recvchars(dev);
            break;

          case D13X_UART_IIR_THR_EMPTY:
            d13x_uart_putreg32(d13x_uart_getreg32(priv->base +
                               D13X_UART_IER_OFF) &
                               ~D13X_UART_IER_ETBEI,
                               priv->base + D13X_UART_IER_OFF);
            break;

          case D13X_UART_IIR_RECV_LINE:
            (void)d13x_uart_getreg32(priv->base + D13X_UART_LSR_OFF);
            uart_recvchars(dev);
            break;

          case D13X_UART_IIR_BUSY:
            (void)d13x_uart_getreg32(priv->base + D13X_UART_USR_OFF);
            break;

          default:
            (void)d13x_uart_getreg32(priv->base + D13X_UART_LSR_OFF);
            break;
        }
    }

  return OK;
}

static int d13x_uart_ioctl(FAR struct file *filep, int cmd, unsigned long arg)
{
  (void)filep;
  (void)cmd;
  (void)arg;
  return -ENOTTY;
}

static int d13x_uart_receive(FAR struct uart_dev_s *dev,
                             FAR unsigned int *status)
{
  FAR struct d13x_uart_s *priv = dev->priv;

  if (status != NULL)
    {
      *status = d13x_uart_getreg32(priv->base + D13X_UART_LSR_OFF);
    }

  return d13x_uart_getreg32(priv->base + D13X_UART_RBR_OFF) & 0xffU;
}

static void d13x_uart_rxint(FAR struct uart_dev_s *dev, bool enable)
{
  FAR struct d13x_uart_s *priv = dev->priv;
  uint32_t ier;

  ier = d13x_uart_getreg32(priv->base + D13X_UART_IER_OFF);
  ier &= ~D13X_UART_IER_ERBFI;
  d13x_uart_putreg32(ier, priv->base + D13X_UART_IER_OFF);

  (void)enable;
}

static bool d13x_uart_rxavailable(FAR struct uart_dev_s *dev)
{
  FAR struct d13x_uart_s *priv = dev->priv;

  return (d13x_uart_getreg32(priv->base + D13X_UART_LSR_OFF) &
          D13X_UART_LSR_DR) != 0;
}

static void d13x_uart_send(FAR struct uart_dev_s *dev, int ch)
{
  FAR struct d13x_uart_s *priv = dev->priv;

  d13x_uart_lowputc(priv, ch);
}

static void d13x_uart_txint(FAR struct uart_dev_s *dev, bool enable)
{
  FAR struct d13x_uart_s *priv = dev->priv;
  uint32_t ier;

  ier = d13x_uart_getreg32(priv->base + D13X_UART_IER_OFF);
  ier &= ~D13X_UART_IER_ETBEI;
  d13x_uart_putreg32(ier, priv->base + D13X_UART_IER_OFF);

  if (enable)
    {
      uart_xmitchars(dev);
    }
}

static bool d13x_uart_txready(FAR struct uart_dev_s *dev)
{
  (void)dev;
  return true;
}

static bool d13x_uart_txempty(FAR struct uart_dev_s *dev)
{
  (void)dev;
  return true;
}

static int d13x_uart_rxpoll(int argc, FAR char *argv[])
{
  (void)argc;
  (void)argv;

  for (;;)
    {
      if (d13x_uart_rxavailable(&g_d13x_uart0port))
        {
          uart_recvchars(&g_d13x_uart0port);
        }
      else
        {
          nxsig_usleep(D13X_UART_RXPOLL_DELAY_US);
        }
    }

  return OK;
}

void riscv_earlyserialinit(void)
{
  (void)d13x_uart_setup(&g_d13x_uart0port);
  d13x_uart_puts("[D13X] riscv_earlyserialinit\n");
}

void riscv_serialinit(void)
{
  d13x_uart_puts("[D13X] riscv_serialinit\n");
  uart_register("/dev/console", &g_d13x_uart0port);
  uart_register("/dev/ttyS0", &g_d13x_uart0port);
  d13x_uart_puts("[D13X] console reg\n");

  if (!g_d13x_uart_rxpoll_started)
    {
      int pid;

      pid = kthread_create("uart0_rx", D13X_UART_RXPOLL_PRIORITY,
                           D13X_UART_RXPOLL_STACKSIZE,
                           d13x_uart_rxpoll, NULL);
      if (pid >= 0)
        {
          g_d13x_uart_rxpoll_started = true;
        }
    }
}

void up_putc(int ch)
{
  FAR struct d13x_uart_s *priv = &g_d13x_uart0priv;

  d13x_uart_hwinit(priv);

  if (ch == '\n')
    {
      d13x_uart_lowputc(priv, '\r');
    }

  d13x_uart_lowputc(priv, ch);
}
