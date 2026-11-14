/** @file fifocapture_c.c
 * @author team-04
 * @brief Pure C FIFO watermark and DMA acquisition state machine.
 */
#include "fifocapture_c.h"
#include <limits.h>
#include <stddef.h>
#include <string.h>

#define FIFO_BATCH_SAMPLES 16U
#define FIFO_DMA_BYTES 97U
#define FIFO_QUEUE_SLOTS 128U
#define FIFO_STATUS_COMMAND 0xbaU
#define FIFO_DMA_TIMEOUT_DEFAULT_US 5000U
#define FLAG_OVERRUN 1U
#define FLAG_SATURATION 2U
#define FLAG_SPI_ERROR 4U

_Static_assert(sizeof(fifocapture_c_sample_t) == 15U, "Sample contract must remain 15 bytes");

static bool valid_ops(const fifocapture_c_ops_t *ops)
{
    return ops != NULL && ops->start_dma != NULL && ops->abort_dma != NULL &&
        ops->micros != NULL && ops->critical_enter != NULL && ops->critical_leave != NULL;
}

static uint32_t enter_critical(fifocapture_c_t *capture)
{
    return capture->ops.critical_enter ? capture->ops.critical_enter(capture->ops.context) : 0U;
}
static void leave_critical(fifocapture_c_t *capture, uint32_t key)
{
    if (capture->ops.critical_leave) capture->ops.critical_leave(capture->ops.context, key);
}
static void note_time(fifocapture_c_t *capture, uint32_t now)
{
    if (capture->time_seen && now < capture->last_time_us) ++capture->stats.timer_wraps;
    capture->last_time_us = now;
    capture->time_seen = true;
}
static void start_time(fifocapture_c_t *capture, uint32_t irq_us)
{
    capture->irq_time_us = irq_us;
    capture->sample_time_us = irq_us - 2252U;
    capture->time_remainder = 0U;
}
static void advance_time(fifocapture_c_t *capture)
{
    capture->sample_time_us += 150U;
    capture->time_remainder = (uint16_t)(capture->time_remainder + 1000U);
    if (capture->time_remainder >= 6660U) {
        capture->time_remainder = (uint16_t)(capture->time_remainder - 6660U);
        ++capture->sample_time_us;
    }
}
static void fault(fifocapture_c_t *capture)
{
    capture->state = FIFOCAPTURE_C_IDLE;
    capture->faulted = true;
}
static bool begin_dma(fifocapture_c_t *capture, fifocapture_c_state_t state,
                      const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    lsm6ds3_c_status_t status;
    capture->state = state;
    capture->dma_start_us = capture->ops.micros(capture->ops.context);
    status = capture->ops.start_dma(capture->ops.context, tx, rx, len);
    if (status != LSM6DS3_C_OK) {
        ++capture->stats.dma_errors;
        capture->pending_flags |= FLAG_SPI_ERROR;
        capture->state = FIFOCAPTURE_C_ABORTING;
        if (capture->ops.abort_dma(capture->ops.context) != LSM6DS3_C_OK) ++capture->stats.dma_errors;
        return false;
    }
    ++capture->stats.dma_started;
    return true;
}
static void begin_status(fifocapture_c_t *capture)
{
    capture->pending_irq = false;
    (void)begin_dma(capture, FIFOCAPTURE_C_STATUS, capture->status_tx, capture->status_rx, 5U);
}

void fifocapture_c_init(fifocapture_c_t *capture, const fifocapture_c_ops_t *ops)
{
    if (capture == NULL) return;
    memset(capture, 0, sizeof(*capture));
    if (!valid_ops(ops)) {
        capture->faulted = true;
        return;
    }
    capture->ops = *ops;
    capture->state = FIFOCAPTURE_C_IDLE;
    capture->status_tx[0] = FIFO_STATUS_COMMAND;
    memset(capture->status_tx + 1, 0xff, 4U);
    (void)lsm6ds3_c_prepare_fifo_dma(capture->data_tx, FIFO_BATCH_SAMPLES, FIFO_DMA_BYTES);
}

void fifocapture_c_on_watermark(fifocapture_c_t *capture)
{
    uint32_t key;
    uint32_t now;
    if (capture == NULL || capture->faulted) return;
    key = enter_critical(capture);
    now = capture->ops.micros(capture->ops.context);
    ++capture->stats.watermark_irqs;
    note_time(capture, now);
    if (capture->state != FIFOCAPTURE_C_IDLE) {
        ++capture->stats.busy_irqs;
        capture->pending_irq = true;
        capture->next_irq_us = now;
    } else {
        start_time(capture, now);
        begin_status(capture);
    }
    leave_critical(capture, key);
}

void fifocapture_c_kick(fifocapture_c_t *capture)
{
    uint32_t key;
    if (capture == NULL || capture->faulted || capture->state != FIFOCAPTURE_C_IDLE) return;
    key = enter_critical(capture);
    if (!capture->faulted && capture->state == FIFOCAPTURE_C_IDLE) {
        start_time(capture, capture->ops.micros(capture->ops.context));
        begin_status(capture);
    }
    leave_critical(capture, key);
}

void fifocapture_c_on_dma_complete(fifocapture_c_t *capture, bool success)
{
    uint32_t key;
    if (capture == NULL) return;
    key = enter_critical(capture);
    if (capture->state == FIFOCAPTURE_C_IDLE || capture->state == FIFOCAPTURE_C_ABORTING || capture->faulted) {
        leave_critical(capture, key);
        return;
    }
    if (!success) {
        ++capture->stats.dma_errors;
        capture->pending_flags |= FLAG_SPI_ERROR;
        capture->state = FIFOCAPTURE_C_ABORTING;
        if (capture->ops.abort_dma(capture->ops.context) != LSM6DS3_C_OK) ++capture->stats.dma_errors;
        leave_critical(capture, key);
        return;
    }
    ++capture->stats.dma_completed;
    if (capture->state == FIFOCAPTURE_C_STATUS) {
        const uint8_t *b = capture->status_rx + 1U;
        const uint16_t words = (uint16_t)(b[0] | ((uint16_t)(b[1] & 0x0fU) << 8U));
        const uint16_t pattern = (uint16_t)(b[2] | ((uint16_t)(b[3] & 3U) << 8U));
        if (words > capture->stats.max_fifo_words) capture->stats.max_fifo_words = words;
        if ((b[1] & 0x60U) != 0U) {
            ++capture->stats.fifo_overruns;
            capture->pending_flags |= FLAG_OVERRUN;
            fault(capture);
        } else if (pattern != 0U) {
            ++capture->stats.pattern_errors;
            capture->pending_flags |= FLAG_SPI_ERROR;
            fault(capture);
        } else if (words >= FIFO_BATCH_SAMPLES * 3U) {
            capture->active_samples = FIFO_BATCH_SAMPLES;
            (void)begin_dma(capture, FIFOCAPTURE_C_DATA, capture->data_tx, capture->data_rx, FIFO_DMA_BYTES);
        } else {
            capture->state = FIFOCAPTURE_C_IDLE;
            if (capture->pending_irq) {
                start_time(capture, capture->next_irq_us);
                begin_status(capture);
            }
        }
    } else {
        const uint32_t observed = capture->ops.micros(capture->ops.context);
        const uint32_t latency = observed - capture->irq_time_us;
        uint16_t i;
        if (latency > capture->stats.max_service_us) capture->stats.max_service_us = latency;
        if (capture->used == FIFO_QUEUE_SLOTS) {
            capture->stats.dropped_samples += capture->active_samples;
            capture->pending_flags |= FLAG_OVERRUN;
            for (i = 0U; i < capture->active_samples; ++i) advance_time(capture);
        } else {
            fifocapture_c_block_t *block = &capture->queue[capture->head];
            block->count = capture->active_samples;
            block->timestamps_estimated = true;
            for (i = 0U; i < capture->active_samples; ++i) {
                fifocapture_c_sample_t *sample = &block->samples[i];
                int16_t x = 0, y = 0, z = 0;
                (void)lsm6ds3_c_decode_fifo_sample(capture->data_rx, capture->active_samples, i, &x, &y, &z);
                sample->timestamp_us = capture->sample_time_us;
                advance_time(capture);
                sample->sample_index = capture->next_sequence + i;
                sample->raw_x = x; sample->raw_y = y; sample->raw_z = z;
                sample->flags = (i == 0U) ? capture->pending_flags : 0U;
                if (x == INT16_MIN || x == INT16_MAX || y == INT16_MIN || y == INT16_MAX || z == INT16_MIN || z == INT16_MAX)
                    sample->flags |= FLAG_SATURATION;
            }
            capture->pending_flags = 0U;
            capture->head = (uint8_t)((capture->head + 1U) % FIFO_QUEUE_SLOTS);
            ++capture->used;
            capture->stats.published_samples += capture->active_samples;
            if (capture->used > capture->stats.queue_high_water) capture->stats.queue_high_water = capture->used;
        }
        {
            const uint32_t next = capture->next_sequence + capture->active_samples;
            if (next < capture->next_sequence) ++capture->stats.sequence_wraps;
            capture->next_sequence = next;
        }
        begin_status(capture);
    }
    leave_critical(capture, key);
}

void fifocapture_c_watchdog(fifocapture_c_t *capture, uint32_t timeout_us)
{
    uint32_t key;
    uint32_t now;
    if (capture == NULL || timeout_us == 0U || capture->ops.micros == NULL) return;
    key = enter_critical(capture);
    now = capture->ops.micros(capture->ops.context);
    note_time(capture, now);
    if ((capture->state == FIFOCAPTURE_C_STATUS || capture->state == FIFOCAPTURE_C_DATA) &&
        (uint32_t)(now - capture->dma_start_us) >= timeout_us) {
        capture->state = FIFOCAPTURE_C_ABORTING;
        ++capture->stats.dma_timeouts;
        capture->pending_flags |= FLAG_SPI_ERROR;
        if (capture->ops.abort_dma(capture->ops.context) != LSM6DS3_C_OK) ++capture->stats.dma_errors;
    }
    leave_critical(capture, key);
}

void fifocapture_c_on_abort_complete(fifocapture_c_t *capture)
{
    uint32_t key;
    if (capture == NULL) return;
    key = enter_critical(capture);
    if (capture->state == FIFOCAPTURE_C_ABORTING) fault(capture);
    leave_critical(capture, key);
}

bool fifocapture_c_can_recover(fifocapture_c_t *capture)
{
    bool result;
    uint32_t key;
    if (capture == NULL) return false;
    key = enter_critical(capture);
    result = valid_ops(&capture->ops) && capture->faulted && capture->state == FIFOCAPTURE_C_IDLE;
    leave_critical(capture, key);
    return result;
}

bool fifocapture_c_rearm(fifocapture_c_t *capture)
{
    bool result;
    uint32_t key;
    if (capture == NULL) return false;
    key = enter_critical(capture);
    result = valid_ops(&capture->ops) && capture->faulted && capture->state == FIFOCAPTURE_C_IDLE;
    if (result) { capture->faulted = false; capture->pending_irq = false; }
    leave_critical(capture, key);
    return result;
}

const fifocapture_c_block_t *fifocapture_c_peek(fifocapture_c_t *capture)
{
    const fifocapture_c_block_t *result;
    uint32_t key;
    if (capture == NULL) return NULL;
    key = enter_critical(capture);
    result = capture->used ? &capture->queue[capture->tail] : NULL;
    leave_critical(capture, key);
    return result;
}

void fifocapture_c_release(fifocapture_c_t *capture)
{
    uint32_t key;
    if (capture == NULL) return;
    key = enter_critical(capture);
    if (capture->used) { capture->tail = (uint8_t)((capture->tail + 1U) % FIFO_QUEUE_SLOTS); --capture->used; }
    leave_critical(capture, key);
}

void fifocapture_c_snapshot(fifocapture_c_t *capture, fifocapture_c_stats_t *stats)
{
    uint32_t key;
    if (capture == NULL || stats == NULL) return;
    key = enter_critical(capture);
    *stats = capture->stats;
    leave_critical(capture, key);
}
/***************************** END OF FILE ************************************/
