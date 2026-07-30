# Power-management wake test

`pm_test wake [seconds]` is the first D13x suspend/wake milestone. It waits
for PD.15 WAKEUP to be released, arms falling-edge-only wake, powers off the
display pipeline through `/dev/fb0`, and blocks until WAKEUP or timeout.

The command restores the display and normal both-edge `/dev/buttons` behavior
on every exit path. CPU and PLL clocks remain running at this stage.
