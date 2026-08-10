/****************************************************************************
 * contest2026_094_andy/board/d13x/demo88-nor/src/board_bringup.c
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <syslog.h>

#include <nuttx/i2c/i2c_master.h>
#include <nuttx/input/buttons.h>
#include <nuttx/video/fb.h>

#include <arch/chip/d13x_i2c.h>
#include <arch/chip/d13x_pwm.h>
#include <arch/chip/d13x_sdmc.h>

#include <aic_utils.h>
#include "board.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int d13x_board_bringup(void)
{
  int result = OK;
  int ret;

  aic_board_pinmux_init();

#ifdef CONFIG_D13X_SDMC1
  ret = d13x_sdmc1_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[D13X] failed to register /dev/mmcsd1: %d\n", ret);
      result = ret;
    }
  else
    {
      syslog(LOG_INFO,
             "[D13X] SDMC1 TF card registered as /dev/mmcsd1\n");
    }
#endif

#ifdef CONFIG_D13X_WDT
  extern int aic_wdt_initialize(void);

  ret = aic_wdt_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[D13X] failed to register /dev/watchdog0: %d\n",
             ret);
      result = ret;
    }
  else
    {
      syslog(LOG_INFO, "[D13X] watchdog registered as /dev/watchdog0\n");
    }
#endif

#ifdef CONFIG_INPUT_BUTTONS_LOWER
  ret = btn_lower_initialize("/dev/buttons");
  if (ret < 0)
    {
      syslog(LOG_ERR, "[D13X] failed to register /dev/buttons: %d\n", ret);
      result = ret;
    }
  else
    {
      syslog(LOG_INFO,
             "[D13X] WAKEUP key registered as /dev/buttons (PD.15)\n");
    }
#endif

#ifdef CONFIG_D13X_KEYADC
  ret = d13x_keyadc_register("/dev/dpad");
  if (ret < 0)
    {
      syslog(LOG_ERR, "[D13X] failed to register /dev/dpad: %d\n", ret);
      result = ret;
    }
  else
    {
      syslog(LOG_INFO,
             "[D13X] ADC direction keys registered as /dev/dpad (PA.2)\n");
    }
#endif

#ifdef CONFIG_D13X_PWM1
  ret = d13x_pwm1_initialize("/dev/pwm1");
  if (ret < 0)
    {
      syslog(LOG_ERR, "[D13X] failed to register /dev/pwm1: %d\n", ret);
      result = ret;
    }
  else
    {
      syslog(LOG_INFO, "[D13X] PWM1 registered as /dev/pwm1\n");
    }
#endif

#ifdef CONFIG_D13X_I2C2
  FAR struct i2c_master_s *i2c;

#ifdef CONFIG_D13X_TOUCH_GT911
  ret = d13x_gt911_reset();
  if (ret < 0)
    {
      syslog(LOG_ERR, "[D13X] GT911 reset failed: %d\n", ret);
      result = ret;
    }
  else
    {
      syslog(LOG_INFO, "[D13X] GT911 factory reset/address sequence done\n");
    }
#endif

  i2c = d13x_i2cbus_initialize(2);
  if (i2c == NULL)
    {
      syslog(LOG_ERR, "[D13X] failed to initialize I2C2\n");
      result = -ENODEV;
    }
  else
    {
#ifdef CONFIG_D13X_TOUCH_GT911
      uint8_t address;

      ret = d13x_gt911_configure(i2c, &address);
      if (ret < 0)
        {
          syslog(LOG_ERR, "[D13X] GT911 configuration failed: %d\n", ret);
          result = ret;
        }
      else
        {
          syslog(LOG_INFO,
                 "[D13X] GT911 configured at 0x%02x for 1024x600/5\n",
                 address);
#ifdef CONFIG_D13X_GT911_INPUT
          ret = d13x_gt911_input_register(i2c, address, "/dev/input0");
          if (ret < 0)
            {
              syslog(LOG_ERR,
                     "[D13X] failed to register /dev/input0: %d\n", ret);
              result = ret;
            }
          else
            {
              syslog(LOG_INFO,
                     "[D13X] GT911 registered as /dev/input0 "
                     "(PA.11 IRQ, watchdog fallback)\n");
            }
#endif
        }
#endif

      ret = i2c_register(i2c, 2);
      if (ret < 0)
        {
          syslog(LOG_ERR, "[D13X] failed to register /dev/i2c2: %d\n",
                 ret);
          result = ret;
        }
      else
        {
          syslog(LOG_INFO, "[D13X] I2C2 registered as /dev/i2c2\n");
        }
    }
#endif

#ifdef CONFIG_D13X_DISPLAY
  ret = fb_register(0, 0);
  if (ret < 0)
    {
      syslog(LOG_ERR, "[D13X] failed to register /dev/fb0: %d\n", ret);
      result = ret;
    }
  else
    {
      syslog(LOG_INFO, "[D13X] LVDS framebuffer registered as /dev/fb0\n");
    }
#endif

  return result;
}
