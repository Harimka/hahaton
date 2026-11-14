/** @file sdcard_c.h
 * @author team-04
 * @brief Pure C SD SPI protocol context and operations.
 */
#ifndef TEAM04_SDCARD_C_H_
#define TEAM04_SDCARD_C_H_
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SDCARD_C_OK = 0,
    SDCARD_C_BUSY,
    SDCARD_C_TIMEOUT,
    SDCARD_C_ERROR,
    SDCARD_C_NOT_READY,
    SDCARD_C_INVALID,
    SDCARD_C_UNSUPPORTED
} sdcard_c_status_t;

typedef struct {
    void *context;
    sdcard_c_status_t (*set_clock)(void *context, uint32_t hz);
    void (*select)(void *context);
    void (*deselect)(void *context);
    sdcard_c_status_t (*transfer)(void *context, const uint8_t *tx, uint8_t *rx,
                                   uint16_t length, uint32_t timeout_ms);
    uint32_t (*millis)(void *context);
} sdcard_c_ops_t;

typedef struct {
    sdcard_c_ops_t ops;
    bool ready;
    bool block_addressed;
    uint8_t last_response;
    uint8_t last_data_response;
    uint32_t max_write_ms;
    uint32_t transfer_error_count;
} sdcard_c_t;

void sdcard_c_init(sdcard_c_t *driver, const sdcard_c_ops_t *ops);
sdcard_c_status_t sdcard_c_initialize(sdcard_c_t *driver);
sdcard_c_status_t sdcard_c_read(sdcard_c_t *driver, uint32_t lba, uint8_t *buffer);
sdcard_c_status_t sdcard_c_write(sdcard_c_t *driver, uint32_t lba, const uint8_t *buffer);
sdcard_c_status_t sdcard_c_sector_count(sdcard_c_t *driver, uint64_t *sectors);
#ifdef __cplusplus
}
#endif
#endif
/***************************** END OF FILE ************************************/
