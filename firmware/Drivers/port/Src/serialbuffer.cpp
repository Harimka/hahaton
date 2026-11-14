/** @file serialbuffer.cpp
 * @author team-04
 * @brief Static nonblocking serial TX buffer implementation.
 */
#include "serialbuffer.h"
#include <cstring>
namespace team04 {
SerialBuffer::SerialBuffer(TransmitFn transmit, void* context)
    : transmit_(transmit)
    , context_(context)
{
}
uint16_t SerialBuffer::txFree() const {
    const uint16_t used = head_ >= tail_ ? static_cast<uint16_t>(head_ - tail_)
                                         : static_cast<uint16_t>(kCapacity - tail_ + head_);
    return static_cast<uint16_t>(kCapacity - used - 1U);
}
uint16_t SerialBuffer::write(const uint8_t* data, uint16_t length) {
    if (data == nullptr) return 0U;
    const uint16_t accepted = length > txFree() ? txFree() : length;
    for (uint16_t i = 0U; i < accepted; ++i) {
        buffer_[head_] = data[i]; head_ = static_cast<uint16_t>((head_ + 1U) % kCapacity);
    }
    droppedBytes_ += static_cast<uint32_t>(length - accepted);
    return accepted;
}
void SerialBuffer::service() {
    if (busy_ || head_ == tail_ || transmit_ == nullptr) return;
    const uint16_t length = head_ > tail_ ? static_cast<uint16_t>(head_ - tail_)
                                         : static_cast<uint16_t>(kCapacity - tail_);
    busy_ = true;
    if (transmit_(context_, buffer_ + tail_, length)) {
        tail_ = static_cast<uint16_t>((tail_ + length) % kCapacity);
    }
    busy_ = false;
}
}
/***************************** END OF FILE ************************************/
