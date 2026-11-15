# SPI Clock-Only Transfer Fix

Firmware commit: `3ac63161ea4b76409fee3ee2ec9e2e03f5c21a40`

## Change

Updated `firmware/Drivers/port/Src/stm32g4spi.cpp` so `Stm32G4Spi::transfer()` supports clock-only transfers where both TX and RX pointers are null. SPI still transmits dummy `0xFF` bytes, while received bytes are placed in a small local discard buffer instead of passing a null pointer to `HAL_SPI_TransmitReceive`.

## Rationale

The SD initialization code requests clock pulses with `transfer(nullptr, nullptr, length, timeout)`. The STM32 SPI adapter must still provide a valid RX buffer to the HAL's full-duplex transfer function, even when the caller does not need received bytes. This change prevents the HAL from receiving a null destination pointer and preserves the intended clock generation.

## Verification and Limitations

- The firmware build succeeded after this change: `text=41384`, `data=468`, `bss=35088`.
- The card has not been verified with this change in isolation; subsequent device log still reported `FR_NOT_READY`.
- This fix addresses one software defect but does not establish SD wiring, card compatibility, SPI signal integrity, or successful card initialization.
- The local discard buffer is used synchronously by the blocking HAL transfer and is not retained after return.
