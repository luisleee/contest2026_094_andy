/****************************************************************************
 * contest2026_094_andy/app/dpad_test/dpad_test_main.c
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

#include <arch/board/board.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define DPAD_TEST_DEVICE           "/dev/dpad"
#define DPAD_TEST_SUPPORTED        0x0f
#define DPAD_TEST_DEFAULT_SECONDS  20u
#define DPAD_TEST_MIN_SECONDS      1u
#define DPAD_TEST_MAX_SECONDS      300u
#define DPAD_TEST_POLL_MS          1000

/****************************************************************************
 * Private Data
 ****************************************************************************/

static FAR const char *g_dpad_names[4] =
{
  "UP",
  "DOWN",
  "LEFT",
  "RIGHT"
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int dpad_test_parse_seconds(FAR const char *text,
                                   FAR uint32_t *seconds)
{
  FAR char *endptr;
  unsigned long value;

  errno = 0;
  value = strtoul(text, &endptr, 10);
  if (errno != 0 || *text == '\0' || *endptr != '\0' ||
      value < DPAD_TEST_MIN_SECONDS || value > DPAD_TEST_MAX_SECONDS)
    {
      return -EINVAL;
    }

  *seconds = (uint32_t)value;
  return OK;
}

static uint64_t dpad_test_now_ms(void)
{
  struct timespec now;

  clock_gettime(CLOCK_MONOTONIC, &now);
  return (uint64_t)now.tv_sec * 1000u + now.tv_nsec / 1000000u;
}

static int dpad_test_read(int fd, FAR btn_buttonset_t *sample)
{
  ssize_t nbytes;

  nbytes = read(fd, sample, sizeof(*sample));
  if (nbytes < 0)
    {
      return -errno;
    }

  return nbytes == sizeof(*sample) ? OK : -EIO;
}

static FAR const char *dpad_test_state(btn_buttonset_t sample)
{
  unsigned int i;

  for (i = 0; i < 4; i++)
    {
      if ((sample & (1u << i)) != 0)
        {
          return g_dpad_names[i];
        }
    }

  return "NONE";
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  btn_buttonset_t supported;
  btn_buttonset_t previous;
  btn_buttonset_t sample;
  btn_buttonset_t changes;
  struct pollfd pfd;
  uint32_t duration = DPAD_TEST_DEFAULT_SECONDS;
  uint32_t presses[4] =
  {
    0
  };

  uint64_t deadline;
  uint64_t now;
  uint16_t raw = 0;
  unsigned int i;
  int timeout;
  int result = 1;
  int fd;
  int ret;

  if (argc > 2 ||
      (argc == 2 && dpad_test_parse_seconds(argv[1], &duration) < 0))
    {
      printf("Usage: dpad_test [seconds 1..300]\n");
      return 1;
    }

  fd = open(DPAD_TEST_DEVICE, O_RDONLY | O_NONBLOCK);
  if (fd < 0)
    {
      printf("dpad_test: open %s failed: %s\n",
             DPAD_TEST_DEVICE, strerror(errno));
      return 1;
    }

  ret = ioctl(fd, BTNIOC_SUPPORTED,
              (unsigned long)((uintptr_t)&supported));
  if (ret < 0)
    {
      printf("dpad_test: BTNIOC_SUPPORTED failed: %s\n",
             strerror(errno));
      goto out;
    }

  if ((supported & DPAD_TEST_SUPPORTED) != DPAD_TEST_SUPPORTED)
    {
      printf("dpad_test: expected mask 0x0000000f, got 0x%08lx\n",
             (unsigned long)supported);
      goto out;
    }

  ret = dpad_test_read(fd, &previous);
  if (ret < 0)
    {
      printf("dpad_test: initial read failed: %s\n", strerror(-ret));
      goto out;
    }

  d13x_keyadc_last_raw(&raw);
  printf("dpad_test: %s supported=0x%08lx initial=%s raw=%u "
         "duration=%lu seconds\n",
         DPAD_TEST_DEVICE, (unsigned long)supported,
         dpad_test_state(previous), raw, (unsigned long)duration);

  pfd.fd = fd;
  pfd.events = POLLIN;
  pfd.revents = 0;
  deadline = dpad_test_now_ms() + (uint64_t)duration * 1000u;

  for (; ; )
    {
      now = dpad_test_now_ms();
      if (now >= deadline)
        {
          break;
        }

      timeout = deadline - now > DPAD_TEST_POLL_MS ?
                DPAD_TEST_POLL_MS : (int)(deadline - now);
      ret = poll(&pfd, 1, timeout);
      if (ret < 0)
        {
          if (errno == EINTR)
            {
              continue;
            }

          printf("dpad_test: poll failed: %s\n", strerror(errno));
          goto out;
        }

      if (ret == 0)
        {
          continue;
        }

      if ((pfd.revents & POLLIN) == 0)
        {
          printf("dpad_test: unexpected poll events 0x%04lx\n",
                 (unsigned long)pfd.revents);
          goto out;
        }

      ret = dpad_test_read(fd, &sample);
      if (ret < 0)
        {
          printf("dpad_test: read failed: %s\n", strerror(-ret));
          goto out;
        }

      changes = sample ^ previous;
      d13x_keyadc_last_raw(&raw);
      for (i = 0; i < 4; i++)
        {
          if ((changes & (1u << i)) == 0)
            {
              continue;
            }

          if ((sample & (1u << i)) != 0)
            {
              presses[i]++;
              printf("%s PRESS raw=%u\n", g_dpad_names[i], raw);
            }
          else
            {
              printf("%s RELEASE raw=%u\n", g_dpad_names[i], raw);
            }
        }

      previous = sample;
      pfd.revents = 0;
    }

  d13x_keyadc_last_raw(&raw);
  printf("dpad_test: up=%lu down=%lu left=%lu right=%lu "
         "final=%s raw=%u\n",
         (unsigned long)presses[0], (unsigned long)presses[1],
         (unsigned long)presses[2], (unsigned long)presses[3],
         dpad_test_state(previous), raw);
  result = 0;

out:
  close(fd);
  return result;
}
