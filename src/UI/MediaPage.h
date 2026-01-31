#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../AudioEngine.h"
#include "../IOSettingsManager.h"
#include "PlaylistComponent.h"
#include "StyledSlider.h" 

class MediaPage : public juce::Component, private juce::Timer
{
public:
    MediaPage(AudioEngine& engine, IOSettingsManager& settings);
    ~MediaPage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::String formatTime(double seconds) const;
    AudioEngine& audioEngine;
    
    std::unique_ptr<PlaylistComponent> playlistComponent;
    
    MidiTooltipTextButton playPauseBtn;
    MidiTooltipTextButton stopBtn;
    
    // Changed to unique_ptr for custom button class
    std::unique_ptr<juce::TextButton> showVideoBtn;
    
    StyledSlider progressSlider;
    juce::Label currentTimeLabel;
    juce::Label totalTimeLabel;
    juce::Label countdownLabel;
    
    bool isUserDraggingSlider = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MediaPage)
};