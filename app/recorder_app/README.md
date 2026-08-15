# Recorder

Insert the card before boot, then run `recorder` from NSH. Board bring-up
mounts it at `/sdcard`; the app retries the mount if necessary and opens a
touch-first recording library. Select a WAV file to view its waveform and
play it, or start a new 16 kHz mono 16-bit recording. The recording screen
shows a live waveform and elapsed recording time, with pause/resume and finish
controls.
