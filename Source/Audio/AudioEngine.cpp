#include "AudioEngine.h"

namespace k2m
{

AudioEngine::AudioEngine()
{
}

AudioEngine::~AudioEngine()
{
    deviceManager.removeAudioCallback (this);
}

juce::String AudioEngine::initAudio (int numInputs, int numOutputs)
{
    deviceManager.addAudioCallback (this);
    return deviceManager.initialiseWithDefaultDevices (numInputs, numOutputs);
}

void AudioEngine::setPlugin (juce::AudioPluginInstance* plugin)
{
    activePlugin.store (plugin);
}

void AudioEngine::injectMidiMessage (const juce::MidiMessage& msg)
{
    const juce::ScopedLock sl (midiLock);
    immediateMidi.addEvent (msg, 0);
}

void AudioEngine::armGateACapture (int note,
                                   int velocity,
                                   double noteDurationSec,
                                   double preRollSec,
                                   double releaseSec,
                                   std::function<void (bool, const juce::AudioBuffer<float>&, double)> onComplete)
{
    cancelCapture();

    const double sr = currentSampleRate > 0.0 ? currentSampleRate : 44100.0;
    const int64_t preRoll = (int64_t) (preRollSec * sr);
    const int64_t noteOn = preRoll;
    const int64_t noteOff = noteOn + (int64_t) (noteDurationSec * sr);
    const int64_t end = noteOff + (int64_t) (releaseSec * sr);

    scheduledEvents.clear();
    scheduledEvents.push_back ({ noteOn,  juce::MidiMessage::noteOn  (1, note, (juce::uint8) velocity) });
    scheduledEvents.push_back ({ noteOff, juce::MidiMessage::noteOff (1, note) });

    captureSink.prepareTake (2, (int) end);
    captureSink.arm();

    captureCompleteCallback = std::move (onComplete);

    frameCursor.store (0);
    totalFrames.store (end);
    capturing.store (true);
}

void AudioEngine::cancelCapture()
{
    capturing.store (false);
    captureSink.disarm();
    scheduledEvents.clear();
    captureCompleteCallback = nullptr;
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device != nullptr)
    {
        currentSampleRate = device->getCurrentSampleRate();
        currentBlockSize = device->getCurrentBufferSizeSamples();
    }

    scratchBuffer.setSize (2, currentBlockSize);
}

void AudioEngine::audioDeviceStopped()
{
    scratchBuffer.setSize (0, 0);
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* /*inputChannelData*/,
                                                    int /*numInputChannels*/,
                                                    float* const* outputChannelData,
                                                    int numOutputChannels,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext& /*context*/)
{
    juce::ScopedNoDenormals noDenormals;

    // Limpar buffers de saída
    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);
    }

    auto* plugin = activePlugin.load();
    if (plugin == nullptr)
        return;

    juce::MidiBuffer blockMidi;

    // Injetar mensagens manuais imediatas
    {
        const juce::ScopedLock sl (midiLock);
        if (! immediateMidi.isEmpty())
        {
            blockMidi.addEvents (immediateMidi, 0, numSamples, 0);
            immediateMidi.clear();
        }
    }

    const bool isCurrentlyCapturing = capturing.load (std::memory_order_relaxed);
    const int64_t cursor = frameCursor.load (std::memory_order_relaxed);
    const int64_t total = totalFrames.load (std::memory_order_relaxed);

    if (isCurrentlyCapturing)
    {
        // Adicionar eventos agendados dentro da janela [cursor, cursor + numSamples)
        for (const auto& event : scheduledEvents)
        {
            if (event.framePosition >= cursor && event.framePosition < cursor + numSamples)
            {
                const int sampleOffset = (int) (event.framePosition - cursor);
                blockMidi.addEvent (event.message, sampleOffset);
            }
        }
    }

    // Processar áudio através do plugin
    juce::AudioBuffer<float> outputBuffer (outputChannelData,
                                          juce::jmin (numOutputChannels, 2),
                                          numSamples);

    plugin->processBlock (outputBuffer, blockMidi);

    // Gravar no sink de captura
    if (isCurrentlyCapturing)
    {
        captureSink.recordBlock (outputBuffer, numSamples);

        const int64_t nextCursor = cursor + numSamples;
        frameCursor.store (nextCursor, std::memory_order_release);

        if (nextCursor >= total)
        {
            capturing.store (false, std::memory_order_release);
            captureSink.disarm();

            // Notificar conclusão na message thread de forma assíncrona
            if (captureCompleteCallback != nullptr)
            {
                auto callback = captureCompleteCallback;
                captureCompleteCallback = nullptr;
                const double sr = currentSampleRate;

                juce::MessageManager::callAsync ([this, callback, sr]() {
                    callback (true, captureSink.getCapturedAudio(), sr);
                });
            }
        }
    }
}

} // namespace k2m
