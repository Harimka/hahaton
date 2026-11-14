/** @file fifocapture_test.cpp
 * @author team-04
 * @brief Host-side tests for the accelerometer and FIFO DMA state machine.
 */
#include "fifocapture.h"
#include <cassert>
#include <cstring>
using namespace team04;
namespace {
class MockClock final : public ClockDrv {
public:
    uint32_t us = 10000U;
    uint32_t millis() const override { return us / 1000U; }
    uint32_t micros() const override { return us; }
};
class MockCritical final : public CriticalSection {
public:
    uint32_t enter() override { return 0U; }
    void leave(uint32_t) override {}
};
class MockSpi final : public SpiDrv {
public:
    uint8_t regs[256]{};
    uint8_t* dmaRx = nullptr;
    uint16_t dmaLen = 0U;
    uint8_t command = 0U;
    bool selected = false;
    bool failStart = false;
    uint32_t abortCount = 0U;
    IoStatus setClock(uint32_t) override { return IoStatus::kOk; }
    void select() override { assert(!selected); selected = true; command = 0U; }
    void deselect() override { assert(selected); selected = false; }
    IoStatus transfer(const uint8_t* tx, uint8_t* rx, uint16_t len, uint32_t) override {
        assert(selected);
        if (tx && len == 1U) { command = tx[0]; return IoStatus::kOk; }
        if (tx && len == 2U) { regs[tx[0]] = tx[1]; return IoStatus::kOk; }
        if (rx && command) { std::memcpy(rx, regs + (command & 0x7fU), len); return IoStatus::kOk; }
        return IoStatus::kError;
    }
    IoStatus startDma(const uint8_t* tx, uint8_t* rx, uint16_t len) override {
        assert(selected && tx && (tx[0] == 0xbaU || tx[0] == 0xbeU));
        if (failStart) return IoStatus::kError;
        dmaRx = rx; dmaLen = len; return IoStatus::kOk;
    }
    IoStatus abortDma() override { ++abortCount; return IoStatus::kOk; }
};
void status(FifoCapture& capture, MockSpi& bus, uint16_t words, uint8_t flags = 0U, uint16_t pattern = 0U) {
    assert(bus.dmaLen == 5U);
    bus.dmaRx[0] = 0U; bus.dmaRx[1] = static_cast<uint8_t>(words);
    bus.dmaRx[2] = static_cast<uint8_t>((words >> 8U) | flags);
    bus.dmaRx[3] = static_cast<uint8_t>(pattern); bus.dmaRx[4] = static_cast<uint8_t>(pattern >> 8U);
    bus.deselect(); capture.onDmaComplete(true);
}
void data(FifoCapture& capture, MockSpi& bus, MockClock& clock) {
    assert(bus.dmaLen == FifoCapture::kDmaBytes);
    std::memset(bus.dmaRx, 0, bus.dmaLen);
    bus.dmaRx[1] = 0x00; bus.dmaRx[2] = 0x80; bus.dmaRx[3] = 0xff;
    bus.dmaRx[4] = 0x7f; bus.dmaRx[5] = 0xff; bus.dmaRx[6] = 0xff;
    clock.us += 100U; bus.deselect(); capture.onDmaComplete(true);
}
}
int main() {
    MockSpi spi; MockClock clock; MockCritical critical;
    spi.regs[0x0f] = Lsm6ds3::kWhoAmI;
    Lsm6ds3 sensor(spi); assert(sensor.configure() == IoStatus::kOk);
    assert(spi.regs[0x13] == 0U && spi.regs[0x10] == 0xa4U);
    assert(sensor.configureFifo(FifoCapture::kBatchSamples) == IoStatus::kOk);
    assert(spi.regs[0x06] == 48U && spi.regs[0x08] == 1U && spi.regs[0x0a] == 0x56U);
    assert(sensor.enableFifoInterrupt(true) == IoStatus::kOk && spi.regs[0x0d] == 8U);
    static FifoCapture capture(spi, clock, critical);
    capture.onWatermark(); status(capture, spi, 48); data(capture, spi, clock); status(capture, spi, 0);
    const CaptureBlock* block = capture.peek(); assert(block && block->count == 16U && block->timestampsEstimated);
    assert(block->samples[0].rawX == -32768 && block->samples[0].rawY == 32767 && block->samples[0].rawZ == -1);
    assert((block->samples[0].flags & static_cast<uint8_t>(ImuFlag::kSaturation)) != 0U);
    assert(block->samples[1].sampleIndex == block->samples[0].sampleIndex + 1U);
    assert(block->samples[1].timestampUs - block->samples[0].timestampUs == 150U);
    capture.release(); assert(!capture.peek());
    capture.onWatermark(); status(capture, spi, 48, 0x40); assert(capture.canRecover()); assert(capture.rearm());
    capture.onWatermark(); status(capture, spi, 48); data(capture, spi, clock); status(capture, spi, 0);
    block = capture.peek(); assert(block && (block->samples[0].flags & static_cast<uint8_t>(ImuFlag::kOverrun)));
    capture.release();
    capture.onWatermark(); clock.us += 6000U; capture.watchdog(5000U);
    assert(spi.abortCount == 1U && !capture.canRecover());
    spi.deselect(); capture.onAbortComplete(); assert(capture.canRecover() && capture.rearm());
    for (unsigned i = 0; i < FifoCapture::kSlots + 1U; ++i) {
        capture.onWatermark(); status(capture, spi, 48); data(capture, spi, clock); status(capture, spi, 0);
    }
    CaptureStats stats = capture.snapshot();
    assert(stats.fifoOverruns == 1U && stats.dmaTimeouts == 1U && stats.droppedSamples == 16U);
    assert(stats.queueHighWater == FifoCapture::kSlots);
    for (unsigned i = 0; i < FifoCapture::kSlots; ++i) capture.release();
    capture.onWatermark(); status(capture, spi, 48, 0, 1U);
    assert(capture.canRecover() && capture.snapshot().patternErrors == 1U);
    clock.us = 0xfffffff0U; capture.watchdog(5000U);
    clock.us = 100U; capture.watchdog(5000U);
    assert(capture.snapshot().timerWraps == 1U);
    assert(capture.rearm());
    spi.failStart = true;
    capture.onWatermark();
    assert(spi.selected && !capture.canRecover());
    spi.deselect(); capture.onAbortComplete();
    assert(capture.canRecover());
}
/***************************** END OF FILE ************************************/
