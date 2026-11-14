/** @file lsm6ds3.h
 * @author team-04
 * @brief LSM6DS3 accelerometer and FIFO driver.
 */
#ifndef TEAM04_LSM6DS3_H_
#define TEAM04_LSM6DS3_H_
#include "imudrv.h"
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
    uint8_t observedId() const { return observedId_; }
    static bool prepareFifoDma(uint8_t* tx, uint16_t samples, uint16_t capacity);
    static bool decodeFifoSample(const uint8_t* rx, uint16_t samples, uint16_t index,
                                 int16_t& x, int16_t& y, int16_t& z);
private:
    static constexpr uint8_t kFifoCtrl1 = 0x06U, kFifoCtrl2 = 0x07U, kFifoCtrl3 = 0x08U;
    static constexpr uint8_t kFifoCtrl4 = 0x09U, kFifoCtrl5 = 0x0aU, kInt1Ctrl = 0x0dU;
    static constexpr uint8_t kWhoAmIReg = 0x0fU, kCtrl1 = 0x10U, kCtrl2 = 0x11U;
    static constexpr uint8_t kCtrl3 = 0x12U, kCtrl4 = 0x13U, kCtrl6 = 0x15U;
    static constexpr uint8_t kCtrl8 = 0x17U, kCtrl9 = 0x18U, kStatus = 0x1eU, kOutX = 0x28U;
    IoStatus readRegister(uint8_t reg, uint8_t* data, uint16_t len);
    IoStatus writeChecked(uint8_t reg, uint8_t value, uint8_t mask);
    static int16_t decodeWord(const uint8_t* p);
    SpiDrv& bus_;
    uint8_t observedId_;
    bool configured_;
};
}
#endif
/***************************** END OF FILE ************************************/
