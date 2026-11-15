/**
 * @file    lsm6ds3.h
 * @author  team-04
 * @brief   LSM6DS3 6-axis IMU (accelerometer + gyroscope) SPI driver.
 */

#ifndef TEAM04_LSM6DS3_H_
#define TEAM04_LSM6DS3_H_

#include "imudrv.h"

namespace team04 {

/**
 * @brief LSM6DS3 hardware driver.
 *        No MCU HAL dependencies; uses SpiDrv exclusively.
 */
class Lsm6ds3 final : public ImuDrv {
public:
    static constexpr uint8_t kExpectedWhoAmI = 0x69U;
    static constexpr float kAccelScaleG = 0.000488f;    ///< ±16g: 0.488 mg/LSB
    static constexpr float kGyroScaleDps = 0.070f;       ///< ±2000 dps: 70 mdps/LSB

    explicit Lsm6ds3(SpiDrv& bus);
    ~Lsm6ds3() override = default;

    Lsm6ds3(const Lsm6ds3&) = delete;
    Lsm6ds3& operator=(const Lsm6ds3&) = delete;

    /**
     * @brief Read WHO_AM_I register.
     * @param[out] id Received identifier.
     * @return kOk on success.
     */
    IoStatus probe(uint8_t& id);

    /**
     * @brief Configure accelerometer and gyroscope for 6.66 kHz operation.
     */
    IoStatus configure() override;

    /**
     * @brief Read STATUS_REG.
     * @param[out] status Raw status register (bit 0 = XLDA, bit 1 = GDA).
     */
    IoStatus readStatus(uint8_t& status);

    /**
     * @brief Poll XLDA and read one accelerometer sample without using FIFO.
     * @return kOk when a fresh sample was read, kNotReady when XLDA is clear.
     */
    IoStatus readAccelIfReady(AccelRawData& raw);

    /**
     * @brief Read raw 16-bit 6-axis measurements.
     */
    IoStatus readRaw(ImuRawData& raw) override;

    /**
     * @brief Read scaled physical measurements.
     */
    IoStatus readScaled(ImuScaledData& scaled) override;

    uint8_t whoAmI() const { return whoAmI_; }
    bool isConfigured() const { return isConfigured_; }

private:
    /* LSM6DS3 official register addresses from ST datasheets */
    static constexpr uint8_t kRegWhoAmI     = 0x0FU;
    static constexpr uint8_t kRegCtrl1Xl    = 0x10U;
    static constexpr uint8_t kRegCtrl2G     = 0x11U;
    static constexpr uint8_t kRegCtrl3C     = 0x12U;
    static constexpr uint8_t kRegCtrl4C     = 0x13U;
    static constexpr uint8_t kRegCtrl6C     = 0x15U;
    static constexpr uint8_t kRegCtrl7G     = 0x16U;
    static constexpr uint8_t kRegStatus     = 0x1EU;
    static constexpr uint8_t kRegOutX_L_G   = 0x22U;
    static constexpr uint8_t kRegOutX_L_XL  = 0x28U;

    IoStatus readRegister(uint8_t reg, uint8_t* data, uint16_t len);
    IoStatus writeRegister(uint8_t reg, uint8_t val);
    IoStatus writeChecked(uint8_t reg, uint8_t val, uint8_t mask);
    static int16_t decodeWord(const uint8_t* buf);

    SpiDrv& bus_;
    uint8_t whoAmI_;
    bool isConfigured_;
};

} // namespace team04

#endif // TEAM04_LSM6DS3_H_

/***************************** END OF FILE ************************************/
