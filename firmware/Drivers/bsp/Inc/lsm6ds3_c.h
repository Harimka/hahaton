/** @file lsm6ds3_c.h
 * @author team-04
 * @brief Pure C LSM6DS3 register and FIFO protocol API.
 */
#ifndef TEAM04_LSM6DS3_C_H_
#define TEAM04_LSM6DS3_C_H_
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LSM6DS3_C_OK = 0,
    LSM6DS3_C_INVALID,
    LSM6DS3_C_IO_ERROR,
    LSM6DS3_C_NOT_READY,
    LSM6DS3_C_UNSUPPORTED,
    LSM6DS3_C_BUSY,
    LSM6DS3_C_TIMEOUT
} lsm6ds3_c_status_t;

typedef struct {
    void *context;
    void (*select)(void *context);
    void (*deselect)(void *context);
    lsm6ds3_c_status_t (*transfer)(void *context, const uint8_t *tx, uint8_t *rx,
                                   uint16_t length, uint32_t timeout_ms);
} lsm6ds3_c_bus_t;

typedef struct {
    lsm6ds3_c_bus_t bus;
    uint8_t observed_id;
    bool configured;
} lsm6ds3_c_t;

typedef struct {
    uint16_t unread_words;
    uint16_t next_pattern;
    bool empty;
    bool full;
    bool overrun;
    bool watermark;
} lsm6ds3_c_fifo_status_t;

void lsm6ds3_c_init(lsm6ds3_c_t *driver, const lsm6ds3_c_bus_t *bus);
lsm6ds3_c_status_t lsm6ds3_c_reset_begin(lsm6ds3_c_t *driver);
lsm6ds3_c_status_t lsm6ds3_c_reset_poll(lsm6ds3_c_t *driver, bool *complete);
lsm6ds3_c_status_t lsm6ds3_c_configure(lsm6ds3_c_t *driver);
lsm6ds3_c_status_t lsm6ds3_c_read_raw(lsm6ds3_c_t *driver, int16_t *x, int16_t *y, int16_t *z);
lsm6ds3_c_status_t lsm6ds3_c_configure_fifo(lsm6ds3_c_t *driver, uint16_t watermark_samples);
lsm6ds3_c_status_t lsm6ds3_c_enable_fifo_interrupt(lsm6ds3_c_t *driver, bool enabled);
lsm6ds3_c_status_t lsm6ds3_c_fifo_status(lsm6ds3_c_t *driver, lsm6ds3_c_fifo_status_t *status);
bool lsm6ds3_c_prepare_fifo_dma(uint8_t *tx, uint16_t samples, uint16_t capacity);
bool lsm6ds3_c_decode_fifo_sample(const uint8_t *rx, uint16_t samples, uint16_t index,
                                  int16_t *x, int16_t *y, int16_t *z);
#ifdef __cplusplus
}
#endif
#endif
/***************************** END OF FILE ************************************/
