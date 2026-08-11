/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_gmac.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * D13x GMAC0 RMII Ethernet lower-half for the demo88-nor board.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <debug.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/net/ethernet.h>
#include <nuttx/net/ip.h>
#include <nuttx/net/netdev.h>
#include <nuttx/net/pkt.h>
#include <nuttx/wdog.h>
#include <nuttx/wqueue.h>

#include "riscv_internal.h"

#include "chip.h"
#include "include/d13x_gmac.h"
#include "include/irq.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define D13X_GMAC_REG(offset)         (EMAC_BASE + (offset))

#define D13X_GMAC_MACCONF            0x0000u
#define D13X_GMAC_DMA0CONF           0x0004u
#define D13X_GMAC_DMA0INTSTS         0x000cu
#define D13X_GMAC_DMA0INTEN          0x0010u
#define D13X_GMAC_MACTXFUNC          0x001cu
#define D13X_GMAC_MACRXFUNC          0x0020u
#define D13X_GMAC_TXDMA0CTL          0x0024u
#define D13X_GMAC_RXDMA0CTL          0x0028u
#define D13X_GMAC_MACFRMFLT          0x0040u
#define D13X_GMAC_MACADDR0HIGH       0x0050u
#define D13X_GMAC_MACADDR0LOW        0x0054u
#define D13X_GMAC_MDIOCTL            0x0090u
#define D13X_GMAC_MDIODATA           0x0094u
#define D13X_GMAC_TXDESCSTART        0x00b0u
#define D13X_GMAC_RXDESCSTART        0x00b4u
#define D13X_GMAC_VERSION            0x0ffcu

#define D13X_GMAC_MACCONF_SWR        (1u << 0)
#define D13X_GMAC_MACCONF_SPEED_MASK (3u << 1)
#define D13X_GMAC_MACCONF_SPEED_10   (2u << 1)
#define D13X_GMAC_MACCONF_SPEED_100  (3u << 1)
#define D13X_GMAC_MACCONF_DUPLEX     (1u << 4)

#define D13X_GMAC_MACTX_ENABLE       (1u << 0)
#define D13X_GMAC_MACRX_ENABLE       (1u << 0)

#define D13X_GMAC_DMA0CONF_AAL       (1u << 26)
#define D13X_GMAC_DMA0CONF_USP       (1u << 25)
#define D13X_GMAC_DMA0CONF_RXPBL_8   (8u << 19)
#define D13X_GMAC_DMA0CONF_FB        (1u << 17)
#define D13X_GMAC_DMA0CONF_PBLX8     (1u << 16)
#define D13X_GMAC_DMA0CONF_PBL_8     (8u << 10)
#define D13X_GMAC_DMA0CONF_ATDS      (1u << 9)
#define D13X_GMAC_DMA0CONF_PR_3_1    (2u << 1)

#define D13X_GMAC_DMAINT_NIS         (1u << 16)
#define D13X_GMAC_DMAINT_AIS         (1u << 15)
#define D13X_GMAC_DMAINT_FATAL       (1u << 13)
#define D13X_GMAC_DMAINT_RX_UNAVAIL  (1u << 7)
#define D13X_GMAC_DMAINT_RX          (1u << 6)
#define D13X_GMAC_DMAINT_TX          (1u << 0)
#define D13X_GMAC_DMAINT_MASK        (D13X_GMAC_DMAINT_NIS | \
                                      D13X_GMAC_DMAINT_AIS | \
                                      D13X_GMAC_DMAINT_FATAL | \
                                      D13X_GMAC_DMAINT_RX_UNAVAIL | \
                                      D13X_GMAC_DMAINT_RX | \
                                      D13X_GMAC_DMAINT_TX)

#define D13X_GMAC_TXDMA_POLL         (1u << 7)
#define D13X_GMAC_TXDMA_STORE_FWD    (1u << 5)
#define D13X_GMAC_TXDMA_FLUSH        (1u << 4)
#define D13X_GMAC_TXDMA_START        (1u << 0)
#define D13X_GMAC_RXDMA_POLL         (1u << 9)
#define D13X_GMAC_RXDMA_STORE_FWD    (1u << 3)
#define D13X_GMAC_RXDMA_START        (1u << 0)

#define D13X_MDIO_PHY_SHIFT          11u
#define D13X_MDIO_REG_SHIFT          6u
#define D13X_MDIO_CLOCK_DIV102       (4u << 2)
#define D13X_MDIO_WRITE              (1u << 1)
#define D13X_MDIO_BUSY               (1u << 0)

#define D13X_GMAC_DESC_OWN           (1u << 31)
#define D13X_GMAC_TXDESC_INTERRUPT   (1u << 30)
#define D13X_GMAC_TXDESC_LAST        (1u << 29)
#define D13X_GMAC_TXDESC_FIRST       (1u << 28)
#define D13X_GMAC_TXDESC_CHAINED     (1u << 20)
#define D13X_GMAC_RXDESC_LENGTH_MASK 0x3fff0000u
#define D13X_GMAC_RXDESC_LENGTH_SHIFT 16u
#define D13X_GMAC_RXDESC_ERROR       (1u << 15)
#define D13X_GMAC_RXDESC_CHAINED     (1u << 14)
#define D13X_GMAC_RXDESC_FIRST       (1u << 9)
#define D13X_GMAC_RXDESC_LAST        (1u << 8)

#define D13X_MII_BMCR                0u
#define D13X_MII_BMSR                1u
#define D13X_MII_PHYID1              2u
#define D13X_MII_PHYID2              3u
#define D13X_MII_ADVERTISE           4u
#define D13X_MII_LPA                 5u

#define D13X_BMCR_RESET              (1u << 15)
#define D13X_BMCR_ANENABLE           (1u << 12)
#define D13X_BMCR_ANRESTART          (1u << 9)
#define D13X_BMSR_LINK               (1u << 2)
#define D13X_ADVERTISE_10HALF        (1u << 5)
#define D13X_ADVERTISE_10FULL        (1u << 6)
#define D13X_ADVERTISE_100HALF       (1u << 7)
#define D13X_ADVERTISE_100FULL       (1u << 8)

#define D13X_RTL8201F_PHY_ADDR       0u

#define D13X_CMU_CLK_OUT2            (CMU_BASE + 0x00e8u)
#define D13X_CMU_CLK_GMAC0           (CMU_BASE + 0x0440u)
#define D13X_CMU_CLK_SYSCFG          (CMU_BASE + 0x0800u)
#define D13X_SYSCFG_GMAC0            (SYSCFG_BASE + 0x0410u)

#define D13X_CMU_DIV_MASK            0x1fu
#define D13X_CMU_MODULE_CLOCK        (1u << 8)
#define D13X_CMU_BUS_CLOCK           (1u << 12)
#define D13X_CMU_RESET_RELEASE       (1u << 13)
#define D13X_CMU_CLK_OUT_ENABLE      (1u << 16)
#define D13X_SYSCFG_RMII_EXTCLK      (1u << 1)

#define D13X_GPIO_GROUP_E            4u
#define D13X_GPIO_GROUP_STRIDE       0x100u
#define D13X_GPIO_OUTPUT_CLEAR       0x010u
#define D13X_GPIO_OUTPUT_SET         0x014u
#define D13X_GPIO_PIN_CONFIG         0x080u

#define D13X_GPIO_FUNCTION_MASK      0x0fu
#define D13X_GPIO_DRIVE_MASK         (7u << 4)
#define D13X_GPIO_PULL_MASK          (3u << 8)
#define D13X_GPIO_DIRECTION_MASK     (3u << 16)
#define D13X_GPIO_FUNCTION_GPIO      1u
#define D13X_GPIO_FUNCTION_GMAC      2u
#define D13X_GPIO_DRIVE_LEVEL_3      (3u << 4)
#define D13X_GPIO_DIRECTION_INPUT    (1u << 16)
#define D13X_GPIO_DIRECTION_OUTPUT   (2u << 16)

#define D13X_GMAC_RESET_TIMEOUT_US   100000u
#define D13X_MDIO_TIMEOUT_US         10000u
#define D13X_PHY_RESET_TIMEOUT_MS    500u
#define D13X_PHY_LINK_WAIT_LOOPS     100u
#define D13X_PHY_LINK_WAIT_MS        50u

#define D13X_GMAC_RX_COUNT           16u
#define D13X_GMAC_TX_COUNT           2u
#define D13X_GMAC_BUFFER_SIZE        1536u
#define D13X_GMAC_LINK_POLL          SEC2TICK(1)

#define D13X_MHCR_DCACHE_ENABLE      (1u << 1)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct d13x_gmac_desc_s
{
  volatile uint32_t status;
  uint32_t size;
  uint32_t buffer;
  uint32_t next;
  uint32_t ext_status;
  uint32_t reserved1;
  uint32_t timestamp_low;
  uint32_t timestamp_high;
  uint32_t reserved2[8];
} __attribute__((aligned(CACHE_LINE_SIZE)));

struct d13x_gmac_s
{
  struct net_driver_s dev;
  struct work_s irqwork;
  struct work_s pollwork;
  struct work_s linkwork;
  struct wdog_s linktimer;
  uint8_t phyaddr;
  uint8_t rxhead;
  uint8_t txhead;
  uint8_t txtail;
  uint8_t txpending;
  bool ifup;
  bool link;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct d13x_gmac_s g_d13x_gmac;
static struct d13x_gmac_desc_s g_d13x_gmac_rxdesc[D13X_GMAC_RX_COUNT]
  __attribute__((aligned(CACHE_LINE_SIZE)));
static struct d13x_gmac_desc_s g_d13x_gmac_txdesc[D13X_GMAC_TX_COUNT]
  __attribute__((aligned(CACHE_LINE_SIZE)));
static uint8_t g_d13x_gmac_rxbuffer[D13X_GMAC_RX_COUNT]
                                      [D13X_GMAC_BUFFER_SIZE]
  __attribute__((aligned(CACHE_LINE_SIZE)));
static uint8_t g_d13x_gmac_txbuffer[D13X_GMAC_TX_COUNT]
                                      [D13X_GMAC_BUFFER_SIZE]
  __attribute__((aligned(CACHE_LINE_SIZE)));
static uint8_t g_d13x_gmac_netbuffer[MAX_NETDEV_PKTSIZE +
                                      CONFIG_NET_GUARDSIZE]
  __attribute__((aligned(CACHE_LINE_SIZE)));

#define BUF ((FAR struct eth_hdr_s *)g_d13x_gmac.dev.d_buf)

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void d13x_gmac_link_timer(wdparm_t arg);
static int d13x_gmac_txpoll(FAR struct net_driver_s *dev);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void d13x_gmac_modifyreg32(uintptr_t address, uint32_t clearbits,
                                  uint32_t setbits)
{
  putreg32((getreg32(address) & ~clearbits) | setbits, address);
}

static bool d13x_gmac_dcache_enabled(void)
{
  uint32_t mhcr;

  __asm__ __volatile__ ("csrr %0, 0x7c1" : "=r"(mhcr));
  return (mhcr & D13X_MHCR_DCACHE_ENABLE) != 0;
}

static void d13x_gmac_cache_clean_invalidate(uintptr_t address,
                                             size_t length)
{
  uintptr_t end;

  if (!d13x_gmac_dcache_enabled() || length == 0)
    {
      return;
    }

  end = (address + length + CACHE_LINE_SIZE - 1) &
        ~(uintptr_t)(CACHE_LINE_SIZE - 1);
  address &= ~(uintptr_t)(CACHE_LINE_SIZE - 1);

  __asm__ __volatile__ ("fence rw, rw" ::: "memory");
  while (address < end)
    {
      register uintptr_t cache_line __asm__("a5") = address;

      __asm__ __volatile__ (".long 0x02b7800b"
                            : : "r"(cache_line) : "memory");
      address += CACHE_LINE_SIZE;
    }

  __asm__ __volatile__ ("fence rw, rw" ::: "memory");
}

static void d13x_gmac_cache_invalidate(uintptr_t address, size_t length)
{
  uintptr_t end;

  if (!d13x_gmac_dcache_enabled() || length == 0)
    {
      return;
    }

  end = (address + length + CACHE_LINE_SIZE - 1) &
        ~(uintptr_t)(CACHE_LINE_SIZE - 1);
  address &= ~(uintptr_t)(CACHE_LINE_SIZE - 1);

  __asm__ __volatile__ ("fence rw, rw" ::: "memory");
  while (address < end)
    {
      register uintptr_t cache_line __asm__("a5") = address;

      __asm__ __volatile__ (".long 0x02a7800b"
                            : : "r"(cache_line) : "memory");
      address += CACHE_LINE_SIZE;
    }

  __asm__ __volatile__ ("fence rw, rw" ::: "memory");
}

static uintptr_t d13x_gmac_gpio_reg(uint32_t offset)
{
  return GPIO_BASE + D13X_GPIO_GROUP_E * D13X_GPIO_GROUP_STRIDE + offset;
}

static void d13x_gmac_gpio_config(unsigned int pin, uint32_t function,
                                  uint32_t direction)
{
  uintptr_t address = d13x_gmac_gpio_reg(D13X_GPIO_PIN_CONFIG + pin * 4u);
  uint32_t value = getreg32(address);

  value &= ~(D13X_GPIO_FUNCTION_MASK | D13X_GPIO_DRIVE_MASK |
             D13X_GPIO_PULL_MASK | D13X_GPIO_DIRECTION_MASK);
  value |= function | D13X_GPIO_DRIVE_LEVEL_3 | direction;
  putreg32(value, address);
}

static void d13x_gmac_pinmux(void)
{
  static const uint8_t input_pins[] =
  {
    0, 1, 2, 3, 9
  };

  static const uint8_t output_pins[] =
  {
    4, 5, 7, 8, 10
  };

  unsigned int i;

  for (i = 0; i < sizeof(input_pins); i++)
    {
      d13x_gmac_gpio_config(input_pins[i], D13X_GPIO_FUNCTION_GMAC,
                            D13X_GPIO_DIRECTION_INPUT);
    }

  for (i = 0; i < sizeof(output_pins); i++)
    {
      d13x_gmac_gpio_config(output_pins[i], D13X_GPIO_FUNCTION_GMAC,
                            D13X_GPIO_DIRECTION_OUTPUT);
    }

  d13x_gmac_gpio_config(6, D13X_GPIO_FUNCTION_GPIO,
                        D13X_GPIO_DIRECTION_OUTPUT);
}

static void d13x_gmac_clock_enable(void)
{
  uint32_t value;

  /* CLK_OUT2 = PLL_INT1 / 48 = 25 MHz for the RTL8201F crystal input. */

  putreg32(D13X_CMU_CLK_OUT_ENABLE | D13X_CMU_BUS_CLOCK | 47u,
           D13X_CMU_CLK_OUT2);

  /* GMAC0 module clock = PLL_INT1 / 24 = 50 MHz. */

  value = getreg32(D13X_CMU_CLK_GMAC0);
  value &= ~D13X_CMU_DIV_MASK;
  value |= D13X_CMU_RESET_RELEASE | D13X_CMU_BUS_CLOCK |
           D13X_CMU_MODULE_CLOCK | 23u;
  putreg32(value, D13X_CMU_CLK_GMAC0);

  d13x_gmac_modifyreg32(D13X_CMU_CLK_SYSCFG, 0,
                        D13X_CMU_BUS_CLOCK | D13X_CMU_RESET_RELEASE);
  d13x_gmac_modifyreg32(D13X_SYSCFG_GMAC0, 0, D13X_SYSCFG_RMII_EXTCLK);
}

static void d13x_gmac_phy_hard_reset(void)
{
  putreg32(1u << 6, d13x_gmac_gpio_reg(D13X_GPIO_OUTPUT_CLEAR));
  up_mdelay(50);
  putreg32(1u << 6, d13x_gmac_gpio_reg(D13X_GPIO_OUTPUT_SET));
  up_mdelay(50);
}

static int d13x_mdio_wait(void)
{
  unsigned int timeout;

  for (timeout = 0; timeout < D13X_MDIO_TIMEOUT_US; timeout++)
    {
      if ((getreg32(D13X_GMAC_REG(D13X_GMAC_MDIOCTL)) &
           D13X_MDIO_BUSY) == 0)
        {
          return OK;
        }

      up_udelay(1);
    }

  return -ETIMEDOUT;
}

static int d13x_mdio_read(uint8_t phy, uint8_t reg, uint16_t *value)
{
  uint32_t command;
  int ret;

  ret = d13x_mdio_wait();
  if (ret < 0)
    {
      return ret;
    }

  command = ((uint32_t)phy << D13X_MDIO_PHY_SHIFT) |
            ((uint32_t)reg << D13X_MDIO_REG_SHIFT) |
            D13X_MDIO_CLOCK_DIV102 | D13X_MDIO_BUSY;
  putreg32(command, D13X_GMAC_REG(D13X_GMAC_MDIOCTL));

  ret = d13x_mdio_wait();
  if (ret == OK)
    {
      *value = getreg32(D13X_GMAC_REG(D13X_GMAC_MDIODATA)) & 0xffffu;
    }

  return ret;
}

static int d13x_mdio_write(uint8_t phy, uint8_t reg, uint16_t value)
{
  uint32_t command;
  int ret;

  ret = d13x_mdio_wait();
  if (ret < 0)
    {
      return ret;
    }

  putreg32(value, D13X_GMAC_REG(D13X_GMAC_MDIODATA));
  command = ((uint32_t)phy << D13X_MDIO_PHY_SHIFT) |
            ((uint32_t)reg << D13X_MDIO_REG_SHIFT) |
            D13X_MDIO_CLOCK_DIV102 | D13X_MDIO_WRITE | D13X_MDIO_BUSY;
  putreg32(command, D13X_GMAC_REG(D13X_GMAC_MDIOCTL));

  return d13x_mdio_wait();
}

static int d13x_gmac_reset_controller(void)
{
  unsigned int timeout;

  d13x_gmac_modifyreg32(D13X_GMAC_REG(D13X_GMAC_MACCONF), 0,
                        D13X_GMAC_MACCONF_SWR);

  for (timeout = 0; timeout < D13X_GMAC_RESET_TIMEOUT_US; timeout++)
    {
      if ((getreg32(D13X_GMAC_REG(D13X_GMAC_MACCONF)) &
           D13X_GMAC_MACCONF_SWR) == 0)
        {
          return OK;
        }

      up_udelay(1);
    }

  return -ETIMEDOUT;
}

static bool d13x_gmac_valid_phy_id(uint16_t id1, uint16_t id2)
{
  return id1 != 0 && id1 != 0xffff && id2 != 0 && id2 != 0xffff;
}

static int d13x_gmac_find_phy(FAR struct d13x_gmac_s *priv)
{
  uint16_t id1;
  uint16_t id2;
  unsigned int phy;

  for (phy = 0; phy < 32; phy++)
    {
      if (d13x_mdio_read(phy, D13X_MII_PHYID1, &id1) == OK &&
          d13x_mdio_read(phy, D13X_MII_PHYID2, &id2) == OK &&
          d13x_gmac_valid_phy_id(id1, id2))
        {
          priv->phyaddr = phy;
          syslog(LOG_INFO, "[D13X] GMAC0 PHY addr=%u id=%04x:%04x\n",
                 phy, id1, id2);
          return OK;
        }
    }

  return -ENODEV;
}

static int d13x_gmac_reset_phy(uint8_t phy)
{
  uint16_t bmcr;
  unsigned int timeout;
  int ret;

  ret = d13x_mdio_write(phy, D13X_MII_BMCR, D13X_BMCR_RESET);
  if (ret < 0)
    {
      return ret;
    }

  for (timeout = 0; timeout < D13X_PHY_RESET_TIMEOUT_MS; timeout++)
    {
      up_mdelay(1);
      ret = d13x_mdio_read(phy, D13X_MII_BMCR, &bmcr);
      if (ret == OK && (bmcr & D13X_BMCR_RESET) == 0)
        {
          return OK;
        }
    }

  return -ETIMEDOUT;
}

static int d13x_gmac_read_bmsr(uint8_t phy, uint16_t *bmsr)
{
  int ret;

  ret = d13x_mdio_read(phy, D13X_MII_BMSR, bmsr);
  if (ret < 0)
    {
      return ret;
    }

  /* Link status is latched low in BMSR. Read twice for current state. */

  return d13x_mdio_read(phy, D13X_MII_BMSR, bmsr);
}

static void d13x_gmac_set_link(bool speed100, bool full_duplex)
{
  uint32_t value;

  value = getreg32(D13X_GMAC_REG(D13X_GMAC_MACCONF));
  value &= ~(D13X_GMAC_MACCONF_SPEED_MASK | D13X_GMAC_MACCONF_DUPLEX);
  value |= speed100 ? D13X_GMAC_MACCONF_SPEED_100 :
                      D13X_GMAC_MACCONF_SPEED_10;
  if (full_duplex)
    {
      value |= D13X_GMAC_MACCONF_DUPLEX;
    }

  putreg32(value, D13X_GMAC_REG(D13X_GMAC_MACCONF));
}

static void d13x_gmac_update_link(FAR struct d13x_gmac_s *priv)
{
  uint16_t bmsr;
  uint16_t advertise;
  uint16_t partner;
  uint16_t common;
  bool link;
  bool speed100 = true;
  bool full_duplex = true;

  if (d13x_gmac_read_bmsr(priv->phyaddr, &bmsr) < 0)
    {
      return;
    }

  link = (bmsr & D13X_BMSR_LINK) != 0;
  if (link)
    {
      if (d13x_mdio_read(priv->phyaddr, D13X_MII_ADVERTISE,
                         &advertise) == OK &&
          d13x_mdio_read(priv->phyaddr, D13X_MII_LPA, &partner) == OK)
        {
          common = advertise & partner;
          if ((common & D13X_ADVERTISE_100FULL) != 0)
            {
              speed100 = true;
              full_duplex = true;
            }
          else if ((common & D13X_ADVERTISE_100HALF) != 0)
            {
              speed100 = true;
              full_duplex = false;
            }
          else if ((common & D13X_ADVERTISE_10FULL) != 0)
            {
              speed100 = false;
              full_duplex = true;
            }
          else
            {
              speed100 = false;
              full_duplex = false;
            }
        }

      d13x_gmac_set_link(speed100, full_duplex);
    }

  if (link != priv->link)
    {
      priv->link = link;
      if (link)
        {
          netdev_carrier_on(&priv->dev);
          syslog(LOG_INFO, "[D13X] GMAC0 link up %uM %s duplex\n",
                 speed100 ? 100 : 10, full_duplex ? "full" : "half");
          devif_poll(&priv->dev, d13x_gmac_txpoll);
        }
      else
        {
          netdev_carrier_off(&priv->dev);
          syslog(LOG_WARNING, "[D13X] GMAC0 link down\n");
        }
    }
}

static void d13x_gmac_link_work(FAR void *arg)
{
  FAR struct d13x_gmac_s *priv = arg;

  if (priv->ifup)
    {
      net_lock();
      d13x_gmac_update_link(priv);
      net_unlock();
      wd_start(&priv->linktimer, D13X_GMAC_LINK_POLL,
               d13x_gmac_link_timer, (wdparm_t)priv);
    }
}

static void d13x_gmac_link_timer(wdparm_t arg)
{
  FAR struct d13x_gmac_s *priv = (FAR struct d13x_gmac_s *)arg;

  if (work_available(&priv->linkwork))
    {
      work_queue(LPWORK, &priv->linkwork, d13x_gmac_link_work, priv, 0);
    }
}

static void d13x_gmac_desc_initialize(FAR struct d13x_gmac_s *priv)
{
  unsigned int i;

  memset(g_d13x_gmac_rxdesc, 0, sizeof(g_d13x_gmac_rxdesc));
  memset(g_d13x_gmac_txdesc, 0, sizeof(g_d13x_gmac_txdesc));

  for (i = 0; i < D13X_GMAC_RX_COUNT; i++)
    {
      g_d13x_gmac_rxdesc[i].size = D13X_GMAC_RXDESC_CHAINED |
                                   D13X_GMAC_BUFFER_SIZE;
      g_d13x_gmac_rxdesc[i].buffer =
        (uintptr_t)g_d13x_gmac_rxbuffer[i];
      g_d13x_gmac_rxdesc[i].next =
        (uintptr_t)&g_d13x_gmac_rxdesc[(i + 1) % D13X_GMAC_RX_COUNT];
      g_d13x_gmac_rxdesc[i].status = D13X_GMAC_DESC_OWN;
    }

  for (i = 0; i < D13X_GMAC_TX_COUNT; i++)
    {
      g_d13x_gmac_txdesc[i].status = D13X_GMAC_TXDESC_CHAINED;
      g_d13x_gmac_txdesc[i].buffer =
        (uintptr_t)g_d13x_gmac_txbuffer[i];
      g_d13x_gmac_txdesc[i].next =
        (uintptr_t)&g_d13x_gmac_txdesc[(i + 1) % D13X_GMAC_TX_COUNT];
    }

  priv->rxhead = 0;
  priv->txhead = 0;
  priv->txtail = 0;
  priv->txpending = 0;

  d13x_gmac_cache_clean_invalidate((uintptr_t)g_d13x_gmac_rxdesc,
                                   sizeof(g_d13x_gmac_rxdesc));
  d13x_gmac_cache_clean_invalidate((uintptr_t)g_d13x_gmac_txdesc,
                                   sizeof(g_d13x_gmac_txdesc));
  d13x_gmac_cache_clean_invalidate((uintptr_t)g_d13x_gmac_rxbuffer,
                                   sizeof(g_d13x_gmac_rxbuffer));
  d13x_gmac_cache_clean_invalidate((uintptr_t)g_d13x_gmac_txbuffer,
                                   sizeof(g_d13x_gmac_txbuffer));
}

static int d13x_gmac_transmit(FAR struct d13x_gmac_s *priv)
{
  FAR struct d13x_gmac_desc_s *desc;
  unsigned int index;

  if (priv->dev.d_len == 0 || priv->dev.d_len > D13X_GMAC_BUFFER_SIZE ||
      priv->txpending >= D13X_GMAC_TX_COUNT)
    {
      return -EBUSY;
    }

  index = priv->txhead;
  desc = &g_d13x_gmac_txdesc[index];
  d13x_gmac_cache_invalidate((uintptr_t)desc, sizeof(*desc));
  if ((desc->status & D13X_GMAC_DESC_OWN) != 0)
    {
      return -EBUSY;
    }

  memcpy(g_d13x_gmac_txbuffer[index], priv->dev.d_buf, priv->dev.d_len);
  desc->size = priv->dev.d_len;
  desc->status = D13X_GMAC_TXDESC_CHAINED | D13X_GMAC_TXDESC_FIRST |
                 D13X_GMAC_TXDESC_LAST | D13X_GMAC_TXDESC_INTERRUPT |
                 D13X_GMAC_DESC_OWN;

  d13x_gmac_cache_clean_invalidate((uintptr_t)g_d13x_gmac_txbuffer[index],
                                   priv->dev.d_len);
  d13x_gmac_cache_clean_invalidate((uintptr_t)desc, sizeof(*desc));

  priv->txhead = (index + 1) % D13X_GMAC_TX_COUNT;
  priv->txpending++;
  NETDEV_TXPACKETS(priv->dev);

  putreg32(getreg32(D13X_GMAC_REG(D13X_GMAC_TXDMA0CTL)) |
           D13X_GMAC_TXDMA_POLL, D13X_GMAC_REG(D13X_GMAC_TXDMA0CTL));
  priv->dev.d_len = 0;
  return OK;
}

static int d13x_gmac_txpoll(FAR struct net_driver_s *dev)
{
  FAR struct d13x_gmac_s *priv = dev->d_private;

  if (priv->txpending >= D13X_GMAC_TX_COUNT)
    {
      return 1;
    }

  return d13x_gmac_transmit(priv) < 0 ? 1 : 0;
}

static void d13x_gmac_reply(FAR struct d13x_gmac_s *priv)
{
  if (priv->dev.d_len > 0)
    {
      d13x_gmac_transmit(priv);
    }
}

static void d13x_gmac_receive(FAR struct d13x_gmac_s *priv)
{
  FAR struct d13x_gmac_desc_s *desc;
  uint32_t status;
  unsigned int length;
  unsigned int processed = 0;

  while (processed < D13X_GMAC_RX_COUNT)
    {
      desc = &g_d13x_gmac_rxdesc[priv->rxhead];
      d13x_gmac_cache_invalidate((uintptr_t)desc, sizeof(*desc));
      status = desc->status;
      if ((status & D13X_GMAC_DESC_OWN) != 0)
        {
          break;
        }

      length = (status & D13X_GMAC_RXDESC_LENGTH_MASK) >>
               D13X_GMAC_RXDESC_LENGTH_SHIFT;
      if (length >= 4)
        {
          length -= 4;
        }

      if ((status & (D13X_GMAC_RXDESC_ERROR | D13X_GMAC_RXDESC_FIRST |
                     D13X_GMAC_RXDESC_LAST)) ==
          (D13X_GMAC_RXDESC_FIRST | D13X_GMAC_RXDESC_LAST) &&
          length <= MAX_NETDEV_PKTSIZE)
        {
          d13x_gmac_cache_invalidate(
            (uintptr_t)g_d13x_gmac_rxbuffer[priv->rxhead], length);
          memcpy(priv->dev.d_buf, g_d13x_gmac_rxbuffer[priv->rxhead],
                 length);
          priv->dev.d_len = length;
          NETDEV_RXPACKETS(priv->dev);

#ifdef CONFIG_NET_PKT
          pkt_input(&priv->dev);
#endif

#ifdef CONFIG_NET_IPv4
          if (BUF->type == HTONS(ETHTYPE_IP))
            {
              NETDEV_RXIPV4(&priv->dev);
              ipv4_input(&priv->dev);
              d13x_gmac_reply(priv);
            }
          else
#endif
#ifdef CONFIG_NET_IPv6
          if (BUF->type == HTONS(ETHTYPE_IP6))
            {
              NETDEV_RXIPV6(&priv->dev);
              ipv6_input(&priv->dev);
              d13x_gmac_reply(priv);
            }
          else
#endif
#ifdef CONFIG_NET_ARP
          if (BUF->type == HTONS(ETHTYPE_ARP))
            {
              arp_input(&priv->dev);
              NETDEV_RXARP(&priv->dev);
              d13x_gmac_reply(priv);
            }
          else
#endif
            {
              NETDEV_RXDROPPED(&priv->dev);
            }
        }
      else
        {
          NETDEV_RXERRORS(priv->dev);
        }

      desc->status = D13X_GMAC_DESC_OWN;
      d13x_gmac_cache_clean_invalidate((uintptr_t)desc, sizeof(*desc));
      priv->rxhead = (priv->rxhead + 1) % D13X_GMAC_RX_COUNT;
      processed++;
    }

  putreg32(getreg32(D13X_GMAC_REG(D13X_GMAC_RXDMA0CTL)) |
           D13X_GMAC_RXDMA_POLL, D13X_GMAC_REG(D13X_GMAC_RXDMA0CTL));
}

static void d13x_gmac_txdone(FAR struct d13x_gmac_s *priv)
{
  FAR struct d13x_gmac_desc_s *desc;

  while (priv->txpending > 0)
    {
      desc = &g_d13x_gmac_txdesc[priv->txtail];
      d13x_gmac_cache_invalidate((uintptr_t)desc, sizeof(*desc));
      if ((desc->status & D13X_GMAC_DESC_OWN) != 0)
        {
          break;
        }

      NETDEV_TXDONE(priv->dev);
      priv->txtail = (priv->txtail + 1) % D13X_GMAC_TX_COUNT;
      priv->txpending--;
    }

  if (priv->txpending < D13X_GMAC_TX_COUNT)
    {
      devif_poll(&priv->dev, d13x_gmac_txpoll);
    }
}

static void d13x_gmac_interrupt_work(FAR void *arg)
{
  FAR struct d13x_gmac_s *priv = arg;
  uint32_t status;

  net_lock();
  status = getreg32(D13X_GMAC_REG(D13X_GMAC_DMA0INTSTS));
  putreg32(status, D13X_GMAC_REG(D13X_GMAC_DMA0INTSTS));

  if ((status & D13X_GMAC_DMAINT_FATAL) != 0)
    {
      syslog(LOG_ERR, "[D13X] GMAC0 fatal DMA status=%08lx\n",
             (unsigned long)status);
      NETDEV_RXERRORS(priv->dev);
    }

  if ((status & (D13X_GMAC_DMAINT_RX |
                 D13X_GMAC_DMAINT_RX_UNAVAIL)) != 0)
    {
      d13x_gmac_receive(priv);
    }

  if ((status & D13X_GMAC_DMAINT_TX) != 0)
    {
      d13x_gmac_txdone(priv);
    }

  net_unlock();
  up_enable_irq(D13X_IRQ_GMAC0);
}

static int d13x_gmac_interrupt(int irq, FAR void *context, FAR void *arg)
{
  FAR struct d13x_gmac_s *priv = arg;

  (void)irq;
  (void)context;

  up_disable_irq(D13X_IRQ_GMAC0);
  if (work_available(&priv->irqwork))
    {
      work_queue(LPWORK, &priv->irqwork, d13x_gmac_interrupt_work, priv, 0);
    }
  else
    {
      up_enable_irq(D13X_IRQ_GMAC0);
    }

  return OK;
}

static int d13x_gmac_ifup(FAR struct net_driver_s *dev)
{
  FAR struct d13x_gmac_s *priv = dev->d_private;
  unsigned int timeout;

  d13x_gmac_desc_initialize(priv);
  putreg32((uintptr_t)g_d13x_gmac_txdesc,
           D13X_GMAC_REG(D13X_GMAC_TXDESCSTART));
  putreg32((uintptr_t)g_d13x_gmac_rxdesc,
           D13X_GMAC_REG(D13X_GMAC_RXDESCSTART));
  putreg32(D13X_GMAC_DMAINT_MASK,
           D13X_GMAC_REG(D13X_GMAC_DMA0INTEN));
  putreg32(getreg32(D13X_GMAC_REG(D13X_GMAC_MACTXFUNC)) |
           D13X_GMAC_MACTX_ENABLE,
           D13X_GMAC_REG(D13X_GMAC_MACTXFUNC));
  putreg32(getreg32(D13X_GMAC_REG(D13X_GMAC_MACRXFUNC)) |
           D13X_GMAC_MACRX_ENABLE,
           D13X_GMAC_REG(D13X_GMAC_MACRXFUNC));

  putreg32(D13X_GMAC_TXDMA_STORE_FWD | D13X_GMAC_TXDMA_FLUSH,
           D13X_GMAC_REG(D13X_GMAC_TXDMA0CTL));
  for (timeout = 0; timeout < D13X_GMAC_RESET_TIMEOUT_US; timeout++)
    {
      if ((getreg32(D13X_GMAC_REG(D13X_GMAC_TXDMA0CTL)) &
           D13X_GMAC_TXDMA_FLUSH) == 0)
        {
          break;
        }

      up_udelay(1);
    }

  putreg32(D13X_GMAC_TXDMA_STORE_FWD | D13X_GMAC_TXDMA_START,
           D13X_GMAC_REG(D13X_GMAC_TXDMA0CTL));
  putreg32(D13X_GMAC_RXDMA_STORE_FWD | D13X_GMAC_RXDMA_START,
           D13X_GMAC_REG(D13X_GMAC_RXDMA0CTL));

  priv->ifup = true;
  priv->link = false;
  netdev_carrier_off(dev);
  up_enable_irq(D13X_IRQ_GMAC0);
  syslog(LOG_INFO, "[D13X] GMAC0 eth0 up, waiting for carrier\n");

  for (timeout = 0; timeout < D13X_PHY_LINK_WAIT_LOOPS && !priv->link;
       timeout++)
    {
      d13x_gmac_update_link(priv);
      if (!priv->link)
        {
          up_mdelay(D13X_PHY_LINK_WAIT_MS);
        }
    }

  wd_start(&priv->linktimer, D13X_GMAC_LINK_POLL, d13x_gmac_link_timer,
           (wdparm_t)priv);
  return OK;
}

static int d13x_gmac_ifdown(FAR struct net_driver_s *dev)
{
  FAR struct d13x_gmac_s *priv = dev->d_private;

  up_disable_irq(D13X_IRQ_GMAC0);
  wd_cancel(&priv->linktimer);
  putreg32(0, D13X_GMAC_REG(D13X_GMAC_DMA0INTEN));
  d13x_gmac_modifyreg32(D13X_GMAC_REG(D13X_GMAC_MACTXFUNC),
                        D13X_GMAC_MACTX_ENABLE, 0);
  d13x_gmac_modifyreg32(D13X_GMAC_REG(D13X_GMAC_MACRXFUNC),
                        D13X_GMAC_MACRX_ENABLE, 0);
  d13x_gmac_modifyreg32(D13X_GMAC_REG(D13X_GMAC_TXDMA0CTL),
                        D13X_GMAC_TXDMA_START, 0);
  d13x_gmac_modifyreg32(D13X_GMAC_REG(D13X_GMAC_RXDMA0CTL),
                        D13X_GMAC_RXDMA_START, 0);

  priv->ifup = false;
  priv->link = false;
  netdev_carrier_off(dev);
  return OK;
}

static void d13x_gmac_txavail_work(FAR void *arg)
{
  FAR struct d13x_gmac_s *priv = arg;

  net_lock();
  if (priv->ifup && priv->link && priv->txpending < D13X_GMAC_TX_COUNT)
    {
      devif_poll(&priv->dev, d13x_gmac_txpoll);
    }

  net_unlock();
}

static int d13x_gmac_txavail(FAR struct net_driver_s *dev)
{
  FAR struct d13x_gmac_s *priv = dev->d_private;

  if (work_available(&priv->pollwork))
    {
      work_queue(LPWORK, &priv->pollwork, d13x_gmac_txavail_work, priv, 0);
    }

  return OK;
}

static int d13x_gmac_hardware_initialize(FAR struct d13x_gmac_s *priv)
{
  uint16_t bmcr;
  unsigned int timeout;
  int ret;

  d13x_gmac_clock_enable();
  d13x_gmac_pinmux();
  up_mdelay(1);

  ret = d13x_gmac_reset_controller();
  if (ret < 0)
    {
      return ret;
    }

  d13x_gmac_phy_hard_reset();
  putreg32(D13X_MDIO_CLOCK_DIV102, D13X_GMAC_REG(D13X_GMAC_MDIOCTL));

  ret = d13x_gmac_find_phy(priv);
  if (ret < 0)
    {
      return ret;
    }

  ret = d13x_gmac_reset_phy(priv->phyaddr);
  if (ret < 0)
    {
      return ret;
    }

  ret = d13x_mdio_write(priv->phyaddr, D13X_MII_BMCR,
                        D13X_BMCR_ANENABLE | D13X_BMCR_ANRESTART);
  if (ret < 0)
    {
      return ret;
    }

  for (timeout = 0; timeout < D13X_PHY_RESET_TIMEOUT_MS; timeout++)
    {
      if (d13x_mdio_read(priv->phyaddr, D13X_MII_BMCR, &bmcr) == OK &&
          (bmcr & D13X_BMCR_RESET) == 0)
        {
          break;
        }

      up_mdelay(1);
    }

  d13x_gmac_set_link(true, true);
  putreg32(D13X_GMAC_DMA0CONF_AAL | D13X_GMAC_DMA0CONF_USP |
           D13X_GMAC_DMA0CONF_RXPBL_8 | D13X_GMAC_DMA0CONF_FB |
           D13X_GMAC_DMA0CONF_PBLX8 | D13X_GMAC_DMA0CONF_PBL_8 |
           D13X_GMAC_DMA0CONF_ATDS | D13X_GMAC_DMA0CONF_PR_3_1,
           D13X_GMAC_REG(D13X_GMAC_DMA0CONF));
  putreg32(0, D13X_GMAC_REG(D13X_GMAC_MACFRMFLT));
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int d13x_gmac_initialize(void)
{
  static const uint8_t mac[6] =
  {
    0x02, 0x13, 0x58, 0x88, 0x00, 0x01
  };

  FAR struct d13x_gmac_s *priv = &g_d13x_gmac;
  uint32_t low;
  uint32_t high;
  int ret;

  memset(priv, 0, sizeof(*priv));

  ret = d13x_gmac_hardware_initialize(priv);
  if (ret < 0)
    {
      syslog(LOG_ERR, "[D13X] GMAC0 hardware init failed: %d version=%08lx\n",
             ret,
             (unsigned long)getreg32(D13X_GMAC_REG(D13X_GMAC_VERSION)));
      return ret;
    }

  memcpy(priv->dev.d_mac.ether.ether_addr_octet, mac, sizeof(mac));
  low = mac[0] | ((uint32_t)mac[1] << 8) | ((uint32_t)mac[2] << 16) |
        ((uint32_t)mac[3] << 24);
  high = mac[4] | ((uint32_t)mac[5] << 8);
  putreg32(low, D13X_GMAC_REG(D13X_GMAC_MACADDR0LOW));
  putreg32(high, D13X_GMAC_REG(D13X_GMAC_MACADDR0HIGH));

  priv->dev.d_buf = g_d13x_gmac_netbuffer;
  priv->dev.d_ifup = d13x_gmac_ifup;
  priv->dev.d_ifdown = d13x_gmac_ifdown;
  priv->dev.d_txavail = d13x_gmac_txavail;
  priv->dev.d_private = priv;

  ret = irq_attach(D13X_IRQ_GMAC0, d13x_gmac_interrupt, priv);
  if (ret < 0)
    {
      syslog(LOG_ERR, "[D13X] GMAC0 IRQ attach failed: %d\n", ret);
      return ret;
    }

  up_disable_irq(D13X_IRQ_GMAC0);
  ret = netdev_register(&priv->dev, NET_LL_ETHERNET);
  if (ret == OK)
    {
      syslog(LOG_INFO,
             "[D13X] GMAC0 registered eth0 version=%08lx "
             "mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
             (unsigned long)getreg32(D13X_GMAC_REG(D13X_GMAC_VERSION)),
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

  return ret;
}
