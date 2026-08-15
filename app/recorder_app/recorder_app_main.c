/****************************************************************************
 * contest2026_094_andy/app/recorder_app/recorder_app_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statfs.h>

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <nuttx/audio/audio.h>
#include <nuttx/fs/fs.h>
#include <nuttx/fs/partition.h>
#include <lvgl/lvgl.h>
#include <system/nxplayer.h>
#include <system/nxrecorder.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_PATH_MAX
#  define CONFIG_PATH_MAX 256
#endif

#define RECORDER_SD_DEVICE       "/dev/mmcsd0"
#define RECORDER_SD_PART_PREFIX  "/dev/mmcsd0p"
#define RECORDER_SD_MOUNTPOINT   "/sdcard"
#define RECORDER_CAPTURE_DEVICE  "/dev/audio/pcm0c"
#define RECORDER_PLAY_DEVICE     "/dev/audio/pcm0p"
#define RECORDER_FILE_PREFIX     "/sdcard/recorder_"
#define RECORDER_SAMPLE_RATE     16000
#define RECORDER_CHANNELS        1
#define RECORDER_SAMPLE_BITS     16
#define RECORDER_WAV_HEADER_SIZE 44
#define RECORDER_WAVE_BARS       96
#define RECORDER_PLAYBACK_WAVE_POINTS 960
#define RECORDER_WAVE_HEIGHT     240
#define RECORDER_WORKER_STACK    12288
#define RECORDER_UI_PRIORITY     120
#define RECORDER_MAX_SECONDS     600
#define RECORDER_START_TIMEOUT_MS 3000
#define RECORDER_PLAYER_IDLE     0
#define RECORDER_MAX_PARTS       4

#define RECORDER_RIFF            0x46464952u
#define RECORDER_WAVE            0x45564157u
#define RECORDER_FMT             0x20746d66u
#define RECORDER_DATA            0x61746164u

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

extern int d13x_sdmc1_reprobe(void);
extern int d13x_dmic_get_peak(FAR uint32_t *sequence,
                              FAR uint8_t *peak_percent);

/****************************************************************************
 * Private Types
 ****************************************************************************/

enum recorder_state_e
{
  RECORDER_IDLE = 0,
  RECORDER_BUSY,
  RECORDER_RECORDING,
  RECORDER_RECORD_PAUSED,
  RECORDER_PLAYING,
  RECORDER_STOPPING,
};

enum recorder_view_e
{
  RECORDER_VIEW_LIBRARY = 0,
  RECORDER_VIEW_RECORD,
  RECORDER_VIEW_PLAYER,
};

struct recorder_app_s;

struct recorder_file_s
{
  FAR struct recorder_app_s *app;
  char path[CONFIG_PATH_MAX];
  char name[NAME_MAX + 1];
  uint32_t duration_ms;
  uint32_t data_bytes;
  bool valid;
};

struct recorder_wav_s
{
  uint16_t format;
  uint16_t channels;
  uint16_t samplebits;
  uint32_t samplerate;
  uint32_t byterate;
  uint32_t data_offset;
  uint32_t data_bytes;
};

struct recorder_app_s
{
  pthread_mutex_t lock;
  enum recorder_state_e state;
  enum recorder_view_e view;
  bool quit;
  bool stop_requested;
  bool return_to_library;
  bool worker_active;
  bool library_dirty;
  char status[96];
  char path[CONFIG_PATH_MAX];
  uint8_t peaks[RECORDER_WAVE_BARS];
  FAR uint8_t *playback_peaks;
  uint16_t playback_peak_count;
  uint64_t state_start_ms;
  uint32_t duration_ms;
  uint32_t total_duration_ms;
  uint32_t live_peak_sequence;
  unsigned int next_file_index;
  FAR struct nxrecorder_s *recorder;
  FAR struct nxplayer_s *player;
  FAR struct recorder_file_s *files;
  size_t file_count;
};

struct recorder_ui_s
{
  FAR struct recorder_app_s *app;
  enum recorder_view_e shown_view;
  lv_obj_t *library_page;
  lv_obj_t *library_status;
  lv_obj_t *library_count;
  lv_obj_t *library_list;
  lv_obj_t *record_page;
  lv_obj_t *record_status;
  lv_obj_t *record_time;
  lv_obj_t *record_file;
  lv_obj_t *record_wave;
  lv_obj_t *record_pause_button;
  lv_obj_t *record_pause_label;
  lv_obj_t *record_stop_button;
  lv_obj_t *player_page;
  lv_obj_t *player_status;
  lv_obj_t *player_time;
  lv_obj_t *player_file;
  lv_obj_t *player_wave;
  lv_obj_t *player_progress;
  lv_obj_t *player_progress_fill;
  lv_obj_t *player_play_button;
  lv_obj_t *player_play_label;
};

struct recorder_wave_draw_s
{
  uint8_t peaks[RECORDER_WAVE_BARS];
  uint32_t color;
  bool playhead;
};

struct recorder_play_arg_s
{
  FAR struct recorder_app_s *app;
  char path[CONFIG_PATH_MAX];
};

struct recorder_partitions_s
{
  uint8_t count;
  char paths[RECORDER_MAX_PARTS][PATH_MAX];
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint64_t recorder_milliseconds(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static uint16_t recorder_le16(FAR const uint8_t *buffer)
{
  return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
}

static uint32_t recorder_le32(FAR const uint8_t *buffer)
{
  return (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) |
         ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 24);
}

static int recorder_read_exact(int fd, FAR uint8_t *buffer, size_t length)
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

static void recorder_partition_handler(FAR struct partition_s *part,
                                       FAR void *arg)
{
  FAR struct recorder_partitions_s *partitions = arg;
  char path[PATH_MAX];
  int ret;

  if (partitions->count >= RECORDER_MAX_PARTS || part->nblocks == 0)
    {
      return;
    }

  ret = snprintf(path, sizeof(path), "%s%u", RECORDER_SD_PART_PREFIX,
                 (unsigned int)part->index + 1);
  if (ret < 0 || ret >= (int)sizeof(path))
    {
      return;
    }

  ret = register_blockpartition(path, 0660, RECORDER_SD_DEVICE,
                                part->firstblock, part->nblocks);
  if (ret < 0 && ret != -EEXIST)
    {
      return;
    }

  strlcpy(partitions->paths[partitions->count], path,
          sizeof(partitions->paths[partitions->count]));
  partitions->count++;
}

static int recorder_reprobe_sd(void)
{
  return d13x_sdmc1_reprobe();
}

static int recorder_scan_partitions(FAR struct recorder_partitions_s *partitions)
{
  int ret;

  memset(partitions, 0, sizeof(*partitions));

  ret = parse_block_partition(RECORDER_SD_DEVICE, recorder_partition_handler,
                              partitions);
  if (ret < 0)
    {
      return ret == -EINVAL ? OK : ret;
    }

  return OK;
}

static bool recorder_sd_mounted(void)
{
  struct statfs filesystem;

  return statfs(RECORDER_SD_MOUNTPOINT, &filesystem) == 0 &&
         filesystem.f_type == FATFS_SUPER_MAGIC;
}

static int recorder_mount_device(FAR const char *device)
{
  int ret;

  ret = mount(device, RECORDER_SD_MOUNTPOINT, "fatfs",
              MS_NOSUID | MS_SYNCHRONOUS, NULL);
  if (ret < 0)
    {
      if (errno == EBUSY && recorder_sd_mounted())
        {
          return OK;
        }

      return -errno;
    }

  return OK;
}

static int recorder_mount_sd(void)
{
  struct recorder_partitions_s partitions;
  unsigned int i;
  int ret;

  if (recorder_sd_mounted())
    {
      return OK;
    }

  ret = recorder_reprobe_sd();
  if (ret < 0)
    {
      return ret;
    }

  ret = recorder_mount_device(RECORDER_SD_DEVICE);
  if (ret == OK)
    {
      return ret;
    }

  ret = recorder_scan_partitions(&partitions);
  if (ret < 0)
    {
      return ret;
    }

  for (i = 0; i < partitions.count; i++)
    {
      ret = recorder_mount_device(partitions.paths[i]);
      if (ret == OK)
        {
          return OK;
        }
    }

  return ret;
}

static int recorder_reserve_path(FAR struct recorder_app_s *app,
                                 FAR char *path, size_t length)
{
  unsigned int index;
  int fd;
  int ret;

  pthread_mutex_lock(&app->lock);
  index = app->next_file_index;
  pthread_mutex_unlock(&app->lock);

  for (; index <= 999; index++)
    {
      ret = snprintf(path, length, "%s%03u.wav",
                     RECORDER_FILE_PREFIX, index);
      if (ret < 0 || ret >= (int)length)
        {
          return -ENAMETOOLONG;
        }

      fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
      if (fd >= 0)
        {
          close(fd);
          pthread_mutex_lock(&app->lock);
          app->next_file_index = index + 1;
          pthread_mutex_unlock(&app->lock);
          return OK;
        }

      if (errno != EEXIST)
        {
          return -errno;
        }
    }

  return -ENOSPC;
}

static int recorder_parse_wav(FAR const char *path,
                              FAR struct recorder_wav_s *wav)
{
  uint8_t header[12];
  uint8_t chunk[8];
  uint8_t format[16];
  uint32_t chunk_id;
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

  ret = recorder_read_exact(fd, header, sizeof(header));
  if (ret < 0 || recorder_le32(&header[0]) != RECORDER_RIFF ||
      recorder_le32(&header[8]) != RECORDER_WAVE)
    {
      ret = ret < 0 ? ret : -EINVAL;
      goto out;
    }

  for (index = 0; index < 32; index++)
    {
      ret = recorder_read_exact(fd, chunk, sizeof(chunk));
      if (ret < 0)
        {
          goto out;
        }

      chunk_id = recorder_le32(&chunk[0]);
      chunk_size = recorder_le32(&chunk[4]);
      if (chunk_id == RECORDER_FMT)
        {
          if (chunk_size < sizeof(format))
            {
              ret = -EINVAL;
              goto out;
            }

          ret = recorder_read_exact(fd, format, sizeof(format));
          if (ret < 0)
            {
              goto out;
            }

          wav->format = recorder_le16(&format[0]);
          wav->channels = recorder_le16(&format[2]);
          wav->samplerate = recorder_le32(&format[4]);
          wav->byterate = recorder_le32(&format[8]);
          wav->samplebits = recorder_le16(&format[14]);
          chunk_size -= sizeof(format);
        }
      else if (chunk_id == RECORDER_DATA)
        {
          wav->data_offset = lseek(fd, 0, SEEK_CUR);
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

  ret = -EINVAL;

out:
  close(fd);
  if (ret < 0)
    {
      return ret;
    }

  if (wav->format != 1 || wav->channels != RECORDER_CHANNELS ||
      wav->samplebits != RECORDER_SAMPLE_BITS || wav->samplerate == 0 ||
      wav->byterate == 0 ||
      wav->data_offset == 0 || wav->data_bytes == 0)
    {
      return -ENOTSUP;
    }

  return OK;
}

static int recorder_update_waveform(FAR struct recorder_app_s *app,
                                    FAR const char *path,
                                    FAR struct recorder_wav_s *wav,
                                    bool playback)
{
  FAR uint8_t *peaks;
  uint8_t sample_bytes[512];
  uint32_t samples;
  uint32_t samples_per_bar;
  uint32_t point_count;
  uint32_t bar;
  int fd;
  int ret = OK;

  point_count = playback ? RECORDER_PLAYBACK_WAVE_POINTS :
                           RECORDER_WAVE_BARS;
  peaks = calloc(point_count, sizeof(*peaks));
  if (peaks == NULL)
    {
      return -ENOMEM;
    }

  fd = open(path, O_RDONLY);
  if (fd < 0)
    {
      ret = -errno;
      goto out_free;
    }

  samples = wav->data_bytes / 2;
  samples_per_bar = samples / point_count;
  if (samples_per_bar == 0)
    {
      samples_per_bar = 1;
    }

  for (bar = 0; bar < point_count; bar++)
    {
      uint32_t start = bar * samples_per_bar;
      uint32_t count = samples_per_bar;
      uint32_t peak = 0;
      uint32_t remaining;

      if (start >= samples)
        {
          break;
        }

      if (start + count > samples)
        {
          count = samples - start;
        }

      if (count > 1024)
        {
          count = 1024;
        }

      if (lseek(fd, wav->data_offset + start * 2, SEEK_SET) < 0)
        {
          ret = -errno;
          break;
        }

      remaining = count;
      while (remaining > 0)
        {
          uint32_t chunk = remaining;
          uint32_t offset;

          if (chunk > sizeof(sample_bytes) / 2)
            {
              chunk = sizeof(sample_bytes) / 2;
            }

          ret = recorder_read_exact(fd, sample_bytes, chunk * 2);
          if (ret < 0)
            {
              goto out;
            }

          for (offset = 0; offset < chunk * 2; offset += 2)
            {
              int16_t sample = (int16_t)recorder_le16(&sample_bytes[offset]);
              uint32_t magnitude = sample < 0 ? -(int32_t)sample : sample;

              if (magnitude > peak)
                {
                  peak = magnitude;
                }
            }

          remaining -= chunk;
        }

      peaks[bar] = (uint8_t)((peak * 100u) / 32768u);
      if (peaks[bar] == 0 && peak != 0)
        {
          peaks[bar] = 1;
        }
    }

out:
  close(fd);
  if (ret == OK)
    {
      pthread_mutex_lock(&app->lock);
      if (playback)
        {
          free(app->playback_peaks);
          app->playback_peaks = peaks;
          app->playback_peak_count = point_count;
          peaks = NULL;
        }
      else
        {
          memcpy(app->peaks, peaks, point_count);
        }

      pthread_mutex_unlock(&app->lock);
    }

out_free:
  free(peaks);
  return ret;
}

static bool recorder_is_wav_name(FAR const char *name)
{
  size_t length = strlen(name);

  return length > 4 && strcasecmp(name + length - 4, ".wav") == 0;
}

static int recorder_file_compare(FAR const void *left, FAR const void *right)
{
  FAR const struct recorder_file_s *first = left;
  FAR const struct recorder_file_s *second = right;

  return strcmp(second->name, first->name);
}

static int recorder_scan_library(FAR struct recorder_app_s *app)
{
  FAR struct recorder_file_s *files = NULL;
  FAR struct recorder_file_s *grown;
  struct recorder_wav_s wav;
  FAR struct dirent *entry;
  DIR *directory;
  size_t count = 0;
  unsigned int index;
  unsigned int next_file_index = 1;
  int ret;

  ret = recorder_mount_sd();
  if (ret < 0)
    {
      return ret;
    }

  directory = opendir(RECORDER_SD_MOUNTPOINT);
  if (directory == NULL)
    {
      return -errno;
    }

  while ((entry = readdir(directory)) != NULL)
    {
      struct recorder_file_s file;

      if (!recorder_is_wav_name(entry->d_name))
        {
          continue;
        }

      memset(&file, 0, sizeof(file));
      ret = snprintf(file.path, sizeof(file.path), "%s/%s",
                     RECORDER_SD_MOUNTPOINT, entry->d_name);
      if (ret < 0 || ret >= (int)sizeof(file.path))
        {
          continue;
        }

      ret = recorder_parse_wav(file.path, &wav);
      if (ret < 0)
        {
          continue;
        }

      file.app = app;
      file.valid = true;
      file.duration_ms = (uint32_t)(((uint64_t)wav.data_bytes * 1000u) /
                                    wav.byterate);
      file.data_bytes = wav.data_bytes;
      strlcpy(file.name, entry->d_name, sizeof(file.name));
      if (sscanf(file.name, "recorder_%u.wav", &index) == 1 &&
          index >= next_file_index && index < 999)
        {
          next_file_index = index + 1;
        }

      grown = realloc(files, (count + 1) * sizeof(*files));
      if (grown == NULL)
        {
          free(files);
          closedir(directory);
          return -ENOMEM;
        }

      files = grown;
      files[count++] = file;
    }

  closedir(directory);
  if (count > 1)
    {
      qsort(files, count, sizeof(*files), recorder_file_compare);
    }

  free(app->files);
  app->files = files;
  app->file_count = count;
  app->next_file_index = next_file_index;
  return OK;
}

static void recorder_update_live_waveform(FAR struct recorder_app_s *app)
{
  uint32_t sequence;
  uint8_t peak;

  if (d13x_dmic_get_peak(&sequence, &peak) < 0)
    {
      return;
    }

  pthread_mutex_lock(&app->lock);
  if (sequence != app->live_peak_sequence)
    {
      memmove(app->peaks, app->peaks + 1, sizeof(app->peaks) - 1);
      app->peaks[RECORDER_WAVE_BARS - 1] = peak;
      app->live_peak_sequence = sequence;
    }

  pthread_mutex_unlock(&app->lock);
}

static void recorder_set_status(FAR struct recorder_app_s *app,
                                FAR const char *text)
{
  pthread_mutex_lock(&app->lock);
  strlcpy(app->status, text, sizeof(app->status));
  pthread_mutex_unlock(&app->lock);
}

static void recorder_set_label_text(lv_obj_t *label, FAR const char *text)
{
  if (strcmp(lv_label_get_text(label), text) != 0)
    {
      lv_label_set_text(label, text);
    }
}

static FAR void *recorder_record_worker(FAR void *argument)
{
  FAR struct recorder_app_s *app = argument;
  FAR struct nxrecorder_s *recorder;
  struct recorder_wav_s wav;
  char path[CONFIG_PATH_MAX];
  uint64_t start_deadline_ms;
  uint32_t peak_sequence;
  uint32_t sequence;
  uint8_t peak;
  int ret;

  ret = recorder_mount_sd();
  if (ret < 0)
    {
      pthread_mutex_lock(&app->lock);
      snprintf(app->status, sizeof(app->status), "SD card: %s",
               strerror(-ret));
      app->state = RECORDER_IDLE;
      app->worker_active = false;
      app->view = RECORDER_VIEW_LIBRARY;
      pthread_mutex_unlock(&app->lock);
      return NULL;
    }

  ret = recorder_reserve_path(app, path, sizeof(path));
  if (ret < 0)
    {
      pthread_mutex_lock(&app->lock);
      snprintf(app->status, sizeof(app->status), "File: %s",
               strerror(-ret));
      app->state = RECORDER_IDLE;
      app->worker_active = false;
      app->view = RECORDER_VIEW_LIBRARY;
      pthread_mutex_unlock(&app->lock);
      return NULL;
    }

  recorder = nxrecorder_create();
  if (recorder == NULL)
    {
      unlink(path);
      recorder_set_status(app, "Recorder allocation failed");
      pthread_mutex_lock(&app->lock);
      app->state = RECORDER_IDLE;
      app->worker_active = false;
      app->view = RECORDER_VIEW_LIBRARY;
      pthread_mutex_unlock(&app->lock);
      return NULL;
    }

  ret = nxrecorder_setdevice(recorder, RECORDER_CAPTURE_DEVICE);
  if (ret < 0)
    {
      pthread_mutex_lock(&app->lock);
      snprintf(app->status, sizeof(app->status), "Mic: %s", strerror(-ret));
      app->state = RECORDER_IDLE;
      app->worker_active = false;
      app->view = RECORDER_VIEW_LIBRARY;
      pthread_mutex_unlock(&app->lock);
      nxrecorder_release(recorder);
      unlink(path);
      return NULL;
    }

  d13x_dmic_get_peak(&peak_sequence, &peak);
  ret = nxrecorder_recordinternal(recorder, path, AUDIO_FMT_PCM,
                                  RECORDER_CHANNELS, RECORDER_SAMPLE_BITS,
                                  RECORDER_SAMPLE_RATE, 0);
  if (ret < 0)
    {
      pthread_mutex_lock(&app->lock);
      snprintf(app->status, sizeof(app->status), "Record: %s",
               strerror(-ret));
      app->state = RECORDER_IDLE;
      app->worker_active = false;
      app->view = RECORDER_VIEW_LIBRARY;
      pthread_mutex_unlock(&app->lock);
      nxrecorder_release(recorder);
      unlink(path);
      return NULL;
    }

  pthread_mutex_lock(&app->lock);
  app->recorder = recorder;
  app->duration_ms = 0;
  app->total_duration_ms = 0;
  strlcpy(app->path, path, sizeof(app->path));
  strlcpy(app->status, "Starting microphone", sizeof(app->status));
  memset(app->peaks, 0, sizeof(app->peaks));
  pthread_mutex_unlock(&app->lock);

  start_deadline_ms = recorder_milliseconds() + RECORDER_START_TIMEOUT_MS;
  for (; ; )
    {
      bool quit;
      int start_status;

      usleep(20000);
      pthread_mutex_lock(&app->lock);
      quit = app->quit;
      pthread_mutex_unlock(&app->lock);
      if (quit)
        {
          break;
        }

      start_status = nxrecorder_getstartstatus(recorder);
      if (start_status < 0 && start_status != -EINPROGRESS)
        {
          nxrecorder_stop(recorder);
          nxrecorder_release(recorder);
          unlink(path);
          pthread_mutex_lock(&app->lock);
          app->recorder = NULL;
          app->stop_requested = false;
          app->state = RECORDER_IDLE;
          app->worker_active = false;
          app->view = RECORDER_VIEW_LIBRARY;
          app->library_dirty = true;
          snprintf(app->status, sizeof(app->status), "Mic start: %s",
                   strerror(-start_status));
          pthread_mutex_unlock(&app->lock);
          return NULL;
        }

      if (recorder_milliseconds() >= start_deadline_ms)
        {
          nxrecorder_stop(recorder);
          nxrecorder_release(recorder);
          unlink(path);
          pthread_mutex_lock(&app->lock);
          app->recorder = NULL;
          app->stop_requested = false;
          app->state = RECORDER_IDLE;
          app->worker_active = false;
          app->view = RECORDER_VIEW_LIBRARY;
          app->library_dirty = true;
          strlcpy(app->status, "Microphone start timeout",
                  sizeof(app->status));
          pthread_mutex_unlock(&app->lock);
          return NULL;
        }

      d13x_dmic_get_peak(&sequence, &peak);
      if (start_status == OK && sequence != peak_sequence &&
          nxrecorder_getbyteswritten(recorder) > 0)
        {
          pthread_mutex_lock(&app->lock);
          app->live_peak_sequence = sequence;
          app->peaks[RECORDER_WAVE_BARS - 1] = peak;
          app->state = RECORDER_RECORDING;
          app->state_start_ms = recorder_milliseconds();
          strlcpy(app->status, "Recording", sizeof(app->status));
          pthread_mutex_unlock(&app->lock);
          break;
        }
    }

  for (; ; )
    {
      bool stop;
      bool recording;
      uint32_t elapsed;
      uint64_t now_ms;

      usleep(100000);
      now_ms = recorder_milliseconds();

      pthread_mutex_lock(&app->lock);
      recording = app->state == RECORDER_RECORDING;
      elapsed = app->duration_ms;
      if (recording)
        {
          elapsed += (uint32_t)(now_ms - app->state_start_ms);
        }

      stop = app->stop_requested || app->quit ||
             elapsed >= RECORDER_MAX_SECONDS * 1000u;
      pthread_mutex_unlock(&app->lock);

      if (recording)
        {
          recorder_update_live_waveform(app);
        }

      if (stop)
        {
          break;
        }
    }

  pthread_mutex_lock(&app->lock);
  if (app->state == RECORDER_RECORDING)
    {
      app->duration_ms +=
          (uint32_t)(recorder_milliseconds() - app->state_start_ms);
    }

  pthread_mutex_unlock(&app->lock);

  ret = nxrecorder_stop(recorder);
  nxrecorder_release(recorder);

  if (ret == OK)
    {
      ret = recorder_parse_wav(path, &wav);
      if (ret == OK)
        {
          recorder_update_waveform(app, path, &wav, false);
        }
    }
  else
    {
      unlink(path);
    }

  pthread_mutex_lock(&app->lock);
  app->recorder = NULL;
  app->stop_requested = false;
  app->state = RECORDER_IDLE;
  app->worker_active = false;
  app->view = RECORDER_VIEW_LIBRARY;
  app->library_dirty = true;
  if (ret < 0)
    {
      snprintf(app->status, sizeof(app->status), "WAV: %s", strerror(-ret));
      app->duration_ms = 0;
    }
  else
    {
      app->duration_ms = (uint32_t)(((uint64_t)wav.data_bytes * 1000u) /
                         wav.byterate);
      app->total_duration_ms = app->duration_ms;
      snprintf(app->status, sizeof(app->status), "Saved %lu KB",
               (unsigned long)((wav.data_bytes + 1023u) / 1024u));
    }

  pthread_mutex_unlock(&app->lock);
  return NULL;
}

static FAR void *recorder_play_worker(FAR void *argument)
{
  FAR struct recorder_play_arg_s *arg = argument;
  FAR struct recorder_app_s *app = arg->app;
  FAR struct nxplayer_s *player;
  bool cancel = false;
  bool stopped = false;
  int ret;

  player = nxplayer_create();
  if (player == NULL)
    {
      recorder_set_status(app, "Player allocation failed");
      pthread_mutex_lock(&app->lock);
      app->state = RECORDER_IDLE;
      app->worker_active = false;
      pthread_mutex_unlock(&app->lock);
      free(arg);
      return NULL;
    }

  ret = nxplayer_setdevice(player, RECORDER_PLAY_DEVICE);
  if (ret < 0)
    {
      pthread_mutex_lock(&app->lock);
      snprintf(app->status, sizeof(app->status), "Speaker: %s",
               strerror(-ret));
      app->state = RECORDER_IDLE;
      app->worker_active = false;
      pthread_mutex_unlock(&app->lock);
      nxplayer_release(player);
      free(arg);
      return NULL;
    }

  ret = nxplayer_playfile(player, arg->path, AUDIO_FMT_PCM,
                          AUDIO_SUBFMT_PCM_S16_LE);
  if (ret == OK)
    {
      pthread_mutex_lock(&app->lock);
      app->player = player;
      cancel = app->stop_requested || app->quit;
      if (cancel)
        {
          app->state = RECORDER_STOPPING;
          strlcpy(app->status, "Stopping", sizeof(app->status));
        }
      else
        {
          app->state = RECORDER_PLAYING;
          app->state_start_ms = recorder_milliseconds();
          strlcpy(app->status, "Playing", sizeof(app->status));
        }

      pthread_mutex_unlock(&app->lock);

      if (cancel)
        {
          stopped = true;
          ret = nxplayer_stop(player);
        }

      for (; ; )
        {
          bool stop;

          if (cancel)
            {
              break;
            }

          pthread_mutex_lock(&app->lock);
          stop = app->stop_requested || app->quit;
          pthread_mutex_unlock(&app->lock);

          if (stop)
            {
              stopped = true;
              ret = nxplayer_stop(player);
              break;
            }

          if (player->state == RECORDER_PLAYER_IDLE)
            {
              break;
            }

          usleep(100000);
        }
    }

  pthread_mutex_lock(&app->lock);
  app->player = NULL;
  pthread_mutex_unlock(&app->lock);

  nxplayer_release(player);

  pthread_mutex_lock(&app->lock);
  if (app->state == RECORDER_PLAYING ||
      app->state == RECORDER_STOPPING)
    {
      app->duration_ms = stopped ?
          (uint32_t)(recorder_milliseconds() - app->state_start_ms) :
          app->total_duration_ms;
      if (app->duration_ms > app->total_duration_ms)
        {
          app->duration_ms = app->total_duration_ms;
        }
    }

  app->stop_requested = false;
  app->state = RECORDER_IDLE;
  app->worker_active = false;
  if (app->return_to_library)
    {
      app->view = RECORDER_VIEW_LIBRARY;
      app->return_to_library = false;
    }

  if (ret < 0)
    {
      snprintf(app->status, sizeof(app->status), "Play: %s",
               strerror(-ret));
    }
  else
    {
      strlcpy(app->status, "Ready", sizeof(app->status));
    }

  pthread_mutex_unlock(&app->lock);
  free(arg);
  return NULL;
}

static int recorder_start_detached(FAR void *(*entry)(FAR void *),
                                   FAR void *argument)
{
  pthread_attr_t attr;
  pthread_t tid;
  int ret;

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  pthread_attr_setstacksize(&attr, RECORDER_WORKER_STACK);
  ret = pthread_create(&tid, &attr, entry, argument);
  pthread_attr_destroy(&attr);
  return ret == OK ? OK : -ret;
}

static void recorder_start_record(FAR struct recorder_app_s *app)
{
  int ret;

  pthread_mutex_lock(&app->lock);
  if (app->state != RECORDER_IDLE)
    {
      pthread_mutex_unlock(&app->lock);
      return;
    }

  app->state = RECORDER_BUSY;
  app->view = RECORDER_VIEW_RECORD;
  app->worker_active = true;
  app->stop_requested = false;
  app->duration_ms = 0;
  app->total_duration_ms = 0;
  app->path[0] = '\0';
  memset(app->peaks, 0, sizeof(app->peaks));
  strlcpy(app->status, "Preparing", sizeof(app->status));
  pthread_mutex_unlock(&app->lock);

  ret = recorder_start_detached(recorder_record_worker, app);
  if (ret < 0)
    {
      pthread_mutex_lock(&app->lock);
      snprintf(app->status, sizeof(app->status), "Thread: %s",
               strerror(-ret));
      app->state = RECORDER_IDLE;
      app->worker_active = false;
      app->view = RECORDER_VIEW_LIBRARY;
      pthread_mutex_unlock(&app->lock);
    }
}

static void recorder_start_play(FAR struct recorder_app_s *app)
{
  FAR struct recorder_play_arg_s *arg;
  int ret;

  arg = malloc(sizeof(*arg));
  if (arg == NULL)
    {
      recorder_set_status(app, "Out of memory");
      return;
    }

  pthread_mutex_lock(&app->lock);
  if (app->state != RECORDER_IDLE || app->path[0] == '\0')
    {
      pthread_mutex_unlock(&app->lock);
      free(arg);
      return;
    }

  arg->app = app;
  strlcpy(arg->path, app->path, sizeof(arg->path));
  app->state = RECORDER_BUSY;
  app->worker_active = true;
  app->stop_requested = false;
  app->duration_ms = 0;
  strlcpy(app->status, "Preparing", sizeof(app->status));
  pthread_mutex_unlock(&app->lock);

  ret = recorder_start_detached(recorder_play_worker, arg);
  if (ret < 0)
    {
      pthread_mutex_lock(&app->lock);
      snprintf(app->status, sizeof(app->status), "Thread: %s",
               strerror(-ret));
      app->state = RECORDER_IDLE;
      app->worker_active = false;
      pthread_mutex_unlock(&app->lock);
      free(arg);
    }
}

static void recorder_format_time(FAR char *buffer, size_t length,
                                 uint32_t milliseconds)
{
  uint32_t seconds = milliseconds / 1000u;

  snprintf(buffer, length, "%02lu:%02lu",
           (unsigned long)(seconds / 60u),
           (unsigned long)(seconds % 60u));
}

static FAR const char *recorder_basename(FAR const char *path)
{
  FAR const char *name = strrchr(path, '/');

  return name == NULL ? path : name + 1;
}

static void recorder_exit_event(lv_event_t *event)
{
  FAR struct recorder_app_s *app = lv_event_get_user_data(event);

  pthread_mutex_lock(&app->lock);
  app->quit = true;
  app->stop_requested = true;
  if (app->state != RECORDER_IDLE)
    {
      strlcpy(app->status, "Stopping", sizeof(app->status));
    }

  pthread_mutex_unlock(&app->lock);
}

static void recorder_new_event(lv_event_t *event)
{
  FAR struct recorder_app_s *app = lv_event_get_user_data(event);

  recorder_start_record(app);
}

static void recorder_record_pause_event(lv_event_t *event)
{
  FAR struct recorder_app_s *app = lv_event_get_user_data(event);
  uint64_t now_ms = recorder_milliseconds();
  int ret = OK;

  pthread_mutex_lock(&app->lock);
  if (app->state == RECORDER_RECORDING && app->recorder != NULL)
    {
      ret = nxrecorder_pause(app->recorder);
      if (ret == OK)
        {
          app->duration_ms += (uint32_t)(now_ms - app->state_start_ms);
          app->state = RECORDER_RECORD_PAUSED;
          strlcpy(app->status, "Paused", sizeof(app->status));
        }
    }
  else if (app->state == RECORDER_RECORD_PAUSED && app->recorder != NULL)
    {
      ret = nxrecorder_resume(app->recorder);
      if (ret == OK)
        {
          app->state_start_ms = now_ms;
          app->state = RECORDER_RECORDING;
          strlcpy(app->status, "Recording", sizeof(app->status));
        }
    }

  if (ret < 0)
    {
      snprintf(app->status, sizeof(app->status), "Audio: %s",
               strerror(-ret));
    }

  pthread_mutex_unlock(&app->lock);
}

static void recorder_record_stop_event(lv_event_t *event)
{
  FAR struct recorder_app_s *app = lv_event_get_user_data(event);

  pthread_mutex_lock(&app->lock);
  if (app->state == RECORDER_RECORDING ||
      app->state == RECORDER_RECORD_PAUSED || app->state == RECORDER_BUSY)
    {
      app->stop_requested = true;
      app->view = RECORDER_VIEW_LIBRARY;
      strlcpy(app->status, "Saving recording", sizeof(app->status));
    }

  pthread_mutex_unlock(&app->lock);
}

static void recorder_player_play_event(lv_event_t *event)
{
  FAR struct recorder_app_s *app = lv_event_get_user_data(event);
  enum recorder_state_e state;

  pthread_mutex_lock(&app->lock);
  state = app->state;
  pthread_mutex_unlock(&app->lock);

  if (state == RECORDER_IDLE)
    {
      recorder_start_play(app);
    }
}

static void recorder_player_back_event(lv_event_t *event)
{
  FAR struct recorder_app_s *app = lv_event_get_user_data(event);
  bool playing;

  pthread_mutex_lock(&app->lock);
  playing = app->state == RECORDER_PLAYING ||
            app->state == RECORDER_STOPPING || app->state == RECORDER_BUSY;
  if (playing)
    {
      app->return_to_library = true;
      strlcpy(app->status, "Playing", sizeof(app->status));
    }
  else
    {
      app->view = RECORDER_VIEW_LIBRARY;
      strlcpy(app->status, "Ready", sizeof(app->status));
    }

  pthread_mutex_unlock(&app->lock);
}

static void recorder_file_event(lv_event_t *event)
{
  FAR struct recorder_file_s *file = lv_event_get_user_data(event);
  FAR struct recorder_app_s *app = file->app;
  struct recorder_wav_s wav;
  int ret;

  pthread_mutex_lock(&app->lock);
  if (app->state != RECORDER_IDLE)
    {
      pthread_mutex_unlock(&app->lock);
      return;
    }

  strlcpy(app->status, "Loading waveform", sizeof(app->status));
  pthread_mutex_unlock(&app->lock);

  ret = recorder_parse_wav(file->path, &wav);
  if (ret == OK)
    {
      ret = recorder_update_waveform(app, file->path, &wav, true);
    }

  pthread_mutex_lock(&app->lock);
  if (ret < 0)
    {
      snprintf(app->status, sizeof(app->status), "WAV: %s", strerror(-ret));
    }
  else
    {
      strlcpy(app->path, file->path, sizeof(app->path));
      app->duration_ms = 0;
      app->total_duration_ms = file->duration_ms;
      app->view = RECORDER_VIEW_PLAYER;
      strlcpy(app->status, "Ready", sizeof(app->status));
    }

  pthread_mutex_unlock(&app->lock);
}

static lv_obj_t *recorder_panel(lv_obj_t *parent, int32_t x, int32_t y,
                                int32_t width, int32_t height,
                                uint32_t color)
{
  lv_obj_t *panel;

  panel = lv_obj_create(parent);
  lv_obj_set_pos(panel, x, y);
  lv_obj_set_size(panel, width, height);
  lv_obj_set_style_radius(panel, 6, 0);
  lv_obj_set_style_border_width(panel, 0, 0);
  lv_obj_set_style_bg_color(panel, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(panel, 0, 0);
  lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  return panel;
}

static lv_obj_t *recorder_button(lv_obj_t *parent, int32_t x, int32_t y,
                                 int32_t width, int32_t height,
                                 uint32_t color, FAR const char *text,
                                 lv_event_cb_t cb,
                                 FAR struct recorder_app_s *app,
                                 FAR lv_obj_t **label)
{
  lv_obj_t *button;

  button = lv_button_create(parent);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, width, height);
  lv_obj_set_style_radius(button, 6, 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x56616b),
                            LV_STATE_DISABLED);
  lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, app);
  *label = lv_label_create(button);
  lv_label_set_text(*label, text);
  lv_obj_set_style_text_font(*label, &lv_font_montserrat_24, 0);
  lv_obj_center(*label);
  return button;
}

static void recorder_use_pressed_event(FAR lv_obj_t *button,
                                       lv_event_cb_t cb,
                                       FAR struct recorder_app_s *app)
{
  lv_obj_remove_event_cb_with_user_data(button, cb, app);
  lv_obj_add_event_cb(button, cb, LV_EVENT_PRESSED, app);
}

static void recorder_wave_draw_event(lv_event_t *event)
{
  FAR struct recorder_wave_draw_s *wave = lv_event_get_user_data(event);
  lv_obj_t *object = lv_event_get_target_obj(event);
  lv_layer_t *layer = lv_event_get_layer(event);
  lv_area_t area;
  lv_draw_line_dsc_t line;
  unsigned int i;

  lv_obj_get_coords(object, &area);
  lv_draw_line_dsc_init(&line);
  line.base.layer = layer;
  line.color = lv_color_hex(wave->color);
  line.width = 7;
  line.round_start = 1;
  line.round_end = 1;

  for (i = 0; i < RECORDER_WAVE_BARS; i++)
    {
      int32_t height = 2 + (int32_t)((uint32_t)wave->peaks[i] *
                       (RECORDER_WAVE_HEIGHT - 12) / 100u);
      int32_t x = area.x1 + 18 + (int32_t)i * 9 + 3;
      int32_t center = area.y1 + 10 + RECORDER_WAVE_HEIGHT / 2;

      line.p1.x = x;
      line.p1.y = center - height / 2;
      line.p2.x = x;
      line.p2.y = center + height / 2;
      lv_draw_line(layer, &line);
    }

  if (wave->playhead)
    {
      line.color = lv_color_hex(0xe9eef2);
      line.width = 2;
      line.round_start = 0;
      line.round_end = 0;
      line.p1.x = area.x1 + 479;
      line.p1.y = area.y1 + 10;
      line.p2.x = area.x1 + 479;
      line.p2.y = area.y1 + 250;
      lv_draw_line(layer, &line);
    }
}

static lv_obj_t *recorder_create_wave(FAR lv_obj_t *parent, uint32_t color)
{
  FAR struct recorder_wave_draw_s *wave;
  lv_obj_t *object;

  wave = calloc(1, sizeof(*wave));
  if (wave == NULL)
    {
      return NULL;
    }

  wave->color = color;
  object = recorder_panel(parent, 0, 0, 960, 260, 0x20262b);
  lv_obj_add_event_cb(object, recorder_wave_draw_event,
                      LV_EVENT_DRAW_MAIN_END, wave);
  lv_obj_set_user_data(object, wave);
  return object;
}

static lv_obj_t *recorder_icon_button(lv_obj_t *parent, int32_t x, int32_t y,
                                      int32_t size, uint32_t color,
                                      FAR const char *symbol,
                                      lv_event_cb_t cb,
                                      FAR struct recorder_app_s *app)
{
  lv_obj_t *label;
  lv_obj_t *button;

  button = lv_button_create(parent);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, size, size);
  lv_obj_set_style_radius(button, 6, 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(color), 0);
  lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, app);
  label = lv_label_create(button);
  lv_label_set_text(label, symbol);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
  lv_obj_center(label);
  return button;
}

static lv_obj_t *recorder_create_header(FAR lv_obj_t *page,
                                        FAR const char *title,
                                        bool back,
                                        lv_event_cb_t event,
                                        FAR struct recorder_app_s *app)
{
  lv_obj_t *header = recorder_panel(page, 0, 0, 1024, 80, 0x20262b);
  lv_obj_t *label = lv_label_create(header);

  lv_label_set_text(label, title);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
  lv_obj_set_pos(label, back ? 88 : 28, 26);
  return recorder_icon_button(header, back ? 20 : 944, 12, 56, 0x353d43,
                              back ? "<" : "X", event, app);
}

static void recorder_create_ui(FAR struct recorder_ui_s *ui,
                               FAR struct recorder_app_s *app)
{
  lv_obj_t *screen;
  lv_obj_t *wave;
  lv_obj_t *label;
  lv_obj_t *player_back_button;

  memset(ui, 0, sizeof(*ui));
  ui->app = app;
  ui->shown_view = (enum recorder_view_e)-1;

  screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x14181b), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(screen, lv_color_hex(0xf2f4f1), 0);

  ui->library_page = recorder_panel(screen, 0, 0, 1024, 600, 0x14181b);
  recorder_create_header(ui->library_page, "Recordings", false,
                         recorder_exit_event, app);
  ui->library_count = lv_label_create(ui->library_page);
  lv_obj_set_pos(ui->library_count, 28, 94);
  lv_obj_set_style_text_color(ui->library_count, lv_color_hex(0xaeb8be), 0);
  ui->library_status = lv_label_create(ui->library_page);
  lv_obj_align(ui->library_status, LV_ALIGN_TOP_RIGHT, -28, 94);
  lv_obj_set_style_text_color(ui->library_status, lv_color_hex(0x75c68b), 0);
  ui->library_list = lv_list_create(ui->library_page);
  lv_obj_set_pos(ui->library_list, 24, 126);
  lv_obj_set_size(ui->library_list, 976, 372);
  lv_obj_set_style_radius(ui->library_list, 4, 0);
  lv_obj_set_style_bg_color(ui->library_list, lv_color_hex(0x20262b), 0);
  recorder_button(ui->library_page, 24, 516, 976, 68, 0xc94747,
                  "New recording", recorder_new_event,
                  app, &label);

  ui->record_page = recorder_panel(screen, 0, 0, 1024, 600, 0x14181b);
  recorder_create_header(ui->record_page, "New recording", true,
                         recorder_record_stop_event, app);
  ui->record_status = lv_label_create(ui->record_page);
  lv_obj_set_pos(ui->record_status, 32, 98);
  lv_obj_set_style_text_color(ui->record_status, lv_color_hex(0xe06060), 0);
  ui->record_time = lv_label_create(ui->record_page);
  lv_label_set_text(ui->record_time, "00:00");
  lv_obj_set_style_text_font(ui->record_time, &lv_font_montserrat_24, 0);
  lv_obj_align(ui->record_time, LV_ALIGN_TOP_RIGHT, -32, 92);
  ui->record_file = lv_label_create(ui->record_page);
  lv_obj_set_pos(ui->record_file, 32, 132);
  lv_obj_set_width(ui->record_file, 920);
  lv_label_set_long_mode(ui->record_file, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_color(ui->record_file, lv_color_hex(0xaeb8be), 0);
  wave = recorder_panel(ui->record_page, 32, 174, 960, 260, 0x20262b);
  ui->record_wave = recorder_create_wave(wave, 0xd95454);
  ui->record_pause_button = recorder_button(ui->record_page, 32, 466,
                                            456, 86, 0x3b4650,
                                            "Pause",
                                            recorder_record_pause_event, app,
                                            &ui->record_pause_label);
  ui->record_stop_button = recorder_button(ui->record_page, 536, 466,
                                           456, 86, 0xc94747,
                                           "Finish",
                                           recorder_record_stop_event, app,
                                           &label);

  ui->player_page = recorder_panel(screen, 0, 0, 1024, 600, 0x14181b);
  player_back_button = recorder_create_header(ui->player_page, "Recording",
                                              true,
                                              recorder_player_back_event,
                                              app);
  recorder_use_pressed_event(player_back_button,
                             recorder_player_back_event, app);
  ui->player_status = lv_label_create(ui->player_page);
  lv_obj_set_pos(ui->player_status, 32, 98);
  lv_obj_set_style_text_color(ui->player_status, lv_color_hex(0x62a9e8), 0);
  ui->player_time = lv_label_create(ui->player_page);
  lv_label_set_text(ui->player_time, "00:00 / 00:00");
  lv_obj_set_style_text_font(ui->player_time, &lv_font_montserrat_24, 0);
  lv_obj_align(ui->player_time, LV_ALIGN_TOP_RIGHT, -32, 92);
  ui->player_file = lv_label_create(ui->player_page);
  lv_obj_set_pos(ui->player_file, 32, 132);
  lv_obj_set_width(ui->player_file, 920);
  lv_label_set_long_mode(ui->player_file, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_color(ui->player_file, lv_color_hex(0xaeb8be), 0);
  wave = recorder_panel(ui->player_page, 32, 174, 960, 260, 0x20262b);
  ui->player_wave = recorder_create_wave(wave, 0x4c9cdb);
  ui->player_progress = recorder_panel(ui->player_page, 32, 446,
                                       960, 10, 0x30383e);
  ui->player_progress_fill = recorder_panel(ui->player_progress, 0, 0,
                                            1, 10, 0x4c9cdb);
  ui->player_play_button = recorder_button(ui->player_page, 282, 476,
                                           460, 76, 0x347db8,
                                           "Play",
                                           recorder_player_play_event, app,
                                           &ui->player_play_label);
  recorder_use_pressed_event(ui->player_play_button,
                             recorder_player_play_event, app);
}

static void recorder_render_wave(FAR lv_obj_t *object,
                                 FAR const uint8_t *peaks, bool playhead)
{
  FAR struct recorder_wave_draw_s *wave = lv_obj_get_user_data(object);

  if (wave == NULL)
    {
      return;
    }

  memcpy(wave->peaks, peaks, sizeof(wave->peaks));
  wave->playhead = playhead;
  lv_obj_invalidate(object);
}

static void recorder_render_player_wave(FAR struct recorder_ui_s *ui,
                                        FAR const uint8_t *peaks,
                                        uint16_t peak_count,
                                        uint32_t elapsed_ms,
                                        uint32_t total_ms,
                                        bool scrolling)
{
  uint8_t visible[RECORDER_WAVE_BARS];
  unsigned int bar;

  memset(visible, 0, sizeof(visible));
  if (peak_count == 0)
    {
      recorder_render_wave(ui->player_wave, visible, false);
      return;
    }

  if (!scrolling || total_ms == 0)
    {
      for (bar = 0; bar < RECORDER_WAVE_BARS; bar++)
        {
          uint32_t begin = (uint32_t)bar * peak_count /
                           RECORDER_WAVE_BARS;
          uint32_t end = (uint32_t)(bar + 1) * peak_count /
                         RECORDER_WAVE_BARS;
          uint32_t index;

          if (end <= begin)
            {
              end = begin + 1;
            }

          for (index = begin; index < end && index < peak_count; index++)
            {
              if (peaks[index] > visible[bar])
                {
                  visible[bar] = peaks[index];
                }
            }
        }
    }
  else
    {
      int64_t position;

      position = (int64_t)elapsed_ms * (peak_count - 1) * 65536 /
                 total_ms;
      for (bar = 0; bar < RECORDER_WAVE_BARS; bar++)
        {
          int64_t sample = position +
              ((int32_t)bar - RECORDER_WAVE_BARS / 2) * 65536;

          if (sample >= 0 && sample <= (int64_t)(peak_count - 1) * 65536)
            {
              uint32_t index = (uint32_t)(sample >> 16);
              uint32_t fraction = (uint32_t)sample & 0xffff;
              uint32_t value = peaks[index];

              if (index + 1 < peak_count)
                {
                  value = (value * (65536 - fraction) +
                           peaks[index + 1] * fraction) >> 16;
                }

              visible[bar] = value;
            }
        }
    }

  recorder_render_wave(ui->player_wave, visible, scrolling);
}

static void recorder_refresh_library(FAR struct recorder_ui_s *ui)
{
  FAR struct recorder_app_s *app = ui->app;
  char detail[96];
  char duration[16];
  size_t index;
  int ret;

  lv_obj_clean(ui->library_list);
  ret = recorder_scan_library(app);
  if (ret < 0)
    {
      snprintf(detail, sizeof(detail), "SD card: %s", strerror(-ret));
      lv_label_set_text(ui->library_status, detail);
      lv_label_set_text(ui->library_count, "No recordings available");
      return;
    }

  snprintf(detail, sizeof(detail), "%lu recording%s",
           (unsigned long)app->file_count, app->file_count == 1 ? "" : "s");
  lv_label_set_text(ui->library_count, detail);
  lv_label_set_text(ui->library_status, app->status);

  if (app->file_count == 0)
    {
      lv_list_add_text(ui->library_list,
                       "No WAV recordings on the SD card");
      return;
    }

  for (index = 0; index < app->file_count; index++)
    {
      FAR struct recorder_file_s *file = &app->files[index];
      lv_obj_t *button;

      recorder_format_time(duration, sizeof(duration), file->duration_ms);
      snprintf(detail, sizeof(detail), "%s    %s    %lu KB", file->name,
               duration, (unsigned long)((file->data_bytes + 1023u) / 1024u));
      button = lv_list_add_button(ui->library_list, NULL, detail);
      lv_obj_set_height(button, 64);
      lv_obj_add_event_cb(button, recorder_file_event, LV_EVENT_CLICKED, file);
    }
}

static void recorder_show_view(FAR struct recorder_ui_s *ui,
                               enum recorder_view_e view)
{
  lv_obj_add_flag(ui->library_page, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui->record_page, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui->player_page, LV_OBJ_FLAG_HIDDEN);

  if (view == RECORDER_VIEW_RECORD)
    {
      lv_obj_remove_flag(ui->record_page, LV_OBJ_FLAG_HIDDEN);
    }
  else if (view == RECORDER_VIEW_PLAYER)
    {
      lv_obj_remove_flag(ui->player_page, LV_OBJ_FLAG_HIDDEN);
    }
  else
    {
      lv_obj_remove_flag(ui->library_page, LV_OBJ_FLAG_HIDDEN);
    }

  ui->shown_view = view;
}

static void recorder_update_ui(FAR struct recorder_ui_s *ui,
                               FAR struct recorder_app_s *app)
{
  enum recorder_state_e state;
  enum recorder_view_e view;
  char status[96];
  char path[CONFIG_PATH_MAX];
  char elapsed_text[16];
  char total_text[16];
  char player_time[40];
  uint8_t peaks[RECORDER_WAVE_BARS];
  FAR const uint8_t *playback_peaks;
  uint16_t playback_peak_count;
  uint32_t duration_ms;
  uint32_t total_duration_ms;
  uint64_t start_ms;
  bool library_dirty;
  bool worker_active;
  bool view_changed;

  pthread_mutex_lock(&app->lock);
  state = app->state;
  view = app->view;
  strlcpy(status, app->status, sizeof(status));
  strlcpy(path, app->path, sizeof(path));
  memcpy(peaks, app->peaks, sizeof(peaks));
  playback_peaks = app->playback_peaks;
  playback_peak_count = app->playback_peak_count;
  duration_ms = app->duration_ms;
  total_duration_ms = app->total_duration_ms;
  start_ms = app->state_start_ms;
  library_dirty = app->library_dirty;
  worker_active = app->worker_active;
  if (!worker_active)
    {
      app->library_dirty = false;
    }

  pthread_mutex_unlock(&app->lock);

  if (state == RECORDER_RECORDING)
    {
      duration_ms += (uint32_t)(recorder_milliseconds() - start_ms);
    }
  else if (state == RECORDER_PLAYING)
    {
      duration_ms = (uint32_t)(recorder_milliseconds() - start_ms);
      if (duration_ms > total_duration_ms)
        {
          duration_ms = total_duration_ms;
        }
    }

  view_changed = ui->shown_view != view;
  if (view_changed)
    {
      recorder_show_view(ui, view);
    }

  if (!worker_active && (library_dirty ||
      (view == RECORDER_VIEW_LIBRARY && view_changed)))
    {
      recorder_refresh_library(ui);
    }

  recorder_format_time(elapsed_text, sizeof(elapsed_text), duration_ms);
  recorder_format_time(total_text, sizeof(total_text), total_duration_ms);

  if (view == RECORDER_VIEW_RECORD)
    {
      recorder_set_label_text(ui->record_status, status);
      recorder_set_label_text(ui->record_time, elapsed_text);
      recorder_set_label_text(ui->record_file,
                              path[0] == '\0' ? "Preparing WAV file" :
                              recorder_basename(path));
      recorder_render_wave(ui->record_wave, peaks, false);

      if (state == RECORDER_RECORD_PAUSED)
        {
          recorder_set_label_text(ui->record_pause_label, "Resume");
        }
      else
        {
          recorder_set_label_text(ui->record_pause_label, "Pause");
        }

      if (state == RECORDER_BUSY)
        {
          lv_obj_add_state(ui->record_pause_button, LV_STATE_DISABLED);
        }
      else
        {
          lv_obj_clear_state(ui->record_pause_button, LV_STATE_DISABLED);
        }
    }
  else if (view == RECORDER_VIEW_PLAYER)
    {
      snprintf(player_time, sizeof(player_time), "%s / %s",
               elapsed_text, total_text);
      recorder_set_label_text(ui->player_status, status);
      recorder_set_label_text(ui->player_time, player_time);
      recorder_set_label_text(ui->player_file, recorder_basename(path));
      recorder_render_player_wave(ui, playback_peaks, playback_peak_count,
                                  duration_ms, total_duration_ms,
                                  state == RECORDER_PLAYING);
      lv_obj_set_width(ui->player_progress_fill,
                       total_duration_ms == 0 ? 1 :
                       (int32_t)((uint64_t)duration_ms * 960u /
                                 total_duration_ms));

      if (state == RECORDER_PLAYING || state == RECORDER_BUSY)
        {
          recorder_set_label_text(ui->player_play_label, "Playing");
          lv_obj_add_state(ui->player_play_button, LV_STATE_DISABLED);
        }
      else
        {
          recorder_set_label_text(ui->player_play_label, "Play");
          lv_obj_clear_state(ui->player_play_button, LV_STATE_DISABLED);
        }
    }
  else
    {
      recorder_set_label_text(ui->library_status, status);
    }
}

static int recorder_run(void)
{
  struct recorder_app_s app;
  struct recorder_ui_s ui;
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;
  uint64_t last_update_ms;
  bool done;

  memset(&app, 0, sizeof(app));
  pthread_mutex_init(&app.lock, NULL);
  app.state = RECORDER_IDLE;
  app.view = RECORDER_VIEW_LIBRARY;
  app.library_dirty = true;
  app.next_file_index = 1;
  strlcpy(app.status, "Ready", sizeof(app.status));

  if (lv_is_initialized())
    {
      printf("recorder: LVGL is already initialized\n");
      pthread_mutex_destroy(&app.lock);
      return 1;
    }

  lv_init();
  lv_nuttx_dsc_init(&info);
  info.input_path = "/dev/input0";
  lv_nuttx_init(&info, &result);
  if (result.disp == NULL || result.indev == NULL)
    {
      printf("recorder: failed to initialize LVGL devices\n");
      lv_nuttx_deinit(&result);
      lv_deinit();
      pthread_mutex_destroy(&app.lock);
      return 1;
    }

  lv_indev_set_display(result.indev, result.disp);
  recorder_create_ui(&ui, &app);
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(result.disp);

  last_update_ms = recorder_milliseconds();
  do
    {
      uint64_t now_ms = recorder_milliseconds();
      uint32_t delay;

      if (now_ms - last_update_ms >= 100u)
        {
          recorder_update_ui(&ui, &app);
          last_update_ms = now_ms;
        }

      delay = lv_timer_handler();
      if (delay == 0 || delay > 20)
        {
          delay = 20;
        }

      usleep(delay * 1000u);

      pthread_mutex_lock(&app.lock);
      done = app.quit && !app.worker_active && app.state == RECORDER_IDLE;
      pthread_mutex_unlock(&app.lock);
    }
  while (!done);

  lv_nuttx_deinit(&result);
  free(lv_obj_get_user_data(ui.record_wave));
  free(lv_obj_get_user_data(ui.player_wave));
  lv_deinit();
  free(app.playback_peaks);
  free(app.files);
  pthread_mutex_destroy(&app.lock);
  return 0;
}

static FAR void *recorder_ui_thread(FAR void *argument)
{
  intptr_t result;

  UNUSED(argument);
  result = recorder_run();
  return (FAR void *)result;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  pthread_attr_t attr;
  struct sched_param priority;
  FAR void *thread_result;
  pthread_t thread;
  int ret;

  UNUSED(argc);
  UNUSED(argv);

  pthread_attr_init(&attr);
  priority.sched_priority = RECORDER_UI_PRIORITY;
  pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  pthread_attr_setschedpolicy(&attr, SCHED_RR);
  pthread_attr_setschedparam(&attr, &priority);
  pthread_attr_setstacksize(&attr,
                            CONFIG_LVX_USE_DEMO_CONTEST2026_094_RECORDER_STACKSIZE);
  ret = pthread_create(&thread, &attr, recorder_ui_thread, NULL);
  pthread_attr_destroy(&attr);
  if (ret != OK)
    {
      printf("recorder: failed to create UI thread: %s\n", strerror(ret));
      return 1;
    }

  ret = pthread_join(thread, &thread_result);
  if (ret != OK)
    {
      printf("recorder: failed to join UI thread: %s\n", strerror(ret));
      return 1;
    }

  return (int)(intptr_t)thread_result;
}
