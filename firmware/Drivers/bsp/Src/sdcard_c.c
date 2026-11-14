/** @file sdcard_c.c
 * @author team-04
 * @brief Pure C SD v1/v2 and SDHC/SDXC SPI protocol implementation.
 */
#include "sdcard_c.h"
#include <limits.h>
#include <stddef.h>

#define SDCARD_C_POLL_LIMIT 1048576U
#define SDCARD_C_TRANSFER_TIMEOUT_MS 100U

static bool valid_driver(const sdcard_c_t *driver)
{
    return driver != NULL && driver->ops.set_clock != NULL && driver->ops.select != NULL &&
           driver->ops.deselect != NULL && driver->ops.transfer != NULL && driver->ops.millis != NULL;
}

static sdcard_c_status_t transfer(sdcard_c_t *driver, const uint8_t *tx, uint8_t *rx, uint16_t length)
{
    sdcard_c_status_t status = driver->ops.transfer(driver->ops.context, tx, rx, length,
                                                    SDCARD_C_TRANSFER_TIMEOUT_MS);
    if (status != SDCARD_C_OK) ++driver->transfer_error_count;
    return status;
}

static void deselect(sdcard_c_t *driver)
{
    driver->ops.deselect(driver->ops.context);
    (void)transfer(driver, NULL, NULL, 1U);
}

static sdcard_c_status_t command(sdcard_c_t *driver, uint8_t index, uint32_t arg, uint8_t crc,
                                 uint8_t *response)
{
    uint8_t cmd[6] = {(uint8_t)(0x40U | index), (uint8_t)(arg >> 24U),
        (uint8_t)(arg >> 16U), (uint8_t)(arg >> 8U), (uint8_t)arg, crc};
    sdcard_c_status_t status = transfer(driver, NULL, NULL, 1U);
    uint8_t i;
    if (response == NULL) return SDCARD_C_INVALID;
    if (status == SDCARD_C_OK) status = transfer(driver, cmd, NULL, 6U);
    if (status != SDCARD_C_OK) return status;
    for (i = 0U; i < 16U; ++i) {
        status = transfer(driver, NULL, response, 1U);
        if (status != SDCARD_C_OK) return status;
        if ((*response & 0x80U) == 0U) {
            driver->last_response = *response;
            return SDCARD_C_OK;
        }
    }
    return SDCARD_C_TIMEOUT;
}

static sdcard_c_status_t single_command(sdcard_c_t *driver, uint8_t index, uint32_t arg,
                                        uint8_t crc, uint8_t *response)
{
    sdcard_c_status_t status;
    driver->ops.select(driver->ops.context);
    status = command(driver, index, arg, crc, response);
    deselect(driver);
    return status;
}

static sdcard_c_status_t wait_for(sdcard_c_t *driver, uint8_t expected, uint32_t timeout_ms,
                                  bool data_token)
{
    uint32_t start = driver->ops.millis(driver->ops.context);
    uint32_t i;
    for (i = 0U; i < SDCARD_C_POLL_LIMIT; ++i) {
        uint8_t byte;
        sdcard_c_status_t status = transfer(driver, NULL, &byte, 1U);
        if (status != SDCARD_C_OK) return status;
        if (byte == expected) return SDCARD_C_OK;
        if (data_token && byte != 0xffU) {
            driver->last_data_response = byte;
            return SDCARD_C_ERROR;
        }
        if ((uint32_t)(driver->ops.millis(driver->ops.context) - start) >= timeout_ms) break;
    }
    return SDCARD_C_TIMEOUT;
}

static uint16_t crc16(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0U;
    uint16_t i;
    for (i = 0U; i < length; ++i) {
        uint8_t bit;
        crc ^= (uint16_t)((uint16_t)data[i] << 8U);
        for (bit = 0U; bit < 8U; ++bit)
            crc = (uint16_t)((crc & 0x8000U) ? (uint16_t)(crc << 1U) ^ 0x1021U : (uint16_t)(crc << 1U));
    }
    return crc;
}

static sdcard_c_status_t address(const sdcard_c_t *driver, uint32_t lba, uint32_t *arg)
{
    if (!driver->ready) return SDCARD_C_NOT_READY;
    if (!driver->block_addressed && lba > UINT32_MAX / 512U) return SDCARD_C_INVALID;
    *arg = driver->block_addressed ? lba : lba * 512U;
    return SDCARD_C_OK;
}

void sdcard_c_init(sdcard_c_t *driver, const sdcard_c_ops_t *ops)
{
    if (driver == NULL) return;
    driver->ready = false;
    driver->block_addressed = false;
    driver->last_response = 0xffU;
    driver->last_data_response = 0xffU;
    driver->max_write_ms = 0U;
    driver->transfer_error_count = 0U;
    if (ops == NULL) {
        driver->ops.context = NULL;
        driver->ops.set_clock = NULL;
        driver->ops.select = NULL;
        driver->ops.deselect = NULL;
        driver->ops.transfer = NULL;
        driver->ops.millis = NULL;
    } else {
        driver->ops = *ops;
    }
}

sdcard_c_status_t sdcard_c_initialize(sdcard_c_t *driver)
{
    sdcard_c_status_t status;
    uint8_t response = 0xffU;
    uint32_t start;
    uint16_t i;
    bool version2 = false;
    if (!valid_driver(driver)) return SDCARD_C_INVALID;
    driver->ready = false;
    driver->block_addressed = false;
    driver->last_response = 0xffU;
    driver->ops.deselect(driver->ops.context);
    status = driver->ops.set_clock(driver->ops.context, 400000U);
    if (status != SDCARD_C_OK) return status;
    status = transfer(driver, NULL, NULL, 10U);
    if (status != SDCARD_C_OK) return status;
    start = driver->ops.millis(driver->ops.context);
    for (i = 0U; i < 1024U; ++i) {
        status = single_command(driver, 0U, 0U, 0x95U, &response);
        if (status == SDCARD_C_OK && response == 1U) break;
        if (status != SDCARD_C_OK && status != SDCARD_C_TIMEOUT) return status;
        if ((uint32_t)(driver->ops.millis(driver->ops.context) - start) >= 1500U) break;
    }
    if (status != SDCARD_C_OK || response != 1U) return SDCARD_C_TIMEOUT;

    driver->ops.select(driver->ops.context);
    status = command(driver, 8U, 0x1aaU, 0x87U, &response);
    if (status == SDCARD_C_OK && response == 1U) {
        uint8_t r7[4];
        status = transfer(driver, NULL, r7, 4U);
        if (status == SDCARD_C_OK) {
            if (r7[2] != 1U || r7[3] != 0xaaU) status = SDCARD_C_UNSUPPORTED;
            else version2 = true;
        }
    } else if (status == SDCARD_C_OK && response != 5U) {
        status = SDCARD_C_ERROR;
    }
    deselect(driver);
    if (status != SDCARD_C_OK) return status;

    start = driver->ops.millis(driver->ops.context);
    for (i = 0U; i < 4096U; ++i) {
        driver->ops.select(driver->ops.context);
        status = command(driver, 55U, 0U, 0x01U, &response);
        if (status == SDCARD_C_OK && (response == 0U || response == 1U))
            status = command(driver, 41U, version2 ? 0x40000000U : 0U, 0x01U, &response);
        else if (status == SDCARD_C_OK)
            status = SDCARD_C_UNSUPPORTED;
        deselect(driver);
        if (status != SDCARD_C_OK) return status;
        if (response == 0U) break;
        if (response != 1U) return SDCARD_C_ERROR;
        if ((uint32_t)(driver->ops.millis(driver->ops.context) - start) >= 1500U) break;
    }
    if (response != 0U) return SDCARD_C_TIMEOUT;

    driver->ops.select(driver->ops.context);
    status = command(driver, 58U, 0U, 0x01U, &response);
    if (status == SDCARD_C_OK && response == 0U) {
        uint8_t ocr[4];
        status = transfer(driver, NULL, ocr, 4U);
        if (status == SDCARD_C_OK) {
            if ((ocr[0] & 0x80U) == 0U) status = SDCARD_C_NOT_READY;
            else driver->block_addressed = version2 && (ocr[0] & 0x40U) != 0U;
        }
    } else if (status == SDCARD_C_OK) {
        status = SDCARD_C_ERROR;
    }
    deselect(driver);
    if (status != SDCARD_C_OK) return status;
    if (!driver->block_addressed) {
        status = single_command(driver, 16U, 512U, 0x01U, &response);
        if (status != SDCARD_C_OK) return status;
        if (response != 0U) return SDCARD_C_ERROR;
    }
    status = driver->ops.set_clock(driver->ops.context, 10000000U);
    if (status != SDCARD_C_OK) return status;
    driver->ready = true;
    return SDCARD_C_OK;
}

sdcard_c_status_t sdcard_c_read(sdcard_c_t *driver, uint32_t lba, uint8_t *buffer)
{
    uint32_t arg;
    uint8_t response, crc[2];
    sdcard_c_status_t status;
    if (!valid_driver(driver) || buffer == NULL) return SDCARD_C_INVALID;
    status = address(driver, lba, &arg);
    if (status != SDCARD_C_OK) return status;
    driver->ops.select(driver->ops.context);
    status = command(driver, 17U, arg, 0x01U, &response);
    if (status == SDCARD_C_OK && response != 0U) status = SDCARD_C_ERROR;
    if (status == SDCARD_C_OK) status = wait_for(driver, 0xfeU, 300U, true);
    if (status == SDCARD_C_OK) status = transfer(driver, NULL, buffer, 512U);
    if (status == SDCARD_C_OK) status = transfer(driver, NULL, crc, 2U);
    if (status == SDCARD_C_OK && crc16(buffer, 512U) != (uint16_t)(((uint16_t)crc[0] << 8U) | crc[1]))
        status = SDCARD_C_ERROR;
    deselect(driver);
    return status;
}

sdcard_c_status_t sdcard_c_write(sdcard_c_t *driver, uint32_t lba, const uint8_t *buffer)
{
    uint32_t arg, start, elapsed;
    uint8_t response, token = 0xfeU;
    uint16_t checksum;
    uint8_t check[2];
    sdcard_c_status_t status;
    if (!valid_driver(driver) || buffer == NULL) return SDCARD_C_INVALID;
    status = address(driver, lba, &arg);
    if (status != SDCARD_C_OK) return status;
    start = driver->ops.millis(driver->ops.context);
    checksum = crc16(buffer, 512U);
    check[0] = (uint8_t)(checksum >> 8U);
    check[1] = (uint8_t)checksum;
    driver->ops.select(driver->ops.context);
    status = command(driver, 24U, arg, 0x01U, &response);
    if (status == SDCARD_C_OK && response != 0U) status = SDCARD_C_ERROR;
    if (status == SDCARD_C_OK) status = transfer(driver, &token, NULL, 1U);
    if (status == SDCARD_C_OK) status = transfer(driver, buffer, NULL, 512U);
    if (status == SDCARD_C_OK) status = transfer(driver, check, NULL, 2U);
    if (status == SDCARD_C_OK) {
        uint8_t i;
        response = 0xffU;
        for (i = 0U; i < 32U && response == 0xffU; ++i) {
            status = transfer(driver, NULL, &response, 1U);
            if (status != SDCARD_C_OK) break;
        }
        driver->last_data_response = response;
        if (status == SDCARD_C_OK && response == 0xffU) status = SDCARD_C_TIMEOUT;
        else if (status == SDCARD_C_OK && (response & 0x1fU) != 5U) status = SDCARD_C_ERROR;
    }
    if (status == SDCARD_C_OK) status = wait_for(driver, 0xffU, 1000U, false);
    if (status == SDCARD_C_OK) {
        uint8_t card_status = 0xffU;
        status = command(driver, 13U, 0U, 0x01U, &response);
        if (status == SDCARD_C_OK) status = transfer(driver, NULL, &card_status, 1U);
        if (status == SDCARD_C_OK && (response != 0U || card_status != 0U)) status = SDCARD_C_ERROR;
    }
    deselect(driver);
    elapsed = driver->ops.millis(driver->ops.context) - start;
    if (elapsed > driver->max_write_ms) driver->max_write_ms = elapsed;
    return status;
}

sdcard_c_status_t sdcard_c_sector_count(sdcard_c_t *driver, uint64_t *sectors)
{
    uint8_t response, csd[16], crc[2];
    sdcard_c_status_t status;
    uint8_t version;
    if (!valid_driver(driver) || sectors == NULL) return SDCARD_C_INVALID;
    if (!driver->ready) return SDCARD_C_NOT_READY;
    driver->ops.select(driver->ops.context);
    status = command(driver, 9U, 0U, 0x01U, &response);
    if (status == SDCARD_C_OK && response != 0U) status = SDCARD_C_ERROR;
    if (status == SDCARD_C_OK) status = wait_for(driver, 0xfeU, 300U, true);
    if (status == SDCARD_C_OK) status = transfer(driver, NULL, csd, 16U);
    if (status == SDCARD_C_OK) status = transfer(driver, NULL, crc, 2U);
    if (status == SDCARD_C_OK && crc16(csd, 16U) != (uint16_t)(((uint16_t)crc[0] << 8U) | crc[1]))
        status = SDCARD_C_ERROR;
    deselect(driver);
    if (status != SDCARD_C_OK) return status;
    version = (uint8_t)(csd[0] >> 6U);
    if ((version == 1U && !driver->block_addressed) || (version == 0U && driver->block_addressed))
        return SDCARD_C_UNSUPPORTED;
    if (version == 1U) {
        uint32_t size = ((uint32_t)(csd[7] & 0x3fU) << 16U) | ((uint32_t)csd[8] << 8U) | csd[9];
        *sectors = ((uint64_t)size + 1U) * 1024U;
    } else if (version == 0U) {
        uint32_t read_len = csd[5] & 0x0fU;
        uint32_t size = ((uint32_t)(csd[6] & 3U) << 10U) | ((uint32_t)csd[7] << 2U) | (csd[8] >> 6U);
        uint32_t multiplier = ((uint32_t)(csd[9] & 3U) << 1U) | (csd[10] >> 7U);
        if (read_len < 9U || read_len > 11U) return SDCARD_C_UNSUPPORTED;
        *sectors = ((uint64_t)size + 1U) << (multiplier + 2U + read_len - 9U);
    } else {
        return SDCARD_C_UNSUPPORTED;
    }
    return SDCARD_C_OK;
}
/***************************** END OF FILE ************************************/
