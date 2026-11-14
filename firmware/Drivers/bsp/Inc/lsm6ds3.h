/*******************************************************************************
 * @file    lsm6ds3.h
 * @author  team-04 (Hackathon 2026)
 * @brief   LSM6DS3 6-axis IMU accelerometer driver interface.
 ******************************************************************************/

#ifndef LSM6DS3_H_
#define LSM6DS3_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "bus_spi.h"
#include "sensor_imu.h"

/* Registers */
#define LSM6DS3_REG_WHO_AM_I            (0x0FU)  ///< Device identification register
#define LSM6DS3_WHO_AM_I_VAL            (0x69U)  ///< Expected WHO_AM_I value

#define LSM6DS3_REG_CTRL1_XL            (0x10U)  ///< Accelerometer control register 1
#define LSM6DS3_REG_CTRL2_G             (0x11U)  ///< Gyroscope control register 2
#define LSM6DS3_REG_CTRL3_C             (0x12U)  ///< Control register 3
#define LSM6DS3_REG_STATUS_REG          (0x1EU)  ///< Status register

#define LSM6DS3_REG_OUTX_L_XL           (0x28U)  ///< X-axis output low byte
#define LSM6DS3_REG_OUTX_H_XL           (0x29U)  ///< X-axis output high byte
#define LSM6DS3_REG_OUTY_L_XL           (0x2AU)  ///< Y-axis output low byte
#define LSM6DS3_REG_OUTY_H_XL           (0x2BU)  ///< Y-axis output high byte
#define LSM6DS3_REG_OUTZ_L_XL           (0x2CU)  ///< Z-axis output low byte
#define LSM6DS3_REG_OUTZ_H_XL           (0x2DU)  ///< Z-axis output high byte

/* CTRL1_XL Configuration Bits */
#define LSM6DS3_ODR_XL_POWER_DOWN       (0x00U)  ///< Accelerometer power-down
#define LSM6DS3_ODR_XL_12_5HZ           (0x10U)  ///< ODR 12.5 Hz
#define LSM6DS3_ODR_XL_26HZ             (0x20U)  ///< ODR 26 Hz
#define LSM6DS3_ODR_XL_52HZ             (0x30U)  ///< ODR 52 Hz
#define LSM6DS3_ODR_XL_104HZ            (0x40U)  ///< ODR 104 Hz
#define LSM6DS3_ODR_XL_208HZ            (0x50U)  ///< ODR 208 Hz
#define LSM6DS3_ODR_XL_416HZ            (0x60U)  ///< ODR 416 Hz
#define LSM6DS3_ODR_XL_833HZ            (0x70U)  ///< ODR 833 Hz
#define LSM6DS3_ODR_XL_1660HZ           (0x80U)  ///< ODR 1.66 kHz
#define LSM6DS3_ODR_XL_3330HZ           (0x90U)  ///< ODR 3.33 kHz
#define LSM6DS3_ODR_XL_6660HZ           (0xA0U)  ///< ODR 6.66 kHz (high-performance)

#define LSM6DS3_FS_XL_2G                (0x00U)  ///< Full scale ±2g
#define LSM6DS3_FS_XL_16G               (0x04U)  ///< Full scale ±16g
#define LSM6DS3_FS_XL_4G                (0x08U)  ///< Full scale ±4g
#define LSM6DS3_FS_XL_8G                (0x0CU)  ///< Full scale ±8g

#define LSM6DS3_BW_XL_400HZ             (0x00U)  ///< Anti-aliasing bandwidth 400 Hz
#define LSM6DS3_BW_XL_200HZ             (0x01U)  ///< Anti-aliasing bandwidth 200 Hz
#define LSM6DS3_BW_XL_100HZ             (0x02U)  ///< Anti-aliasing bandwidth 100 Hz
#define LSM6DS3_BW_XL_50HZ              (0x03U)  ///< Anti-aliasing bandwidth 50 Hz

/* Sensitivity (g / LSB) */
#define LSM6DS3_SENSITIVITY_2G          (0.000061f) ///< Sensitivity for ±2g scale [g/LSB]
#define LSM6DS3_SENSITIVITY_4G          (0.000122f) ///< Sensitivity for ±4g scale [g/LSB]
#define LSM6DS3_SENSITIVITY_8G          (0.000244f) ///< Sensitivity for ±8g scale [g/LSB]
#define LSM6DS3_SENSITIVITY_16G         (0.000488f) ///< Sensitivity for ±16g scale [g/LSB] (~0.488 mg/LSB)

/* CTRL3_C Bits */
#define LSM6DS3_CTRL3_C_IF_INC          (0x04U)     ///< Register address auto-increment
#define LSM6DS3_CTRL3_C_BDU             (0x40U)     ///< Block data update

/* STATUS_REG Bits */
#define LSM6DS3_STATUS_XLDA             (0x01U)     ///< Accelerometer new data available

/**
 * @brief Concrete context structure for LSM6DS3 driver.
 */
typedef struct {
    spi_bus_device_t *bus;              ///< Pointer to abstract SPI bus handle
    float sensitivity;                  ///< Current sensitivity factor [g/LSB]
    bool is_initialized;                ///< Initialization state flag
} lsm6ds3_ctx_t;

/**
 * @brief Initialize concrete LSM6DS3 driver instance with user-supplied context.
 * @param driver Pointer to IMU driver interface table to populate
 * @param ctx Pointer to driver context memory
 * @param bus Pointer to SPI bus device handle
 * @return true on success, false on invalid arguments
 */
bool lsm6ds3_driver_init_instance(imu_driver_t *driver, lsm6ds3_ctx_t *ctx, spi_bus_device_t *bus);

/**
 * @brief Get pre-allocated static singleton LSM6DS3 IMU driver instance.
 * @param bus Pointer to SPI bus device handle
 * @return Pointer to initialized imu_driver_t interface, or NULL on error
 */
imu_driver_t *lsm6ds3_get_driver(spi_bus_device_t *bus);

/**
 * @brief Initialize LSM6DS3 sensor over SPI according to configuration.
 * Verifies WHO_AM_I (0x69) and programs CTRL1_XL.
 * @param self Pointer to imu_driver_t interface
 * @param cfg Pointer to target configuration (NULL for default 6.66 kHz, ±16g)
 * @return true on success, false otherwise
 */
bool lsm6ds3_init(imu_driver_t *self, const imu_config_t *cfg);

/**
 * @brief Read acceleration sample converted to float 'g' from LSM6DS3.
 * Reads registers OUTX_L_XL (0x28) through OUTZ_H_XL (0x2D).
 * @param self Pointer to imu_driver_t interface
 * @param out_sample Pointer to output acceleration structure
 * @return true on success, false otherwise
 */
bool lsm6ds3_read_sample(imu_driver_t *self, imu_accel_raw_t *out_sample);

/**
 * @brief Check if new accelerometer data is available.
 * Reads STATUS_REG (0x1E) and checks XLDA bit.
 * @param self Pointer to imu_driver_t interface
 * @return true if new data is ready, false otherwise
 */
bool lsm6ds3_is_data_ready(imu_driver_t *self);

/**
 * @brief Read raw 16-bit signed integer values directly from sensor registers.
 * Reads 6 contiguous bytes from OUTX_L_XL (0x28) to OUTZ_H_XL (0x2D).
 * @param ctx Pointer to LSM6DS3 driver context
 * @param x Output pointer for raw X-axis value
 * @param y Output pointer for raw Y-axis value
 * @param z Output pointer for raw Z-axis value
 * @return true on success, false otherwise
 */
bool lsm6ds3_read_raw(lsm6ds3_ctx_t *ctx, int16_t *x, int16_t *y, int16_t *z);

#ifdef __cplusplus
}
#endif

#endif /* LSM6DS3_H_ */
/***************************** END OF FILE ************************************/
