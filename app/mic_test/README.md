# mic_test

This command validates the demo88-nor onboard PDM microphones through
`/dev/audio/pcm0c`. The first capture milestone records mono signed 16-bit
little-endian PCM at 16 kHz and stores a standard WAV file. The default path
is `/data/mic.wav`, and the default duration is three seconds.

```text
nsh> mic_test record
nsh> mic_test play
nsh> mic_test loop /data/mic.wav 3
```

`loop` records, finalizes the WAV header, prints the resulting file size, and
plays it through `/dev/audio/pcm0p`. The app accepts both the stock nxrecorder
raw-PCM output and trees where nxrecorder already emits WAV, so no private
nxrecorder API is required. Durations are limited to one through five seconds
so the recording remains bounded for the 1 MiB LittleFS partition.

The capture lower-half uses two 8192-byte DMA periods. Its two explicit D13x
v1.x descriptors are 32-byte aligned, linked as a ring, and cache-cleaned
before DMA channel 1 starts. Startup logs print both descriptor addresses,
destinations, and links. An address-request failure additionally prints the
former descriptor and completed package count.

The custom D13x arch does not provide NuttX cache hooks. The vendor range
helper also emits its fixed-register T-Head cache instruction without tying
the loop address to that register. This lower-half therefore performs its own
32-byte-line clean/invalidate operations with an explicit `a5` constraint.
Continuous three-second capture and WAV finalization are board-verified.
