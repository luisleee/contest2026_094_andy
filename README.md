# D13x openvela Bring-up

Contest team `094/andy` port of openvela/NuttX to the ArtInChip D133CBS
demo88-nor board. The current target boots from 16 MiB SPI NOR, runs from the
on-chip SRAM, and exposes an interactive NSH console on UART0 at 115200 baud.

## Repository Layout

- `chip/d13x/`: E907 startup, CLIC, timer, UART0, and SoC definitions.
- `board/d13x/demo88-nor/`: board configuration, linker script, and pack data.
- `app/hello_app/`: terminal command used to verify builtin application support.
- `logs/`: exported AI coding logs and submission metadata.

The manifest maps these directories into the openvela workspace without
copying contest-owned source into vendor repositories.

## Build And Pack

Run from the openvela workspace root:

```bash
rm -rf cmake_out/demo88-nor_nsh
./build.sh contest2026_094_andy/board/d13x/demo88-nor/configs/nsh \
  --cmake -j8

cp -f cmake_out/demo88-nor_nsh/nuttx nuttx/nuttx.elf
cp -f cmake_out/demo88-nor_nsh/nuttx.manifest nuttx/nuttx.manifest

cd vendor/artinchip/pack
./pack.sh demo88-nor d13x
```

Burn this image with AiBurn:

```text
vendor/artinchip/pack/prebuilt/d13x_demo88-nor_v1.0.0.img
```

Use UART0 with `115200 8N1` and no flow control. A successful boot reaches:

```text
NuttShell (NSH)
nsh>
```

## Hello Command

The NSH configuration enables `hello_app` by default:

```text
nsh> hello_app
Hello from openvela contest 2026 team 094 (andy)!
```

The board-specific build and burn details are also documented in
`board/d13x/demo88-nor/README.md`.
