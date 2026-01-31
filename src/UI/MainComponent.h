/*
  ==============================================================================

    MainComponent.h
    Playlisted2

  ==============================================================================
*/

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../AudioEngine.h"
#include "../IOSettingsManager.h"
#include "HeaderBar.h"
#include "MediaPage.h"
#include "StyledSlider.h" 

class MainComponent : public juce::Component
{
public:
    MainComponent(AudioEngine& engine, IOSettingsManager& settings);
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    AudioEngine& audioEngine;
    IOSettingsManager& ioSettingsManager;
    
    std::unique_ptr<GoldenSliderLookAndFeel> goldenLookAndFeel;
    HeaderBar header;
    std::unique_ptr<MediaPage> mediaPage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};