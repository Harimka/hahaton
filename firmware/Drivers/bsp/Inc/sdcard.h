/** @file sdcard.h
 * @author team-04
 * @brief SD memory card SPI block driver.
 */
#ifndef TEAM04_SDCARD_H_
#define TEAM04_SDCARD_H_
#include "storagedrv.h"
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
    bool isReady() const { return ready_; }
    uint8_t lastResponse() const { return lastResponse_; }
    uint8_t lastDataResponse() const { return lastDataResponse_; }
    uint32_t maxWriteMs() const { return maxWriteMs_; }
    uint32_t transferErrorCount() const { return transferErrorCount_; }
private:
    IoStatus transfer(const uint8_t* tx, uint8_t* rx, uint16_t n);
    void deselect();
    IoStatus command(uint8_t index, uint32_t arg, uint8_t crc, uint8_t& response);
    IoStatus singleCommand(uint8_t index, uint32_t arg, uint8_t crc, uint8_t& response);
    IoStatus waitFor(uint8_t expected, uint32_t timeoutMs, bool dataToken);
    IoStatus address(uint32_t lba, uint32_t& arg) const;
    static uint16_t crc16(const uint8_t* data, uint16_t len);
    SpiDrv& bus_;
    ClockDrv& clock_;
    bool ready_;
    bool blockAddressed_;
    uint8_t lastResponse_;
    uint8_t lastDataResponse_;
    uint32_t maxWriteMs_;
    uint32_t transferErrorCount_;
};
}
#endif
/***************************** END OF FILE ************************************/
