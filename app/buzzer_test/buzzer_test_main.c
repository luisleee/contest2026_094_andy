/****************************************************************************
 * contest2026_094_andy/app/buzzer_test/buzzer_test_main.c
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>

#include <errno.h>
#include <fcntl.h>
#include <fixedmath.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nuttx/timers/pwm.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BUZZER_DEVICE              "/dev/pwm1"
#define BUZZER_DEFAULT_FREQUENCY   4000u
#define BUZZER_DEFAULT_DURATION_MS 1000u
#define BUZZER_MIN_FREQUENCY       100u
#define BUZZER_MAX_FREQUENCY       10000u
#define BUZZER_MIN_DURATION_MS     1u
#define BUZZER_MAX_DURATION_MS     5000u

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int buzzer_parse_arg(const char *text, uint32_t minimum,
                            uint32_t maximum, uint32_t *value)
{
  char *endptr;
  unsigned long parsed;

  errno = 0;
  parsed = strtoul(text, &endptr, 10);
  if (errno != 0 || *text == '\0' || *endptr != '\0' ||
      parsed < minimum || parsed > maximum)
    {
      return -EINVAL;
    }

  *value = (uint32_t)parsed;
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, char *argv[])
{
  struct pwm_info_s info;
  uint32_t frequency = BUZZER_DEFAULT_FREQUENCY;
  uint32_t duration_ms = BUZZER_DEFAULT_DURATION_MS;
  int result = 1;
  int fd;
  int ret;

  if (argc > 3 ||
      (argc >= 2 && buzzer_parse_arg(argv[1], BUZZER_MIN_FREQUENCY,
                                     BUZZER_MAX_FREQUENCY,
                                     &frequency) < 0) ||
      (argc == 3 && buzzer_parse_arg(argv[2], BUZZER_MIN_DURATION_MS,
                                     BUZZER_MAX_DURATION_MS,
                                     &duration_ms) < 0))
    {
      printf("Usage: buzzer_test [frequency_hz 100..10000] "
             "[duration_ms 1..5000]\n");
      return 1;
    }

  fd = open(BUZZER_DEVICE, O_RDONLY);
  if (fd < 0)
    {
      printf("buzzer_test: open %s failed: %s\n",
             BUZZER_DEVICE, strerror(errno));
      return 1;
    }

  memset(&info, 0, sizeof(info));
  info.frequency = frequency;
  info.duty = b16HALF;
  info.cpol = PWM_CPOL_LOW;
  info.dcpol = PWM_DCPOL_LOW;

  ret = ioctl(fd, PWMIOC_SETCHARACTERISTICS,
              (unsigned long)((uintptr_t)&info));
  if (ret < 0)
    {
      printf("buzzer_test: configure failed: %s\n", strerror(errno));
      goto out_stop;
    }

  ret = ioctl(fd, PWMIOC_START, 0);
  if (ret < 0)
    {
      printf("buzzer_test: start failed: %s\n", strerror(errno));
      goto out_stop;
    }

  printf("Buzzer on: %lu Hz, 50%% duty, %lu ms\n",
         (unsigned long)frequency, (unsigned long)duration_ms);
  usleep(duration_ms * 1000u);
  result = 0;

out_stop:
  ret = ioctl(fd, PWMIOC_STOP, 0);
  if (ret < 0)
    {
      printf("buzzer_test: stop failed: %s\n", strerror(errno));
      result = 1;
    }

  close(fd);
  if (result == 0)
    {
      printf("Buzzer off\n");
    }

  return result;
}
