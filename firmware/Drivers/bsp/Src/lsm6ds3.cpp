/** @file lsm6ds3.cpp
 * @author team-04
 * @brief C++ ImuDrv facade over the pure C LSM6DS3 protocol driver.
 */
#include "lsm6ds3.h"
namespace team04 {
Lsm6ds3::Lsm6ds3(SpiDrv& bus)
    : bus_(bus)
    , driver_{}
{
    const lsm6ds3_c_bus_t cBus = {this, &Lsm6ds3::busSelect, &Lsm6ds3::busDeselect, &Lsm6ds3::busTransfer};
    lsm6ds3_c_init(&driver_, &cBus);
}
IoStatus Lsm6ds3::fromCStatus(lsm6ds3_c_status_t status) {
    switch (status) {
    case LSM6DS3_C_OK: return IoStatus::kOk;
    case LSM6DS3_C_INVALID: return IoStatus::kInvalid;
    case LSM6DS3_C_NOT_READY: return IoStatus::kNotReady;
    case LSM6DS3_C_UNSUPPORTED: return IoStatus::kUnsupported;
    case LSM6DS3_C_BUSY: return IoStatus::kBusy;
    case LSM6DS3_C_TIMEOUT: return IoStatus::kTimeout;
    case LSM6DS3_C_IO_ERROR: return IoStatus::kError;
    default: return IoStatus::kError;
    }
}
void Lsm6ds3::busSelect(void* context) {
    static_cast<Lsm6ds3*>(context)->bus_.select();
}
void Lsm6ds3::busDeselect(void* context) {
    static_cast<Lsm6ds3*>(context)->bus_.deselect();
}
lsm6ds3_c_status_t Lsm6ds3::busTransfer(void* context, const uint8_t* tx, uint8_t* rx,
                                        uint16_t len, uint32_t timeoutMs) {
    const IoStatus status = static_cast<Lsm6ds3*>(context)->bus_.transfer(tx, rx, len, timeoutMs);
    switch (status) {
    case IoStatus::kOk: return LSM6DS3_C_OK;
    case IoStatus::kInvalid: return LSM6DS3_C_INVALID;
    case IoStatus::kNotReady: return LSM6DS3_C_NOT_READY;
    case IoStatus::kUnsupported: return LSM6DS3_C_UNSUPPORTED;
    case IoStatus::kBusy: return LSM6DS3_C_BUSY;
    case IoStatus::kTimeout: return LSM6DS3_C_TIMEOUT;
    default: return LSM6DS3_C_IO_ERROR;
    }
}
IoStatus Lsm6ds3::resetBegin() { return fromCStatus(lsm6ds3_c_reset_begin(&driver_)); }
IoStatus Lsm6ds3::resetPoll(bool& complete) {
    return fromCStatus(lsm6ds3_c_reset_poll(&driver_, &complete));
}
IoStatus Lsm6ds3::configure() { return fromCStatus(lsm6ds3_c_configure(&driver_)); }
IoStatus Lsm6ds3::readRaw(int16_t& x, int16_t& y, int16_t& z) {
    return fromCStatus(lsm6ds3_c_read_raw(&driver_, &x, &y, &z));
}
IoStatus Lsm6ds3::configureFifo(uint16_t watermarkSamples) {
    return fromCStatus(lsm6ds3_c_configure_fifo(&driver_, watermarkSamples));
}
IoStatus Lsm6ds3::enableFifoInterrupt(bool enabled) {
    return fromCStatus(lsm6ds3_c_enable_fifo_interrupt(&driver_, enabled));
}
IoStatus Lsm6ds3::fifoStatus(FifoStatus& status) {
    lsm6ds3_c_fifo_status_t cStatus{};
    const IoStatus result = fromCStatus(lsm6ds3_c_fifo_status(&driver_, &cStatus));
    if (result == IoStatus::kOk) {
        status.unreadWords = cStatus.unread_words;
        status.nextPattern = cStatus.next_pattern;
        status.empty = cStatus.empty;
        status.full = cStatus.full;
        status.overrun = cStatus.overrun;
        status.watermark = cStatus.watermark;
    }
    return result;
}
bool Lsm6ds3::prepareFifoDma(uint8_t* tx, uint16_t samples, uint16_t capacity) {
    return lsm6ds3_c_prepare_fifo_dma(tx, samples, capacity);
}
bool Lsm6ds3::decodeFifoSample(const uint8_t* rx, uint16_t samples, uint16_t index,
                               int16_t& x, int16_t& y, int16_t& z) {
    return lsm6ds3_c_decode_fifo_sample(rx, samples, index, &x, &y, &z);
}
}
/***************************** END OF FILE ************************************/
