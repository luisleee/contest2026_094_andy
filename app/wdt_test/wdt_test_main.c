/****************************************************************************
 * contest2026_094_andy/app/wdt_test/wdt_test_main.c
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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nuttx/timers/watchdog.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define WDT_TEST_DEVICE             "/dev/watchdog0"
#define WDT_TEST_FEED_TIMEOUT_MS    3000u
#define WDT_TEST_FEED_DEFAULT_SEC   8u
#define WDT_TEST_FEED_MIN_SEC       3u
#define WDT_TEST_FEED_MAX_SEC       60u
#define WDT_TEST_RESET_DEFAULT_SEC  5u
#define WDT_TEST_RESET_MIN_SEC      3u
#define WDT_TEST_RESET_MAX_SEC      30u

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int wdt_test_parse_seconds(FAR const char *text,
                                  uint32_t minimum, uint32_t maximum,
                                  FAR uint32_t *seconds)
{
  FAR char *endptr;
  unsigned long value;

  errno = 0;
  value = strtoul(text, &endptr, 10);
  if (errno != 0 || *text == '\0' || *endptr != '\0' ||
      value < minimum || value > maximum)
    {
      return -EINVAL;
    }

  *seconds = value;
  return OK;
}

static int wdt_test_ioctl(int fd, int command, unsigned long argument,
                          FAR const char *name)
{
  if (ioctl(fd, command, argument) < 0)
    {
      printf("wdt_test: %s failed: %s\n", name, strerror(errno));
      return -errno;
    }

  return OK;
}

static int wdt_test_status(int fd, FAR struct watchdog_status_s *status)
{
  return wdt_test_ioctl(fd, WDIOC_GETSTATUS,
                        (unsigned long)((uintptr_t)status),
                        "WDIOC_GETSTATUS");
}

static int wdt_test_feed(uint32_t duration_seconds)
{
  struct watchdog_status_s status;
  unsigned int feeds = 0;
  unsigned int elapsed;
  bool started = false;
  int fd;
  int ret;

  fd = open(WDT_TEST_DEVICE, O_RDONLY);
  if (fd < 0)
    {
      printf("wdt_test: open %s failed: %s\n",
             WDT_TEST_DEVICE, strerror(errno));
      return 1;
    }

  ret = wdt_test_ioctl(fd, WDIOC_SETTIMEOUT, WDT_TEST_FEED_TIMEOUT_MS,
                       "WDIOC_SETTIMEOUT");
  if (ret < 0)
    {
      goto errout;
    }

  ret = wdt_test_ioctl(fd, WDIOC_START, 0, "WDIOC_START");
  if (ret < 0)
    {
      goto errout;
    }

  started = true;
  printf("wdt_test: feed mode, timeout=3 seconds, duration=%lu seconds\n",
         (unsigned long)duration_seconds);

  for (elapsed = 0; elapsed < duration_seconds; elapsed++)
    {
      sleep(1);
      ret = wdt_test_ioctl(fd, WDIOC_KEEPALIVE, 0, "WDIOC_KEEPALIVE");
      if (ret < 0)
        {
          goto errout;
        }

      feeds++;
      ret = wdt_test_status(fd, &status);
      if (ret < 0)
        {
          goto errout;
        }

      printf("wdt_test: feed=%u active=%s timeleft=%lu ms\n",
             feeds,
             (status.flags & WDFLAGS_ACTIVE) != 0 ? "yes" : "no",
             (unsigned long)status.timeleft);
    }

  ret = wdt_test_ioctl(fd, WDIOC_STOP, 0, "WDIOC_STOP");
  if (ret < 0)
    {
      goto errout;
    }

  started = false;
  close(fd);
  printf("wdt_test: PASS; %u keepalives completed and watchdog stopped\n",
         feeds);
  return 0;

errout:
  if (started)
    {
      ioctl(fd, WDIOC_STOP, 0);
    }

  close(fd);
  return 1;
}

static int wdt_test_reset(uint32_t timeout_seconds)
{
  int fd;
  int ret;

  fd = open(WDT_TEST_DEVICE, O_RDONLY);
  if (fd < 0)
    {
      printf("wdt_test: open %s failed: %s\n",
             WDT_TEST_DEVICE, strerror(errno));
      return 1;
    }

  ret = wdt_test_ioctl(fd, WDIOC_SETTIMEOUT, timeout_seconds * 1000u,
                       "WDIOC_SETTIMEOUT");
  if (ret < 0)
    {
      close(fd);
      return 1;
    }

  printf("wdt_test: reset mode armed for %lu seconds\n",
         (unsigned long)timeout_seconds);
  printf("wdt_test: no keepalive will be sent; board should reboot\n");
  fflush(stdout);

  ret = wdt_test_ioctl(fd, WDIOC_START, 0, "WDIOC_START");
  if (ret < 0)
    {
      close(fd);
      return 1;
    }

  sleep(timeout_seconds + 3u);
  ioctl(fd, WDIOC_STOP, 0);
  close(fd);
  printf("wdt_test: FAIL; watchdog did not reset the board\n");
  return 1;
}

static void wdt_test_usage(void)
{
  printf("Usage: wdt_test feed [seconds 3..60]\n");
  printf("       wdt_test reset [seconds 3..30] confirm\n");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  uint32_t seconds;

  if (argc >= 2 && strcmp(argv[1], "feed") == 0)
    {
      seconds = WDT_TEST_FEED_DEFAULT_SEC;
      if (argc > 3 ||
          (argc == 3 &&
           wdt_test_parse_seconds(argv[2], WDT_TEST_FEED_MIN_SEC,
                                  WDT_TEST_FEED_MAX_SEC, &seconds) < 0))
        {
          wdt_test_usage();
          return 1;
        }

      return wdt_test_feed(seconds);
    }

  if (argc >= 2 && strcmp(argv[1], "reset") == 0)
    {
      seconds = WDT_TEST_RESET_DEFAULT_SEC;
      if (argc < 3 || argc > 4 ||
          strcmp(argv[argc - 1], "confirm") != 0 ||
          (argc == 4 &&
           wdt_test_parse_seconds(argv[2], WDT_TEST_RESET_MIN_SEC,
                                  WDT_TEST_RESET_MAX_SEC, &seconds) < 0))
        {
          wdt_test_usage();
          return 1;
        }

      return wdt_test_reset(seconds);
    }

  wdt_test_usage();
  return 1;
}
