/** @file controlservice.h
 * @author team-04
 * @brief Fixed-frame USB/UART control, telemetry and IMU calibration service.
 */
#ifndef TEAM04_CONTROLSERVICE_H_
#define TEAM04_CONTROLSERVICE_H_
#include "fifocapture.h"
#include "serialdrv.h"
namespace team04 {
class ControlService final {
public:
    static constexpr uint8_t kSof = 0xa5U;
    static constexpr uint16_t kMaxPayload = 240U;
    enum class FrameType : uint8_t { kCommand = 1U, kResponse = 2U, kSample = 3U, kStats = 4U, kEvent = 5U };
    enum class Command : uint8_t {
        kStatus = 1U, kResetOrientation = 2U, kCalibrateAll = 3U,
        kCalibrateAxis = 4U, kStartStream = 5U, kStopStream = 6U, kTextMode = 7U
    };
    enum class Result : uint8_t { kOk = 0U, kInvalid = 1U, kBusy = 2U, kNoData = 3U };
    ControlService(SerialDrv& serial, FifoCapture& capture);
    ControlService(const ControlService&) = delete;
    ControlService& operator=(const ControlService&) = delete;
    /* Call from the main loop. It never waits for USB/UART. */
    void poll();
    /* Feed bytes from CDC_Receive_FS or UART RX callback. */
    void receive(const uint8_t* data, uint16_t length);
    /* Applies the current baseline and emits live samples when streaming. */
    void consume(const CaptureBlock& block);
    void reportStats();
    bool streaming() const { return streaming_; }
    bool calibrationActive() const { return calibrationRequested_; }
    const float* baseline() const { return baseline_; }
private:
    static constexpr uint16_t kRxCapacity = 512U;
    static constexpr uint16_t kFrameCapacity = 256U;
    static constexpr uint16_t kCalibrationWindow = 64U;
    void parseByte(uint8_t byte);
    void handleCommand(const uint8_t* payload, uint16_t length);
    bool send(FrameType type, const uint8_t* payload, uint16_t length);
    bool sendResult(Command command, Result result);
    void resetCalibration();
    SerialDrv& serial_;
    FifoCapture& capture_;
    uint8_t rx_[kRxCapacity]{};
    uint16_t rxHead_ = 0U;
    uint16_t rxTail_ = 0U;
    uint8_t frame_[kFrameCapacity]{};
    uint8_t payload_[kMaxPayload]{};
    uint16_t frameLength_ = 0U;
    uint16_t expectedLength_ = 0U;
    uint16_t payloadLength_ = 0U;
    uint8_t frameType_ = 0U;
    uint8_t frameState_ = 0U;
    bool streaming_ = false;
    bool textMode_ = true;
    bool calibrationRequested_ = false;
    uint16_t calibrationSamples_ = 0U;
    uint8_t calibrationAxis_ = 3U;
    int64_t calibrationSum_[3]{};
    float baseline_[3]{};
};
}
#endif
/***************************** END OF FILE ************************************/
