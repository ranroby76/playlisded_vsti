#include "PluginProcessor.h"
#include "PluginEditor.h"

PlaylistedAudioProcessor::PlaylistedAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
#endif
{
}

PlaylistedAudioProcessor::~PlaylistedAudioProcessor() {}

const juce::String PlaylistedAudioProcessor::getName() const { return "Playlisted"; }
bool PlaylistedAudioProcessor::acceptsMidi() const { return true; }
bool PlaylistedAudioProcessor::producesMidi() const { return false; }
bool PlaylistedAudioProcessor::isMidiEffect() const { return false; }
double PlaylistedAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int PlaylistedAudioProcessor::getNumPrograms() { return 1; }
int PlaylistedAudioProcessor::getCurrentProgram() { return 0; }
void PlaylistedAudioProcessor::setCurrentProgram (int index) {}
const juce::String PlaylistedAudioProcessor::getProgramName (int index) { return {}; }
void PlaylistedAudioProcessor::changeProgramName (int index, const juce::String& newName) {}

void PlaylistedAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    audioEngine.prepareToPlay(sampleRate, samplesPerBlock);
}

void PlaylistedAudioProcessor::releaseResources()
{
    audioEngine.releaseResources();
}

bool PlaylistedAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void PlaylistedAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    audioEngine.processPluginBlock(buffer, midiMessages);
}

bool PlaylistedAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* PlaylistedAudioProcessor::createEditor() { 
    return new PlaylistedProcessorEditor (*this);
}

void PlaylistedAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    std::unique_ptr<juce::XmlElement> xml(audioEngine.getStateXml());
    copyXmlToBinary(*xml, destData);
}

void PlaylistedAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName("OnStageState") || xmlState->hasTagName("PlaylistedState"))
        {
            audioEngine.setStateXml(xmlState.get());
            if (auto* editor = dynamic_cast<PlaylistedProcessorEditor*>(getActiveEditor()))
            {
                editor->repaint();
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PlaylistedAudioProcessor();
}