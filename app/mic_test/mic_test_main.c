/****************************************************************************
 * contest2026_094_andy/app/mic_test/mic_test_main.c
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <unistd.h>

#include <nuttx/audio/audio.h>
#include <nuttx/fs/fs.h>
#include <system/nxplayer.h>
#include <system/nxrecorder.h>

#define MIC_TEST_CAPTURE_DEVICE "/dev/audio/pcm0c"
#define MIC_TEST_PLAY_DEVICE    "/dev/audio/pcm0p"
#define MIC_TEST_DEFAULT_WAV    "/sdcard/mic.wav"
#define MIC_TEST_SAMPLE_RATE    16000
#define MIC_TEST_CHANNELS       1
#define MIC_TEST_SAMPLE_BITS    16
#define MIC_TEST_WAV_HEADER_SIZE 44
#define MIC_TEST_COPY_SIZE       4096
#define MIC_TEST_STATUS_INTERVAL_US 250000

extern int d13x_dmic_get_peak(FAR uint32_t *sequence,
                              FAR uint8_t *peak_percent);

static int mic_test_require_sdcard(FAR const char *path)
{
  struct statfs filesystem;

  if (strncmp(path, "/sdcard/", strlen("/sdcard/")) != 0)
    {
      return OK;
    }

  if (statfs("/sdcard", &filesystem) == 0 &&
      filesystem.f_type == FATFS_SUPER_MAGIC)
    {
      return OK;
    }

  printf("mic_test: /sdcard is not a mounted FAT filesystem\n");
  printf("mic_test: refusing to record into NOR; run 'tf_test mount' first\n");
  return -ENODEV;
}

static void mic_test_put_le16(FAR uint8_t *buffer, uint16_t value)
{
  buffer[0] = value & 0xff;
  buffer[1] = value >> 8;
}

static void mic_test_put_le32(FAR uint8_t *buffer, uint32_t value)
{
  buffer[0] = value & 0xff;
  buffer[1] = (value >> 8) & 0xff;
  buffer[2] = (value >> 16) & 0xff;
  buffer[3] = value >> 24;
}

static int mic_test_write_exact(int fd, FAR const uint8_t *buffer,
                                size_t length)
{
  size_t done = 0;

  while (done < length)
    {
      ssize_t count = write(fd, buffer + done, length - done);

      if (count <= 0)
        {
          return -EIO;
        }

      done += count;
    }

  return OK;
}

static void mic_test_make_wav_header(FAR uint8_t *header,
                                     uint32_t data_bytes)
{
  uint32_t byte_rate = MIC_TEST_SAMPLE_RATE * MIC_TEST_CHANNELS *
                       (MIC_TEST_SAMPLE_BITS / 8);
  uint16_t block_align = MIC_TEST_CHANNELS * (MIC_TEST_SAMPLE_BITS / 8);

  memset(header, 0, MIC_TEST_WAV_HEADER_SIZE);
  memcpy(&header[0], "RIFF", 4);
  mic_test_put_le32(&header[4], 36 + data_bytes);
  memcpy(&header[8], "WAVEfmt ", 8);
  mic_test_put_le32(&header[16], 16);
  mic_test_put_le16(&header[20], 1);
  mic_test_put_le16(&header[22], MIC_TEST_CHANNELS);
  mic_test_put_le32(&header[24], MIC_TEST_SAMPLE_RATE);
  mic_test_put_le32(&header[28], byte_rate);
  mic_test_put_le16(&header[32], block_align);
  mic_test_put_le16(&header[34], MIC_TEST_SAMPLE_BITS);
  memcpy(&header[36], "data", 4);
  mic_test_put_le32(&header[40], data_bytes);
}

static int mic_test_finalize_wav(FAR const char *raw_path,
                                 FAR const char *wav_path)
{
  FAR uint8_t *buffer;
  uint8_t header[MIC_TEST_WAV_HEADER_SIZE];
  struct stat file_stat;
  ssize_t count;
  int raw_fd;
  int wav_fd;
  int ret = -EIO;

  if (stat(raw_path, &file_stat) < 0 || file_stat.st_size <= 0 ||
      (uint64_t)file_stat.st_size > UINT32_MAX - 36)
    {
      return -EINVAL;
    }

  raw_fd = open(raw_path, O_RDONLY);
  if (raw_fd < 0)
    {
      return -errno;
    }

  wav_fd = open(wav_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (wav_fd < 0)
    {
      ret = -errno;
      goto out_raw;
    }

  buffer = malloc(MIC_TEST_COPY_SIZE);
  if (buffer == NULL)
    {
      ret = -ENOMEM;
      goto out_wav;
    }

  mic_test_make_wav_header(header, file_stat.st_size);
  if (mic_test_write_exact(wav_fd, header, sizeof(header)) < 0)
    {
      goto out_buffer;
    }

  while ((count = read(raw_fd, buffer, MIC_TEST_COPY_SIZE)) > 0)
    {
      if (mic_test_write_exact(wav_fd, buffer, count) < 0)
        {
          goto out_buffer;
        }
    }

  if (count < 0 || fsync(wav_fd) < 0)
    {
      goto out_buffer;
    }

  ret = OK;

out_buffer:
  free(buffer);
out_wav:
  close(wav_fd);
  if (ret < 0)
    {
      unlink(wav_path);
    }

out_raw:
  close(raw_fd);
  return ret;
}

static int mic_test_prepare_wav(FAR const char *path)
{
  char temp_path[CONFIG_PATH_MAX];
  uint8_t signature[12];
  struct stat file_stat;
  ssize_t count;
  int fd;
  int ret;

  if (stat(path, &file_stat) < 0)
    {
      return -errno;
    }

  if (file_stat.st_size <= 0)
    {
      return -ENODATA;
    }

  fd = open(path, O_RDONLY);
  if (fd < 0)
    {
      return -errno;
    }

  count = read(fd, signature, sizeof(signature));
  close(fd);
  if (count < 0)
    {
      return -errno;
    }

  if (count == sizeof(signature) &&
      memcmp(&signature[0], "RIFF", 4) == 0 &&
      memcmp(&signature[8], "WAVE", 4) == 0)
    {
      return file_stat.st_size > MIC_TEST_WAV_HEADER_SIZE ? OK : -ENODATA;
    }

  ret = snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);
  if (ret < 0 || ret >= (int)sizeof(temp_path))
    {
      return -ENAMETOOLONG;
    }

  unlink(temp_path);
  ret = mic_test_finalize_wav(path, temp_path);
  if (ret < 0)
    {
      return ret;
    }

  if (rename(temp_path, path) < 0)
    {
      ret = -errno;
      unlink(temp_path);
      return ret;
    }

  return OK;
}

static int mic_test_parse_seconds(FAR const char *text,
                                  FAR uint32_t *seconds)
{
  FAR char *endptr;
  unsigned long value;

  errno = 0;
  value = strtoul(text, &endptr, 10);
  if (errno != 0 || *text == '\0' || *endptr != '\0' ||
      value < 1 || value > 5)
    {
      return -EINVAL;
    }

  *seconds = value;
  return OK;
}

static int mic_test_record(FAR const char *path, uint32_t seconds)
{
  FAR struct nxrecorder_s *recorder;
  struct sched_param capture_param;
  struct sched_param saved_param;
  struct stat file_stat;
  uint32_t peak_sequence;
  uint32_t previous_sequence = 0;
  uint32_t intervals;
  uint32_t interval;
  uint8_t peak_percent;
  int priority_raised = 0;
  int start_status;
  int ret;

  ret = mic_test_require_sdcard(path);
  if (ret < 0)
    {
      return 1;
    }

  unlink(path);

  recorder = nxrecorder_create();
  if (recorder == NULL)
    {
      printf("mic_test: cannot create recorder\n");
      return 1;
    }

  ret = nxrecorder_setdevice(recorder, MIC_TEST_CAPTURE_DEVICE);
  if (ret < 0)
    {
      printf("mic_test: cannot select %s: %s (%d)\n",
             MIC_TEST_CAPTURE_DEVICE, strerror(-ret), ret);
      nxrecorder_release(recorder);
      return 1;
    }

  printf("mic_test: recording %lu second(s), 16000 Hz mono S16_LE\n",
         (unsigned long)seconds);

  if (sched_getparam(0, &saved_param) == 0)
    {
      capture_param = saved_param;
      capture_param.sched_priority = sched_get_priority_max(SCHED_FIFO) - 8;
      if (capture_param.sched_priority > saved_param.sched_priority &&
          sched_setparam(0, &capture_param) == 0)
        {
          priority_raised = 1;
        }
    }

  ret = nxrecorder_recordinternal(recorder, path, AUDIO_FMT_PCM,
                                  MIC_TEST_CHANNELS, MIC_TEST_SAMPLE_BITS,
                                  MIC_TEST_SAMPLE_RATE, 0);
  if (ret < 0)
    {
      if (priority_raised)
        {
          sched_setparam(0, &saved_param);
        }

      printf("mic_test: recording start failed: %s (%d)\n",
             strerror(-ret), ret);
      nxrecorder_release(recorder);
      unlink(path);
      return 1;
    }

  printf("mic_test: capture request submitted\n");
  intervals = seconds * 1000000 / MIC_TEST_STATUS_INTERVAL_US;
  for (interval = 0; interval < intervals; interval++)
    {
      usleep(MIC_TEST_STATUS_INTERVAL_US);
      peak_sequence = 0;
      peak_percent = 0;
      d13x_dmic_get_peak(&peak_sequence, &peak_percent);
      start_status = nxrecorder_getstartstatus(recorder);
      printf("mic_test: t=%lu ms start=%d dma_seq=%lu%s "
             "peak=%u%% bytes=%lu\n",
             (unsigned long)((interval + 1) *
                             (MIC_TEST_STATUS_INTERVAL_US / 1000)),
             start_status, (unsigned long)peak_sequence,
             peak_sequence == previous_sequence ? " (stalled)" : "",
             peak_percent,
             (unsigned long)nxrecorder_getbyteswritten(recorder));
      previous_sequence = peak_sequence;
    }

  printf("mic_test: stopping capture\n");
  ret = nxrecorder_stop(recorder);
  if (priority_raised)
    {
      sched_setparam(0, &saved_param);
    }

  nxrecorder_release(recorder);
  if (ret < 0)
    {
      printf("mic_test: recording stop failed: %s (%d)\n",
             strerror(-ret), ret);
      unlink(path);
      return 1;
    }

  printf("mic_test: validating WAV\n");
  ret = mic_test_prepare_wav(path);
  if (ret < 0)
    {
      printf("mic_test: WAV preparation failed: %s (%d)\n",
             strerror(-ret), ret);
      return 1;
    }

  if (stat(path, &file_stat) < 0 || file_stat.st_size <= 44)
    {
      printf("mic_test: recording file is missing or empty: %s\n", path);
      return 1;
    }

  printf("mic_test: saved %s (%lu bytes)\n", path,
         (unsigned long)file_stat.st_size);
  return 0;
}

static int mic_test_play(FAR const char *path)
{
  FAR struct nxplayer_s *player;
  int ret;

  player = nxplayer_create();
  if (player == NULL)
    {
      printf("mic_test: cannot create player\n");
      return 1;
    }

  ret = nxplayer_setdevice(player, MIC_TEST_PLAY_DEVICE);
  if (ret < 0)
    {
      printf("mic_test: cannot select %s: %s (%d)\n",
             MIC_TEST_PLAY_DEVICE, strerror(-ret), ret);
      nxplayer_release(player);
      return 1;
    }

  printf("mic_test: playing %s\n", path);
  ret = nxplayer_playfile(player, path, AUDIO_FMT_PCM,
                          AUDIO_SUBFMT_PCM_S16_LE);
  if (ret < 0)
    {
      printf("mic_test: playback start failed: %s (%d)\n",
             strerror(-ret), ret);
      nxplayer_release(player);
      return 1;
    }

  nxplayer_release(player);
  printf("mic_test: playback complete\n");
  return 0;
}

static void mic_test_usage(FAR const char *program)
{
  printf("Usage:\n");
  printf("  %s record [wav] [seconds]\n", program);
  printf("  %s play [wav]\n", program);
  printf("  %s loop [wav] [seconds]\n", program);
  printf("Default: %s, 3 seconds\n", MIC_TEST_DEFAULT_WAV);
}

int main(int argc, FAR char *argv[])
{
  FAR const char *path = argc > 2 ? argv[2] : MIC_TEST_DEFAULT_WAV;
  uint32_t seconds = 3;
  int ret;

  if (argc < 2 || argc > 4)
    {
      mic_test_usage(argv[0]);
      return 1;
    }

  if (argc > 3 && mic_test_parse_seconds(argv[3], &seconds) < 0)
    {
      printf("mic_test: duration must be 1..5 seconds\n");
      return 1;
    }

  if (strcmp(argv[1], "record") == 0)
    {
      return mic_test_record(path, seconds);
    }

  if (strcmp(argv[1], "play") == 0 && argc <= 3)
    {
      return mic_test_play(path);
    }

  if (strcmp(argv[1], "loop") == 0)
    {
      ret = mic_test_record(path, seconds);
      return ret == 0 ? mic_test_play(path) : ret;
    }

  mic_test_usage(argv[0]);
  return 1;
}
