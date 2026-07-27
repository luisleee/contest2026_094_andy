# D13x demo88-nor NSH

This board port targets the ArtInChip D133CBS demo88-nor board and boots
openvela/NuttX from SPI NOR into the on-chip SRAM. The console is UART0 at
115200 baud.

## Build

Run from the openvela workspace root:

```bash
rm -rf cmake_out/demo88-nor_nsh
./build.sh contest2026_094_andy/board/d13x/demo88-nor/configs/nsh \
  --cmake -j8
```

The ELF entry point must be `0x30044100`, matching `pack/d13x_os.its`:

```bash
prebuilts/gcc/linux-x86_64/riscv-none-elf/bin/riscv-none-elf-readelf \
  -h cmake_out/demo88-nor_nsh/nuttx | grep 'Entry point'
```

## Pack

Copy the CMake outputs to the ArtInChip pack inputs, then run the packer:

```bash
cp -f cmake_out/demo88-nor_nsh/nuttx nuttx/nuttx.elf
cp -f cmake_out/demo88-nor_nsh/nuttx.manifest nuttx/nuttx.manifest

cd vendor/artinchip/pack
./pack.sh demo88-nor d13x
```

The burnable image is:

```text
vendor/artinchip/pack/prebuilt/d13x_demo88-nor_v1.0.0.img
```

An `img2simg` error about `libselinux.so.1` only affects optional sparse image
conversion. The raw `.img` above is still generated and is the AiBurn input.

## Burn And Verify

1. Hold the board BOOT key while connecting USB.
2. Select the generated `.img` in AiBurn and burn the complete image.
3. Connect UART0 using 115200 baud, 8 data bits, no parity, 1 stop bit, and no
   flow control.
4. Power-cycle the board after the burn completes.

A successful boot reaches:

```text
NuttShell (NSH)
nsh>
```

Check the expanded diagnostic shell before peripheral tests:

```text
nsh> ls /dev
nsh> ps
nsh> free
nsh> uptime
nsh> uname -a
nsh> fdinfo
```

Tab completion is enabled. The shell also provides `cat`, `cd`, `hexdump`,
`pidof`, `pwd`, `sleep`, `time`, and `usleep`.

At the prompt, run `help` to verify UART receive interrupts and task context
switching, not only console output.

Then verify the first board peripheral milestone:

```text
nsh> ls /dev
/dev/i2c2 must be present

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

On the earlier image, `0x14` returned product ID `911` and firmware `0x1060`.
After configuration, status still stayed `0x00` and PA.11 stayed high. The
board now performs the exact demo88 factory PA.10/PA.11 sequence, selects
`0x5d`, and downloads ArtInChip's complete 1024x600 five-point configuration
before registering I2C2. The default command also prints full configuration
checksum/fresh state and PA.11 input level. Raw touch mode
supports five points, runs for 15 seconds by default, and
accepts a duration from 1 through 300 seconds. Test a tap, drag to all four
corners, release, and multiple simultaneous fingers. Then rerun `hello_app`
to check that I2C polling has not regressed UART, timer, task creation, or task
exit. This raw diagnostic does not open `/dev/input0`, so it leaves the PA.11
GPIO IRQ disabled.

The factory `0x5d` reset sequence and raw touch reporting have passed hardware
testing. Run `gt911_test draw 30` to clear `/dev/fb0`, draw corner/center
targets, and show a separate colored trail for each track ID. Touch all five
targets for coverage PASS and use at least two fingers to verify
`max_points >= 2`. The final drawing remains on the panel after the command.

Verify the standard touchscreen separately. It publishes one primary pointer
for GUI use while `gt911_test` remains the five-point diagnostic. PA.11 uses
a falling-edge IRQ to request worker-side reads, with a 100 ms status poll as
a missed-edge watchdog:

```text
nsh> ls /dev
/dev/input0 must be present
nsh> getevent -t /dev/input0
```

Tap, drag, and release, then confirm standard DOWN/MOVE/UP samples. Stop
`getevent` with Ctrl-C before running `lvgl_test`; `/dev/input0` intentionally
allows only one reader. Its close log must show `irq > 0` and
`ready_frames > 0` for IRQ hardware acceptance.

Verify the onboard buzzer separately:

```text
nsh> ls /dev
/dev/pwm1 must be present

nsh> buzzer_test
Buzzer on: 4000 Hz, 50% duty, 1000 ms
Buzzer off
```

The optional form is `buzzer_test <frequency_hz> <duration_ms>`. Accepted
ranges are 100..10000 Hz and 1..5000 ms. Confirm that the buzzer becomes
silent after the command, then rerun `hello_app` and `gt911_test`. The default
effect has passed real-board testing.

Verify the J18 1024x600 LVDS framebuffer:

```text
nsh> ls /dev
/dev/fb0 must be present

nsh> fb_test
Framebuffer color bars: RGB565 1024x600, addr=0x40000000, stride=2048
```

The expected output is eight vertical bars: red, yellow, green, cyan, blue,
magenta, white, and black. This display image is build- and pack-verified.
PE.13 active-high panel/backlight enable after a 20 ms DE/LVDS stabilization
delay has passed hardware testing; the color bars still need to be recorded.
The framebuffer is black at boot, so execute `fb_test` before evaluating the
display.

Verify LVGL separately after the color bars are correct:

```text
nsh> free
nsh> lvgl_test
lvgl_test: LVGL 9.1.0, 1024x600 RGB565, /dev/input0, duration=30 seconds
lvgl_test: touch events button=... slider=... switch=... drag=...
lvgl_test: completed; final frame remains on the panel
nsh> free
```

Click the button, move the slider, toggle the switch, and drag the yellow
block. The four event counters must all be nonzero.

The test renders color swatches, fixed geometry, an animated progress bar, a
moving block, and a frame counter through LVGL's NuttX `/dev/fb0` backend. An
optional duration from 5 to 300 seconds may be supplied. Touch is not part of
this test; the GT911 LVGL input path is a later board milestone.
