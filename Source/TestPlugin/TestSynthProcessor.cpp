#include "TestSynthProcessor.h"
#include <cmath>

TestSynthProcessor::TestSynthProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void TestSynthProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    currentPhase = 0.0;
    phaseDelta = 0.0;
    currentLevel = 0.0f;
    targetLevel = 0.0f;
    noteIsOn = false;
}

void TestSynthProcessor::releaseResources()
{
}

bool TestSynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (! layouts.inputBuses.isEmpty())
        return false;

    return true;
}

void TestSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            const double freq = 440.0 * std::pow (2.0, (msg.getNoteNumber() - 69.0) / 12.0);
            phaseDelta = (freq * 2.0 * juce::MathConstants<double>::pi) / currentSampleRate;
            targetLevel = msg.getFloatVelocity() * 0.7f;
            noteIsOn = true;
        }
        else if (msg.isNoteOff())
        {
            noteIsOn = false;
            targetLevel = 0.0f;
        }
    }

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    const float attackRate = 1.0f / (float) (0.01 * currentSampleRate);  // 10ms ataque
    const float releaseRate = 1.0f / (float) (0.8 * currentSampleRate);  // 800ms release natural

    for (int i = 0; i < numSamples; ++i)
    {
        if (noteIsOn)
        {
            if (currentLevel < targetLevel)
                currentLevel = std::min (targetLevel, currentLevel + attackRate);
        }
        else
        {
            if (currentLevel > 0.0f)
                currentLevel = std::max (0.0f, currentLevel - releaseRate);
        }

        const float sample = (float) std::sin (currentPhase) * currentLevel;
        currentPhase += phaseDelta;
        if (currentPhase >= 2.0 * juce::MathConstants<double>::pi)
            currentPhase -= 2.0 * juce::MathConstants<double>::pi;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample (ch, i, sample);
    }
}

class TestSynthEditor final : public juce::AudioProcessorEditor
{
public:
    explicit TestSynthEditor (TestSynthProcessor& p) : AudioProcessorEditor (p)
    {
        setSize (400, 250);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff202028));
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        g.drawText ("K2M Controlled Test Synthesizer", 0, 30, getWidth(), 30, juce::Justification::centred);

        g.setFont (juce::FontOptions (14.0f));
        g.setColour (juce::Colours::lightgreen);
        g.drawText ("Status: Ativo | Responde a MIDI Note On/Off", 0, 70, getWidth(), 25, juce::Justification::centred);

        g.setColour (juce::Colours::grey);
        g.drawText ("Gera onda senoidal pura calibrada com envelope ADSR", 0, 110, getWidth(), 25, juce::Justification::centred);
        g.drawText ("Usado para aprovação do Gate A e diagnósticos", 0, 135, getWidth(), 25, juce::Justification::centred);
    }
};

juce::AudioProcessorEditor* TestSynthProcessor::createEditor()
{
    return new TestSynthEditor (*this);
}

bool TestSynthProcessor::hasEditor() const
{
    return true;
}

// Export factory for JUCE plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TestSynthProcessor();
}
