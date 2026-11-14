/** @file fifocapture_c_test.c
 * @author team-04
 * @brief Direct C ABI, invalid-ops and DMA recovery regression tests.
 */
#include "fifocapture_c.h"
#include <assert.h>
#include <string.h>

typedef struct {
    uint32_t now, aborts, locks;
    uint8_t *rx;
    uint16_t length;
} mock_t;
static lsm6ds3_c_status_t start(void *ctx, const uint8_t *tx, uint8_t *rx, uint16_t len) {
    mock_t *m = ctx;
    assert(tx && rx && m->locks == 1U);
    m->rx = rx; m->length = len;
    return LSM6DS3_C_OK;
}
static lsm6ds3_c_status_t abort_dma(void *ctx) { ++((mock_t *)ctx)->aborts; return LSM6DS3_C_OK; }
static uint32_t micros(void *ctx) { return ((mock_t *)ctx)->now; }
static uint32_t lock(void *ctx) { mock_t *m = ctx; assert(m->locks == 0U); ++m->locks; return 42U; }
static void unlock(void *ctx, uint32_t key) { mock_t *m = ctx; assert(key == 42U && m->locks == 1U); --m->locks; }
int main(void) {
    static fifocapture_c_t c;
    mock_t m = {10000U, 0U, 0U, NULL, 0U};
    fifocapture_c_ops_t ops = {&m, start, abort_dma, micros, lock, unlock};
    const fifocapture_c_block_t *block;
    fifocapture_c_init(&c, NULL);
    assert(!fifocapture_c_rearm(&c));
    fifocapture_c_on_watermark(&c);
    fifocapture_c_watchdog(&c, 1U);
    fifocapture_c_init(&c, &ops);
    fifocapture_c_on_watermark(&c);
    assert(m.length == 5U);
    memset(m.rx, 0, m.length); m.rx[1] = 48U;
    fifocapture_c_on_dma_complete(&c, true);
    assert(m.length == 97U);
    memset(m.rx, 0, m.length); m.rx[1] = 0xffU; m.rx[2] = 0xffU;
    fifocapture_c_on_dma_complete(&c, true);
    block = fifocapture_c_peek(&c);
    assert(block && block->count == 16U && block->samples[0].raw_x == -1);
    assert(block->samples[1].sample_index == 1U);
    fifocapture_c_release(&c);
    assert(!fifocapture_c_peek(&c));
    m.now += 6000U;
    fifocapture_c_watchdog(&c, 5000U);
    assert(m.aborts == 1U && !fifocapture_c_can_recover(&c));
    fifocapture_c_on_abort_complete(&c);
    assert(fifocapture_c_can_recover(&c) && fifocapture_c_rearm(&c));
    assert(m.locks == 0U);
    return 0;
}
/***************************** END OF FILE ************************************/
