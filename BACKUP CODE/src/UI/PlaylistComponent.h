#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "PlaylistDataStructures.h"
#include "TrackBannerComponent.h"
#include "../AudioEngine.h"
#include "../IOSettingsManager.h" 

class PlaylistListContainer : public juce::Component
{
public:
    PlaylistListContainer() { setOpaque(true); }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xFF222222)); }
};

class PlaylistComponent : public juce::Component, private juce::Timer
{
public:
    PlaylistComponent(AudioEngine& engine, IOSettingsManager& settings);
    ~PlaylistComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void addTrack(const juce::File& file);
    void playTrack(int index);
    void removeTrack(int index);
    void selectTrack(int index);
    void clearPlaylist();
    // Returns remaining wait time in seconds (or 0 if not waiting)
    int getWaitSecondsRemaining() const {
        if (!waitingForTransition) return 0;
        return (int)std::ceil(transitionCountdown / 30.0f);
    }

private:
    void timerCallback() override;
    void rebuildList();
    void updateBannerVisuals();
    void scrollToBanner(int index);

    void savePlaylist();
    void loadPlaylist();
    void setDefaultFolder();

    AudioEngine& audioEngine;
    IOSettingsManager& ioSettings;
    
    std::vector<PlaylistItem>& playlist; 

    int currentTrackIndex = -1;
    bool autoPlayEnabled = true;
    bool waitingForTransition = false;
    int transitionCountdown = 0;
    
    // Debounce counter for detecting track finish to prevent "Skip on Speed Change"
    int finishDebounceCounter = 0;

    juce::Label headerLabel;
    juce::ToggleButton autoPlayToggle;
    juce::TextButton defaultFolderButton; 
    juce::TextButton addTrackButton;
    juce::TextButton clearButton;
    juce::TextButton saveButton;
    juce::TextButton loadButton;

    juce::Viewport viewport;
    PlaylistListContainer listContainer;
    juce::OwnedArray<TrackBannerComponent> banners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaylistComponent)
};