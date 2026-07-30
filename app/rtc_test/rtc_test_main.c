/****************************************************************************
 * contest2026_094_andy/app/rtc_test/rtc_test_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <nuttx/arch.h>

#include <arch/chip/d13x_rtc.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define RTC_TEST_COUNT_DEFAULT_SEC  5u
#define RTC_TEST_COUNT_MIN_SEC      2u
#define RTC_TEST_COUNT_MAX_SEC      30u
#define RTC_TEST_ALARM_DEFAULT_SEC  5u
#define RTC_TEST_ALARM_MIN_SEC      2u
#define RTC_TEST_ALARM_MAX_SEC      30u

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int rtc_test_parse_seconds(FAR const char *text,
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

static void rtc_test_print_time(FAR const char *prefix, time_t seconds)
{
  struct tm timeinfo;

  if (gmtime_r(&seconds, &timeinfo) == NULL)
    {
      printf("rtc_test: %s epoch=%lld\n", prefix, (long long)seconds);
      return;
    }

  printf("rtc_test: %s %04d-%02d-%02dT%02d:%02d:%02dZ epoch=%lld\n",
         prefix, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
         timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min,
         timeinfo.tm_sec, (long long)seconds);
}

static int rtc_test_parse_time(FAR const char *text, FAR time_t *seconds)
{
  struct tm check;
  struct tm timeinfo;
  char tail;
  time_t value;
  int fields;

  memset(&timeinfo, 0, sizeof(timeinfo));
  fields = sscanf(text, "%d-%d-%dT%d:%d:%d%c",
                  &timeinfo.tm_year, &timeinfo.tm_mon,
                  &timeinfo.tm_mday, &timeinfo.tm_hour,
                  &timeinfo.tm_min, &timeinfo.tm_sec, &tail);
  if (fields != 6 || timeinfo.tm_year < 2000 ||
      timeinfo.tm_year > 2099 || timeinfo.tm_mon < 1 ||
      timeinfo.tm_mon > 12 || timeinfo.tm_mday < 1 ||
      timeinfo.tm_mday > 31 || timeinfo.tm_hour < 0 ||
      timeinfo.tm_hour > 23 || timeinfo.tm_min < 0 ||
      timeinfo.tm_min > 59 || timeinfo.tm_sec < 0 ||
      timeinfo.tm_sec > 59)
    {
      return -EINVAL;
    }

  timeinfo.tm_year -= 1900;
  timeinfo.tm_mon -= 1;
  value = timegm(&timeinfo);
  if (value < 0 || gmtime_r(&value, &check) == NULL ||
      check.tm_year != timeinfo.tm_year || check.tm_mon != timeinfo.tm_mon ||
      check.tm_mday != timeinfo.tm_mday ||
      check.tm_hour != timeinfo.tm_hour || check.tm_min != timeinfo.tm_min ||
      check.tm_sec != timeinfo.tm_sec)
    {
      return -EINVAL;
    }

  *seconds = value;
  return OK;
}

static uint64_t rtc_test_milliseconds(void)
{
  struct timespec now;

  clock_gettime(CLOCK_MONOTONIC, &now);
  return (uint64_t)now.tv_sec * 1000u + now.tv_nsec / 1000000u;
}

static int rtc_test_show(void)
{
  struct d13x_rtc_state_s state;
  time_t seconds;

  if (d13x_rtc_get_state(&state) < 0)
    {
      printf("rtc_test: failed to read RTC state\n");
      return 1;
    }

  seconds = (time_t)state.counter;
  printf("rtc_test: version=0x%08lx ctl=0x%02x init=0x%02x "
         "irq_en=0x%02x irq_sta=0x%02x\n",
         (unsigned long)state.version, state.control, state.init,
         state.irq_enable, state.irq_status);
  printf("rtc_test: time_set=%lu counter=%lu\n",
         (unsigned long)state.time_set, (unsigned long)state.counter);
  rtc_test_print_time("counter", seconds);
  return 0;
}

static int rtc_test_count(uint32_t duration_seconds)
{
  time_t start;
  time_t finish;
  time_t delta;

  start = up_rtc_time();
  rtc_test_print_time("start", start);
  sleep(duration_seconds);
  finish = up_rtc_time();
  rtc_test_print_time("finish", finish);
  delta = finish - start;

  if (delta < (time_t)duration_seconds - 1 ||
      delta > (time_t)duration_seconds + 1)
    {
      printf("rtc_test: FAIL; counter advanced %lld seconds\n",
             (long long)delta);
      return 1;
    }

  printf("rtc_test: PASS; counter advanced %lld seconds\n",
         (long long)delta);
  return 0;
}

static int rtc_test_set(FAR const char *text)
{
  struct d13x_rtc_state_s state;
  struct timespec value;
  time_t actual;

  if (rtc_test_parse_time(text, &value.tv_sec) < 0)
    {
      printf("rtc_test: invalid UTC time\n");
      return 1;
    }

  value.tv_nsec = 0;
  if (clock_settime(CLOCK_REALTIME, &value) < 0)
    {
      printf("rtc_test: clock_settime failed: %s\n", strerror(errno));
      return 1;
    }

  actual = up_rtc_time();
  rtc_test_print_time("set", actual);
  if (actual < value.tv_sec || actual > value.tv_sec + 1)
    {
      printf("rtc_test: FAIL; requested=%lld actual=%lld\n",
             (long long)value.tv_sec, (long long)actual);
      if (d13x_rtc_get_state(&state) == OK)
        {
          printf("rtc_test: raw ctl=0x%02x init=0x%02x time_set=%lu "
                 "counter=%lu\n",
                 state.control, state.init,
                 (unsigned long)state.time_set,
                 (unsigned long)state.counter);
        }

      return 1;
    }

  printf("rtc_test: PASS; system time and RTC updated\n");
  return 0;
}

static int rtc_test_alarm(uint32_t delay_seconds)
{
  uint64_t start;
  uint64_t elapsed;
  int ret;

  printf("rtc_test: alarm armed for %lu seconds on raw IRQ 50\n",
         (unsigned long)delay_seconds);
  start = rtc_test_milliseconds();
  ret = d13x_rtc_alarm_wait(delay_seconds,
                            (delay_seconds + 3u) * 1000u);
  elapsed = rtc_test_milliseconds() - start;
  if (ret < 0)
    {
      printf("rtc_test: alarm wait failed after %llu ms: %s\n",
             (unsigned long long)elapsed, strerror(-ret));
      return 1;
    }

  printf("rtc_test: PASS; alarm IRQ arrived after %llu ms\n",
         (unsigned long long)elapsed);
  return 0;
}

static void rtc_test_usage(void)
{
  printf("Usage: rtc_test show\n");
  printf("       rtc_test count [seconds 2..30]\n");
  printf("       rtc_test set YYYY-MM-DDTHH:MM:SS\n");
  printf("       rtc_test alarm [seconds 2..30]\n");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  uint32_t seconds;

  if (argc == 2 && strcmp(argv[1], "show") == 0)
    {
      return rtc_test_show();
    }

  if (argc >= 2 && strcmp(argv[1], "count") == 0)
    {
      seconds = RTC_TEST_COUNT_DEFAULT_SEC;
      if (argc > 3 ||
          (argc == 3 &&
           rtc_test_parse_seconds(argv[2], RTC_TEST_COUNT_MIN_SEC,
                                  RTC_TEST_COUNT_MAX_SEC, &seconds) < 0))
        {
          rtc_test_usage();
          return 1;
        }

      return rtc_test_count(seconds);
    }

  if (argc == 3 && strcmp(argv[1], "set") == 0)
    {
      return rtc_test_set(argv[2]);
    }

  if (argc >= 2 && strcmp(argv[1], "alarm") == 0)
    {
      seconds = RTC_TEST_ALARM_DEFAULT_SEC;
      if (argc > 3 ||
          (argc == 3 &&
           rtc_test_parse_seconds(argv[2], RTC_TEST_ALARM_MIN_SEC,
                                  RTC_TEST_ALARM_MAX_SEC, &seconds) < 0))
        {
          rtc_test_usage();
          return 1;
        }

      return rtc_test_alarm(seconds);
    }

  rtc_test_usage();
  return 1;
}
