#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/Plugin/PluginHost.h"
#include "../Source/Plugin/PluginScanWorker.h"
#include "../Source/Audio/CaptureSink.h"
#include <iostream>
#include <cmath>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    // Este mesmo .exe é relançado como worker de scan isolado (ver PluginHost::scanSearchPath).
    // Quando é o caso, só escaneia o plugin pedido e responde — não roda o teste do Gate A.
    juce::String commandLine;
    for (int i = 1; i < argc; ++i)
        commandLine << argv[i] << " ";

    if (k2m::runPluginScanWorkerIfRequested (commandLine))
        return 0;

    std::cout << "=== K2M Gate A - Teste Automatizado de Captura ===" << std::endl;

    k2m::PluginHost host;
    auto plugins = host.scanDefaultVst3Directory();

    std::cout << "[INFO] Plugins VST3 descobertos: " << plugins.size() << std::endl;
    for (const auto& p : plugins)
    {
        std::cout << "  - " << p.name.toStdString() << " (" << p.fileOrIdentifier.toStdString() << ")" << std::endl;
    }

    if (plugins.isEmpty())
    {
        std::cerr << "[FALHA] Nenhum plugin VST3 encontrado para o teste!" << std::endl;
        return 1;
    }

    // Selecionar Kontakt se disponível, senão o plugin de teste
    juce::PluginDescription targetDesc = plugins[0];
    for (const auto& p : plugins)
    {
        if (p.name.containsIgnoreCase ("Kontakt"))
        {
            targetDesc = p;
            break;
        }
    }

    std::cout << "[INFO] Alvo selecionado: " << targetDesc.name.toStdString() << std::endl;

    const double sampleRate = 44100.0;
    const int blockSize = 512;
    bool loadDone = false;
    bool loadSuccess = false;

    host.loadPluginAsync (targetDesc, sampleRate, blockSize, [&] (bool success, const juce::String& err)
    {
        loadDone = true;
        loadSuccess = success;
        if (! success)
            std::cerr << "[ERRO] " << err.toStdString() << std::endl;
        juce::MessageManager::getInstance()->stopDispatchLoop();
    });

    // Processar message loop até o callback chamar stopDispatchLoop()
    if (! loadDone)
    {
        juce::MessageManager::getInstance()->runDispatchLoop();
    }

    if (! loadSuccess || host.getInstance() == nullptr)
    {
        std::cerr << "[FALHA] Não foi possível carregar a instância do plugin." << std::endl;
        return 1;
    }

    std::cout << "[OK] Plugin carregado: " << host.getLoadedPluginName().toStdString() << std::endl;

    // Configurar Timeline do Gate A (7.25s)
    const int64_t preRollFrames = (int64_t) (0.25 * sampleRate);
    const int64_t noteOnFrame   = preRollFrames;
    const int64_t noteOffFrame  = noteOnFrame + (int64_t) (5.0 * sampleRate);
    const int64_t totalFrames   = noteOffFrame + (int64_t) (2.0 * sampleRate);

    k2m::CaptureSink sink;
    sink.prepareTake (2, (int) totalFrames);
    sink.arm();

    auto* plugin = host.getInstance();

    juce::AudioBuffer<float> blockBuffer (2, blockSize);
    int64_t cursor = 0;

    std::cout << "[INFO] Processando blocos determinísticos (" << totalFrames << " frames)..." << std::endl;

    while (cursor < totalFrames)
    {
        const int count = (int) std::min ((int64_t) blockSize, totalFrames - cursor);
        blockBuffer.clear();

        juce::MidiBuffer midi;
        // Note On em 0.25s
        if (noteOnFrame >= cursor && noteOnFrame < cursor + count)
        {
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), (int) (noteOnFrame - cursor));
        }
        // Note Off em 5.25s
        if (noteOffFrame >= cursor && noteOffFrame < cursor + count)
        {
            midi.addEvent (juce::MidiMessage::noteOff (1, 60), (int) (noteOffFrame - cursor));
        }

        plugin->processBlock (blockBuffer, midi);
        sink.recordBlock (blockBuffer, count);

        cursor += count;
    }

    sink.disarm();

    // Exportar arquivo WAV
    const juce::File exportDir = juce::File::getCurrentWorkingDirectory().getChildFile ("export");
    const juce::File outputFile = exportDir.getChildFile ("C3_v100.wav");

    std::cout << "[INFO] Salvando WAV em: " << outputFile.getFullPathName().toStdString() << std::endl;

    const bool saved = k2m::CaptureSink::saveWavFile (outputFile, sink.getCapturedAudio(), (int) totalFrames, sampleRate, 24);
    if (! saved)
    {
        std::cerr << "[FALHA] Falha ao gravar arquivo WAV." << std::endl;
        return 1;
    }

    // Validação independente do WAV gravado
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatReader> reader (wavFormat.createReaderFor (outputFile.createInputStream().release(), true));

    if (reader == nullptr)
    {
        std::cerr << "[FALHA] O arquivo WAV gerado não pôde ser lido pelo format reader." << std::endl;
        return 1;
    }

    const double readSampleRate = reader->sampleRate;
    const int readChannels = (int) reader->numChannels;
    const int64_t readLengthFrames = reader->lengthInSamples;

    std::cout << "[CHECK] Taxa lida: " << readSampleRate << " Hz (esperado: " << sampleRate << ")" << std::endl;
    std::cout << "[CHECK] Canais: " << readChannels << " (esperado: 2)" << std::endl;
    std::cout << "[CHECK] Frames: " << readLengthFrames << " (esperado: " << totalFrames << ")" << std::endl;

    if (readSampleRate != sampleRate || readChannels != 2 || readLengthFrames != totalFrames)
    {
        std::cerr << "[FALHA] Metadados do arquivo WAV divergem do esperado!" << std::endl;
        return 1;
    }

    // Checar valores finitos e medir amplitude de pico
    juce::AudioBuffer<float> checkBuffer (readChannels, (int) readLengthFrames);
    reader->read (&checkBuffer, 0, (int) readLengthFrames, 0, true, true);

    float peak = 0.0f;
    bool hasNanOrInf = false;

    for (int ch = 0; ch < readChannels; ++ch)
    {
        const float* data = checkBuffer.getReadPointer (ch);
        for (int i = 0; i < readLengthFrames; ++i)
        {
            if (std::isnan (data[i]) || std::isinf (data[i]))
            {
                hasNanOrInf = true;
                break;
            }
            peak = std::max (peak, std::abs (data[i]));
        }
    }

    std::cout << "[CHECK] Presença de NaN/Inf: " << (hasNanOrInf ? "SIM (ERRO)" : "NÃO (OK)") << std::endl;
    std::cout << "[CHECK] Pico máximo medido: " << peak << " (" << (20.0 * std::log10 (std::max (peak, 1e-6f))) << " dBFS)" << std::endl;

    if (hasNanOrInf)
    {
        std::cerr << "[FALHA] O arquivo contém valores NaN ou Inf!" << std::endl;
        return 1;
    }

    std::cout << "============================================================" << std::endl;
    std::cout << "[SUCESSO] Gate A APROVADO! Take C3_v100.wav gerado e validado." << std::endl;
    std::cout << "============================================================" << std::endl;

    return 0;
}
