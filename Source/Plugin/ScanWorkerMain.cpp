// Executável worker dedicado para o scan de plugins isolado por processo (ver PluginScanWorker.h).
//
// K2M.exe é um juce::JUCEApplication completo; relançar o próprio K2M.exe como worker (via
// ChildProcessCoordinator::launchWorkerProcess apontando para currentExecutableFile) se mostrou
// instável nesta máquina: quando o worker crashava ao consultar um plugin de terceiros mal-comportado,
// o processo PAI (a instância real do K2M com a janela aberta) morria alguns segundos depois com uma
// violação de acesso dentro de VCRUNTIME140.dll — plausivelmente uma corrida entre a máquina de
// AsyncUpdater/ping-timeout do ChildProcessCoordinator e a infraestrutura de JUCEApplication/janela
// nativa/single-instance do processo pai.
//
// GateATest.exe e GateBFixtureTest.exe (mains de console simples, sem JUCEApplication) já provaram
// ser um coordinator estável mesmo sobrevivendo a esse mesmo crash de worker. Este executável reproduz
// exatamente esse padrão simples, só para ser usado como alvo do scan pelo K2M.exe — ver
// PluginHost::resolveScanWorkerExecutable().

#include "PluginScanWorker.h"
#include <juce_gui_extra/juce_gui_extra.h>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    juce::String commandLine;
    for (int i = 1; i < argc; ++i)
        commandLine << argv[i] << " ";

    if (k2m::runPluginScanWorkerIfRequested (commandLine))
        return 0;

    // Lançado sem o marcador esperado na linha de comando — não é para ser executado diretamente.
    return 1;
}
