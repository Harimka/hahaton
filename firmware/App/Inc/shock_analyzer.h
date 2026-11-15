/**
 * @file    shock_analyzer.h
 * @author  team-04
 * @brief   Static shock trigger, frame extraction and SRS analysis.
 */

#ifndef TEAM04_SHOCK_ANALYZER_H_
#define TEAM04_SHOCK_ANALYZER_H_

#include <stdint.h>
#include "sample_ring_buffer.h"

namespace team04 {

struct ShockSample {
    uint32_t timestampUs;
    float accelX;
    float accelY;
    float accelZ;
};

struct ShockFrame {
    static constexpr uint16_t kCapacity = 2048U;
    uint32_t eventId;
    uint32_t triggerTimestampUs;
    uint16_t count;
    ShockSample samples[kCapacity];
};

struct SrsPoint {
    float frequencyHz;
    float gx;
    float gy;
    float gz;
};

struct SrsResult {
    static constexpr uint16_t kPointCount = 92U;
    SrsPoint points[kPointCount];
    bool thresholdConfigured;
    bool thresholdExceeded;
};

/** @brief Static, allocation-free shock detector and SRS calculator. */
class ShockAnalyzer final {
public:
    ShockAnalyzer();
    ~ShockAnalyzer() = default;

    ShockAnalyzer(const ShockAnalyzer&) = delete;
    ShockAnalyzer& operator=(const ShockAnalyzer&) = delete;

    void reset();
    void process(const CaptureSample& sample, const SampleRingBuffer& history);
    /** @brief Execute a bounded slice of deferred SRS work. */
    void service();
    /** @brief Copy a completed frame and start deferred SRS into caller-owned
     * storage. Both references must remain valid until srsReady() is true. */
    bool takeResult(ShockFrame& frame, SrsResult& srs);
    bool srsReady() const { return !calculating_; }

private:
    static constexpr uint16_t kBaselineLength = 256U;
    static constexpr uint16_t kHistoryLength = 1332U;
    static constexpr uint16_t kQuietSamples = 64U;
    static constexpr float kAccelScaleG = 0.000488f;
    static constexpr float kTriggerG = 3.0f;
    static constexpr float kEndG = 1.0f;

    void finishFrame();
    void trimFrameToThreshold();
    void startSrsPoint();
    void evaluateThreshold();
    void append(const CaptureSample& sample);
    float magnitude(const ShockSample& sample) const;

    ShockFrame frame_;
    SrsResult srs_;
    float baselineX_;
    float baselineY_;
    float baselineZ_;
    float baselineSumX_;
    float baselineSumY_;
    float baselineSumZ_;
    uint16_t baselineCount_;
    uint16_t quietCount_;
    bool capturing_;
    bool resultReady_;
    uint32_t eventId_;
    struct Coefficients {
        float frequency;
        float b0;
        float b1;
        float b2;
        float a1;
        float a2;
    };
    Coefficients coefficients_[SrsResult::kPointCount];
    bool calculating_;
    uint16_t srsPoint_;
    uint32_t srsSample_;
    uint32_t srsLength_;
    float input1_[3];
    float input2_[3];
    float output1_[3];
    float output2_[3];
    float maxima_[3];
    const ShockFrame* analysisFrame_;
    SrsResult* analysisResult_;
};

} // namespace team04

#endif // TEAM04_SHOCK_ANALYZER_H_

/***************************** END OF FILE ************************************/
