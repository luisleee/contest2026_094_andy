/****************************************************************************
 * contest2026_094_andy/app/wifi_test/wifi_test_main.c
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <netinet/arp.h>
#include <netinet/in.h>
#include <sched.h>

#include <nuttx/kthread.h>
#include <nuttx/mutex.h>
#include <nuttx/net/ethernet.h>
#include <nuttx/net/ip.h>
#include <nuttx/net/netdev.h>
#include <nuttx/net/pkt.h>
#include <nuttx/sdio.h>
#include <nuttx/wireless/wireless.h>

#include <arch/chip/d13x_sdmc.h>

#include "fmacfw_8800d80_u02.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#define WIFI_TEST_CCCR_DUMP_LEN       0x18
#define WIFI_TEST_FBR_DUMP_LEN        0x10
#define WIFI_TEST_CIS_PTR_LEN         3
#define WIFI_TEST_CIS_MAX_BYTES       256
#define WIFI_TEST_CIS_MAX_TUPLE_DATA  64
#define WIFI_TEST_AIC_BLOCK_SIZE      512
#define WIFI_TEST_AIC_F0_VENDOR_INIT  0x0f2
#define WIFI_TEST_AIC_V3_FLOW_CTRL_Q1 0x003
#define WIFI_TEST_AIC_V3_MISC_STATUS  0x004
#define WIFI_TEST_AIC_V3_BYTEMODE_LEN 0x005
#define WIFI_TEST_AIC_V3_BYTEMODE_REG 0x007
#define WIFI_TEST_AIC_V3_INTR_PENDING 0x001
#define WIFI_TEST_AIC_V3_RD_FIFO_REG  0x00f
#define WIFI_TEST_AIC_V3_WR_FIFO_REG  0x010
#define WIFI_TEST_AIC_DATA_FW2SOC     0x00
#define WIFI_TEST_AIC_DATA_SOC2FW     0x01
#define WIFI_TEST_AIC_CFG_TYPE        0x10
#define WIFI_TEST_AIC_MSG_TYPE        0x11
#define WIFI_TEST_AIC_CFG_DATA_CFM    0x12
#define WIFI_TEST_AIC_TX_MAX_SIZE     512
#define WIFI_TEST_AIC_RX_MAX_SIZE     8192
#define WIFI_TEST_AIC_TX_HDR_LEN      4
#define WIFI_TEST_AIC_TX_DUMMY_LEN    4
#define WIFI_TEST_AIC_LMAC_HDR_LEN    8
#define WIFI_TEST_AIC_E2A_HDR_LEN     12
#define WIFI_TEST_ENABLE_RETRIES      100
#define WIFI_TEST_FLOW_RETRIES        50
#define WIFI_TEST_MSG_POLL_RETRIES    8
#define WIFI_TEST_LONG_MSG_POLL_RETRIES 100
#define WIFI_TEST_CONNECT_POLL_COUNT    200
#define WIFI_TEST_TASK_MM             0
#define WIFI_TEST_TASK_DBG            1
#define WIFI_TEST_TASK_SCANU          4
#define WIFI_TEST_TASK_ME             5
#define WIFI_TEST_TASK_SM             6
#define WIFI_TEST_DRV_TASK_ID         100
#define WIFI_TEST_MM_RESET_REQ        0
#define WIFI_TEST_MM_RESET_CFM        1
#define WIFI_TEST_MM_START_REQ        2
#define WIFI_TEST_MM_START_CFM        3
#define WIFI_TEST_MM_VERSION_REQ      4
#define WIFI_TEST_MM_VERSION_CFM      5
#define WIFI_TEST_MM_ADD_IF_REQ       6
#define WIFI_TEST_MM_ADD_IF_CFM       7
#define WIFI_TEST_MM_SET_RF_CALIB_REQ 105
#define WIFI_TEST_MM_SET_RF_CALIB_CFM 106
#define WIFI_TEST_MM_SET_STACK_START_REQ 123
#define WIFI_TEST_MM_SET_STACK_START_CFM 124
#define WIFI_TEST_SCANU_START_REQ     (WIFI_TEST_TASK_SCANU << 10)
#define WIFI_TEST_SCANU_START_CFM     (WIFI_TEST_SCANU_START_REQ + 1)
#define WIFI_TEST_SCANU_RESULT_IND    (WIFI_TEST_SCANU_START_REQ + 4)
#define WIFI_TEST_SCANU_START_CFM_ADDITIONAL \
  (WIFI_TEST_SCANU_START_REQ + 9)
#define WIFI_TEST_ME_CONFIG_REQ       (WIFI_TEST_TASK_ME << 10)
#define WIFI_TEST_ME_CONFIG_CFM       (WIFI_TEST_ME_CONFIG_REQ + 1)
#define WIFI_TEST_ME_CHAN_CONFIG_REQ  (WIFI_TEST_ME_CONFIG_REQ + 2)
#define WIFI_TEST_ME_CHAN_CONFIG_CFM  (WIFI_TEST_ME_CONFIG_REQ + 3)
#define WIFI_TEST_ME_CONFIG_LEN       112
#define WIFI_TEST_ME_CHAN_CONFIG_LEN  254
#define WIFI_TEST_MM_ADD_IF_LEN       10
#define WIFI_TEST_SM_CONNECT_REQ      (WIFI_TEST_TASK_SM << 10)
#define WIFI_TEST_SM_CONNECT_CFM      (WIFI_TEST_SM_CONNECT_REQ + 1)
#define WIFI_TEST_SM_CONNECT_IND      (WIFI_TEST_SM_CONNECT_REQ + 2)
#define WIFI_TEST_SM_CONNECT_REQ_LEN  320
#define WIFI_TEST_SM_CONNECT_SSID_OFF 0
#define WIFI_TEST_SM_CONNECT_BSSID_OFF 34
#define WIFI_TEST_SM_CONNECT_CHAN_OFF 40
#define WIFI_TEST_SM_CONNECT_FLAGS_OFF 48
#define WIFI_TEST_SM_CONNECT_CTRL_PORT_OFF 52
#define WIFI_TEST_SM_CONNECT_IE_LEN_OFF 54
#define WIFI_TEST_SM_CONNECT_LISTEN_OFF 56
#define WIFI_TEST_SM_CONNECT_DONT_WAIT_OFF 58
#define WIFI_TEST_SM_CONNECT_AUTH_OFF 59
#define WIFI_TEST_SM_CONNECT_UAPSD_OFF 60
#define WIFI_TEST_SM_CONNECT_VIF_OFF 61
#define WIFI_TEST_SM_CONNECT_IE_BUF_OFF 64
#define WIFI_TEST_SM_CONNECT_CFM_LEN  1
#define WIFI_TEST_SM_CONNECT_IND_MIN_LEN 16
#define WIFI_TEST_SM_ASSOC_IE_LEN     800
#define WIFI_TEST_SM_CONNECT_IND_IE_OFF 20
#define WIFI_TEST_SM_CONNECT_IND_AID_OFF \
  (WIFI_TEST_SM_CONNECT_IND_IE_OFF + WIFI_TEST_SM_ASSOC_IE_LEN)
#define WIFI_TEST_SM_CONNECT_IND_BAND_OFF \
  (WIFI_TEST_SM_CONNECT_IND_AID_OFF + 2)
#define WIFI_TEST_SM_CONNECT_IND_FREQ_OFF \
  (WIFI_TEST_SM_CONNECT_IND_AID_OFF + 4)
#define WIFI_TEST_SM_CONNECT_IND_WIDTH_OFF \
  (WIFI_TEST_SM_CONNECT_IND_AID_OFF + 6)
#define WIFI_TEST_SM_CONNECT_IND_FREQ1_OFF \
  (WIFI_TEST_SM_CONNECT_IND_AID_OFF + 8)
#define WIFI_TEST_SM_CONNECT_IND_FREQ2_OFF \
  (WIFI_TEST_SM_CONNECT_IND_AID_OFF + 12)
#define WIFI_TEST_AUTH_OPEN           0
#define WIFI_TEST_OPEN_CTRL_PORT_ETHERTYPE 0x888e
#define WIFI_TEST_SCANU_CHAN_DEF_LEN  6
#define WIFI_TEST_SCANU_SSID_LEN      33
#define WIFI_TEST_SCANU_CHANNEL_MAX   42
#define WIFI_TEST_SCANU_SSID_MAX      3
#define WIFI_TEST_SCANU_BSSID_OFF     352
#define WIFI_TEST_SCANU_ADD_IES_OFF   360
#define WIFI_TEST_SCANU_ADD_IE_LEN_OFF 364
#define WIFI_TEST_SCANU_VIF_IDX_OFF   366
#define WIFI_TEST_SCANU_CHAN_CNT_OFF  367
#define WIFI_TEST_SCANU_SSID_CNT_OFF  368
#define WIFI_TEST_SCANU_NO_CCK_OFF    369
#define WIFI_TEST_SCANU_DURATION_OFF  372
#define WIFI_TEST_SCANU_START_LEN     376
#define WIFI_TEST_SCANU_RESULT_META_LEN 12
#define WIFI_TEST_80211_MGMT_BSSID_OFF 16
#define WIFI_TEST_80211_MGMT_IES_OFF  36
#define WIFI_TEST_MM_START_LEN        72
#define WIFI_TEST_DBG_MEM_READ_REQ    (WIFI_TEST_TASK_DBG << 10)
#define WIFI_TEST_DBG_MEM_READ_CFM    (WIFI_TEST_DBG_MEM_READ_REQ + 1)
#define WIFI_TEST_DBG_MEM_BLOCK_WRITE_REQ \
  (WIFI_TEST_DBG_MEM_READ_REQ + 11)
#define WIFI_TEST_DBG_MEM_BLOCK_WRITE_CFM \
  (WIFI_TEST_DBG_MEM_READ_REQ + 12)
#define WIFI_TEST_DBG_START_APP_REQ   (WIFI_TEST_DBG_MEM_READ_REQ + 13)
#define WIFI_TEST_DBG_START_APP_CFM   (WIFI_TEST_DBG_MEM_READ_REQ + 14)
#define WIFI_TEST_RAM_FMAC_FW_ADDR    0x00120000
#define WIFI_TEST_HOST_START_APP_AUTO 1
#define WIFI_TEST_FW_BLOCK_SIZE       480
#define WIFI_TEST_FW_PROGRESS_STEP    (16 * 1024)
#define WIFI_TEST_POST_STACK_DELAY_US (100 * 1000)
#define WIFI_TEST_SDIO_OTHER_INT      0x80
#define WIFI_TEST_NETBUF_SIZE         (CONFIG_NET_ETH_PKTSIZE + \
                                       CONFIG_NET_GUARDSIZE)
#define WIFI_TEST_RX_DESC_LEN         56
#define WIFI_TEST_80211_FCTL_TYPE_MASK 0x000c
#define WIFI_TEST_80211_FCTL_SUBT_MASK 0x00f0
#define WIFI_TEST_80211_FCTL_TODS     0x0100
#define WIFI_TEST_80211_FCTL_FROMDS   0x0200
#define WIFI_TEST_80211_FCTL_ORDER    0x8000
#define WIFI_TEST_80211_TYPE_MGMT     0x0000
#define WIFI_TEST_80211_TYPE_DATA     0x0008
#define WIFI_TEST_80211_SUBT_QOS      0x0080
#define WIFI_TEST_80211_SUBT_NODATA   0x0040
#define WIFI_TEST_80211_HDR_LEN       24
#define WIFI_TEST_80211_QOS_LEN       2
#define WIFI_TEST_80211_HTCTRL_LEN    4
#define WIFI_TEST_LLC_SNAP_LEN        8
#define WIFI_TEST_ETH_HDR_LEN         14
#define WIFI_TEST_ETHERTYPE_IP        0x0800
#define WIFI_TEST_ETHERTYPE_ARP       0x0806
#define WIFI_TEST_ETHERTYPE_EAPOL     0x888e
#define WIFI_TEST_TX_HOSTDESC_LEN     28
#define WIFI_TEST_DATA_TX_MAX_SIZE    2048
#define WIFI_TEST_DATA_FLOW_THRESH    2
#define WIFI_TEST_INVALID_STA_IDX     0xff
#define WIFI_TEST_RX_THREAD_STACK     3072
#define WIFI_TEST_RX_THREAD_SLEEP_US  20000
#define WIFI_TEST_SSID_MAX_LEN        IW_ESSID_MAX_SIZE

#define WIFI_TEST_ETHBUF(dev) \
  ((FAR struct eth_hdr_s *)((dev)->d_buf))

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct wifi_test_netdev_s
{
  struct net_driver_s dev;
  bool registered;
  bool ifup;
  bool connected;
  bool vif_added;
  unsigned int txavail_count;
  unsigned int tx_count;
  pid_t rx_pid;
  bool rx_stop;
  bool rx_running;
  bool want_connect;
  bool bssid_set;
  bool freq_set;
  uint8_t vif_idx;
  uint8_t ap_idx;
  uint16_t freq;
  uint32_t mode;
  char ssid[WIFI_TEST_SSID_MAX_LEN + 1];
  uint8_t bssid[6];
};

struct wifi_test_rx_msg_s
{
  FAR const uint8_t *base;
  FAR const uint8_t *param;
  unsigned int offset;
  unsigned int sdio_len;
  unsigned int total_len;
  uint16_t msg_id;
  uint16_t dest_id;
  uint16_t src_id;
  uint16_t param_len;
  uint32_t pattern;
  uint8_t type;
  uint8_t crc;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct wifi_test_netdev_s g_wifi_test_netdev;
static mutex_t g_wifi_test_sdio_lock = NXMUTEX_INITIALIZER;
static uint8_t g_wifi_test_rxbuf[WIFI_TEST_AIC_RX_MAX_SIZE]
  __attribute__((aligned(4)));
static uint8_t g_wifi_test_rxthread_buf[WIFI_TEST_AIC_RX_MAX_SIZE]
  __attribute__((aligned(4)));
static uint8_t g_wifi_test_netbuf[WIFI_TEST_NETBUF_SIZE]
  __attribute__((aligned(2)));

static const uint8_t g_wifi_test_mac[6] =
{
  0x02, 0xa1, 0xc8, 0x88, 0x00, 0x01
};

static bool g_wifi_test_quiet = true;
static bool g_wifi_test_fw_loaded;
static bool g_wifi_test_brought_up;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void wifi_test_put_chan_def(FAR uint8_t *buffer, uint16_t freq,
                                   uint8_t band, uint8_t flags,
                                   int8_t tx_power);
static void wifi_test_dump_rx_packet(FAR const uint8_t *rx,
                                     unsigned int rx_len);
static int wifi_test_wait_connect_ind(void);
static int wifi_test_connect_open(FAR const char *ssid,
                                  FAR const uint8_t *bssid,
                                  uint16_t freq);
static int wifi_test_auto_init(bool verbose);
static int wifi_test_rx_thread(int argc, FAR char *argv[]);
static int wifi_test_rx_thread_start(void);
static void wifi_test_rx_thread_stop(void);
static void wifi_test_dump_connect_ind(FAR const uint8_t *param,
                                       unsigned int param_len);
static void wifi_test_dump_data_frame(FAR const struct wifi_test_rx_msg_s *msg);
static int wifi_test_feed_rx_msg(FAR const struct wifi_test_rx_msg_s *msg,
                                 bool verbose);
static unsigned int wifi_test_feed_rx_packet(FAR const uint8_t *rx,
                                             unsigned int rx_len,
                                             bool verbose);
static int wifi_test_msg_recv_locked(FAR uint8_t *rx, size_t rx_size,
                                     FAR unsigned int *rx_len,
                                     bool verbose, int poll_retries);
static FAR const uint8_t *wifi_test_find_80211_frame(FAR const uint8_t *data,
                                                     unsigned int data_len,
                                                     FAR unsigned int *offset);
static bool wifi_test_80211_frame_valid(FAR const uint8_t *frame,
                                        unsigned int frame_len);
static bool wifi_test_rx_msg_parse(FAR const uint8_t *rx,
                                   unsigned int rx_len,
                                   unsigned int offset,
                                   FAR struct wifi_test_rx_msg_s *msg);
static unsigned int wifi_test_rx_padded_len(FAR const struct
                                            wifi_test_rx_msg_s *msg);
static int wifi_test_send_eth_frame(FAR const uint8_t *frame,
                                    unsigned int frame_len);

static FAR const char *wifi_test_tuple_name(uint8_t code)
{
  switch (code)
    {
      case 0x01:
        return "DEVICE";
      case 0x15:
        return "VERS_1";
      case 0x20:
        return "MANFID";
      case 0x21:
        return "FUNCID";
      case 0x22:
        return "FUNCE";
      default:
        return "UNKNOWN";
    }
}

static int wifi_test_readb(uint8_t function, uint32_t address,
                           FAR uint8_t *value)
{
  int ret;

  ret = d13x_sdmc0_wifi_readb(function, address, value);
  if (ret < 0)
    {
      printf("wifi_test: CMD52 fn=%u addr=0x%05lx failed: %s (%d)\n",
             function, (unsigned long)address, strerror(-ret), ret);
      return ret;
    }

  return 0;
}

static int wifi_test_writeb(uint8_t function, uint32_t address,
                            uint8_t value)
{
  int ret;

  ret = d13x_sdmc0_wifi_writeb(function, address, value);
  if (ret < 0)
    {
      printf("wifi_test: CMD52 write fn=%u addr=0x%05lx val=0x%02x "
             "failed: %s (%d)\n",
             function, (unsigned long)address, value, strerror(-ret), ret);
      return ret;
    }

  return 0;
}

static int wifi_test_read_cis_ptr(uint8_t function, FAR uint32_t *ptr)
{
  uint32_t base;
  uint8_t value[WIFI_TEST_CIS_PTR_LEN];
  int ret;
  int i;

  if (function == 0)
    {
      base = SDIO_CCCR_CCP;
    }
  else
    {
      base = ((uint32_t)function << SDIO_FBR_SHIFT) + 0x09;
    }

  for (i = 0; i < WIFI_TEST_CIS_PTR_LEN; i++)
    {
      ret = wifi_test_readb(0, base + i, &value[i]);
      if (ret < 0)
        {
          return ret;
        }
    }

  *ptr = (uint32_t)value[0] | ((uint32_t)value[1] << 8) |
         ((uint32_t)value[2] << 16);
  return 0;
}

static int wifi_test_dump_bytes(uint8_t function, uint32_t address,
                                unsigned int length)
{
  uint8_t value;
  int ret;
  unsigned int i;

  for (i = 0; i < length; i++)
    {
      if ((i % 16) == 0)
        {
          printf("  %05lx:", (unsigned long)(address + i));
        }

      ret = wifi_test_readb(function, address + i, &value);
      if (ret < 0)
        {
          return ret;
        }

      printf(" %02x", value);
      if ((i % 16) == 15 || i + 1 == length)
        {
          printf("\n");
        }
    }

  return 0;
}

static void wifi_test_hexdump(FAR const uint8_t *buffer, unsigned int length)
{
  unsigned int i;

  for (i = 0; i < length; i++)
    {
      if ((i % 16) == 0)
        {
          printf("  %04x:", i);
        }

      printf(" %02x", buffer[i]);
      if ((i % 16) == 15 || i + 1 == length)
        {
          printf("\n");
        }
    }
}

static uint8_t wifi_test_crc8(FAR const uint8_t *buffer, uint16_t length)
{
  uint8_t crc = 0;
  uint8_t mask;

  while (length-- > 0)
    {
      for (mask = 0x80; mask != 0; mask >>= 1)
        {
          if ((crc & 0x80) != 0)
            {
              crc <<= 1;
              crc ^= 0x07;
            }
          else
            {
              crc <<= 1;
            }

          if ((*buffer & mask) != 0)
            {
              crc ^= 0x07;
            }
        }

      buffer++;
    }

  return crc;
}

static void wifi_test_put_le16(FAR uint8_t *buffer, uint16_t value)
{
  buffer[0] = (uint8_t)(value & 0xff);
  buffer[1] = (uint8_t)(value >> 8);
}

static void wifi_test_put_le32(FAR uint8_t *buffer, uint32_t value)
{
  buffer[0] = (uint8_t)(value & 0xff);
  buffer[1] = (uint8_t)((value >> 8) & 0xff);
  buffer[2] = (uint8_t)((value >> 16) & 0xff);
  buffer[3] = (uint8_t)(value >> 24);
}

static uint16_t wifi_test_get_le16(FAR const uint8_t *buffer)
{
  return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
}

static uint32_t wifi_test_get_le32(FAR const uint8_t *buffer)
{
  return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) |
         ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 24);
}

static void wifi_test_usage(FAR const char *progname)
{
  printf("Usage:\n");
  printf("  %s probe\n", progname);
  printf("  %s cccr\n", progname);
  printf("  %s cis\n", progname);
  printf("  %s enable\n", progname);
  printf("  %s cmd53\n", progname);
  printf("  %s msg\n", progname);
  printf("  %s memtest\n", progname);
  printf("  %s fwload\n", progname);
  printf("  %s fwstate\n", progname);
  printf("  %s version\n", progname);
  printf("  %s initcmd\n", progname);
  printf("  %s stackstart\n", progname);
  printf("  %s rfcalib\n", progname);
  printf("  %s rfchain\n", progname);
  printf("  %s mebasic\n", progname);
  printf("  %s macstart\n", progname);
  printf("  %s bringup\n", progname);
  printf("  %s addif\n", progname);
  printf("  %s scan [1|6|11]\n", progname);
  printf("  %s scanpoll [1|6|11] [count]\n", progname);
  printf("  %s joinportal\n", progname);
  printf("  %s joinportalbssid\n", progname);
  printf("  %s connect <ssid> <bssid> <freq>\n", progname);
  printf("  %s netreg\n", progname);
  printf("  %s rxpoll [count]\n", progname);
  printf("  %s rxfeed [count]\n", progname);
  printf("  %s power on|off\n", progname);
}

static int wifi_test_net_ifup(FAR struct net_driver_s *dev)
{
  FAR struct wifi_test_netdev_s *priv = dev->d_private;

  priv->ifup = true;
  if (priv->connected)
    {
      netdev_carrier_on(dev);
    }

  return 0;
}

static int wifi_test_net_ifdown(FAR struct net_driver_s *dev)
{
  FAR struct wifi_test_netdev_s *priv = dev->d_private;

  wifi_test_rx_thread_stop();
  priv->ifup = false;
  priv->connected = false;
  netdev_carrier_off(dev);
  return 0;
}

static bool wifi_test_mac_is_bcast(FAR const uint8_t *mac)
{
  int i;

  for (i = 0; i < 6; i++)
    {
      if (mac[i] != 0xff)
        {
          return false;
        }
    }

  return true;
}

static bool wifi_test_mac_is_zero(FAR const uint8_t *mac)
{
  int i;

  for (i = 0; i < 6; i++)
    {
      if (mac[i] != 0)
        {
          return false;
        }
    }

  return true;
}

static uint16_t wifi_test_iwfreq_to_mhz(FAR const struct iw_freq *freq)
{
  int32_t m;
  int e;

  if (freq == NULL || freq->flags == IW_FREQ_AUTO || freq->m == 0)
    {
      return 0xffff;
    }

  m = freq->m;
  e = freq->e;
  while (e > 6)
    {
      m /= 10;
      e--;
    }

  while (e < 6)
    {
      m *= 10;
      e++;
    }

  return (uint16_t)m;
}

static void wifi_test_mhz_to_iwfreq(uint16_t mhz, FAR struct iw_freq *freq)
{
  memset(freq, 0, sizeof(*freq));
  freq->m = mhz;
  freq->e = 6;
  freq->flags = mhz == 0xffff ? IW_FREQ_AUTO : IW_FREQ_FIXED;
}

static int wifi_test_ioctl_connect(FAR struct wifi_test_netdev_s *priv)
{
  static const uint8_t any_bssid[6] =
  {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff
  };
  FAR const uint8_t *bssid;

  if (priv->ssid[0] == '\0')
    {
      return 0;
    }

  bssid = priv->bssid_set && !wifi_test_mac_is_zero(priv->bssid) ?
          priv->bssid : any_bssid;

  return wifi_test_connect_open(priv->ssid, bssid,
                                priv->freq_set ? priv->freq : 0xffff);
}

static int wifi_test_net_ioctl(FAR struct net_driver_s *dev, int cmd,
                               unsigned long arg)
{
  FAR struct wifi_test_netdev_s *priv = dev->d_private;
  FAR struct iwreq *iwr = (FAR struct iwreq *)(uintptr_t)arg;
  size_t len;
  int ret = 0;

  if (iwr == NULL)
    {
      return -EINVAL;
    }

  switch (cmd)
    {
      case SIOCSIWMODE:
        priv->mode = iwr->u.mode;
        if (priv->mode != IW_MODE_INFRA && priv->mode != IW_MODE_AUTO)
          {
            printf("wifi_test: only managed/open mode is supported now\n");
            return -EOPNOTSUPP;
          }

        return 0;

      case SIOCGIWMODE:
        iwr->u.mode = priv->mode == 0 ? IW_MODE_INFRA : priv->mode;
        return 0;

      case SIOCSIWFREQ:
        priv->freq = wifi_test_iwfreq_to_mhz(&iwr->u.freq);
        priv->freq_set = priv->freq != 0xffff;
        return 0;

      case SIOCGIWFREQ:
        wifi_test_mhz_to_iwfreq(priv->freq_set ? priv->freq : 0xffff,
                                &iwr->u.freq);
        return 0;

      case SIOCSIWAP:
        memcpy(priv->bssid, iwr->u.ap_addr.sa_data, 6);
        priv->bssid_set = !wifi_test_mac_is_zero(priv->bssid);
        if (!wifi_test_mac_is_bcast(priv->bssid) && priv->ssid[0] != '\0')
          {
            ret = wifi_test_ioctl_connect(priv);
          }

        return ret;

      case SIOCGIWAP:
        iwr->u.ap_addr.sa_family = ARPHRD_ETHER;
        memcpy(iwr->u.ap_addr.sa_data, priv->bssid, 6);
        return 0;

      case SIOCSIWESSID:
        len = iwr->u.essid.length;
        if (len > WIFI_TEST_SSID_MAX_LEN)
          {
            return -EINVAL;
          }

        memset(priv->ssid, 0, sizeof(priv->ssid));
        if (len > 0 && iwr->u.essid.pointer != NULL)
          {
            memcpy(priv->ssid, iwr->u.essid.pointer, len);
          }

        priv->want_connect = iwr->u.essid.flags != IW_ESSID_OFF;
        if (!priv->want_connect)
          {
            return 0;
          }

        return wifi_test_ioctl_connect(priv);

      case SIOCGIWESSID:
        len = strlen(priv->ssid);
        if (iwr->u.essid.pointer != NULL && iwr->u.essid.length > 0)
          {
            if (len >= iwr->u.essid.length)
              {
                len = iwr->u.essid.length - 1;
              }

            memcpy(iwr->u.essid.pointer, priv->ssid, len);
            ((FAR char *)iwr->u.essid.pointer)[len] = '\0';
          }

        iwr->u.essid.length = len;
        iwr->u.essid.flags = priv->want_connect ? IW_ESSID_ON :
                                               IW_ESSID_OFF;
        return 0;

      case SIOCSIWSCAN:
        return 0;

      case SIOCGIWSCAN:
        iwr->u.data.length = 0;
        return 0;

      case SIOCGIWRANGE:
        if (iwr->u.data.pointer != NULL &&
            iwr->u.data.length >= sizeof(struct iw_range))
          {
            FAR struct iw_range *range = iwr->u.data.pointer;

            memset(range, 0, sizeof(*range));
            range->num_frequency = 3;
            range->freq[0].i = 1;
            range->freq[0].m = 2412;
            range->freq[0].e = 6;
            range->freq[1].i = 6;
            range->freq[1].m = 2437;
            range->freq[1].e = 6;
            range->freq[2].i = 11;
            range->freq[2].m = 2462;
            range->freq[2].e = 6;
            iwr->u.data.length = sizeof(*range);
          }

        return 0;

      case SIOCGIWENCODE:
        iwr->u.encoding.flags = IW_ENCODE_DISABLED;
        iwr->u.encoding.length = 0;
        return 0;

      default:
        return -ENOTTY;
    }
}

static int wifi_test_net_txpoll(FAR struct net_driver_s *dev)
{
  FAR struct wifi_test_netdev_s *priv = dev->d_private;
  int ret;

  if (dev->d_len == 0)
    {
      return 0;
    }

  if (!priv->connected)
    {
      printf("wifi_test: %s drop tx len=%u: not connected\n",
             dev->d_ifname, dev->d_len);
      dev->d_len = 0;
      return 0;
    }

  ret = wifi_test_send_eth_frame(dev->d_buf, dev->d_len);
  if (ret < 0)
    {
      printf("wifi_test: %s tx len=%u failed: %s (%d)\n",
             dev->d_ifname, dev->d_len, strerror(-ret), ret);
    }

  dev->d_len = 0;
  return ret < 0 ? ret : 0;
}

static int wifi_test_rx_thread(int argc, FAR char *argv[])
{
  FAR struct wifi_test_netdev_s *priv = &g_wifi_test_netdev;
  unsigned int rx_len;
  unsigned int fed;
  unsigned int err_count = 0;
  unsigned int rx_count = 0;
  int ret;

  UNUSED(argc);
  UNUSED(argv);

  priv->rx_running = true;
  while (!priv->rx_stop)
    {
      if (!priv->connected)
        {
          usleep(WIFI_TEST_RX_THREAD_SLEEP_US);
          continue;
        }

      ret = nxmutex_lock(&g_wifi_test_sdio_lock);
      if (ret < 0)
        {
          usleep(WIFI_TEST_RX_THREAD_SLEEP_US);
          continue;
        }

      rx_len = 0;
      memset(g_wifi_test_rxthread_buf, 0, sizeof(g_wifi_test_rxthread_buf));
      ret = wifi_test_msg_recv_locked(g_wifi_test_rxthread_buf,
                                      sizeof(g_wifi_test_rxthread_buf),
                                      &rx_len, false, 5);
      nxmutex_unlock(&g_wifi_test_sdio_lock);

      if (ret == -ETIMEDOUT)
        {
          usleep(WIFI_TEST_RX_THREAD_SLEEP_US);
          continue;
        }

      if (ret < 0)
        {
          err_count++;
          if (err_count <= 8 || (err_count % 32) == 0)
            {
              printf("wifi_test: rx thread read failed: %s (%d)\n",
                     strerror(-ret), ret);
            }

          usleep(WIFI_TEST_RX_THREAD_SLEEP_US);
          continue;
        }

      err_count = 0;
      rx_count++;
      fed = wifi_test_feed_rx_packet(g_wifi_test_rxthread_buf, rx_len,
                                     false);
      if (fed > 0 && !g_wifi_test_quiet)
        {
          printf("wifi_test: rx thread rx[%u] len=%u fed=%u\n",
                 rx_count, rx_len, fed);
        }
    }

  priv->rx_running = false;
  priv->rx_pid = -1;
  return 0;
}

static int wifi_test_rx_thread_start(void)
{
  FAR struct wifi_test_netdev_s *priv = &g_wifi_test_netdev;
  pid_t pid;

  if (priv->rx_running)
    {
      return 0;
    }

  priv->rx_stop = false;
  pid = kthread_create("wifi_rx", SCHED_PRIORITY_DEFAULT,
                       WIFI_TEST_RX_THREAD_STACK, wifi_test_rx_thread, NULL);
  if (pid < 0)
    {
      printf("wifi_test: rx thread start failed: %s (%d)\n",
             strerror(-pid), pid);
      return pid;
    }

  priv->rx_pid = pid;
  return 0;
}

static void wifi_test_rx_thread_stop(void)
{
  FAR struct wifi_test_netdev_s *priv = &g_wifi_test_netdev;
  int i;

  if (!priv->rx_running)
    {
      priv->rx_stop = true;
      priv->rx_pid = -1;
      return;
    }

  priv->rx_stop = true;
  for (i = 0; i < 50 && priv->rx_running; i++)
    {
      usleep(WIFI_TEST_RX_THREAD_SLEEP_US);
    }
}

static int wifi_test_net_txavail(FAR struct net_driver_s *dev)
{
  FAR struct wifi_test_netdev_s *priv = dev->d_private;

  priv->txavail_count++;

  devif_poll(dev, wifi_test_net_txpoll);
  return 0;
}

static int wifi_test_netreg(void)
{
  FAR struct wifi_test_netdev_s *priv = &g_wifi_test_netdev;
  int ret;

  if (priv->registered)
    {
      return 0;
    }

  memset(priv, 0, sizeof(*priv));
  memcpy(priv->dev.d_mac.ether.ether_addr_octet, g_wifi_test_mac,
         sizeof(g_wifi_test_mac));
  memset(&priv->dev.d_ipaddr, 0, sizeof(priv->dev.d_ipaddr));
  memset(&priv->dev.d_draddr, 0, sizeof(priv->dev.d_draddr));
  memset(&priv->dev.d_netmask, 0, sizeof(priv->dev.d_netmask));

  priv->dev.d_buf = g_wifi_test_netbuf;
  priv->dev.d_ifup = wifi_test_net_ifup;
  priv->dev.d_ifdown = wifi_test_net_ifdown;
  priv->dev.d_txavail = wifi_test_net_txavail;
  priv->dev.d_ioctl = wifi_test_net_ioctl;
  priv->dev.d_private = priv;
  priv->rx_pid = -1;
  priv->mode = IW_MODE_INFRA;
  priv->freq = 0xffff;
  memset(priv->bssid, 0xff, sizeof(priv->bssid));

  ret = netdev_register(&priv->dev, NET_LL_IEEE80211);
  if (ret < 0)
    {
      printf("wifi_test: wlan netdev register failed: %s (%d)\n",
             strerror(-ret), ret);
      printf("wifi_test: check CONFIG_DRIVERS_IEEE80211=y\n");
      return 1;
    }

  netdev_carrier_off(&priv->dev);
  priv->registered = true;
  if (!g_wifi_test_quiet)
    {
      printf("wifi_test: registered %s mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
             priv->dev.d_ifname,
             g_wifi_test_mac[0], g_wifi_test_mac[1], g_wifi_test_mac[2],
             g_wifi_test_mac[3], g_wifi_test_mac[4], g_wifi_test_mac[5]);
    }

  return 0;
}

static int wifi_test_set_blocksize(uint8_t function, uint16_t blocksize)
{
  uint32_t base;
  uint8_t lo;
  uint8_t hi;
  int ret;

  base = ((uint32_t)function << SDIO_FBR_SHIFT) +
         SDIO_CCCR_FN0_BLKSIZE_0;

  ret = wifi_test_writeb(0, base, (uint8_t)(blocksize & 0xff));
  if (ret < 0)
    {
      return ret;
    }

  ret = wifi_test_writeb(0, base + 1, (uint8_t)(blocksize >> 8));
  if (ret < 0)
    {
      return ret;
    }

  ret = wifi_test_readb(0, base, &lo);
  if (ret < 0)
    {
      return ret;
    }

  ret = wifi_test_readb(0, base + 1, &hi);
  if (ret < 0)
    {
      return ret;
    }

  if (!g_wifi_test_quiet)
    {
      printf("  fn%u block size regs=0x%02x 0x%02x (%u)\n",
             function, lo, hi, (unsigned int)(lo | ((uint16_t)hi << 8)));
    }

  return 0;
}

static int wifi_test_enable_function(uint8_t function)
{
  uint8_t ioen;
  uint8_t iordy;
  int ret;
  int i;

  ret = wifi_test_readb(0, SDIO_CCCR_IOEN, &ioen);
  if (ret < 0)
    {
      return ret;
    }

  ret = wifi_test_readb(0, SDIO_CCCR_IORDY, &iordy);
  if (ret < 0)
    {
      return ret;
    }

  if (!g_wifi_test_quiet)
    {
      printf("  before: IOEN=0x%02x IORDY=0x%02x\n", ioen, iordy);
    }

  ioen |= (uint8_t)(1 << function);
  ret = wifi_test_writeb(0, SDIO_CCCR_IOEN, ioen);
  if (ret < 0)
    {
      return ret;
    }

  for (i = 0; i < WIFI_TEST_ENABLE_RETRIES; i++)
    {
      usleep(10 * 1000);

      ret = wifi_test_readb(0, SDIO_CCCR_IORDY, &iordy);
      if (ret < 0)
        {
          return ret;
        }

      if ((iordy & (1 << function)) != 0)
        {
          break;
        }
    }

  ret = wifi_test_readb(0, SDIO_CCCR_IOEN, &ioen);
  if (ret < 0)
    {
      return ret;
    }

  if (!g_wifi_test_quiet)
    {
      printf("  after:  IOEN=0x%02x IORDY=0x%02x\n", ioen, iordy);
    }

  if ((iordy & (1 << function)) == 0)
    {
      printf("wifi_test: fn%u did not become ready\n", function);
      return -ETIMEDOUT;
    }

  return 0;
}

static int wifi_test_probe(void)
{
  struct d13x_sdmc0_wifi_probe_s result;
  int ret;

  ret = d13x_sdmc0_wifi_probe(&result);
  if (ret < 0)
    {
      printf("wifi_test: SDMC0 CMD5 probe failed: %s (%d)\n",
             strerror(-ret), ret);
      return 1;
    }

  printf("wifi_test: CMD5 OCR=0x%08lx ready=%s funcs=%u memory=%s rca=0x%04x\n",
         (unsigned long)result.ocr,
         result.ready ? "yes" : "no",
         result.function_count,
         result.memory_present ? "yes" : "no",
         result.rca);

  if (result.function_count == 0)
    {
      printf("wifi_test: no SDIO I/O functions reported\n");
      return 1;
    }

  return 0;
}

static int wifi_test_cccr(void)
{
  struct d13x_sdmc0_wifi_probe_s result;
  uint8_t value;
  uint32_t cisptr;
  int ret;
  int fn;

  ret = d13x_sdmc0_wifi_probe(&result);
  if (ret < 0)
    {
      printf("wifi_test: SDIO probe failed: %s (%d)\n", strerror(-ret), ret);
      return 1;
    }

  if (!result.ready || result.function_count == 0)
    {
      printf("wifi_test: SDIO not ready\n");
      return 1;
    }

  printf("wifi_test: SDIO ready funcs=%u rca=0x%04x\n",
         result.function_count, result.rca);

  printf("CCCR:\n");
  ret = wifi_test_dump_bytes(0, 0, WIFI_TEST_CCCR_DUMP_LEN);
  if (ret < 0)
    {
      return 1;
    }

  ret = wifi_test_read_cis_ptr(0, &cisptr);
  if (ret < 0)
    {
      return 1;
    }

  printf("  common CIS ptr=0x%05lx\n", (unsigned long)cisptr);

  ret = wifi_test_readb(0, SDIO_CCCR_REV, &value);
  if (ret < 0)
    {
      return 1;
    }

  printf("  cccr/sdio rev=0x%02x\n", value);

  ret = wifi_test_readb(0, SDIO_CCCR_CARD_CAP, &value);
  if (ret < 0)
    {
      return 1;
    }

  printf("  card cap=0x%02x\n", value);

  ret = wifi_test_readb(0, SDIO_CCCR_HIGHSPEED, &value);
  if (ret < 0)
    {
      return 1;
    }

  printf("  high speed=0x%02x\n", value);

  for (fn = 1; fn <= result.function_count; fn++)
    {
      printf("FBR%u:\n", fn);
      ret = wifi_test_dump_bytes(0, (uint32_t)fn << SDIO_FBR_SHIFT,
                                 WIFI_TEST_FBR_DUMP_LEN);
      if (ret < 0)
        {
          return 1;
        }

      ret = wifi_test_read_cis_ptr(fn, &cisptr);
      if (ret < 0)
        {
          return 1;
        }

      printf("  function %u CIS ptr=0x%05lx\n",
             fn, (unsigned long)cisptr);
    }

  return 0;
}

static int wifi_test_dump_cis(uint8_t function)
{
  uint8_t data[WIFI_TEST_CIS_MAX_TUPLE_DATA];
  uint32_t ptr;
  uint32_t offset;
  uint8_t code;
  uint8_t link;
  unsigned int total = 0;
  unsigned int i;
  int ret;

  ret = wifi_test_read_cis_ptr(function, &ptr);
  if (ret < 0)
    {
      return ret;
    }

  printf("CIS%u ptr=0x%05lx\n", function, (unsigned long)ptr);
  if (ptr == 0 || ptr == 0x0001fffful)
    {
      printf("  empty\n");
      return 0;
    }

  offset = ptr;
  while (total < WIFI_TEST_CIS_MAX_BYTES)
    {
      ret = wifi_test_readb(0, offset, &code);
      if (ret < 0)
        {
          return ret;
        }

      offset++;
      total++;

      if (code == 0xff)
        {
          printf("  %05lx: END\n", (unsigned long)(offset - 1));
          return 0;
        }

      if (code == 0x00)
        {
          printf("  %05lx: NULL\n", (unsigned long)(offset - 1));
          continue;
        }

      ret = wifi_test_readb(0, offset, &link);
      if (ret < 0)
        {
          return ret;
        }

      offset++;
      total++;

      printf("  %05lx: 0x%02x %-7s len=%u data:",
             (unsigned long)(offset - 2), code, wifi_test_tuple_name(code),
             link);

      for (i = 0; i < link && i < WIFI_TEST_CIS_MAX_TUPLE_DATA; i++)
        {
          ret = wifi_test_readb(0, offset + i, &data[i]);
          if (ret < 0)
            {
              return ret;
            }

          printf(" %02x", data[i]);
        }

      if (link > WIFI_TEST_CIS_MAX_TUPLE_DATA)
        {
          printf(" ...");
        }

      printf("\n");

      if (code == 0x20 && link >= 4)
        {
          printf("    manf=0x%04x card=0x%04x\n",
                 (uint16_t)(data[0] | ((uint16_t)data[1] << 8)),
                 (uint16_t)(data[2] | ((uint16_t)data[3] << 8)));
        }

      offset += link;
      total += link;
    }

  printf("  stopped after %u bytes\n", total);
  return 0;
}

static int wifi_test_cis(void)
{
  struct d13x_sdmc0_wifi_probe_s result;
  int ret;
  int fn;

  ret = d13x_sdmc0_wifi_probe(&result);
  if (ret < 0)
    {
      printf("wifi_test: SDIO probe failed: %s (%d)\n", strerror(-ret), ret);
      return 1;
    }

  if (!result.ready || result.function_count == 0)
    {
      printf("wifi_test: SDIO not ready\n");
      return 1;
    }

  printf("wifi_test: SDIO ready funcs=%u rca=0x%04x\n",
         result.function_count, result.rca);

  for (fn = 0; fn <= result.function_count; fn++)
    {
      ret = wifi_test_dump_cis((uint8_t)fn);
      if (ret < 0)
        {
          return 1;
        }
    }

  return 0;
}

static int wifi_test_enable(void)
{
  struct d13x_sdmc0_wifi_probe_s result;
  uint8_t value;
  int ret;

  ret = d13x_sdmc0_wifi_probe(&result);
  if (ret < 0)
    {
      printf("wifi_test: SDIO probe failed: %s (%d)\n", strerror(-ret), ret);
      return 1;
    }

  if (!result.ready || result.function_count < 1)
    {
      printf("wifi_test: SDIO fn1 not available\n");
      return 1;
    }

  if (!g_wifi_test_quiet)
    {
      printf("wifi_test: SDIO ready funcs=%u rca=0x%04x\n",
             result.function_count, result.rca);
      printf("wifi_test: init fn1 as AIC8800D80 SDIO\n");
    }

  ret = wifi_test_set_blocksize(1, WIFI_TEST_AIC_BLOCK_SIZE);
  if (ret < 0)
    {
      return 1;
    }

  ret = wifi_test_enable_function(1);
  if (ret < 0)
    {
      return 1;
    }

  ret = wifi_test_writeb(0, WIFI_TEST_AIC_F0_VENDOR_INIT, 0x7f);
  if (ret < 0)
    {
      return 1;
    }

  ret = wifi_test_readb(0, WIFI_TEST_AIC_F0_VENDOR_INIT, &value);
  if (ret < 0)
    {
      return 1;
    }

  if (!g_wifi_test_quiet)
    {
      printf("  F0[0x%03x]=0x%02x\n", WIFI_TEST_AIC_F0_VENDOR_INIT, value);
    }

  ret = wifi_test_writeb(1, WIFI_TEST_AIC_V3_BYTEMODE_REG, 0x01);
  if (ret < 0)
    {
      return 1;
    }

  ret = wifi_test_readb(1, WIFI_TEST_AIC_V3_BYTEMODE_REG, &value);
  if (ret < 0)
    {
      return 1;
    }

  if (!g_wifi_test_quiet)
    {
      printf("  F1[0x%02x]=0x%02x (bytemode disable)\n",
             WIFI_TEST_AIC_V3_BYTEMODE_REG, value);
      printf("wifi_test: fn1 init ok\n");
    }

  return 0;
}

static int wifi_test_cmd53(void)
{
  static uint8_t block[WIFI_TEST_AIC_BLOCK_SIZE];
  uint8_t flow = 0;
  int i;
  int ret;

  ret = wifi_test_enable();
  if (ret != 0)
    {
      return ret;
    }

  ret = wifi_test_readb(1, WIFI_TEST_AIC_V3_FLOW_CTRL_Q1, &flow);
  if (ret < 0)
    {
      return 1;
    }

  printf("  F1[0x03]=0x%02x (flow ctrl q1)\n", flow);

  for (i = 0; i < WIFI_TEST_AIC_BLOCK_SIZE; i++)
    {
      block[i] = 0;
    }

  ret = d13x_sdmc0_wifi_write(1, WIFI_TEST_AIC_V3_WR_FIFO_REG, false,
                              block, sizeof(block));
  if (ret < 0)
    {
      printf("wifi_test: CMD53 block write fn1 fifo addr=0x%05x failed: "
             "%s (%d)\n", WIFI_TEST_AIC_V3_WR_FIFO_REG, strerror(-ret), ret);
      return 1;
    }

  printf("wifi_test: CMD53 block-mode fifo write ok\n");
  return 0;
}

static int wifi_test_flow_ctrl_msg(FAR uint8_t *flow)
{
  uint8_t value = 0;
  int ret;
  int i;

  for (i = 0; i < WIFI_TEST_FLOW_RETRIES; i++)
    {
      ret = wifi_test_readb(1, WIFI_TEST_AIC_V3_FLOW_CTRL_Q1, &value);
      if (ret < 0)
        {
          return ret;
        }

      if (value != 0)
        {
          *flow = value;
          return 0;
        }

      if (i < 30)
        {
          usleep(200);
        }
      else if (i < 40)
        {
          usleep(2 * 1000);
        }
      else
        {
          usleep(10 * 1000);
        }
    }

  *flow = value;
  return -ETIMEDOUT;
}

static int wifi_test_flow_ctrl_data(FAR uint8_t *flow)
{
  uint8_t value = 0;
  int ret;
  int i;

  for (i = 0; i < WIFI_TEST_FLOW_RETRIES; i++)
    {
      ret = wifi_test_readb(1, WIFI_TEST_AIC_V3_FLOW_CTRL_Q1, &value);
      if (ret < 0)
        {
          return ret;
        }

      if (value > WIFI_TEST_DATA_FLOW_THRESH)
        {
          *flow = value;
          return 0;
        }

      if (i < 30)
        {
          usleep(200);
        }
      else if (i < 40)
        {
          usleep(2 * 1000);
        }
      else
        {
          usleep(10 * 1000);
        }
    }

  *flow = value;
  return -ETIMEDOUT;
}

static unsigned int wifi_test_msg_rx_length(uint8_t status, uint8_t byte_len)
{
  if (status == 120)
    {
      return (unsigned int)byte_len * 4;
    }

  return (unsigned int)(status & 0x7f) * WIFI_TEST_AIC_BLOCK_SIZE;
}

static FAR uint8_t *wifi_test_rx_alloc(FAR size_t *rx_size)
{
  *rx_size = sizeof(g_wifi_test_rxbuf);
  return g_wifi_test_rxbuf;
}

static void wifi_test_rx_free(FAR uint8_t *rx)
{
  UNUSED(rx);
}

static int wifi_test_msg_recv_locked(FAR uint8_t *rx, size_t rx_size,
                                     FAR unsigned int *rx_len,
                                     bool verbose, int poll_retries)
{
  uint8_t status = 0;
  uint8_t pending;
  uint8_t byte_len = 0;
  unsigned int length;
  int ret;
  int i;

  *rx_len = 0;
  usleep(100);

  for (i = 0; i < poll_retries; i++)
    {
      do
        {
          ret = wifi_test_readb(1, WIFI_TEST_AIC_V3_MISC_STATUS, &status);
          if (ret == 0)
            {
              break;
            }

          printf("wifi_test: misc status read ret=%d status=0x%02x\n",
                 ret, status);
        }
      while (true);

      if ((status & WIFI_TEST_SDIO_OTHER_INT) != 0)
        {
          ret = wifi_test_readb(1, WIFI_TEST_AIC_V3_INTR_PENDING, &pending);
          if (ret == 0)
            {
              pending &= ~0x01;
              ret = wifi_test_writeb(1, WIFI_TEST_AIC_V3_INTR_PENDING,
                                     pending);
              if (ret < 0)
                {
                  return ret;
                }
            }
        }

      if (verbose)
        {
          printf("  msg poll[%d] status=0x%02x\n", i, status);
        }
      if (status != 0)
        {
          if (status == 120)
            {
              ret = wifi_test_readb(1, WIFI_TEST_AIC_V3_BYTEMODE_LEN,
                                    &byte_len);
              if (ret < 0)
                {
                  return ret;
                }

              if (verbose)
                {
                  printf("  byte mode len=%u words\n", byte_len);
                }
            }

          length = wifi_test_msg_rx_length(status, byte_len);
          if (length == 0)
            {
              printf("wifi_test: rx length %u unsupported\n", length);
              return -EINVAL;
            }

          if (length > rx_size)
            {
              printf("wifi_test: rx length %u unsupported\n", length);
              return -EINVAL;
            }

          memset(rx, 0, length);
          ret = d13x_sdmc0_wifi_read(1, WIFI_TEST_AIC_V3_RD_FIFO_REG, false,
                                     rx, length);
          if (ret < 0)
            {
              printf("wifi_test: CMD53 read fn1 fifo addr=0x%05x len=%u "
                     "failed: %s (%d)\n",
                     WIFI_TEST_AIC_V3_RD_FIFO_REG, length, strerror(-ret),
                     ret);
              return ret;
            }

          *rx_len = length;
          return 0;
        }

      usleep(20 * 1000);
    }

  return -ETIMEDOUT;
}

static int wifi_test_msg_recv(FAR uint8_t *rx, size_t rx_size,
                              FAR unsigned int *rx_len,
                              bool verbose, int poll_retries)
{
  int ret;

  ret = nxmutex_lock(&g_wifi_test_sdio_lock);
  if (ret < 0)
    {
      return ret;
    }

  ret = wifi_test_msg_recv_locked(rx, rx_size, rx_len,
                                  verbose, poll_retries);
  nxmutex_unlock(&g_wifi_test_sdio_lock);
  return ret;
}

static void wifi_test_msg_drain_late(void)
{
  FAR uint8_t *drain;
  size_t drain_size;
  unsigned int rx_len;
  int i;
  int ret;

  drain = wifi_test_rx_alloc(&drain_size);
  for (i = 0; i < 4; i++)
    {
      ret = wifi_test_msg_recv(drain, drain_size, &rx_len, false, 1);
      if (ret != 0)
        {
          break;
        }

      if (!g_wifi_test_quiet)
        {
          printf("wifi_test: drained late rx[%d] len=%u\n", i, rx_len);
          wifi_test_dump_rx_packet(drain, rx_len);
        }
    }

  wifi_test_rx_free(drain);
}

static unsigned int wifi_test_align_block(unsigned int length)
{
  return ((length + WIFI_TEST_AIC_BLOCK_SIZE - 1) /
          WIFI_TEST_AIC_BLOCK_SIZE) * WIFI_TEST_AIC_BLOCK_SIZE;
}

static void wifi_test_put_hostdesc_mac(FAR uint8_t *buffer,
                                       FAR const uint8_t *mac)
{
  wifi_test_put_le16(&buffer[0], (uint16_t)mac[0] |
                                ((uint16_t)mac[1] << 8));
  wifi_test_put_le16(&buffer[2], (uint16_t)mac[2] |
                                ((uint16_t)mac[3] << 8));
  wifi_test_put_le16(&buffer[4], (uint16_t)mac[4] |
                                ((uint16_t)mac[5] << 8));
}

static int wifi_test_send_eth_frame(FAR const uint8_t *frame,
                                    unsigned int frame_len)
{
  static uint8_t tx[WIFI_TEST_DATA_TX_MAX_SIZE];
  FAR struct wifi_test_netdev_s *priv = &g_wifi_test_netdev;
  unsigned int payload_len;
  unsigned int sdio_len;
  unsigned int tx_len;
  uint16_t ethertype;
  uint8_t flow = 0;
  int ret;

  if (frame == NULL || frame_len < WIFI_TEST_ETH_HDR_LEN)
    {
      return -EINVAL;
    }

  payload_len = frame_len - WIFI_TEST_ETH_HDR_LEN;
  sdio_len = WIFI_TEST_TX_HOSTDESC_LEN + payload_len;
  tx_len = wifi_test_align_block(WIFI_TEST_AIC_TX_HDR_LEN + sdio_len);
  if (tx_len > sizeof(tx))
    {
      return -EMSGSIZE;
    }

  memset(tx, 0, tx_len);
  tx[0] = (uint8_t)(sdio_len & 0xff);
  tx[1] = (uint8_t)((sdio_len >> 8) & 0x0f);
  tx[2] = WIFI_TEST_AIC_DATA_SOC2FW;
  tx[3] = wifi_test_crc8(tx, 3);

  ethertype = ((uint16_t)frame[12] << 8) | frame[13];

  wifi_test_put_le16(&tx[4], payload_len);
  wifi_test_put_le16(&tx[6], 0);
  wifi_test_put_le32(&tx[8], 0);
  wifi_test_put_hostdesc_mac(&tx[12], &frame[0]);
  wifi_test_put_hostdesc_mac(&tx[18], &frame[6]);
  memcpy(&tx[24], &frame[12], 2);
  tx[26] = 0; /* AC_BE */
  tx[27] = 0; /* TID_BE */
  tx[28] = priv->vif_idx;
  tx[29] = priv->ap_idx;
  wifi_test_put_le16(&tx[30], 0);

  memcpy(&tx[WIFI_TEST_AIC_TX_HDR_LEN + WIFI_TEST_TX_HOSTDESC_LEN],
         &frame[WIFI_TEST_ETH_HDR_LEN], payload_len);

  ret = nxmutex_lock(&g_wifi_test_sdio_lock);
  if (ret < 0)
    {
      return ret;
    }

  ret = wifi_test_flow_ctrl_data(&flow);
  if (ret < 0)
    {
      nxmutex_unlock(&g_wifi_test_sdio_lock);
      printf("wifi_test: data flow ctrl failed: %s (%d), last=0x%02x\n",
             strerror(-ret), ret, flow);
      return ret;
    }

  ret = d13x_sdmc0_wifi_write(1, WIFI_TEST_AIC_V3_WR_FIFO_REG, false,
                              tx, tx_len);
  nxmutex_unlock(&g_wifi_test_sdio_lock);
  if (ret < 0)
    {
      return ret;
    }

  priv->tx_count++;
  if (!g_wifi_test_quiet &&
      (priv->tx_count <= 8 || (priv->tx_count % 32) == 0))
    {
      printf("wifi_test: tx data[%u] eth_len=%u payload=%u padded=%u "
             "flow=0x%02x vif=%u sta=%u eth=0x%04x\n",
             priv->tx_count, frame_len, payload_len, tx_len, flow,
             priv->vif_idx, priv->ap_idx, ethertype);
    }

  return 0;
}

static int wifi_test_send_msg(uint16_t msg_id, uint16_t dest_id,
                              uint16_t param_len, FAR const uint8_t *param,
                              uint16_t cfm_id, FAR uint8_t *cfm_param,
                              FAR unsigned int *cfm_param_len,
                              bool verbose, int poll_retries)
{
  static uint8_t tx[WIFI_TEST_AIC_TX_MAX_SIZE];
  FAR uint8_t *rx;
  size_t rx_size;
  unsigned int lmac_len;
  unsigned int tx_len;
  unsigned int rx_len;
  unsigned int rx_msg_len;
  unsigned int copy_len;
  unsigned int offset;
  unsigned int next;
  struct wifi_test_rx_msg_s msg;
  uint16_t rx_id;
  uint16_t rx_param_len;
  uint32_t pattern;
  uint8_t flow = 0;
  bool found;
  int skip_count = 0;
  int unexpected_count = 0;
  int ret;
  bool locked = false;

  rx = wifi_test_rx_alloc(&rx_size);
  if (param_len > 0 && param == NULL)
    {
      wifi_test_rx_free(rx);
      return -EINVAL;
    }

  wifi_test_msg_drain_late();

  lmac_len = WIFI_TEST_AIC_LMAC_HDR_LEN + param_len;
  tx_len = wifi_test_align_block(WIFI_TEST_AIC_TX_HDR_LEN +
                                 WIFI_TEST_AIC_TX_DUMMY_LEN + lmac_len);
  if (tx_len == 0 || tx_len > sizeof(tx))
    {
      printf("wifi_test: tx length %u unsupported for param_len=%u\n",
             tx_len, param_len);
      wifi_test_rx_free(rx);
      return -EINVAL;
    }

  memset(tx, 0, sizeof(tx));
  memset(rx, 0, rx_size);

  tx[0] = (uint8_t)((lmac_len + WIFI_TEST_AIC_TX_HDR_LEN) & 0xff);
  tx[1] = (uint8_t)(((lmac_len + WIFI_TEST_AIC_TX_HDR_LEN) >> 8) & 0x0f);
  tx[2] = WIFI_TEST_AIC_MSG_TYPE;
  tx[3] = wifi_test_crc8(tx, 3);

  wifi_test_put_le16(&tx[8], msg_id);
  wifi_test_put_le16(&tx[10], dest_id);
  wifi_test_put_le16(&tx[12], WIFI_TEST_DRV_TASK_ID);
  wifi_test_put_le16(&tx[14], param_len);
  if (param_len > 0)
    {
      memcpy(&tx[16], param, param_len);
    }

  if (verbose)
    {
      printf("  tx msg id=0x%04x lmac_len=%u padded=%u crc=0x%02x\n",
             msg_id, lmac_len, tx_len, tx[3]);
    }

  ret = nxmutex_lock(&g_wifi_test_sdio_lock);
  if (ret < 0)
    {
      wifi_test_rx_free(rx);
      return ret;
    }

  locked = true;

  ret = wifi_test_flow_ctrl_msg(&flow);
  if (ret < 0)
    {
      printf("wifi_test: msg flow ctrl failed: %s (%d), last=0x%02x\n",
             strerror(-ret), ret, flow);
      goto out;
    }

  if (verbose)
    {
      printf("  msg flow ctrl=0x%02x\n", flow);
    }

  ret = d13x_sdmc0_wifi_write(1, WIFI_TEST_AIC_V3_WR_FIFO_REG, false,
                              tx, tx_len);
  if (ret < 0)
    {
      printf("wifi_test: msg TX failed: %s (%d)\n", strerror(-ret), ret);
      goto out;
    }

wait_rx:
  ret = wifi_test_msg_recv_locked(rx, rx_size, &rx_len,
                                  verbose, poll_retries);
  if (ret < 0)
    {
      printf("wifi_test: msg RX failed: %s (%d)\n", strerror(-ret), ret);
      nxmutex_unlock(&g_wifi_test_sdio_lock);
      locked = false;
      if (ret == -ETIMEDOUT)
        {
          wifi_test_msg_drain_late();
        }

      goto out;
    }

  if (verbose)
    {
      printf("  rx len=%u\n", rx_len);
      wifi_test_dump_rx_packet(rx, rx_len);
    }

  if (rx_len < WIFI_TEST_AIC_TX_HDR_LEN + WIFI_TEST_AIC_E2A_HDR_LEN)
    {
      printf("wifi_test: rx message too short\n");
      ret = -EINVAL;
      goto out;
    }

  memset(&msg, 0, sizeof(msg));
  found = false;
  offset = 0;
  while (wifi_test_rx_msg_parse(rx, rx_len, offset, &msg))
    {
      if (msg.param != NULL)
        {
          if (msg.msg_id == cfm_id)
            {
              found = true;
              break;
            }

          if (cfm_id == WIFI_TEST_SCANU_START_CFM_ADDITIONAL &&
              msg.msg_id == WIFI_TEST_SCANU_START_CFM && skip_count < 4)
            {
              printf("wifi_test: skipped SCANU_START_CFM while waiting for "
                     "0x%04x\n", cfm_id);
              skip_count++;
            }
        }

      next = offset + wifi_test_rx_padded_len(&msg);
      if (next <= offset)
        {
          break;
        }

      offset = next;
    }

  if (!found)
    {
      printf("wifi_test: expected message id 0x%04x not found in rx\n",
             cfm_id);
      if (!verbose && !g_wifi_test_quiet)
        {
          printf("  rx len=%u\n", rx_len);
          wifi_test_dump_rx_packet(rx, rx_len);
        }

      unexpected_count++;
      if (unexpected_count <= 4)
        {
          goto wait_rx;
        }

      ret = -EPROTO;
      goto out;
    }

  rx_msg_len = msg.sdio_len;
  rx_id = msg.msg_id;
  rx_param_len = msg.param_len;
  pattern = msg.pattern;

  if (verbose)
    {
      printf("  cfm id=0x%04x param_len=%u pattern=0x%08lx msg_len=%u\n",
             rx_id, rx_param_len, (unsigned long)pattern, rx_msg_len);
    }

  if (rx_msg_len < WIFI_TEST_AIC_E2A_HDR_LEN ||
      msg.offset + WIFI_TEST_AIC_TX_HDR_LEN + rx_msg_len > rx_len)
    {
      printf("wifi_test: rx msg_len %u invalid for rx_len=%u\n",
             rx_msg_len, rx_len);
      ret = -EPROTO;
      goto out;
    }

  if (rx_param_len > rx_msg_len - WIFI_TEST_AIC_E2A_HDR_LEN)
    {
      printf("wifi_test: cfm param_len %u invalid for msg_len=%u\n",
             rx_param_len, rx_msg_len);
      ret = -EPROTO;
      goto out;
    }

  if (cfm_param_len != NULL)
    {
      copy_len = *cfm_param_len;
      if (copy_len > rx_param_len)
        {
          copy_len = rx_param_len;
        }

      if (copy_len > 0 && cfm_param != NULL)
        {
          memcpy(cfm_param, msg.param, copy_len);
        }

      *cfm_param_len = rx_param_len;
    }

  ret = 0;

out:
  if (locked)
    {
      nxmutex_unlock(&g_wifi_test_sdio_lock);
    }

  wifi_test_rx_free(rx);
  return ret;
}

static int wifi_test_dbg_mem_read(uint32_t address, FAR uint32_t *data)
{
  uint8_t req[4];
  uint8_t cfm[8];
  unsigned int cfm_len = sizeof(cfm);
  uint32_t memaddr;
  int ret;

  wifi_test_put_le32(req, address);

  ret = wifi_test_send_msg(WIFI_TEST_DBG_MEM_READ_REQ, WIFI_TEST_TASK_DBG,
                           sizeof(req), req, WIFI_TEST_DBG_MEM_READ_CFM,
                           cfm, &cfm_len, !g_wifi_test_quiet,
                           WIFI_TEST_MSG_POLL_RETRIES);
  if (ret < 0)
    {
      return ret;
    }

  if (cfm_len < sizeof(cfm))
    {
      printf("wifi_test: DBG_MEM_READ_CFM too short: %u\n", cfm_len);
      return -EPROTO;
    }

  memaddr = wifi_test_get_le32(&cfm[0]);
  if (data != NULL)
    {
      *data = wifi_test_get_le32(&cfm[4]);
    }

  if (!g_wifi_test_quiet)
    {
      printf("  mem[0x%08lx]=0x%08lx\n",
             (unsigned long)memaddr,
             (unsigned long)wifi_test_get_le32(&cfm[4]));
    }

  if (memaddr != address)
    {
      printf("wifi_test: read CFM address mismatch, expected 0x%08lx\n",
             (unsigned long)address);
      return -EPROTO;
    }

  return 0;
}

static int wifi_test_dbg_mem_block_write(uint32_t address,
                                         FAR const uint8_t *data,
                                         uint32_t size,
                                         bool verbose)
{
  static uint8_t req[8 + 1024];
  uint8_t cfm[4];
  unsigned int cfm_len = sizeof(cfm);
  uint32_t status;
  int ret;

  if (data == NULL || size == 0 || size > 1024)
    {
      return -EINVAL;
    }

  memset(req, 0, sizeof(req));
  wifi_test_put_le32(&req[0], address);
  wifi_test_put_le32(&req[4], size);
  memcpy(&req[8], data, size);

  ret = wifi_test_send_msg(WIFI_TEST_DBG_MEM_BLOCK_WRITE_REQ,
                           WIFI_TEST_TASK_DBG, 8 + size, req,
                           WIFI_TEST_DBG_MEM_BLOCK_WRITE_CFM, cfm, &cfm_len,
                           verbose, WIFI_TEST_MSG_POLL_RETRIES);
  if (ret < 0)
    {
      return ret;
    }

  if (cfm_len < sizeof(cfm))
    {
      printf("wifi_test: DBG_MEM_BLOCK_WRITE_CFM too short: %u\n", cfm_len);
      return -EPROTO;
    }

  status = wifi_test_get_le32(cfm);
  if (verbose)
    {
      printf("  block write status=0x%08lx\n", (unsigned long)status);
    }
  if (status != 0)
    {
      return -EIO;
    }

  return 0;
}

static int wifi_test_dbg_start_app(uint32_t address, uint32_t type)
{
  uint8_t req[8];
  uint8_t cfm[4];
  unsigned int cfm_len = sizeof(cfm);
  uint32_t status;
  int ret;

  wifi_test_put_le32(&req[0], address);
  wifi_test_put_le32(&req[4], type);

  ret = wifi_test_send_msg(WIFI_TEST_DBG_START_APP_REQ, WIFI_TEST_TASK_DBG,
                           sizeof(req), req, WIFI_TEST_DBG_START_APP_CFM,
                           cfm, &cfm_len, !g_wifi_test_quiet,
                           WIFI_TEST_MSG_POLL_RETRIES);
  if (ret < 0)
    {
      return ret;
    }

  if (cfm_len < sizeof(cfm))
    {
      printf("wifi_test: DBG_START_APP_CFM too short: %u\n", cfm_len);
      return -EPROTO;
    }

  status = wifi_test_get_le32(cfm);
  if (!g_wifi_test_quiet)
    {
      printf("  start app status=0x%08lx\n", (unsigned long)status);
    }
  if (status != 0)
    {
      return -EIO;
    }

  return 0;
}

static int wifi_test_msg(void)
{
  uint32_t memdata;
  bool old_quiet = g_wifi_test_quiet;
  int ret;

  g_wifi_test_quiet = false;
  ret = wifi_test_enable();
  if (ret != 0)
    {
      g_wifi_test_quiet = old_quiet;
      return ret;
    }

  ret = wifi_test_dbg_mem_read(0x40500000, &memdata);
  if (ret < 0)
    {
      printf("wifi_test: DBG_MEM_READ failed: %s (%d)\n",
             strerror(-ret), ret);
      g_wifi_test_quiet = old_quiet;
      return 1;
    }

  printf("wifi_test: DBG_MEM_READ message ok\n");
  g_wifi_test_quiet = old_quiet;
  return 0;
}

static FAR const char *wifi_test_host_type_name(uint8_t type)
{
  switch (type)
    {
      case WIFI_TEST_AIC_DATA_FW2SOC:
        return "DATA_FW2SOC";
      case WIFI_TEST_AIC_DATA_SOC2FW:
        return "DATA_SOC2FW";
      case WIFI_TEST_AIC_CFG_TYPE:
        return "CFG";
      case WIFI_TEST_AIC_MSG_TYPE:
        return "CFG_CMD_RSP";
      case WIFI_TEST_AIC_CFG_DATA_CFM:
        return "CFG_DATA_CFM";
      default:
        return "UNKNOWN";
    }
}

static void wifi_test_print_mac(FAR const uint8_t *mac)
{
  printf("%02x:%02x:%02x:%02x:%02x:%02x",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void wifi_test_print_ipv4(FAR const uint8_t *ip)
{
  printf("%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

static void wifi_test_dump_scanu_start_cfm(FAR const uint8_t *param,
                                           unsigned int param_len,
                                           FAR const char *tag)
{
  if (param_len >= 3)
    {
      printf("  %s vif=%u status=%u result_cnt=%u\n",
             tag, param[0], param[1], param[2]);
    }
  else
    {
      printf("  %s len=%u\n", tag, param_len);
    }
}

static void wifi_test_dump_ascii(FAR const uint8_t *data,
                                 unsigned int data_len)
{
  unsigned int i;

  for (i = 0; i < data_len; i++)
    {
      uint8_t ch = data[i];

      if (ch < 0x20 || ch > 0x7e)
        {
          ch = '.';
        }

      putchar(ch);
    }
}

static unsigned int wifi_test_rx_padded_len(FAR const struct
                                            wifi_test_rx_msg_s *msg)
{
  unsigned int raw_len = WIFI_TEST_AIC_TX_HDR_LEN + msg->sdio_len;

  if (msg->type == WIFI_TEST_AIC_DATA_FW2SOC)
    {
      raw_len = WIFI_TEST_AIC_TX_HDR_LEN + WIFI_TEST_RX_DESC_LEN +
                msg->sdio_len;
      return (raw_len + 3) & ~3u;
    }

  return (raw_len + 3) & ~3u;
}

static bool wifi_test_rx_msg_parse(FAR const uint8_t *rx,
                                   unsigned int rx_len,
                                   unsigned int offset,
                                   FAR struct wifi_test_rx_msg_s *msg)
{
  unsigned int sdio_len;
  uint8_t type;

  if (offset + WIFI_TEST_AIC_TX_HDR_LEN > rx_len)
    {
      return false;
    }

  rx += offset;
  sdio_len = (unsigned int)rx[0] | (((unsigned int)rx[1] & 0x0f) << 8);
  type = rx[2];
  if (type == WIFI_TEST_AIC_DATA_FW2SOC)
    {
      if (sdio_len == 0 ||
          offset + WIFI_TEST_AIC_TX_HDR_LEN + WIFI_TEST_RX_DESC_LEN +
          sdio_len > rx_len)
        {
          return false;
        }
    }
  else if (sdio_len < WIFI_TEST_AIC_E2A_HDR_LEN ||
           offset + WIFI_TEST_AIC_TX_HDR_LEN + sdio_len > rx_len)
    {
      return false;
    }

  memset(msg, 0, sizeof(*msg));
  msg->base = rx;
  msg->offset = offset;
  msg->sdio_len = sdio_len;
  msg->total_len = rx_len;
  msg->type = type;
  msg->crc = wifi_test_crc8(rx, 3);

  if ((type == WIFI_TEST_AIC_MSG_TYPE || type == WIFI_TEST_AIC_CFG_TYPE ||
       type == WIFI_TEST_AIC_CFG_DATA_CFM) &&
      sdio_len >= WIFI_TEST_AIC_E2A_HDR_LEN)
    {
      msg->msg_id = wifi_test_get_le16(&rx[4]);
      msg->dest_id = wifi_test_get_le16(&rx[6]);
      msg->src_id = wifi_test_get_le16(&rx[8]);
      msg->param_len = wifi_test_get_le16(&rx[10]);
      msg->pattern = wifi_test_get_le32(&rx[12]);
      if (msg->param_len <=
          sdio_len - WIFI_TEST_AIC_E2A_HDR_LEN)
        {
          msg->param = &rx[16];
        }
    }

  return true;
}

static bool wifi_test_rx_data_frame(FAR const struct wifi_test_rx_msg_s *msg,
                                    FAR const uint8_t **frame,
                                    FAR unsigned int *frame_len)
{
  FAR const uint8_t *data;
  FAR const uint8_t *candidate;
  unsigned int packet_len;
  unsigned int frame_off;
  unsigned int mpdu_len;

  if (msg->type != WIFI_TEST_AIC_DATA_FW2SOC ||
      msg->sdio_len == 0 ||
      msg->offset + WIFI_TEST_AIC_TX_HDR_LEN + WIFI_TEST_RX_DESC_LEN +
      msg->sdio_len > msg->total_len)
    {
      return false;
    }

  data = &msg->base[WIFI_TEST_AIC_TX_HDR_LEN];
  mpdu_len = msg->sdio_len;
  candidate = &data[WIFI_TEST_RX_DESC_LEN];
  if (wifi_test_80211_frame_valid(candidate, mpdu_len))
    {
      *frame = candidate;
      *frame_len = mpdu_len;
      return true;
    }

  packet_len = WIFI_TEST_RX_DESC_LEN + mpdu_len;
  candidate = wifi_test_find_80211_frame(data, packet_len, &frame_off);
  if (candidate == NULL)
    {
      return false;
    }

  *frame = candidate;
  *frame_len = packet_len - frame_off;
  return true;
}

static bool wifi_test_80211_frame_valid(FAR const uint8_t *frame,
                                        unsigned int frame_len)
{
  uint16_t fctl;
  uint16_t type;
  uint16_t subtype;

  if (frame_len < WIFI_TEST_80211_HDR_LEN)
    {
      return false;
    }

  fctl = wifi_test_get_le16(frame);
  type = fctl & WIFI_TEST_80211_FCTL_TYPE_MASK;
  subtype = fctl & WIFI_TEST_80211_FCTL_SUBT_MASK;
  if (type == WIFI_TEST_80211_TYPE_DATA)
    {
      return (subtype & WIFI_TEST_80211_SUBT_NODATA) == 0;
    }

  if (type != WIFI_TEST_80211_TYPE_MGMT)
    {
      return false;
    }

  return subtype <= 0x00d0;
}

static unsigned int wifi_test_80211_hdr_len(FAR const uint8_t *frame,
                                            unsigned int frame_len)
{
  uint16_t fctl;
  unsigned int hdr_len = WIFI_TEST_80211_HDR_LEN;

  if (frame_len < WIFI_TEST_80211_HDR_LEN)
    {
      return 0;
    }

  fctl = wifi_test_get_le16(frame);
  if ((fctl & WIFI_TEST_80211_SUBT_QOS) != 0)
    {
      hdr_len += WIFI_TEST_80211_QOS_LEN;
    }

  if ((fctl & WIFI_TEST_80211_FCTL_ORDER) != 0)
    {
      hdr_len += WIFI_TEST_80211_HTCTRL_LEN;
    }

  return hdr_len <= frame_len ? hdr_len : 0;
}

static FAR const uint8_t *wifi_test_80211_da(FAR const uint8_t *frame)
{
  uint16_t fctl = wifi_test_get_le16(frame);

  if ((fctl & WIFI_TEST_80211_FCTL_TODS) != 0 &&
      (fctl & WIFI_TEST_80211_FCTL_FROMDS) != 0)
    {
      return &frame[16];
    }

  if ((fctl & WIFI_TEST_80211_FCTL_TODS) != 0)
    {
      return &frame[16];
    }

  return &frame[4];
}

static FAR const uint8_t *wifi_test_80211_sa(FAR const uint8_t *frame)
{
  uint16_t fctl = wifi_test_get_le16(frame);

  if ((fctl & WIFI_TEST_80211_FCTL_TODS) != 0 &&
      (fctl & WIFI_TEST_80211_FCTL_FROMDS) != 0)
    {
      return &frame[24];
    }

  if ((fctl & WIFI_TEST_80211_FCTL_FROMDS) != 0)
    {
      return &frame[16];
    }

  return &frame[10];
}

static FAR const uint8_t *wifi_test_find_80211_frame(FAR const uint8_t *data,
                                                     unsigned int data_len,
                                                     FAR unsigned int *offset)
{
  static const unsigned int offsets[] =
  {
    0, 2, 4, 8, 12, 16, 20, 24, 32, 40, 48, 56, 60, 64, 68, 72, 76, 80
  };
  unsigned int i;

  for (i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++)
    {
      unsigned int off = offsets[i];
      FAR const uint8_t *candidate;
      uint16_t fctl;
      unsigned int type;

      if (off < data_len &&
          wifi_test_80211_frame_valid(&data[off], data_len - off))
        {
          candidate = &data[off];
          fctl = wifi_test_get_le16(candidate);
          type = fctl & WIFI_TEST_80211_FCTL_TYPE_MASK;
          if (type == WIFI_TEST_80211_TYPE_DATA)
            {
              unsigned int hdr_len;
              FAR const uint8_t *llc;

              hdr_len = wifi_test_80211_hdr_len(candidate, data_len - off);
              if (hdr_len == 0 ||
                  hdr_len + WIFI_TEST_LLC_SNAP_LEN > data_len - off)
                {
                  continue;
                }

              llc = &candidate[hdr_len];
              if (llc[0] != 0xaa || llc[1] != 0xaa || llc[2] != 0x03)
                {
                  continue;
                }
            }

          *offset = off;
          return candidate;
        }
    }

  return NULL;
}

static void wifi_test_dump_ethertype(uint16_t ethertype)
{
  switch (ethertype)
    {
      case WIFI_TEST_ETHERTYPE_IP:
        printf("IPv4");
        break;
      case WIFI_TEST_ETHERTYPE_ARP:
        printf("ARP");
        break;
      case WIFI_TEST_ETHERTYPE_EAPOL:
        printf("EAPOL");
        break;
      default:
        printf("0x%04x", ethertype);
        break;
    }
}

static int wifi_test_80211_to_eth(FAR const uint8_t *frame,
                                  unsigned int frame_len,
                                  FAR uint8_t *eth,
                                  unsigned int eth_size,
                                  FAR uint16_t *ethertype)
{
  FAR const uint8_t *llc;
  FAR const uint8_t *da;
  FAR const uint8_t *sa;
  unsigned int hdr_len;
  unsigned int payload_len;
  uint16_t fctl;

  if (frame == NULL || eth == NULL ||
      frame_len < WIFI_TEST_80211_HDR_LEN ||
      eth_size < WIFI_TEST_ETH_HDR_LEN)
    {
      return -EINVAL;
    }

  fctl = wifi_test_get_le16(frame);
  if ((fctl & WIFI_TEST_80211_FCTL_TYPE_MASK) != WIFI_TEST_80211_TYPE_DATA)
    {
      return -ENOMSG;
    }

  hdr_len = wifi_test_80211_hdr_len(frame, frame_len);
  if (hdr_len == 0 || hdr_len + WIFI_TEST_LLC_SNAP_LEN > frame_len)
    {
      return -EPROTO;
    }

  llc = &frame[hdr_len];
  if (llc[0] != 0xaa || llc[1] != 0xaa || llc[2] != 0x03)
    {
      return -EPROTO;
    }

  payload_len = frame_len - hdr_len - WIFI_TEST_LLC_SNAP_LEN;
  if (payload_len + WIFI_TEST_ETH_HDR_LEN > eth_size)
    {
      return -EMSGSIZE;
    }

  da = wifi_test_80211_da(frame);
  sa = wifi_test_80211_sa(frame);
  memcpy(&eth[0], da, 6);
  memcpy(&eth[6], sa, 6);
  memcpy(&eth[12], &llc[6], 2);
  memcpy(&eth[WIFI_TEST_ETH_HDR_LEN],
         &llc[WIFI_TEST_LLC_SNAP_LEN], payload_len);

  if (ethertype != NULL)
    {
      *ethertype = ((uint16_t)llc[6] << 8) | llc[7];
    }

  return WIFI_TEST_ETH_HDR_LEN + payload_len;
}

static int wifi_test_rx_msg_to_eth(FAR const struct wifi_test_rx_msg_s *msg,
                                   FAR uint8_t *eth,
                                   unsigned int eth_size,
                                   FAR uint16_t *ethertype)
{
  FAR const uint8_t *frame;
  unsigned int frame_len;

  if (!wifi_test_rx_data_frame(msg, &frame, &frame_len))
    {
      return -ENOMSG;
    }

  return wifi_test_80211_to_eth(frame, frame_len, eth, eth_size, ethertype);
}

static void wifi_test_net_reply(FAR struct net_driver_s *dev)
{
  if (dev->d_len > 0)
    {
      int ret = wifi_test_send_eth_frame(dev->d_buf, dev->d_len);

      if (ret < 0)
        {
          printf("wifi_test: rx reply tx len=%u failed: %s (%d)\n",
                 dev->d_len, strerror(-ret), ret);
        }

      dev->d_len = 0;
    }
}

static int wifi_test_feed_rx_msg(FAR const struct wifi_test_rx_msg_s *msg,
                                 bool verbose)
{
  FAR struct wifi_test_netdev_s *priv = &g_wifi_test_netdev;
  FAR struct net_driver_s *dev = &priv->dev;
  FAR const uint8_t *eth;
  uint16_t ethertype = 0;
  static unsigned int feed_count;
  int eth_len;

  if (!priv->registered || !priv->connected)
    {
      return 0;
    }

  eth_len = wifi_test_rx_msg_to_eth(msg, dev->d_buf, WIFI_TEST_NETBUF_SIZE,
                                   &ethertype);
  if (eth_len < 0)
    {
      if (verbose && eth_len != -ENOMSG)
        {
          printf("wifi_test: rx data convert failed: %s (%d)\n",
                 strerror(-eth_len), eth_len);
        }

      return 0;
    }

  dev->d_len = eth_len;
  feed_count++;
  if (verbose || feed_count <= 16 || (feed_count % 32) == 0)
    {
      printf("wifi_test: rx feed len=%u eth=", dev->d_len);
      wifi_test_dump_ethertype(ethertype);
      eth = dev->d_buf;
      printf(" ");
      wifi_test_print_mac(&eth[6]);
      printf(" -> ");
      wifi_test_print_mac(&eth[0]);
      if (ethertype == WIFI_TEST_ETHERTYPE_IP && eth_len >= 34)
        {
          printf(" ip ");
          wifi_test_print_ipv4(&eth[26]);
          printf(" -> ");
          wifi_test_print_ipv4(&eth[30]);
        }
      else if (ethertype == WIFI_TEST_ETHERTYPE_ARP && eth_len >= 42)
        {
          printf(" arp op=%u spa=", ((unsigned int)eth[20] << 8) | eth[21]);
          wifi_test_print_ipv4(&eth[28]);
          printf(" tpa=");
          wifi_test_print_ipv4(&eth[38]);
        }

      printf("\n");
    }

  net_lock();
  NETDEV_RXPACKETS(*dev);

#ifdef CONFIG_NET_PKT
  pkt_input(dev);
#endif

#ifdef CONFIG_NET_IPv4
  if (WIFI_TEST_ETHBUF(dev)->type == HTONS(ETHTYPE_IP))
    {
      NETDEV_RXIPV4(dev);
      ipv4_input(dev);
      wifi_test_net_reply(dev);
    }
  else
#endif
#ifdef CONFIG_NET_ARP
  if (WIFI_TEST_ETHBUF(dev)->type == HTONS(ETHTYPE_ARP))
    {
      arp_input(dev);
      NETDEV_RXARP(dev);
      wifi_test_net_reply(dev);
    }
  else
#endif
    {
      NETDEV_RXDROPPED(dev);
      dev->d_len = 0;
    }

  net_unlock();
  return 1;
}

static void wifi_test_dump_data_frame(FAR const struct wifi_test_rx_msg_s *msg)
{
  FAR const uint8_t *data = &msg->base[WIFI_TEST_AIC_TX_HDR_LEN];
  FAR const uint8_t *frame;
  FAR const uint8_t *llc;
  FAR const uint8_t *da;
  FAR const uint8_t *sa;
  unsigned int data_len = msg->sdio_len;
  unsigned int packet_len = WIFI_TEST_RX_DESC_LEN + msg->sdio_len;
  unsigned int frame_off = WIFI_TEST_RX_DESC_LEN;
  unsigned int frame_len;
  unsigned int hdr_len;
  uint32_t rx_flags = 0;
  uint16_t fctl;
  uint16_t ethertype;

  if (packet_len >= WIFI_TEST_RX_DESC_LEN)
    {
      rx_flags = wifi_test_get_le32(&data[48]);
    }

  printf("    data fw2soc mpdu_len=%u desc_len=%u flags=0x%08lx\n",
         data_len, WIFI_TEST_RX_DESC_LEN, (unsigned long)rx_flags);

  if (wifi_test_rx_data_frame(msg, &frame, &frame_len))
    {
      frame_off = WIFI_TEST_RX_DESC_LEN;
    }
  else
    {
      frame = wifi_test_find_80211_frame(data, packet_len, &frame_off);
      if (frame != NULL)
        {
          frame_len = packet_len - frame_off;
        }
    }

  if (frame == NULL)
    {
      printf("    data fw2soc no 802.11 frame candidate\n");
      wifi_test_hexdump(data, data_len > 96 ? 96 : data_len);
      return;
    }

  hdr_len = wifi_test_80211_hdr_len(frame, frame_len);
  fctl = wifi_test_get_le16(frame);
  da = wifi_test_80211_da(frame);
  sa = wifi_test_80211_sa(frame);

  printf("    802.11 off=%u fctl=0x%04x hdr=%u "
         "da=%02x:%02x:%02x:%02x:%02x:%02x "
         "sa=%02x:%02x:%02x:%02x:%02x:%02x\n",
         frame_off, fctl, hdr_len,
         da[0], da[1], da[2], da[3], da[4], da[5],
         sa[0], sa[1], sa[2], sa[3], sa[4], sa[5]);

  if ((fctl & WIFI_TEST_80211_FCTL_TYPE_MASK) != WIFI_TEST_80211_TYPE_DATA ||
      hdr_len == 0 || hdr_len + WIFI_TEST_LLC_SNAP_LEN > frame_len)
    {
      return;
    }

  llc = &frame[hdr_len];
  if (llc[0] != 0xaa || llc[1] != 0xaa || llc[2] != 0x03)
    {
      printf("    802.11 data has no LLC/SNAP at hdr=%u: "
             "%02x %02x %02x %02x\n",
             hdr_len, llc[0], llc[1], llc[2], llc[3]);
      return;
    }

  ethertype = ((uint16_t)llc[6] << 8) | llc[7];
  printf("    ethernet candidate len=%u ethertype=",
         frame_len - hdr_len - WIFI_TEST_LLC_SNAP_LEN +
         WIFI_TEST_ETH_HDR_LEN);
  wifi_test_dump_ethertype(ethertype);
  printf("\n");
}

static void wifi_test_dump_scan_result(FAR const uint8_t *param,
                                       unsigned int param_len)
{
  unsigned int frame_len;
  unsigned int body_len;
  unsigned int pos;
  unsigned int ssid_len;

  if (param_len < WIFI_TEST_SCANU_RESULT_META_LEN)
    {
      printf("  scanu_result len=%u\n", param_len);
      return;
    }

  frame_len = wifi_test_get_le16(param);
  printf("  scanu_result len=%u framectrl=0x%04x freq=%u band=%u sta=%u "
         "vif=%u rssi=%d\n",
         frame_len, wifi_test_get_le16(&param[2]),
         wifi_test_get_le16(&param[4]), param[6], param[7], param[8],
         (int8_t)param[9]);

  body_len = param_len - WIFI_TEST_SCANU_RESULT_META_LEN;
  if (body_len >= WIFI_TEST_80211_MGMT_IES_OFF)
    {
      printf("  bssid=%02x:%02x:%02x:%02x:%02x:%02x",
             param[WIFI_TEST_SCANU_RESULT_META_LEN +
                   WIFI_TEST_80211_MGMT_BSSID_OFF],
             param[WIFI_TEST_SCANU_RESULT_META_LEN +
                   WIFI_TEST_80211_MGMT_BSSID_OFF + 1],
             param[WIFI_TEST_SCANU_RESULT_META_LEN +
                   WIFI_TEST_80211_MGMT_BSSID_OFF + 2],
             param[WIFI_TEST_SCANU_RESULT_META_LEN +
                   WIFI_TEST_80211_MGMT_BSSID_OFF + 3],
             param[WIFI_TEST_SCANU_RESULT_META_LEN +
                   WIFI_TEST_80211_MGMT_BSSID_OFF + 4],
             param[WIFI_TEST_SCANU_RESULT_META_LEN +
                   WIFI_TEST_80211_MGMT_BSSID_OFF + 5]);

      pos = WIFI_TEST_SCANU_RESULT_META_LEN + WIFI_TEST_80211_MGMT_IES_OFF;
      if (pos + 2 <= param_len && param[pos] == 0)
        {
          ssid_len = param[pos + 1];
          if (ssid_len > 32)
            {
              ssid_len = 32;
            }

          if (pos + 2 + ssid_len <= param_len)
            {
              printf(" ssid=\"");
              wifi_test_dump_ascii(&param[pos + 2], ssid_len);
              printf("\"");
            }
        }

      printf("\n");
    }
}

static void wifi_test_dump_rx_msg(FAR const struct wifi_test_rx_msg_s *msg,
                                  unsigned int index)
{
  printf("  submsg[%u] off=%u sdio_len=%u host_type=0x%02x(%s) "
         "crc=0x%02x/%s\n",
         index, msg->offset, msg->sdio_len, msg->type,
         wifi_test_host_type_name(msg->type), msg->base[3],
         msg->crc == msg->base[3] ? "ok" : "bad");

  if (msg->param != NULL)
    {
      printf("    msg id=0x%04x dest=0x%04x src=0x%04x param_len=%u "
             "pattern=0x%08lx\n",
             msg->msg_id, msg->dest_id, msg->src_id, msg->param_len,
             (unsigned long)msg->pattern);

      if (msg->msg_id == WIFI_TEST_SCANU_START_CFM)
        {
          wifi_test_dump_scanu_start_cfm(msg->param, msg->param_len,
                                         "scanu_start_cfm");
        }
      else if (msg->msg_id == WIFI_TEST_SCANU_START_CFM_ADDITIONAL)
        {
          wifi_test_dump_scanu_start_cfm(msg->param, msg->param_len,
                                         "scanu_start_cfm_additional");
        }
      else if (msg->msg_id == WIFI_TEST_SCANU_RESULT_IND)
        {
          wifi_test_dump_scan_result(msg->param, msg->param_len);
        }
      else if (msg->msg_id == WIFI_TEST_SM_CONNECT_IND)
        {
          wifi_test_dump_connect_ind(msg->param, msg->param_len);
        }
    }
  else if (msg->type == WIFI_TEST_AIC_DATA_FW2SOC)
    {
      wifi_test_dump_data_frame(msg);
    }
  else
    {
      printf("    unsupported or malformed submsg\n");
    }
}

static void wifi_test_dump_rx_packet(FAR const uint8_t *rx,
                                     unsigned int rx_len)
{
  struct wifi_test_rx_msg_s msg;
  unsigned int offset = 0;
  unsigned int index = 0;
  unsigned int next;

  if (rx_len < WIFI_TEST_AIC_TX_HDR_LEN)
    {
      printf("wifi_test: rx packet too short: %u\n", rx_len);
      return;
    }

  printf("wifi_test: rx total=%u\n", rx_len);

  while (wifi_test_rx_msg_parse(rx, rx_len, offset, &msg))
    {
      wifi_test_dump_rx_msg(&msg, index++);
      next = offset + wifi_test_rx_padded_len(&msg);
      if (next <= offset)
        {
          break;
        }

      offset = next;
    }

  if (index == 0)
    {
      printf("wifi_test: no valid submsg decoded\n");
    }

  wifi_test_hexdump(rx, rx_len > 384 ? 384 : rx_len);
}

static unsigned int wifi_test_feed_rx_packet(FAR const uint8_t *rx,
                                             unsigned int rx_len,
                                             bool verbose)
{
  struct wifi_test_rx_msg_s msg;
  unsigned int offset = 0;
  unsigned int next;
  unsigned int fed = 0;

  while (wifi_test_rx_msg_parse(rx, rx_len, offset, &msg))
    {
      if (msg.type == WIFI_TEST_AIC_DATA_FW2SOC)
        {
          fed += wifi_test_feed_rx_msg(&msg, verbose) > 0 ? 1 : 0;
        }

      next = offset + wifi_test_rx_padded_len(&msg);
      if (next <= offset)
        {
          break;
        }

      offset = next;
    }

  return fed;
}

static int wifi_test_rxpoll_common(unsigned int count, bool dump)
{
  FAR uint8_t *rx;
  size_t rx_size;
  unsigned int rx_len;
  unsigned int fed;
  unsigned int i;
  int ret;

  ret = wifi_test_enable();
  if (ret != 0)
    {
      return ret;
    }

  rx = wifi_test_rx_alloc(&rx_size);
  for (i = 0; i < count; i++)
    {
      memset(rx, 0, rx_size);
      rx_len = 0;

      ret = wifi_test_msg_recv(rx, rx_size, &rx_len, true, 1);
      if (ret == -ETIMEDOUT)
        {
          printf("wifi_test: %s[%u] no packet\n",
                 dump ? "rxpoll" : "rxfeed", i);
          usleep(100 * 1000);
          continue;
        }

      if (ret < 0)
        {
          printf("wifi_test: %s[%u] failed: %s (%d)\n",
                 dump ? "rxpoll" : "rxfeed", i, strerror(-ret), ret);
          wifi_test_rx_free(rx);
          return 1;
        }

      fed = wifi_test_feed_rx_packet(rx, rx_len, !dump);
      if (dump)
        {
          printf("wifi_test: rxpoll[%u] fed=%u\n", i, fed);
          wifi_test_dump_rx_packet(rx, rx_len);
        }
      else if (fed > 0)
        {
          printf("wifi_test: rxfeed[%u] len=%u fed=%u\n", i, rx_len, fed);
        }
    }

  wifi_test_rx_free(rx);
  return 0;
}

static int wifi_test_rxpoll(unsigned int count)
{
  return wifi_test_rxpoll_common(count, true);
}

static int wifi_test_rxfeed(unsigned int count)
{
  return wifi_test_rxpoll_common(count, false);
}

static int wifi_test_wait_connect_ind(void)
{
  FAR struct wifi_test_netdev_s *priv = &g_wifi_test_netdev;
  FAR uint8_t *rx;
  uint8_t eth[WIFI_TEST_NETBUF_SIZE];
  size_t rx_size;
  unsigned int rx_len;
  unsigned int offset;
  unsigned int next;
  unsigned int i;
  unsigned int submsgs;
  unsigned int scan_results = 0;
  unsigned int data_frames = 0;
  unsigned int other_msgs = 0;
  struct wifi_test_rx_msg_s msg;
  uint16_t ethertype;
  int eth_len;
  int ret;

  rx = wifi_test_rx_alloc(&rx_size);
  for (i = 0; i < WIFI_TEST_CONNECT_POLL_COUNT; i++)
    {
      memset(rx, 0, rx_size);
      rx_len = 0;

      ret = wifi_test_msg_recv(rx, rx_size, &rx_len, false, 1);
      if (ret == -ETIMEDOUT)
        {
          usleep(100 * 1000);
          continue;
        }

      if (ret < 0)
        {
          printf("wifi_test: connect wait rx failed: %s (%d)\n",
                 strerror(-ret), ret);
          wifi_test_rx_free(rx);
          return ret;
        }

      if (!g_wifi_test_quiet)
        {
          printf("wifi_test: connect wait rx[%u] len=%u\n", i, rx_len);
        }

      offset = 0;
      submsgs = 0;
      while (wifi_test_rx_msg_parse(rx, rx_len, offset, &msg))
        {
          if (!g_wifi_test_quiet)
            {
              printf("  submsg[%u] id=0x%04x type=0x%02x len=%u param=%u\n",
                     submsgs, msg.msg_id, msg.type, msg.sdio_len,
                     msg.param_len);
            }

          if (msg.param != NULL &&
              msg.msg_id == WIFI_TEST_SM_CONNECT_IND)
            {
              if (!g_wifi_test_quiet)
                {
                  wifi_test_dump_connect_ind(msg.param, msg.param_len);
                }
              if (msg.param_len >= WIFI_TEST_SM_CONNECT_IND_MIN_LEN &&
                  wifi_test_get_le16(&msg.param[0]) == 0)
                {
                  g_wifi_test_netdev.vif_idx = msg.param[9];
                  g_wifi_test_netdev.ap_idx = msg.param[10];
                  g_wifi_test_netdev.connected = true;
                  wifi_test_rx_free(rx);
                  return 0;
                }

              printf("wifi_test: connect ind failed status=%u\n",
                     msg.param_len >= 2 ? wifi_test_get_le16(&msg.param[0]) :
                     0xffff);
              wifi_test_rx_free(rx);
              return -ECONNREFUSED;
            }

          if (msg.param != NULL &&
              msg.msg_id == WIFI_TEST_SCANU_RESULT_IND)
            {
              scan_results++;
            }
          else if (msg.type == WIFI_TEST_AIC_DATA_FW2SOC)
            {
              data_frames++;
              eth_len = wifi_test_rx_msg_to_eth(&msg, eth, sizeof(eth),
                                                &ethertype);
              if (eth_len > 0)
                {
                  priv->vif_idx = 0;
                  priv->ap_idx = 1;
                  priv->connected = true;
                  printf("wifi_test: connected by data fallback eth=");
                  wifi_test_dump_ethertype(ethertype);
                  printf(" vif=%u sta=%u\n", priv->vif_idx, priv->ap_idx);
                  wifi_test_rx_free(rx);
                  return 0;
                }

              if (!g_wifi_test_quiet && data_frames <= 4)
                {
                  wifi_test_dump_data_frame(&msg);
                }
            }
          else
            {
              other_msgs++;
            }

          submsgs++;

          next = offset + wifi_test_rx_padded_len(&msg);
          if (next <= offset)
            {
              break;
            }

          offset = next;
        }

      if (!g_wifi_test_quiet && submsgs == 0)
        {
          wifi_test_dump_rx_packet(rx, rx_len);
        }
    }

  printf("wifi_test: connect ind timeout scan_results=%u data_frames=%u "
         "other_msgs=%u\n", scan_results, data_frames, other_msgs);
  wifi_test_rx_free(rx);
  return -ETIMEDOUT;
}

static void wifi_test_prepare_add_if(FAR uint8_t *req)
{
  memset(req, 0, WIFI_TEST_MM_ADD_IF_LEN);
  req[0] = 0; /* VIF_STA */
  req[2] = g_wifi_test_mac[0];
  req[3] = g_wifi_test_mac[1];
  req[4] = g_wifi_test_mac[2];
  req[5] = g_wifi_test_mac[3];
  req[6] = g_wifi_test_mac[4];
  req[7] = g_wifi_test_mac[5];
  req[8] = 0; /* p2p */
}

static int wifi_test_add_if(FAR uint8_t *vif_idx)
{
  uint8_t req[WIFI_TEST_MM_ADD_IF_LEN];
  uint8_t cfm[2];
  unsigned int cfm_len = sizeof(cfm);
  int ret;

  ret = wifi_test_enable();
  if (ret != 0)
    {
      return ret;
    }

  wifi_test_prepare_add_if(req);
  printf("wifi_test: send MM_ADD_IF_REQ sta mac=%02x:%02x:%02x:%02x:%02x:%02x "
         "len=%u\n",
         g_wifi_test_mac[0], g_wifi_test_mac[1], g_wifi_test_mac[2],
         g_wifi_test_mac[3], g_wifi_test_mac[4], g_wifi_test_mac[5],
         WIFI_TEST_MM_ADD_IF_LEN);

  ret = wifi_test_send_msg(WIFI_TEST_MM_ADD_IF_REQ, WIFI_TEST_TASK_MM,
                           sizeof(req), req, WIFI_TEST_MM_ADD_IF_CFM,
                           cfm, &cfm_len, !g_wifi_test_quiet,
                           WIFI_TEST_MSG_POLL_RETRIES);
  if (ret < 0)
    {
      printf("wifi_test: MM_ADD_IF_REQ failed: %s (%d)\n",
             strerror(-ret), ret);
      return ret;
    }

  if (cfm_len < sizeof(cfm))
    {
      printf("wifi_test: MM_ADD_IF_CFM too short: %u\n", cfm_len);
      return -EPROTO;
    }

  if (!g_wifi_test_quiet)
    {
      printf("wifi_test: add if status=%u vif=%u\n", cfm[0], cfm[1]);
    }
  if (cfm[0] != 0)
    {
      return -EIO;
    }

  if (vif_idx != NULL)
    {
      *vif_idx = cfm[1];
    }

  return 0;
}

static void wifi_test_prepare_scanu_start(FAR uint8_t *req, uint8_t vif_idx,
                                          uint16_t freq)
{
  unsigned int i;

  memset(req, 0, WIFI_TEST_SCANU_START_LEN);
  wifi_test_put_chan_def(&req[0], freq, 0, 0, 20);

  for (i = 0; i < 6; i++)
    {
      req[WIFI_TEST_SCANU_BSSID_OFF + i] = 0xff;
    }

  wifi_test_put_le32(&req[WIFI_TEST_SCANU_ADD_IES_OFF], 0);
  wifi_test_put_le16(&req[WIFI_TEST_SCANU_ADD_IE_LEN_OFF], 0);
  req[WIFI_TEST_SCANU_VIF_IDX_OFF] = vif_idx;
  req[WIFI_TEST_SCANU_CHAN_CNT_OFF] = 1;
  req[WIFI_TEST_SCANU_SSID_CNT_OFF] = 0;       /* wildcard SSID */
  req[WIFI_TEST_SCANU_NO_CCK_OFF] = 0;
  wifi_test_put_le32(&req[WIFI_TEST_SCANU_DURATION_OFF], 0);
}

static int wifi_test_scan(uint16_t freq, bool add_if)
{
  uint8_t req[WIFI_TEST_SCANU_START_LEN];
  uint8_t cfm[4];
  unsigned int cfm_len = sizeof(cfm);
  uint8_t vif_idx = 0;
  int ret;

  if (add_if)
    {
      ret = wifi_test_add_if(&vif_idx);
      if (ret != 0)
        {
          return ret < 0 ? 1 : ret;
        }
    }
  else
    {
      ret = wifi_test_enable();
      if (ret != 0)
        {
          return ret;
        }
    }

  wifi_test_prepare_scanu_start(req, vif_idx, freq);
  printf("wifi_test: send SCANU_START_REQ vif=%u freq=%u len=%u\n",
         vif_idx, freq, WIFI_TEST_SCANU_START_LEN);

  ret = wifi_test_send_msg(WIFI_TEST_SCANU_START_REQ, WIFI_TEST_TASK_SCANU,
                           WIFI_TEST_SCANU_START_LEN, req,
                           WIFI_TEST_SCANU_START_CFM_ADDITIONAL,
                           cfm, &cfm_len, true,
                           WIFI_TEST_LONG_MSG_POLL_RETRIES);
  if (ret < 0)
    {
      printf("wifi_test: SCANU_START_REQ failed: %s (%d)\n",
             strerror(-ret), ret);
      return 1;
    }

  if (cfm_len >= 3)
    {
      printf("wifi_test: scan start additional cfm vif=%u status=%u "
             "result_cnt=%u\n",
             cfm[0], cfm[1], cfm[2]);
    }
  else
    {
      printf("wifi_test: scan start additional cfm len=%u\n", cfm_len);
    }

  return 0;
}

static uint16_t wifi_test_scan_freq_from_arg(FAR const char *arg)
{
  unsigned long channel;

  if (arg == NULL)
    {
      return 2437;
    }

  channel = strtoul(arg, NULL, 0);
  switch (channel)
    {
      case 1:
        return 2412;
      case 6:
        return 2437;
      case 11:
        return 2462;
      default:
        return (uint16_t)channel;
    }
}

static int wifi_test_parse_hex_nibble(char ch)
{
  if (ch >= '0' && ch <= '9')
    {
      return ch - '0';
    }

  if (ch >= 'a' && ch <= 'f')
    {
      return ch - 'a' + 10;
    }

  if (ch >= 'A' && ch <= 'F')
    {
      return ch - 'A' + 10;
    }

  return -EINVAL;
}

static int wifi_test_parse_mac(FAR const char *text, FAR uint8_t *mac)
{
  int hi;
  int lo;
  int i;

  if (text == NULL || mac == NULL)
    {
      return -EINVAL;
    }

  for (i = 0; i < 6; i++)
    {
      hi = wifi_test_parse_hex_nibble(text[0]);
      lo = wifi_test_parse_hex_nibble(text[1]);
      if (hi < 0 || lo < 0)
        {
          return -EINVAL;
        }

      mac[i] = (uint8_t)((hi << 4) | lo);
      text += 2;

      if (i < 5)
        {
          if (*text != ':')
            {
              return -EINVAL;
            }

          text++;
        }
    }

  return *text == '\0' ? 0 : -EINVAL;
}

static void wifi_test_put_mac_addr(FAR uint8_t *buffer,
                                   FAR const uint8_t *mac)
{
  wifi_test_put_le16(&buffer[0], (uint16_t)mac[0] |
                                ((uint16_t)mac[1] << 8));
  wifi_test_put_le16(&buffer[2], (uint16_t)mac[2] |
                                ((uint16_t)mac[3] << 8));
  wifi_test_put_le16(&buffer[4], (uint16_t)mac[4] |
                                ((uint16_t)mac[5] << 8));
}

static void wifi_test_dump_connect_ind(FAR const uint8_t *param,
                                       unsigned int param_len)
{
  uint16_t status;
  uint16_t assoc_req_ie_len;
  uint16_t assoc_rsp_ie_len;

  if (param_len < WIFI_TEST_SM_CONNECT_IND_MIN_LEN)
    {
      printf("  sm_connect_ind len=%u\n", param_len);
      return;
    }

  status = wifi_test_get_le16(&param[0]);
  assoc_req_ie_len = wifi_test_get_le16(&param[14]);
  assoc_rsp_ie_len = wifi_test_get_le16(&param[16]);
  printf("  sm_connect_ind status=%u bssid=%02x:%02x:%02x:%02x:%02x:%02x "
         "roamed=%u vif=%u ap_idx=%u ch_idx=%u qos=%u acm=0x%02x\n",
         status, param[2], param[3], param[4],
         param[5], param[6], param[7], param[8], param[9], param[10],
         param[11], param[12], param[13]);
  printf("  assoc_ie req_len=%u rsp_len=%u\n",
         assoc_req_ie_len, assoc_rsp_ie_len);

  if (param_len >= WIFI_TEST_SM_CONNECT_IND_FREQ2_OFF + 4)
    {
      printf("  aid=%u band=%u center_freq=%u width=%u "
             "center_freq1=%lu center_freq2=%lu\n",
             wifi_test_get_le16(&param[WIFI_TEST_SM_CONNECT_IND_AID_OFF]),
             param[WIFI_TEST_SM_CONNECT_IND_BAND_OFF],
             wifi_test_get_le16(&param[WIFI_TEST_SM_CONNECT_IND_FREQ_OFF]),
             param[WIFI_TEST_SM_CONNECT_IND_WIDTH_OFF],
             (unsigned long)wifi_test_get_le32(
               &param[WIFI_TEST_SM_CONNECT_IND_FREQ1_OFF]),
             (unsigned long)wifi_test_get_le32(
               &param[WIFI_TEST_SM_CONNECT_IND_FREQ2_OFF]));
    }
  else
    {
      printf("  sm_connect_ind missing channel tail len=%u need=%u\n",
             param_len, WIFI_TEST_SM_CONNECT_IND_FREQ2_OFF + 4);
    }
}

static void wifi_test_prepare_sm_connect(FAR uint8_t *req,
                                         FAR const char *ssid,
                                         FAR const uint8_t *bssid,
                                         uint16_t freq,
                                         uint8_t vif_idx)
{
  size_t ssid_len;

  memset(req, 0, WIFI_TEST_SM_CONNECT_REQ_LEN);

  ssid_len = strlen(ssid);
  if (ssid_len > 32)
    {
      ssid_len = 32;
    }

  req[WIFI_TEST_SM_CONNECT_SSID_OFF] = (uint8_t)ssid_len;
  memcpy(&req[WIFI_TEST_SM_CONNECT_SSID_OFF + 1], ssid, ssid_len);
  wifi_test_put_mac_addr(&req[WIFI_TEST_SM_CONNECT_BSSID_OFF], bssid);
  wifi_test_put_chan_def(&req[WIFI_TEST_SM_CONNECT_CHAN_OFF], freq, 0, 0, 20);
  wifi_test_put_le32(&req[WIFI_TEST_SM_CONNECT_FLAGS_OFF], 0);
  req[WIFI_TEST_SM_CONNECT_CTRL_PORT_OFF] =
    (uint8_t)(WIFI_TEST_OPEN_CTRL_PORT_ETHERTYPE >> 8);
  req[WIFI_TEST_SM_CONNECT_CTRL_PORT_OFF + 1] =
    (uint8_t)WIFI_TEST_OPEN_CTRL_PORT_ETHERTYPE;
  wifi_test_put_le16(&req[WIFI_TEST_SM_CONNECT_IE_LEN_OFF], 0);
  wifi_test_put_le16(&req[WIFI_TEST_SM_CONNECT_LISTEN_OFF], 10);
  req[WIFI_TEST_SM_CONNECT_DONT_WAIT_OFF] = 0;
  req[WIFI_TEST_SM_CONNECT_AUTH_OFF] = WIFI_TEST_AUTH_OPEN;
  req[WIFI_TEST_SM_CONNECT_UAPSD_OFF] = 0;
  req[WIFI_TEST_SM_CONNECT_VIF_OFF] = vif_idx;
}

static int wifi_test_connect_open(FAR const char *ssid,
                                  FAR const uint8_t *bssid,
                                  uint16_t freq)
{
  FAR struct wifi_test_netdev_s *priv = &g_wifi_test_netdev;
  uint8_t req[WIFI_TEST_SM_CONNECT_REQ_LEN];
  uint8_t cfm[WIFI_TEST_SM_CONNECT_CFM_LEN];
  unsigned int cfm_len = sizeof(cfm);
  uint8_t vif_idx;
  int ret;

  ret = wifi_test_auto_init(false);
  if (ret != 0)
    {
      return ret < 0 ? 1 : ret;
    }

  if (priv->connected)
    {
      printf("wifi_test: %s already connected\n", priv->dev.d_ifname);
      netdev_carrier_on(&priv->dev);
      ret = wifi_test_rx_thread_start();
      return ret < 0 ? 1 : 0;
    }

  if (priv->vif_added)
    {
      vif_idx = priv->vif_idx;
    }
  else
    {
      ret = wifi_test_add_if(&vif_idx);
      if (ret != 0)
        {
          return ret < 0 ? 1 : ret;
        }

      priv->vif_idx = vif_idx;
      priv->vif_added = true;
    }

  wifi_test_prepare_sm_connect(req, ssid, bssid, freq, vif_idx);
  printf("wifi_test: connecting %s\n", ssid);

  ret = wifi_test_send_msg(WIFI_TEST_SM_CONNECT_REQ, WIFI_TEST_TASK_SM,
                           WIFI_TEST_SM_CONNECT_REQ_LEN, req,
                           WIFI_TEST_SM_CONNECT_CFM, cfm, &cfm_len,
                           !g_wifi_test_quiet,
                           WIFI_TEST_LONG_MSG_POLL_RETRIES);
  if (ret < 0)
    {
      printf("wifi_test: SM_CONNECT_REQ failed: %s (%d)\n",
             strerror(-ret), ret);
      return 1;
    }

  if (cfm_len < sizeof(cfm))
    {
      printf("wifi_test: SM_CONNECT_CFM too short: %u\n", cfm_len);
      return 1;
    }

  if (cfm[0] != 0)
    {
      printf("wifi_test: connect cfm status=%u\n", cfm[0]);
      return 1;
    }

  ret = wifi_test_wait_connect_ind();
  if (ret < 0)
    {
      return 1;
    }

  priv->ifup = true;
  priv->dev.d_flags |= IFF_UP;
  netdev_carrier_on(&priv->dev);
  ret = wifi_test_rx_thread_start();
  if (ret < 0)
    {
      return 1;
    }

  printf("wifi_test: %s connected\n", priv->dev.d_ifname);
  return 0;
}

static int wifi_test_memtest(void)
{
  static const uint8_t pattern[16] =
  {
    0x78, 0x56, 0x34, 0x12,
    0xef, 0xcd, 0xab, 0x90,
    0x55, 0xaa, 0x00, 0xff,
    0x5a, 0xa5, 0xc3, 0x3c
  };
  uint32_t value;
  int ret;

  ret = wifi_test_enable();
  if (ret != 0)
    {
      return ret;
    }

  printf("wifi_test: write %u bytes to 0x%08lx\n",
         (unsigned int)sizeof(pattern),
         (unsigned long)WIFI_TEST_RAM_FMAC_FW_ADDR);

  ret = wifi_test_dbg_mem_block_write(WIFI_TEST_RAM_FMAC_FW_ADDR,
                                      pattern, sizeof(pattern), true);
  if (ret < 0)
    {
      printf("wifi_test: DBG_MEM_BLOCK_WRITE failed: %s (%d)\n",
             strerror(-ret), ret);
      return 1;
    }

  ret = wifi_test_dbg_mem_read(WIFI_TEST_RAM_FMAC_FW_ADDR, &value);
  if (ret < 0)
    {
      printf("wifi_test: DBG_MEM_READ verify failed: %s (%d)\n",
             strerror(-ret), ret);
      return 1;
    }

  if (value != wifi_test_get_le32(pattern))
    {
      printf("wifi_test: memtest mismatch expected=0x%08lx got=0x%08lx\n",
             (unsigned long)wifi_test_get_le32(pattern),
             (unsigned long)value);
      return 1;
    }

  printf("wifi_test: DBG_MEM_BLOCK_WRITE/READ memtest ok\n");
  return 0;
}

static int wifi_test_fwload(void)
{
  FAR const uint8_t *fw = (FAR const uint8_t *)fmacfw_8800d80_u02;
  const uint32_t fw_addr = WIFI_TEST_RAM_FMAC_FW_ADDR;
  const uint32_t fw_size = sizeof(fmacfw_8800d80_u02);
  uint32_t offset = 0;
  uint32_t chunk;
  uint32_t value;
  int ret;

  ret = wifi_test_enable();
  if (ret != 0)
    {
      return ret;
    }

  if (!g_wifi_test_quiet)
    {
      printf("wifi_test: upload fmacfw_8800d80_u02 size=%lu to 0x%08lx\n",
             (unsigned long)fw_size, (unsigned long)fw_addr);
    }

  while (offset < fw_size)
    {
      chunk = fw_size - offset;
      if (chunk > WIFI_TEST_FW_BLOCK_SIZE)
        {
          chunk = WIFI_TEST_FW_BLOCK_SIZE;
        }

      ret = wifi_test_dbg_mem_block_write(fw_addr + offset,
                                          &fw[offset], chunk, false);
      if (ret < 0)
        {
          printf("wifi_test: firmware upload failed at offset 0x%08lx: "
                 "%s (%d)\n",
                 (unsigned long)offset, strerror(-ret), ret);
          return 1;
        }

      offset += chunk;
      if (!g_wifi_test_quiet &&
          ((offset % WIFI_TEST_FW_PROGRESS_STEP) == 0 || offset == fw_size))
        {
          printf("wifi_test: firmware upload %lu/%lu bytes\n",
                 (unsigned long)offset, (unsigned long)fw_size);
        }
    }

  ret = wifi_test_dbg_mem_read(fw_addr, &value);
  if (ret < 0)
    {
      printf("wifi_test: firmware readback failed: %s (%d)\n",
             strerror(-ret), ret);
      return 1;
    }

  if (value != fmacfw_8800d80_u02[0])
    {
      printf("wifi_test: firmware first word mismatch expected=0x%08lx "
             "got=0x%08lx\n",
             (unsigned long)fmacfw_8800d80_u02[0], (unsigned long)value);
      return 1;
    }

  if (!g_wifi_test_quiet)
    {
      printf("wifi_test: start app at 0x%08lx\n", (unsigned long)fw_addr);
    }
  ret = wifi_test_dbg_start_app(fw_addr, WIFI_TEST_HOST_START_APP_AUTO);
  if (ret < 0)
    {
      printf("wifi_test: DBG_START_APP failed: %s (%d)\n",
             strerror(-ret), ret);
      return 1;
    }

  if (!g_wifi_test_quiet)
    {
      printf("wifi_test: firmware upload/start ok\n");
    }

  return 0;
}

static int wifi_test_fwstate(void)
{
  uint32_t value;
  int ret;

  ret = wifi_test_enable();
  if (ret != 0)
    {
      return ret;
    }

  ret = wifi_test_dbg_mem_read(0x40500004, &value);
  if (ret < 0)
    {
      printf("wifi_test: fwstate read failed: %s (%d)\n",
             strerror(-ret), ret);
      return 1;
    }

  printf("wifi_test: fw_state=%lu raw=0x%08lx\n",
         (unsigned long)((value >> 4) & 0x01),
         (unsigned long)value);
  return 0;
}

static int wifi_test_mm_version(bool enable)
{
  uint8_t cfm[32];
  unsigned int cfm_len = sizeof(cfm);
  uint16_t max_sta;
  int ret;

  if (enable)
    {
      ret = wifi_test_enable();
      if (ret != 0)
        {
          return ret;
        }
    }

  ret = wifi_test_send_msg(WIFI_TEST_MM_VERSION_REQ, WIFI_TEST_TASK_MM,
                           0, NULL, WIFI_TEST_MM_VERSION_CFM, cfm,
                           &cfm_len, !g_wifi_test_quiet,
                           WIFI_TEST_MSG_POLL_RETRIES);
  if (ret < 0)
    {
      printf("wifi_test: MM_VERSION_REQ failed: %s (%d)\n",
             strerror(-ret), ret);
      return 1;
    }

  if (cfm_len < 27)
    {
      printf("wifi_test: MM_VERSION_CFM too short: %u\n", cfm_len);
      return 1;
    }

  max_sta = wifi_test_get_le16(&cfm[24]);
  if (!g_wifi_test_quiet)
    {
      printf("wifi_test: version lmac=0x%08lx machw=0x%08lx/0x%08lx "
             "phy=0x%08lx/0x%08lx features=0x%08lx max_sta=%u max_vif=%u\n",
             (unsigned long)wifi_test_get_le32(&cfm[0]),
             (unsigned long)wifi_test_get_le32(&cfm[4]),
             (unsigned long)wifi_test_get_le32(&cfm[8]),
             (unsigned long)wifi_test_get_le32(&cfm[12]),
             (unsigned long)wifi_test_get_le32(&cfm[16]),
             (unsigned long)wifi_test_get_le32(&cfm[20]),
             max_sta, cfm[26]);
    }

  return 0;
}

static int wifi_test_version(void)
{
  return wifi_test_mm_version(true);
}

static int wifi_test_mm_reset(bool verbose)
{
  unsigned int cfm_len = 0;
  int ret;

  if (verbose)
    {
      printf("wifi_test: send MM_RESET_REQ\n");
    }

  ret = wifi_test_send_msg(WIFI_TEST_MM_RESET_REQ, WIFI_TEST_TASK_MM,
                           0, NULL, WIFI_TEST_MM_RESET_CFM, NULL,
                           &cfm_len, verbose, WIFI_TEST_MSG_POLL_RETRIES);
  if (ret < 0)
    {
      printf("wifi_test: MM_RESET_REQ failed: %s (%d)\n",
             strerror(-ret), ret);
      return ret;
    }

  if (verbose)
    {
      printf("wifi_test: MM_RESET_REQ ok\n");
    }

  return 0;
}

static int wifi_test_initcmd(void)
{
  int ret;

  ret = wifi_test_enable();
  if (ret != 0)
    {
      return ret;
    }

  ret = wifi_test_mm_reset(true);
  if (ret < 0)
    {
      return 1;
    }

  return wifi_test_mm_version(false);
}

static int wifi_test_mm_stackstart(bool enable, bool verbose)
{
  uint8_t req[4];
  uint8_t cfm[4];
  unsigned int cfm_len = sizeof(cfm);
  int ret;

  if (enable)
    {
      ret = wifi_test_enable();
      if (ret != 0)
        {
          return ret;
        }
    }

  req[0] = 1;    /* is_stack_start */
  req[1] = 0;    /* efuse_valid */
  req[2] = 1 << 5; /* set_vendor_info: CO_BIT(5), as in D80 path */
  req[3] = 0;    /* fwtrace_redir */

  if (verbose)
    {
      printf("wifi_test: send MM_SET_STACK_START_REQ\n");
    }

  ret = wifi_test_send_msg(WIFI_TEST_MM_SET_STACK_START_REQ,
                           WIFI_TEST_TASK_MM, sizeof(req), req,
                           WIFI_TEST_MM_SET_STACK_START_CFM, cfm,
                           &cfm_len, verbose, WIFI_TEST_MSG_POLL_RETRIES);
  if (ret < 0)
    {
      printf("wifi_test: MM_SET_STACK_START_REQ failed: %s (%d)\n",
             strerror(-ret), ret);
      return ret;
    }

  if (cfm_len < 2)
    {
      printf("wifi_test: MM_SET_STACK_START_CFM too short: %u\n", cfm_len);
      return -EPROTO;
    }

  if (!g_wifi_test_quiet)
    {
      printf("wifi_test: stack start ok is_5g_support=%u vendor_info=%u\n",
             cfm[0], cfm[1]);
    }

  return 0;
}

static int wifi_test_stackstart(void)
{
  int ret;

  ret = wifi_test_mm_stackstart(true, true);
  return ret < 0 ? 1 : 0;
}

static int wifi_test_mm_rfcalib(bool enable, bool verbose)
{
  uint8_t req[24];
  uint8_t cfm[16];
  unsigned int cfm_len = sizeof(cfm);
  int ret;

  if (enable)
    {
      ret = wifi_test_enable();
      if (ret != 0)
        {
          return ret;
        }
    }

  memset(req, 0, sizeof(req));
  wifi_test_put_le32(&req[0], 0x00000f8f);
  wifi_test_put_le32(&req[4], 0x00000f0f);
  wifi_test_put_le32(&req[8], 0x0c34c008);
  wifi_test_put_le32(&req[12], 0);
  wifi_test_put_le32(&req[16], 0x00264203);
  req[20] = 0;
  req[21] = 0;

  if (verbose)
    {
      printf("wifi_test: send MM_SET_RF_CALIB_REQ\n");
    }

  ret = wifi_test_send_msg(WIFI_TEST_MM_SET_RF_CALIB_REQ,
                           WIFI_TEST_TASK_MM, sizeof(req), req,
                           WIFI_TEST_MM_SET_RF_CALIB_CFM, cfm,
                           &cfm_len, verbose,
                           WIFI_TEST_LONG_MSG_POLL_RETRIES);
  if (ret < 0)
    {
      printf("wifi_test: MM_SET_RF_CALIB_REQ failed: %s (%d)\n",
             strerror(-ret), ret);
      return ret;
    }

  if (cfm_len < 16)
    {
      printf("wifi_test: MM_SET_RF_CALIB_CFM too short: %u\n", cfm_len);
      return -EPROTO;
    }

  if (!g_wifi_test_quiet)
    {
      printf("wifi_test: rf calib ok rxgain24=0x%08lx rxgain5=0x%08lx "
             "txgain24=0x%08lx txgain5=0x%08lx\n",
             (unsigned long)wifi_test_get_le32(&cfm[0]),
             (unsigned long)wifi_test_get_le32(&cfm[4]),
             (unsigned long)wifi_test_get_le32(&cfm[8]),
             (unsigned long)wifi_test_get_le32(&cfm[12]));
    }

  return 0;
}

static int wifi_test_rfcalib(void)
{
  int ret;

  ret = wifi_test_mm_rfcalib(true, true);
  return ret < 0 ? 1 : 0;
}

static int wifi_test_rfchain(void)
{
  int ret;

  ret = wifi_test_enable();
  if (ret != 0)
    {
      return ret;
    }

  ret = wifi_test_mm_stackstart(false, true);
  if (ret < 0)
    {
      return 1;
    }

  usleep(WIFI_TEST_POST_STACK_DELAY_US);

  ret = wifi_test_mm_rfcalib(false, true);
  if (ret < 0)
    {
      return 1;
    }

  printf("wifi_test: rf chain ok\n");
  return 0;
}

static void wifi_test_put_chan_def(FAR uint8_t *buffer, uint16_t freq,
                                   uint8_t band, uint8_t flags,
                                   int8_t tx_power)
{
  wifi_test_put_le16(&buffer[0], freq);
  buffer[2] = band;
  buffer[3] = flags;
  buffer[4] = (uint8_t)tx_power;
  buffer[5] = 0;
}

static void wifi_test_prepare_me_config(FAR uint8_t *req)
{
  memset(req, 0, WIFI_TEST_ME_CONFIG_LEN);

  /* Offset after HT/VHT/HE capabilities. Keep capabilities disabled for the
   * first accepted-configuration checkpoint.
   */

  wifi_test_put_le16(&req[104], 512); /* tx_lft */
  req[106] = 2;                       /* PHY_CHNL_BW_80 */
  req[107] = 0;                       /* ht_supp */
  req[108] = 0;                       /* vht_supp */
  req[109] = 0;                       /* he_supp */
  req[110] = 0;                       /* he_ul_on */
  req[111] = 0;                       /* ps_on */
}

static void wifi_test_prepare_chan_config(FAR uint8_t *req)
{
  static const uint16_t chan2g_freq[] =
  {
    2412, 2417, 2422, 2427, 2432, 2437, 2442,
    2447, 2452, 2457, 2462, 2467, 2472
  };
  static const uint16_t chan5g_freq[] =
  {
    5180, 5200, 5220, 5240
  };
  unsigned int i;

  memset(req, 0, WIFI_TEST_ME_CHAN_CONFIG_LEN);

  for (i = 0; i < sizeof(chan2g_freq) / sizeof(chan2g_freq[0]); i++)
    {
      wifi_test_put_chan_def(&req[i * 6], chan2g_freq[i], 0, 0, 20);
    }

  for (i = 0; i < sizeof(chan5g_freq) / sizeof(chan5g_freq[0]); i++)
    {
      wifi_test_put_chan_def(&req[84 + i * 6], chan5g_freq[i], 1, 0, 20);
    }

  req[252] = sizeof(chan2g_freq) / sizeof(chan2g_freq[0]);
  req[253] = sizeof(chan5g_freq) / sizeof(chan5g_freq[0]);
}

static void wifi_test_prepare_mm_start(FAR uint8_t *req)
{
  memset(req, 0, WIFI_TEST_MM_START_LEN);
}

static int wifi_test_mebasic(void)
{
  uint8_t req[WIFI_TEST_ME_CHAN_CONFIG_LEN];
  unsigned int cfm_len = 0;
  int ret;

  ret = wifi_test_enable();
  if (ret != 0)
    {
      return ret;
    }

  wifi_test_prepare_me_config(req);
  if (!g_wifi_test_quiet)
    {
      printf("wifi_test: send ME_CONFIG_REQ len=%u\n",
             WIFI_TEST_ME_CONFIG_LEN);
    }

  ret = wifi_test_send_msg(WIFI_TEST_ME_CONFIG_REQ, WIFI_TEST_TASK_ME,
                           WIFI_TEST_ME_CONFIG_LEN, req,
                           WIFI_TEST_ME_CONFIG_CFM, NULL, &cfm_len,
                           !g_wifi_test_quiet, WIFI_TEST_MSG_POLL_RETRIES);
  if (ret < 0)
    {
      printf("wifi_test: ME_CONFIG_REQ failed: %s (%d)\n",
             strerror(-ret), ret);
      return 1;
    }

  wifi_test_prepare_chan_config(req);
  if (!g_wifi_test_quiet)
    {
      printf("wifi_test: send ME_CHAN_CONFIG_REQ len=%u\n",
             WIFI_TEST_ME_CHAN_CONFIG_LEN);
    }

  ret = wifi_test_send_msg(WIFI_TEST_ME_CHAN_CONFIG_REQ, WIFI_TEST_TASK_ME,
                           WIFI_TEST_ME_CHAN_CONFIG_LEN, req,
                           WIFI_TEST_ME_CHAN_CONFIG_CFM, NULL, &cfm_len,
                           !g_wifi_test_quiet, WIFI_TEST_MSG_POLL_RETRIES);
  if (ret < 0)
    {
      printf("wifi_test: ME_CHAN_CONFIG_REQ failed: %s (%d)\n",
             strerror(-ret), ret);
      return 1;
    }

  if (!g_wifi_test_quiet)
    {
      printf("wifi_test: ME basic config ok\n");
    }

  return 0;
}

static int wifi_test_macstart(void)
{
  uint8_t req[WIFI_TEST_MM_START_LEN];
  unsigned int cfm_len = 0;
  int ret;

  ret = wifi_test_enable();
  if (ret != 0)
    {
      return ret;
    }

  wifi_test_prepare_mm_start(req);
  if (!g_wifi_test_quiet)
    {
      printf("wifi_test: send MM_START_REQ len=%u\n", WIFI_TEST_MM_START_LEN);
    }

  ret = wifi_test_send_msg(WIFI_TEST_MM_START_REQ, WIFI_TEST_TASK_MM,
                           WIFI_TEST_MM_START_LEN, req,
                           WIFI_TEST_MM_START_CFM, NULL, &cfm_len,
                           !g_wifi_test_quiet, WIFI_TEST_MSG_POLL_RETRIES);
  if (ret < 0)
    {
      printf("wifi_test: MM_START_REQ failed: %s (%d)\n",
             strerror(-ret), ret);
      return 1;
    }

  if (!g_wifi_test_quiet)
    {
      printf("wifi_test: MM start ok\n");
    }

  return 0;
}

static int wifi_test_bringup(void)
{
  int ret;

  ret = wifi_test_enable();
  if (ret != 0)
    {
      return ret;
    }

  ret = wifi_test_mm_reset(false);
  if (ret < 0)
    {
      return 1;
    }

  ret = wifi_test_mm_version(false);
  if (ret != 0)
    {
      return ret;
    }

  ret = wifi_test_mm_stackstart(false, false);
  if (ret < 0)
    {
      return 1;
    }

  usleep(WIFI_TEST_POST_STACK_DELAY_US);

  ret = wifi_test_mm_rfcalib(false, false);
  if (ret < 0)
    {
      return 1;
    }

  ret = wifi_test_mebasic();
  if (ret != 0)
    {
      return ret;
    }

  ret = wifi_test_macstart();
  if (ret != 0)
    {
      return ret;
    }

  g_wifi_test_brought_up = true;
  if (!g_wifi_test_quiet)
    {
      printf("wifi_test: bringup checkpoint ok\n");
    }

  return 0;
}

static int wifi_test_auto_init(bool verbose)
{
  bool old_quiet = g_wifi_test_quiet;
  int ret;

  g_wifi_test_quiet = !verbose;

  if (!g_wifi_test_fw_loaded)
    {
      ret = wifi_test_fwload();
      if (ret != 0)
        {
          g_wifi_test_quiet = old_quiet;
          return ret;
        }

      g_wifi_test_fw_loaded = true;
    }

  if (!g_wifi_test_brought_up)
    {
      ret = wifi_test_bringup();
      if (ret != 0)
        {
          g_wifi_test_quiet = old_quiet;
          return ret;
        }
    }

  ret = wifi_test_netreg();
  if (ret == 0 && verbose)
    {
      printf("wifi_test: auto init ok\n");
    }

  g_wifi_test_quiet = old_quiet;
  return ret;
}

int wifi_test_auto_start(void)
{
  return wifi_test_auto_init(false);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int ret;

  if (argc == 2 && strcmp(argv[1], "probe") == 0)
    {
      return wifi_test_probe();
    }

  if (argc == 2 && strcmp(argv[1], "cccr") == 0)
    {
      return wifi_test_cccr();
    }

  if (argc == 2 && strcmp(argv[1], "cis") == 0)
    {
      return wifi_test_cis();
    }

  if (argc == 2 && strcmp(argv[1], "enable") == 0)
    {
      return wifi_test_enable();
    }

  if (argc == 2 && strcmp(argv[1], "cmd53") == 0)
    {
      return wifi_test_cmd53();
    }

  if (argc == 2 && strcmp(argv[1], "msg") == 0)
    {
      return wifi_test_msg();
    }

  if (argc == 2 && strcmp(argv[1], "memtest") == 0)
    {
      return wifi_test_memtest();
    }

  if (argc == 2 && strcmp(argv[1], "fwload") == 0)
    {
      return wifi_test_fwload();
    }

  if (argc == 2 && strcmp(argv[1], "fwstate") == 0)
    {
      return wifi_test_fwstate();
    }

  if (argc == 2 && strcmp(argv[1], "version") == 0)
    {
      return wifi_test_version();
    }

  if (argc == 2 && strcmp(argv[1], "initcmd") == 0)
    {
      return wifi_test_initcmd();
    }

  if (argc == 2 && strcmp(argv[1], "stackstart") == 0)
    {
      return wifi_test_stackstart();
    }

  if (argc == 2 && strcmp(argv[1], "rfcalib") == 0)
    {
      return wifi_test_rfcalib();
    }

  if (argc == 2 && strcmp(argv[1], "rfchain") == 0)
    {
      return wifi_test_rfchain();
    }

  if (argc == 2 && strcmp(argv[1], "mebasic") == 0)
    {
      return wifi_test_mebasic();
    }

  if (argc == 2 && strcmp(argv[1], "macstart") == 0)
    {
      return wifi_test_macstart();
    }

  if (argc == 2 && strcmp(argv[1], "bringup") == 0)
    {
      return wifi_test_bringup();
    }

  if (argc == 2 && strcmp(argv[1], "addif") == 0)
    {
      ret = wifi_test_add_if(NULL);
      return ret < 0 ? 1 : ret;
    }

  if ((argc == 2 || argc == 3) && strcmp(argv[1], "scan") == 0)
    {
      return wifi_test_scan(wifi_test_scan_freq_from_arg(argc == 3 ?
                                                         argv[2] : NULL),
                            false);
    }

  if ((argc >= 2 && argc <= 4) && strcmp(argv[1], "scanpoll") == 0)
    {
      unsigned int count = 30;

      ret = wifi_test_scan(wifi_test_scan_freq_from_arg(argc >= 3 ?
                                                        argv[2] : NULL),
                           true);
      if (ret != 0)
        {
          return ret;
        }

      if (argc == 4)
        {
          count = (unsigned int)strtoul(argv[3], NULL, 0);
          if (count == 0)
            {
              count = 1;
            }
        }

      return wifi_test_rxpoll(count);
    }

  if (argc == 2 && strcmp(argv[1], "joinportal") == 0)
    {
      static const uint8_t any_bssid[6] =
      {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff
      };

      return wifi_test_connect_open("BUPT-portal", any_bssid, 0xffff);
    }

  if (argc == 2 && strcmp(argv[1], "joinportalbssid") == 0)
    {
      static const uint8_t bupt_portal_bssid[6] =
      {
        0xb0, 0xb8, 0x67, 0x69, 0xf4, 0xa1
      };

      return wifi_test_connect_open("BUPT-portal", bupt_portal_bssid,
                                    0xffff);
    }

  if (argc == 5 && strcmp(argv[1], "connect") == 0)
    {
      uint8_t bssid[6];

      ret = wifi_test_parse_mac(argv[3], bssid);
      if (ret < 0)
        {
          printf("wifi_test: invalid bssid '%s'\n", argv[3]);
          return 1;
        }

      return wifi_test_connect_open(argv[2], bssid,
                                    (uint16_t)strtoul(argv[4], NULL, 0));
    }

  if (argc == 2 && strcmp(argv[1], "netreg") == 0)
    {
      return wifi_test_netreg();
    }

  if ((argc == 2 || argc == 3) && strcmp(argv[1], "rxpoll") == 0)
    {
      unsigned int count = 10;

      if (argc == 3)
        {
          count = (unsigned int)strtoul(argv[2], NULL, 0);
          if (count == 0)
            {
              count = 1;
            }
        }

      return wifi_test_rxpoll(count);
    }

  if ((argc == 2 || argc == 3) && strcmp(argv[1], "rxfeed") == 0)
    {
      unsigned int count = 30;

      if (argc == 3)
        {
          count = (unsigned int)strtoul(argv[2], NULL, 0);
          if (count == 0)
            {
              count = 1;
            }
        }

      return wifi_test_rxfeed(count);
    }

  if (argc == 3 && strcmp(argv[1], "power") == 0)
    {
      if (strcmp(argv[2], "on") == 0)
        {
          ret = d13x_sdmc0_wifi_power(true);
        }
      else if (strcmp(argv[2], "off") == 0)
        {
          ret = d13x_sdmc0_wifi_power(false);
        }
      else
        {
          wifi_test_usage(argv[0]);
          return 1;
        }

      if (ret < 0)
        {
          printf("wifi_test: power %s failed: %s (%d)\n",
                 argv[2], strerror(-ret), ret);
          return 1;
        }

      printf("wifi_test: power %s\n", argv[2]);
      return 0;
    }

  wifi_test_usage(argv[0]);
  return 1;
}
