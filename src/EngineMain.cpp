/*
  ==============================================================================

    EngineMain.cpp
    Playlisted2 Engine (Standalone Process)
    
    SINGLE DECK ARCHITECTURE - CROSS PLATFORM (Win/Mac)
    - Windows: Uses VLCMediaPlayer_Desktop (Direct HWND Rendering)
    - macOS: Uses NativeMediaPlayer_Apple (AVFoundation Image Extraction)
    
    ADDED: Heartbeat watchdog - auto-quit if plugin stops responding
    FIX: Engine reads DAW sample rate from IPC and reconfigures VLC accordingly.
    FIX: Faster audio pump loop (1ms instead of 2ms) to reduce underruns.
    FIX: OpenGL-accelerated video rendering on macOS for smooth playback.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>
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
#if JUCE_MAC

// macOS: OpenGL-accelerated video rendering
class VideoComponent : public juce::Component, 
                       private juce::Timer,
                       private juce::OpenGLRenderer
{
public:
    VideoComponent() 
    { 
        setOpaque(true);
        
        // Attach OpenGL context to this component
        glContext.setRenderer(this);
        glContext.setContinuousRepainting(false); // We control repainting via timer
        glContext.setComponentPaintingEnabled(false); // We handle all rendering in GL
        glContext.attachTo(*this);
        
        logToDesktop("VideoComponent: OpenGL context attached");
    }
    
    ~VideoComponent() override
    {
        glContext.detach();
    }

    void setPlayer(PlatformPlayer* p) 
    { 
        player = p; 
        startTimerHz(30); 
    }

    void timerCallback() override 
    {
        if (player && player->isPlaying()) 
        {
            juce::Image newFrame = player->getCurrentVideoFrame();
            if (newFrame.isValid())
            {
                // Thread-safe swap of the pending frame
                const juce::ScopedLock sl(frameLock);
                pendingFrame = newFrame;
                hasNewFrame = true;
            }
            // Trigger an OpenGL repaint
            glContext.triggerRepaint();
        }
        else if (player && !player->isPlaying())
        {
            // Still trigger repaint to show last frame / idle state
            glContext.triggerRepaint();
        }
    }
    
    // --- OpenGLRenderer callbacks ---
    
    void newOpenGLContextCreated() override
    {
        logToDesktop("VideoComponent: OpenGL context created");
        textureID = 0;
        textureWidth = 0;
        textureHeight = 0;
    }
    
    void openGLContextClosing() override
    {
        logToDesktop("VideoComponent: OpenGL context closing");
        if (textureID != 0)
        {
            juce::gl::glDeleteTextures(1, &textureID);
            textureID = 0;
        }
    }
    
    void renderOpenGL() override
    {
        using namespace juce::gl;
        
        // Check for new frame and upload to texture
        juce::Image frameToUpload;
        {
            const juce::ScopedLock sl(frameLock);
            if (hasNewFrame)
            {
                frameToUpload = pendingFrame;
                hasNewFrame = false;
            }
        }
        
        if (frameToUpload.isValid())
        {
            uploadTexture(frameToUpload);
        }
        
        // Get actual pixel dimensions (retina-aware)
        const float scale = (float)glContext.getRenderingScale();
        const int viewW = (int)(getWidth() * scale);
        const int viewH = (int)(getHeight() * scale);
        
        glViewport(0, 0, viewW, viewH);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        if (textureID != 0 && textureWidth > 0 && textureHeight > 0)
        {
            // Calculate aspect-ratio-correct quad
            float srcAspect = (float)textureWidth / (float)textureHeight;
            float dstAspect = (float)viewW / (float)viewH;
            
            float quadX, quadY, quadW, quadH;
            if (srcAspect > dstAspect)
            {
                // Video is wider — letterbox top/bottom
                quadW = (float)viewW;
                quadH = quadW / srcAspect;
                quadX = 0.0f;
                quadY = ((float)viewH - quadH) * 0.5f;
            }
            else
            {
                // Video is taller — pillarbox left/right
                quadH = (float)viewH;
                quadW = quadH * srcAspect;
                quadX = ((float)viewW - quadW) * 0.5f;
                quadY = 0.0f;
            }
            
            // Convert to GL normalized coordinates (-1 to 1)
            float left   = (quadX / (float)viewW) * 2.0f - 1.0f;
            float right  = ((quadX + quadW) / (float)viewW) * 2.0f - 1.0f;
            float bottom = (quadY / (float)viewH) * 2.0f - 1.0f;
            float top    = ((quadY + quadH) / (float)viewH) * 2.0f - 1.0f;
            
            // Draw textured quad using fixed-function pipeline
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, textureID);
            
            glBegin(GL_QUADS);
                glTexCoord2f(0.0f, 0.0f); glVertex2f(left,  top);
                glTexCoord2f(1.0f, 0.0f); glVertex2f(right, top);
                glTexCoord2f(1.0f, 1.0f); glVertex2f(right, bottom);
                glTexCoord2f(0.0f, 1.0f); glVertex2f(left,  bottom);
            glEnd();
            
            glBindTexture(GL_TEXTURE_2D, 0);
            glDisable(GL_TEXTURE_2D);
        }
        else
        {
            // No texture — black screen is already cleared above
            // Could draw text here but keeping it simple
        }
    }
    
    // Fallback paint — only called if OpenGL fails to attach
    void paint(juce::Graphics& g) override 
    { 
        g.fillAll(juce::Colours::black);
        g.setColour(juce::Colours::grey);
        g.setFont(16.0f);
        g.drawText("Playlisted2 Video Output", getLocalBounds(), juce::Justification::centred);
    }

private:
    void uploadTexture(const juce::Image& image)
    {
        using namespace juce::gl;
        
        // Convert to ARGB format for GL upload
        juce::Image argbImage = image.convertedToFormat(juce::Image::ARGB);
        juce::Image::BitmapData bitmapData(argbImage, juce::Image::BitmapData::readOnly);
        
        int w = argbImage.getWidth();
        int h = argbImage.getHeight();
        
        // Create or resize texture if dimensions changed
        if (textureID == 0 || w != textureWidth || h != textureHeight)
        {
            if (textureID != 0)
                glDeleteTextures(1, &textureID);
            
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, nullptr);
            
            textureWidth = w;
            textureHeight = h;
            
            logToDesktop("VideoComponent: Created GL texture " + juce::String(w) + "x" + juce::String(h));
        }
        else
        {
            glBindTexture(GL_TEXTURE_2D, textureID);
        }
        
        // Upload pixel data row by row (handles stride/padding differences)
        for (int y = 0; y < h; ++y)
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, w, 1, 
                           GL_BGRA_EXT, GL_UNSIGNED_BYTE, 
                           bitmapData.getLinePointer(y));
        }
        
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    
    juce::OpenGLContext glContext;
    juce::CriticalSection frameLock;
    juce::Image pendingFrame;
    bool hasNewFrame = false;
    
    GLuint textureID = 0;
    int textureWidth = 0;
    int textureHeight = 0;
    
    PlatformPlayer* player = nullptr;
};

#else

// Windows: VLC renders directly to HWND — no OpenGL needed
class VideoComponent : public juce::Component, private juce::Timer
{
public:
    VideoComponent() { setOpaque(true); }
    
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::black); }
    void timerCallback() override {}
};

#endif

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
        toFront(true);
        
        logToDesktop("VideoWindow created and set visible");
    }
    
    void* getNativeHandle()
    {
        if (auto* peer = getPeer())
            return peer->getNativeHandle();
        return nullptr;
    }
    
    void bindPlayer(PlatformPlayer* player)
    {
        #if JUCE_MAC
        if (videoComp) 
        {
            videoComp->setPlayer(player);
            logToDesktop("VideoWindow: Player bound to VideoComponent (OpenGL)");
        }
        #else
        juce::ignoreUnused(player);
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
// FIX: Added reconfigureSampleRate() to sync with DAW sample rate from IPC.
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
            if (window && window->getNativeHandle()) 
                player.setWindowHandle(window->getNativeHandle());
        #elif JUCE_MAC
            if (window)
                window->bindPlayer(&player);
        #endif
    }

    // FIX: Called by the pump loop when DAW sample rate changes
    void reconfigureSampleRate(int newRate)
    {
        if (newRate == currentSampleRate || newRate < 8000) return;
        
        currentSampleRate = newRate;
        logToDesktop("SingleDeckPlayer: Reconfiguring to DAW sample rate: " + juce::String(newRate));
        player.prepareToPlay(512, (double)newRate);
    }
    
    int getCurrentSampleRate() const { return currentSampleRate; }

    void load(const juce::String& path, float vol, float rate)
    {
        player.stop();
        
        #if JUCE_WINDOWS
        if (window && window->getNativeHandle()) 
            player.setWindowHandle(window->getNativeHandle());
        #endif

        logToDesktop("SingleDeckPlayer: Loading file: " + path);
        
        bool loaded = player.loadFile(path);
        if (loaded)
        {
            player.setVolume(vol);
            player.setRate(rate);
            logToDesktop("SingleDeckPlayer: File loaded successfully");
        }
        else
        {
            logToDesktop("SingleDeckPlayer: FAILED to load file!");
        }
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
            return 4096; 
        #endif
    }

private:
    PlatformPlayer player;
    VideoWindow* window = nullptr;
    int currentSampleRate = 44100;
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
        logToDesktop("Another instance attempted to start - showing existing window");
        if (videoWin)
        {
            videoWin->setVisible(true);
            if (videoWin->isMinimised()) videoWin->setMinimised(false);
            videoWin->toFront(true);
        }
    }

    void initialise(const juce::String&) override
    {
        logToDesktop("=== Engine Process Started (Single Deck Mode) ===");
        
        if (!ipc.initialize()) 
        { 
            logToDesktop("FATAL: IPC initialization failed!");
            quit(); 
            return;
        }
        
        logToDesktop("IPC initialized successfully");

        // FIX: Read DAW sample rate early and apply it
        int dawRate = ipc.getDawSampleRate();
        logToDesktop("DAW sample rate from IPC: " + juce::String(dawRate));

        // Create video window on message thread
        videoWin = std::make_unique<VideoWindow>();
        videoWin->toFront(true);
        
        logToDesktop("VideoWindow created, binding player...");
        
        // FIX: Configure player with DAW sample rate before binding
        player.reconfigureSampleRate(dawRate);
        player.setVideoWindow(videoWin.get());

        logToDesktop("Starting audio pump thread...");
        startThread(juce::Thread::Priority::highest);
        
        logToDesktop("Engine initialization complete");
    }

    void shutdown() override
    {
        logToDesktop("Engine shutting down...");
        signalThreadShouldExit();
        stopThread(2000);
        videoWin = nullptr;
        logToDesktop("Engine shutdown complete");
    }

    void run() override
    {
        const int blockSize = 512;
        juce::AudioBuffer<float> tempBuffer(2, blockSize);
        juce::AudioSourceChannelInfo info(&tempBuffer, 0, blockSize);
        int counter = 0;
        
        // Heartbeat watchdog - quit if no heartbeat for 10 seconds
        const int heartbeatTimeoutFrames = 10 * 1000; // 10 seconds at 1ms loop
        int framesSinceHeartbeat = 0;
        
        // FIX: Track sample rate changes from DAW
        int lastKnownRate = player.getCurrentSampleRate();

        while (!threadShouldExit())
        {
            juce::String cmdJson = ipc.getNextCommand();
            if (cmdJson.isNotEmpty()) 
            {
                if (cmdJson.contains("\"heartbeat\""))
                {
                    framesSinceHeartbeat = 0;
                }
                else
                {
                    handleCommand(cmdJson);
                }
            }
            
            framesSinceHeartbeat++;
            
            if (framesSinceHeartbeat > heartbeatTimeoutFrames)
            {
                logToDesktop("WATCHDOG: No heartbeat for 10 seconds - plugin likely terminated. Quitting engine.");
                juce::MessageManager::callAsync([this]() {
                    quit();
                });
                return;
            }

            // FIX: Check for sample rate changes from DAW (poll every ~500ms)
            if (counter % 500 == 0)
            {
                int dawRate = ipc.getDawSampleRate();
                if (dawRate != lastKnownRate && dawRate > 1000)
                {
                    logToDesktop("DAW sample rate changed: " + juce::String(lastKnownRate) + " -> " + juce::String(dawRate));
                    player.reconfigureSampleRate(dawRate);
                    lastKnownRate = dawRate;
                }
            }

            // Audio Pumping
            if (player.getNumAudioSamplesAvailable() >= blockSize)
            {
                tempBuffer.clear();
                player.getNextAudioBlock(info);
                ipc.pushAudio(tempBuffer.getArrayOfReadPointers(), 2, blockSize);
            }

            if (counter++ % 8 == 0)
            {
                bool isWinOpen = (videoWin && videoWin->isVisible());
                ipc.setEngineStatus(
                    player.isPlaying(), 
                    player.hasFinished(),
                    isWinOpen,
                    player.getPosition(), 
                    player.getLengthMs()
                );
            }
            
            // FIX: 1ms pump loop for tighter audio delivery
            wait(1);
        }
    }

private:
    void handleCommand(const juce::String& json)
    {
        auto var = juce::JSON::parse(json);
        if (!var.isObject()) return;
        juce::String type = var["type"];
        
        logToDesktop("Received command: " + type);
        
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
                    logToDesktop("show_window: Window shown and brought to front");
                }
            });
        }
        else if (type == "quit") {
            logToDesktop("Received quit command from plugin");
            juce::MessageManager::callAsync([this]() {
                quit();
            });
        }
    }

    SharedMemoryManager ipc { SharedMemoryManager::Mode::Engine_Server };
    SingleDeckPlayer player;
    std::unique_ptr<VideoWindow> videoWin;
};
START_JUCE_APPLICATION(PlaylistedEngineApplication)
