/** @file lsm6ds3_c.c
 * @author team-04
 * @brief Pure C LSM6DS3 register and FIFO protocol implementation.
 */
#include "lsm6ds3_c.h"
#include <stddef.h>
#include <limits.h>

#define LSM6DS3_WHO_AM_I_VALUE 0x69U
#define REG_FIFO_CTRL1 0x06U
#define REG_FIFO_CTRL2 0x07U
#define REG_FIFO_CTRL3 0x08U
#define REG_FIFO_CTRL4 0x09U
#define REG_FIFO_CTRL5 0x0aU
#define REG_INT1_CTRL 0x0dU
#define REG_WHO_AM_I 0x0fU
#define REG_CTRL1_XL 0x10U
#define REG_CTRL2_G 0x11U
#define REG_CTRL3_C 0x12U
#define REG_CTRL4_C 0x13U
#define REG_CTRL6_C 0x15U
#define REG_CTRL8_XL 0x17U
#define REG_CTRL9_XL 0x18U
#define REG_STATUS_REG 0x1eU
#define REG_OUTX_L_XL 0x28U
#define REG_FIFO_STATUS1 0x3aU
#define REG_FIFO_DATA_OUT_L 0x3eU
#define SPI_TIMEOUT_MS 20U

static lsm6ds3_c_status_t read_register(lsm6ds3_c_t *driver, uint8_t reg, uint8_t *data, uint16_t length)
{
    uint8_t command;
    lsm6ds3_c_status_t status;
    if (driver == NULL || data == NULL || length == 0U ||
        driver->bus.select == NULL || driver->bus.deselect == NULL || driver->bus.transfer == NULL)
        return LSM6DS3_C_INVALID;
    command = (uint8_t)(reg | 0x80U);
    driver->bus.select(driver->bus.context);
    status = driver->bus.transfer(driver->bus.context, &command, NULL, 1U, SPI_TIMEOUT_MS);
    if (status == LSM6DS3_C_OK)
        status = driver->bus.transfer(driver->bus.context, NULL, data, length, SPI_TIMEOUT_MS);
    driver->bus.deselect(driver->bus.context);
    return status;
}

static lsm6ds3_c_status_t write_checked(lsm6ds3_c_t *driver, uint8_t reg, uint8_t value, uint8_t mask)
{
    uint8_t tx[2] = {(uint8_t)(reg & 0x7fU), value};
    uint8_t readback = 0U;
    lsm6ds3_c_status_t status;
    driver->bus.select(driver->bus.context);
    status = driver->bus.transfer(driver->bus.context, tx, NULL, 2U, SPI_TIMEOUT_MS);
    driver->bus.deselect(driver->bus.context);
    if (status != LSM6DS3_C_OK) return status;
    status = read_register(driver, reg, &readback, 1U);
    if (status != LSM6DS3_C_OK) return status;
    return ((readback & mask) == (value & mask)) ? LSM6DS3_C_OK : LSM6DS3_C_IO_ERROR;
}

static int16_t decode_word(const uint8_t *bytes)
{
    uint16_t bits = (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
    int32_t value = (bits & 0x8000U) ? ((int32_t)bits - 65536) : (int32_t)bits;
    return (int16_t)value;
}

void lsm6ds3_c_init(lsm6ds3_c_t *driver, const lsm6ds3_c_bus_t *bus)
{
    if (driver == NULL) return;
    driver->observed_id = 0U;
    driver->configured = false;
    if (bus == NULL) {
        driver->bus.context = NULL;
        driver->bus.select = NULL;
        driver->bus.deselect = NULL;
        driver->bus.transfer = NULL;
        return;
    }
    driver->bus = *bus;
}

lsm6ds3_c_status_t lsm6ds3_c_reset_begin(lsm6ds3_c_t *driver)
{
    uint8_t tx[2] = {REG_CTRL3_C, 1U};
    lsm6ds3_c_status_t status;
    if (driver == NULL || driver->bus.select == NULL || driver->bus.deselect == NULL || driver->bus.transfer == NULL)
        return LSM6DS3_C_INVALID;
    driver->configured = false;
    driver->bus.select(driver->bus.context);
    status = driver->bus.transfer(driver->bus.context, tx, NULL, 2U, SPI_TIMEOUT_MS);
    driver->bus.deselect(driver->bus.context);
    return status;
}

lsm6ds3_c_status_t lsm6ds3_c_reset_poll(lsm6ds3_c_t *driver, bool *complete)
{
    uint8_t value = 0U;
    lsm6ds3_c_status_t status;
    if (complete == NULL) return LSM6DS3_C_INVALID;
    status = read_register(driver, REG_CTRL3_C, &value, 1U);
    if (status == LSM6DS3_C_OK) *complete = (value & 1U) == 0U;
    return status;
}

lsm6ds3_c_status_t lsm6ds3_c_configure(lsm6ds3_c_t *driver)
{
    static const uint8_t registers[] = {REG_CTRL3_C, REG_CTRL2_G, REG_CTRL4_C, REG_CTRL6_C,
        REG_CTRL8_XL, REG_CTRL9_XL, REG_INT1_CTRL, REG_CTRL1_XL};
    static const uint8_t values[] = {0x44U, 0xacU, 0U, 0U, 0U, 0x38U, 0U, 0xa4U};
    static const uint8_t masks[] = {0x7eU, 0xf0U, 0x80U, 0x10U, 0xe4U, 0x38U, 0xffU, 0xffU};
    size_t i;
    lsm6ds3_c_status_t status;
    if (driver == NULL || driver->bus.select == NULL || driver->bus.deselect == NULL || driver->bus.transfer == NULL)
        return LSM6DS3_C_INVALID;
    driver->configured = false;
    status = read_register(driver, REG_WHO_AM_I, &driver->observed_id, 1U);
    if (status != LSM6DS3_C_OK) return status;
    if (driver->observed_id != LSM6DS3_WHO_AM_I_VALUE) return LSM6DS3_C_UNSUPPORTED;
    for (i = 0U; i < sizeof(registers) / sizeof(registers[0]); ++i) {
        status = write_checked(driver, registers[i], values[i], masks[i]);
        if (status != LSM6DS3_C_OK) return status;
    }
    driver->configured = true;
    return LSM6DS3_C_OK;
}

lsm6ds3_c_status_t lsm6ds3_c_read_raw(lsm6ds3_c_t *driver, int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t status_reg = 0U;
    uint8_t data[6];
    lsm6ds3_c_status_t status;
    if (driver == NULL || x == NULL || y == NULL || z == NULL) return LSM6DS3_C_INVALID;
    if (!driver->configured) return LSM6DS3_C_NOT_READY;
    status = read_register(driver, REG_STATUS_REG, &status_reg, 1U);
    if (status != LSM6DS3_C_OK) return status;
    if ((status_reg & 1U) == 0U) return LSM6DS3_C_NOT_READY;
    status = read_register(driver, REG_OUTX_L_XL, data, 6U);
    if (status == LSM6DS3_C_OK) {
        *x = decode_word(data);
        *y = decode_word(data + 2U);
        *z = decode_word(data + 4U);
    }
    return status;
}

lsm6ds3_c_status_t lsm6ds3_c_configure_fifo(lsm6ds3_c_t *driver, uint16_t watermark_samples)
{
    uint16_t words;
    uint8_t registers[7];
    uint8_t values[7];
    static const uint8_t masks[7] = {0xffU, 0x7fU, 0xffU, 0xcfU, 0x3fU, 0x7fU, 0x7fU};
    size_t i;
    lsm6ds3_c_status_t status;
    if (driver == NULL) return LSM6DS3_C_INVALID;
    if (!driver->configured) return LSM6DS3_C_NOT_READY;
    if (watermark_samples == 0U || watermark_samples > 1365U) return LSM6DS3_C_INVALID;
    words = (uint16_t)(watermark_samples * 6U);
    registers[0] = REG_INT1_CTRL; registers[1] = REG_FIFO_CTRL5;
    registers[2] = REG_FIFO_CTRL1; registers[3] = REG_FIFO_CTRL2; registers[4] = REG_FIFO_CTRL3;
    registers[5] = REG_FIFO_CTRL4; registers[6] = REG_FIFO_CTRL5;
    values[0] = 0U; values[1] = 0U; values[2] = (uint8_t)words;
    values[3] = (uint8_t)(words >> 8U); values[4] = 0x09U; values[5] = 0U; values[6] = 0x56U;
    for (i = 0U; i < 7U; ++i) {
        status = write_checked(driver, registers[i], values[i], masks[i]);
        if (status != LSM6DS3_C_OK) return status;
    }
    return LSM6DS3_C_OK;
}

lsm6ds3_c_status_t lsm6ds3_c_enable_fifo_interrupt(lsm6ds3_c_t *driver, bool enabled)
{
    if (driver == NULL) return LSM6DS3_C_INVALID;
    if (!driver->configured) return LSM6DS3_C_NOT_READY;
    return write_checked(driver, REG_INT1_CTRL, enabled ? 0x08U : 0U, 0xffU);
}

lsm6ds3_c_status_t lsm6ds3_c_fifo_status(lsm6ds3_c_t *driver, lsm6ds3_c_fifo_status_t *out)
{
    uint8_t bytes[4];
    lsm6ds3_c_status_t status;
    if (driver == NULL || out == NULL) return LSM6DS3_C_INVALID;
    if (!driver->configured) return LSM6DS3_C_NOT_READY;
    status = read_register(driver, REG_FIFO_STATUS1, bytes, 4U);
    if (status != LSM6DS3_C_OK) return status;
    out->unread_words = (uint16_t)((uint16_t)bytes[0] | ((uint16_t)(bytes[1] & 0x0fU) << 8U));
    out->next_pattern = (uint16_t)((uint16_t)bytes[2] | ((uint16_t)(bytes[3] & 3U) << 8U));
    out->empty = (bytes[1] & 0x10U) != 0U;
    out->full = (bytes[1] & 0x20U) != 0U;
    out->overrun = (bytes[1] & 0x40U) != 0U;
    out->watermark = (bytes[1] & 0x80U) != 0U;
    return LSM6DS3_C_OK;
}

bool lsm6ds3_c_prepare_fifo_dma(uint8_t *tx, uint16_t samples, uint16_t capacity)
{
    uint32_t bytes = 1U + (uint32_t)samples * 6U;
    uint32_t i;
    if (tx == NULL || samples == 0U || bytes > capacity) return false;
    tx[0] = (uint8_t)(REG_FIFO_DATA_OUT_L | 0x80U);
    for (i = 1U; i < bytes; ++i) tx[i] = 0xffU;
    return true;
}

bool lsm6ds3_c_decode_fifo_sample(const uint8_t *rx, uint16_t samples, uint16_t index,
                                  int16_t *x, int16_t *y, int16_t *z)
{
    const uint8_t *sample;
    if (rx == NULL || x == NULL || y == NULL || z == NULL || index >= samples) return false;
    sample = rx + 1U + (uint32_t)index * 6U;
    *x = decode_word(sample);
    *y = decode_word(sample + 2U);
    *z = decode_word(sample + 4U);
    return true;
}
/***************************** END OF FILE ************************************/
