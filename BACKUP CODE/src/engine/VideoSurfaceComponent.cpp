/*
  ==============================================================================

    VideoSurfaceComponent.cpp
    Playlisted2

    Implementation of the placeholder video component for the VST editor.
    Since the actual video plays in a separate process/window, this component
    serves as a status indicator or black backdrop in the UI.

  ==============================================================================
*/

#include "VideoSurfaceComponent.h"
#include "../AudioEngine.h"

VideoSurfaceComponent::VideoSurfaceComponent(AudioEngine& engine)
    : audioEngine(engine)
{
    // Start a timer to update UI if necessary (e.g. blinking rec light or status)
    startTimer(40); // 25fps update rate
}

VideoSurfaceComponent::~VideoSurfaceComponent()
{
    stopTimer();
}

void VideoSurfaceComponent::paint(juce::Graphics& g)
{
    // Draw a black background to represent the video area
    g.fillAll(juce::Colours::black);

    // Optional: Draw text indicating video is running externally
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(14.0f);
    g.drawText("Video Output (External Window)", getLocalBounds(), juce::Justification::centred, true);
    
    // Draw a border
    g.setColour(juce::Colour(0xFF404040));
    g.drawRect(getLocalBounds(), 1);
}

void VideoSurfaceComponent::resized()
{
    // Layout logic if you add child components later
}

void VideoSurfaceComponent::timerCallback()
{
    // Repaint to keep UI responsive or animate if needed
    // For a static placeholder, this could be removed, but we keep it to match the header's inheritance of Timer.
    repaint();
}