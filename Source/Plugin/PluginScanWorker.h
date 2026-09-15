#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

namespace k2m
{

// Identificador curto usado pelo par coordinator/worker para se reconhecerem na linha de comando
// (ver juce::ChildProcessCoordinator::launchWorkerProcess / juce::ChildProcessWorker::initialiseFromCommandLine).
constexpr const char* kPluginScanWorkerUID = "K2MSCAN01";

// Verifica se o processo atual foi lançado como worker de scan por um ScanCoordinator (installOutOfProcessScanner).
// Se sim, bloqueia rodando o loop de mensagens até o coordinator terminar a conexão, então retorna true —
// o chamador deve encerrar o processo (return 0) imediatamente nesse caso, sem inicializar o resto do app/teste.
bool runPluginScanWorkerIfRequested (const juce::String& commandLine);

// Reconhece a linha de comando de um worker de scan sem tentar conectar — útil em contextos que
// rodam antes do processo decidir se vai virar worker (ex.: JUCEApplication::moreThanOneInstanceAllowed(),
// que roda antes de initialise() e cujo resultado decide se o worker é encerrado por engano como
// "segunda instância" do app principal).
bool commandLineIsScanWorker (const juce::String& commandLine);

// Instala um CustomScanner que isola cada arquivo de plugin escaneado num processo filho descartável.
// Se o plugin travar ou crashar durante a consulta de fábrica, só o processo filho morre — o host
// (K2M.exe ou os executáveis de teste) continua de pé e segue para o próximo arquivo.
//
// O processo filho lançado é, por padrão, "K2M_ScanWorker(.exe)" se ele existir ao lado do executável
// atual; caso contrário, cai de volta para relançar o próprio executável atual (que precisa reconhecer
// o modo worker via runPluginScanWorkerIfRequested, como já fazem K2M.exe/GateATest.exe/
// GateBFixtureTest.exe). Ver ScanWorkerMain.cpp para o motivo de preferir o worker dedicado.
void installOutOfProcessScanner (juce::KnownPluginList& listToConfigure);

} // namespace k2m
