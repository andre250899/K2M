#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace k2m
{

struct SampleAsset
{
    juce::String id;
    juce::String rawPath;
    juce::String processedPath;
    juce::String hash;
    int rootKey = 60;
    int velocity = 100;
    double sampleRate = 44100.0;
    int numChannels = 2;
    int64_t totalFrames = 0;
    int64_t noteOnFrame = 0;
    int64_t noteOffFrame = 0;
    float peakDb = -120.0f;
    float rmsDb = -120.0f;
};

struct Region
{
    juce::String assetId;
    juce::String samplePath;
    int rootKey = 60;
    int keyLow = 60;
    int keyHigh = 60;
    int velocityLow = 1;
    int velocityHigh = 127;
    juce::String articulation = "default";
    juce::String controllerStateId = "default";
    int roundRobin = 0;
    juce::String loopMode = "no_loop"; // no_loop, loop_continuous, loop_sustain
    int64_t loopStart = 0;
    int64_t loopEnd = 0;
};

} // namespace k2m
