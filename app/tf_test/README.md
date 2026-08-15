# tf_test

This directory is linked to `packages/demos/contest2026_094_tf_test` by the
team manifest.

The command validates the demo88-nor J5 TF-card path through `/dev/mmcsd0`
and a FATFS mount at `/sdcard`. It never formats media and only creates or
removes `/sdcard/tf_test.bin`.

Insert a FAT/FAT32/exFAT-formatted card before boot, then run:

```text
nsh> tf_test info
nsh> ls /sdcard
nsh> tf_test write
nsh> tf_test check
nsh> reboot
nsh> tf_test check
nsh> tf_test clear
nsh> tf_test umount
```

The board mounts the card at `/sdcard` during boot. Use `tf_test mount` only
to retry after a boot-time mount failure.

Card removal and reinsertion are deferred until card-detect hotplug support is
added. Unmount before removing the card.

Before mounting, `info` confirms that `/dev/mmcsd0` is a block device. After
mounting, it also reports FATFS capacity and available space. NuttX block
device nodes do not support application-level `open()`/`BIOC_GEOMETRY`; the
filesystem and MMC/SD core use the internal block-driver interface instead.
