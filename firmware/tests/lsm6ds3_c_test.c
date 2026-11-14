/** @file lsm6ds3_c_test.c
 * @author team-04
 * @brief Host tests for the pure C LSM6DS3 protocol driver.
 */
#include "lsm6ds3_c.h"
#include <assert.h>
#include <string.h>

typedef struct {
    uint8_t registers[256];
    uint8_t command;
    bool selected;
    uint32_t transfers;
    uint32_t fail_transfer;
} mock_bus_t;

static void mock_select(void *context)
{
    mock_bus_t *mock = (mock_bus_t *)context;
    assert(!mock->selected);
    mock->selected = true;
    mock->command = 0U;
}

static void mock_deselect(void *context)
{
    mock_bus_t *mock = (mock_bus_t *)context;
    assert(mock->selected);
    mock->selected = false;
}

static lsm6ds3_c_status_t mock_transfer(void *context, const uint8_t *tx, uint8_t *rx,
                                       uint16_t length, uint32_t timeout_ms)
{
    mock_bus_t *mock = (mock_bus_t *)context;
    assert(mock->selected);
    assert(timeout_ms == 20U);
    ++mock->transfers;
    if (mock->fail_transfer == mock->transfers) return LSM6DS3_C_TIMEOUT;
    if (tx != NULL && length == 1U) {
        mock->command = tx[0];
        return LSM6DS3_C_OK;
    }
    if (tx != NULL && length == 2U) {
        mock->registers[tx[0]] = tx[1];
        return LSM6DS3_C_OK;
    }
    if (rx != NULL) {
        const uint8_t address = (uint8_t)(mock->command & 0x7fU);
        memcpy(rx, mock->registers + address, length);
        return LSM6DS3_C_OK;
    }
    return LSM6DS3_C_IO_ERROR;
}

int main(void)
{
    mock_bus_t mock = {{0}, 0U, false, 0U, 0U};
    lsm6ds3_c_bus_t bus = {&mock, mock_select, mock_deselect, mock_transfer};
    lsm6ds3_c_t driver;
    int16_t x = 0, y = 0, z = 0;
    uint8_t fifo_tx[97];
    uint8_t fifo_rx[97] = {0};
    bool complete = false;
    lsm6ds3_c_fifo_status_t fifo_status;

    mock.registers[0x0f] = 0x69U;
    lsm6ds3_c_init(&driver, &bus);
    assert(lsm6ds3_c_configure(&driver) == LSM6DS3_C_OK);
    assert(driver.configured && driver.observed_id == 0x69U);
    assert(mock.registers[0x10] == 0xa4U && mock.registers[0x18] == 0x38U);

    mock.registers[0x1e] = 1U;
    mock.registers[0x28] = 0x00U; mock.registers[0x29] = 0x80U;
    mock.registers[0x2a] = 0xffU; mock.registers[0x2b] = 0x7fU;
    mock.registers[0x2c] = 0xffU; mock.registers[0x2d] = 0xffU;
    assert(lsm6ds3_c_read_raw(&driver, &x, &y, &z) == LSM6DS3_C_OK);
    assert(x == -32768 && y == 32767 && z == -1);
    mock.registers[0x1e] = 0U;
    assert(lsm6ds3_c_read_raw(&driver, &x, &y, &z) == LSM6DS3_C_NOT_READY);

    assert(lsm6ds3_c_reset_begin(&driver) == LSM6DS3_C_OK);
    assert(!driver.configured);
    mock.registers[0x12] = 0U;
    assert(lsm6ds3_c_reset_poll(&driver, &complete) == LSM6DS3_C_OK && complete);
    assert(lsm6ds3_c_configure(&driver) == LSM6DS3_C_OK);
    assert(lsm6ds3_c_configure_fifo(&driver, 16U) == LSM6DS3_C_OK);
    assert(mock.registers[0x06] == 96U && mock.registers[0x08] == 0x09U && mock.registers[0x0a] == 0x56U);
    assert(lsm6ds3_c_enable_fifo_interrupt(&driver, true) == LSM6DS3_C_OK);
    assert(mock.registers[0x0d] == 8U);

    mock.registers[0x3a] = 48U; mock.registers[0x3b] = 0x80U;
    mock.registers[0x3c] = 2U; mock.registers[0x3d] = 1U;
    assert(lsm6ds3_c_fifo_status(&driver, &fifo_status) == LSM6DS3_C_OK);
    assert(fifo_status.unread_words == 48U && fifo_status.next_pattern == 258U && fifo_status.watermark);
    assert(lsm6ds3_c_prepare_fifo_dma(fifo_tx, 16U, sizeof(fifo_tx)));
    assert(fifo_tx[0] == 0xbeU && fifo_tx[96] == 0xffU);
    fifo_rx[1] = 0x00U; fifo_rx[2] = 0x80U; fifo_rx[3] = 0xffU;
    fifo_rx[4] = 0x7fU; fifo_rx[5] = 0xffU; fifo_rx[6] = 0xffU;
    assert(lsm6ds3_c_decode_fifo_sample(fifo_rx, 16U, 0U, &x, &y, &z));
    assert(x == -32768 && y == 32767 && z == -1);
    assert(!lsm6ds3_c_prepare_fifo_dma(fifo_tx, 16U, 96U));
    assert(!lsm6ds3_c_decode_fifo_sample(fifo_rx, 16U, 16U, &x, &y, &z));

    mock.registers[0x0f] = 0U;
    assert(lsm6ds3_c_configure(&driver) == LSM6DS3_C_UNSUPPORTED);
    assert(!mock.selected);
    mock.fail_transfer = mock.transfers + 1U;
    assert(lsm6ds3_c_reset_begin(&driver) == LSM6DS3_C_TIMEOUT);
    assert(!mock.selected);
    assert(lsm6ds3_c_read_raw(&driver, NULL, &y, &z) == LSM6DS3_C_INVALID);
    lsm6ds3_c_init(&driver, NULL);
    assert(lsm6ds3_c_reset_poll(&driver, &complete) == LSM6DS3_C_INVALID);
    return 0;
}
/***************************** END OF FILE ************************************/
