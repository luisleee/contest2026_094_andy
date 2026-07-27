# lvgl_test

This directory is linked to `packages/demos/contest2026_094_lvgl_test` by the
team manifest.

The command opens `/dev/fb0` and `/dev/input0` through LVGL's NuttX backends
and renders a bounded interactive 1024x600 RGB565 test page. It provides a
button, slider, switch, and draggable block while retaining the animated
progress and cache-coherency checks. Solid objects explicitly set
`LV_OPA_COVER`; the minimal LVGL configuration does not supply an opaque
background style by default.

```text
nsh> lvgl_test
lvgl_test: LVGL 9.1.0, 1024x600 RGB565, /dev/input0, duration=30 seconds
lvgl_test: touch events button=... slider=... switch=... drag=...
lvgl_test: completed; final frame remains on the panel
```

An optional duration from 5 to 300 seconds may be supplied:

```text
nsh> lvgl_test 60
```

Exercise every control and require all four counters to be nonzero. The
GT911 input node permits one reader, so close `getevent` before starting this
command.
