/** @file fifocapture.h
 * @author team-04
 * @brief FIFO watermark acquisition and statically allocated sample queue.
 */
#ifndef TEAM04_FIFOCAPTURE_H_
#define TEAM04_FIFOCAPTURE_H_
#include "lsm6ds3.h"
namespace team04 {
class CriticalSection {
public:
    CriticalSection() = default;
    CriticalSection(const CriticalSection&) = delete;
    CriticalSection& operator=(const CriticalSection&) = delete;
    virtual uint32_t enter() = 0;
    virtual void leave(uint32_t state) = 0;
protected:
    ~CriticalSection() = default;
};
struct CaptureStats {
    uint32_t watermarkIrqs, dmaStarted, dmaCompleted, dmaErrors, dmaTimeouts;
    uint32_t busyIrqs, fifoOverruns, patternErrors, droppedSamples;
    uint32_t publishedSamples, queueHighWater, maxFifoWords, maxServiceUs;
    uint32_t timerWraps, sequenceWraps;
};
struct CaptureBlock {
    ImuRawSample samples[16];
    uint16_t count;
    /* True: timestamps are reconstructed from nominal ODR and IRQ time. */
    bool timestampsEstimated;
};
class FifoCapture final {
public:
    static constexpr uint16_t kBatchSamples = 16U;
    static constexpr uint16_t kDmaBytes = 1U + 6U * kBatchSamples;
    static constexpr uint8_t kSlots = 128U;
    FifoCapture(SpiDrv& bus, ClockDrv& clock, CriticalSection& critical);
    FifoCapture(const FifoCapture&) = delete;
    FifoCapture& operator=(const FifoCapture&) = delete;
    /* Configure the sensor for kBatchSamples watermark before enabling EXTI. */
    void onWatermark();
    /* Request one status check without counting a physical interrupt. */
    void kick();
    /* Port calls complete after SPI idle and CS high. startDma/abortDma must
     * not invoke these callbacks synchronously. */
    void onDmaComplete(bool success);
    void onAbortComplete();
    void watchdog(uint32_t timeoutUs);
    bool canRecover();
    bool rearm();
    const CaptureBlock* peek();
    void release();
    CaptureStats snapshot();
private:
    enum class State : uint8_t { kIdle, kStatus, kData, kAborting };
    bool begin(State state, const uint8_t* tx, uint8_t* rx, uint16_t size);
    void beginStatus();
    void fault();
    void noteTime(uint32_t nowUs);
    void startTime(uint32_t irqUs);
    void advanceTime();
    SpiDrv& bus_;
    ClockDrv& clock_;
    CriticalSection& critical_;
    CaptureBlock queue_[kSlots];
    CaptureStats stats_;
    uint8_t statusTx_[5], statusRx_[5];
    uint8_t dataTx_[kDmaBytes], dataRx_[kDmaBytes];
    uint8_t head_, tail_, used_;
    State state_;
    uint16_t activeSamples_;
    uint8_t pendingFlags_;
    uint32_t nextSequence_, irqTimeUs_, nextIrqUs_, dmaStartUs_, sampleTimeUs_;
    uint16_t timeRemainder_;
    uint32_t lastTimeUs_;
    bool pendingIrq_, faulted_, timeSeen_;
};
}
#endif
/***************************** END OF FILE ************************************/
