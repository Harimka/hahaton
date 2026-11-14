/** @file sdcard.cpp
 * @author team-04
 * @brief SD v1/v2 and SDHC/SDXC SPI single-block operations.
 */
#include "sdcard.h"
namespace team04 {
namespace { constexpr uint32_t kPollLimit = 1048576U; }
SdCard::SdCard(SpiDrv& bus, ClockDrv& clock)
    : bus_(bus)
    , clock_(clock)
    , ready_(false)
    , blockAddressed_(false)
    , lastResponse_(0xffU)
    , lastDataResponse_(0xffU)
    , maxWriteMs_(0U)
    , transferErrorCount_(0U)
{
}
IoStatus SdCard::transfer(const uint8_t* tx, uint8_t* rx, uint16_t n) {
    const IoStatus s = bus_.transfer(tx, rx, n, 100U);
    if (s != IoStatus::kOk) ++transferErrorCount_;
    return s;
}
void SdCard::deselect() { bus_.deselect(); (void)transfer(nullptr, nullptr, 1U); }
IoStatus SdCard::command(uint8_t index, uint32_t arg, uint8_t crc, uint8_t& response) {
    const uint8_t cmd[6] = {static_cast<uint8_t>(0x40U | index), static_cast<uint8_t>(arg >> 24U),
        static_cast<uint8_t>(arg >> 16U), static_cast<uint8_t>(arg >> 8U), static_cast<uint8_t>(arg), crc};
    IoStatus s = transfer(nullptr, nullptr, 1U);
    if (s == IoStatus::kOk) s = transfer(cmd, nullptr, 6U);
    if (s != IoStatus::kOk) return s;
    for (uint8_t i = 0; i < 16U; ++i) {
        s = transfer(nullptr, &response, 1U); if (s != IoStatus::kOk) return s;
        if (!(response & 0x80U)) { lastResponse_ = response; return IoStatus::kOk; }
    }
    return IoStatus::kTimeout;
}
IoStatus SdCard::singleCommand(uint8_t index, uint32_t arg, uint8_t crc, uint8_t& response) {
    bus_.select(); const IoStatus s = command(index, arg, crc, response); deselect(); return s;
}
IoStatus SdCard::waitFor(uint8_t expected, uint32_t timeoutMs, bool dataToken) {
    const uint32_t start = clock_.millis();
    for (uint32_t i = 0; i < kPollLimit; ++i) {
        uint8_t b; IoStatus s = transfer(nullptr, &b, 1U); if (s != IoStatus::kOk) return s;
        if (b == expected) return IoStatus::kOk;
        if (dataToken && b != 0xffU) { lastDataResponse_ = b; return IoStatus::kError; }
        if (static_cast<uint32_t>(clock_.millis() - start) >= timeoutMs) break;
    }
    return IoStatus::kTimeout;
}
uint16_t SdCard::crc16(const uint8_t* data, uint16_t len) {
    uint16_t crc = 0U;
    for (uint16_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8U;
        for (uint8_t b = 0; b < 8U; ++b)
            crc = static_cast<uint16_t>((crc & 0x8000U) ? (crc << 1U) ^ 0x1021U : crc << 1U);
    }
    return crc;
}
IoStatus SdCard::initialize() {
    ready_ = false; blockAddressed_ = false; lastResponse_ = 0xffU;
    bus_.deselect(); IoStatus s = bus_.setClock(400000U); if (s != IoStatus::kOk) return s;
    s = transfer(nullptr, nullptr, 10U); if (s != IoStatus::kOk) return s;
    uint8_t r = 0xffU; const uint32_t start = clock_.millis();
    for (uint16_t i = 0; i < 1024U; ++i) {
        s = singleCommand(0U, 0U, 0x95U, r);
        if (s == IoStatus::kOk && r == 1U) break;
        if (s != IoStatus::kOk && s != IoStatus::kTimeout) return s;
        if (static_cast<uint32_t>(clock_.millis() - start) >= 1500U) break;
    }
    if (s != IoStatus::kOk || r != 1U) return IoStatus::kTimeout;
    bool v2 = false; bus_.select(); s = command(8U, 0x1aaU, 0x87U, r);
    if (s == IoStatus::kOk && r == 1U) {
        uint8_t r7[4]; s = transfer(nullptr, r7, 4U);
        if (s == IoStatus::kOk) { if (r7[2] != 1U || r7[3] != 0xaaU) s = IoStatus::kUnsupported; else v2 = true; }
    } else if (s == IoStatus::kOk && r != 5U) s = IoStatus::kError;
    deselect(); if (s != IoStatus::kOk) return s;
    const uint32_t readyStart = clock_.millis();
    for (uint16_t i = 0; i < 4096U; ++i) {
        bus_.select(); s = command(55U, 0U, 0x01U, r);
        if (s == IoStatus::kOk && (r == 0U || r == 1U)) s = command(41U, v2 ? 0x40000000U : 0U, 0x01U, r);
        else if (s == IoStatus::kOk) s = IoStatus::kUnsupported;
        deselect(); if (s != IoStatus::kOk) return s;
        if (r == 0U) break;
        if (r != 1U) return IoStatus::kError;
        if (static_cast<uint32_t>(clock_.millis() - readyStart) >= 1500U) break;
    }
    if (r != 0U) return IoStatus::kTimeout;
    bus_.select(); s = command(58U, 0U, 0x01U, r);
    if (s == IoStatus::kOk && r == 0U) {
        uint8_t ocr[4]; s = transfer(nullptr, ocr, 4U);
        if (s == IoStatus::kOk) {
            if (!(ocr[0] & 0x80U)) s = IoStatus::kNotReady;
            else blockAddressed_ = v2 && (ocr[0] & 0x40U);
        }
    } else if (s == IoStatus::kOk) s = IoStatus::kError;
    deselect(); if (s != IoStatus::kOk) return s;
    if (!blockAddressed_) {
        s = singleCommand(16U, 512U, 0x01U, r);
        if (s != IoStatus::kOk) return s;
        if (r != 0U) return IoStatus::kError;
    }
    s = bus_.setClock(10000000U); if (s != IoStatus::kOk) return s;
    ready_ = true; return IoStatus::kOk;
}
IoStatus SdCard::address(uint32_t lba, uint32_t& arg) const {
    if (!ready_) return IoStatus::kNotReady;
    if (!blockAddressed_ && lba > UINT32_MAX / 512U) return IoStatus::kInvalid;
    arg = blockAddressed_ ? lba : lba * 512U; return IoStatus::kOk;
}
IoStatus SdCard::read(uint32_t lba, uint8_t* buffer) {
    if (!buffer) return IoStatus::kInvalid;
    uint32_t arg; IoStatus s = address(lba, arg); if (s != IoStatus::kOk) return s;
    uint8_t r, crc[2]; bus_.select(); s = command(17U, arg, 0x01U, r);
    if (s == IoStatus::kOk && r != 0U) s = IoStatus::kError;
    if (s == IoStatus::kOk) s = waitFor(0xfeU, 300U, true);
    if (s == IoStatus::kOk) s = transfer(nullptr, buffer, 512U);
    if (s == IoStatus::kOk) s = transfer(nullptr, crc, 2U);
    if (s == IoStatus::kOk && crc16(buffer, 512U) != (static_cast<uint16_t>(crc[0]) << 8U | crc[1])) s = IoStatus::kError;
    deselect(); return s;
}
IoStatus SdCard::write(uint32_t lba, const uint8_t* buffer) {
    if (!buffer) return IoStatus::kInvalid;
    uint32_t arg; IoStatus s = address(lba, arg); if (s != IoStatus::kOk) return s;
    const uint32_t start = clock_.millis(); uint8_t r, token = 0xfeU;
    const uint16_t crc = crc16(buffer, 512U);
    const uint8_t check[2] = {static_cast<uint8_t>(crc >> 8U), static_cast<uint8_t>(crc)};
    bus_.select(); s = command(24U, arg, 0x01U, r);
    if (s == IoStatus::kOk && r != 0U) s = IoStatus::kError;
    if (s == IoStatus::kOk) s = transfer(&token, nullptr, 1U);
    if (s == IoStatus::kOk) s = transfer(buffer, nullptr, 512U);
    if (s == IoStatus::kOk) s = transfer(check, nullptr, 2U);
    if (s == IoStatus::kOk) {
        r = 0xffU;
        for (uint8_t i = 0; i < 32U && r == 0xffU; ++i) { s = transfer(nullptr, &r, 1U); if (s != IoStatus::kOk) break; }
        lastDataResponse_ = r;
        if (s == IoStatus::kOk && r == 0xffU) s = IoStatus::kTimeout;
        else if (s == IoStatus::kOk && (r & 0x1fU) != 5U) s = IoStatus::kError;
    }
    if (s == IoStatus::kOk) s = waitFor(0xffU, 1000U, false);
    if (s == IoStatus::kOk) {
        uint8_t status = 0xffU; s = command(13U, 0U, 0x01U, r);
        if (s == IoStatus::kOk) s = transfer(nullptr, &status, 1U);
        if (s == IoStatus::kOk && (r != 0U || status != 0U)) s = IoStatus::kError;
    }
    deselect(); const uint32_t elapsed = clock_.millis() - start;
    if (elapsed > maxWriteMs_) maxWriteMs_ = elapsed;
    return s;
}
IoStatus SdCard::sectorCount(uint64_t& sectors) {
    if (!ready_) return IoStatus::kNotReady;
    uint8_t r, csd[16], crc[2]; bus_.select(); IoStatus s = command(9U, 0U, 0x01U, r);
    if (s == IoStatus::kOk && r != 0U) s = IoStatus::kError;
    if (s == IoStatus::kOk) s = waitFor(0xfeU, 300U, true);
    if (s == IoStatus::kOk) s = transfer(nullptr, csd, 16U);
    if (s == IoStatus::kOk) s = transfer(nullptr, crc, 2U);
    if (s == IoStatus::kOk && crc16(csd, 16U) != (static_cast<uint16_t>(crc[0]) << 8U | crc[1])) s = IoStatus::kError;
    deselect(); if (s != IoStatus::kOk) return s;
    const uint8_t version = csd[0] >> 6U;
    if ((version == 1U && !blockAddressed_) || (version == 0U && blockAddressed_)) return IoStatus::kUnsupported;
    if (version == 1U) {
        const uint32_t size = (static_cast<uint32_t>(csd[7] & 0x3fU) << 16U) | (static_cast<uint32_t>(csd[8]) << 8U) | csd[9];
        sectors = (static_cast<uint64_t>(size) + 1U) * 1024U;
    } else if (version == 0U) {
        const uint32_t readLen = csd[5] & 0x0fU;
        const uint32_t size = (static_cast<uint32_t>(csd[6] & 3U) << 10U) | (static_cast<uint32_t>(csd[7]) << 2U) | (csd[8] >> 6U);
        const uint32_t mult = (static_cast<uint32_t>(csd[9] & 3U) << 1U) | (csd[10] >> 7U);
        if (readLen < 9U || readLen > 11U) return IoStatus::kUnsupported;
        sectors = (static_cast<uint64_t>(size) + 1U) << (mult + 2U + readLen - 9U);
    } else return IoStatus::kUnsupported;
    return IoStatus::kOk;
}
}
/***************************** END OF FILE ************************************/
