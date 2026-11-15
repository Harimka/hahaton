/**
 * @file    sample_ring_buffer.cpp
 * @author  team-04
 * @brief   Static ring buffer implementation for accelerometer samples.
 */

#include "sample_ring_buffer.h"

namespace team04 {

SampleRingBuffer::SampleRingBuffer()
    : samples_{}
    , head_(0U)
    , count_(0U)
    , overwriteCount_(0U)
{
}

void SampleRingBuffer::clear()
{
    head_ = 0U;
    count_ = 0U;
    overwriteCount_ = 0U;
}

void SampleRingBuffer::push(const CaptureSample& sample)
{
    samples_[head_] = sample;
    head_ = static_cast<uint16_t>((head_ + 1U) & kMask);
    if (count_ < kCapacity) {
        ++count_;
    } else {
        ++overwriteCount_;
    }
}

bool SampleRingBuffer::latest(CaptureSample& sample) const
{
    if (count_ == 0U) {
        return false;
    }

    const uint16_t index = static_cast<uint16_t>((head_ - 1U) & kMask);
    sample = samples_[index];
    return true;
}

bool SampleRingBuffer::at(uint16_t offset, CaptureSample& sample) const
{
    if (offset >= count_) {
        return false;
    }

    const uint16_t oldest = static_cast<uint16_t>((head_ - count_) & kMask);
    const uint16_t index = static_cast<uint16_t>((oldest + offset) & kMask);
    sample = samples_[index];
    return true;
}

} // namespace team04

/***************************** END OF FILE ************************************/
