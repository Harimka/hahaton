/*******************************************************************************
 * @file    sensor_imu.h
 * @author  team-04 (Hackathon 2026)
 * @brief   Abstract hardware-independent IMU / Accelerometer sensor interface.
 ******************************************************************************/

#ifndef SENSOR_IMU_H_
#define SENSOR_IMU_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 3-Axis acceleration vector in 'g' units.
 */
typedef struct {
    float x;
    float y;
    float z;
} imu_accel_raw_t;

/**
 * @brief IMU configuration parameters.
 */
typedef struct {
    float odr_hz;       ///< Output data rate target (e.g. 6660.0f)
    uint8_t full_scale; ///< Full scale in g (e.g. 16)
} imu_config_t;

/**
 * @brief Universal IMU driver interface table.
 */
typedef struct imu_driver_s {
    void *ctx; ///< Concrete driver context (e.g. lsm6ds3_t*)
    bool (*init)(struct imu_driver_s *self, const imu_config_t *cfg);
    bool (*read_sample)(struct imu_driver_s *self, imu_accel_raw_t *out_sample);
    bool (*is_data_ready)(struct imu_driver_s *self);
} imu_driver_t;

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_IMU_H_ */
/***************************** END OF FILE ************************************/
