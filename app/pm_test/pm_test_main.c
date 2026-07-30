/****************************************************************************
 * contest2026_094_andy/app/pm_test/pm_test_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <nuttx/video/fb.h>

#include <arch/board/board.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define PM_TEST_FB_DEVICE          "/dev/fb0"
#define PM_TEST_DEFAULT_SECONDS    30u
#define PM_TEST_MIN_SECONDS        5u
#define PM_TEST_MAX_SECONDS        300u

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int pm_test_parse_seconds(FAR const char *text,
                                 FAR uint32_t *seconds)
{
  FAR char *endptr;
  unsigned long value;

  errno = 0;
  value = strtoul(text, &endptr, 10);
  if (errno != 0 || *text == '\0' || *endptr != '\0' ||
      value < PM_TEST_MIN_SECONDS || value > PM_TEST_MAX_SECONDS)
    {
      return -EINVAL;
    }

  *seconds = value;
  return OK;
}

static uint64_t pm_test_milliseconds(void)
{
  struct timespec now;

  clock_gettime(CLOCK_MONOTONIC, &now);
  return (uint64_t)now.tv_sec * 1000u + now.tv_nsec / 1000000u;
}

static int pm_test_wake(uint32_t timeout_seconds)
{
  uint64_t start;
  uint64_t elapsed;
  int initial_power;
  int fbfd;
  int ret;
  int restore_ret;

  fbfd = open(PM_TEST_FB_DEVICE, O_RDWR);
  if (fbfd < 0)
    {
      printf("pm_test: open %s failed: %s\n",
             PM_TEST_FB_DEVICE, strerror(errno));
      return 1;
    }

  ret = ioctl(fbfd, FBIOGET_POWER,
              (unsigned long)((uintptr_t)&initial_power));
  if (ret < 0)
    {
      printf("pm_test: FBIOGET_POWER failed: %s\n", strerror(errno));
      close(fbfd);
      return 1;
    }

  if (initial_power <= 0)
    {
      printf("pm_test: display must be powered before the wake test\n");
      close(fbfd);
      return 1;
    }

  ret = d13x_wakeup_pm_arm();
  if (ret < 0)
    {
      printf("pm_test: WAKEUP arm failed: %s\n", strerror(-ret));
      close(fbfd);
      return 1;
    }

  printf("pm_test: phase-1 wake standby, timeout=%lu seconds\n",
         (unsigned long)timeout_seconds);
  printf("pm_test: display will turn off; press WAKEUP to resume\n");
  printf("pm_test: CPU/PLL clocks remain running in this milestone\n");
  fflush(stdout);

  ret = ioctl(fbfd, FBIOSET_POWER, 0);
  if (ret < 0)
    {
      printf("pm_test: display power-off failed: %s\n", strerror(errno));
      d13x_wakeup_pm_disarm();
      close(fbfd);
      return 1;
    }

  start = pm_test_milliseconds();
  ret = d13x_wakeup_pm_wait(timeout_seconds * 1000u);
  elapsed = pm_test_milliseconds() - start;
  d13x_wakeup_pm_disarm();

  restore_ret = ioctl(fbfd, FBIOSET_POWER, initial_power);
  close(fbfd);
  if (restore_ret < 0)
    {
      printf("pm_test: display restore failed: %s\n", strerror(errno));
      return 1;
    }

  if (ret < 0)
    {
      printf("pm_test: WAKEUP wait failed after %lu ms: %s\n",
             (unsigned long)elapsed, strerror(-ret));
      return 1;
    }

  printf("pm_test: WAKEUP resumed display after %lu ms\n",
         (unsigned long)elapsed);
  printf("pm_test: PASS; falling-edge wake and display restore verified\n");
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  uint32_t timeout_seconds = PM_TEST_DEFAULT_SECONDS;

  if (argc < 2 || argc > 3 || strcmp(argv[1], "wake") != 0 ||
      (argc == 3 &&
       pm_test_parse_seconds(argv[2], &timeout_seconds) < 0))
    {
      printf("Usage: pm_test wake [seconds 5..300]\n");
      return 1;
    }

  return pm_test_wake(timeout_seconds);
}
