/** @file controlservice.cpp
 * @author team-04
 * @brief Nonblocking framed control and telemetry service implementation.
 */
#include "controlservice.h"
#include <cstring>
#include <cstdio>
namespace team04 {
namespace {
uint16_t crc16(const uint8_t* data, uint16_t length) {
    uint16_t crc = 0xffffU;
    for (uint16_t i = 0U; i < length; ++i) {
        crc ^= static_cast<uint16_t>(data[i]);
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            crc = (crc & 1U) != 0U ? static_cast<uint16_t>((crc >> 1U) ^ 0xa001U) : static_cast<uint16_t>(crc >> 1U);
        }
    }
    return crc;
}
}
ControlService::ControlService(SerialDrv& serial, FifoCapture& capture)
    : serial_(serial)
    , capture_(capture)
{
}
void ControlService::receive(const uint8_t* data, uint16_t length) {
    if (data == nullptr) return;
    for (uint16_t i = 0U; i < length; ++i) {
        const uint16_t next = static_cast<uint16_t>((rxHead_ + 1U) % kRxCapacity);
        if (next == rxTail_) rxTail_ = static_cast<uint16_t>((rxTail_ + 1U) % kRxCapacity);
        rx_[rxHead_] = data[i]; rxHead_ = next;
    }
}
void ControlService::poll() {
    while (rxTail_ != rxHead_) {
        const uint8_t value = rx_[rxTail_];
        rxTail_ = static_cast<uint16_t>((rxTail_ + 1U) % kRxCapacity);
        if (asciiMode_ && frameState_ == 0U && value != kSof) {
            parseAsciiByte(value);
        } else {
            parseByte(value);
        }
    }
    if (streaming_ || calibrationRequested_) {
        const CaptureBlock* block = capture_.peek();
        if (block != nullptr) { consume(*block); capture_.release(); }
    }
}
void ControlService::parseAsciiByte(uint8_t byte) {
    if (byte == '\r' || byte == '\n') {
        if (asciiLength_ != 0U) { asciiCommand_[asciiLength_] = '\0'; handleAsciiCommand(); asciiLength_ = 0U; }
        return;
    }
    if (asciiLength_ + 1U < sizeof(asciiCommand_) && byte >= 0x20U && byte <= 0x7eU) {
        asciiCommand_[asciiLength_++] = static_cast<char>(byte);
    } else if (byte < 0x20U || byte > 0x7eU) {
        asciiLength_ = 0U;
    }
}
void ControlService::handleAsciiCommand() {
    if (std::strcmp(asciiCommand_, "START") == 0 || std::strcmp(asciiCommand_, "START_STREAM") == 0) {
        streaming_ = true; textMode_ = true; return;
    }
    if (std::strcmp(asciiCommand_, "STOP") == 0 || std::strcmp(asciiCommand_, "STOP_STREAM") == 0) {
        streaming_ = false; return;
    }
    if (std::strcmp(asciiCommand_, "STATUS") == 0) { reportStats(); return; }
    if (std::strcmp(asciiCommand_, "RESET") == 0 || std::strcmp(asciiCommand_, "RESET_ORIENTATION") == 0) { resetCalibration(); return; }
    if (std::strcmp(asciiCommand_, "CALIBRATE") == 0 || std::strcmp(asciiCommand_, "CALIBRATE_ALL") == 0) {
        calibrationSamples_ = 0U; calibrationAxis_ = 3U; calibrationRequested_ = true;
        calibrationSum_[0] = calibrationSum_[1] = calibrationSum_[2] = 0; return;
    }
    if (std::strncmp(asciiCommand_, "CALIBRATE_AXIS ", 15U) == 0 && asciiCommand_[15] >= '0' && asciiCommand_[15] <= '2') {
        calibrationSamples_ = 0U; calibrationAxis_ = static_cast<uint8_t>(asciiCommand_[15] - '0'); calibrationRequested_ = true;
        calibrationSum_[0] = calibrationSum_[1] = calibrationSum_[2] = 0;
    }
}
void ControlService::parseByte(uint8_t byte) {
    switch (frameState_) {
    case 0U:
        if (byte == kSof) { frameState_ = 1U; frameLength_ = 0U; }
        break;
    case 1U: frameType_ = byte; frameState_ = 2U; break;
    case 2U: expectedLength_ = byte; frameState_ = 3U; break;
    case 3U:
        expectedLength_ = static_cast<uint16_t>(expectedLength_ | (static_cast<uint16_t>(byte) << 8U));
        if (expectedLength_ > kMaxPayload) { frameState_ = 0U; break; }
        payloadLength_ = 0U; frameState_ = expectedLength_ == 0U ? 5U : 4U; break;
    case 4U:
        payload_[payloadLength_++] = byte;
        if (payloadLength_ == expectedLength_) frameState_ = 5U;
        break;
    case 5U: frame_[0] = byte; frameState_ = 6U; break;
    case 6U: {
        const uint16_t received = static_cast<uint16_t>(frame_[0] | (static_cast<uint16_t>(byte) << 8U));
        uint8_t header[3] = {frameType_, static_cast<uint8_t>(expectedLength_), static_cast<uint8_t>(expectedLength_ >> 8U)};
        uint16_t check = crc16(header, 3U);
        check = static_cast<uint16_t>(crc16(payload_, payloadLength_) ^ check);
        if (received == check && frameType_ == static_cast<uint8_t>(FrameType::kCommand)) handleCommand(payload_, payloadLength_);
        frameState_ = 0U;
        break;
    }
    default: frameState_ = 0U; break;
    }
}
void ControlService::handleCommand(const uint8_t* payload, uint16_t length) {
    if (length == 0U) return;
    const Command command = static_cast<Command>(payload[0]);
    switch (command) {
    case Command::kStatus: reportStats(); break;
    case Command::kResetOrientation: resetCalibration(); sendResult(command, Result::kOk); break;
    case Command::kCalibrateAll:
        calibrationSamples_ = 0U; calibrationAxis_ = 3U; calibrationRequested_ = true;
        calibrationSum_[0] = calibrationSum_[1] = calibrationSum_[2] = 0;
        sendResult(command, Result::kOk); break;
    case Command::kCalibrateAxis:
        if (length < 2U || payload[1] > 2U) { sendResult(command, Result::kInvalid); break; }
        calibrationSamples_ = 0U; calibrationAxis_ = payload[1];
        calibrationRequested_ = true;
        calibrationSum_[0] = calibrationSum_[1] = calibrationSum_[2] = 0;
        sendResult(command, Result::kOk); break;
    case Command::kStartStream: streaming_ = true; sendResult(command, Result::kOk); break;
    case Command::kStopStream: streaming_ = false; sendResult(command, Result::kOk); break;
    case Command::kTextMode:
        if (length < 2U) { sendResult(command, Result::kInvalid); break; }
        textMode_ = payload[1] != 0U; sendResult(command, Result::kOk); break;
    default: sendResult(command, Result::kInvalid); break;
    }
}
void ControlService::resetCalibration() {
    calibrationRequested_ = false;
    calibrationSamples_ = 0U;
    calibrationSum_[0] = calibrationSum_[1] = calibrationSum_[2] = 0;
    baseline_[0] = baseline_[1] = baseline_[2] = 0.0F;
}
bool ControlService::send(FrameType type, const uint8_t* payload, uint16_t length) {
    if (length > kMaxPayload || serial_.txFree() < static_cast<uint16_t>(length + 7U)) return false;
    frame_[0] = kSof; frame_[1] = static_cast<uint8_t>(type);
    frame_[2] = static_cast<uint8_t>(length); frame_[3] = static_cast<uint8_t>(length >> 8U);
    if (length != 0U && payload != nullptr) std::memcpy(frame_ + 4U, payload, length);
    uint8_t header[3] = {frame_[1], frame_[2], frame_[3]};
    uint16_t check = static_cast<uint16_t>(crc16(header, 3U) ^ crc16(frame_ + 4U, length));
    frame_[4U + length] = static_cast<uint8_t>(check); frame_[5U + length] = static_cast<uint8_t>(check >> 8U);
    return serial_.write(frame_, static_cast<uint16_t>(length + 6U)) == static_cast<uint16_t>(length + 6U);
}
bool ControlService::sendResult(Command command, Result result) {
    const uint8_t payload[2] = {static_cast<uint8_t>(command), static_cast<uint8_t>(result)};
    return send(FrameType::kResponse, payload, 2U);
}
void ControlService::consume(const CaptureBlock& block) {
    if (calibrationRequested_) {
        for (uint16_t i = 0U; i < block.count && calibrationSamples_ < kCalibrationWindow; ++i) {
            const ImuRawSample& sample = block.samples[i];
            const int16_t values[3] = {sample.rawX, sample.rawY, sample.rawZ};
            for (uint8_t axis = 0U; axis < 3U; ++axis) {
                if (calibrationAxis_ == 3U || calibrationAxis_ == axis) calibrationSum_[axis] += values[axis];
            }
            ++calibrationSamples_;
            if (calibrationSamples_ == kCalibrationWindow) {
                for (uint8_t axis = 0U; axis < 3U; ++axis) {
                    if (calibrationAxis_ == 3U || calibrationAxis_ == axis) baseline_[axis] = static_cast<float>(calibrationSum_[axis]) / static_cast<float>(calibrationSamples_);
                }
                calibrationRequested_ = false;
            }
        }
    }
    if (!streaming_) return;
    if (textMode_) {
        for (uint16_t i = 0U; i < block.count; ++i) {
            const ImuRawSample& sample = block.samples[i];
            char line[96];
            int written = snprintf(line, sizeof(line), "G %lu %lu %d %d %d %u\r\n",
                static_cast<unsigned long>(sample.timestampUs),
                static_cast<unsigned long>(sample.sampleIndex),
                sample.rawX, sample.rawY, sample.rawZ, sample.flags);
            if (written > 0 && static_cast<size_t>(written) < sizeof(line)) {
                serial_.write(reinterpret_cast<const uint8_t*>(line), static_cast<uint16_t>(written));
            }
        }
    } else {
        const uint16_t bytes = static_cast<uint16_t>(block.count * sizeof(ImuRawSample));
        if (bytes <= kMaxPayload) send(FrameType::kSample, reinterpret_cast<const uint8_t*>(block.samples), bytes);
    }
}
void ControlService::reportStats() {
    const CaptureStats stats = capture_.snapshot();
    if (textMode_) {
        char line[256];
        int written = snprintf(line, sizeof(line),
            "STATS irq=%lu dmaS=%lu dmaC=%lu dmaE=%lu dmaT=%lu busy=%lu ovr=%lu pat=%lu drop=%lu pub=%lu qHW=%lu fifoMax=%lu svcMax=%lu tmWrap=%lu seqWrap=%lu\r\n",
            static_cast<unsigned long>(stats.watermarkIrqs), static_cast<unsigned long>(stats.dmaStarted),
            static_cast<unsigned long>(stats.dmaCompleted), static_cast<unsigned long>(stats.dmaErrors),
            static_cast<unsigned long>(stats.dmaTimeouts), static_cast<unsigned long>(stats.busyIrqs),
            static_cast<unsigned long>(stats.fifoOverruns), static_cast<unsigned long>(stats.patternErrors),
            static_cast<unsigned long>(stats.droppedSamples), static_cast<unsigned long>(stats.publishedSamples),
            static_cast<unsigned long>(stats.queueHighWater), static_cast<unsigned long>(stats.maxFifoWords),
            static_cast<unsigned long>(stats.maxServiceUs), static_cast<unsigned long>(stats.timerWraps),
            static_cast<unsigned long>(stats.sequenceWraps));
        if (written > 0 && static_cast<size_t>(written) < sizeof(line)) {
            serial_.write(reinterpret_cast<const uint8_t*>(line), static_cast<uint16_t>(written));
        }
    } else {
        uint8_t payload[sizeof(CaptureStats)]; std::memcpy(payload, &stats, sizeof(stats));
        send(FrameType::kStats, payload, sizeof(payload));
    }
}
}
/***************************** END OF FILE ************************************/
