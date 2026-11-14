# C Driver Core

## Architecture

The canonical migrated source tree is `team04-integration/firmware/Drivers`.
The separate top-level `firmware/` tree is a legacy alternative and must not be
added to the same build. Folder names and public C++ interfaces are retained.
No source code from ztlib or ztgd32 is used.

```text
Application / existing C++ interfaces
  -> Lsm6ds3 / SdCard / FifoCapture C++ facades
  -> lsm6ds3_c.c / sdcard_c.c / fifocapture_c.c
  -> injected callback operations
  -> existing C++ STM32 HAL port
```

The chip protocol algorithms, FIFO state machine, buffers and counters are C11.
The C++ facades translate typed status values and dispatch bus/time/critical
operations. They do not implement the chip protocols. C headers have C linkage
guards and do not include HAL, CMSIS or FatFS.

## Source list

Compile with a **C compiler**, not as C++:

- `Drivers/bsp/Src/lsm6ds3_c.c`
- `Drivers/bsp/Src/fifocapture_c.c`
- `Drivers/bsp/Src/sdcard_c.c`

Compile the matching `.cpp` facades and abstract interfaces as C++17. Existing
`Drivers/port` code remains C++: it is platform/FatFS/board glue, not migrated
chip protocol code. Its HAL dependencies still require the real Cube project.
This change does not provide a complete STM32 ELF or claim hardware validation.

## Ownership and callbacks

Contexts and their callback dependencies must remain alive for all operations.
Use static storage for embedded contexts. Never copy a live context/facade.
C initialization is startup-only and must not race interrupts or active DMA.

SPI transfer callbacks must honor bounded timeouts. Null TX clocks `0xff`; null
RX discards data. Sensor select remains asserted across command and payload.

FIFO operations require a valid matched critical-enter/leave pair. DMA callbacks
must never run inline from start/abort. The start callback owns CS selection.
The port must make DMA physically quiescent and CS high before a successful
completion or abort-completion notification. A failed start may have partially
configured hardware, so it still follows abort/quiescence recovery. Failed
abort leaves capture blocked rather than reusing live buffers.

One consumer may peek/release blocks. C++ peek translates into a static member
cache without type-punning. A returned pointer must not be retained after
release or another peek. Timestamps are nominal-ODR estimates, not sensor
hardware timestamps. The 15-byte raw sample contract is preserved. Known queue
drops advance sequence indices and set an overrun indication on the next
published block. Unknown sensor loss cannot be reconstructed from indices.

## Host verification

Run from any working directory:

```sh
sh /absolute/path/to/team04-integration/firmware/run_host_tests.sh
```

The script uses clang/clang++ by default, supports CC/CXX/BUILD_DIR overrides,
and writes build products under `/tmp/team04-c-firmware-build`. Tests keep
assertions enabled. It compiles cores as C11 and facades/tests as C++17 with
`-Wall -Wextra -Werror -pedantic`.

An optional CMake host configuration is included. CMake was not installed in
the implementation environment; use a Debug build so assertion-based tests
remain active. The shell test runner is the verified build path.

Hardware checks still required: real Cube build/link, DMA SRAM placement,
SPI modes/clocks, IRQ latency, abort quiescence, FIFO loss counters and capture
while SD writes are active. Host mocks cannot establish these properties.
