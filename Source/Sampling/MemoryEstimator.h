#pragma once

#include "SamplingPlan.h"
#include <cstdint>

namespace k2m
{

struct BatchEstimation
{
    int totalNotes = 0;
    int totalJobs = 0;
    double estimatedTimeSeconds = 0.0;
    int64_t pcm16Bytes = 0;
    int64_t pcm24Bytes = 0;
    int64_t float32Bytes = 0;
    double pcm16Megabytes = 0.0;
    double pcm24Megabytes = 0.0;
    double float32Megabytes = 0.0;
};

class MemoryEstimator
{
public:
    static BatchEstimation estimate (const SamplingPlan& plan, int numChannels = 2)
    {
        BatchEstimation est;

        std::vector<int> notes;
        for (int n = plan.startNote; n <= plan.endNote; n += plan.noteStep)
            notes.push_back (n);

        if (plan.includeEndNote && (notes.empty() || notes.back() != plan.endNote))
            notes.push_back (plan.endNote);

        est.totalNotes = (int) notes.size();
        const int numVels = (int) plan.velocities.size();
        est.totalJobs = est.totalNotes * numVels * plan.roundRobins;

        const double timePerJob = plan.preRollSeconds + plan.noteDurationSeconds + plan.releaseMinSeconds + plan.settleSeconds;
        est.estimatedTimeSeconds = (double) est.totalJobs * timePerJob;

        const int64_t framesPerJob = (int64_t) ((plan.preRollSeconds + plan.noteDurationSeconds + plan.releaseMinSeconds) * plan.sampleRate);
        const int64_t totalFrames = (int64_t) est.totalJobs * framesPerJob;

        est.pcm16Bytes = totalFrames * numChannels * 2;
        est.pcm24Bytes = totalFrames * numChannels * 3;
        est.float32Bytes = totalFrames * numChannels * 4;

        est.pcm16Megabytes = (double) est.pcm16Bytes / (1024.0 * 1024.0);
        est.pcm24Megabytes = (double) est.pcm24Bytes / (1024.0 * 1024.0);
        est.float32Megabytes = (double) est.float32Bytes / (1024.0 * 1024.0);

        return est;
    }
};

} // namespace k2m
