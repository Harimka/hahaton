/** @file fifocapture.h
 * @author team-04
 * @brief FIFO watermark acquisition and statically allocated sample queue.
 */
#ifndef TEAM04_FIFOCAPTURE_H_
#define TEAM04_FIFOCAPTURE_H_
#include "lsm6ds3.h"
#include "fifocapture_c.h"
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
    static lsm6ds3_c_status_t startDma(void* context, const uint8_t* tx, uint8_t* rx, uint16_t len);
    static lsm6ds3_c_status_t abortDma(void* context);
    static uint32_t micros(void* context);
    static uint32_t criticalEnter(void* context);
    static void criticalLeave(void* context, uint32_t key);
    SpiDrv& bus_;
    ClockDrv& clock_;
    CriticalSection& critical_;
    fifocapture_c_t core_;
    CaptureBlock peekCache_{};
};
}
#endif
/***************************** END OF FILE ************************************/
