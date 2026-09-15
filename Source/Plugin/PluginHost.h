#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <memory>

namespace k2m
{

class PluginHost
{
public:
    // useIsolatedScanning=true isola cada plugin consultado num processo filho descartável (ver
    // PluginScanWorker.h) — comprovadamente estável nos executáveis de teste em console (GateATest,
    // GateBFixtureTest), mas o processo pai morre pouco depois quando esse mecanismo roda dentro de um
    // juce::JUCEApplication com janela (K2M.exe) — causa não 100% confirmada apesar de investigação,
    // reproduzida mesmo em scans sem nenhum plugin problemático. K2M.exe usa false (scan em processo
    // único, como era antes desse isolamento existir) e depende só da blacklist de PluginHost.cpp
    // (isKnownBadPluginFile) para os plugins que sabidamente crasham na consulta de fábrica.
    explicit PluginHost (bool useIsolatedScanning = true);
    ~PluginHost();

    // Varredura de plugins (cada arquivo é isolado num processo filho, ver PluginScanWorker.h)
    juce::Array<juce::PluginDescription> scanDefaultVst3Directory();
    juce::Array<juce::PluginDescription> scanDirectory (const juce::File& dir, bool recursive = true);

    // Carregamento assíncrono seguro
    void loadPluginAsync (const juce::PluginDescription& desc,
                          double sampleRate,
                          int blockSize,
                          std::function<void (bool success, const juce::String& errorMsg)> onComplete);

    void loadPluginFromPathAsync (const juce::File& file,
                                  double sampleRate,
                                  int blockSize,
                                  std::function<void (bool success, const juce::String& errorMsg)> onComplete);

    // Fechamento e liberação segura de recursos
    void unloadPlugin();

    // Controle da janela do editor nativo
    void showPluginWindow();
    void hidePluginWindow();
    bool isWindowVisible() const;

    // Acesso à instância
    juce::AudioPluginInstance* getInstance() const noexcept { return currentInstance.get(); }
    bool isLoaded() const noexcept { return currentInstance != nullptr; }

    juce::String getLoadedPluginName() const;

private:
    juce::Array<juce::PluginDescription> scanSearchPath (const juce::FileSearchPath& searchPath, bool recursive);

    class PluginWindow final : public juce::DocumentWindow
    {
    public:
        PluginWindow (juce::AudioPluginInstance& plugin, std::function<void()> onClose);
        ~PluginWindow() override;

        void closeButtonPressed() override;

    private:
        std::function<void()> onCloseCallback;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginWindow)
    };

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;
    std::unique_ptr<juce::AudioPluginInstance> currentInstance;
    std::unique_ptr<PluginWindow> currentWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginHost)
};

} // namespace k2m
