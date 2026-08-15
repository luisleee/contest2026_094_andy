# wifi_test

`wifi_test probe` powers the onboard SDIO Wi-Fi module and sends SDIO CMD5 on
SDMC0. It is a bus-level checkpoint only; it does not load Wi-Fi firmware or
register `wlan0`.

Useful checkpoints:

- `wifi_test cccr` dumps CCCR and FBR registers.
- `wifi_test cis` dumps the common and function CIS tuples.
- `wifi_test enable` sets function 1 block size to 512 bytes, enables
  function 1, and applies the minimum AIC8800D80 SDIO register writes used by
  the Luban driver.
- `wifi_test cmd53` runs the same initialization and then verifies CMD53
  block-mode FIFO write on function 1.
- `wifi_test msg` sends a minimal AIC8800D80 `DBG_MEM_READ_REQ` message and
  dumps the confirmation packet using the D80 E2A message layout.
- `wifi_test memtest` writes 16 bytes to `0x00120000` with
  `DBG_MEM_BLOCK_WRITE_REQ`, then reads the first word back with
  `DBG_MEM_READ_REQ`. This is the firmware-upload preflight check.
- `wifi_test fwload` uploads `fmacfw_8800d80_u02` to `0x00120000` in
  480-byte message chunks, verifies the first word, and sends
  `DBG_START_APP_REQ`.
- `wifi_test fwstate` reads `0x40500004` and prints the firmware-up bit used by
  the Luban driver.
- `wifi_test version` sends `MM_VERSION_REQ` after firmware start and prints
  the `MM_VERSION_CFM` fields.
- `wifi_test initcmd` sends `MM_RESET_REQ` after firmware start, then repeats
  `MM_VERSION_REQ`.
- `wifi_test stackstart` sends `MM_SET_STACK_START_REQ` using the D80 normal
  mode parameters from the Luban driver.
- `wifi_test rfcalib` sends D80 `MM_SET_RF_CALIB_REQ` with the Luban default
  calibration parameters and prints the returned gain-table addresses.
- `wifi_test rfchain` sends stack start, waits briefly, then sends RF
  calibration. This isolates post-stack timing from the full bringup chain.
- `wifi_test mebasic` sends minimal `ME_CONFIG_REQ` and `ME_CHAN_CONFIG_REQ`
  messages.
- `wifi_test macstart` sends a minimal `MM_START_REQ` message.
- `wifi_test bringup` runs the current post-firmware checkpoint chain:
  reset, version, stack start, RF calibration, ME config, channel config, and
  MAC start.
- `wifi_test addif` sends `MM_ADD_IF_REQ` to create one STA VIF using the
  test MAC address.
- `wifi_test scan [1|6|11]` sends a one-channel wildcard `SCANU_START_REQ`
  using existing VIF 0. The optional argument selects channel 1, 6, or 11; a
  raw frequency in MHz is also accepted.
- `wifi_test scanpoll [1|6|11]` creates one STA VIF, starts a one-channel
  wildcard scan, then polls the RX FIFO for scan confirmations and
  `SCANU_RESULT_IND` events.
- `wifi_test netreg` registers the current minimal AIC fullmac network-device
  skeleton as `wlan0`. At this checkpoint RX/TX are still stubbed, so this is
  for verifying NuttX network registration with `ifconfig wlan0`.
- `wifi_test rxpoll [count]` polls the D80 SDIO RX FIFO and prints the raw
  host packet type/header. Use it after bringup or later scan/connect
  checkpoints to see firmware async events and data packets.
