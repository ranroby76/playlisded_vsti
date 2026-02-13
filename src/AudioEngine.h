/*
  ==============================================================================

    AudioEngine.h
    Playlisted2

    FIX: Added cleanupSharedMemory() to remove stale IPC files on shutdown.
    FIX: Added engineExePath for macOS terminate fallback.

  ==============================================================================
*/

#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "IPC/SharedMemoryManager.h"
#include "UI/PlaylistDataStructures.h"

// ==============================================================================
// REMOTE PLAYER FACADE
// ==============================================================================
class RemotePlayerFacade
{
public:
    RemotePlayerFacade(SharedMemoryManager& manager) : ipc(manager) {}

    void loadFile(const juce::String& path, float vol = 1.0f, float rate = 1.0f)
    {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty("type", "load");
        o->setProperty("path", path);
        o->setProperty("vol", vol);
        o->setProperty("speed", rate);
        ipc.sendCommand(juce::JSON::toString(juce::var(o.get())));
    }

    void play()     { send("play"); }
    void pause()    { send("pause"); }
    void stop()     { send("stop"); }

    void setPosition(float pos) { send("seek", "pos", pos); }
    void setVolume(float v)     { send("volume", "val", v); }
    void setRate(float r)       { send("rate", "val", r); }

    void updateStatus()
    {
        status = ipc.getEngineStatus();
    }

    bool isPlaying()   const { return status.playing; }
    bool hasFinished()  const { return status.finished; }
    bool isWindowOpen() const { return status.winOpen; }
    float getPosition() const { return status.pos; }
    int64_t getLengthMs() const { return status.len; }

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
    void cleanupSharedMemory();
    void handleMidi(juce::MidiBuffer& midiMessages);
    void timerCallback() override;
    void sendHeartbeat();

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
    juce::String engineExePath;  // FIX: Store path for macOS terminate fallback
    
    std::vector<PlaylistItem> playlist;
    int startupRetries = 0;

    // Store the active track index here so it survives UI close/open
    int activeTrackIndex = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
