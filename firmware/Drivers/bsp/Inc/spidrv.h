/**
 * @file    spidrv.h
 * @author  team-04
 * @brief   Abstract hardware-independent SPI bus interface and status types.
 */

#ifndef TEAM04_SPIDRV_H_
#define TEAM04_SPIDRV_H_

#include <stdint.h>

namespace team04 {

/**
 * @brief Return status for hardware I/O operations.
 */
enum class IoStatus : uint8_t {
    kOk = 0U,
    kBusy,
    kTimeout,
    kError,
    kNotReady,
    kInvalid,
    kUnsupported
};

/**
 * @brief Abstract SPI bus master driver interface.
 */
class SpiDrv {
public:
    SpiDrv() = default;
    virtual ~SpiDrv() = default;

    SpiDrv(const SpiDrv&) = delete;
    SpiDrv& operator=(const SpiDrv&) = delete;

    /**
     * @brief Configure maximum SPI clock frequency in Hz.
     */
    virtual IoStatus setClock(uint32_t maxHz) = 0;

    /**
     * @brief Assert Chip Select (Active LOW).
     */
    virtual void select() = 0;

    /**
     * @brief Deassert Chip Select (Inactive HIGH).
     */
    virtual void deselect() = 0;

    /**
     * @brief Full-duplex synchronous SPI transfer.
     * @param tx Pointer to transmit buffer (nullptr sends 0xFF).
     * @param rx Pointer to receive buffer (nullptr discards received data).
     * @param len Transfer length in bytes.
     * @param timeoutMs Maximum timeout in milliseconds.
     */
    virtual IoStatus transfer(const uint8_t* tx, uint8_t* rx, uint16_t len, uint32_t timeoutMs) = 0;
};

} // namespace team04

#endif // TEAM04_SPIDRV_H_

/***************************** END OF FILE ************************************/
