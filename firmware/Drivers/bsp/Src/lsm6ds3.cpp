/**
 * @file    lsm6ds3.cpp
 * @author  team-04
 * @brief   LSM6DS3 accelerometer and gyroscope register operations.
 */

#include "lsm6ds3.h"

namespace team04 {

Lsm6ds3::Lsm6ds3(SpiDrv& bus)
    : bus_(bus)
    , whoAmI_(0U)
    , isConfigured_(false)
{
}

IoStatus Lsm6ds3::readRegister(uint8_t reg, uint8_t* data, uint16_t len)
{
    if (!data || len == 0U) {
        return IoStatus::kInvalid;
    }

    const uint8_t cmd = static_cast<uint8_t>(reg | 0x80U); // Bit 7 = 1 for READ

    bus_.select();
    IoStatus status = bus_.transfer(&cmd, nullptr, 1U, 20U);
    if (status == IoStatus::kOk) {
        status = bus_.transfer(nullptr, data, len, 20U);
    }
    bus_.deselect();

    return status;
}

IoStatus Lsm6ds3::writeRegister(uint8_t reg, uint8_t val)
{
    const uint8_t tx[2] = {
        static_cast<uint8_t>(reg & 0x7FU), // Bit 7 = 0 for WRITE
        val
    };

    bus_.select();
    const IoStatus status = bus_.transfer(tx, nullptr, 2U, 20U);
    bus_.deselect();

    return status;
}

IoStatus Lsm6ds3::writeChecked(uint8_t reg, uint8_t val, uint8_t mask)
{
    const IoStatus status = writeRegister(reg, val);
    if (status != IoStatus::kOk) {
        return status;
    }

    uint8_t readBack = 0U;
    const IoStatus readStatus = readRegister(reg, &readBack, 1U);
    if (readStatus != IoStatus::kOk) {
        return readStatus;
    }

    if ((readBack & mask) != (val & mask)) {
        return IoStatus::kError;
    }

    return IoStatus::kOk;
}

int16_t Lsm6ds3::decodeWord(const uint8_t* buf)
{
    const uint16_t u = static_cast<uint16_t>(buf[0]) |
                       (static_cast<uint16_t>(buf[1]) << 8U);
    return static_cast<int16_t>(u);
}

IoStatus Lsm6ds3::probe(uint8_t& id)
{
    isConfigured_ = false;
    whoAmI_ = 0U;

    const IoStatus status = readRegister(kRegWhoAmI, &whoAmI_, 1U);
    if (status != IoStatus::kOk) {
        return status;
    }

    id = whoAmI_;
    if (whoAmI_ != kExpectedWhoAmI) {
        return IoStatus::kUnsupported;
    }

    return IoStatus::kOk;
}

IoStatus Lsm6ds3::configure()
{
    isConfigured_ = false;

    // 1. Verify WHO_AM_I
    uint8_t id = 0U;
    IoStatus status = probe(id);
    if (status != IoStatus::kOk) {
        return status;
    }

    // 2. Set CTRL3_C = 0x44 (BDU: Block Data Update + IF_INC: Auto register address increment)
    status = writeChecked(kRegCtrl3C, 0x44U, 0x44U);
    if (status != IoStatus::kOk) {
        return status;
    }

    // 3. Set CTRL1_XL = 0xA4 (ODR 6.66 kHz, ±16g, BW 400Hz/anti-alias bypass)
    status = writeChecked(kRegCtrl1Xl, 0xA4U, 0xFCU);
    if (status != IoStatus::kOk) {
        return status;
    }

    // 4. Set CTRL2_G = 0xAC (ODR 6.66 kHz, ±2000 dps full-scale)
    status = writeChecked(kRegCtrl2G, 0xACU, 0xFCU);
    if (status != IoStatus::kOk) {
        return status;
    }

    // 5. CTRL4_C = 0x00 (Anti-alias filter bypass)
    status = writeChecked(kRegCtrl4C, 0x00U, 0x80U);
    if (status != IoStatus::kOk) {
        return status;
    }

    // 6. CTRL6_C = 0x00 (High-performance XL enabled)
    status = writeChecked(kRegCtrl6C, 0x00U, 0x10U);
    if (status != IoStatus::kOk) {
        return status;
    }

    // 7. CTRL7_G = 0x00 (High-performance G enabled)
    status = writeChecked(kRegCtrl7G, 0x00U, 0x80U);
    if (status != IoStatus::kOk) {
        return status;
    }

    isConfigured_ = true;
    return IoStatus::kOk;
}

IoStatus Lsm6ds3::readStatus(uint8_t& status)
{
    return readRegister(kRegStatus, &status, 1U);
}

IoStatus Lsm6ds3::readAccelIfReady(AccelRawData& raw)
{
    if (!isConfigured_) {
        return IoStatus::kNotReady;
    }

    uint8_t status = 0U;
    IoStatus result = readStatus(status);
    if (result != IoStatus::kOk) {
        return result;
    }

    if ((status & 0x01U) == 0U) {
        return IoStatus::kNotReady;
    }

    uint8_t data[6] = {};
    result = readRegister(kRegOutX_L_XL, data, sizeof(data));
    if (result != IoStatus::kOk) {
        return result;
    }

    raw.x = decodeWord(&data[0]);
    raw.y = decodeWord(&data[2]);
    raw.z = decodeWord(&data[4]);
    return IoStatus::kOk;
}

IoStatus Lsm6ds3::readRaw(ImuRawData& raw)
{
    if (!isConfigured_) {
        return IoStatus::kNotReady;
    }

    // Read 12 consecutive bytes: 6 bytes Gyro (0x22..0x27) + 6 bytes Accel (0x28..0x2D)
    uint8_t data[12];
    const IoStatus status = readRegister(kRegOutX_L_G, data, sizeof(data));
    if (status != IoStatus::kOk) {
        return status;
    }

    raw.gyroX  = decodeWord(&data[0]);
    raw.gyroY  = decodeWord(&data[2]);
    raw.gyroZ  = decodeWord(&data[4]);

    raw.accelX = decodeWord(&data[6]);
    raw.accelY = decodeWord(&data[8]);
    raw.accelZ = decodeWord(&data[10]);

    return IoStatus::kOk;
}

IoStatus Lsm6ds3::readScaled(ImuScaledData& scaled)
{
    ImuRawData raw{};
    const IoStatus status = readRaw(raw);
    if (status != IoStatus::kOk) {
        return status;
    }

    scaled.accelX_g = static_cast<float>(raw.accelX) * kAccelScaleG;
    scaled.accelY_g = static_cast<float>(raw.accelY) * kAccelScaleG;
    scaled.accelZ_g = static_cast<float>(raw.accelZ) * kAccelScaleG;

    scaled.gyroX_dps = static_cast<float>(raw.gyroX) * kGyroScaleDps;
    scaled.gyroY_dps = static_cast<float>(raw.gyroY) * kGyroScaleDps;
    scaled.gyroZ_dps = static_cast<float>(raw.gyroZ) * kGyroScaleDps;

    return IoStatus::kOk;
}

} // namespace team04

/***************************** END OF FILE ************************************/
