#include "CaptureSink.h"

namespace k2m
{

void CaptureSink::prepareTake (int numChannels, int numTotalFrames)
{
    captureBuffer.setSize (numChannels, numTotalFrames);
    captureBuffer.clear();
    targetFrames.store (numTotalFrames);
    writeCursor.store (0);
    armed.store (false);
    completed.store (false);
}

bool CaptureSink::recordBlock (const juce::AudioBuffer<float>& sourceBuffer, int numSamples)
{
    if (! armed.load (std::memory_order_relaxed))
        return false;

    const int cursor = writeCursor.load (std::memory_order_relaxed);
    const int total = targetFrames.load (std::memory_order_relaxed);

    if (cursor >= total)
    {
        armed.store (false, std::memory_order_release);
        completed.store (true, std::memory_order_release);
        return false;
    }

    const int framesToCopy = juce::jmin (numSamples, total - cursor);
    const int channels = juce::jmin (captureBuffer.getNumChannels(), sourceBuffer.getNumChannels());

    for (int ch = 0; ch < channels; ++ch)
    {
        captureBuffer.copyFrom (ch, cursor, sourceBuffer, ch, 0, framesToCopy);
    }

    const int newCursor = cursor + framesToCopy;
    writeCursor.store (newCursor, std::memory_order_release);

    if (newCursor >= total)
    {
        armed.store (false, std::memory_order_release);
        completed.store (true, std::memory_order_release);
        return false;
    }

    return true;
}

bool CaptureSink::saveWavFile (const juce::File& destinationFile,
                              const juce::AudioBuffer<float>& buffer,
                              int validFrames,
                              double sampleRate,
                              int bitDepth)
{
    destinationFile.getParentDirectory().createDirectory();

    auto tempFile = destinationFile.getSiblingFile (destinationFile.getFileName() + ".tmp");
    if (tempFile.existsAsFile())
        tempFile.deleteFile();

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::OutputStream> stream (tempFile.createOutputStream());
    if (stream == nullptr)
        return false;

    auto options = juce::AudioFormatWriterOptions{}
                       .withSampleRate (sampleRate)
                       .withNumChannels ((size_t) buffer.getNumChannels())
                       .withBitsPerSample ((size_t) bitDepth);

    std::unique_ptr<juce::AudioFormatWriter> writer (wavFormat.createWriterFor (stream, options));

    if (writer == nullptr)
        return false;

    writer->writeFromAudioSampleBuffer (buffer, 0, validFrames);
    writer.reset(); // Fecha e descarrega o stream de arquivo

    // Substituição atômica no destino final
    if (destinationFile.existsAsFile())
        destinationFile.deleteFile();

    return tempFile.moveFileTo (destinationFile);
}

} // namespace k2m
