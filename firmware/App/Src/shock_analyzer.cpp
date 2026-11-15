/**
 * @file    shock_analyzer.cpp
 * @author  team-04
 * @brief   Static shock trigger, frame extraction and SRS analysis.
 */

#include "shock_analyzer.h"
#include <math.h>

namespace team04 {

namespace {
/* Fill these 92 values with the approved limit curve in g. Zero means that
   the corresponding point is not configured yet. */
static constexpr float kSrsThresholdG[SrsResult::kPointCount] = {
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f
};
}

ShockAnalyzer::ShockAnalyzer()
    : frame_{}, srs_{}, baselineX_(0.0f), baselineY_(0.0f), baselineZ_(0.0f)
    , baselineSumX_(0.0f), baselineSumY_(0.0f), baselineSumZ_(0.0f)
    , baselineCount_(0U), quietCount_(0U), capturing_(false)
    , resultReady_(false), eventId_(0U)
    , coefficients_{}
    , calculating_(false)
    , srsPoint_(0U)
    , srsSample_(0U)
    , srsLength_(0U)
    , input1_{}, input2_{}, output1_{}, output2_{}, maxima_{}
    , analysisFrame_(nullptr)
    , analysisResult_(nullptr)
{
    /* Precompute once at startup, never while sampling an event. Formulas:
       Smallwood, ramp invariant absolute-acceleration SRS, Q=10. */
    for (uint16_t i = 0U; i < SrsResult::kPointCount; ++i) {
        const double frequency = 10.0 * pow(2.0, static_cast<double>(i) / 12.0);
        const double omegaT = 6.283185307179586 * frequency / 6660.0;
        const double a = omegaT / 20.0;
        const double b = omegaT * sqrt(1.0 - 0.0025);
        const double decay = exp(-a);
        const double s = decay * sin(b) / b;
        const double c = decay * cos(b);
        coefficients_[i] = {static_cast<float>(frequency),
            static_cast<float>(1.0 - s), static_cast<float>(2.0 * (s - c)),
            static_cast<float>(decay * decay - s),
            static_cast<float>(-2.0 * c), static_cast<float>(decay * decay)};
    }
}

void ShockAnalyzer::reset()
{
    baselineX_ = 0.0f;
    baselineY_ = 0.0f;
    baselineZ_ = 0.0f;
    baselineSumX_ = 0.0f;
    baselineSumY_ = 0.0f;
    baselineSumZ_ = 0.0f;
    baselineCount_ = 0U;
    quietCount_ = 0U;
    capturing_ = false;
    resultReady_ = false;
    calculating_ = false;
    frame_.count = 0U;
}

float ShockAnalyzer::magnitude(const ShockSample& sample) const
{
    const float ax = fabsf(sample.accelX);
    const float ay = fabsf(sample.accelY);
    const float az = fabsf(sample.accelZ);
    return fmaxf(ax, fmaxf(ay, az));
}

void ShockAnalyzer::append(const CaptureSample& sample)
{
    if (frame_.count >= ShockFrame::kCapacity) {
        finishFrame();
        return;
    }

    ShockSample& output = frame_.samples[frame_.count++];
    output.timestampUs = sample.timestampUs;
    output.accelX = static_cast<float>(sample.accelX) * kAccelScaleG - baselineX_;
    output.accelY = static_cast<float>(sample.accelY) * kAccelScaleG - baselineY_;
    output.accelZ = static_cast<float>(sample.accelZ) * kAccelScaleG - baselineZ_;
}

void ShockAnalyzer::process(const CaptureSample& sample, const SampleRingBuffer& history)
{
    if (resultReady_) {
        return;
    }

    const float x = static_cast<float>(sample.accelX) * kAccelScaleG;
    const float y = static_cast<float>(sample.accelY) * kAccelScaleG;
    const float z = static_cast<float>(sample.accelZ) * kAccelScaleG;

    if (!capturing_) {
        if (baselineCount_ < kBaselineLength) {
            baselineSumX_ += x;
            baselineSumY_ += y;
            baselineSumZ_ += z;
            ++baselineCount_;
            baselineX_ = baselineSumX_ / static_cast<float>(baselineCount_);
            baselineY_ = baselineSumY_ / static_cast<float>(baselineCount_);
            baselineZ_ = baselineSumZ_ / static_cast<float>(baselineCount_);
        }

        ShockSample current{sample.timestampUs, x - baselineX_, y - baselineY_, z - baselineZ_};
        if (magnitude(current) < kTriggerG || baselineCount_ < kBaselineLength
            || history.size() <= kHistoryLength) {
            return;
        }

        frame_.eventId = ++eventId_;
        frame_.triggerTimestampUs = sample.timestampUs;
        frame_.count = 0U;
        /* The current sample was pushed before process(), so exclude it here. */
        const uint16_t historySize = history.size();
        const uint16_t available = historySize == 0U
            ? 0U
            : static_cast<uint16_t>(historySize - 1U);
        const uint16_t prehistory = available < kHistoryLength ? available : kHistoryLength;
        const uint16_t start = static_cast<uint16_t>(available - prehistory);
        CaptureSample previous{};
        for (uint16_t i = start; i < available; ++i) {
            if (history.at(i, previous)) {
                append(previous);
            }
        }
        capturing_ = true;
        quietCount_ = 0U;
    }

    append(sample);
    const ShockSample current{sample.timestampUs, x - baselineX_, y - baselineY_, z - baselineZ_};
    if (magnitude(current) < kEndG) {
        ++quietCount_;
    } else {
        quietCount_ = 0U;
    }
    if (quietCount_ >= kQuietSamples || frame_.count == ShockFrame::kCapacity) {
        finishFrame();
    }
}

void ShockAnalyzer::finishFrame()
{
    if (!capturing_ || frame_.count == 0U) {
        return;
    }
    trimFrameToThreshold();
    capturing_ = false;
    resultReady_ = true;
}

void ShockAnalyzer::trimFrameToThreshold()
{
    if (frame_.count == 0U) {
        return;
    }

    float peak = 0.0f;
    uint16_t peakIndex = 0U;
    for (uint16_t i = 0U; i < frame_.count; ++i) {
        const float level = magnitude(frame_.samples[i]);
        if (level > peak) {
            peak = level;
            peakIndex = i;
        }
    }

    const float cutoff = peak * 0.25f;
    uint16_t first = 0U;
    for (uint16_t i = 0U; i <= peakIndex; ++i) {
        if (magnitude(frame_.samples[i]) >= cutoff) {
            first = i;
            break;
        }
    }

    uint16_t last = frame_.count - 1U;
    for (uint16_t i = frame_.count; i > peakIndex; --i) {
        const uint16_t index = static_cast<uint16_t>(i - 1U);
        if (magnitude(frame_.samples[index]) >= cutoff) {
            last = index;
            break;
        }
    }

    const uint16_t preservedStart = first > 16U
        ? static_cast<uint16_t>(first - 16U)
        : 0U;
    const uint16_t trimmedCount = static_cast<uint16_t>(last - preservedStart + 1U);
    for (uint16_t i = 0U; i < trimmedCount; ++i) {
        frame_.samples[i] = frame_.samples[static_cast<uint16_t>(preservedStart + i)];
    }
    frame_.count = trimmedCount;
}

void ShockAnalyzer::startSrsPoint()
{
    srsSample_ = 0U;
    /* Include one full lowest-frequency period after input removal. For an
       underdamped free response the first-cycle extrema bound later cycles. */
    srsLength_ = analysisFrame_->count + 1U
        + static_cast<uint32_t>(ceilf(6660.0f
            / (coefficients_[srsPoint_].frequency * 0.998749218f)));
    for (uint16_t axis = 0U; axis < 3U; ++axis) {
        input1_[axis] = input2_[axis] = 0.0f;
        output1_[axis] = output2_[axis] = maxima_[axis] = 0.0f;
    }
}

void ShockAnalyzer::service()
{
    if (!calculating_) {
        return;
    }
    const Coefficients& c = coefficients_[srsPoint_];
    /* Fixed work budget so acquisition can run between slices. */
    for (uint16_t step = 0U; step < 8U && srsSample_ < srsLength_; ++step) {
        float input[3] = {};
        if (srsSample_ < analysisFrame_->count) {
            const ShockSample& sample = analysisFrame_->samples[srsSample_];
            input[0] = sample.accelX;
            input[1] = sample.accelY;
            input[2] = sample.accelZ;
        }
        for (uint16_t axis = 0U; axis < 3U; ++axis) {
            const float out = c.b0 * input[axis] + c.b1 * input1_[axis]
                + c.b2 * input2_[axis] - c.a1 * output1_[axis]
                - c.a2 * output2_[axis];
            input2_[axis] = input1_[axis];
            input1_[axis] = input[axis];
            output2_[axis] = output1_[axis];
            output1_[axis] = out;
            maxima_[axis] = fmaxf(maxima_[axis], fabsf(out));
        }
        ++srsSample_;
    }
    if (srsSample_ == srsLength_) {
        srs_.points[srsPoint_] = {c.frequency, maxima_[0], maxima_[1], maxima_[2]};
        ++srsPoint_;
        if (srsPoint_ == SrsResult::kPointCount) {
            calculating_ = false;
            evaluateThreshold();
            *analysisResult_ = srs_;
        } else {
            startSrsPoint();
        }
    }
}

void ShockAnalyzer::evaluateThreshold()
{
    srs_.thresholdConfigured = true;
    srs_.thresholdExceeded = false;
    for (uint16_t point = 0U; point < SrsResult::kPointCount; ++point) {
        if (kSrsThresholdG[point] <= 0.0f) {
            srs_.thresholdConfigured = false;
            continue;
        }
        const SrsPoint& value = srs_.points[point];
        if (value.gx > kSrsThresholdG[point]
            || value.gy > kSrsThresholdG[point]
            || value.gz > kSrsThresholdG[point]) {
            srs_.thresholdExceeded = true;
        }
    }
}

bool ShockAnalyzer::takeResult(ShockFrame& frame, SrsResult& srs)
{
    if (!resultReady_ || calculating_) {
        return false;
    }
    frame = frame_;
    analysisFrame_ = &frame;
    analysisResult_ = &srs;
    calculating_ = true;
    srsPoint_ = 0U;
    startSrsPoint();
    resultReady_ = false;
    frame_.count = 0U;
    return true;
}

} // namespace team04

/***************************** END OF FILE ************************************/
