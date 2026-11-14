/** @file controlservice_test.cpp
 * @author team-04
 * @brief Host test for framed USB/UART control service.
 */
#include "controlservice.h"
#include <cassert>
#include <cstring>
using namespace team04;
namespace {
class MockSerial final : public SerialDrv {
public:
    uint8_t data[1024]{}; uint16_t length = 0U;
    uint16_t write(const uint8_t* source, uint16_t count) override { std::memcpy(data + length, source, count); length = static_cast<uint16_t>(length + count); return count; }
    uint16_t txFree() const override { return static_cast<uint16_t>(sizeof(data) - length); }
};
class MockClock final : public ClockDrv { public: uint32_t millis() const override { return 0U; } uint32_t micros() const override { return 0U; } };
class MockCritical final : public CriticalSection { public: uint32_t enter() override { return 0U; } void leave(uint32_t) override {} };
class MockSpi final : public SpiDrv {
public:
    IoStatus setClock(uint32_t) override { return IoStatus::kOk; } void select() override {} void deselect() override {}
    IoStatus transfer(const uint8_t*, uint8_t*, uint16_t, uint32_t) override { return IoStatus::kError; }
    IoStatus startDma(const uint8_t*, uint8_t*, uint16_t) override { return IoStatus::kError; }
    IoStatus abortDma() override { return IoStatus::kOk; }
};
}
int main() {
    MockSpi spi; MockClock clock; MockCritical critical; FifoCapture capture(spi, clock, critical); MockSerial serial;
    ControlService service(serial, capture);
    const uint8_t command[] = {ControlService::kSof, static_cast<uint8_t>(ControlService::FrameType::kCommand), 1U, 0U,
                               static_cast<uint8_t>(ControlService::Command::kStartStream), 0x5eU, 0xd3U};
    service.receive(command, sizeof(command)); service.poll();
    assert(service.streaming()); assert(serial.length == 8U);
    const uint8_t stop[] = {ControlService::kSof, static_cast<uint8_t>(ControlService::FrameType::kCommand), 1U, 0U,
                            static_cast<uint8_t>(ControlService::Command::kStopStream), 0x1eU, 0xd2U};
    service.receive(stop, sizeof(stop)); service.poll(); assert(!service.streaming());
}
/***************************** END OF FILE ************************************/
