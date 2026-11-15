# SD Smoke Test Artifact

Firmware commit: `6b65d1aa6b910576593454a66a598c354b016126`

## Change

Replaced startup-only `imu.csv` open/header handling with a bounded SD smoke test in `firmware/Core/Src/main.cpp`:

1. Uses the existing FatFs, diskio adapter, SD block driver, and SPI3 path.
2. Creates or overwrites only `0:/SDTEST.TXT`.
3. Writes the fixed payload `team04-sd-smoke-test-v1\r\n`.
4. Checks the written byte count, calls `f_sync`, and closes the file.
5. Reopens the file, reads the expected number of bytes, compares them byte-for-byte, and closes it.
6. Reports `[SD TEST] PASS` or the FatFs result code over UART before starting sensor capture.

## Rationale

The previous firmware initialized/mounted the card and opened a file, but did not exercise the write path. The smoke test covers file creation, sector writes through the existing storage stack, synchronization, close, and readback. Running it once at startup keeps SD latency out of the high-rate acquisition loop. A fixed test filename avoids overwriting the future event log files, although any previous `SDTEST.TXT` is intentionally replaced.

No direct SD-register implementation or DMA was added: the current task is to establish whether the existing driver methods work end-to-end. DMA would not remove SD-card internal write latency and is not required for this startup test.

## Verification

- Firmware build succeeded with `cmake --build firmware/build --parallel`.
- Link output: `text=41344`, `data=468`, `bss=35088`.
- Build emitted an existing warning at `FATFS_LinkDriver(..., "0:")` about converting a string literal to `char*`; it did not prevent the build.
- `git diff --check` passed before commit.

## Hardware Status and Risks

This change has **not** been run on the STM32 board or SD card. The successful build does not establish that card initialization, writes, synchronization, or readback work on hardware. The test can fail because of wiring, card formatting, SPI timing, card compatibility, or defects in the existing SD/FatFs stack. Its UART PASS/FAIL line must be observed on the board.

The test is intentionally destructive only to `SDTEST.TXT`: each boot truncates/replaces that file. It does not append measurement data and does not prove sustained high-rate logging performance.
