/** @file fifocapture.cpp
 * @author team-04
 * @brief C++ facade over the pure C FIFO/DMA acquisition state machine.
 */
#include "fifocapture.h"
#include "fifocapture_c.h"
#include <type_traits>
namespace team04 {
static_assert(sizeof(CaptureStats) == sizeof(fifocapture_c_stats_t), "C/C++ stats ABI mismatch");
namespace {
lsm6ds3_c_status_t toCStatus(IoStatus status) {
    switch (status) {
    case IoStatus::kOk: return LSM6DS3_C_OK;
    case IoStatus::kInvalid: return LSM6DS3_C_INVALID;
    case IoStatus::kNotReady: return LSM6DS3_C_NOT_READY;
    case IoStatus::kUnsupported: return LSM6DS3_C_UNSUPPORTED;
    case IoStatus::kBusy: return LSM6DS3_C_BUSY;
    case IoStatus::kTimeout: return LSM6DS3_C_TIMEOUT;
    default: return LSM6DS3_C_IO_ERROR;
    }
}
}
FifoCapture::FifoCapture(SpiDrv& bus, ClockDrv& clock, CriticalSection& critical)
    : bus_(bus), clock_(clock), critical_(critical), core_{}, peekCache_{}
{
    const fifocapture_c_ops_t ops = {
        this,
        &FifoCapture::startDma,
        &FifoCapture::abortDma,
        &FifoCapture::micros,
        &FifoCapture::criticalEnter,
        &FifoCapture::criticalLeave
    };
    fifocapture_c_init(&core_, &ops);
}
lsm6ds3_c_status_t FifoCapture::startDma(void* context, const uint8_t* tx, uint8_t* rx, uint16_t len) {
    FifoCapture* self = static_cast<FifoCapture*>(context);
    self->bus_.select();
    /* Keep CS/buffers owned until the port confirms abort quiescence, even
     * when startDma fails: a failed HAL start can leave partial DMA setup. */
    return toCStatus(self->bus_.startDma(tx, rx, len));
}
lsm6ds3_c_status_t FifoCapture::abortDma(void* context) {
    return toCStatus(static_cast<FifoCapture*>(context)->bus_.abortDma());
}
uint32_t FifoCapture::micros(void* context) {
    return static_cast<FifoCapture*>(context)->clock_.micros();
}
uint32_t FifoCapture::criticalEnter(void* context) {
    return static_cast<FifoCapture*>(context)->critical_.enter();
}
void FifoCapture::criticalLeave(void* context, uint32_t key) {
    static_cast<FifoCapture*>(context)->critical_.leave(key);
}
void FifoCapture::onWatermark() { fifocapture_c_on_watermark(&core_); }
void FifoCapture::kick() { fifocapture_c_kick(&core_); }
void FifoCapture::onDmaComplete(bool success) {
    fifocapture_c_on_dma_complete(&core_, success);
}
void FifoCapture::onAbortComplete() {
    fifocapture_c_on_abort_complete(&core_);
}
void FifoCapture::watchdog(uint32_t timeoutUs) { fifocapture_c_watchdog(&core_, timeoutUs); }
bool FifoCapture::canRecover() { return fifocapture_c_can_recover(&core_); }
bool FifoCapture::rearm() { return fifocapture_c_rearm(&core_); }
const CaptureBlock* FifoCapture::peek() {
    const fifocapture_c_block_t* block = fifocapture_c_peek(&core_);
    if (block == nullptr) return nullptr;
    peekCache_.count = block->count;
    peekCache_.timestampsEstimated = block->timestamps_estimated;
    for (uint16_t i = 0U; i < block->count; ++i) {
        const auto& source = block->samples[i];
        auto& target = peekCache_.samples[i];
        target.timestampUs = source.timestamp_us;
        target.sampleIndex = source.sample_index;
        target.rawX = source.raw_x;
        target.rawY = source.raw_y;
        target.rawZ = source.raw_z;
        target.flags = source.flags;
    }
    return &peekCache_;
}
void FifoCapture::release() { fifocapture_c_release(&core_); }
CaptureStats FifoCapture::snapshot() {
    fifocapture_c_stats_t stats{};
    fifocapture_c_snapshot(&core_, &stats);
    return CaptureStats{
        stats.watermark_irqs, stats.dma_started, stats.dma_completed, stats.dma_errors, stats.dma_timeouts,
        stats.busy_irqs, stats.fifo_overruns, stats.pattern_errors, stats.dropped_samples,
        stats.published_samples, stats.queue_high_water, stats.max_fifo_words, stats.max_service_us,
        stats.timer_wraps, stats.sequence_wraps
    };
}
}
/***************************** END OF FILE ************************************/
