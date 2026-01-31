/*
  ==============================================================================

    AudioEngine.h
    Playlisted2

    Handles:
    - IPC with external Engine process (Video/Decoding)
    - Playlist Management
    - Pitch Shifting (Master Bus)
    
    REMOVED: Vocal Effects and Preset Manager references.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_graphics/juce_graphics.h>
#include "IPC/SharedMemoryManager.h"
#include "UI/PlaylistDataStructures.h"

class RemotePlayerFacade
{
public:
    RemotePlayerFacade(SharedMemoryManager& m) : ipc(m) {}

    bool isPlaying() const { return status.playing; }
    bool hasFinished() const { return status.finished; } 
    // Window status
    bool isWindowOpen() const { return status.winOpen; }
    
    float getPosition() const { return status.pos; }
    int64_t getLengthMs() const { return status.len; }

    juce::Image getCurrentVideoFrame() { return juce::Image(); }

    void play()  { if (ipc.isConnected()) send("play"); }
    void pause() { if (ipc.isConnected()) send("pause"); }
    void stop()  { if (ipc.isConnected()) send("stop"); }
    void setVolume(float v) { if (ipc.isConnected()) send("volume", "val", v); }
    void setRate(float r)   { if (ipc.isConnected()) send("rate", "val", r); }
    void setPosition(float p) { if (ipc.isConnected()) send("seek", "pos", p); }

    void updateStatus() { if (ipc.isConnected()) status = ipc.getEngineStatus(); }
    
    bool loadFile(const juce::String& path) {
        if (!ipc.isConnected()) return false;
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty("type", "load");
        o->setProperty("path", path);
        o->setProperty("vol", 1.0f);
        o->setProperty("speed", 1.0f);
        ipc.sendCommand(juce::JSON::toString(juce::var(o.get())));
        return true;
    }

private:
    SharedMemoryManager& ipc;
    SharedMemoryManager::EngineStatus status;

    void send(const juce::String& type) {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty("type", type);
        ipc.sendCommand(juce::JSON::toString(juce::var(o.get())));
    }
    
    void send(const juce::String& type, const juce::String& key, float val) {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty("type", type);
        o->setProperty(key, val);
        ipc.sendCommand(juce::JSON::toString(juce::var(o.get())));
    }
};

class AudioEngine : private juce::Timer
{
public:
    AudioEngine();
    ~AudioEngine();
    void prepareToPlay(double sampleRate, int samplesPerBlockExpected);
    void releaseResources();
    void processPluginBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);
    void stopAllPlayback();
    
    RemotePlayerFacade& getMediaPlayer() { return *remotePlayer; }
    std::vector<PlaylistItem>& getPlaylist() { return playlist; }
    juce::AudioFormatManager& getFormatManager() { return formatManager; }
    
    void updateCrossfadeState();
    void showVideoWindow();

    // Pitch Control (Master)
    void setPitchSemitones(int semitones);

    // Persistent Track Index Accessors
    int getActiveTrackIndex() const { return activeTrackIndex; }
    void setActiveTrackIndex(int i) { activeTrackIndex = i; }
    
    juce::XmlElement* getStateXml();
    void setStateXml(const juce::XmlElement* xml);
    
private:
    void launchEngine();
    void terminateEngine();
    void handleMidi(juce::MidiBuffer& midiMessages);
    void timerCallback() override;

    // --- Pitch Shifter DSP ---
    void processPitchShift(juce::AudioBuffer<float>& buffer);
    juce::AudioBuffer<float> pitchDelayBuffer;
    int pitchWritePos = 0;
    float pitchReadPos = 0.0f;
    int pitchWindowSize = 4096;
    int currentPitchSemitones = 0;
    float currentPitchFactor = 1.0f;
    float pitchCrossfade = 0.0f;

    juce::AudioFormatManager formatManager;
    juce::AudioBuffer<float> ipcBuffer;
    SharedMemoryManager ipc { SharedMemoryManager::Mode::Plugin_Client };
    std::unique_ptr<RemotePlayerFacade> remotePlayer;
    juce::ChildProcess engineProcess;
    
    std::vector<PlaylistItem> playlist;
    int startupRetries = 0;

    // Store the active track index here so it survives UI close/open
    int activeTrackIndex = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};