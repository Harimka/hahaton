/** @file spidrv.h
 * @author team-04
 * @brief Hardware-independent SPI, DMA and time interfaces.
 */
#ifndef TEAM04_SPIDRV_H_
#define TEAM04_SPIDRV_H_
#include <stdint.h>
namespace team04 {
enum class IoStatus : uint8_t { kOk, kBusy, kTimeout, kError, kNotReady, kInvalid, kUnsupported };
class SpiDrv {
public:
    SpiDrv() = default;
    SpiDrv(const SpiDrv&) = delete;
    SpiDrv& operator=(const SpiDrv&) = delete;
    virtual IoStatus setClock(uint32_t maxHz) = 0;
    virtual void select() = 0;
    virtual void deselect() = 0;
    /* nullptr TX sends 0xff; nullptr RX discards received data. */
    virtual IoStatus transfer(const uint8_t* tx, uint8_t* rx, uint16_t len, uint32_t timeoutMs) = 0;
    /* DMA owns the buffers and selected CS until completion or abort callback. */
    virtual IoStatus startDma(const uint8_t* tx, uint8_t* rx, uint16_t len) = 0;
    virtual IoStatus abortDma() = 0;
protected:
    ~SpiDrv() = default;
};
class ClockDrv {
public:
    ClockDrv() = default;
    ClockDrv(const ClockDrv&) = delete;
    ClockDrv& operator=(const ClockDrv&) = delete;
    virtual uint32_t millis() const = 0;
    virtual uint32_t micros() const = 0;
protected:
    ~ClockDrv() = default;
};
}
#endif
/***************************** END OF FILE ************************************/
