# D13x openvela Bring-up

Contest team `094/andy` port of openvela/NuttX to the ArtInChip D133CBS
demo88-nor board. The current target boots from 16 MiB SPI NOR, runs from the
on-chip SRAM, and exposes an interactive NSH console on UART0 at 115200 baud.

## Repository Layout

- `chip/d13x/`: E907 startup, CLIC, timer, UART0, and SoC definitions.
- `board/d13x/demo88-nor/`: board configuration, linker script, and pack data.
- `app/hello_app/`: terminal command used to verify builtin application support.
- `app/gt911_test/`: I2C2 command that probes the onboard GT911 product ID.
- `app/buzzer_test/`: bounded PWM1_A test for the onboard buzzer.
- `app/fb_test/`: RGB565 color-bar test for the J18 LVDS framebuffer.
- `app/lvgl_test/`: LVGL framebuffer, animation, and touch interaction test.
- `logs/`: exported AI coding logs and submission metadata.

The manifest maps these directories into the openvela workspace without
copying contest-owned source into vendor repositories.

## Build And Pack

Run from the openvela workspace root:

```bash
rm -rf cmake_out/demo88-nor_nsh
./build.sh contest2026_094_andy/board/d13x/demo88-nor/configs/nsh \
  --cmake -j8

cp -f cmake_out/demo88-nor_nsh/nuttx nuttx/nuttx.elf
cp -f cmake_out/demo88-nor_nsh/nuttx.manifest nuttx/nuttx.manifest

cd vendor/artinchip/pack
./pack.sh demo88-nor d13x
```

Burn this image with AiBurn:

```text
vendor/artinchip/pack/prebuilt/d13x_demo88-nor_v1.0.0.img
```

Use UART0 with `115200 8N1` and no flow control. A successful boot reaches:

```text
NuttShell (NSH)
nsh>
```

## Hello Command

The NSH configuration enables `hello_app` by default:

```text
nsh> hello_app
Hello from openvela contest 2026 team 094 (andy)!
```

The board-specific build and burn details are also documented in
`board/d13x/demo88-nor/README.md`.

## NSH Diagnostics

The compact configuration explicitly enables tab completion and the following
NSH diagnostics:

```text
cat cd fdinfo free hexdump ls pidof ps pwd
sleep time uname uptime usleep
```

Procfs exposes process, memory, and uptime data required by `ps`, `free`,
`fdinfo`, and related inspection commands.

## I2C2 And GT911 Test

The board late-initialization path configures PA.8/PA.9 and registers I2C2 as
`/dev/i2c2`. The lower-half uses polling and does not enable the I2C CLIC path.

```text
nsh> ls /dev
nsh> gt911_test
GT911 found at 0x5d, product ID: 911. (39 31 31 00)
nsh> gt911_test touch 30
Touch monitor: 30 seconds, poll=10 ms, display=off. Tap targets, drag, and try multiple fingers.
FRAME points=1
  DOWN id=0 x=... y=... size=...
FRAME points=1
  MOVE id=0 x=... y=... size=...
  UP id=0 x=... y=...
```

The earlier board image found the controller at `0x14` with firmware `0x1060`,
but its configuration registers were all zero and it produced no ready frames.
The first configured image used address `0x14`; its configuration read back
correctly, but status stayed `0x00` and PA.11 stayed high while touching. The
board now follows the exact demo88 factory reset sequence, selects `0x5d`, and
downloads ArtInChip's complete 1024x600 five-point configuration with a
calculated checksum. The default command reports firmware, the full config
checksum/fresh state, and PA.11 level. `touch` mode
polls raw ready frames at 10 ms intervals and prints up to five track IDs,
coordinates, and touch areas. Its default duration is 15 seconds, with an
accepted range of 1 through 300 seconds.

The factory `0x5d` sequence and raw touch reports have passed hardware
testing. Use `gt911_test draw 30` for the framebuffer-assisted test: it draws
five targets and five-color track-ID trails, then reports per-ID events,
observed coordinate range, maximum simultaneous points, and corner/center
coverage.

The board also registers a standard, single-pointer touchscreen node. Its
PA.11 falling-edge interrupt requests worker-side I2C reads only while one
client has the node open, so the raw five-point test remains available when
the node is closed. A 100 ms status poll covers missed edges, and the ISR does
not perform I2C or wake the scheduler directly.

```text
nsh> ls /dev
/dev/input0
nsh> getevent -t /dev/input0
```

Tap and drag to observe standard NuttX `TOUCH_DOWN`, `TOUCH_MOVE`, and
`TOUCH_UP` reports, then press Ctrl-C before starting `lvgl_test`. The close
log prints `irq`, `watchdog`, and `ready_frames`; require `irq > 0` when
validating the PA.11 path.

## PWM1 Buzzer Test

The board configures PE.11 as PWM1_A and registers `/dev/pwm1`. The hardware
manual specifies a 4 kHz PWM input for the MLT-7525 buzzer circuit.

```text
nsh> ls /dev
/dev/pwm1

nsh> buzzer_test
Buzzer on: 4000 Hz, 50% duty, 1000 ms
Buzzer off

nsh> buzzer_test 3000 250
```

Frequency is limited to 100..10000 Hz and duration to 1..5000 ms. The command
issues `PWMIOC_STOP` before it closes the device on every execution path.
The default buzzer effect has passed real-board testing.

## LVDS Framebuffer Test

The board configures PD.18..PD.27 for the J18 single-link LVDS panel and
registers one 1024x600 RGB565 framebuffer at the start of PSRAM. The OS still
runs from SRAM, and PSRAM is not part of the heap. PE.13 is driven high 20 ms
after DE/LVDS start, matching the official demo88-nor configuration and the
hardware-verified lladlam implementation.

```text
nsh> ls /dev
/dev/fb0

nsh> fb_test
Framebuffer color bars: RGB565 1024x600, addr=0x40000000, stride=2048
```

The interactive LVGL test opens both the framebuffer and touchscreen:

```text
nsh> lvgl_test 60
lvgl_test: LVGL 9.1.0, 1024x600 RGB565, /dev/input0, duration=60 seconds
lvgl_test: touch events button=... slider=... switch=... drag=...
```

Click the button, move the slider, toggle the switch, and drag the yellow
block. All four final counters should be nonzero.

The display implementation and command are build- and pack-verified, and the
PE.13 panel/backlight enable sequence has passed hardware testing. Color-bar
scanout still needs to be recorded. The framebuffer starts black; run
`fb_test` before judging scanout.

## LVGL Framebuffer Test

The image includes LVGL 9.1.0 and its NuttX framebuffer backend. It maps the
existing 1024x600 RGB565 `/dev/fb0` directly, so no second framebuffer is
required. pthread is explicitly retained because the LVGL NuttX initialization
layer uses it. The NuttX touchscreen backend opens `/dev/input0`.

```text
nsh> free
nsh> lvgl_test
lvgl_test: LVGL 9.1.0, 1024x600 RGB565, /dev/input0, duration=30 seconds
lvgl_test: touch events button=... slider=... switch=... drag=...
lvgl_test: completed; final frame remains on the panel
nsh> free
```

The display should show color swatches, interactive controls, an animated
progress bar, a moving block, and a frame counter. Use `lvgl_test 60` to select
a duration from 5 to 300 seconds.
