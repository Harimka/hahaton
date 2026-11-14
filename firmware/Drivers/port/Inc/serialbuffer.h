/** @file serialbuffer.h
 * @author team-04
 * @brief Static nonblocking serial TX buffer for USB CDC and UART adapters.
 */
#ifndef TEAM04_SERIALBUFFER_H_
#define TEAM04_SERIALBUFFER_H_
#include "serialdrv.h"
namespace team04 {
class SerialBuffer final : public SerialDrv {
public:
    using TransmitFn = bool (*)(void* context, const uint8_t* data, uint16_t length);
    static constexpr uint16_t kCapacity = 1024U;
    SerialBuffer(TransmitFn transmit, void* context);
    SerialBuffer(const SerialBuffer&) = delete;
    SerialBuffer& operator=(const SerialBuffer&) = delete;
    uint16_t write(const uint8_t* data, uint16_t length) override;
    uint16_t txFree() const override;
    /* Call from the main loop or TX-complete callback. */
    void service();
    uint32_t droppedBytes() const { return droppedBytes_; }
private:
    TransmitFn transmit_;
    void* context_;
    uint8_t buffer_[kCapacity]{};
    uint16_t head_ = 0U;
    uint16_t tail_ = 0U;
    bool busy_ = false;
    uint32_t droppedBytes_ = 0U;
};
}
#endif
/***************************** END OF FILE ************************************/
