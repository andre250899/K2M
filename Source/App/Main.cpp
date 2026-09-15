#include "MainComponent.h"
#include "../Plugin/PluginScanWorker.h"

class K2MApplication final : public juce::JUCEApplication
{
public:
    K2MApplication() = default;

    const juce::String getApplicationName() override       { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override    { return JUCE_APPLICATION_VERSION_STRING; }

    // Normalmente só uma instância do K2M é permitida. Exceção: o worker de scan isolado (ver
    // PluginHost::scanSearchPath) relança este mesmo .exe com um marcador na linha de comando — sem
    // essa exceção, o worker seria tratado como "segunda instância", encerrado antes de chegar em
    // initialise(), e o coordinator ficaria esperando para sempre uma resposta que nunca chega.
    bool moreThanOneInstanceAllowed() override
    {
        return k2m::commandLineIsScanWorker (getCommandLineParameters());
    }

    void initialise (const juce::String& commandLine) override
    {
        // Se este processo foi lançado como worker de scan isolado (ver PluginHost/PluginScanWorker),
        // ele só existe para escanear um plugin e responder — não deve abrir a janela do K2M.
        if (k2m::runPluginScanWorkerIfRequested (commandLine))
        {
            quit();
            return;
        }

        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        juce::ignoreUnused (commandLine);
    }

    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (juce::String name)
            : DocumentWindow (name,
                              juce::Colour (0xff1e1e24),
                              allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (K2MApplication)
