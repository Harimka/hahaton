/** @file lsm6ds3.h
 * @author team-04
 * @brief LSM6DS3 accelerometer and FIFO driver.
 */
#ifndef TEAM04_LSM6DS3_H_
#define TEAM04_LSM6DS3_H_
#include "imudrv.h"
#include "lsm6ds3_c.h"
namespace team04 {
struct FifoStatus {
    uint16_t unreadWords;
    uint16_t nextPattern;
    bool empty;
    bool full;
    bool overrun;
    bool watermark;
};
class Lsm6ds3 final : public ImuDrv {
public:
    static constexpr float kSensitivityG = 0.000488f;
    static constexpr uint8_t kWhoAmI = 0x69U;
    static constexpr uint8_t kFifoStatus1 = 0x3aU;
    static constexpr uint8_t kFifoDataOutL = 0x3eU;
    explicit Lsm6ds3(SpiDrv& bus);
    Lsm6ds3(const Lsm6ds3&) = delete;
    Lsm6ds3& operator=(const Lsm6ds3&) = delete;
    IoStatus resetBegin();
    IoStatus resetPoll(bool& complete);
    IoStatus configure() override;
    IoStatus readRaw(int16_t& x, int16_t& y, int16_t& z) override;
    IoStatus configureFifo(uint16_t watermarkSamples);
    IoStatus enableFifoInterrupt(bool enabled);
    IoStatus fifoStatus(FifoStatus& status);
    uint8_t observedId() const { return driver_.observed_id; }
    static bool prepareFifoDma(uint8_t* tx, uint16_t samples, uint16_t capacity);
    static bool decodeFifoSample(const uint8_t* rx, uint16_t samples, uint16_t index,
                                 int16_t& x, int16_t& y, int16_t& z);
private:
    static IoStatus fromCStatus(lsm6ds3_c_status_t status);
    static void busSelect(void* context);
    static void busDeselect(void* context);
    static lsm6ds3_c_status_t busTransfer(void* context, const uint8_t* tx, uint8_t* rx,
                                         uint16_t len, uint32_t timeoutMs);
    SpiDrv& bus_;
    lsm6ds3_c_t driver_;
};
}
#endif
/***************************** END OF FILE ************************************/
