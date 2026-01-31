/*
  ==============================================================================

    HeaderBar.h
    Playlisted2

  ==============================================================================
*/

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "../AudioEngine.h"

class HeaderBar : public juce::Component, private juce::Timer
{
public:
    HeaderBar(AudioEngine& engine);
    ~HeaderBar() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
private:
    void timerCallback() override;

    AudioEngine& audioEngine;
    juce::Image fananLogo;
    juce::Image onStageLogo; // The product logo

    juce::TextButton manualButton;
    juce::TextButton registerButton;
    juce::Label modeLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeaderBar)
};