# speaker_test

This command validates the demo88-nor onboard speaker path through DSPK1 on
PE.12 and the active-low LM4871 shutdown control on PD.10. The standard NuttX
PCM device is `/dev/audio/pcm0p`.

The initial board-validation image uses bounded CPU-polling playback and does
not enable audio DMA. Run the short generated tone first, then inspect and
play the WAV file already stored in the LittleFS data partition:

```text
nsh> ls /dev/audio
nsh> speaker_test tone 1000 1
nsh> speaker_test info /data/s16le1c.wav
nsh> speaker_test play /data/s16le1c.wav
```

The WAV path defaults to `/data/s16le1c.wav`. Only PCM signed 16-bit
little-endian mono or stereo files at a D13x-supported sample rate are
accepted. The test never modifies the WAV file.

`speaker_test tone` uses `nxplayer_playtone()`, which marks its headerless PCM
as raw while normal file playback continues through RIFF/WAV parsing. Tone
generation uses a fixed-point triangle wave rather than per-sample `sinf()`;
this prevents buffer underruns on the soft-float E907.
