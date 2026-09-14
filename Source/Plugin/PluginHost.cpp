#include "PluginHost.h"

namespace k2m
{

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
PluginHost::PluginHost()
{
    juce::addDefaultFormatsToManager (formatManager);
}

PluginHost::~PluginHost()
{
    unloadPlugin();
}

juce::Array<juce::PluginDescription> PluginHost::scanDefaultVst3Directory()
{
   #if JUCE_WINDOWS
    const juce::File defaultDir ("C:\\Program Files\\Common Files\\VST3");
   #elif JUCE_MAC
    const juce::File defaultDir ("/Library/Audio/Plug-Ins/VST3");
   #else
    const juce::File defaultDir ("/usr/lib/vst3");
   #endif

    return scanDirectory (defaultDir, true);
}

juce::Array<juce::PluginDescription> PluginHost::scanDirectory (const juce::File& dir, bool recursive)
{
    juce::Array<juce::PluginDescription> results;

    if (! dir.isDirectory())
        return results;

    auto files = dir.findChildFiles (juce::File::findFilesAndDirectories,
                                     recursive,
                                     "*.vst3");

    for (const auto& file : files)
    {
        for (int i = 0; i < formatManager.getNumFormats(); ++i)
        {
            if (auto* format = formatManager.getFormat (i))
            {
                if (format->fileMightContainThisPluginType (file.getFullPathName()))
                {
                    juce::OwnedArray<juce::PluginDescription> descriptions;
                    format->findAllTypesForFile (descriptions, file.getFullPathName());

                    for (auto* desc : descriptions)
                    {
                        if (desc != nullptr)
                            results.add (*desc);
                    }
                }
            }
        }
    }

    return results;
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
