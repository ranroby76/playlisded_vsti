/*
  ==============================================================================

    PluginEditor.cpp
    Playlisted2

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

PlaylistedProcessorEditor::PlaylistedProcessorEditor (PlaylistedAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Pass references from the Processor to the MainComponent
    mainComponent = std::make_unique<MainComponent>(p.getAudioEngine(), p.getSettings());
    addAndMakeVisible(mainComponent.get());

    setResizable(true, true);
    setResizeLimits(800, 500, 1920, 1080);
    setSize(1000, 700);
}

PlaylistedProcessorEditor::~PlaylistedProcessorEditor()
{
}

void PlaylistedProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF202020));
}

void PlaylistedProcessorEditor::resized()
{
    if (mainComponent)
        mainComponent->setBounds(getLocalBounds());
}