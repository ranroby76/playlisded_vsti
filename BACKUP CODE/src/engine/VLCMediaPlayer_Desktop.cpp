/*
  ==============================================================================

    VLCMediaPlayer_Desktop.cpp
    Playlisted2 Engine

  ==============================================================================
*/

#include "VLCMediaPlayer_Desktop.h"
#include <cstring>
#include <juce_core/juce_core.h>

#if JUCE_WINDOWS
#include <windows.h>
#include <stdlib.h>
#endif

static bool loadVlcDlls(const juce::File& appDir)
{
    #if JUCE_WINDOWS
    juce::File vlcCore = appDir.getChildFile("libvlccore.dll");
    juce::File vlcLib  = appDir.getChildFile("libvlc.dll");

    if (!vlcCore.exists()) return false;
    if (!vlcLib.exists()) return false;

    if (!LoadLibraryA(vlcCore.getFullPathName().toRawUTF8())) return false;
    if (!LoadLibraryA(vlcLib.getFullPathName().toRawUTF8())) return false;
    return true;
    #else
    return true;
    #endif
}

VLCMediaPlayer_Desktop::VLCMediaPlayer_Desktop()
{
}

VLCMediaPlayer_Desktop::~VLCMediaPlayer_Desktop()
{
    stop();
    if (m_mediaPlayer) libvlc_media_player_release(m_mediaPlayer);
    if (m_instance) libvlc_release(m_instance);
}

void VLCMediaPlayer_Desktop::ensureInitialized()
{
    if (isInitialized) return;

    juce::File appDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    
    bool dllsLoaded = loadVlcDlls(appDir);
    if (dllsLoaded)
    {
        juce::File vlcPlugins = appDir.getChildFile("plugins");
        if (vlcPlugins.isDirectory())
        {
            #if JUCE_WINDOWS
                juce::String pathEnv = "VLC_PLUGIN_PATH=" + vlcPlugins.getFullPathName();
                _putenv(pathEnv.toRawUTF8());
            #endif
        }

        const char* args[] = { 
            "--aout=amem", 
            "--no-video-title-show",
            "--no-osd",
            "--no-xlib",
            "--quiet",
            
            // --- SYNC FIXES ---
            "--avcodec-hw=any",       
            "--vout=direct3d9",       
            "--no-drop-late-frames",  
            "--no-skip-frames", 
            
            // FIX: Re-enable strict clock sync to keep video tied to audio clock
            "--clock-jitter=0",
            "--clock-synchro=0",

            // Reduce caching for tighter response
            "--file-caching=150",     
            "--network-caching=150" 
        };
        m_instance = libvlc_new(sizeof(args) / sizeof(args[0]), args);
        if (m_instance)
        {
            m_mediaPlayer = libvlc_media_player_new(m_instance);
            if (m_mediaPlayer)
            {
                libvlc_audio_set_callbacks(m_mediaPlayer, audioPlay, audioPause, audioResume, audioFlush, audioDrain, this);
                libvlc_audio_set_format(m_mediaPlayer, "S16N", static_cast<int>(currentSampleRate), 2);
            }
        }
    }
    
    isInitialized = true;
}

bool VLCMediaPlayer_Desktop::prepareToPlay(int samplesPerBlock, double sampleRate)
{
    if (sampleRate > 1000.0) currentSampleRate = sampleRate;
    else currentSampleRate = 44100.0;

    ensureInitialized();
    maxBlockSize = samplesPerBlock;
    
    // Resize with new smaller constant
    ringBuffer.setSize(2, InternalBufferSize); 
    fifo.setTotalSize(ringBuffer.getNumSamples());
    fifo.reset();

    if (m_mediaPlayer)
    {
        libvlc_audio_set_format(m_mediaPlayer, "S16N", static_cast<int>(currentSampleRate), 2);
    }

    isPrepared = true;
    return true;
}

void VLCMediaPlayer_Desktop::releaseResources()
{
    stop();
    fifo.reset();
    ringBuffer.clear();
    isPrepared = false;
}

void VLCMediaPlayer_Desktop::setWindowHandle(void* handle)
{
    ensureInitialized();
    if (m_mediaPlayer && handle != nullptr)
    {
        #if JUCE_WINDOWS
            libvlc_media_player_set_hwnd(m_mediaPlayer, handle);
        #elif JUCE_MAC
            libvlc_media_player_set_nsobject(m_mediaPlayer, handle);
        #elif JUCE_LINUX
            libvlc_media_player_set_xwindow(m_mediaPlayer, (uint32_t)(uintptr_t)handle);
        #endif
    }
}

void VLCMediaPlayer_Desktop::setVideoEnabled(bool enabled)
{
    if (m_mediaPlayer)
    {
        libvlc_video_set_track(m_mediaPlayer, enabled ? 0 : -1);
    }
}

void VLCMediaPlayer_Desktop::flushAudioBuffers()
{
    juce::ScopedLock sl(audioLock);
    fifo.reset();
    ringBuffer.clear();
}

bool VLCMediaPlayer_Desktop::loadFile(const juce::String& path)
{
    ensureInitialized();
    if (!m_instance || !m_mediaPlayer) return false;
    
    libvlc_media_player_set_rate(m_mediaPlayer, 1.0f);
    int rate = (currentSampleRate > 0) ? static_cast<int>(currentSampleRate) : 44100;
    libvlc_audio_set_format(m_mediaPlayer, "S16N", rate, 2);

    // FIX: Use file:/// URI with proper URL encoding for Unicode support
    juce::URL fileURL = juce::URL(juce::File(path));
    juce::String urlString = fileURL.toString(true); // URL-encode the path
    
    libvlc_media_t* media = libvlc_media_new_location(m_instance, urlString.toUTF8());
    if (media == nullptr) return false;

    libvlc_media_player_set_media(m_mediaPlayer, media);
    libvlc_media_release(media);
    
    flushAudioBuffers();
    return true;
}

void VLCMediaPlayer_Desktop::play()
{
    if (m_mediaPlayer) libvlc_media_player_play(m_mediaPlayer);
}

void VLCMediaPlayer_Desktop::pause()
{
    flushAudioBuffers();
    if (m_mediaPlayer) libvlc_media_player_pause(m_mediaPlayer);
}

void VLCMediaPlayer_Desktop::stop()
{
    if (m_mediaPlayer) libvlc_media_player_stop(m_mediaPlayer);
    flushAudioBuffers();
}

void VLCMediaPlayer_Desktop::setVolume(float newVolume) { volume = newVolume; }
float VLCMediaPlayer_Desktop::getVolume() const { return volume; }

void VLCMediaPlayer_Desktop::setRate(float newRate) 
{ 
    if (m_mediaPlayer) 
    {
        // Must flush to prevent hearing old-speed audio
        flushAudioBuffers();
        libvlc_media_player_set_rate(m_mediaPlayer, newRate);
    }
}

float VLCMediaPlayer_Desktop::getRate() const { return m_mediaPlayer ? libvlc_media_player_get_rate(m_mediaPlayer) : 1.0f; }

bool VLCMediaPlayer_Desktop::hasFinished() const { 
    if (!m_mediaPlayer) return false; 
    return libvlc_media_player_get_state(m_mediaPlayer) == libvlc_Ended;
}

bool VLCMediaPlayer_Desktop::isPlaying() const { 
    if (!m_mediaPlayer) return false; 
    return libvlc_media_player_is_playing(m_mediaPlayer) != 0;
}

float VLCMediaPlayer_Desktop::getPosition() const { 
    if (!m_mediaPlayer) return 0.0f; 
    return libvlc_media_player_get_position(m_mediaPlayer);
}

void VLCMediaPlayer_Desktop::setPosition(float pos) 
{ 
    if (m_mediaPlayer) 
    {
        flushAudioBuffers();
        libvlc_media_player_set_position(m_mediaPlayer, pos);
    }
}

int64_t VLCMediaPlayer_Desktop::getLengthMs() const { 
    if (!m_mediaPlayer) return 0; 
    return libvlc_media_player_get_length(m_mediaPlayer);
}

int VLCMediaPlayer_Desktop::getNumAudioSamplesAvailable() const
{
    return fifo.getNumReady();
}

void VLCMediaPlayer_Desktop::audioPlay(void* data, const void* samples, unsigned count, int64_t pts) {
    auto* self = static_cast<VLCMediaPlayer_Desktop*>(data);
    if (self) self->addAudioSamples(samples, count, pts);
}
void VLCMediaPlayer_Desktop::audioPause(void*, int64_t) {}
void VLCMediaPlayer_Desktop::audioResume(void*, int64_t) {}
void VLCMediaPlayer_Desktop::audioFlush(void* data, int64_t) {
    auto* self = static_cast<VLCMediaPlayer_Desktop*>(data);
    if (self) self->flushAudioBuffers();
}
void VLCMediaPlayer_Desktop::audioDrain(void*) {}

void VLCMediaPlayer_Desktop::addAudioSamples(const void* samples, unsigned count, int64_t pts) {
    juce::ScopedLock sl(audioLock);
    const int space = fifo.getFreeSpace();
    int toWrite = juce::jmin(static_cast<int>(count), space);
    if (toWrite > 0) {
        int start1, size1, start2, size2;
        fifo.prepareToWrite(toWrite, start1, size1, start2, size2);
        const int16_t* src = static_cast<const int16_t*>(samples);
        const float scale = 1.0f / 32768.0f;
        if (size1 > 0) { 
            for (int i = 0; i < size1; ++i) { 
                ringBuffer.setSample(0, start1 + i, src[i * 2] * scale);
                ringBuffer.setSample(1, start1 + i, src[i * 2 + 1] * scale);
            } 
        }
        if (size2 > 0) { 
            for (int i = 0; i < size2; ++i) { 
                ringBuffer.setSample(0, start2 + i, src[(size1 + i) * 2] * scale);
                ringBuffer.setSample(1, start2 + i, src[(size1 + i) * 2 + 1] * scale);
            } 
        }
        fifo.finishedWrite(size1 + size2);
    }
}

void VLCMediaPlayer_Desktop::getNextAudioBlock(const juce::AudioSourceChannelInfo& info) {
    if (!isPrepared) { info.clearActiveBufferRegion(); return; }
    if (libvlc_media_player_get_state(m_mediaPlayer) == libvlc_Paused) { info.clearActiveBufferRegion(); return; }
    
    juce::ScopedLock sl(audioLock);
    int toRead = juce::jmin(info.numSamples, fifo.getNumReady());
    if (toRead > 0) {
        int start1, size1, start2, size2;
        fifo.prepareToRead(toRead, start1, size1, start2, size2);
        if (size1 > 0) for (int ch = 0; ch < 2; ++ch) info.buffer->addFrom(ch, info.startSample, ringBuffer, ch, start1, size1, volume);
        if (size2 > 0) for (int ch = 0; ch < 2; ++ch) info.buffer->addFrom(ch, info.startSample + size1, ringBuffer, ch, start2, size2, volume);
        fifo.finishedRead(size1 + size2);
    }
    if (toRead < info.numSamples) info.buffer->clear(info.startSample + toRead, info.numSamples - toRead);
}