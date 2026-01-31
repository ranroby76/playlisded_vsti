// **Fix:** Removed references to `VLCMediaPlayer` and simplified it, since the video is now handled by the external window. **File Location: D:\\Workspace\\onstage\_with\_chatgpt\_10\\src\\engine\\VideoSurfaceComponent.h**

/*
  ==============================================================================

    VideoSurfaceComponent.h
    Playlisted2

    For the VST3: This is just a placeholder black screen because the 
    actual video runs in the external "PiP" Engine window.

  ==============================================================================
*/

#ifndef ONSTAGE_ENGINE_VIDEO_SURFACE_COMPONENT_H
#define ONSTAGE_ENGINE_VIDEO_SURFACE_COMPONENT_H

#include <juce_gui_basics/juce_gui_basics.h>

// Forward Declaration
class AudioEngine;

class VideoSurfaceComponent : public juce::Component, private juce::Timer
{
public:
    VideoSurfaceComponent(AudioEngine& engine);
    ~VideoSurfaceComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    
    AudioEngine& audioEngine;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VideoSurfaceComponent)
};
#endif