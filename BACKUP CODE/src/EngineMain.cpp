/*
  ==============================================================================

    EngineMain.cpp
    Playlisted2 Engine (Standalone Process)
    
    SINGLE DECK ARCHITECTURE - CROSS PLATFORM (Win/Mac)
    - Windows: Uses VLCMediaPlayer_Desktop (Direct HWND Rendering)
    - macOS: Uses NativeMediaPlayer_Apple (AVFoundation Image Extraction)

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "IPC/SharedMemoryManager.h"
#include <fstream>

// --- PLATFORM INCLUDES ---
#if JUCE_WINDOWS
    #include "engine/VLCMediaPlayer_Desktop.h"
    using PlatformPlayer = VLCMediaPlayer_Desktop;
#elif JUCE_MAC
    #include "engine/NativeMediaPlayer_Apple.h"
    using PlatformPlayer = NativeMediaPlayer_Apple;
#endif

void logToDesktop(const juce::String& text)
{
    auto desktop = juce::File::getSpecialLocation(juce::File::userDesktopDirectory);
    auto logFile = desktop.getChildFile("OnStage_EngineLog.txt");
    
    std::ofstream stream(logFile.getFullPathName().toStdString(), std::ios::app);
    if (stream.is_open())
    {
        auto time = juce::Time::getCurrentTime().toString(true, true, true, true);
        stream << "[" << time.toStdString() << "] " << text.toStdString() << std::endl;
    }
}

// ==============================================================================
// VIDEO COMPONENT (Handles Drawing)
// ==============================================================================
class VideoComponent : public juce::Component, private juce::Timer
{
public:
    VideoComponent() { setOpaque(true); }

    #if JUCE_MAC
    void setPlayer(PlatformPlayer* p) 
    { 
        player = p; 
        // Mac needs to poll for frames
        startTimerHz(30); 
    }

    void timerCallback() override 
    {
        if (player && player->isPlaying()) 
        {
            // Fetch frame from AVFoundation
            juce::Image newFrame = player->getCurrentVideoFrame();
            if (newFrame.isValid())
            {
                currentFrame = newFrame;
                repaint();
            }
        }
        else if (player && !player->isPlaying() && currentFrame.isValid())
        {
            // Keep drawing the last frame if paused, or clear if stopped?
            // For now, repaint to ensure updates
            repaint();
        }
    }

    void paint(juce::Graphics& g) override 
    { 
        g.fillAll(juce::Colours::black);
        
        if (currentFrame.isValid())
        {
            // Draw video frame scaling to fit
            g.drawImage(currentFrame, getLocalBounds().toFloat(), juce::RectanglePlacement::centred);
        }
    }
    
    private:
        PlatformPlayer* player = nullptr;
        juce::Image currentFrame;

    #else
    // Windows (VLC handles painting directly to the window handle)
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::black); }
    
    // FIX: Must implement timerCallback on Windows too, even if empty, 
    // because we inherit from juce::Timer.
    void timerCallback() override {}
    #endif
};

// ==============================================================================
// VIDEO WINDOW
// ==============================================================================
class VideoWindow : public juce::DocumentWindow
{
public:
    VideoWindow() 
        : DocumentWindow("Playlisted2 Video Output", juce::Colours::black, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        videoComp = new VideoComponent();
        setContentOwned(videoComp, true);
        setResizable(true, true);
        setAlwaysOnTop(true);
        centreWithSize(640, 360);
        setVisible(true);
    }
    
    void* getNativeHandle()
    {
        if (auto* peer = getPeer())
            return peer->getNativeHandle();
        return nullptr;
    }
    
    // Helper to pass player to component (Mac only needs this)
    void bindPlayer(PlatformPlayer* player)
    {
        #if JUCE_MAC
        if (videoComp) videoComp->setPlayer(player);
        #endif
    }
    
    void closeButtonPressed() override { 
        setVisible(false);
    }

private:
    VideoComponent* videoComp = nullptr;
};

// ==============================================================================
// SINGLE DECK CLASS (WRAPPER)
// ==============================================================================
class SingleDeckPlayer
{
public:
    SingleDeckPlayer()
    {
        player.prepareToPlay(512, 44100.0);
    }

    void setVideoWindow(VideoWindow* win)
    {
        window = win;
        
        #if JUCE_WINDOWS
            // Windows: Pass HWND to VLC
            if (window && window->getNativeHandle()) 
                player.setWindowHandle(window->getNativeHandle());
        #elif JUCE_MAC
            // Mac: Pass Player instance to Window for polling
            if (window)
                window->bindPlayer(&player);
        #endif
    }

    void load(const juce::String& path, float vol, float rate)
    {
        player.stop();
        
        #if JUCE_WINDOWS
        // Ensure handle is set before loading to prevent popup
        if (window && window->getNativeHandle()) 
            player.setWindowHandle(window->getNativeHandle());
        #endif

        player.loadFile(path);
        player.setVolume(vol);
        player.setRate(rate);
    }

    void play() { player.play(); }
    void pause() { player.pause(); }
    void stop() { player.stop(); }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
    {
        player.getNextAudioBlock(info);
    }

    bool isPlaying() { return player.isPlaying(); }
    bool hasFinished() { return player.hasFinished(); }
    float getPosition() { return player.getPosition(); }
    int64_t getLengthMs() { return player.getLengthMs(); }
    
    void setVolume(float v) { player.setVolume(v); }
    void setRate(float r) { player.setRate(r); }
    void setPosition(float p) { player.setPosition(p); }
    
    int getNumAudioSamplesAvailable() 
    { 
        #if JUCE_WINDOWS
            return player.getNumAudioSamplesAvailable(); 
        #else
            // AVFoundation implementation might not use the same FIFO structure explicitly exposed
            // But NativeMediaPlayer_Apple usually handles this internally via ResamplingAudioSource.
            // If the native class doesn't expose this method, we can mock it or add it.
            // Checking NativeMediaPlayer_Apple.h... it DOES NOT expose getNumAudioSamplesAvailable.
            // However, it pulls synchronously via getNextAudioBlock.
            // So we can always return block size to keep the pump running.
            return 4096; 
        #endif
    }

private:
    PlatformPlayer player;
    VideoWindow* window = nullptr;
};

// ==============================================================================
// APPLICATION MAIN
// ==============================================================================
class PlaylistedEngineApplication : public juce::JUCEApplication, public juce::Thread
{
public:
    PlaylistedEngineApplication() : Thread("AudioPumpThread") {}

    const juce::String getApplicationName() override       { return "PlaylistedEngine"; }
    const juce::String getApplicationVersion() override    { return "2.0.0"; }
    bool moreThanOneInstanceAllowed() override             { return false; }
    
    void anotherInstanceStarted(const juce::String&) override
    {
        if (videoWin)
        {
            videoWin->setVisible(true);
            if (videoWin->isMinimised()) videoWin->setMinimised(false);
            videoWin->toFront(true);
        }
    }

    void initialise(const juce::String&) override
    {
        logToDesktop("Engine Process Started (Single Deck Mode)");
        if (!ipc.initialize()) { 
            quit(); 
            return;
        }

        videoWin = std::make_unique<VideoWindow>();
        videoWin->toFront(true);
        
        player.setVideoWindow(videoWin.get());

        startThread(juce::Thread::Priority::highest);
    }

    void shutdown() override
    {
        signalThreadShouldExit();
        stopThread(2000);
        videoWin = nullptr;
    }

    void run() override
    {
        const int blockSize = 512;
        juce::AudioBuffer<float> tempBuffer(2, blockSize);
        juce::AudioSourceChannelInfo info(&tempBuffer, 0, blockSize);
        int counter = 0;

        while (!threadShouldExit())
        {
            juce::String cmdJson = ipc.getNextCommand();
            if (cmdJson.isNotEmpty()) {
                handleCommand(cmdJson);
            }

            // Audio Pumping
            // On Windows (VLC), we wait for FIFO.
            // On Mac (Native), getNextAudioBlock blocks/pulls from reader.
            if (player.getNumAudioSamplesAvailable() >= blockSize)
            {
                tempBuffer.clear();
                player.getNextAudioBlock(info);
                ipc.pushAudio(tempBuffer.getArrayOfReadPointers(), 2, blockSize);
            }

            if (counter++ % 4 == 0)
            {
                // Push status to shared memory, including Window Visibility
                bool isWinOpen = (videoWin && videoWin->isVisible());
                ipc.setEngineStatus(
                    player.isPlaying(), 
                    player.hasFinished(),
                    isWinOpen,
                    player.getPosition(), 
                    player.getLengthMs()
                );
            }
            wait(2);
        }
    }

private:
    void handleCommand(const juce::String& json)
    {
        auto var = juce::JSON::parse(json);
        if (!var.isObject()) return;
        juce::String type = var["type"];
        
        if (type == "load") {
            juce::String path = var["path"];
            float vol = var.hasProperty("vol") ? (float)var["vol"] : 1.0f;
            float rate = var.hasProperty("speed") ? (float)var["speed"] : 1.0f;
            
            player.load(path, vol, rate);
            juce::MessageManager::callAsync([this]() {
                if (videoWin && !videoWin->isVisible()) {
                    videoWin->setVisible(true);
                    videoWin->toFront(true);
                }
            });
        }
        else if (type == "play")  { player.play(); }
        else if (type == "pause") { player.pause(); }
        else if (type == "stop")  { player.stop(); }
        else if (type == "seek")  { player.setPosition((float)var["pos"]); }
        else if (type == "volume"){ player.setVolume((float)var["val"]); }
        else if (type == "rate")  { player.setRate((float)var["val"]); }
        else if (type == "show_window") {
            juce::MessageManager::callAsync([this]() {
                if (videoWin) {
                    if (videoWin->isMinimised()) videoWin->setMinimised(false);
                    videoWin->setVisible(true);
                    videoWin->toFront(true);
                }
            });
        }
    }

    SharedMemoryManager ipc { SharedMemoryManager::Mode::Engine_Server };
    SingleDeckPlayer player;
    std::unique_ptr<VideoWindow> videoWin;
};
START_JUCE_APPLICATION(PlaylistedEngineApplication)