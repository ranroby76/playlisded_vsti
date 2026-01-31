/*
  ==============================================================================

    AudioEngine.cpp
    Playlisted2

    VSTi Compliance Fixes:
    - Fixed launchEngine() to use robust dynamic library path detection 
      (GetModuleHandleEx on Win, dladdr on Mac).
    - Fixed macOS executable name (removes .exe extension).
    - FIXED: Compilation error on Mac using dladdr with member function pointer.

  ==============================================================================
*/

#include "AudioEngine.h"
#include "AppLogger.h"

#if JUCE_WINDOWS
    #include <windows.h>
#elif JUCE_MAC
    #include <dlfcn.h> // Required for dladdr to find the VST3 bundle path
#endif

using namespace juce;

AudioEngine::AudioEngine()
{
    formatManager.registerBasicFormats();
    remotePlayer = std::make_unique<RemotePlayerFacade>(ipc);
    
    // Attempt to launch the external video engine
    launchEngine();
    // Start IPC timer
    startTimer(200); 
}

AudioEngine::~AudioEngine()
{
    stopTimer();
    stopAllPlayback();
    terminateEngine();
}

void AudioEngine::setPitchSemitones(int semitones)
{
    currentPitchSemitones = semitones;
    currentPitchFactor = std::pow(2.0f, semitones / 12.0f);
}

void AudioEngine::processPitchShift(juce::AudioBuffer<float>& buffer)
{
    if (currentPitchSemitones == 0) return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    const int bufferLen = pitchDelayBuffer.getNumSamples();
    const int windowSize = pitchWindowSize;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* channelData = buffer.getWritePointer(ch);
        auto* delayData = pitchDelayBuffer.getWritePointer(ch);
        
        int localWrite = pitchWritePos;
        float localRead = pitchReadPos;

        for (int i = 0; i < numSamples; ++i)
        {
            float inputSample = channelData[i];
            delayData[localWrite] = inputSample;
            localWrite = (localWrite + 1) % bufferLen;

            float phase = pitchCrossfade + ((float)i * (1.0f - currentPitchFactor) / (float)windowSize);
            phase = phase - std::floor(phase);

            float delaySamples = phase * (float)windowSize;
            float rA = (float)localWrite - delaySamples;
            if (rA < 0) rA += bufferLen;

            int iA = (int)rA;
            float fracA = rA - iA;
            float sA = delayData[iA] * (1.0f - fracA) + delayData[(iA + 1) % bufferLen] * fracA;

            float phaseB = phase + 0.5f;
            if (phaseB >= 1.0f) phaseB -= 1.0f;
            float delaySamplesB = phaseB * (float)windowSize;

            float rB = (float)localWrite - delaySamplesB;
            if (rB < 0) rB += bufferLen;
            int iB = (int)rB;
            float fracB = rB - iB;
            float sB = delayData[iB] * (1.0f - fracB) + delayData[(iB + 1) % bufferLen] * fracB;

            float gainA = 1.0f - std::abs(2.0f * phase - 1.0f);
            float gainB = 1.0f - std::abs(2.0f * phaseB - 1.0f);

            channelData[i] = (sA * gainA + sB * gainB);
        }
        
        if (ch == numChannels - 1)
        {
            pitchWritePos = localWrite;
            float phaseIncrement = (1.0f - currentPitchFactor) / (float)windowSize;
            pitchCrossfade += phaseIncrement * numSamples;
            pitchCrossfade = pitchCrossfade - std::floor(pitchCrossfade);
        }
    }
}

void AudioEngine::timerCallback()
{
    if (!ipc.isConnected()) 
    {
        if (startupRetries < 20)
        {
            ipc.initialize();
            if (!engineProcess.isRunning())
            {
                launchEngine();
            }
            startupRetries++;
        }
    }
    else 
    {
        if (getTimerInterval() != 40) startTimer(40);
        if (startupRetries < 999) 
        {
             showVideoWindow();
             startupRetries = 999; 
        }
        remotePlayer->updateStatus();
    }
}

void AudioEngine::launchEngine()
{
    if (engineProcess.isRunning()) return;
    File engineExe;
    File pluginDir;

    // --- FIND PLUGIN BINARY LOCATION ---
    
    #if JUCE_WINDOWS
        HMODULE hModule = NULL;
        static int dummyAnchor = 0; 
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | 
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)&dummyAnchor, &hModule))
        {
            char path[MAX_PATH];
            if (GetModuleFileNameA(hModule, path, MAX_PATH))
            {
                pluginDir = File(path).getParentDirectory();
            }
        }
        // Windows uses .exe
        engineExe = pluginDir.getChildFile("PlaylistedEngine.exe");
    #elif JUCE_MAC
        // On Mac, we use dladdr to find the path of the VST3 bundle/dylib
        Dl_info info;
        // FIX: Use a static anchor variable instead of member function pointer
        static int dummyAnchor = 0; 
        if (dladdr((void*)&dummyAnchor, &info))
        {
            pluginDir = File(info.dli_fname).getParentDirectory();
        }
        // Mac uses no extension (Mach-O executable)
        engineExe = pluginDir.getChildFile("PlaylistedEngine");
        
        // Fallback: If inside VST3 bundle (Contents/MacOS), check sibling Resources or similar
        if (!engineExe.existsAsFile())
        {
             // Common setup: Playlisted.vst3/Contents/MacOS/Playlisted (dylib)
             // We want: Playlisted.vst3/Contents/Resources/PlaylistedEngine
             File resourcesDir = pluginDir.getParentDirectory().getChildFile("Resources");
             engineExe = resourcesDir.getChildFile("PlaylistedEngine");
        }
    #endif

    // Debug Fallback (Standalone Builds)
    if (!engineExe.existsAsFile())
    {
        File hostFile = File::getSpecialLocation(File::currentApplicationFile);
        #if JUCE_WINDOWS
            File siblingExe = hostFile.getSiblingFile("PlaylistedEngine.exe");
        #else
            File siblingExe = hostFile.getSiblingFile("PlaylistedEngine");
        #endif
        
        if (siblingExe.existsAsFile())
        {
            engineExe = siblingExe;
        }
    }

    if (engineExe.existsAsFile())
    {
        LOG_INFO("AudioEngine: Launching External Process: " + engineExe.getFullPathName());
        bool started = engineProcess.start(engineExe.getFullPathName());
        
        if (started)
        {
            LOG_INFO("AudioEngine: Process started successfully.");
            ipc.initialize();
        }
        else
        {
            LOG_ERROR("AudioEngine: Failed to start process!");
        }
    }
    else
    {
        LOG_ERROR("AudioEngine: CRITICAL - Could not find PlaylistedEngine executable at " + pluginDir.getFullPathName());
    }
}

void AudioEngine::showVideoWindow()
{
    if (!ipc.isConnected())
    {
        if (!engineProcess.isRunning()) launchEngine();
        return;
    }
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty("type", "show_window");
    ipc.sendCommand(JSON::toString(var(o.get())));
}

void AudioEngine::terminateEngine() 
{
    if (engineProcess.isRunning())
    {
        engineProcess.kill();
    }
}

void AudioEngine::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    ipcBuffer.setSize(2, samplesPerBlock);
    
    pitchDelayBuffer.setSize(2, 16384);
    pitchDelayBuffer.clear();
    pitchWritePos = 0;
    pitchReadPos = 0.0f;
    pitchCrossfade = 0.0f;
    
    if (!ipc.isConnected())
    {
        ipc.initialize();
    }
    
    if (ipc.isConnected())
    {
        ipc.flushAudioBuffer();
    }
    
    showVideoWindow();
}

void AudioEngine::releaseResources() {
    ipcBuffer.setSize(0, 0);
    pitchDelayBuffer.setSize(0, 0);
}

void AudioEngine::processPluginBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    const int numSamples = buffer.getNumSamples();
    if (ipcBuffer.getNumSamples() < numSamples)
        ipcBuffer.setSize(2, numSamples);

    buffer.clear();
    ipcBuffer.clear();

    handleMidi(midiMessages);

    if (!ipc.isConnected())
    {
        ipc.initialize();
    }
    
    if (ipc.isConnected())
    {
        ipc.popAudio(ipcBuffer);
        
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            if (ch < 2) 
            {
                buffer.copyFrom(ch, 0, ipcBuffer, ch, 0, numSamples);
            }
        }
        
        processPitchShift(buffer);
    }
}

void AudioEngine::handleMidi(juce::MidiBuffer& midiMessages)
{
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        if (message.isNoteOn())
        {
            int note = message.getNoteNumber();
            if (note == 15) // Play/Pause
            {
                if (remotePlayer->isPlaying()) remotePlayer->pause();
                else remotePlayer->play();
            }
            else if (note == 16) // Stop
            {
                stopAllPlayback();
            }
            else if (note == 17)
            {
                showVideoWindow();
            }
        }
    }
}

void AudioEngine::stopAllPlayback()
{
    if (remotePlayer) remotePlayer->stop();
}

void AudioEngine::updateCrossfadeState()
{
    if (remotePlayer) remotePlayer->updateStatus();
}

XmlElement* AudioEngine::getStateXml()
{
    auto* xml = new XmlElement("OnStageState");
    auto* playlistXml = new XmlElement("Playlist");
    for (const auto& item : playlist)
    {
        auto* itemXml = new XmlElement("Item");
        itemXml->setAttribute("path", item.filePath);
        itemXml->setAttribute("title", item.title);
        itemXml->setAttribute("vol", item.volume);
        itemXml->setAttribute("pitch", item.pitchSemitones);
        itemXml->setAttribute("speed", item.playbackSpeed);
        itemXml->setAttribute("delay", item.transitionDelaySec);
        itemXml->setAttribute("xfade", item.isCrossfade);
        playlistXml->addChildElement(itemXml);
    }
    xml->addChildElement(playlistXml);
    return xml;
}

void AudioEngine::setStateXml(const XmlElement* xml)
{
    if (!xml) return;
    playlist.clear();
    
    if (auto* playlistXml = xml->getChildByName("Playlist"))
    {
        for (auto* itemXml : playlistXml->getChildIterator())
        {
            PlaylistItem item;
            item.filePath = itemXml->getStringAttribute("path");
            item.title = itemXml->getStringAttribute("title");
            item.volume = (float)itemXml->getDoubleAttribute("vol", 1.0);
            item.pitchSemitones = itemXml->getIntAttribute("pitch", 0);
            item.playbackSpeed = (float)itemXml->getDoubleAttribute("speed", 1.0);
            item.transitionDelaySec = itemXml->getIntAttribute("delay", 0);
            item.isCrossfade = itemXml->getBoolAttribute("xfade", false);
            playlist.push_back(item);
        }
    }

    if (!playlist.empty())
    {
        auto& first = playlist[0];
        remotePlayer->loadFile(first.filePath);
        remotePlayer->setVolume(first.volume);
        remotePlayer->setRate(first.playbackSpeed);
        setPitchSemitones(first.pitchSemitones);
    }
}