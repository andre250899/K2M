// Fase 3 - Fixture do Gate B: gera 4 WAV reais (2 notas x 2 velocities) via captura
// determinística no PluginHost/CaptureSink e exporta um instrumento SFZ portável,
// pronto para o teste manual de importação direta e de rota externa (seção 14.2 e 15.4
// do K2M-Arquitetura-e-Plano.md). A verificação em hardware (MODX M) permanece manual;
// este executável só valida o que pode ser comprovado localmente.

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/Plugin/PluginHost.h"
#include "../Source/Plugin/PluginScanWorker.h"
#include "../Source/Audio/CaptureSink.h"
#include "../Source/Mapping/Mapper.h"
#include "../Source/Export/SfzExporter.h"
#include "../Source/Project/ProjectStore.h"
#include <iostream>
#include <cassert>
#include <cmath>

namespace
{

struct FixtureJob
{
    int note = 60;
    int velocity = 100;
    juce::String id;
};

// Captura determinística de uma nota única na instância já carregada do plugin.
// Mesma topologia de timeline do Gate A (pré-roll -> Note On -> hold -> Note Off -> release),
// apenas com durações reduzidas por ser um fixture pequeno, não um take de produção.
bool captureOneJob (juce::AudioPluginInstance& plugin,
                    double sampleRate,
                    int blockSize,
                    const FixtureJob& job,
                    const juce::File& wavDestination,
                    k2m::SampleAsset& outAsset)
{
    const double preRollSec = 0.10;
    const double holdSec = 1.00;
    const double releaseSec = 0.50;

    const int64_t preRollFrames = (int64_t) (preRollSec * sampleRate);
    const int64_t noteOnFrame = preRollFrames;
    const int64_t noteOffFrame = noteOnFrame + (int64_t) (holdSec * sampleRate);
    const int64_t totalFrames = noteOffFrame + (int64_t) (releaseSec * sampleRate);

    k2m::CaptureSink sink;
    sink.prepareTake (2, (int) totalFrames);
    sink.arm();

    juce::AudioBuffer<float> blockBuffer (2, blockSize);
    int64_t cursor = 0;

    while (cursor < totalFrames)
    {
        const int count = (int) std::min ((int64_t) blockSize, totalFrames - cursor);
        blockBuffer.clear();

        juce::MidiBuffer midi;
        if (noteOnFrame >= cursor && noteOnFrame < cursor + count)
            midi.addEvent (juce::MidiMessage::noteOn (1, job.note, (juce::uint8) job.velocity), (int) (noteOnFrame - cursor));
        if (noteOffFrame >= cursor && noteOffFrame < cursor + count)
            midi.addEvent (juce::MidiMessage::noteOff (1, job.note), (int) (noteOffFrame - cursor));

        plugin.processBlock (blockBuffer, midi);
        sink.recordBlock (blockBuffer, count);

        cursor += count;
    }

    sink.disarm();

    if (! k2m::CaptureSink::saveWavFile (wavDestination, sink.getCapturedAudio(), (int) totalFrames, sampleRate, 24))
        return false;

    // Validação independente, igual ao Gate A: reabrir por leitor separado e checar metadados/NaN/Inf.
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatReader> reader (wavFormat.createReaderFor (wavDestination.createInputStream().release(), true));
    if (reader == nullptr)
        return false;

    if (reader->sampleRate != sampleRate || (int) reader->numChannels != 2 || reader->lengthInSamples != totalFrames)
        return false;

    juce::AudioBuffer<float> checkBuffer (2, (int) totalFrames);
    reader->read (&checkBuffer, 0, (int) totalFrames, 0, true, true);

    float peak = 0.0f;
    double sumSquares = 0.0;
    bool hasNanOrInf = false;

    for (int ch = 0; ch < 2; ++ch)
    {
        const float* data = checkBuffer.getReadPointer (ch);
        for (int64_t i = 0; i < totalFrames; ++i)
        {
            if (std::isnan (data[i]) || std::isinf (data[i]))
            {
                hasNanOrInf = true;
                break;
            }
            peak = std::max (peak, std::abs (data[i]));
            sumSquares += (double) data[i] * (double) data[i];
        }
    }

    if (hasNanOrInf || peak <= 0.0001f)
        return false;

    const double rms = std::sqrt (sumSquares / (double) (totalFrames * 2));

    outAsset.id = job.id;
    outAsset.rawPath = "raw/" + job.id + ".wav";
    outAsset.processedPath = "export/wav/" + job.id + ".wav";
    outAsset.rootKey = job.note;
    outAsset.velocity = job.velocity;
    outAsset.sampleRate = sampleRate;
    outAsset.numChannels = 2;
    outAsset.totalFrames = totalFrames;
    outAsset.noteOnFrame = noteOnFrame;
    outAsset.noteOffFrame = noteOffFrame;
    outAsset.peakDb = 20.0f * std::log10 (std::max (peak, 1e-6f));
    outAsset.rmsDb = 20.0f * (float) std::log10 (std::max (rms, 1e-6));

    return true;
}

} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    // Este mesmo .exe é relançado como worker de scan isolado (ver PluginHost::scanSearchPath).
    // Quando é o caso, só escaneia o plugin pedido e responde — não roda o fixture do Gate B.
    juce::String commandLine;
    for (int i = 1; i < argc; ++i)
        commandLine << argv[i] << " ";

    if (k2m::runPluginScanWorkerIfRequested (commandLine))
        return 0;

    std::cout << "=== K2M Fase 3 - Fixture do Gate B (2 notas x 2 velocities) ===" << std::endl;

    k2m::PluginHost host;
    auto plugins = host.scanDefaultVst3Directory();

    std::cout << "[INFO] Plugins VST3 descobertos: " << plugins.size() << std::endl;

    if (plugins.isEmpty())
    {
        std::cerr << "[FALHA] Nenhum plugin VST3 encontrado para gerar o fixture!" << std::endl;
        return 1;
    }

    juce::PluginDescription targetDesc = plugins[0];
    bool usingRealKontakt = false;
    for (const auto& p : plugins)
    {
        if (p.name.containsIgnoreCase ("Kontakt"))
        {
            targetDesc = p;
            usingRealKontakt = true;
            break;
        }
    }

    std::cout << "[INFO] Alvo selecionado: " << targetDesc.name.toStdString()
               << (usingRealKontakt ? " (Kontakt real)" : " (fonte de teste controlada)") << std::endl;

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

    if (! loadDone)
        juce::MessageManager::getInstance()->runDispatchLoop();

    if (! loadSuccess || host.getInstance() == nullptr)
    {
        std::cerr << "[FALHA] Não foi possível carregar a instância do plugin." << std::endl;
        return 1;
    }

    std::cout << "[OK] Plugin carregado: " << host.getLoadedPluginName().toStdString() << std::endl;

    // Plano mínimo do fixture: A2 (MIDI 45) e C3 (MIDI 60), velocities 64 e 127.
    // Convenção K2M: C3 = MIDI 60.
    std::vector<FixtureJob> fixtureJobs = {
        { 45, 64,  "n045_v064" },
        { 45, 127, "n045_v127" },
        { 60, 64,  "n060_v064" },
        { 60, 127, "n060_v127" },
    };

    const juce::File projectRoot = juce::File::getCurrentWorkingDirectory().getChildFile ("export");
    const juce::File wavDir = projectRoot.getChildFile ("wav");
    const juce::File sfzSamplesDir = projectRoot.getChildFile ("sfz").getChildFile ("samples");

    std::vector<k2m::SampleAsset> assets;
    bool allJobsOk = true;

    for (const auto& job : fixtureJobs)
    {
        std::cout << "[INFO] Capturando job " << job.id.toStdString()
                   << " (nota=" << job.note << ", velocity=" << job.velocity << ")..." << std::endl;

        const juce::File wavFile = wavDir.getChildFile (job.id + ".wav");
        k2m::SampleAsset asset;

        if (! captureOneJob (*host.getInstance(), sampleRate, blockSize, job, wavFile, asset))
        {
            std::cerr << "[FALHA] Job " << job.id.toStdString() << " não produziu um take válido." << std::endl;
            allJobsOk = false;
            continue;
        }

        // Cópia portável para a pasta de exportação SFZ (export/sfz/samples/), conforme seção 8.2/13.
        const juce::File sfzCopy = sfzSamplesDir.getChildFile (job.id + ".wav");
        sfzSamplesDir.createDirectory();
        wavFile.copyFileTo (sfzCopy);

        std::cout << "  [OK] " << asset.totalFrames << " frames, peak=" << asset.peakDb
                   << " dBFS, rms=" << asset.rmsDb << " dBFS" << std::endl;

        assets.push_back (asset);
    }

    if (! allJobsOk || assets.size() != fixtureJobs.size())
    {
        std::cerr << "[FALHA] Nem todos os 4 takes do fixture foram capturados com sucesso." << std::endl;
        return 1;
    }

    // Referenciar o caminho portável (relativo ao .sfz) para o mapeamento/exportação.
    for (auto& a : assets)
        a.processedPath = "samples/" + a.id + ".wav";

    // Mapear regiões: domínio de teclas 36-71 (três oitavas em torno das duas raízes),
    // sem extrapolar para 0-127 (seção 12).
    auto regions = k2m::Mapper::buildRegions (assets, 36, 71);

    std::cout << "[CHECK] Regiões construídas: " << regions.size() << " (esperado: 4)" << std::endl;
    assert (regions.size() == 4);

    // Fronteiras esperadas: raízes 45 e 60 -> boundary = (45+60)/2 = 52
    for (const auto& r : regions)
    {
        if (r.rootKey == 45)
        {
            assert (r.keyLow == 36 && r.keyHigh == 52);
        }
        else if (r.rootKey == 60)
        {
            assert (r.keyLow == 53 && r.keyHigh == 71);
        }

        // Fronteiras de velocity esperadas: 64 e 127 -> boundary = (64+127)/2 = 95
        if (r.velocityLow == 1)
            assert (r.velocityHigh == 95);
        else
            assert (r.velocityLow == 96 && r.velocityHigh == 127);
    }

    std::cout << "[CHECK] Cobertura de teclas/velocities sem sobreposição: OK" << std::endl;

    const juce::File sfzFile = projectRoot.getChildFile ("sfz").getChildFile ("k2m_fixture.sfz");
    const bool sfzSaved = k2m::SfzExporter::exportToFile (sfzFile, regions, "K2M Gate B Fixture");
    std::cout << "[CHECK] SFZ exportado em: " << sfzFile.getFullPathName().toStdString()
               << " -> " << (sfzSaved ? "OK" : "ERRO") << std::endl;
    assert (sfzSaved && sfzFile.existsAsFile());

    // Validar que cada referência "sample=" do SFZ resolve para um arquivo real ao lado do .sfz.
    const auto sfzContent = sfzFile.loadFileAsString();
    for (const auto& a : assets)
    {
        const juce::String needle = "sample=" + a.processedPath;
        if (! sfzContent.contains (needle))
        {
            std::cerr << "[FALHA] Referência ausente no SFZ: " << needle.toStdString() << std::endl;
            return 1;
        }

        const juce::File resolved = sfzFile.getParentDirectory().getChildFile (a.processedPath);
        if (! resolved.existsAsFile())
        {
            std::cerr << "[FALHA] Sample referenciado não existe no caminho portável: "
                       << resolved.getFullPathName().toStdString() << std::endl;
            return 1;
        }
    }
    std::cout << "[CHECK] Todas as 4 referências de sample do SFZ resolvem para arquivos reais: OK" << std::endl;

    // Persistir o manifesto do projeto (.k2m) com o estado real deste fixture.
    k2m::ProjectData projData;
    projData.projectId = "gateB-fixture";
    projData.name = "K2M Gate B Fixture (2 notas x 2 velocities)";
    projData.pluginName = host.getLoadedPluginName();
    projData.plan.startNote = 45;
    projData.plan.endNote = 60;
    projData.plan.noteStep = 15;
    projData.plan.includeEndNote = true;
    projData.plan.velocities = { 64, 127 };
    projData.plan.sampleRate = (int) sampleRate;
    for (const auto& job : fixtureJobs)
    {
        k2m::CaptureJob j;
        j.id = job.id;
        j.note = job.note;
        j.velocity = job.velocity;
        j.status = "complete";
        j.rawPath = "raw/" + job.id + ".wav";
        projData.jobs.push_back (j);
    }
    projData.assets = assets;
    projData.regions = regions;

    const juce::File k2mFile = projectRoot.getChildFile ("gateB_fixture.k2m");
    const bool projSaved = k2m::ProjectStore::saveProject (k2mFile, projData);
    std::cout << "[CHECK] Manifesto .k2m salvo: " << (projSaved ? "OK" : "ERRO") << std::endl;
    assert (projSaved);

    std::cout << "============================================================" << std::endl;
    std::cout << "[SUCESSO] Fixture da Fase 3 gerado e validado localmente." << std::endl;
    std::cout << "  WAV:  " << wavDir.getFullPathName().toStdString() << std::endl;
    std::cout << "  SFZ:  " << sfzFile.getFullPathName().toStdString() << std::endl;
    std::cout << "  .k2m: " << k2mFile.getFullPathName().toStdString() << std::endl;
    std::cout << "------------------------------------------------------------" << std::endl;
    std::cout << "[PENDENTE - MANUAL] Gate B só é aprovado em hardware real:" << std::endl;
    std::cout << "  1. Copiar export/sfz/ para USB." << std::endl;
    std::cout << "  2. No MODX M: New Waveform / Edit Waveform (Osc/Tune) por Element," << std::endl;
    std::cout << "     seguindo o roteiro da secao 14.2 do K2M-Arquitetura-e-Plano.md." << std::endl;
    std::cout << "  3. Testar rota direta (import WAV) e rota externa (SFZ -> ConvertWithMoss" << std::endl;
    std::cout << "     -> arquivo legado Yamaha -> MODX M), conforme secao 15.4." << std::endl;
    std::cout << "  4. Registrar: duas notas x duas velocities corretas, memoria ocupada e" << std::endl;
    std::cout << "     firmware instalado (criterio de aceite da Fase 3)." << std::endl;
    std::cout << "============================================================" << std::endl;

    return 0;
}
