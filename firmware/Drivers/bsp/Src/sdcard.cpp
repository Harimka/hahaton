/** @file sdcard.cpp
 * @author team-04
 * @brief Thin C++ facade for the pure C SD SPI block driver.
 */
#include "sdcard.h"
namespace team04 {
namespace {
sdcard_c_status_t toCStatus(IoStatus status) {
    switch (status) {
    case IoStatus::kOk: return SDCARD_C_OK;
    case IoStatus::kBusy: return SDCARD_C_BUSY;
    case IoStatus::kTimeout: return SDCARD_C_TIMEOUT;
    case IoStatus::kError: return SDCARD_C_ERROR;
    case IoStatus::kNotReady: return SDCARD_C_NOT_READY;
    case IoStatus::kInvalid: return SDCARD_C_INVALID;
    case IoStatus::kUnsupported: return SDCARD_C_UNSUPPORTED;
    }
    return SDCARD_C_ERROR;
}
IoStatus fromCStatus(sdcard_c_status_t status) {
    switch (status) {
    case SDCARD_C_OK: return IoStatus::kOk;
    case SDCARD_C_BUSY: return IoStatus::kBusy;
    case SDCARD_C_TIMEOUT: return IoStatus::kTimeout;
    case SDCARD_C_ERROR: return IoStatus::kError;
    case SDCARD_C_NOT_READY: return IoStatus::kNotReady;
    case SDCARD_C_INVALID: return IoStatus::kInvalid;
    case SDCARD_C_UNSUPPORTED: return IoStatus::kUnsupported;
    }
    return IoStatus::kError;
}
}
SdCard::SdCard(SpiDrv& bus, ClockDrv& clock)
    : bus_(bus)
    , clock_(clock)
    , driver_{} {
    const sdcard_c_ops_t ops = {this, &SdCard::setClock, &SdCard::select,
                                &SdCard::deselect, &SdCard::transfer, &SdCard::millis};
    sdcard_c_init(&driver_, &ops);
}
sdcard_c_status_t SdCard::setClock(void* context, uint32_t hz) {
    return toCStatus(static_cast<SdCard*>(context)->bus_.setClock(hz));
}
void SdCard::select(void* context) {
    static_cast<SdCard*>(context)->bus_.select();
}
void SdCard::deselect(void* context) {
    static_cast<SdCard*>(context)->bus_.deselect();
}
sdcard_c_status_t SdCard::transfer(void* context, const uint8_t* tx, uint8_t* rx,
                                   uint16_t length, uint32_t timeoutMs) {
    return toCStatus(static_cast<SdCard*>(context)->bus_.transfer(tx, rx, length, timeoutMs));
}
uint32_t SdCard::millis(void* context) {
    return static_cast<SdCard*>(context)->clock_.millis();
}
IoStatus SdCard::initialize() { return fromCStatus(sdcard_c_initialize(&driver_)); }
IoStatus SdCard::read(uint32_t lba, uint8_t* buffer) {
    return fromCStatus(sdcard_c_read(&driver_, lba, buffer));
}
IoStatus SdCard::write(uint32_t lba, const uint8_t* buffer) {
    return fromCStatus(sdcard_c_write(&driver_, lba, buffer));
}
IoStatus SdCard::sectorCount(uint64_t& sectors) {
    return fromCStatus(sdcard_c_sector_count(&driver_, &sectors));
}
}
/***************************** END OF FILE ************************************/
