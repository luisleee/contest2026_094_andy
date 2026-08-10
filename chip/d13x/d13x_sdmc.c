/****************************************************************************
 * contest2026_094_andy/chip/d13x/d13x_sdmc.c
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdbool.h>

#include <nuttx/mmcsd.h>
#include <nuttx/sdio.h>

#include "aic_sdio.h"
#include "d13x_sdmc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define D13X_SDMC1_SLOT  1
#define D13X_SDMC1_MINOR 1

/****************************************************************************
 * Private Data
 ****************************************************************************/

static FAR struct sdio_dev_s *g_sdmc1;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int d13x_sdmc1_initialize(void)
{
  int ret;

  if (g_sdmc1 != NULL)
    {
      return OK;
    }

  g_sdmc1 = sdio_initialize(D13X_SDMC1_SLOT);
  if (g_sdmc1 == NULL)
    {
      return -ENODEV;
    }

  /* The first milestone requires a card to be inserted at boot and does not
   * start the shared hotplug polling worker.
   */

  sdio_mediachange(g_sdmc1, true);
  ret = mmcsd_slotinitialize(D13X_SDMC1_MINOR, g_sdmc1);
  if (ret < 0)
    {
      g_sdmc1 = NULL;
      return ret;
    }

  return OK;
}
