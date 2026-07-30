# Direction-key test

`dpad_test [seconds]` validates the demo88-nor PA.2/GPAI2 resistor-ladder
direction keys through the standard NuttX button device at `/dev/dpad`.
WAKEUP remains a separate key at `/dev/buttons`.

Run `dpad_test 20` and press UP, DOWN, LEFT, and RIGHT. The command reports
press/release transitions, calibrated ADC values, and a per-direction count.
