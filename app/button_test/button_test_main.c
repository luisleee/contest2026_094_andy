/****************************************************************************
 * contest2026_094_andy/app/button_test/button_test_main.c
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
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <nuttx/input/buttons.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BUTTON_TEST_DEVICE             "/dev/buttons"
#define BUTTON_TEST_WAKEUP_BIT         (1u << 0)
#define BUTTON_TEST_DEFAULT_SECONDS    15u
#define BUTTON_TEST_MIN_SECONDS        1u
#define BUTTON_TEST_MAX_SECONDS        300u
#define BUTTON_TEST_POLL_MS            1000

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int button_test_parse_seconds(FAR const char *text,
                                     FAR uint32_t *seconds)
{
  FAR char *endptr;
  unsigned long value;

  errno = 0;
  value = strtoul(text, &endptr, 10);
  if (errno != 0 || *text == '\0' || *endptr != '\0' ||
      value < BUTTON_TEST_MIN_SECONDS ||
      value > BUTTON_TEST_MAX_SECONDS)
    {
      return -EINVAL;
    }

  *seconds = (uint32_t)value;
  return OK;
}

static uint64_t button_test_now_ms(void)
{
  struct timespec now;

  clock_gettime(CLOCK_MONOTONIC, &now);
  return (uint64_t)now.tv_sec * 1000u + now.tv_nsec / 1000000u;
}

static int button_test_read(int fd, FAR btn_buttonset_t *sample)
{
  ssize_t nbytes;

  nbytes = read(fd, sample, sizeof(*sample));
  if (nbytes < 0)
    {
      return -errno;
    }

  return nbytes == sizeof(*sample) ? OK : -EIO;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  btn_buttonset_t supported;
  btn_buttonset_t previous;
  btn_buttonset_t sample;
  struct pollfd pfd;
  uint32_t duration = BUTTON_TEST_DEFAULT_SECONDS;
  uint32_t presses = 0;
  uint32_t releases = 0;
  uint64_t deadline;
  uint64_t now;
  int timeout;
  int result = 1;
  int fd;
  int ret;

  if (argc > 2 ||
      (argc == 2 && button_test_parse_seconds(argv[1], &duration) < 0))
    {
      printf("Usage: button_test [seconds 1..300]\n");
      return 1;
    }

  fd = open(BUTTON_TEST_DEVICE, O_RDONLY | O_NONBLOCK);
  if (fd < 0)
    {
      printf("button_test: open %s failed: %s\n",
             BUTTON_TEST_DEVICE, strerror(errno));
      return 1;
    }

  ret = ioctl(fd, BTNIOC_SUPPORTED,
              (unsigned long)((uintptr_t)&supported));
  if (ret < 0)
    {
      printf("button_test: BTNIOC_SUPPORTED failed: %s\n",
             strerror(errno));
      goto out;
    }

  if ((supported & BUTTON_TEST_WAKEUP_BIT) == 0)
    {
      printf("button_test: WAKEUP is not supported (mask=0x%08lx)\n",
             (unsigned long)supported);
      goto out;
    }

  ret = button_test_read(fd, &previous);
  if (ret < 0)
    {
      printf("button_test: initial read failed: %s\n", strerror(-ret));
      goto out;
    }

  printf("button_test: %s supported=0x%08lx initial=%s "
         "duration=%lu seconds\n",
         BUTTON_TEST_DEVICE, (unsigned long)supported,
         (previous & BUTTON_TEST_WAKEUP_BIT) != 0 ? "pressed" : "released",
         (unsigned long)duration);

  pfd.fd = fd;
  pfd.events = POLLIN;
  pfd.revents = 0;
  deadline = button_test_now_ms() + (uint64_t)duration * 1000u;

  for (; ; )
    {
      now = button_test_now_ms();
      if (now >= deadline)
        {
          break;
        }

      timeout = deadline - now > BUTTON_TEST_POLL_MS ?
                BUTTON_TEST_POLL_MS : (int)(deadline - now);
      ret = poll(&pfd, 1, timeout);
      if (ret < 0)
        {
          if (errno == EINTR)
            {
              continue;
            }

          printf("button_test: poll failed: %s\n", strerror(errno));
          goto out;
        }

      if (ret == 0)
        {
          continue;
        }

      if ((pfd.revents & POLLIN) == 0)
        {
          printf("button_test: unexpected poll events 0x%04lx\n",
                 (unsigned long)pfd.revents);
          goto out;
        }

      ret = button_test_read(fd, &sample);
      if (ret < 0)
        {
          printf("button_test: read failed: %s\n", strerror(-ret));
          goto out;
        }

      if ((sample & BUTTON_TEST_WAKEUP_BIT) !=
          (previous & BUTTON_TEST_WAKEUP_BIT))
        {
          if ((sample & BUTTON_TEST_WAKEUP_BIT) != 0)
            {
              presses++;
              printf("WAKEUP PRESS\n");
            }
          else
            {
              releases++;
              printf("WAKEUP RELEASE\n");
            }

          previous = sample;
        }

      pfd.revents = 0;
    }

  printf("button_test: presses=%lu releases=%lu final=%s\n",
         (unsigned long)presses, (unsigned long)releases,
         (previous & BUTTON_TEST_WAKEUP_BIT) != 0 ? "pressed" : "released");
  result = 0;

out:
  close(fd);
  return result;
}
