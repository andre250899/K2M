#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace k2m
{

struct AudioMetrics
{
    float peak = 0.0f;
    float peakDb = -120.0f;
    float rms = 0.0f;
    float rmsDb = -120.0f;
    bool hasNanOrInf = false;
};

struct TrimResult
{
    juce::AudioBuffer<float> processedAudio;
    int64_t startFrameRemoved = 0;
    int64_t endFrameRemoved = 0;
    AudioMetrics metrics;
};

class SampleProcessor
{
public:
    // Calcula métricas de pico, RMS e dBFS
    static AudioMetrics calculateMetrics (const juce::AudioBuffer<float>& buffer, int numFrames);

    // Aplica trim conservador mantendo ataque e cauda natural com fades de borda
    static TrimResult processSample (const juce::AudioBuffer<float>& rawBuffer,
                                     int totalFrames,
                                     double sampleRate,
                                     int64_t noteOnFrame,
                                     float silenceThresholdDb = -70.0f,
                                     double preAttackLeadSec = 0.02,
                                     double postTailLeadSec = 0.10);

    // Converte amplitude linear em dBFS
    static float toDecibels (float amplitude) noexcept;
};

} // namespace k2m
