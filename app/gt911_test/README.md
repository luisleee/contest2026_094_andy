# gt911_test

This directory is linked to
`packages/demos/contest2026_094_gt911_test` by the team manifest.

The command verifies the demo88-nor I2C2 board path and reads the onboard
GT911 product ID, firmware information, and active configuration without
registering the full touchscreen driver.

```text
nsh> gt911_test
GT911 found at 0x5d, product ID: 911. (39 31 31 00)
Firmware: 0x1060, sensor resolution: 0x0, vendor: 0xff
Config: version=0x6b, output=1024x600, max_touches=5, module_switch1=0x0d
Config checksum: stored=0x6f, expected=0x6f, fresh=0x..
Interrupt: PA.11=HIGH
```

The command probes both legal addresses. Before board-controlled reset, the
tested controller was present at `0x14`; the demo88 factory reset sequence
selects `0x5d`.

The board uses PA.10/PA.11 for reset/address selection and downloads a complete
1024x600 five-point configuration during bring-up. Use raw polling mode to
validate all controller points independently of the single-pointer NuttX
input driver:

```text
nsh> gt911_test touch 30
Touch monitor: 30 seconds, poll=10 ms, display=off. Tap targets, drag, and try multiple fingers.
FRAME points=1
  DOWN id=0 x=512 y=300 size=42
FRAME points=1
  MOVE id=0 x=540 y=318 size=45
  UP id=0 x=540 y=318
Touch monitor complete: ready_frames=..., touch_frames=..., samples=..., max_points=1
```

The default duration is 15 seconds; accepted values are 1 through 300
seconds. Each ready frame is acknowledged by clearing register `0x814e`.
Point records start at `0x814f` and contain track ID, X, Y, and touch area.
The command supports all five GT911 points and intentionally remains in
polling mode; while no frame is ready, it prints the raw status and PA.11
input level once per second. Because it does not open `/dev/input0`, the
standard input driver's PA.11 GPIO IRQ remains disabled during this test.

Visual mode clears `/dev/fb0`, draws a grid with targets at the four corners
and center, and leaves a different RGB565 trail for each track ID:

```text
nsh> gt911_test draw 30
Touch monitor: 30 seconds, poll=10 ms, display=on. Tap targets, drag, and try multiple fingers.
Events: down=... move=... up=..., ids=0x....
Observed range: x=..... y=.....
Coverage: TL=yes TR=yes C=yes BL=yes BR=yes (PASS)
```

Touch all five target regions to obtain coverage PASS. Try at least two
simultaneous fingers and require `max_points` to reach 2 or more. The final
test frame remains visible after the command returns.
