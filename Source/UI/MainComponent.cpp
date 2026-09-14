#include "MainComponent.h"

MainComponent::MainComponent()
{
    titleLabel.setText ("K2M", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Kontakt VST3 to Yamaha MODX M Autosampler (Phase 0 Skeleton)", juce::dontSendNotification);
    subtitleLabel.setFont (juce::FontOptions (14.0f));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (subtitleLabel);

    statusLabel.setText ("Ambiente: JUCE 8.0.12 | C++20 | Host VST3 pronto", juce::dontSendNotification);
    statusLabel.setFont (juce::FontOptions (13.0f));
    statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff4caf50));
    addAndMakeVisible (statusLabel);

    logViewer.setMultiLine (true);
    logViewer.setReadOnly (true);
    logViewer.setCaretVisible (false);
    logViewer.setScrollbarsShown (true);
    logViewer.setText ("=== K2M Inicializado ===\r\n"
                       "[OK] Framework: JUCE 8.0.12\r\n"
                       "[OK] Padrão C++: C++20\r\n"
                       "[OK] Host VST3: Habilitado\r\n"
                       "[INFO] Próximo passo: Fase 1 (Varredura e hosting do Kontakt VST3).\r\n");
    addAndMakeVisible (logViewer);

    setSize (720, 480);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e24));
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds().reduced (20);

    titleLabel.setBounds (bounds.removeFromTop (35));
    subtitleLabel.setBounds (bounds.removeFromTop (25));
    statusLabel.setBounds (bounds.removeFromTop (25));
    bounds.removeFromTop (10);

    logViewer.setBounds (bounds);
}
