/*
  ==============================================================================

    VLCMediaPlayer_Desktop.h
    Playlisted2

  ==============================================================================
*/

#ifndef ONSTAGE_ENGINE_VLC_MEDIA_PLAYER_DESKTOP_H
#define ONSTAGE_ENGINE_VLC_MEDIA_PLAYER_DESKTOP_H

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h> 

extern "C" {
    #include <vlc/libvlc.h>
    #include <vlc/libvlc_media.h>
    #include <vlc/libvlc_renderer_discoverer.h> 
    #include <vlc/libvlc_media_player.h>
}

class VLCMediaPlayer_Desktop
{
public:
    VLCMediaPlayer_Desktop();
    ~VLCMediaPlayer_Desktop();

    bool prepareToPlay(int samplesPerBlock, double sampleRate);
    void releaseResources();
    bool loadFile(const juce::String& path);
    void play();
    void pause();
    void stop();
    void setVolume(float newVolume);
    float getVolume() const;
    void setRate(float newRate);
    float getRate() const;
    bool hasFinished() const;

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info);
    int getNumAudioSamplesAvailable() const;

    void setWindowHandle(void* handle);
    
    void setVideoEnabled(bool enabled);
    bool isPlaying() const;
    float getPosition() const;
    void setPosition(float pos);
    int64_t getLengthMs() const;
    
    void flushAudioBuffers();

private:
    void ensureInitialized();
    bool isInitialized = false;

    // Audio Callbacks
    static void audioPlay(void* data, const void* samples, unsigned count, int64_t pts);
    static void audioPause(void* data, int64_t pts);
    static void audioResume(void* data, int64_t pts);
    static void audioFlush(void* data, int64_t pts);
    static void audioDrain(void* data);

    void addAudioSamples(const void* samples, unsigned count, int64_t pts);

    libvlc_instance_t* m_instance = nullptr;
    libvlc_media_player_t* m_mediaPlayer = nullptr;
    juce::CriticalSection audioLock;
    
    // FIX: Reduced buffer size from 131072 to 16384 to fix A/V Sync drift
    // 16384 samples @ 44.1k is approx 370ms, which is much tighter than the previous 3s buffer
    static const int InternalBufferSize = 16384;
    juce::AudioBuffer<float> ringBuffer {2, InternalBufferSize}; 
    juce::AbstractFifo fifo {InternalBufferSize};

    double currentSampleRate = 44100.0;
    int maxBlockSize = 512;
    float volume = 1.0f;
    bool isPrepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VLCMediaPlayer_Desktop)
};
#endif