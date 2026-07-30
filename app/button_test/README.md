# button_test

This directory is linked to
`packages/demos/contest2026_094_button_test` by the team manifest.

The command validates the standard `/dev/buttons` node using the onboard
active-low WAKEUP key on PD.15. It reports debounced press/release transitions
for a bounded duration:

```text
nsh> button_test 15
button_test: /dev/buttons supported=0x00000001 initial=released duration=15 seconds
WAKEUP PRESS
WAKEUP RELEASE
button_test: presses=1 releases=1 final=released
```

PD.15 conflicts with I2S_MCLK and must be released before I2S is enabled.
RESET is a hardware reset input, UBOOT shares PA.0 with UART0 TX, and the four
direction keys require the separate GPAI2 analog-key driver.
