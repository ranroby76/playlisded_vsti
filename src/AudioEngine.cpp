/*
  ==============================================================================

    AudioEngine.cpp
    Playlisted2

    FIX: Removed ipc.initialize() from processPluginBlock (audio thread).
         Memory-mapped file ops on the real-time thread cause glitches.
    FIX: terminateEngine() now waits for graceful quit before killing,
         and handles the macOS /usr/bin/open process tracking issue.
    FIX: Cleans up shared memory file on shutdown.
    FIX: Desktop diagnostic logging for launch debugging.
    FIX: Wide-char API for Unicode path detection on Windows.

  ==============================================================================
*/

#include "AudioEngine.h"
#include "AppLogger.h"
#include <fstream>

#if JUCE_WINDOWS
    #include <windows.h>
#elif JUCE_MAC
    #include <dlfcn.h>
    #include <signal.h>
#endif

using namespace juce;

// ==============================================================================
// Desktop diagnostic log
// ==============================================================================
static void logLaunchDiag(const juce::String& text)
{
    auto desktop = juce::File::getSpecialLocation(juce::File::userDesktopDirectory);
    auto logFile = desktop.getChildFile("Playlisted_LaunchDiag.txt");
    
    std::ofstream stream(logFile.getFullPathName().toStdString(), std::ios::app);
    if (stream.is_open())
    {
        auto time = juce::Time::getCurrentTime().toString(true, true, true, true);
        stream << "[" << time.toStdString() << "] " << text.toStdString() << std::endl;
    }
}

AudioEngine::AudioEngine()
{
    formatManager.registerBasicFormats();
    remotePlayer = std::make_unique<RemotePlayerFacade>(ipc);
    
    logLaunchDiag("=== AudioEngine constructor called ===");
    
    launchEngine();
    startTimer(200); 
}

AudioEngine::~AudioEngine()
{
    stopTimer();
    stopAllPlayback();
    terminateEngine();
    cleanupSharedMemory();
}

// FIX: Remove stale shared memory file
void AudioEngine::cleanupSharedMemory()
{
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    auto sharedFile = tempDir.getChildFile("Playlisted2_SharedMem_v4.dat");
    if (sharedFile.existsAsFile())
    {
        sharedFile.deleteFile();
    }
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
            // FIX: IPC initialization happens here (timer thread), NOT on audio thread
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
        sendHeartbeat();
    }
}

void AudioEngine::sendHeartbeat()
{
    if (!ipc.isConnected()) return;
    juce::DynamicObject::Ptr o = new juce::DynamicObject();
    o->setProperty("type", "heartbeat");
    ipc.sendCommand(JSON::toString(var(o.get())));
}

void AudioEngine::launchEngine()
{
    if (engineProcess.isRunning()) return;
    File engineExe;
    File pluginDir;

    logLaunchDiag("launchEngine() called");

    #if JUCE_WINDOWS
        HMODULE hModule = NULL;
        static int dummyAnchor = 0; 
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | 
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCWSTR)&dummyAnchor, &hModule))
        {
            wchar_t wpath[MAX_PATH];
            if (GetModuleFileNameW(hModule, wpath, MAX_PATH))
            {
                pluginDir = File(String(wpath)).getParentDirectory();
                logLaunchDiag("GetModuleFileNameW found plugin DLL at: " + pluginDir.getFullPathName());
            }
            else
            {
                logLaunchDiag("GetModuleFileNameW FAILED, error: " + String((int)GetLastError()));
            }
        }
        else
        {
            logLaunchDiag("GetModuleHandleExW FAILED, error: " + String((int)GetLastError()));
        }
        engineExe = pluginDir.getChildFile("PlaylistedEngine.exe");
        logLaunchDiag("Looking for engine at: " + engineExe.getFullPathName());
        logLaunchDiag("Exists: " + String(engineExe.existsAsFile() ? "YES" : "NO"));
        
    #elif JUCE_MAC
        Dl_info info;
        static int dummyAnchor = 0; 
        if (dladdr((void*)&dummyAnchor, &info))
        {
            pluginDir = File(info.dli_fname).getParentDirectory();
            logLaunchDiag("dladdr found plugin at: " + pluginDir.getFullPathName());
            LOG_INFO("AudioEngine: dladdr found plugin at: " + pluginDir.getFullPathName());
        }
        else
        {
            logLaunchDiag("dladdr FAILED");
        }
        
        engineExe = pluginDir.getChildFile("PlaylistedEngine");
        logLaunchDiag("Looking for engine at: " + engineExe.getFullPathName() + " exists: " + String(engineExe.existsAsFile() ? "YES" : "NO"));
        
        if (!engineExe.existsAsFile())
        {
            File resourcesDir = pluginDir.getParentDirectory().getChildFile("Resources");
            engineExe = resourcesDir.getChildFile("PlaylistedEngine");
            logLaunchDiag("Trying Resources path: " + engineExe.getFullPathName() + " exists: " + String(engineExe.existsAsFile() ? "YES" : "NO"));
            LOG_INFO("AudioEngine: Trying Resources path: " + engineExe.getFullPathName());
        }
    #endif

    // Fallback: look next to the DAW executable
    if (!engineExe.existsAsFile())
    {
        File hostFile = File::getSpecialLocation(File::currentApplicationFile);
        logLaunchDiag("Primary path failed. Host app: " + hostFile.getFullPathName());
        
        #if JUCE_WINDOWS
            File siblingExe = hostFile.getSiblingFile("PlaylistedEngine.exe");
        #else
            File siblingExe = hostFile.getSiblingFile("PlaylistedEngine");
        #endif
        
        logLaunchDiag("Trying sibling: " + siblingExe.getFullPathName() + " exists: " + String(siblingExe.existsAsFile() ? "YES" : "NO"));
        
        if (siblingExe.existsAsFile())
        {
            engineExe = siblingExe;
        }
    }

    if (engineExe.existsAsFile())
    {
        LOG_INFO("AudioEngine: Launching External Process: " + engineExe.getFullPathName());
        logLaunchDiag("LAUNCHING: " + engineExe.getFullPathName());
        
        #if JUCE_MAC
            engineExe.setExecutePermission(true);
        #endif
        
        // FIX: Store the engine path for terminate fallback on macOS
        engineExePath = engineExe.getFullPathName();
        
        #if JUCE_WINDOWS
            String launchCmd = "\"" + engineExe.getFullPathName() + "\"";
        #elif JUCE_MAC
            String launchCmd = "/usr/bin/open -a \"" + engineExe.getFullPathName() + "\"";
        #else
            String launchCmd = engineExe.getFullPathName();
        #endif
        
        logLaunchDiag("Launch command: " + launchCmd);
        
        bool started = engineProcess.start(launchCmd);
        
        if (started)
        {
            LOG_INFO("AudioEngine: Process started successfully.");
            logLaunchDiag("Process started SUCCESSFULLY");
            ipc.initialize();
        }
        else
        {
            LOG_ERROR("AudioEngine: Failed to start process!");
            logLaunchDiag("Process start FAILED");
            
            #if JUCE_MAC
                logLaunchDiag("Trying direct launch as fallback...");
                String directCmd = "\"" + engineExe.getFullPathName() + "\"";
                started = engineProcess.start(directCmd);
                if (started)
                {
                    logLaunchDiag("Direct launch SUCCEEDED");
                    ipc.initialize();
                }
                else
                {
                    logLaunchDiag("Direct launch also FAILED");
                }
            #endif
        }
    }
    else
    {
        LOG_ERROR("AudioEngine: CRITICAL - Could not find PlaylistedEngine executable at " + pluginDir.getFullPathName());
        logLaunchDiag("CRITICAL: Engine exe NOT FOUND. pluginDir=" + pluginDir.getFullPathName());
        logLaunchDiag("--- Search path dump ---");
        #if JUCE_WINDOWS
            logLaunchDiag("  Expected: " + pluginDir.getChildFile("PlaylistedEngine.exe").getFullPathName());
        #else
            logLaunchDiag("  Expected: " + pluginDir.getChildFile("PlaylistedEngine").getFullPathName());
        #endif
        logLaunchDiag("  Host app: " + File::getSpecialLocation(File::currentApplicationFile).getFullPathName());
        logLaunchDiag("  Current dir: " + File::getCurrentWorkingDirectory().getFullPathName());
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
    // Step 1: Send quit command for graceful shutdown
    if (ipc.isConnected())
    {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty("type", "quit");
        ipc.sendCommand(JSON::toString(var(o.get())));
        
        // FIX: Wait up to 2 seconds for graceful shutdown instead of just 100ms
        for (int i = 0; i < 20; ++i)
        {
            Thread::sleep(100);
            if (!engineProcess.isRunning())
            {
                logLaunchDiag("Engine quit gracefully after " + String((i + 1) * 100) + "ms");
                return;
            }
        }
        logLaunchDiag("Engine did not quit gracefully after 2s, force killing...");
    }
    
    // Step 2: Force kill via ChildProcess handle
    if (engineProcess.isRunning())
    {
        engineProcess.kill();
        Thread::sleep(100);
    }
    
    // Step 3: macOS fallback — if we used /usr/bin/open, the ChildProcess handle
    // tracks the 'open' command, not the actual engine. Use killall as last resort.
    #if JUCE_MAC
    {
        // Check if engine is still running by looking for the process
        juce::ChildProcess check;
        if (check.start("pgrep -x PlaylistedEngine"))
        {
            juce::String output = check.readAllProcessOutput();
            if (output.trim().isNotEmpty())
            {
                logLaunchDiag("Engine still running after kill(), using killall...");
                juce::ChildProcess killer;
                killer.start("killall PlaylistedEngine");
                killer.waitForProcessToFinish(1000);
            }
        }
    }
    #endif
}

void AudioEngine::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    ipcBuffer.setSize(2, samplesPerBlock);
    
    pitchDelayBuffer.setSize(2, 16384);
    pitchDelayBuffer.clear();
    pitchWritePos = 0;
    pitchReadPos = 0.0f;
    pitchCrossfade = 0.0f;
    
    // FIX: IPC init only if not already connected (NOT on audio thread path)
    if (!ipc.isConnected())
    {
        ipc.initialize();
    }
    
    // Send DAW sample rate to engine via shared memory
    ipc.setDawSampleRate(static_cast<int>(sampleRate));
    logLaunchDiag("prepareToPlay: DAW sampleRate=" + String(sampleRate) + " blockSize=" + String(samplesPerBlock));
    
    if (ipc.isConnected())
    {
        ipc.flushAudioBuffer();
    }
    
    showVideoWindow();
}

void AudioEngine::releaseResources() 
{
    ipcBuffer.setSize(0, 0);
    pitchDelayBuffer.setSize(0, 0);
}

void AudioEngine::processPluginBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    const int numSamples = buffer.getNumSamples();
    if (ipcBuffer.getNumSamples() < numSamples)
        ipcBuffer.setSize(2, numSamples);

    buffer.clear();

    handleMidi(midiMessages);

    // FIX: Do NOT call ipc.initialize() here — this is the real-time audio thread.
    // Memory-mapped file operations can block and cause audio glitches.
    // IPC initialization is handled by the timer thread in timerCallback().
    
    if (ipc.isConnected())
    {
        ipcBuffer.clear();
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
            if (note == 15)
            {
                if (remotePlayer->isPlaying()) remotePlayer->pause();
                else remotePlayer->play();
            }
            else if (note == 16)
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
