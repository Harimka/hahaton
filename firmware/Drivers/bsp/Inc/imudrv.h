/**
 * @file    imudrv.h
 * @author  team-04
 * @brief   Abstract IMU sensor driver interface and data contracts.
 */

#ifndef TEAM04_IMUDRV_H_
#define TEAM04_IMUDRV_H_

#include <stdint.h>
#include "spidrv.h"

namespace team04 {

/**
 * @brief Raw 16-bit 6-axis IMU measurements.
 */
struct ImuRawData {
    int16_t accelX;
    int16_t accelY;
    int16_t accelZ;
    int16_t gyroX;
    int16_t gyroY;
    int16_t gyroZ;
};

/**
 * @brief Scaled physical units: Accel in [g], Gyro in [dps] (degrees per second).
 */
struct ImuScaledData {
    float accelX_g;
    float accelY_g;
    float accelZ_g;
    float gyroX_dps;
    float gyroY_dps;
    float gyroZ_dps;
};

/**
 * @brief Raw accelerometer-only measurement for the high-rate capture path.
 */
struct AccelRawData {
    int16_t x;
    int16_t y;
    int16_t z;
};

/**
 * @brief Abstract IMU (Accelerometer + Gyroscope) driver.
 */
class ImuDrv {
public:
    ImuDrv() = default;
    virtual ~ImuDrv() = default;

    ImuDrv(const ImuDrv&) = delete;
    ImuDrv& operator=(const ImuDrv&) = delete;

    /**
     * @brief Initialize and configure the sensor.
     * @return kOk on success.
     */
    virtual IoStatus configure() = 0;

    /**
     * @brief Read raw 16-bit 6-axis measurements.
     */
    virtual IoStatus readRaw(ImuRawData& raw) = 0;

    /**
     * @brief Read scaled physical 6-axis measurements.
     */
    virtual IoStatus readScaled(ImuScaledData& scaled) = 0;
};

} // namespace team04

#endif // TEAM04_IMUDRV_H_

/***************************** END OF FILE ************************************/
