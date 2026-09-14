#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <functional>

namespace k2m
{

class CaptureSink
{
public:
    CaptureSink() = default;
    ~CaptureSink() = default;

    // Alocar buffer antes de armar (chamado fora da audio thread)
    void prepareTake (int numChannels, int numTotalFrames);

    // Chamado dentro da audio thread para registrar frames
    // Retorna true se a captura ainda está em andamento, false se atingiu o fim
    bool recordBlock (const juce::AudioBuffer<float>& sourceBuffer, int numSamples);

    // Status da captura
    bool isArmed() const noexcept { return armed.load(); }
    bool isComplete() const noexcept { return completed.load(); }

    void arm() noexcept
    {
        writeCursor.store (0);
        completed.store (false);
        armed.store (true);
    }

    void disarm() noexcept
    {
        armed.store (false);
    }

    // Acessar o buffer gravado após a captura (imutável)
    const juce::AudioBuffer<float>& getCapturedAudio() const noexcept { return captureBuffer; }
    int getCapturedFrames() const noexcept { return writeCursor.load(); }

    // Salvar o buffer gravado como arquivo WAV (executado em worker thread)
    static bool saveWavFile (const juce::File& destinationFile,
                             const juce::AudioBuffer<float>& buffer,
                             int validFrames,
                             double sampleRate,
                             int bitDepth = 24);

private:
    juce::AudioBuffer<float> captureBuffer;
    std::atomic<int> targetFrames { 0 };
    std::atomic<int> writeCursor { 0 };
    std::atomic<bool> armed { false };
    std::atomic<bool> completed { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CaptureSink)
};

} // namespace k2m
