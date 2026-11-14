/*******************************************************************************
 * @file    imudrv.h
 * @author  team-04 (Hackathon 2026)
 * @brief   Abstract IMU / Accelerometer sensor interface (ZT-style).
 ******************************************************************************/

#pragma once

#include <cstdint>

/**
 * @brief 3-Axis acceleration vector in 'g' units
 */
struct AccelData {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

/**
 * @brief Driver execution status codes
 */
enum class DrvResult {
    kOk = 0,
    kError,
    kTimeout,
    kNotReady
};

/**
 * @brief Abstract IMU sensor base class
 */
class ImuDrv {
public:
    virtual ~ImuDrv() = default;

    /**
     * @brief Configure and initialize sensor hardware
     */
    virtual DrvResult configure() = 0;

    /**
     * @brief Check if new acceleration data is ready
     */
    virtual bool isDataReady() = 0;

    /**
     * @brief Read last acceleration sample (in g)
     */
    virtual DrvResult readSample(AccelData& out) = 0;
};

/***************************** END OF FILE ************************************/
