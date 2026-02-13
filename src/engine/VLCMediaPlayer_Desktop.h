/*
  ==============================================================================

    VLCMediaPlayer_Desktop.h
    Playlisted2

    FIX: Volume smoothing to prevent clicks on volume changes.
    FIX: LoadLibraryW for Unicode DLL paths.

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
    
    // A/V Sync: delay in milliseconds (positive = delay audio, let video catch up)
    void setAudioDelay(int64_t delayMs);

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
    
    static const int InternalBufferSize = 16384;
    juce::AudioBuffer<float> ringBuffer {2, InternalBufferSize}; 
    juce::AbstractFifo fifo {InternalBufferSize};

    double currentSampleRate = 44100.0;
    int maxBlockSize = 512;
    
    // Volume with smoothing
    float volume = 1.0f;
    float smoothedVolume = 1.0f;
    int avSyncDelaySamples = 0;  // Audio delay in samples for A/V sync
    
    // Delay line buffer for A/V sync
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePos = 0;
    int64_t delayTotalWritten = 0;
    
    bool isPrepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VLCMediaPlayer_Desktop)
};
#endif
