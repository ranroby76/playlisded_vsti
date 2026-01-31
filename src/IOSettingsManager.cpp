/*
  ==============================================================================

    IOSettingsManager.cpp
    Playlisted2

  ==============================================================================
*/

#include "IOSettingsManager.h"
#include "AppLogger.h"
#include <juce_core/juce_core.h>

IOSettingsManager::IOSettingsManager()
{
    lastMediaFolder = juce::File::getSpecialLocation(juce::File::userMusicDirectory).getFullPathName();
    lastPlaylistFolder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getFullPathName();
    lastMidiDevice = "";
}

void IOSettingsManager::saveMediaFolder(const juce::String& path) { lastMediaFolder = path; saveToFile(); }
void IOSettingsManager::savePlaylistFolder(const juce::String& path) { lastPlaylistFolder = path; saveToFile(); }
void IOSettingsManager::saveMidiDevice(const juce::String& deviceName) { lastMidiDevice = deviceName; saveToFile(); }

bool IOSettingsManager::loadSettings()
{
    auto file = getSettingsFile();
    if (!file.existsAsFile()) {
        LOG_INFO("IOSettingsManager: Settings file not found at " + file.getFullPathName());
        return false;
    }
    
    LOG_INFO("IOSettingsManager: Loading settings from " + file.getFullPathName());
    try {
        auto json = juce::JSON::parse(file);
        if (auto* obj = json.getDynamicObject())
        {
            if (obj->hasProperty("mediaFolder")) lastMediaFolder = obj->getProperty("mediaFolder").toString();
            if (obj->hasProperty("playlistFolder")) lastPlaylistFolder = obj->getProperty("playlistFolder").toString();
            if (obj->hasProperty("midiDevice")) lastMidiDevice = obj->getProperty("midiDevice").toString();

            LOG_INFO("IOSettingsManager: Load Complete.");
            return true;
        }
    }
    catch (...) {
        LOG_ERROR("IOSettingsManager: Failed to parse JSON");
    }
    return false;
}

bool IOSettingsManager::hasExistingSettings() const { return getSettingsFile().existsAsFile(); }

juce::File IOSettingsManager::getSettingsFile() const {
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto pluginDir = appDataDir.getChildFile("Playlisted");
    if (!pluginDir.exists()) pluginDir.createDirectory();
    return pluginDir.getChildFile("plugin_settings.json");
}

bool IOSettingsManager::saveToFile()
{
    auto file = getSettingsFile();
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    
    obj->setProperty("mediaFolder", lastMediaFolder);
    obj->setProperty("playlistFolder", lastPlaylistFolder);
    obj->setProperty("midiDevice", lastMidiDevice);
    
    file.replaceWithText(juce::JSON::toString(juce::var(obj.get())));
    return true;
}