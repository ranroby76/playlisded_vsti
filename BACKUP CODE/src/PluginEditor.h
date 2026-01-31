/*
  ==============================================================================

    PluginEditor.h
    Playlisted2

  ==============================================================================
*/

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/MainComponent.h"

class PlaylistedProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    PlaylistedProcessorEditor (PlaylistedAudioProcessor&);
    ~PlaylistedProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PlaylistedAudioProcessor& audioProcessor;
    std::unique_ptr<MainComponent> mainComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaylistedProcessorEditor)
};