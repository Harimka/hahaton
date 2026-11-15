# SD Initialization Stage Diagnostics

Firmware commit: `ac3075bf0b6edd1da1df05dfe9ea988c70758740`

## Change

Added an initialization-stage field to the existing SD SPI driver context and expose it through `SdCard::initStage()`. The driver marks progress through startup clock generation, CMD0, CMD8, ACMD41, CMD58, CMD16 (when needed), switching to the operating clock, and ready state.

When the startup smoke test fails, `main.cpp` now prints the stage, readiness, block-addressing mode, last R1 response, last data response, SPI transfer error count, and FatFs disk adapter error count.

## Rationale

The prior UART message only reported FatFs `FR_NOT_READY`, which does not reveal whether the card failed to answer SPI initialization commands or failed later. Stage and response diagnostics make the next hardware log actionable without changing the SD protocol or guessing at a wiring fault.

## Verification

- `cmake --build firmware/build --parallel` succeeded.
- Link output: `text=41680`, `data=468`, `bss=35088`.
- The build still reports the existing string-literal-to-`char*` warning at `FATFS_LinkDriver(..., "0:")`.
- `git diff --check` passed before commit.

## Hardware Status and Limitations

This diagnostic build has not yet been flashed/tested in the reported run. It does not prove that SD wiring is wrong or right, and it does not change card initialization behavior. The next UART log should be used to identify the failing stage and response. A lit module power LED confirms only some power indication; it does not prove 3.3 V at the card under load or successful SPI communication.
