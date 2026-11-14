/** @file lsm6ds3.cpp
 * @author team-04
 * @brief LSM6DS3 SPI register, FIFO, and raw data operations.
 */
#include "lsm6ds3.h"
namespace team04 {
Lsm6ds3::Lsm6ds3(SpiDrv& bus)
    : bus_(bus)
    , observedId_(0U)
    , configured_(false)
{
}
IoStatus Lsm6ds3::readRegister(uint8_t reg, uint8_t* data, uint16_t len) {
    if (!data || !len) return IoStatus::kInvalid;
    const uint8_t cmd = static_cast<uint8_t>(reg | 0x80U);
    bus_.select();
    IoStatus s = bus_.transfer(&cmd, nullptr, 1U, 20U);
    if (s == IoStatus::kOk) s = bus_.transfer(nullptr, data, len, 20U);
    bus_.deselect();
    return s;
}
IoStatus Lsm6ds3::writeChecked(uint8_t reg, uint8_t value, uint8_t mask) {
    const uint8_t tx[2] = {static_cast<uint8_t>(reg & 0x7fU), value};
    bus_.select(); IoStatus s = bus_.transfer(tx, nullptr, 2U, 20U); bus_.deselect();
    if (s != IoStatus::kOk) return s;
    uint8_t got = 0U; s = readRegister(reg, &got, 1U);
    return s != IoStatus::kOk ? s : ((got & mask) == (value & mask) ? IoStatus::kOk : IoStatus::kError);
}
IoStatus Lsm6ds3::resetBegin() {
    configured_ = false;
    const uint8_t tx[2] = {kCtrl3, 1U};
    bus_.select(); const IoStatus s = bus_.transfer(tx, nullptr, 2U, 20U); bus_.deselect(); return s;
}
IoStatus Lsm6ds3::resetPoll(bool& complete) {
    uint8_t v = 0U; IoStatus s = readRegister(kCtrl3, &v, 1U);
    if (s == IoStatus::kOk) complete = (v & 1U) == 0U;
    return s;
}
IoStatus Lsm6ds3::configure() {
    configured_ = false;
    IoStatus s = readRegister(kWhoAmIReg, &observedId_, 1U);
    if (s != IoStatus::kOk) return s;
    if (observedId_ != kWhoAmI) return IoStatus::kUnsupported;
    const uint8_t regs[] = {kCtrl3,kCtrl2,kCtrl4,kCtrl6,kCtrl8,kCtrl9,kInt1Ctrl,kCtrl1};
    const uint8_t values[] = {0x44U,0U,0U,0U,0U,0x38U,0U,0xa4U};
    const uint8_t masks[] = {0x7eU,0xf0U,0x80U,0x10U,0xe4U,0x38U,0xffU,0xffU};
    for (uint8_t i = 0; i < sizeof(regs); ++i) {
        s = writeChecked(regs[i], values[i], masks[i]);
        if (s != IoStatus::kOk) return s;
    }
    configured_ = true; return IoStatus::kOk;
}
int16_t Lsm6ds3::decodeWord(const uint8_t* p) {
    const uint16_t bits = static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8U));
    return static_cast<int16_t>((bits & 0x8000U) ? static_cast<int32_t>(bits) - 65536 : bits);
}
IoStatus Lsm6ds3::readRaw(int16_t& x, int16_t& y, int16_t& z) {
    if (!configured_) return IoStatus::kNotReady;
    uint8_t status = 0U, data[6];
    IoStatus s = readRegister(kStatus, &status, 1U);
    if (s != IoStatus::kOk) return s;
    if (!(status & 1U)) return IoStatus::kNotReady;
    s = readRegister(kOutX, data, 6U);
    if (s == IoStatus::kOk) { x = decodeWord(data); y = decodeWord(data + 2); z = decodeWord(data + 4); }
    return s;
}
IoStatus Lsm6ds3::configureFifo(uint16_t watermarkSamples) {
    if (!configured_) return IoStatus::kNotReady;
    if (!watermarkSamples || watermarkSamples > 1365U) return IoStatus::kInvalid;
    const uint16_t words = static_cast<uint16_t>(watermarkSamples * 3U);
    const uint8_t regs[] = {kInt1Ctrl,kFifoCtrl5,kFifoCtrl1,kFifoCtrl2,kFifoCtrl3,kFifoCtrl4,kFifoCtrl5};
    const uint8_t vals[] = {0U,0U,static_cast<uint8_t>(words),static_cast<uint8_t>(words >> 8U),1U,0U,0x56U};
    const uint8_t masks[] = {0xffU,0x7fU,0xffU,0xcfU,0x3fU,0x7fU,0x7fU};
    for (uint8_t i = 0; i < sizeof(regs); ++i) {
        const IoStatus s = writeChecked(regs[i], vals[i], masks[i]);
        if (s != IoStatus::kOk) return s;
    }
    return IoStatus::kOk;
}
IoStatus Lsm6ds3::enableFifoInterrupt(bool enabled) {
    return configured_ ? writeChecked(kInt1Ctrl, enabled ? 0x08U : 0U, 0xffU) : IoStatus::kNotReady;
}
IoStatus Lsm6ds3::fifoStatus(FifoStatus& out) {
    if (!configured_) return IoStatus::kNotReady;
    uint8_t b[4]; const IoStatus s = readRegister(kFifoStatus1, b, 4U);
    if (s != IoStatus::kOk) return s;
    out.unreadWords = static_cast<uint16_t>(b[0] | (static_cast<uint16_t>(b[1] & 0x0fU) << 8U));
    out.nextPattern = static_cast<uint16_t>(b[2] | (static_cast<uint16_t>(b[3] & 3U) << 8U));
    out.empty = (b[1]&0x10U)!=0U; out.full = (b[1]&0x20U)!=0U;
    out.overrun = (b[1]&0x40U)!=0U; out.watermark = (b[1]&0x80U)!=0U;
    return IoStatus::kOk;
}
bool Lsm6ds3::prepareFifoDma(uint8_t* tx, uint16_t samples, uint16_t capacity) {
    const uint32_t bytes = 1U + static_cast<uint32_t>(samples) * 6U;
    if (!tx || !samples || bytes > capacity) return false;
    tx[0] = kFifoDataOutL | 0x80U;
    for (uint32_t i = 1U; i < bytes; ++i) tx[i] = 0xffU;
    return true;
}
bool Lsm6ds3::decodeFifoSample(const uint8_t* rx, uint16_t samples, uint16_t index,
                               int16_t& x, int16_t& y, int16_t& z) {
    if (!rx || index >= samples) return false;
    const uint8_t* p = rx + 1U + static_cast<uint32_t>(index) * 6U;
    x = decodeWord(p); y = decodeWord(p + 2U); z = decodeWord(p + 4U); return true;
}
}
/***************************** END OF FILE ************************************/
