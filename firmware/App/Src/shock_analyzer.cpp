/**
 * @file    shock_analyzer.cpp
 * @author  team-04
 * @brief   Static shock trigger, frame extraction and SRS analysis.
 */

#include "shock_analyzer.h"
#include <math.h>

namespace team04 {

ShockAnalyzer::ShockAnalyzer()
    : frame_{}, srs_{}, baselineX_(0.0f), baselineY_(0.0f), baselineZ_(0.0f)
    , baselineSumX_(0.0f), baselineSumY_(0.0f), baselineSumZ_(0.0f)
    , baselineCount_(0U), quietCount_(0U), capturing_(false)
    , resultReady_(false), eventId_(0U)
{
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
        baselineSumX_ += x;
        baselineSumY_ += y;
        baselineSumZ_ += z;
        if (baselineCount_ < kBaselineLength) {
            ++baselineCount_;
            baselineX_ = baselineSumX_ / static_cast<float>(baselineCount_);
            baselineY_ = baselineSumY_ / static_cast<float>(baselineCount_);
            baselineZ_ = baselineSumZ_ / static_cast<float>(baselineCount_);
        }

        ShockSample current{sample.timestampUs, x - baselineX_, y - baselineY_, z - baselineZ_};
        if (magnitude(current) < kTriggerG || baselineCount_ < kBaselineLength) {
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
    if (quietCount_ >= kQuietSamples) {
        finishFrame();
    }
}

void ShockAnalyzer::finishFrame()
{
    if (!capturing_ || frame_.count == 0U) {
        return;
    }
    trimFrameToThreshold();
    calculateSrs();
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

void ShockAnalyzer::calculateSrs()
{
    for (uint16_t point = 0U; point < SrsResult::kPointCount; ++point) {
        const float frequency = 10.0f * powf(2.0f, static_cast<float>(point) / 12.0f);
        const float omegaT = 6.28318530718f * frequency / 6660.0f;
        const float q = 10.0f;
        const float a = omegaT / (2.0f * q);
        const float b = omegaT * sqrtf(1.0f - 1.0f / (4.0f * q * q));
        const float decay = expf(-a);
        const float sinc = fabsf(b) < 0.000001f ? 1.0f : sinf(b) / b;
        const float b0 = 1.0f - decay * sinc;
        const float b1 = 2.0f * decay * (sinc - cosf(b));
        const float b2 = decay * decay - decay * sinc;
        const float a1 = -2.0f * decay * cosf(b);
        const float a2 = decay * decay;
        float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
        float x1 = 0.0f, x2 = 0.0f, yx1 = 0.0f, yx2 = 0.0f;
        float y1 = 0.0f, y2 = 0.0f, yy1 = 0.0f, yy2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f, yz1 = 0.0f, yz2 = 0.0f;
        for (uint16_t i = 0U; i < frame_.count; ++i) {
            const ShockSample& input = frame_.samples[i];
            const float outX = b0 * input.accelX + b1 * x1 + b2 * x2 - a1 * yx1 - a2 * yx2;
            const float outY = b0 * input.accelY + b1 * y1 + b2 * y2 - a1 * yy1 - a2 * yy2;
            const float outZ = b0 * input.accelZ + b1 * z1 + b2 * z2 - a1 * yz1 - a2 * yz2;
            x2 = x1; x1 = input.accelX; yx2 = yx1; yx1 = outX;
            y2 = y1; y1 = input.accelY; yy2 = yy1; yy1 = outY;
            z2 = z1; z1 = input.accelZ; yz2 = yz1; yz1 = outZ;
            maxX = fmaxf(maxX, fabsf(outX));
            maxY = fmaxf(maxY, fabsf(outY));
            maxZ = fmaxf(maxZ, fabsf(outZ));
        }
        srs_.points[point] = {frequency, maxX, maxY, maxZ};
    }
}

bool ShockAnalyzer::takeResult(ShockFrame& frame, SrsResult& srs)
{
    if (!resultReady_) {
        return false;
    }
    frame = frame_;
    srs = srs_;
    resultReady_ = false;
    frame_.count = 0U;
    return true;
}

} // namespace team04

/***************************** END OF FILE ************************************/
