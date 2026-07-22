# D13x demo88-nor NSH

This board port targets the ArtInChip D133CBS demo88-nor board and boots
openvela/NuttX from SPI NOR into the on-chip SRAM. The console is UART0 at
115200 baud.

## Build

Run from the openvela workspace root:

```bash
rm -rf cmake_out/demo88-nor_nsh
./build.sh contest2026_094_andy/board/d13x/demo88-nor/configs/nsh \
  --cmake -j8
```

The ELF entry point must be `0x30044100`, matching `pack/d13x_os.its`:

```bash
prebuilts/gcc/linux-x86_64/riscv-none-elf/bin/riscv-none-elf-readelf \
  -h cmake_out/demo88-nor_nsh/nuttx | grep 'Entry point'
```

## Pack

Copy the CMake outputs to the ArtInChip pack inputs, then run the packer:

```bash
cp -f cmake_out/demo88-nor_nsh/nuttx nuttx/nuttx.elf
cp -f cmake_out/demo88-nor_nsh/nuttx.manifest nuttx/nuttx.manifest

cd vendor/artinchip/pack
./pack.sh demo88-nor d13x
```

The burnable image is:

```text
vendor/artinchip/pack/prebuilt/d13x_demo88-nor_v1.0.0.img
```

An `img2simg` error about `libselinux.so.1` only affects optional sparse image
conversion. The raw `.img` above is still generated and is the AiBurn input.

## Burn And Verify

1. Hold the board BOOT key while connecting USB.
2. Select the generated `.img` in AiBurn and burn the complete image.
3. Connect UART0 using 115200 baud, 8 data bits, no parity, 1 stop bit, and no
   flow control.
4. Power-cycle the board after the burn completes.

A successful boot reaches:

```text
NuttShell (NSH)
nsh>
```

At the prompt, run `help` to verify UART receive interrupts and task context
switching, not only console output.
