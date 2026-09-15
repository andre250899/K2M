#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Plugin/PluginHost.h"
#include "Audio/AudioEngine.h"
#include "Sampling/SamplingPlan.h"

class MainComponent final : public juce::Component,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    void scanPlugins();
    void applyScanResults (const juce::Array<juce::PluginDescription>& plugins);
    void loadSelectedPlugin (const juce::PluginDescription& desc);
    void browseForPluginFile();
    void openAudioSettings();
    void testPlayNote();
    void runGateACapture();

    void appendLog (const juce::String& text);

    // Backend
    k2m::PluginHost pluginHost;
    k2m::AudioEngine audioEngine;

    juce::Array<juce::PluginDescription> discoveredPlugins;
    bool scanInProgress = false;

    // Elementos de UI
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label statusLabel;

    juce::ComboBox pluginSelector;
    juce::TextButton scanButton { "Buscar VST3" };
    juce::TextButton browseButton { "Procurar..." };
    juce::TextButton showEditorButton { "Abrir Kontakt" };
    juce::TextButton audioSettingsButton { "Disp. Áudio" };

    juce::TextButton testNoteButton { "Tocar C3 (MIDI 60)" };
    juce::TextButton gateAButton { "Capturar Gate A (C3_v100.wav)" };

    juce::ProgressBar captureProgressBar;
    double captureProgress = 0.0;

    juce::TextEditor logViewer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
