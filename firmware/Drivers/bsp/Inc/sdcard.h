/** @file sdcard.h
 * @author team-04
 * @brief SD memory card SPI block driver.
 */
#ifndef TEAM04_SDCARD_H_
#define TEAM04_SDCARD_H_
#include "storagedrv.h"
#include "sdcard_c.h"
namespace team04 {
class SdCard final : public StorageDrv {
public:
    SdCard(SpiDrv& bus, ClockDrv& clock);
    SdCard(const SdCard&) = delete;
    SdCard& operator=(const SdCard&) = delete;
    IoStatus initialize() override;
    IoStatus read(uint32_t lba, uint8_t* buffer) override;
    IoStatus write(uint32_t lba, const uint8_t* buffer) override;
    IoStatus sectorCount(uint64_t& sectors) override;
    bool isReady() const { return driver_.ready; }
    uint8_t lastResponse() const { return driver_.last_response; }
    uint8_t lastDataResponse() const { return driver_.last_data_response; }
    uint32_t maxWriteMs() const { return driver_.max_write_ms; }
    uint32_t transferErrorCount() const { return driver_.transfer_error_count; }
private:
    static sdcard_c_status_t setClock(void* context, uint32_t hz);
    static void select(void* context);
    static void deselect(void* context);
    static sdcard_c_status_t transfer(void* context, const uint8_t* tx, uint8_t* rx,
                                      uint16_t length, uint32_t timeoutMs);
    static uint32_t millis(void* context);
    SpiDrv& bus_;
    ClockDrv& clock_;
    sdcard_c_t driver_;
};
}
#endif
/***************************** END OF FILE ************************************/
