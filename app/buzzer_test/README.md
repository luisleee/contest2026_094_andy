# buzzer_test

This directory is linked to
`packages/demos/contest2026_094_buzzer_test` by the team manifest.

The command drives the demo88-nor onboard buzzer through PE.11/PWM1_A. It
uses the hardware manual's recommended 4 kHz input at 50 percent duty for one
second by default and always stops the PWM before returning.

```text
nsh> buzzer_test
Buzzer on: 4000 Hz, 50% duty, 1000 ms
Buzzer off

nsh> buzzer_test 3000 250
```
