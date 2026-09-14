#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "CaptureSink.h"
#include <atomic>
#include <memory>
#include <vector>
#include <functional>

namespace k2m
{

struct ScheduledMidiEvent
{
    int64_t framePosition = 0;
    juce::MidiMessage message;
};

class AudioEngine final : public juce::AudioIODeviceCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;

    // Inicialização do dispositivo de áudio
    juce::String initAudio (int numInputs = 0, int numOutputs = 2);

    // Conectar ou desconectar o plugin ativo
    void setPlugin (juce::AudioPluginInstance* plugin);

    // Enviar evento MIDI imediato (ex: teste manual na UI)
    void injectMidiMessage (const juce::MidiMessage& msg);

    // Iniciar uma captura determinística de nota baseada em frames (Gate A)
    void armGateACapture (int note = 60,
                          int velocity = 100,
                          double noteDurationSec = 5.0,
                          double preRollSec = 0.25,
                          double releaseSec = 2.0,
                          std::function<void (bool success, const juce::AudioBuffer<float>& buffer, double sampleRate)> onComplete = nullptr);

    void cancelCapture();

    bool isCapturing() const noexcept { return capturing.load(); }
    int64_t getCurrentFrameCursor() const noexcept { return frameCursor.load(); }
    int64_t getTotalCaptureFrames() const noexcept { return totalFrames.load(); }

    // AudioIODeviceCallback overrides
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    double getSampleRate() const noexcept { return currentSampleRate; }
    int getBlockSize() const noexcept { return currentBlockSize; }

    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }
    CaptureSink& getCaptureSink() noexcept { return captureSink; }

private:
    juce::AudioDeviceManager deviceManager;
    std::atomic<juce::AudioPluginInstance*> activePlugin { nullptr };

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    juce::CriticalSection midiLock;
    juce::MidiBuffer immediateMidi;

    // Agendamento determinístico em frames
    CaptureSink captureSink;
    std::atomic<bool> capturing { false };
    std::atomic<int64_t> frameCursor { 0 };
    std::atomic<int64_t> totalFrames { 0 };
    std::vector<ScheduledMidiEvent> scheduledEvents;

    std::function<void (bool, const juce::AudioBuffer<float>&, double)> captureCompleteCallback;

    juce::AudioBuffer<float> scratchBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

} // namespace k2m
