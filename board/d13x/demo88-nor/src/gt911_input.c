/****************************************************************************
 * contest2026_094_andy/board/d13x/demo88-nor/src/gt911_input.c
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
#include <limits.h>
#include <poll.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <nuttx/arch.h>
#include <nuttx/fs/fs.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/input/touchscreen.h>
#include <nuttx/irq.h>
#include <nuttx/kthread.h>
#include <nuttx/mutex.h>
#include <nuttx/semaphore.h>
#include <nuttx/signal.h>
#include <nuttx/spinlock.h>

#include <arch/chip/irq.h>

#include <aic_hal_gpio.h>

#include "board.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define GT911_INPUT_FREQUENCY          400000
#define GT911_INPUT_STATUS_REG         0x814e
#define GT911_INPUT_POINT_REG          0x814f
#define GT911_INPUT_READY              (1u << 7)
#define GT911_INPUT_POINT_MASK         0x0f
#define GT911_INPUT_MAX_POINTS         5
#define GT911_INPUT_POINT_SIZE         8
#define GT911_INPUT_WIDTH              1024
#define GT911_INPUT_HEIGHT             600
#define GT911_INPUT_WORKER_US          5000
#define GT911_INPUT_IDLE_US            20000
#define GT911_INPUT_ERROR_US           50000
#define GT911_INPUT_WATCHDOG_TICKS     20
#define GT911_INPUT_QUEUE_DEPTH        16
#define GT911_INPUT_STACKSIZE          3072

#define GT911_INPUT_GPIO_GROUP         0
#define GT911_INPUT_INTERRUPT_PIN      11
#define GT911_INPUT_INTERRUPT_MASK     (1u << GT911_INPUT_INTERRUPT_PIN)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct gt911_input_point_s
{
  uint8_t id;
  uint16_t x;
  uint16_t y;
  uint16_t size;
};

struct gt911_input_dev_s
{
  FAR struct i2c_master_s *i2c;
  mutex_t lock;
  sem_t waitsem;
  FAR struct pollfd *fds;
  struct touch_sample_s queue[GT911_INPUT_QUEUE_DEPTH];
  uint8_t address;
  uint8_t head;
  uint8_t tail;
  uint8_t count;
  uint8_t track_id;
  uint16_t x;
  uint16_t y;
  uint16_t size;
  volatile uint32_t irq_count;
  uint32_t watchdog_count;
  uint32_t ready_frames;
  volatile bool irq_pending;
  bool opened;
  bool pressed;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int gt911_input_open(FAR struct file *filep);
static int gt911_input_close(FAR struct file *filep);
static ssize_t gt911_input_read(FAR struct file *filep, FAR char *buffer,
                                size_t buflen);
static int gt911_input_ioctl(FAR struct file *filep, int cmd,
                             unsigned long arg);
static int gt911_input_poll(FAR struct file *filep, FAR struct pollfd *fds,
                            bool setup);
static int gt911_input_interrupt(int irq, FAR void *context, FAR void *arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct gt911_input_dev_s g_gt911_input;

static const struct file_operations g_gt911_input_fops =
{
  .open  = gt911_input_open,
  .close = gt911_input_close,
  .read  = gt911_input_read,
  .ioctl = gt911_input_ioctl,
  .poll  = gt911_input_poll,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint16_t gt911_input_getle16(FAR const uint8_t *buffer)
{
  return (uint16_t)buffer[0] | (uint16_t)buffer[1] << 8;
}

static int gt911_input_read_reg(FAR struct gt911_input_dev_s *dev,
                                uint16_t reg, FAR uint8_t *buffer,
                                size_t length)
{
  uint8_t regaddr[2] =
  {
    reg >> 8,
    reg & 0xff
  };

  struct i2c_msg_s messages[2] =
  {
    {
      .frequency = GT911_INPUT_FREQUENCY,
      .addr = dev->address,
      .flags = I2C_M_NOSTOP,
      .buffer = regaddr,
      .length = sizeof(regaddr),
    },
    {
      .frequency = GT911_INPUT_FREQUENCY,
      .addr = dev->address,
      .flags = I2C_M_READ,
      .buffer = buffer,
      .length = length,
    }
  };

  int ret;

  ret = I2C_TRANSFER(dev->i2c, messages, 2);
  if (ret < 0)
    {
      return ret;
    }

  return ret == 0 || ret == 2 ? OK : -EIO;
}

static int gt911_input_clear_status(FAR struct gt911_input_dev_s *dev)
{
  uint8_t buffer[3] =
  {
    GT911_INPUT_STATUS_REG >> 8,
    GT911_INPUT_STATUS_REG & 0xff,
    0
  };

  struct i2c_msg_s message =
  {
    .frequency = GT911_INPUT_FREQUENCY,
    .addr = dev->address,
    .flags = 0,
    .buffer = buffer,
    .length = sizeof(buffer),
  };

  int ret;

  ret = I2C_TRANSFER(dev->i2c, &message, 1);
  if (ret < 0)
    {
      return ret;
    }

  return ret == 0 || ret == 1 ? OK : -EIO;
}

static void gt911_input_reset_queue(FAR struct gt911_input_dev_s *dev)
{
  dev->head = 0;
  dev->tail = 0;
  dev->count = 0;
  nxsem_reset(&dev->waitsem, 0);
}

static void gt911_input_enqueue(FAR struct gt911_input_dev_s *dev,
                                uint8_t flags, uint8_t id,
                                uint16_t x, uint16_t y, uint16_t size)
{
  FAR struct touch_sample_s *sample;
  int16_t extent;

  if (dev->count >= GT911_INPUT_QUEUE_DEPTH)
    {
      return;
    }

  extent = size > INT16_MAX ? INT16_MAX : (int16_t)size;
  sample = &dev->queue[dev->tail];
  memset(sample, 0, sizeof(*sample));
  sample->npoints = 1;
  sample->point[0].id = id;
  sample->point[0].flags = flags | TOUCH_ID_VALID | TOUCH_POS_VALID |
                           TOUCH_SIZE_VALID;
  sample->point[0].x = x;
  sample->point[0].y = y;
  sample->point[0].h = extent;
  sample->point[0].w = extent;
  sample->point[0].timestamp = touch_get_time();

  dev->tail = (dev->tail + 1) % GT911_INPUT_QUEUE_DEPTH;
  dev->count++;
  nxsem_post(&dev->waitsem);
  poll_notify(&dev->fds, 1, POLLIN);
}

static void gt911_input_decode(FAR const uint8_t *buffer,
                               FAR struct gt911_input_point_s *point)
{
  point->id = buffer[0] & GT911_INPUT_POINT_MASK;
  point->x = gt911_input_getle16(&buffer[1]);
  point->y = gt911_input_getle16(&buffer[3]);
  point->size = gt911_input_getle16(&buffer[5]);
}

static void gt911_input_process_points(FAR struct gt911_input_dev_s *dev,
                                       FAR const uint8_t *point_data,
                                       uint8_t points)
{
  struct gt911_input_point_s point;
  unsigned int selected = 0;
  unsigned int i;
  bool found = false;

  if (points == 0)
    {
      if (dev->pressed)
        {
          gt911_input_enqueue(dev, TOUCH_UP, dev->track_id, dev->x,
                              dev->y, dev->size);
          dev->pressed = false;
        }

      return;
    }

  if (dev->pressed)
    {
      for (i = 0; i < points; i++)
        {
          if ((point_data[i * GT911_INPUT_POINT_SIZE] &
               GT911_INPUT_POINT_MASK) == dev->track_id)
            {
              selected = i;
              found = true;
              break;
            }
        }

      if (!found)
        {
          gt911_input_enqueue(dev, TOUCH_UP, dev->track_id, dev->x,
                              dev->y, dev->size);
          dev->pressed = false;
        }
    }

  gt911_input_decode(&point_data[selected * GT911_INPUT_POINT_SIZE], &point);
  if (!dev->pressed)
    {
      gt911_input_enqueue(dev, TOUCH_DOWN, point.id, point.x, point.y,
                          point.size);
      dev->pressed = true;
    }
  else if (point.x != dev->x || point.y != dev->y ||
           point.size != dev->size)
    {
      gt911_input_enqueue(dev, TOUCH_MOVE, point.id, point.x, point.y,
                          point.size);
    }

  dev->track_id = point.id;
  dev->x = point.x;
  dev->y = point.y;
  dev->size = point.size;
}

static bool gt911_input_take_interrupt(FAR struct gt911_input_dev_s *dev)
{
  irqstate_t flags;
  bool pending;

  flags = enter_critical_section();
  pending = dev->irq_pending;
  dev->irq_pending = false;
  leave_critical_section(flags);
  return pending;
}

static int gt911_input_interrupt(int irq, FAR void *context, FAR void *arg)
{
  FAR struct gt911_input_dev_s *dev = arg;
  unsigned int enabled;
  unsigned int status;

  (void)irq;
  (void)context;

  if (hal_gpio_group_get_irq_stat(GT911_INPUT_GPIO_GROUP, &status) >= 0 &&
      hal_gpio_group_get_irq_en(GT911_INPUT_GPIO_GROUP, &enabled) >= 0 &&
      (status & enabled & GT911_INPUT_INTERRUPT_MASK) != 0)
    {
      hal_gpio_clr_irq_stat(GT911_INPUT_GPIO_GROUP,
                            GT911_INPUT_INTERRUPT_PIN);
      dev->irq_pending = true;
      dev->irq_count++;
    }

  return OK;
}

static int gt911_input_worker(int argc, FAR char *argv[])
{
  FAR struct gt911_input_dev_s *dev = &g_gt911_input;
  uint8_t point_data[GT911_INPUT_MAX_POINTS * GT911_INPUT_POINT_SIZE];
  unsigned int errors = 0;
  unsigned int watchdog_ticks = 0;
  uint8_t status;
  uint8_t points;
  bool irq_pending;
  int ret;

  (void)argc;
  (void)argv;

  for (; ; )
    {
      nxmutex_lock(&dev->lock);
      if (!dev->opened)
        {
          watchdog_ticks = 0;
          nxmutex_unlock(&dev->lock);
          nxsig_usleep(GT911_INPUT_IDLE_US);
          continue;
        }

      irq_pending = gt911_input_take_interrupt(dev);
      if (!irq_pending &&
          watchdog_ticks < GT911_INPUT_WATCHDOG_TICKS - 1)
        {
          watchdog_ticks++;
          nxmutex_unlock(&dev->lock);
          nxsig_usleep(GT911_INPUT_WORKER_US);
          continue;
        }

      if (!irq_pending)
        {
          dev->watchdog_count++;
        }

      watchdog_ticks = 0;
      ret = gt911_input_read_reg(dev, GT911_INPUT_STATUS_REG, &status, 1);
      if (ret >= 0 && (status & GT911_INPUT_READY) != 0)
        {
          points = status & GT911_INPUT_POINT_MASK;
          if (points > GT911_INPUT_MAX_POINTS)
            {
              ret = -EPROTO;
            }
          else if (points > 0)
            {
              ret = gt911_input_read_reg(dev, GT911_INPUT_POINT_REG,
                                         point_data,
                                         points * GT911_INPUT_POINT_SIZE);
            }

          if (gt911_input_clear_status(dev) < 0 && ret >= 0)
            {
              ret = -EIO;
            }

          if (ret >= 0)
            {
              gt911_input_process_points(dev, point_data, points);
              dev->ready_frames++;
            }
        }

      nxmutex_unlock(&dev->lock);

      if (ret < 0)
        {
          errors++;
          if (errors == 1 || errors % 100 == 0)
            {
              syslog(LOG_ERR, "[D13X] GT911 input polling failed: %d\n",
                     ret);
            }

          nxsig_usleep(GT911_INPUT_ERROR_US);
        }
      else
        {
          errors = 0;
          nxsig_usleep(GT911_INPUT_WORKER_US);
        }
    }

  return OK;
}

static int gt911_input_open(FAR struct file *filep)
{
  FAR struct gt911_input_dev_s *dev = filep->f_inode->i_private;
  int ret;

  nxmutex_lock(&dev->lock);
  if (dev->opened)
    {
      ret = -EBUSY;
    }
  else
    {
      gt911_input_reset_queue(dev);
      dev->pressed = false;
      ret = gt911_input_clear_status(dev);
      if (ret >= 0)
        {
          dev->irq_count = 0;
          dev->watchdog_count = 0;
          dev->ready_frames = 0;
          dev->irq_pending = false;
          dev->opened = true;
          hal_gpio_clr_irq_stat(GT911_INPUT_GPIO_GROUP,
                                GT911_INPUT_INTERRUPT_PIN);
          ret = hal_gpio_enable_irq(GT911_INPUT_GPIO_GROUP,
                                    GT911_INPUT_INTERRUPT_PIN);
          if (ret >= 0)
            {
              up_enable_irq(D13X_IRQ_GPIOA);
            }
          else
            {
              dev->opened = false;
            }
        }
    }

  nxmutex_unlock(&dev->lock);
  return ret;
}

static int gt911_input_close(FAR struct file *filep)
{
  FAR struct gt911_input_dev_s *dev = filep->f_inode->i_private;
  uint32_t watchdog_count;
  uint32_t ready_frames;
  uint32_t irq_count;

  nxmutex_lock(&dev->lock);
  hal_gpio_disable_irq(GT911_INPUT_GPIO_GROUP,
                       GT911_INPUT_INTERRUPT_PIN);
  up_disable_irq(D13X_IRQ_GPIOA);
  hal_gpio_clr_irq_stat(GT911_INPUT_GPIO_GROUP,
                        GT911_INPUT_INTERRUPT_PIN);
  dev->opened = false;
  dev->pressed = false;
  dev->irq_pending = false;
  dev->fds = NULL;
  irq_count = dev->irq_count;
  watchdog_count = dev->watchdog_count;
  ready_frames = dev->ready_frames;
  gt911_input_reset_queue(dev);
  nxmutex_unlock(&dev->lock);

  syslog(LOG_INFO,
         "[D13X] GT911 input: irq=%lu, watchdog=%lu, ready_frames=%lu\n",
         (unsigned long)irq_count, (unsigned long)watchdog_count,
         (unsigned long)ready_frames);
  return OK;
}

static ssize_t gt911_input_read(FAR struct file *filep, FAR char *buffer,
                                size_t buflen)
{
  FAR struct gt911_input_dev_s *dev = filep->f_inode->i_private;
  FAR struct touch_sample_s *sample;
  int ret;

  if (buffer == NULL || buflen < sizeof(struct touch_sample_s))
    {
      return -EINVAL;
    }

  if ((filep->f_oflags & O_NONBLOCK) != 0)
    {
      ret = nxsem_trywait(&dev->waitsem);
    }
  else
    {
      ret = nxsem_wait_uninterruptible(&dev->waitsem);
    }

  if (ret < 0)
    {
      return ret;
    }

  nxmutex_lock(&dev->lock);
  if (!dev->opened)
    {
      ret = -ENODEV;
    }
  else if (dev->count == 0)
    {
      ret = -EAGAIN;
    }
  else
    {
      sample = &dev->queue[dev->head];
      memcpy(buffer, sample, sizeof(*sample));
      dev->head = (dev->head + 1) % GT911_INPUT_QUEUE_DEPTH;
      dev->count--;
      ret = sizeof(*sample);
    }

  nxmutex_unlock(&dev->lock);
  return ret;
}

static int gt911_input_ioctl(FAR struct file *filep, int cmd,
                             unsigned long arg)
{
  struct touch_resolution_s resolution;

  (void)filep;

  switch (cmd)
    {
      case TSIOC_GETMAXPOINTS:
        if (arg == 0)
          {
            return -EINVAL;
          }

        *(FAR uint8_t *)(uintptr_t)arg = 1;
        return OK;

      case TSIOC_GETRESOLUTION:
        if (arg == 0)
          {
            return -EINVAL;
          }

        resolution.res_x = GT911_INPUT_WIDTH;
        resolution.res_y = GT911_INPUT_HEIGHT;
        memcpy((FAR void *)(uintptr_t)arg, &resolution, sizeof(resolution));
        return OK;

      case TSIOC_GETFREQUENCY:
        if (arg == 0)
          {
            return -EINVAL;
          }

        *(FAR uint32_t *)(uintptr_t)arg = GT911_INPUT_FREQUENCY;
        return OK;

      default:
        return -ENOTTY;
    }
}

static int gt911_input_poll(FAR struct file *filep, FAR struct pollfd *fds,
                            bool setup)
{
  FAR struct gt911_input_dev_s *dev = filep->f_inode->i_private;
  pollevent_t eventset = 0;
  int ret = OK;

  nxmutex_lock(&dev->lock);
  if (setup)
    {
      if (dev->fds != NULL)
        {
          ret = -EBUSY;
        }
      else
        {
          dev->fds = fds;
          fds->priv = &dev->fds;
          if (dev->count > 0)
            {
              eventset = POLLIN;
            }

          poll_notify(&dev->fds, 1, eventset);
        }
    }
  else if (fds->priv != NULL)
    {
      dev->fds = NULL;
      fds->priv = NULL;
    }

  nxmutex_unlock(&dev->lock);
  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int d13x_gt911_input_register(FAR struct i2c_master_s *i2c,
                              uint8_t address, FAR const char *devpath)
{
  FAR struct gt911_input_dev_s *dev = &g_gt911_input;
  int ret;

  if (i2c == NULL || devpath == NULL)
    {
      return -EINVAL;
    }

  memset(dev, 0, sizeof(*dev));
  dev->i2c = i2c;
  dev->address = address;
  nxmutex_init(&dev->lock);
  nxsem_init(&dev->waitsem, 0, 0);

  hal_gpio_direction_input(GT911_INPUT_GPIO_GROUP,
                           GT911_INPUT_INTERRUPT_PIN);
  hal_gpio_set_bias_pull(GT911_INPUT_GPIO_GROUP,
                         GT911_INPUT_INTERRUPT_PIN, PIN_PULL_DIS);
  ret = hal_gpio_set_irq_mode(GT911_INPUT_GPIO_GROUP,
                              GT911_INPUT_INTERRUPT_PIN,
                              PIN_IRQ_MODE_EDGE_FALLING);
  if (ret < 0)
    {
      nxsem_destroy(&dev->waitsem);
      nxmutex_destroy(&dev->lock);
      return ret;
    }

  hal_gpio_disable_irq(GT911_INPUT_GPIO_GROUP,
                       GT911_INPUT_INTERRUPT_PIN);
  hal_gpio_clr_irq_stat(GT911_INPUT_GPIO_GROUP,
                        GT911_INPUT_INTERRUPT_PIN);
  up_disable_irq(D13X_IRQ_GPIOA);
  ret = irq_attach(D13X_IRQ_GPIOA, gt911_input_interrupt, dev);
  if (ret < 0)
    {
      nxsem_destroy(&dev->waitsem);
      nxmutex_destroy(&dev->lock);
      return ret;
    }

  ret = register_driver(devpath, &g_gt911_input_fops, 0666, dev);
  if (ret < 0)
    {
      irq_detach(D13X_IRQ_GPIOA);
      nxsem_destroy(&dev->waitsem);
      nxmutex_destroy(&dev->lock);
      return ret;
    }

  ret = kthread_create("gt911_poll", SCHED_PRIORITY_DEFAULT,
                       GT911_INPUT_STACKSIZE, gt911_input_worker, NULL);
  if (ret < 0)
    {
      unregister_driver(devpath);
      irq_detach(D13X_IRQ_GPIOA);
      nxsem_destroy(&dev->waitsem);
      nxmutex_destroy(&dev->lock);
      return ret;
    }

  return OK;
}
