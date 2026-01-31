/*
  ==============================================================================

    IOSettingsManager.h
    Playlisted2

    VSTi Cleanup: Removed Audio Driver selection (handled by DAW)
    and Vocal Inputs (removed feature).
    Now purely manages Folders and MIDI preferences.

  ==============================================================================
*/

#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

class IOSettingsManager
{
public:
    IOSettingsManager();
    ~IOSettingsManager() = default;

    // Folders
    void saveMediaFolder(const juce::String& path);
    juce::String getMediaFolder() const { return lastMediaFolder; }

    void savePlaylistFolder(const juce::String& path);
    juce::String getPlaylistFolder() const { return lastPlaylistFolder; }

    // MIDI Settings
    void saveMidiDevice(const juce::String& deviceName);
    juce::String getLastMidiDevice() const { return lastMidiDevice; }

    bool loadSettings();
    bool hasExistingSettings() const;

private:
    juce::File getSettingsFile() const;
    bool saveToFile();

    juce::String lastMediaFolder = ""; 
    juce::String lastPlaylistFolder = "";
    juce::String lastMidiDevice = "";
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IOSettingsManager)
};