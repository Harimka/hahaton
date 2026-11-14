/** @file sdcard_test.cpp
 * @author team-04
 * @brief Simulated SD SPI command, address, CRC and timeout checks.
 */
#include "sdcard.h"
#include <cassert>
#include <deque>
using namespace team04;
namespace {
class MockClock final : public ClockDrv {
public:
    mutable uint32_t now = 0U;
    uint32_t millis() const override { return now++; }
    uint32_t micros() const override { return now * 1000U; }
};
uint16_t crc16(const uint8_t* b, unsigned n) {
    uint16_t crc = 0U;
    for (unsigned i = 0; i < n; ++i) {
        crc ^= static_cast<uint16_t>(b[i]) << 8U;
        for (unsigned j = 0; j < 8; ++j) crc = static_cast<uint16_t>((crc & 0x8000U) ? (crc << 1U) ^ 0x1021U : crc << 1U);
    }
    return crc;
}
class MockSd final : public SpiDrv {
public:
    std::deque<uint8_t> responses;
    bool selected = false, v2 = true, absent = false, badCrc = false;
    uint32_t arg = 0U, speed = 0U; uint8_t phase = 0U;
    IoStatus setClock(uint32_t hz) override { speed = hz; return IoStatus::kOk; }
    void select() override { assert(!selected); selected = true; }
    void deselect() override { selected = false; }
    IoStatus startDma(const uint8_t*, uint8_t*, uint16_t) override { return IoStatus::kUnsupported; }
    IoStatus abortDma() override { return IoStatus::kUnsupported; }
    IoStatus transfer(const uint8_t* tx, uint8_t* rx, uint16_t len, uint32_t) override {
        if (selected && tx && len == 6U && (tx[0] & 0xc0U) == 0x40U) {
            const uint8_t cmd = tx[0] & 63U;
            arg = (static_cast<uint32_t>(tx[1]) << 24U) | (static_cast<uint32_t>(tx[2]) << 16U) |
                  (static_cast<uint32_t>(tx[3]) << 8U) | tx[4];
            if (absent) return IoStatus::kOk;
            switch (cmd) {
            case 0: responses.push_back(1); break;
            case 8: responses.push_back(v2 ? 1 : 5); if (v2) { responses.push_back(0); responses.push_back(0); responses.push_back(1); responses.push_back(0xaa); } break;
            case 55: responses.push_back(1); break;
            case 41: responses.push_back(0); break;
            case 58: responses.push_back(0); responses.push_back(v2 ? 0xc0 : 0x80); responses.push_back(0); responses.push_back(0); responses.push_back(0); break;
            case 16: responses.push_back(0); break;
            case 17:
                responses.push_back(0); responses.push_back(0xfe);
                for (unsigned i=0; i<512; ++i) responses.push_back(0);
                responses.push_back(0); responses.push_back(badCrc ? 1 : 0); break;
            case 9: {
                uint8_t csd[16]{};
                if (v2) { csd[0] = 0x40; csd[7] = 1; csd[8] = 0xff; csd[9] = 0xff; }
                else { csd[5] = 9; csd[7] = 0xff; csd[8] = 0xc0; csd[10] = 0x80; }
                const uint16_t crc = crc16(csd, 16);
                responses.push_back(0); responses.push_back(0xfe);
                for (uint8_t b : csd) responses.push_back(b);
                responses.push_back(static_cast<uint8_t>(crc >> 8U)); responses.push_back(static_cast<uint8_t>(crc)); break;
            }
            case 24: responses.push_back(0); phase = 1; break;
            case 13: responses.push_back(0); responses.push_back(0); break;
            default: assert(false);
            }
            return IoStatus::kOk;
        }
        if (selected && phase == 1U && tx && len == 1U && tx[0] == 0xfeU) { phase = 2U; return IoStatus::kOk; }
        if (selected && phase == 2U && tx && len == 512U) { phase = 3U; return IoStatus::kOk; }
        if (selected && phase == 3U && tx && len == 2U) { phase = 0U; responses.push_back(5U); responses.push_back(0xffU); return IoStatus::kOk; }
        if (rx) for (uint16_t i=0; i<len; ++i) { rx[i] = responses.empty() ? 0xffU : responses.front(); if (!responses.empty()) responses.pop_front(); }
        return IoStatus::kOk;
    }
};
}
int main() {
    MockClock clock; MockSd bus; SdCard card(bus, clock); uint8_t data[512]{};
    assert(card.initialize() == IoStatus::kOk && bus.speed == 10000000U);
    uint64_t sectors = 0U; assert(card.sectorCount(sectors) == IoStatus::kOk && sectors == 134217728ULL);
    assert(card.read(7, data) == IoStatus::kOk && bus.arg == 7U);
    bus.badCrc = true; assert(card.read(7, data) == IoStatus::kError); bus.badCrc = false;
    assert(card.write(7, data) == IoStatus::kOk);
    bus.v2 = false; assert(card.initialize() == IoStatus::kOk);
    assert(card.sectorCount(sectors) == IoStatus::kOk && sectors == 8192U);
    assert(card.read(7, data) == IoStatus::kOk && bus.arg == 3584U);
    bus.absent = true; assert(card.initialize() == IoStatus::kTimeout && !bus.selected);
}
/***************************** END OF FILE ************************************/
