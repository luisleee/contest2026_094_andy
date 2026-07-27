# fb_test

This directory is linked to `packages/demos/contest2026_094_fb_test` by the
team manifest.

The command validates the 1024x600 RGB565 framebuffer and draws eight vertical
color bars on the demo88-nor J18 LVDS panel.

```text
nsh> ls /dev
/dev/fb0

nsh> fb_test
Framebuffer color bars: RGB565 1024x600, addr=0x40000000, stride=2048
```

The framebuffer is cleared to black at boot. PE.13 enables the panel/backlight
after DE/LVDS timing has been running for 20 ms; the color bars appear only
after this command is executed.
