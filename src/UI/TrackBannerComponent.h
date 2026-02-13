#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "PlaylistDataStructures.h"
#include "StyledSlider.h"
#include "LongPressDetector.h"

// ==============================================================================
// PlayTriangleButton (unique to TrackBanner)
// ==============================================================================
class PlayTriangleButton : public juce::Button
{
public:
    PlayTriangleButton() : juce::Button("PlaySelect") {}
    
    void setActive(bool active)
    {
        if (isActive != active)
        {
            isActive = active;
            repaint();
        }
    }
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(shouldDrawButtonAsDown);
        auto bounds = getLocalBounds().toFloat();
        if (isActive) g.setColour(juce::Colour(0xFF335533));
        else g.setColour(juce::Colour(0xFF2A2A2A));
        
        if (shouldDrawButtonAsHighlighted) g.setColour(juce::Colour(0xFF3A3A3A));
        
        g.fillEllipse(bounds.reduced(2));
        
        juce::Path p;
        float s = bounds.getHeight() * 0.4f;
        float cx = bounds.getCentreX() + 2; 
        float cy = bounds.getCentreY();
        p.addTriangle(cx - s/2, cy - s/2, cx - s/2, cy + s/2, cx + s/2, cy);
        g.setColour(isActive ? juce::Colour(0xFF00FF00) : juce::Colour(0xFF008800));
        g.fillPath(p);

        if (isActive)
        {
            g.setColour(juce::Colour(0xFF00FF00));
            g.drawEllipse(bounds.reduced(2), 1.5f);
        }
    }

private:
    bool isActive = false;
};

// ==============================================================================
// TrackBannerComponent
// ==============================================================================
class TrackBannerComponent : public juce::Component, public LongPressDetector
{
public:
    TrackBannerComponent(int index, PlaylistItem& item, 
                         std::function<void()> onRemove,
                         std::function<void()> onExpandToggle,
                         std::function<void()> onSelect,
                         std::function<void(float)> onVolChange,
                         std::function<void(int)> onPitchChange,
                         std::function<void(float)> onSpeedChange);

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void onLongPress() override;

    void setPlaybackState(bool isCurrent, bool isAudioActive);
    bool isExpanded() const { return itemData.isExpanded; }

private:
    int trackIndex;
    PlaylistItem& itemData;
    
    bool isCurrentTrack = false;
    bool isAudioPlaying = false;

    std::function<void()> onRemoveCallback;
    std::function<void()> onExpandToggleCallback;
    std::function<void()> onSelectCallback;
    std::function<void(float)> onVolChangeCallback;
    std::function<void(int)> onPitchChangeCallback; 
    std::function<void(float)> onSpeedChangeCallback;

    juce::Label indexLabel;
    PlayTriangleButton playSelectionButton;
    
    MidiTooltipTextButton removeButton;
    MidiTooltipTextButton expandButton;
    MidiTooltipTextButton crossfadeButton;

    juce::Label volLabel, pitchLabel, speedLabel, delayLabel;
    
    std::unique_ptr<StyledSlider> volSlider;
    std::unique_ptr<StyledSlider> pitchSlider;
    std::unique_ptr<StyledSlider> speedSlider;
    std::unique_ptr<StyledSlider> delaySlider;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackBannerComponent)
};
