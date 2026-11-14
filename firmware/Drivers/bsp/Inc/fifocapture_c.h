/** @file fifocapture_c.h
 * @author team-04
 * @brief Pure C FIFO/DMA acquisition state machine and fixed sample queue.
 */
#ifndef TEAM04_FIFOCAPTURE_C_H_
#define TEAM04_FIFOCAPTURE_C_H_
#include "lsm6ds3_c.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t watermark_irqs, dma_started, dma_completed, dma_errors, dma_timeouts;
    uint32_t busy_irqs, fifo_overruns, pattern_errors, dropped_samples;
    uint32_t published_samples, queue_high_water, max_fifo_words, max_service_us;
    uint32_t timer_wraps, sequence_wraps;
} fifocapture_c_stats_t;

#pragma pack(push, 1)
typedef struct {
    uint32_t timestamp_us;
    uint32_t sample_index;
    int16_t raw_x;
    int16_t raw_y;
    int16_t raw_z;
    uint8_t flags;
} fifocapture_c_sample_t;
#pragma pack(pop)

typedef struct {
    fifocapture_c_sample_t samples[16];
    uint16_t count;
    bool timestamps_estimated;
} fifocapture_c_block_t;

typedef struct {
    void *context;
    lsm6ds3_c_status_t (*start_dma)(void *context, const uint8_t *tx, uint8_t *rx, uint16_t len);
    lsm6ds3_c_status_t (*abort_dma)(void *context);
    uint32_t (*micros)(void *context);
    uint32_t (*critical_enter)(void *context);
    void (*critical_leave)(void *context, uint32_t key);
} fifocapture_c_ops_t;

typedef enum {
    FIFOCAPTURE_C_IDLE = 0,
    FIFOCAPTURE_C_STATUS,
    FIFOCAPTURE_C_DATA,
    FIFOCAPTURE_C_ABORTING
} fifocapture_c_state_t;

typedef struct {
    fifocapture_c_ops_t ops;
    fifocapture_c_block_t queue[128];
    fifocapture_c_stats_t stats;
    uint8_t status_tx[5], status_rx[5];
    uint8_t data_tx[97], data_rx[97];
    uint8_t head, tail, used;
    fifocapture_c_state_t state;
    uint16_t active_samples;
    uint8_t pending_flags;
    uint32_t next_sequence, irq_time_us, next_irq_us, dma_start_us, sample_time_us;
    uint16_t time_remainder;
    uint32_t last_time_us;
    bool pending_irq, faulted, time_seen;
} fifocapture_c_t;

void fifocapture_c_init(fifocapture_c_t *capture, const fifocapture_c_ops_t *ops);
void fifocapture_c_on_watermark(fifocapture_c_t *capture);
void fifocapture_c_kick(fifocapture_c_t *capture);
void fifocapture_c_on_dma_complete(fifocapture_c_t *capture, bool success);
void fifocapture_c_on_abort_complete(fifocapture_c_t *capture);
void fifocapture_c_watchdog(fifocapture_c_t *capture, uint32_t timeout_us);
bool fifocapture_c_can_recover(fifocapture_c_t *capture);
bool fifocapture_c_rearm(fifocapture_c_t *capture);
const fifocapture_c_block_t *fifocapture_c_peek(fifocapture_c_t *capture);
void fifocapture_c_release(fifocapture_c_t *capture);
void fifocapture_c_snapshot(fifocapture_c_t *capture, fifocapture_c_stats_t *stats);
#ifdef __cplusplus
}
#endif
#endif
/***************************** END OF FILE ************************************/
