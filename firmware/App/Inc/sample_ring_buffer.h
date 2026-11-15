/**
 * @file    sample_ring_buffer.h
 * @author  team-04
 * @brief   Static ring buffer for high-rate accelerometer capture.
 */

#ifndef TEAM04_SAMPLE_RING_BUFFER_H_
#define TEAM04_SAMPLE_RING_BUFFER_H_

#include <stdint.h>

namespace team04 {

struct CaptureSample {
    uint32_t timestampUs;
    uint32_t sampleIndex;
    int16_t accelX;
    int16_t accelY;
    int16_t accelZ;
    uint8_t flags;
};

class SampleRingBuffer final {
public:
    static constexpr uint16_t kCapacity = 2048U;
    static constexpr uint16_t kMask = kCapacity - 1U;

    SampleRingBuffer();
    ~SampleRingBuffer() = default;

    SampleRingBuffer(const SampleRingBuffer&) = delete;
    SampleRingBuffer& operator=(const SampleRingBuffer&) = delete;

    void clear();
    void push(const CaptureSample& sample);
    bool latest(CaptureSample& sample) const;
    bool at(uint16_t offset, CaptureSample& sample) const;
    uint16_t size() const { return count_; }
    uint32_t overwriteCount() const { return overwriteCount_; }

private:
    CaptureSample samples_[kCapacity];
    uint16_t head_;
    uint16_t count_;
    uint32_t overwriteCount_;
};

} // namespace team04

#endif // TEAM04_SAMPLE_RING_BUFFER_H_

/***************************** END OF FILE ************************************/
