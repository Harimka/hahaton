/** @file fifocapture.cpp
 * @author team-04
 * @brief Watermark-driven FIFO DMA state machine with bounded queue.
 */
#include "fifocapture.h"
#include <stdint.h>
namespace team04 {
FifoCapture::FifoCapture(SpiDrv& bus, ClockDrv& clock, CriticalSection& critical)
    : bus_(bus)
    , clock_(clock)
    , critical_(critical)
    , queue_{}
    , stats_{}
    , statusTx_{static_cast<uint8_t>(Lsm6ds3::kFifoStatus1 | 0x80U),0xffU,0xffU,0xffU,0xffU}
    , statusRx_{}
    , dataTx_{}
    , dataRx_{}
    , head_(0U)
    , tail_(0U)
    , used_(0U)
    , state_(State::kIdle)
    , activeSamples_(0U)
    , pendingFlags_(0U)
    , nextSequence_(0U)
    , irqTimeUs_(0U)
    , nextIrqUs_(0U)
    , dmaStartUs_(0U)
    , sampleTimeUs_(0U)
    , timeRemainder_(0U)
    , lastTimeUs_(0U)
    , pendingIrq_(false)
    , faulted_(false)
    , timeSeen_(false)
{
    (void)Lsm6ds3::prepareFifoDma(dataTx_, kBatchSamples, kDmaBytes);
}
void FifoCapture::fault() { state_ = State::kIdle; faulted_ = true; }
void FifoCapture::noteTime(uint32_t nowUs) {
    if (timeSeen_ && nowUs < lastTimeUs_) ++stats_.timerWraps;
    lastTimeUs_ = nowUs; timeSeen_ = true;
}
void FifoCapture::startTime(uint32_t irqUs) {
    irqTimeUs_ = irqUs; sampleTimeUs_ = irqUs - 2252U; timeRemainder_ = 0U;
}
void FifoCapture::advanceTime() {
    sampleTimeUs_ += 150U; timeRemainder_ = static_cast<uint16_t>(timeRemainder_ + 1000U);
    if (timeRemainder_ >= 6660U) { timeRemainder_ -= 6660U; ++sampleTimeUs_; }
}
bool FifoCapture::begin(State state, const uint8_t* tx, uint8_t* rx, uint16_t size) {
    state_ = state; dmaStartUs_ = clock_.micros(); bus_.select();
    if (bus_.startDma(tx, rx, size) != IoStatus::kOk) {
        ++stats_.dmaErrors; pendingFlags_ |= static_cast<uint8_t>(ImuFlag::kSpiError);
        state_ = State::kAborting;
        if (bus_.abortDma() != IoStatus::kOk) ++stats_.dmaErrors;
        return false;
    }
    ++stats_.dmaStarted; return true;
}
void FifoCapture::beginStatus() {
    pendingIrq_ = false;
    (void)begin(State::kStatus, statusTx_, statusRx_, 5U);
}
void FifoCapture::onWatermark() {
    const uint32_t key = critical_.enter(); ++stats_.watermarkIrqs; noteTime(clock_.micros());
    if (!faulted_) {
        if (state_ != State::kIdle) { ++stats_.busyIrqs; pendingIrq_ = true; nextIrqUs_ = clock_.micros(); }
        else { startTime(clock_.micros()); beginStatus(); }
    }
    critical_.leave(key);
}
void FifoCapture::kick() {
    const uint32_t key = critical_.enter();
    if (!faulted_ && state_ == State::kIdle) {
        startTime(clock_.micros()); beginStatus();
    }
    critical_.leave(key);
}
void FifoCapture::onDmaComplete(bool success) {
    const uint32_t key = critical_.enter();
    if (state_ == State::kIdle || state_ == State::kAborting || faulted_) { critical_.leave(key); return; }
    if (!success) {
        ++stats_.dmaErrors; pendingFlags_ |= static_cast<uint8_t>(ImuFlag::kSpiError);
        state_ = State::kAborting;
        if (bus_.abortDma() != IoStatus::kOk) ++stats_.dmaErrors;
        critical_.leave(key); return;
    }
    ++stats_.dmaCompleted;
    if (state_ == State::kStatus) {
        const uint8_t* b = statusRx_ + 1U;
        const uint16_t words = static_cast<uint16_t>(b[0] | (static_cast<uint16_t>(b[1] & 0x0fU) << 8U));
        if (words > stats_.maxFifoWords) stats_.maxFifoWords = words;
        const uint16_t pattern = static_cast<uint16_t>(b[2] | (static_cast<uint16_t>(b[3] & 3U) << 8U));
        if (b[1] & 0x60U) {
            ++stats_.fifoOverruns; pendingFlags_ |= static_cast<uint8_t>(ImuFlag::kOverrun); fault();
        } else if (pattern != 0U) {
            ++stats_.patternErrors; pendingFlags_ |= static_cast<uint8_t>(ImuFlag::kSpiError); fault();
        } else if (words >= kBatchSamples * 3U) {
            activeSamples_ = kBatchSamples;
            (void)begin(State::kData, dataTx_, dataRx_, kDmaBytes);
        } else {
            state_ = State::kIdle;
            if (pendingIrq_) { startTime(nextIrqUs_); beginStatus(); }
        }
    } else {
        const uint32_t observed = clock_.micros();
        const uint32_t latency = observed - irqTimeUs_;
        if (latency > stats_.maxServiceUs) stats_.maxServiceUs = latency;
        if (used_ == kSlots) {
            stats_.droppedSamples += activeSamples_;
            for (uint16_t i = 0; i < activeSamples_; ++i) advanceTime();
        } else {
            CaptureBlock& block = queue_[head_]; block.count = activeSamples_; block.timestampsEstimated = true;
            for (uint16_t i = 0; i < activeSamples_; ++i) {
                ImuRawSample& v = block.samples[i];
                int16_t x, y, z;
                (void)Lsm6ds3::decodeFifoSample(dataRx_, activeSamples_, i, x, y, z);
                v.timestampUs = sampleTimeUs_; advanceTime();
                v.sampleIndex = nextSequence_ + i; v.rawX = x; v.rawY = y; v.rawZ = z;
                v.flags = i == 0U ? pendingFlags_ : 0U;
                if (x == INT16_MIN || x == INT16_MAX || y == INT16_MIN || y == INT16_MAX || z == INT16_MIN || z == INT16_MAX)
                    v.flags |= static_cast<uint8_t>(ImuFlag::kSaturation);
            }
            pendingFlags_ = 0U; head_ = static_cast<uint8_t>((head_ + 1U) % kSlots);
            ++used_; stats_.publishedSamples += activeSamples_;
            if (used_ > stats_.queueHighWater) stats_.queueHighWater = used_;
        }
        const uint32_t next = nextSequence_ + activeSamples_;
        if (next < nextSequence_) ++stats_.sequenceWraps;
        nextSequence_ = next;
        beginStatus();
    }
    critical_.leave(key);
}
void FifoCapture::watchdog(uint32_t timeoutUs) {
    if (!timeoutUs) return;
    const uint32_t key = critical_.enter();
    noteTime(clock_.micros());
    if ((state_ == State::kStatus || state_ == State::kData) && !faulted_ &&
        static_cast<uint32_t>(clock_.micros() - dmaStartUs_) >= timeoutUs) {
        state_ = State::kAborting; ++stats_.dmaTimeouts;
        pendingFlags_ |= static_cast<uint8_t>(ImuFlag::kSpiError);
        if (bus_.abortDma() != IoStatus::kOk) ++stats_.dmaErrors;
    }
    critical_.leave(key);
}
void FifoCapture::onAbortComplete() {
    const uint32_t key = critical_.enter(); if (state_ == State::kAborting) fault(); critical_.leave(key);
}
bool FifoCapture::canRecover() {
    const uint32_t key = critical_.enter(); const bool ok = faulted_ && state_ == State::kIdle; critical_.leave(key); return ok;
}
bool FifoCapture::rearm() {
    const uint32_t key = critical_.enter();
    const bool ok = faulted_ && state_ == State::kIdle;
    if (ok) { faulted_ = false; pendingIrq_ = false; }
    critical_.leave(key); return ok;
}
const CaptureBlock* FifoCapture::peek() {
    const uint32_t key = critical_.enter(); const CaptureBlock* out = used_ ? &queue_[tail_] : nullptr;
    critical_.leave(key); return out;
}
void FifoCapture::release() {
    const uint32_t key = critical_.enter();
    if (used_) { tail_ = static_cast<uint8_t>((tail_ + 1U) % kSlots); --used_; }
    critical_.leave(key);
}
CaptureStats FifoCapture::snapshot() {
    const uint32_t key = critical_.enter(); const CaptureStats copy = stats_; critical_.leave(key); return copy;
}
}
/***************************** END OF FILE ************************************/
