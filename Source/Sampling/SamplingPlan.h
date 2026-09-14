#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <string>

namespace k2m
{

struct CaptureJob
{
    juce::String id;
    int ordinal = 0;
    int note = 60;
    int velocity = 100;
    juce::String articulation = "default";
    juce::String controllerStateId = "default";
    int roundRobin = 0;
    juce::String status = "pending"; // pending, recording, processing, complete, failed
    int attempt = 0;
    juce::String rawPath;
    juce::String error;
};

struct SamplingPlan
{
    int startNote = 21;           // MIDI 21 (A0 em notação científica)
    int endNote = 108;            // MIDI 108 (C8 em notação científica)
    int noteStep = 3;             // Intervalo em semitons
    bool includeEndNote = true;   // Forçar inclusão da última nota
    std::vector<int> velocities = {32, 64, 96, 127};
    double noteDurationSeconds = 5.0;
    double releaseMinSeconds = 2.0;
    double releaseMaxSeconds = 8.0;
    double preRollSeconds = 0.25;
    double settleSeconds = 0.25;
    int roundRobins = 1;
    int midiChannel = 1;          // Canal MIDI 1-16
    int sampleRate = 44100;

    std::vector<CaptureJob> generateJobs() const
    {
        std::vector<CaptureJob> jobs;
        int ordinal = 0;

        // Notas a serem capturadas
        std::vector<int> notes;
        for (int n = startNote; n <= endNote; n += noteStep)
            notes.push_back (n);

        if (includeEndNote && (notes.empty() || notes.back() != endNote))
            notes.push_back (endNote);

        for (int note : notes)
        {
            for (int vel : velocities)
            {
                CaptureJob job;
                job.ordinal = ordinal++;
                job.note = note;
                job.velocity = juce::jlimit (1, 127, vel);
                job.id = juce::String::formatted ("n%03d_v%03d", job.note, job.velocity);
                job.rawPath = "raw/" + job.id + ".wav";
                job.status = "pending";
                jobs.push_back (job);
            }
        }

        return jobs;
    }
};

} // namespace k2m
