/*******************************************************************************
 * @file    lsm6ds3.c
 * @author  team-04 (Hackathon 2026)
 * @brief   LSM6DS3 IMU driver implementation for high-speed shock sensing.
 ******************************************************************************/

#include "lsm6ds3.h"
#include <stddef.h>

/* Static singleton driver instances for zero-allocation deployment */
static lsm6ds3_ctx_t s_default_ctx;
static imu_driver_t  s_default_driver;

/**
 * @brief Helper to read registers via abstract SPI device interface.
 * @param bus Pointer to SPI bus handle
 * @param reg Register start address
 * @param data Output data buffer
 * @param len Number of bytes to read
 * @return true on success, false on failure or invalid arguments
 */
static bool lsm6ds3_bus_read_reg(spi_bus_device_t *bus, uint8_t reg, uint8_t *data, uint16_t len)
{
    if ((bus == NULL) || (data == NULL) || (len == 0U)) {
        return false;
    }

    /* Fast path: bus driver provides dedicated register read implementation */
    if (bus->read_reg != NULL) {
        return bus->read_reg(bus, reg, data, len);
    }

    /* Fallback: perform raw synchronous transfer with CS assert/deassert */
    if ((bus->transfer_sync != NULL) && (bus->cs_select != NULL) && (bus->cs_deselect != NULL)) {
        uint8_t tx_cmd = (uint8_t)(reg | 0x80U); /* Bit 7 = 1 for SPI Read */
        uint8_t rx_dummy = 0U;

        bus->cs_select();
        bool ok = bus->transfer_sync(bus, &tx_cmd, &rx_dummy, 1U);
        if (ok) {
            ok = bus->transfer_sync(bus, NULL, data, len);
        }
        bus->cs_deselect();
        return ok;
    }

    return false;
}

/**
 * @brief Helper to write registers via abstract SPI device interface.
 * @param bus Pointer to SPI bus handle
 * @param reg Register start address
 * @param data Input data buffer
 * @param len Number of bytes to write
 * @return true on success, false on failure or invalid arguments
 */
static bool lsm6ds3_bus_write_reg(spi_bus_device_t *bus, uint8_t reg, const uint8_t *data, uint16_t len)
{
    if ((bus == NULL) || (data == NULL) || (len == 0U)) {
        return false;
    }

    /* Fast path: bus driver provides dedicated register write implementation */
    if (bus->write_reg != NULL) {
        return bus->write_reg(bus, reg, data, len);
    }

    /* Fallback: perform raw synchronous transfer with CS assert/deassert */
    if ((bus->transfer_sync != NULL) && (bus->cs_select != NULL) && (bus->cs_deselect != NULL)) {
        uint8_t tx_cmd = (uint8_t)(reg & 0x7FU); /* Bit 7 = 0 for SPI Write */
        uint8_t rx_dummy = 0U;

        bus->cs_select();
        bool ok = bus->transfer_sync(bus, &tx_cmd, &rx_dummy, 1U);
        if (ok) {
            ok = bus->transfer_sync(bus, data, NULL, len);
        }
        bus->cs_deselect();
        return ok;
    }

    return false;
}

/**
 * @brief Initialize concrete LSM6DS3 driver instance with user-supplied context.
 */
bool lsm6ds3_driver_init_instance(imu_driver_t *driver, lsm6ds3_ctx_t *ctx, spi_bus_device_t *bus)
{
    if ((driver == NULL) || (ctx == NULL) || (bus == NULL)) {
        return false;
    }

    ctx->bus = bus;
    ctx->sensitivity = LSM6DS3_SENSITIVITY_16G;
    ctx->is_initialized = false;

    driver->ctx = ctx;
    driver->init = lsm6ds3_init;
    driver->read_sample = lsm6ds3_read_sample;
    driver->is_data_ready = lsm6ds3_is_data_ready;

    return true;
}

/**
 * @brief Get pre-allocated static singleton LSM6DS3 IMU driver instance.
 */
imu_driver_t *lsm6ds3_get_driver(spi_bus_device_t *bus)
{
    if (lsm6ds3_driver_init_instance(&s_default_driver, &s_default_ctx, bus)) {
        return &s_default_driver;
    }
    return NULL;
}

/**
 * @brief Initialize LSM6DS3 sensor over SPI according to configuration.
 */
bool lsm6ds3_init(imu_driver_t *self, const imu_config_t *cfg)
{
    if ((self == NULL) || (self->ctx == NULL)) {
        return false;
    }

    lsm6ds3_ctx_t *ctx = (lsm6ds3_ctx_t *)self->ctx;
    if (ctx->bus == NULL) {
        return false;
    }

    /* 1. Verify WHO_AM_I device signature */
    uint8_t who_am_i = 0U;
    if (!lsm6ds3_bus_read_reg(ctx->bus, LSM6DS3_REG_WHO_AM_I, &who_am_i, 1U)) {
        return false;
    }

    if (who_am_i != LSM6DS3_WHO_AM_I_VAL) {
        return false;
    }

    /* 2. Configure full-scale range and output data rate (ODR) */
    uint8_t fs_bits = LSM6DS3_FS_XL_16G;
    uint8_t odr_bits = LSM6DS3_ODR_XL_6660HZ;
    float sensitivity = LSM6DS3_SENSITIVITY_16G;

    if (cfg != NULL) {
        switch (cfg->full_scale) {
            case 2U:
                fs_bits = LSM6DS3_FS_XL_2G;
                sensitivity = LSM6DS3_SENSITIVITY_2G;
                break;
            case 4U:
                fs_bits = LSM6DS3_FS_XL_4G;
                sensitivity = LSM6DS3_SENSITIVITY_4G;
                break;
            case 8U:
                fs_bits = LSM6DS3_FS_XL_8G;
                sensitivity = LSM6DS3_SENSITIVITY_8G;
                break;
            case 16U:
            default:
                fs_bits = LSM6DS3_FS_XL_16G;
                sensitivity = LSM6DS3_SENSITIVITY_16G;
                break;
        }

        if (cfg->odr_hz <= 12.5f) {
            odr_bits = LSM6DS3_ODR_XL_12_5HZ;
        } else if (cfg->odr_hz <= 26.0f) {
            odr_bits = LSM6DS3_ODR_XL_26HZ;
        } else if (cfg->odr_hz <= 52.0f) {
            odr_bits = LSM6DS3_ODR_XL_52HZ;
        } else if (cfg->odr_hz <= 104.0f) {
            odr_bits = LSM6DS3_ODR_XL_104HZ;
        } else if (cfg->odr_hz <= 208.0f) {
            odr_bits = LSM6DS3_ODR_XL_208HZ;
        } else if (cfg->odr_hz <= 416.0f) {
            odr_bits = LSM6DS3_ODR_XL_416HZ;
        } else if (cfg->odr_hz <= 833.0f) {
            odr_bits = LSM6DS3_ODR_XL_833HZ;
        } else if (cfg->odr_hz <= 1660.0f) {
            odr_bits = LSM6DS3_ODR_XL_1660HZ;
        } else if (cfg->odr_hz <= 3330.0f) {
            odr_bits = LSM6DS3_ODR_XL_3330HZ;
        } else {
            odr_bits = LSM6DS3_ODR_XL_6660HZ;
        }
    }

    /* Program CTRL1_XL: ODR (bits 7:4) | FS_XL (bits 3:2) | BW_XL (bits 1:0) */
    uint8_t ctrl1_val = (uint8_t)(odr_bits | fs_bits);
    if (!lsm6ds3_bus_write_reg(ctx->bus, LSM6DS3_REG_CTRL1_XL, &ctrl1_val, 1U)) {
        return false;
    }

    /* Configure CTRL3_C: Enable register auto-increment (IF_INC) & block data update (BDU) */
    uint8_t ctrl3_val = (uint8_t)(LSM6DS3_CTRL3_C_IF_INC | LSM6DS3_CTRL3_C_BDU);
    if (!lsm6ds3_bus_write_reg(ctx->bus, LSM6DS3_REG_CTRL3_C, &ctrl3_val, 1U)) {
        return false;
    }

    ctx->sensitivity = sensitivity;
    ctx->is_initialized = true;

    return true;
}

/**
 * @brief Read raw 16-bit signed integer values directly from sensor registers.
 */
bool lsm6ds3_read_raw(lsm6ds3_ctx_t *ctx, int16_t *x, int16_t *y, int16_t *z)
{
    if ((ctx == NULL) || (ctx->bus == NULL)) {
        return false;
    }

    /* Read 6 consecutive bytes from OUTX_L_XL (0x28) to OUTZ_H_XL (0x2D) */
    uint8_t raw_buf[6] = {0};
    if (!lsm6ds3_bus_read_reg(ctx->bus, LSM6DS3_REG_OUTX_L_XL, raw_buf, (uint16_t)sizeof(raw_buf))) {
        return false;
    }

    if (x != NULL) {
        *x = (int16_t)(((uint16_t)raw_buf[1] << 8) | (uint16_t)raw_buf[0]);
    }
    if (y != NULL) {
        *y = (int16_t)(((uint16_t)raw_buf[3] << 8) | (uint16_t)raw_buf[2]);
    }
    if (z != NULL) {
        *z = (int16_t)(((uint16_t)raw_buf[5] << 8) | (uint16_t)raw_buf[4]);
    }

    return true;
}

/**
 * @brief Read acceleration sample converted to float 'g' from LSM6DS3.
 */
bool lsm6ds3_read_sample(imu_driver_t *self, imu_accel_raw_t *out_sample)
{
    if ((self == NULL) || (self->ctx == NULL) || (out_sample == NULL)) {
        return false;
    }

    lsm6ds3_ctx_t *ctx = (lsm6ds3_ctx_t *)self->ctx;
    int16_t raw_x = 0;
    int16_t raw_y = 0;
    int16_t raw_z = 0;

    if (!lsm6ds3_read_raw(ctx, &raw_x, &raw_y, &raw_z)) {
        return false;
    }

    /* Convert raw 16-bit counts to engineering unit 'g' */
    out_sample->x = (float)raw_x * ctx->sensitivity;
    out_sample->y = (float)raw_y * ctx->sensitivity;
    out_sample->z = (float)raw_z * ctx->sensitivity;

    return true;
}

/**
 * @brief Check if new accelerometer data is available.
 */
bool lsm6ds3_is_data_ready(imu_driver_t *self)
{
    if ((self == NULL) || (self->ctx == NULL)) {
        return false;
    }

    lsm6ds3_ctx_t *ctx = (lsm6ds3_ctx_t *)self->ctx;
    if (ctx->bus == NULL) {
        return false;
    }

    uint8_t status = 0U;
    if (!lsm6ds3_bus_read_reg(ctx->bus, LSM6DS3_REG_STATUS_REG, &status, 1U)) {
        return false;
    }

    return ((status & LSM6DS3_STATUS_XLDA) != 0U);
}

/***************************** END OF FILE ************************************/
