#include "PluginHost.h"
#include "PluginScanWorker.h"

namespace k2m
{

namespace
{

// Plugins conhecidos por crashar de forma reprodutível ao serem apenas consultados por descrição
// (não ao serem carregados/tocados — trava na fábrica VST3, antes de qualquer uso real). O scan
// isolado por processo já contém o dano a apenas o processo filho, mas o K2M ainda pagava um preço
// (o processo pai (K2M.exe) morria pouco depois com uma violação de acesso em VCRUNTIME140.dll,
// investigado em sessão de depuração mas sem causa raiz 100% confirmada por falta de dump capturado
// a tempo). Excluir esses arquivos do scan de antemão evita acionar o problema.
//   - "Addictive Keys.vst3" (XLN Audio, build 1.6.3/2021): a própria Cotton (framework interno do
//     plugin) não encontra "luasystem,BaseSystem.lua" na instalação e desreferencia um ponteiro nulo
//     em GetPluginFactory logo depois de logar o erro — instalação quebrada/desatualizada do plugin,
//     não um problema do K2M.
bool isKnownBadPluginFile (const juce::String& fileOrIdentifier)
{
    static const char* const knownBadSuffixes[] = { "Addictive Keys.vst3" };

    for (auto* suffix : knownBadSuffixes)
        if (fileOrIdentifier.endsWithIgnoreCase (suffix))
            return true;

    return false;
}

} // namespace

PluginHost::PluginWindow::PluginWindow (juce::AudioPluginInstance& plugin, std::function<void()> onClose)
    : DocumentWindow (plugin.getName(),
                      juce::Colour (0xff252528),
                      allButtons),
      onCloseCallback (std::move (onClose))
{
    setUsingNativeTitleBar (true);

    if (auto* editor = plugin.createEditorIfNeeded())
    {
        setContentOwned (editor, true);
        setResizable (editor->isResizable(), false);
    }
    else
    {
        setSize (400, 200);
    }

    centreWithSize (getWidth(), getHeight());
}

PluginHost::PluginWindow::~PluginWindow()
{
    clearContentComponent();
}

void PluginHost::PluginWindow::closeButtonPressed()
{
    setVisible (false);
    if (onCloseCallback)
        onCloseCallback();
}

//==============================================================================
PluginHost::PluginHost (bool useIsolatedScanning)
{
    juce::addDefaultFormatsToManager (formatManager);

    // Cada arquivo é consultado num processo filho descartável: um plugin de terceiros mal-comportado
    // (ex.: uma fábrica VST3 que lança exceção/crasha ao ser aberta) derruba só esse processo filho,
    // não o host. Ver Source/Plugin/PluginHost.h para o porquê de K2M.exe passar false aqui.
    if (useIsolatedScanning)
        installOutOfProcessScanner (knownPluginList);
}

PluginHost::~PluginHost()
{
    unloadPlugin();
}

juce::Array<juce::PluginDescription> PluginHost::scanDefaultVst3Directory()
{
    juce::FileSearchPath searchPath;

    // 1. Locais padrão do sistema fornecidos pelo JUCE (ex: C:\Program Files\Common Files\VST3)
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        if (auto* format = formatManager.getFormat (i))
            searchPath.addPath (format->getDefaultLocationsToSearch());
    }

    // 2. Diretório do executável K2M e subpastas (ex: VST3/), útil para achar o K2M_TestSynth de build local
    const auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    searchPath.add (exeDir);
    searchPath.add (exeDir.getChildFile ("VST3"));
    searchPath.add (exeDir.getParentDirectory());

    return scanSearchPath (searchPath, true);
}

juce::Array<juce::PluginDescription> PluginHost::scanDirectory (const juce::File& dir, bool recursive)
{
    if (! dir.isDirectory())
        return {};

    juce::FileSearchPath searchPath;
    searchPath.add (dir);
    return scanSearchPath (searchPath, recursive);
}

juce::Array<juce::PluginDescription> PluginHost::scanSearchPath (const juce::FileSearchPath& searchPath, bool recursive)
{
    // Cada arquivo encontrado é consultado num processo filho isolado (ver PluginScanWorker) através do
    // CustomScanner instalado em knownPluginList — se um plugin de terceiros travar/crashar ao ser
    // consultado, só aquele processo filho morre; o scan segue para o próximo arquivo normalmente.
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        if (auto* format = formatManager.getFormat (i))
        {
            juce::PluginDirectoryScanner scanner (knownPluginList, *format, searchPath, recursive, juce::File());

            juce::StringArray filesToScan;
            for (const auto& file : format->searchPathsForPlugins (searchPath, recursive, false))
            {
                if (isKnownBadPluginFile (file))
                    juce::Logger::writeToLog ("[PluginHost] Pulando plugin conhecido por crashar na fábrica: " + file);
                else
                    filesToScan.add (file);
            }
            scanner.setFilesOrIdentifiersToScan (filesToScan);

            juce::String pluginBeingScanned;
            while (scanner.scanNextFile (true, pluginBeingScanned)) { }

            for (const auto& failed : scanner.getFailedFiles())
                juce::Logger::writeToLog ("[PluginHost] Falha ao escanear (isolado): " + failed);
        }
    }

    juce::Array<juce::PluginDescription> allResults;
    for (const auto& desc : knownPluginList.getTypes())
        allResults.add (desc);

    return allResults;
}

void PluginHost::loadPluginAsync (const juce::PluginDescription& desc,
                                  double sampleRate,
                                  int blockSize,
                                  std::function<void (bool, const juce::String&)> onComplete)
{
    unloadPlugin();

    formatManager.createPluginInstanceAsync (
        desc,
        sampleRate,
        blockSize,
        [this, sampleRate, blockSize, onComplete] (std::unique_ptr<juce::AudioPluginInstance> instance,
                                                  const juce::String& error)
        {
            if (instance == nullptr || error.isNotEmpty())
            {
                if (onComplete)
                    onComplete (false, error.isNotEmpty() ? error : "Falha ao instanciar o plugin.");
                return;
            }

            currentInstance = std::move (instance);

            // Configurar layout de buses: estéreo na saída, sem entradas
            auto busLayout = currentInstance->getBusesLayout();
            busLayout.inputBuses.clear();
            if (busLayout.outputBuses.size() > 0)
                busLayout.outputBuses.set (0, juce::AudioChannelSet::stereo());

            currentInstance->setBusesLayout (busLayout);
            currentInstance->prepareToPlay (sampleRate, blockSize);

            if (onComplete)
                onComplete (true, {});
        });
}

void PluginHost::loadPluginFromPathAsync (const juce::File& file,
                                          double sampleRate,
                                          int blockSize,
                                          std::function<void (bool, const juce::String&)> onComplete)
{
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        if (auto* format = formatManager.getFormat (i))
        {
            if (format->fileMightContainThisPluginType (file.getFullPathName()))
            {
                juce::OwnedArray<juce::PluginDescription> descriptions;
                format->findAllTypesForFile (descriptions, file.getFullPathName());

                if (descriptions.size() > 0 && descriptions[0] != nullptr)
                {
                    loadPluginAsync (*descriptions[0], sampleRate, blockSize, onComplete);
                    return;
                }
            }
        }
    }

    if (onComplete)
        onComplete (false, "Nenhum formato compatível encontrado para o arquivo: " + file.getFullPathName());
}

void PluginHost::unloadPlugin()
{
    // Destruir janela e editor primeiro (na message thread)
    hidePluginWindow();
    currentWindow.reset();

    // Em seguida liberar a instância
    if (currentInstance != nullptr)
    {
        currentInstance->releaseResources();
        currentInstance.reset();
    }
}

void PluginHost::showPluginWindow()
{
    if (currentInstance == nullptr)
        return;

    if (currentWindow == nullptr)
    {
        currentWindow = std::make_unique<PluginWindow> (*currentInstance, [this]() {
            currentWindow.reset();
        });
    }

    currentWindow->setVisible (true);
    currentWindow->toFront (true);
}

void PluginHost::hidePluginWindow()
{
    if (currentWindow != nullptr)
        currentWindow->setVisible (false);
}

bool PluginHost::isWindowVisible() const
{
    return currentWindow != nullptr && currentWindow->isVisible();
}

juce::String PluginHost::getLoadedPluginName() const
{
    return currentInstance != nullptr ? currentInstance->getName() : juce::String();
}

} // namespace k2m
