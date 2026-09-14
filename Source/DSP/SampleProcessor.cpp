#include "SampleProcessor.h"
#include <cmath>
#include <algorithm>

namespace k2m
{

float SampleProcessor::toDecibels (float amplitude) noexcept
{
    if (amplitude <= 1e-6f)
        return -120.0f;
    return 20.0f * std::log10 (amplitude);
}

AudioMetrics SampleProcessor::calculateMetrics (const juce::AudioBuffer<float>& buffer, int numFrames)
{
    AudioMetrics m;
    const int channels = buffer.getNumChannels();
    if (channels == 0 || numFrames <= 0)
        return m;

    double sumSquares = 0.0;
    float peak = 0.0f;

    for (int ch = 0; ch < channels; ++ch)
    {
        const float* data = buffer.getReadPointer (ch);
        for (int i = 0; i < numFrames; ++i)
        {
            const float s = data[i];
            if (std::isnan (s) || std::isinf (s))
            {
                m.hasNanOrInf = true;
                continue;
            }

            const float absS = std::abs (s);
            if (absS > peak)
                peak = absS;

            sumSquares += (double) (s * s);
        }
    }

    m.peak = peak;
    m.peakDb = toDecibels (peak);

    const double totalSamples = (double) (channels * numFrames);
    m.rms = (float) std::sqrt (sumSquares / (totalSamples > 0 ? totalSamples : 1.0));
    m.rmsDb = toDecibels (m.rms);

    return m;
}

TrimResult SampleProcessor::processSample (const juce::AudioBuffer<float>& rawBuffer,
                                           int totalFrames,
                                           double sampleRate,
                                           int64_t noteOnFrame,
                                           float silenceThresholdDb,
                                           double preAttackLeadSec,
                                           double postTailLeadSec)
{
    TrimResult res;
    const int channels = rawBuffer.getNumChannels();
    if (channels == 0 || totalFrames <= 0)
        return res;

    const float thresholdAmp = std::pow (10.0f, silenceThresholdDb / 20.0f);
    const int preLeadFrames = (int) (preAttackLeadSec * sampleRate);
    const int postLeadFrames = (int) (postTailLeadSec * sampleRate);

    // 1. Encontrar o início do ataque (a partir de noteOnFrame)
    int attackStart = (int) noteOnFrame;
    for (int i = (int) noteOnFrame; i < totalFrames; ++i)
    {
        float maxCh = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
            maxCh = std::max (maxCh, std::abs (rawBuffer.getSample (ch, i)));

        if (maxCh >= thresholdAmp)
        {
            attackStart = i;
            break;
        }
    }

    const int trimStart = std::max (0, attackStart - preLeadFrames);

    // 2. Encontrar o fim da cauda de release (varrendo do fim para o início)
    int tailEnd = totalFrames - 1;
    for (int i = totalFrames - 1; i >= attackStart; --i)
    {
        float maxCh = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
            maxCh = std::max (maxCh, std::abs (rawBuffer.getSample (ch, i)));

        if (maxCh >= thresholdAmp)
        {
            tailEnd = i;
            break;
        }
    }

    const int trimEnd = std::min (totalFrames, tailEnd + postLeadFrames);
    const int validFrames = std::max (0, trimEnd - trimStart);

    res.startFrameRemoved = trimStart;
    res.endFrameRemoved = totalFrames - trimEnd;
    res.processedAudio.setSize (channels, validFrames);

    for (int ch = 0; ch < channels; ++ch)
    {
        res.processedAudio.copyFrom (ch, 0, rawBuffer, ch, trimStart, validFrames);
    }

    // 3. Aplicar fades curtos de borda para evitar clicks
    const int fadeInFrames = std::min (validFrames / 4, (int) (0.005 * sampleRate)); // 5ms fade in
    const int fadeOutFrames = std::min (validFrames / 4, (int) (0.030 * sampleRate)); // 30ms fade out

    if (fadeInFrames > 0)
    {
        for (int ch = 0; ch < channels; ++ch)
        {
            auto* data = res.processedAudio.getWritePointer (ch);
            for (int i = 0; i < fadeInFrames; ++i)
            {
                data[i] *= (float) i / (float) fadeInFrames;
            }
        }
    }

    if (fadeOutFrames > 0)
    {
        for (int ch = 0; ch < channels; ++ch)
        {
            auto* data = res.processedAudio.getWritePointer (ch);
            for (int i = 0; i < fadeOutFrames; ++i)
            {
                const int idx = validFrames - 1 - i;
                data[idx] *= (float) i / (float) fadeOutFrames;
            }
        }
    }

    res.metrics = calculateMetrics (res.processedAudio, validFrames);
    return res;
}

} // namespace k2m
