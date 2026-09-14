// Fase 3 - Validação cruzada do SFZ com uma engine independente (sfizz).
//
// O MODX M físico não está disponível nesta máquina. Este teste substitui,
// parcialmente, a verificação em hardware: renderiza o export/sfz/k2m_fixture.sfz
// (gerado pelo GateBFixtureTest) através do sfizz_render.exe, um renderer
// offline de linha de comando que usa a mesma engine SFZ do plugin/lib sfizz
// (LGPLv3, https://github.com/sfztools/sfizz). Isso prova que um parser SFZ
// real e independente do K2M aceita o arquivo e resolve pitch_keycenter,
// lokey/hikey, lovel/hivel e o caminho do sample corretamente.
//
// Não substitui o Gate B: não comprova import no MODX M, memória ocupada
// nem firmware. Fica só a comprovação de que a rota "K2M -> SFZ -> player
// SFZ real" funciona antes de investir na rota "SFZ -> ConvertWithMoss ->
// arquivo Yamaha".
//
// Requer o binário sfizz_render.exe (baixado manualmente, não versionado -
// ver external/sfizz_render/ e .gitignore). Se não existir, o teste avisa
// e sai com código 0 (não bloqueia build/CI de quem não tem a ferramenta).

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <cmath>
#include <vector>

namespace
{

struct ExpectedNote
{
    juce::String sampleFileName;
    int midiNote = 60;
    int velocity = 100;
    double onTimeSeconds = 0.0;
    double expectedFreqHz = 440.0;
};

// Conta cruzamentos ascendentes por zero para estimar a frequência fundamental
// de um trecho de senoide limpa (o K2M_TestSynth gera onda senoidal pura).
double estimateFrequencyHz (const float* data, int numSamples, double sampleRate)
{
    int crossings = 0;
    for (int i = 1; i < numSamples; ++i)
        if (data[i - 1] <= 0.0f && data[i] > 0.0f)
            ++crossings;

    const double windowSeconds = (double) numSamples / sampleRate;
    if (windowSeconds <= 0.0 || crossings == 0)
        return 0.0;

    return (double) crossings / windowSeconds;
}

double computeRmsDb (const float* data, int numSamples)
{
    double sumSquares = 0.0;
    for (int i = 0; i < numSamples; ++i)
        sumSquares += (double) data[i] * (double) data[i];

    const double rms = std::sqrt (sumSquares / std::max (1, numSamples));
    return 20.0 * std::log10 (std::max (rms, 1e-9));
}

} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    std::cout << "=== K2M Fase 3 - Validacao cruzada do SFZ com engine independente (sfizz) ===" << std::endl;

    const juce::File repoRoot = juce::File::getCurrentWorkingDirectory().getParentDirectory();
    const juce::File sfizzRenderExe = repoRoot.getChildFile ("external/sfizz_render/sfizz_render.exe");
    const juce::File sfzFile = argc > 1
        ? juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1]))
        : juce::File::getCurrentWorkingDirectory().getChildFile ("export/sfz/k2m_fixture.sfz");

    if (! sfizzRenderExe.existsAsFile())
    {
        std::cout << "[AVISO] sfizz_render.exe nao encontrado em " << sfizzRenderExe.getFullPathName().toStdString() << std::endl;
        std::cout << "[AVISO] Baixe o release Windows de https://github.com/sfztools/sfizz/releases" << std::endl;
        std::cout << "[AVISO] e copie bin/Release/sfizz_render.exe + sfizz.dll para external/sfizz_render/." << std::endl;
        std::cout << "[SKIP] Teste nao bloqueante: ferramenta opcional ausente." << std::endl;
        return 0;
    }

    if (! sfzFile.existsAsFile())
    {
        std::cerr << "[FALHA] " << sfzFile.getFullPathName().toStdString()
                   << " nao existe. Rode GateBFixtureTest primeiro." << std::endl;
        return 1;
    }

    // Mesmo plano do fixture do Gate B: A2 (MIDI 45) e C3 (MIDI 60), velocities 64 e 127.
    // 440 * 2^((nota-69)/12)
    const double freq45 = 440.0 * std::pow (2.0, (45.0 - 69.0) / 12.0);
    const double freq60 = 440.0 * std::pow (2.0, (60.0 - 69.0) / 12.0);

    std::vector<ExpectedNote> notes = {
        { "n045_v064.wav", 45, 64,  0.20, freq45 },
        { "n045_v127.wav", 45, 127, 1.70, freq45 },
        { "n060_v064.wav", 60, 64,  3.20, freq60 },
        { "n060_v127.wav", 60, 127, 4.70, freq60 },
    };
    const double holdSeconds = 1.0; // igual ao GateBFixtureTest
    const double endOfTrackSeconds = 6.30;

    // ---- 1. Gerar o MIDI de teste (mesmo canal/nota/velocity do fixture) ----
    const int ticksPerQuarterNote = 960;
    const double ticksPerSecond = ticksPerQuarterNote * (120.0 / 60.0); // 120 BPM fixo

    juce::MidiMessageSequence track;
    track.addEvent (juce::MidiMessage::tempoMetaEvent (500000).withTimeStamp (0.0));

    for (const auto& n : notes)
    {
        const double onTicks = n.onTimeSeconds * ticksPerSecond;
        const double offTicks = (n.onTimeSeconds + holdSeconds) * ticksPerSecond;
        track.addEvent (juce::MidiMessage::noteOn (1, n.midiNote, (juce::uint8) n.velocity).withTimeStamp (onTicks));
        track.addEvent (juce::MidiMessage::noteOff (1, n.midiNote).withTimeStamp (offTicks));
    }
    track.addEvent (juce::MidiMessage::endOfTrack().withTimeStamp (endOfTrackSeconds * ticksPerSecond));
    track.updateMatchedPairs();

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote (ticksPerQuarterNote);
    midiFile.addTrack (track);

    const juce::File midiPath = juce::File::getCurrentWorkingDirectory().getChildFile ("export/sfz/validation.mid");
    midiPath.getParentDirectory().createDirectory();
    {
        juce::FileOutputStream out (midiPath);
        if (! out.openedOk() || ! midiFile.writeTo (out))
        {
            std::cerr << "[FALHA] Nao foi possivel escrever o MIDI de teste." << std::endl;
            return 1;
        }
    }
    std::cout << "[OK] MIDI de teste gerado: " << midiPath.getFullPathName().toStdString() << std::endl;

    // ---- 2. Renderizar via sfizz_render.exe (engine SFZ independente do K2M) ----
    // argv[2], se presente, e extra flags para o sfizz_render (ex.: "-q 10"), uteis
    // so para diagnostico manual; nao usado no fluxo normal do teste.
    const juce::File renderedWav = sfzFile.getSiblingFile (sfzFile.getFileNameWithoutExtension() + "_rendered.wav");
    const double sampleRate = 44100.0;

    juce::StringArray args;
    args.add (sfizzRenderExe.getFullPathName());
    args.add ("--sfz");   args.add (sfzFile.getFullPathName());
    args.add ("--midi");  args.add (midiPath.getFullPathName());
    args.add ("--wav");   args.add (renderedWav.getFullPathName());
    args.add ("-s");      args.add (juce::String ((int) sampleRate));
    args.add ("-b");      args.add ("64");
    args.add ("--use-eot");
    args.add ("-v");
    if (argc > 2)
        args.addTokens (juce::String (argv[2]), true);

    std::cout << "[INFO] Executando sfizz_render..." << std::endl;
    juce::ChildProcess process;
    if (! process.start (args))
    {
        std::cerr << "[FALHA] Nao foi possivel iniciar sfizz_render.exe." << std::endl;
        return 1;
    }
    const auto output = process.readAllProcessOutput();
    process.waitForProcessToFinish (30000);
    const auto exitCode = process.getExitCode();

    std::cout << "----- saida do sfizz_render -----" << std::endl;
    std::cout << output.toStdString() << std::endl;
    std::cout << "----------------------------------" << std::endl;

    if (exitCode != 0 || ! renderedWav.existsAsFile())
    {
        std::cerr << "[FALHA] sfizz_render terminou com codigo " << exitCode
                   << " ou nao gerou " << renderedWav.getFullPathName().toStdString() << std::endl;
        return 1;
    }
    std::cout << "[OK] Render concluido: " << renderedWav.getFullPathName().toStdString() << std::endl;

    // ---- 3. Comparar cada nota renderizada contra o WAV de origem correspondente ----
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatReader> renderedReader (
        wavFormat.createReaderFor (renderedWav.createInputStream().release(), true));

    if (renderedReader == nullptr)
    {
        std::cerr << "[FALHA] Nao foi possivel reabrir o WAV renderizado." << std::endl;
        return 1;
    }

    juce::AudioBuffer<float> renderedBuffer ((int) renderedReader->numChannels, (int) renderedReader->lengthInSamples);
    renderedReader->read (&renderedBuffer, 0, (int) renderedReader->lengthInSamples, 0, true, true);

    const juce::File samplesDir = juce::File::getCurrentWorkingDirectory().getChildFile ("export/sfz/samples");

    bool allPassed = true;
    std::vector<double> gainRatiosDb; // por regiao, na mesma ordem de "notes"
    // O GateBFixtureTest grava cada WAV com preRoll de 0.10s ANTES do proprio Note On
    // embutido no arquivo (captureOneJob). O sfz toca o arquivo inteiro a partir do
    // frame 0 quando a tecla e pressionada, entao o trecho realmente sustentado (nota
    // em hold) comeca em preRollFrames tanto na origem (frame 0 do arquivo) quanto no
    // render (a partir do instante do Note On do MIDI de teste).
    const int embeddedPreRollFrames = (int) (0.10 * sampleRate);
    const int marginSamples = (int) (0.06 * sampleRate);   // pula a rampa de ataque (~60ms apos o Note On real)
    const int windowSamples = (int) (0.40 * sampleRate);   // janela de comparacao (~400ms), dentro do hold

    for (const auto& n : notes)
    {
        std::cout << "[CHECK] Nota MIDI " << n.midiNote << " vel " << n.velocity
                   << " (" << n.sampleFileName.toStdString() << ")" << std::endl;

        const juce::File sourceFile = samplesDir.getChildFile (n.sampleFileName);
        std::unique_ptr<juce::AudioFormatReader> sourceReader (
            wavFormat.createReaderFor (sourceFile.createInputStream().release(), true));

        if (sourceReader == nullptr)
        {
            std::cerr << "  [FALHA] Nao foi possivel abrir o WAV de origem: "
                       << sourceFile.getFullPathName().toStdString() << std::endl;
            allPassed = false;
            continue;
        }

        juce::AudioBuffer<float> sourceBuffer ((int) sourceReader->numChannels, (int) sourceReader->lengthInSamples);
        sourceReader->read (&sourceBuffer, 0, (int) sourceReader->lengthInSamples, 0, true, true);

        // O sfz toca o sample inteiro a partir do frame 0 quando a tecla e pressionada
        // (sem opcode "offset"), entao a janela de comparacao no arquivo de origem
        // tambem comeca no frame 0, e no render comeca no instante do Note On agendado.
        const int renderOnFrame = (int) std::lround (n.onTimeSeconds * sampleRate);
        const int renderStart = renderOnFrame + embeddedPreRollFrames + marginSamples;
        const int sourceStart = embeddedPreRollFrames + marginSamples;

        if (renderStart + windowSamples > renderedBuffer.getNumSamples()
            || sourceStart + windowSamples > sourceBuffer.getNumSamples())
        {
            std::cerr << "  [FALHA] Janela de comparacao fora dos limites do buffer." << std::endl;
            allPassed = false;
            continue;
        }

        const float* renderedData = renderedBuffer.getReadPointer (0, renderStart);
        const float* sourceData = sourceBuffer.getReadPointer (0, sourceStart);

        const double renderedRmsDb = computeRmsDb (renderedData, windowSamples);
        const double sourceRmsDb = computeRmsDb (sourceData, windowSamples);
        const double rmsDeltaDb = renderedRmsDb - sourceRmsDb;

        const double renderedFreq = estimateFrequencyHz (renderedData, windowSamples, sampleRate);
        const double sourceFreq = estimateFrequencyHz (sourceData, windowSamples, sampleRate);
        const double freqErrorHz = std::abs (renderedFreq - n.expectedFreqHz);

        std::cout << "  RMS origem=" << sourceRmsDb << " dBFS | RMS render=" << renderedRmsDb
                   << " dBFS | delta=" << rmsDeltaDb << " dB" << std::endl;
        std::cout << "  Freq esperada=" << n.expectedFreqHz << " Hz | Freq medida na ORIGEM="
                   << sourceFreq << " Hz | Freq medida no render=" << renderedFreq
                   << " Hz | erro=" << freqErrorHz << " Hz" << std::endl;

        // Erro de frequencia > 2 Hz indicaria mapeamento de tecla errado (arquivo/
        // pitch_keycenter trocado entre as regioes, ou keyrange resolvendo a
        // amostra errada para a nota tocada).
        const bool freqOk = freqErrorHz <= 2.0;
        std::cout << "  [" << (freqOk ? "OK" : "FALHA") << "] Pitch dentro da tolerancia (sample/keyrange corretos)" << std::endl;
        if (! freqOk)
            allPassed = false;

        // O "delta" absoluto de RMS nao e comparavel 1:1 com a origem: o sfizz aplica
        // seu proprio ganho/headroom padrao (independente de nota/velocity, ja medido
        // como uma constante fixa em bateria de diagnostico manual). O que importa
        // para validar o Mapper/SfzExporter e que essa constante seja A MESMA em
        // todas as 4 regioes - se nao for, ha uma curva de velocity ou pitch indevida
        // sendo aplicada por regiao (ex.: amp_veltrack != 0 tratando v64 diferente de
        // v127, ou ganho por nota diferente entre as duas raizes).
        gainRatiosDb.push_back (rmsDeltaDb);
    }

    if (! gainRatiosDb.empty())
    {
        const double referenceDb = gainRatiosDb.front();
        std::cout << "------------------------------------------------------------" << std::endl;
        std::cout << "[CHECK] Consistencia do ganho do player entre as 4 regioes" << std::endl;
        std::cout << "  (deve ser a MESMA constante em todas - senao ha curva de" << std::endl;
        std::cout << "  velocity/pitch indevida por regiao no SFZ gerado)" << std::endl;
        for (size_t i = 0; i < gainRatiosDb.size(); ++i)
        {
            const double diff = gainRatiosDb[i] - referenceDb;
            const bool consistent = std::abs (diff) <= 0.3; // tolerancia apertada: e o mesmo player/config
            std::cout << "  Regiao " << (int) i << ": delta=" << gainRatiosDb[i]
                       << " dB (desvio da referencia=" << diff << " dB) ["
                       << (consistent ? "OK" : "FALHA") << "]" << std::endl;
            if (! consistent)
                allPassed = false;
        }
    }

    std::cout << "============================================================" << std::endl;
    if (allPassed)
    {
        std::cout << "[SUCESSO] O sfizz (engine SFZ independente) reproduziu as 4 regioes" << std::endl;
        std::cout << "com pitch correto e o MESMO ganho relativo entre notas/velocities -" << std::endl;
        std::cout << "ou seja, sem curva de velocity/pitch indevida por regiao. A rota" << std::endl;
        std::cout << "'K2M -> SFZ -> player real' esta validada localmente. Import fisico" << std::endl;
        std::cout << "no MODX M continua pendente." << std::endl;
    }
    else
    {
        std::cout << "[FALHA] Uma ou mais regioes nao bateram com a origem. Revisar Mapper/SfzExporter." << std::endl;
    }
    std::cout << "============================================================" << std::endl;

    return allPassed ? 0 : 1;
}
