#include "../Source/Sampling/SamplingPlan.h"
#include "../Source/Sampling/MemoryEstimator.h"
#include "../Source/Mapping/Mapper.h"
#include "../Source/Export/SfzExporter.h"
#include "../Source/Project/ProjectStore.h"
#include "../Source/DSP/SampleProcessor.h"
#include <iostream>
#include <cassert>

int main()
{
    std::cout << "=== K2M - Teste Unitario e Integrado: Mapeamento, SFZ, DSP e Projeto ===" << std::endl;

    // 1. Teste de Estimativa de Memória e Plano
    k2m::SamplingPlan plan;
    plan.startNote = 36; // C2
    plan.endNote = 48;   // C3
    plan.noteStep = 3;   // C2, D#2, F#2, A2, C3 (5 notas)
    plan.includeEndNote = true;
    plan.velocities = { 32, 64, 96, 127 };

    auto est = k2m::MemoryEstimator::estimate (plan, 2);
    std::cout << "[CHECK] Total de notas: " << est.totalNotes << " (esperado: 5)" << std::endl;
    std::cout << "[CHECK] Total de jobs: " << est.totalJobs << " (esperado: 20)" << std::endl;
    std::cout << "[CHECK] Memória PCM16 estimada: " << est.pcm16Megabytes << " MB" << std::endl;

    assert (est.totalNotes == 5);
    assert (est.totalJobs == 20);

    // 2. Teste do Mapeador de Zonas (Midpoint Partitioning)
    std::vector<int> roots = { 36, 39, 42, 45, 48 };
    auto keyRanges = k2m::Mapper::calculateKeyRanges (roots, 36, 48);

    std::cout << "[CHECK] Regioes de Teclas calculadas:" << std::endl;
    for (size_t i = 0; i < roots.size(); ++i)
    {
        std::cout << "  - Root " << roots[i] << " -> [" << keyRanges[i].low << " .. " << keyRanges[i].high << "]" << std::endl;
    }

    assert (keyRanges[0].low == 36 && keyRanges[0].high == 37); // (36+39)/2 = 37
    assert (keyRanges[1].low == 38 && keyRanges[1].high == 40); // (39+42)/2 = 40
    assert (keyRanges[2].low == 41 && keyRanges[2].high == 43); // (42+45)/2 = 43
    assert (keyRanges[3].low == 44 && keyRanges[3].high == 46); // (45+48)/2 = 46
    assert (keyRanges[4].low == 47 && keyRanges[4].high == 48); // Fim

    // Teste de Particionamento de Velocities
    auto velRanges = k2m::Mapper::calculateVelocityRanges ({ 32, 64, 96, 127 }, 1, 127);
    std::cout << "[CHECK] Camadas de Velocity calculadas:" << std::endl;
    for (size_t i = 0; i < plan.velocities.size(); ++i)
    {
        std::cout << "  - Vel " << plan.velocities[i] << " -> [" << velRanges[i].low << " .. " << velRanges[i].high << "]" << std::endl;
    }
    assert (velRanges[0].low == 1 && velRanges[0].high == 48);
    assert (velRanges[1].low == 49 && velRanges[1].high == 80);
    assert (velRanges[2].low == 81 && velRanges[2].high == 111);
    assert (velRanges[3].low == 112 && velRanges[3].high == 127);

    // 3. Teste de Geração SFZ
    std::vector<k2m::SampleAsset> assets;
    auto jobs = plan.generateJobs();
    for (const auto& j : jobs)
    {
        k2m::SampleAsset a;
        a.id = j.id;
        a.rootKey = j.note;
        a.velocity = j.velocity;
        a.rawPath = "raw/" + j.id + ".wav";
        a.processedPath = "export/wav/" + j.id + ".wav";
        assets.push_back (a);
    }

    auto regions = k2m::Mapper::buildRegions (assets, 36, 48);
    std::cout << "[CHECK] Regioes construidas: " << regions.size() << " (esperado: 20)" << std::endl;
    assert (regions.size() == 20);

    juce::File sfzFile = juce::File::getCurrentWorkingDirectory().getChildFile ("export/instrument.sfz");
    bool sfzSaved = k2m::SfzExporter::exportToFile (sfzFile, regions, "K2M Test Instrument");
    std::cout << "[CHECK] SFZ exportado: " << (sfzSaved ? "SIM (OK)" : "NAO (ERRO)") << std::endl;
    assert (sfzSaved && sfzFile.existsAsFile());

    auto sfzContent = sfzFile.loadFileAsString();
    assert (sfzContent.contains ("<global>"));
    assert (sfzContent.contains ("<group>"));
    assert (sfzContent.contains ("<region>"));
    assert (sfzContent.contains ("pitch_keycenter=36"));
    assert (sfzContent.contains ("lokey=36 hikey=37 lovel=1 hivel=48"));

    // 4. Teste de Persistência do Projeto .k2m (JSON round-trip)
    k2m::ProjectData projData;
    projData.projectId = "proj_test_01";
    projData.name = "Piano Worship Test";
    projData.pluginName = "Kontakt 8";
    projData.plan = plan;
    projData.jobs = jobs;
    projData.assets = assets;
    projData.regions = regions;

    juce::File k2mFile = juce::File::getCurrentWorkingDirectory().getChildFile ("export/project.k2m");
    bool projSaved = k2m::ProjectStore::saveProject (k2mFile, projData);
    std::cout << "[CHECK] Projeto .k2m salvo: " << (projSaved ? "SIM (OK)" : "NAO (ERRO)") << std::endl;
    assert (projSaved && k2mFile.existsAsFile());

    k2m::ProjectData loadedProj;
    bool projLoaded = k2m::ProjectStore::loadProject (k2mFile, loadedProj);
    std::cout << "[CHECK] Projeto .k2m carregado: " << (projLoaded ? "SIM (OK)" : "NAO (ERRO)") << std::endl;
    assert (projLoaded);
    assert (loadedProj.name == "Piano Worship Test");
    assert (loadedProj.jobs.size() == 20);
    assert (loadedProj.regions.size() == 20);

    // 5. Teste de DSP / Trim de Áudio
    const int sampleRate = 44100;
    const int totalFrames = sampleRate * 2; // 2 segundos
    juce::AudioBuffer<float> testAudio (2, totalFrames);
    testAudio.clear();

    // Inserir som apenas entre 0.5s e 1.5s
    const int soundStart = (int) (0.5 * sampleRate);
    const int soundEnd = (int) (1.5 * sampleRate);
    for (int i = soundStart; i < soundEnd; ++i)
    {
        float val = 0.5f * (float) std::sin (2.0 * 3.1415926535 * 440.0 * (i - soundStart) / sampleRate);
        testAudio.setSample (0, i, val);
        testAudio.setSample (1, i, val);
    }

    auto trimResult = k2m::SampleProcessor::processSample (testAudio, totalFrames, sampleRate, soundStart, -70.0f);
    std::cout << "[CHECK] Frames iniciais removidos no trim: " << trimResult.startFrameRemoved << std::endl;
    std::cout << "[CHECK] Frames finais removidos no trim: " << trimResult.endFrameRemoved << std::endl;
    std::cout << "[CHECK] Pico do sample processado: " << trimResult.metrics.peakDb << " dBFS" << std::endl;
    assert (trimResult.startFrameRemoved > 0);
    assert (trimResult.endFrameRemoved > 0);
    assert (! trimResult.metrics.hasNanOrInf);
    assert (trimResult.metrics.peak > 0.4f);

    std::cout << "============================================================" << std::endl;
    std::cout << "[SUCESSO] Todos os modulos de Mapeamento, SFZ, DSP e Projeto estao 100% validados!" << std::endl;
    std::cout << "============================================================" << std::endl;

    return 0;
}
