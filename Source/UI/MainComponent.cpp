#include "MainComponent.h"

MainComponent::MainComponent()
    // true (default): scan de plugins isolado por processo filho descartável. O crash do K2M.exe
    // logo após o scan tinha outra causa raiz (setSelectedId sem dontSendNotification disparando
    // carregamento automático do Kontakt), já corrigida em applyScanResults() — ver PROGRESS.md.
    : pluginHost (true),
      captureProgressBar (captureProgress)
{
    // Cabeçalho
    titleLabel.setText ("K2M — Kontakt to MODX M Autosampler", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Gate A POC: Hospedagem VST3, agendamento de frames e captura WAV 24-bit", juce::dontSendNotification);
    subtitleLabel.setFont (juce::FontOptions (13.0f));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (subtitleLabel);

    // Controles de Plugin
    pluginSelector.setTextWhenNoChoicesAvailable ("Nenhum VST3 encontrado");
    pluginSelector.setTextWhenNothingSelected ("Selecione o plugin (ex: Kontakt)...");
    pluginSelector.onChange = [this]()
    {
        const int index = pluginSelector.getSelectedItemIndex();
        if (index >= 0 && index < discoveredPlugins.size())
            loadSelectedPlugin (discoveredPlugins[index]);
    };
    addAndMakeVisible (pluginSelector);

    scanButton.onClick = [this]() { scanPlugins(); };
    addAndMakeVisible (scanButton);

    browseButton.onClick = [this]() { browseForPluginFile(); };
    addAndMakeVisible (browseButton);

    showEditorButton.setEnabled (false);
    showEditorButton.onClick = [this]()
    {
        if (pluginHost.isWindowVisible())
            pluginHost.hidePluginWindow();
        else
            pluginHost.showPluginWindow();
    };
    addAndMakeVisible (showEditorButton);

    audioSettingsButton.onClick = [this]() { openAudioSettings(); };
    addAndMakeVisible (audioSettingsButton);

    // Ações de Teste e Captura
    testNoteButton.setEnabled (false);
    testNoteButton.onClick = [this]() { testPlayNote(); };
    addAndMakeVisible (testNoteButton);

    gateAButton.setEnabled (false);
    gateAButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2d7d46));
    gateAButton.onClick = [this]() { runGateACapture(); };
    addAndMakeVisible (gateAButton);

    addAndMakeVisible (captureProgressBar);

    // Status e Logs
    statusLabel.setText ("Dispositivo de Áudio: Inicializando...", juce::dontSendNotification);
    statusLabel.setFont (juce::FontOptions (12.0f));
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::cyan);
    addAndMakeVisible (statusLabel);

    logViewer.setMultiLine (true);
    logViewer.setReadOnly (true);
    logViewer.setCaretVisible (false);
    logViewer.setScrollbarsShown (true);
    logViewer.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    addAndMakeVisible (logViewer);

    // Inicializar dispositivo de áudio
    auto audioErr = audioEngine.initAudio (0, 2);
    if (audioErr.isEmpty())
    {
        auto* dev = audioEngine.getDeviceManager().getCurrentAudioDevice();
        const juce::String devName = (dev != nullptr) ? dev->getName() : juce::String ("Dispositivo Padrão");
        statusLabel.setText (juce::String::formatted ("Áudio: %s | %.0f Hz | Bloco %d",
                                                      devName.toRawUTF8(),
                                                      audioEngine.getSampleRate(),
                                                      audioEngine.getBlockSize()),
                             juce::dontSendNotification);
        appendLog ("[OK] Áudio WASAPI inicializado com sucesso (" + devName + ").");
    }
    else
    {
        statusLabel.setText ("Erro ao abrir dispositivo de áudio: " + audioErr, juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::red);
        appendLog ("[ERRO] Dispositivo de áudio: " + audioErr);
    }

    // scanPlugins() dispara sua própria thread de background e retorna na hora (ver comentário lá).
    scanPlugins();

    startTimerHz (30);
    setSize (780, 560);
}

MainComponent::~MainComponent()
{
    stopTimer();
    audioEngine.setPlugin (nullptr);
    pluginHost.unloadPlugin();
}

void MainComponent::appendLog (const juce::String& text)
{
    logViewer.moveCaretToEnd();
    logViewer.insertTextAtCaret (text + "\r\n");
}

void MainComponent::timerCallback()
{
    if (audioEngine.isCapturing())
    {
        const int64_t total = audioEngine.getTotalCaptureFrames();
        const int64_t current = audioEngine.getCurrentFrameCursor();
        if (total > 0)
            captureProgress = (double) current / (double) total;
        else
            captureProgress = 0.0;
    }
    else
    {
        captureProgress = 0.0;
    }

    if (pluginHost.isLoaded())
    {
        showEditorButton.setButtonText (pluginHost.isWindowVisible() ? "Fechar Janela Kontakt" : "Abrir Interface Kontakt");
    }
}

void MainComponent::scanPlugins()
{
    if (scanInProgress)
        return;

    scanInProgress = true;
    scanButton.setEnabled (false);
    appendLog ("[INFO] Varrendo diretório padrão VST3 (C:\\Program Files\\Common Files\\VST3)...");

    // Síncrono, na thread de mensagens — de propósito. Rodar isso numa thread de background separada
    // (tentado nesta sessão) derrubava o K2M.exe ~12-16s depois com uma violação de acesso em
    // VCRUNTIME140.dll, mesmo com o scan em processo único (sem ChildProcessCoordinator envolvido).
    // Suspeita: algum plugin da pasta (provavelmente Kontakt, que usa bastante COM/Windows APIs) não é
    // seguro para consultar fora da thread que inicializou o app. GateATest/GateBFixtureTest, que
    // escaneiam na própria thread principal deles, nunca reproduziram esse crash.
    applyScanResults (pluginHost.scanDefaultVst3Directory());
}

void MainComponent::applyScanResults (const juce::Array<juce::PluginDescription>& plugins)
{
    discoveredPlugins = plugins;

    pluginSelector.clear();
    int kontaktIndex = -1;

    for (int i = 0; i < discoveredPlugins.size(); ++i)
    {
        const auto& desc = discoveredPlugins[i];
        const auto label = desc.name + " (" + desc.manufacturerName + ")";
        pluginSelector.addItem (label, i + 1);

        if (desc.name.containsIgnoreCase ("Kontakt"))
            kontaktIndex = i;
    }

    appendLog (juce::String::formatted ("[INFO] %d plugins VST3 encontrados.", discoveredPlugins.size()));

    // dontSendNotification: só marcar a seleção visualmente. setSelectedId() sem isso dispara
    // pluginSelector.onChange (o notification type padrão é sendNotificationAsync) — que chama
    // loadSelectedPlugin(), instanciando o plugin de verdade (não só consultando a descrição). Isso
    // fazia o K2M tentar carregar o Kontakt sozinho, sem pedir, logo depois de todo scan concluído —
    // e carregar o Kontakt de verdade crasha nesta máquina (investigar como item separado; não é causa
    // do scan em si). Carregar continua um clique de distância, agora por escolha do usuário.
    if (kontaktIndex >= 0)
    {
        appendLog ("[OK] Kontakt VST3 identificado automaticamente.");
        pluginSelector.setSelectedId (kontaktIndex + 1, juce::dontSendNotification);
    }
    else if (discoveredPlugins.size() > 0)
    {
        pluginSelector.setSelectedId (1, juce::dontSendNotification);
    }

    scanButton.setEnabled (true);
    scanInProgress = false;
}

void MainComponent::browseForPluginFile()
{
    auto chooser = std::make_shared<juce::FileChooser> (
        "Selecione o arquivo .vst3 do Kontakt",
        juce::File ("C:\\Program Files\\Common Files\\VST3"),
        "*.vst3"
    );

    chooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.exists())
            {
                appendLog ("[INFO] Carregando arquivo selecionado: " + file.getFullPathName());
                pluginHost.loadPluginFromPathAsync (
                    file,
                    audioEngine.getSampleRate(),
                    audioEngine.getBlockSize(),
                    [this, file] (bool success, const juce::String& error)
                    {
                        if (success)
                        {
                            audioEngine.setPlugin (pluginHost.getInstance());
                            showEditorButton.setEnabled (true);
                            testNoteButton.setEnabled (true);
                            gateAButton.setEnabled (true);
                            appendLog ("[OK] Plugin carregado: " + file.getFileNameWithoutExtension());
                        }
                        else
                        {
                            appendLog ("[ERRO] Falha ao carregar plugin: " + error);
                        }
                    });
            }
        });
}

void MainComponent::loadSelectedPlugin (const juce::PluginDescription& desc)
{
    appendLog ("[INFO] Instanciando plugin: " + desc.name + "...");
    audioEngine.setPlugin (nullptr);

    pluginHost.loadPluginAsync (
        desc,
        audioEngine.getSampleRate(),
        audioEngine.getBlockSize(),
        [this, desc] (bool success, const juce::String& error)
        {
            if (success)
            {
                audioEngine.setPlugin (pluginHost.getInstance());
                showEditorButton.setEnabled (true);
                testNoteButton.setEnabled (true);
                gateAButton.setEnabled (true);
                appendLog ("[OK] Plugin carregado com sucesso: " + desc.name);
            }
            else
            {
                showEditorButton.setEnabled (false);
                testNoteButton.setEnabled (false);
                gateAButton.setEnabled (false);
                appendLog ("[ERRO] Falha ao carregar " + desc.name + ": " + error);
            }
        });
}

void MainComponent::openAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (
        audioEngine.getDeviceManager(),
        0, 0, 2, 2, false, false, true, false
    );
    selector->setSize (500, 300);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (selector);
    options.dialogTitle = "Configurações de Áudio";
    options.componentToCentreAround = this;
    options.dialogBackgroundColour = juce::Colour (0xff252528);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

    options.launchAsync();
}

void MainComponent::testPlayNote()
{
    if (! pluginHost.isLoaded())
        return;

    appendLog ("[MIDI] Emitindo Note On (C3, v100)...");
    audioEngine.injectMidiMessage (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100));

    juce::Timer::callAfterDelay (1000, [this]() {
        audioEngine.injectMidiMessage (juce::MidiMessage::noteOff (1, 60));
        appendLog ("[MIDI] Emitindo Note Off (C3).");
    });
}

void MainComponent::runGateACapture()
{
    if (! pluginHost.isLoaded())
    {
        appendLog ("[ERRO] Carregue o Kontakt antes de iniciar a captura.");
        return;
    }

    if (audioEngine.isCapturing())
    {
        appendLog ("[WARN] Captura já em andamento.");
        return;
    }

    gateAButton.setEnabled (false);
    testNoteButton.setEnabled (false);

    appendLog ("============================================================");
    appendLog ("[GATE A] Iniciando captura determinística:");
    appendLog ("         Nota: C3 (MIDI 60) | Velocity: 100");
    appendLog ("         Pré-roll: 0.25s | Hold: 5.0s | Release: 2.0s");
    appendLog ("         Duração total do take: 7.25s");

    audioEngine.armGateACapture (
        60, 100, 5.0, 0.25, 2.0,
        [this] (bool success, const juce::AudioBuffer<float>& buffer, double sampleRate)
        {
            gateAButton.setEnabled (true);
            testNoteButton.setEnabled (true);

            if (! success)
            {
                appendLog ("[ERRO] A captura falhou.");
                return;
            }

            // Exportar arquivo WAV
            auto outputDir = juce::File::getCurrentWorkingDirectory().getChildFile ("export");
            auto outputFile = outputDir.getChildFile ("C3_v100.wav");

            const int frames = audioEngine.getCaptureSink().getCapturedFrames();
            const bool saved = k2m::CaptureSink::saveWavFile (outputFile, buffer, frames, sampleRate, 24);

            if (saved)
            {
                appendLog ("[OK] Gate A concluído com sucesso!");
                appendLog ("     Arquivo gerado: " + outputFile.getFullPathName());
                appendLog (juce::String::formatted ("     Frames válidos: %d (%.2f segundos) | 24-bit PCM",
                                                    frames, (double) frames / sampleRate));
            }
            else
            {
                appendLog ("[ERRO] Falha ao gravar arquivo WAV em: " + outputFile.getFullPathName());
            }
            appendLog ("============================================================");
        });
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff18181c));
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds().reduced (20);

    titleLabel.setBounds (bounds.removeFromTop (32));
    subtitleLabel.setBounds (bounds.removeFromTop (22));
    bounds.removeFromTop (12);

    // Linha de seleção do plugin
    auto pluginRow = bounds.removeFromTop (32);
    pluginSelector.setBounds (pluginRow.removeFromLeft (pluginRow.getWidth() - 220));
    pluginRow.removeFromLeft (8);
    scanButton.setBounds (pluginRow.removeFromLeft (100));
    pluginRow.removeFromLeft (8);
    browseButton.setBounds (pluginRow);

    bounds.removeFromTop (10);

    // Linha de botões de ação
    auto actionRow = bounds.removeFromTop (34);
    showEditorButton.setBounds (actionRow.removeFromLeft (160));
    actionRow.removeFromLeft (10);
    audioSettingsButton.setBounds (actionRow.removeFromLeft (110));
    actionRow.removeFromLeft (10);
    testNoteButton.setBounds (actionRow.removeFromLeft (150));
    actionRow.removeFromLeft (10);
    gateAButton.setBounds (actionRow);

    bounds.removeFromTop (10);
    captureProgressBar.setBounds (bounds.removeFromTop (16));

    bounds.removeFromTop (10);
    statusLabel.setBounds (bounds.removeFromTop (22));

    bounds.removeFromTop (8);
    logViewer.setBounds (bounds);
}
