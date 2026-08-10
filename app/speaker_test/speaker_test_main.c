/****************************************************************************
 * contest2026_094_andy/app/speaker_test/speaker_test_main.c
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nuttx/audio/audio.h>
#include <system/nxplayer.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define SPEAKER_TEST_DEVICE      "/dev/audio/pcm0p"
#define SPEAKER_TEST_DEFAULT_WAV "/data/s16le1c.wav"
#define SPEAKER_TEST_MAX_CHUNKS  32

#define SPEAKER_TEST_RIFF 0x46464952u
#define SPEAKER_TEST_WAVE 0x45564157u
#define SPEAKER_TEST_FMT  0x20746d66u
#define SPEAKER_TEST_DATA 0x61746164u

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct speaker_test_wav_s
{
  uint16_t format;
  uint16_t channels;
  uint16_t samplebits;
  uint32_t samplerate;
  uint32_t byterate;
  uint32_t data_bytes;
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint16_t speaker_test_le16(FAR const uint8_t *buffer)
{
  return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
}

static uint32_t speaker_test_le32(FAR const uint8_t *buffer)
{
  return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) |
         ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 24);
}

static int speaker_test_read_exact(int fd, FAR uint8_t *buffer, size_t length)
{
  size_t done = 0;

  while (done < length)
    {
      ssize_t received = read(fd, buffer + done, length - done);

      if (received < 0)
        {
          return -errno;
        }

      if (received == 0)
        {
          return -ENODATA;
        }

      done += received;
    }

  return OK;
}

static int speaker_test_samplerate_supported(uint32_t samplerate)
{
  switch (samplerate)
    {
      case 8000:
      case 11025:
      case 12000:
      case 16000:
      case 22050:
      case 24000:
      case 32000:
      case 44100:
      case 48000:
        return 1;

      default:
        return 0;
    }
}

static int speaker_test_parse_wav(FAR const char *path,
                                  FAR struct speaker_test_wav_s *wav)
{
  uint8_t header[12];
  uint8_t chunk[8];
  uint8_t format[16];
  uint32_t chunk_size;
  unsigned int index;
  int fd;
  int ret;

  memset(wav, 0, sizeof(*wav));
  fd = open(path, O_RDONLY);
  if (fd < 0)
    {
      return -errno;
    }

  ret = speaker_test_read_exact(fd, header, sizeof(header));
  if (ret < 0 || speaker_test_le32(&header[0]) != SPEAKER_TEST_RIFF ||
      speaker_test_le32(&header[8]) != SPEAKER_TEST_WAVE)
    {
      ret = ret < 0 ? ret : -EINVAL;
      goto out;
    }

  for (index = 0; index < SPEAKER_TEST_MAX_CHUNKS; index++)
    {
      ret = speaker_test_read_exact(fd, chunk, sizeof(chunk));
      if (ret < 0)
        {
          goto out;
        }

      chunk_size = speaker_test_le32(&chunk[4]);
      if (speaker_test_le32(&chunk[0]) == SPEAKER_TEST_FMT)
        {
          if (chunk_size < sizeof(format))
            {
              ret = -EINVAL;
              goto out;
            }

          ret = speaker_test_read_exact(fd, format, sizeof(format));
          if (ret < 0)
            {
              goto out;
            }

          wav->format = speaker_test_le16(&format[0]);
          wav->channels = speaker_test_le16(&format[2]);
          wav->samplerate = speaker_test_le32(&format[4]);
          wav->byterate = speaker_test_le32(&format[8]);
          wav->samplebits = speaker_test_le16(&format[14]);
          chunk_size -= sizeof(format);
        }
      else if (speaker_test_le32(&chunk[0]) == SPEAKER_TEST_DATA)
        {
          wav->data_bytes = chunk_size;
          ret = OK;
          goto out;
        }

      if (lseek(fd, chunk_size + (chunk_size & 1u), SEEK_CUR) < 0)
        {
          ret = -errno;
          goto out;
        }
    }

  ret = -E2BIG;

out:
  close(fd);
  if (ret < 0)
    {
      return ret;
    }

  if (wav->format != 1 || wav->data_bytes == 0 ||
      (wav->channels != 1 && wav->channels != 2) ||
      wav->samplebits != 16 || wav->byterate == 0 ||
      !speaker_test_samplerate_supported(wav->samplerate))
    {
      return -ENOTSUP;
    }

  return OK;
}

static int speaker_test_info(FAR const char *path)
{
  struct speaker_test_wav_s wav;
  struct stat device_stat;
  uint32_t duration_ms;
  int ret;

  if (stat(SPEAKER_TEST_DEVICE, &device_stat) < 0)
    {
      printf("speaker_test: stat %s failed: %s\n",
             SPEAKER_TEST_DEVICE, strerror(errno));
      return 1;
    }

  ret = speaker_test_parse_wav(path, &wav);
  if (ret < 0)
    {
      printf("speaker_test: unsupported or invalid WAV %s: %s\n",
             path, strerror(-ret));
      return 1;
    }

  duration_ms = (uint32_t)(((uint64_t)wav.data_bytes * 1000u) / wav.byterate);
  printf("Device: %s\n", SPEAKER_TEST_DEVICE);
  printf("WAV: %s\n", path);
  printf("PCM S16_LE, %u channel(s), %lu Hz, %lu data bytes, %lu ms\n",
         wav.channels, (unsigned long)wav.samplerate,
         (unsigned long)wav.data_bytes, (unsigned long)duration_ms);
  return 0;
}

static int speaker_test_run_player(FAR struct nxplayer_s *player, int ret)
{
  if (ret < 0)
    {
      printf("speaker_test: playback start failed: %s (%d)\n",
             strerror(-ret), ret);
      nxplayer_release(player);
      return 1;
    }

  nxplayer_release(player);
  printf("speaker_test: playback complete\n");
  return 0;
}

static FAR struct nxplayer_s *speaker_test_create_player(void)
{
  FAR struct nxplayer_s *player;
  int ret;

  player = nxplayer_create();
  if (player == NULL)
    {
      printf("speaker_test: failed to create nxplayer\n");
      return NULL;
    }

  ret = nxplayer_setdevice(player, SPEAKER_TEST_DEVICE);
  if (ret < 0)
    {
      printf("speaker_test: cannot select %s: %s (%d)\n",
             SPEAKER_TEST_DEVICE, strerror(-ret), ret);
      nxplayer_release(player);
      return NULL;
    }

  return player;
}

static int speaker_test_play(FAR const char *path)
{
  FAR struct nxplayer_s *player;

  if (speaker_test_info(path) != 0)
    {
      return 1;
    }

  player = speaker_test_create_player();
  if (player == NULL)
    {
      return 1;
    }

  printf("speaker_test: playing %s\n", path);
  return speaker_test_run_player(
      player, nxplayer_playfile(player, path, AUDIO_FMT_PCM,
                                AUDIO_SUBFMT_PCM_S16_LE));
}

static int speaker_test_parse_u32(FAR const char *text, uint32_t minimum,
                                  uint32_t maximum, FAR uint32_t *value)
{
  FAR char *endptr;
  unsigned long parsed;

  errno = 0;
  parsed = strtoul(text, &endptr, 10);
  if (errno != 0 || *text == '\0' || *endptr != '\0' ||
      parsed < minimum || parsed > maximum)
    {
      return -EINVAL;
    }

  *value = (uint32_t)parsed;
  return OK;
}

static int speaker_test_tone(int argc, FAR char *argv[])
{
  FAR struct nxplayer_s *player;
  uint32_t frequency = 1000;
  uint32_t seconds = 1;

  if (argc > 2 &&
      speaker_test_parse_u32(argv[2], 100, 5000, &frequency) < 0)
    {
      printf("speaker_test: frequency must be 100..5000 Hz\n");
      return 1;
    }

  if (argc > 3 && speaker_test_parse_u32(argv[3], 1, 5, &seconds) < 0)
    {
      printf("speaker_test: duration must be 1..5 seconds\n");
      return 1;
    }

  player = speaker_test_create_player();
  if (player == NULL)
    {
      return 1;
    }

  printf("speaker_test: playing %lu Hz for %lu second(s)\n",
         (unsigned long)frequency, (unsigned long)seconds);
  return speaker_test_run_player(
      player, nxplayer_playtone(player, 48000, frequency, seconds));
}

static void speaker_test_usage(FAR const char *program)
{
  printf("Usage:\n");
  printf("  %s info [wav]\n", program);
  printf("  %s play [wav]\n", program);
  printf("  %s tone [frequency_hz] [seconds]\n", program);
  printf("Default WAV: %s\n", SPEAKER_TEST_DEFAULT_WAV);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  FAR const char *path;

  if (argc < 2)
    {
      speaker_test_usage(argv[0]);
      return 1;
    }

  path = argc > 2 ? argv[2] : SPEAKER_TEST_DEFAULT_WAV;
  if (strcmp(argv[1], "info") == 0)
    {
      return speaker_test_info(path);
    }

  if (strcmp(argv[1], "play") == 0)
    {
      return speaker_test_play(path);
    }

  if (strcmp(argv[1], "tone") == 0)
    {
      if (argc > 4)
        {
          speaker_test_usage(argv[0]);
          return 1;
        }

      return speaker_test_tone(argc, argv);
    }

  speaker_test_usage(argv[0]);
  return 1;
}
