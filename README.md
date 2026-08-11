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
- `app/button_test/`: bounded `/dev/buttons` test for the PD.15 WAKEUP key.
- `app/dpad_test/`: bounded `/dev/dpad` test for the PA.2/GPAI2 direction keys.
- `app/pm_test/`: display-standby and PD.15 wake/restore test.
- `app/wdt_test/`: bounded keepalive and confirmed reset tests for the WDT.
- `app/rtc_test/`: RTC counter, UTC set, and alarm interrupt tests.
- `app/flash_test/`: SPI NOR checks and bounded `/data` persistence tests.
- `app/tf_test/`: SDMC1 TF-card mount, geometry, and read/write tests.
- `app/speaker_test/`: DSPK1 WAV playback and generated-tone tests.
- `app/mic_test/`: DMIC capture to WAV and immediate speaker loopback.
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
SHA-256: 2b9c2a82bb45775f573e30976e207ba4d29322775fc9c60d212c1f6f139d0d77
```

The corresponding ELF sizes are `text=442876`, `data=1712`, and `bss=86304`.
Its load segment uses `0x30044000..0x300b09af` for file-backed data and ends at
`0x300c5adf` after BSS/stack allocation.

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

## WAKEUP Button Test

The board registers the active-low PD.15 WAKEUP key through the standard
NuttX button upper-half at `/dev/buttons`. GPIOD raw IRQ 71 reports both press
and release edges, and the common driver applies 30 ms debounce.

```text
nsh> ls /dev
/dev/buttons
nsh> button_test 15
button_test: /dev/buttons supported=0x00000001 initial=released duration=15 seconds
WAKEUP PRESS
WAKEUP RELEASE
button_test: presses=1 releases=1 final=released
```

PD.15 conflicts with I2S_MCLK. RESET remains a hardware reset input and UBOOT
is not remuxed because it shares PA.0 with UART0 TX. WAKEUP has passed
real-board press/release testing.

## WAKEUP Standby Test

The first power-management milestone reserves WAKEUP for a controlled standby
test. It switches PD.15 from normal both-edge button reporting to a dedicated
falling-edge wake handler, powers off the panel, display engine, and LVDS
output, then restores them after WAKEUP or a bounded timeout.

```text
nsh> pm_test wake 30
pm_test: phase-1 wake standby, timeout=30 seconds
pm_test: display will turn off; press WAKEUP to resume
pm_test: CPU/PLL clocks remain running in this milestone
pm_test: WAKEUP resumed display after ... ms
pm_test: PASS; falling-edge wake and display restore verified
```

Do not hold WAKEUP while starting the command. The arm path waits up to five
seconds for release and refuses to take the IRQ while `/dev/buttons` owns it.
After the test, run `button_test 15` to verify that normal both-edge press and
release reporting was restored. This is display standby with wake validation,
not CPU light sleep; the current build still reuses D12x clock/reset tables.

## Direction-key Test

UP, DOWN, LEFT, and RIGHT share the PA.2/GPAI2 resistor ladder and are exposed
as a second standard NuttX button device. They are deliberately not merged
with WAKEUP.

```text
nsh> ls /dev
/dev/buttons
/dev/dpad
nsh> dpad_test 20
dpad_test: /dev/dpad supported=0x0000000f initial=NONE raw=4095 duration=20 seconds
UP PRESS raw=...
UP RELEASE raw=...
DOWN PRESS raw=...
DOWN RELEASE raw=...
LEFT PRESS raw=...
LEFT RELEASE raw=...
RIGHT PRESS raw=...
RIGHT RELEASE raw=...
dpad_test: up=1 down=1 left=1 right=1 final=NONE raw=...
```

The GPAI2 lower half samples every 10 ms, requires three equal classifications
before changing state, and then uses the common 30 ms NuttX button debounce.
PA.2 cannot be used as UART2 CTS while this device is enabled.

## Watchdog Test

The D13x WDT uses the 32 kHz clock, CMU register `0x20c`, reset bit 13, and
raw IRQ 64. It is registered through the standard NuttX watchdog upper half.

```text
nsh> ls /dev
/dev/watchdog0
nsh> wdt_test feed 8
wdt_test: feed mode, timeout=3 seconds, duration=8 seconds
wdt_test: feed=1 active=yes timeleft=... ms
...
wdt_test: PASS; 8 keepalives completed and watchdog stopped
```

Run reset validation only after feed mode passes:

```text
nsh> wdt_test reset 5 confirm
wdt_test: reset mode armed for 5 seconds
wdt_test: no keepalive will be sent; board should reboot
```

The board must reboot into NSH after about five seconds. The explicit
`confirm` argument prevents an accidental reset test.

## RTC Test

The D13x battery-backed RTC at `0x19030000` is the NuttX system realtime
source. It uses the 32 kHz clock and raw IRQ 50 for its alarm.

```text
nsh> rtc_test show
nsh> rtc_test count 5
nsh> rtc_test set 2026-07-30T22:00:00
nsh> rtc_test alarm 5
```

`set` accepts UTC in `YYYY-MM-DDTHH:MM:SS` form. After these tests pass,
reboot and use `show` to check warm-reset retention. For battery backup
validation, leave the coin cell installed, remove main power, wait, restore
power, and confirm that the RTC continued to advance. The set path waits for
`TCNT_INIT` completion; `show` and failed set operations print raw control,
initialization, time-set, and counter values for diagnosis. Counter, set,
alarm, warm-reset retention, and coin-cell retention have passed hardware
testing.

## SPI NOR And LittleFS Test

QSPI0 uses PB.0..PB.5 and registers `/dev/nor0` plus the partitions parsed
from the image header. Late boot mounts the existing 1 MiB `/dev/data`
LittleFS partition at `/data`; NuttX creates the mount-point inode as part of
`mount()`.

```text
nsh> flash_test info
nsh> flash_test read 0 64
nsh> flash_test verify 0 65536
nsh> mount
nsh> ls /data
nsh> flash_test fs write
nsh> flash_test fs check
nsh> reboot
nsh> flash_test fs check
nsh> flash_test fs clear
```

`info` reports JEDEC/SFDP data, geometry, and partition boundaries. `read`
prints a bounded hexadecimal dump, while `verify` reads the same range twice
and requires matching CRC32 values. `/dev/nor0` is an MTD inode rather than a
character device, so the test resolves it through the MTD registry instead of
calling `open()`; BCH is not required. Information, partition enumeration,
bounded read, and repeated-read CRC32 have passed hardware testing. The `fs`
commands only create, verify, or remove `/data/spi_test.bin`; they never issue
raw writes to the boot, environment, OS, or rodata partitions. Mounting,
write/readback, reboot retention, and cleanup have passed hardware testing.

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

The interactive LVGL test opens the framebuffer, touchscreen, and direction
key device. It does not open `/dev/buttons`.

```text
nsh> lvgl_test 60
lvgl_test: LVGL 9.1.0, 1024x600 RGB565, /dev/input0, /dev/dpad, duration=60 seconds
lvgl_test: touch events button=... slider=... switch=... drag=...
lvgl_test: dpad up=... down=... left=... right=... focus=...; WAKEUP unused
```

Click the button, move the slider, toggle the switch, and drag the yellow
block. Press every direction key and verify that the yellow focus outline
moves only among the six upper color swatches. UP/LEFT select the previous
swatch; DOWN/RIGHT select the next. The middle button, slider, switch, and
drag block remain touch-only. WAKEUP is reserved for system suspend/resume
management.

The display implementation and command are build- and pack-verified, and the
PE.13 panel/backlight enable sequence has passed hardware testing. Color-bar
scanout still needs to be recorded. The framebuffer starts black; run
`fb_test` before judging scanout.

## LVGL Framebuffer Test

The image includes LVGL 9.1.0 and its NuttX framebuffer backend. It maps the
existing 1024x600 RGB565 `/dev/fb0` directly, so no second framebuffer is
required. pthread is explicitly retained because the LVGL NuttX initialization
layer uses it. The NuttX touchscreen backend opens `/dev/input0`; a separate
LVGL keypad backend opens only `/dev/dpad` for focus navigation.

```text
nsh> free
nsh> lvgl_test
lvgl_test: LVGL 9.1.0, 1024x600 RGB565, /dev/input0, /dev/dpad, duration=30 seconds
lvgl_test: touch events button=... slider=... switch=... drag=...
lvgl_test: dpad up=... down=... left=... right=... focus=...; WAKEUP unused
lvgl_test: completed; final frame remains on the panel
nsh> free
```

The display should show color swatches, interactive controls, an animated
progress bar, a moving block, and a frame counter. Use `lvgl_test 60` to select
a duration from 5 to 300 seconds.

## Digital Microphone Test

The onboard PDM microphones use PD.16 for DMIC clock and PD.17 for DMIC data.
The board registers the capture lower-half as `/dev/audio/pcm0c`; speaker
playback remains `/dev/audio/pcm0p`. Capture uses DMA request 14 on channel 1.
The D13x DMA interrupt is raw CLIC source 32 and NuttX IRQ 48.

The first validation format is fixed at 16000 Hz, mono, signed 16-bit
little-endian PCM. The lower-half uses two aligned 8192-byte buffers as a
continuous cyclic DMA ring, matching the Luban D13x v1.x task layout. The two
32-byte-aligned descriptors are owned by the lower-half, linked in both
directions, cache-cleaned, and submitted directly to DMA channel 1. The test
records through the public nxrecorder API, then converts stock raw-PCM output
to a standard RIFF/WAV file in place. It also accepts nxrecorder variants that
already emit WAV, so the contest source does not require private recorder
interfaces.

```text
nsh> ls /dev/audio
/dev/audio/pcm0c
/dev/audio/pcm0p

nsh> mic_test record /data/mic.wav 3
nsh> ls -l /data/mic.wav
nsh> mic_test play /data/mic.wav
nsh> mic_test loop /data/mic.wav 3
```

The path defaults to `/data/mic.wav` and duration defaults to three seconds.
Accepted durations are one through five seconds to keep LittleFS use bounded.
The board has captured one 8192-byte DMA period and saved a valid 4140-byte WAV
containing 4096 bytes of converted mono S16 PCM. A later diagnostic proved the
CPU-side cyclic links were correct while DMA still loaded zero descriptor
fields. Disassembly exposed the cause: the vendor cache-range helper emits a
fixed-register T-Head cache instruction without constraining the compiler's
loop address to that register. The current image uses local cache maintenance
with an explicit `a5` constraint for both descriptors and capture buffers.
Continuous three-second capture and WAV finalization now pass on the physical
board. The speaker path is independently verified; `mic_test loop` remains the
recommended combined regression command.

## Next: Wired Ethernet

The next adaptation target is the populated GMAC0 RMII path. The board uses an
RTL8201F PHY at MDIO address 0 and an HR911105A RJ45. PE.0..PE.5 and PE.7..PE.9
use mux function 2 for RMII data and management signals. PE.6 is the active-low
PHY reset GPIO. PE.10 uses mux function 2 as CLK_OUT2 and supplies the PHY with
25 MHz; the PHY returns the 50 MHz RMII reference clock on PE.3.

Bring-up will be split into two checkpoints. The first enables GMAC0 clock and
reset, pinmux, SYSCFG RMII external-clock selection, PHY reset, and MDIO reads;
it must identify the RTL8201F at address 0 before packet DMA is enabled. The
second registers a NuttX Ethernet interface and adds cache-safe RX/TX descriptor
rings, IPv4/ARP/ICMP, DHCP, UDP/TCP, and ping tests. Ethernet buffers must reuse
the DMIC-proven constrained cache maintenance rather than the broken vendor
range helper.
