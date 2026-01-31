/*
  ==============================================================================

    SharedMemoryManager.h
    Playlisted2

    Handles Inter-Process Communication (IPC) via Memory Mapped Files.
    FIXED: Added flushAudioBuffer to prevent "ghost audio" bursts on startup.
  ==============================================================================
*/

#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm> // Added for std::fill

namespace IPCConfig
{
    // BUMPED VERSION TO v3 to support new Window State flag
    static const char* SharedMemoryName = "Playlisted2_SharedMem_v3.dat";
    // Audio Settings
    static const int SampleRate = 44100;
    static const int BlockSize  = 512;
    static const int NumChannels = 2;
    
    // Size of the Ring Buffer (Power of 2 is best for wrapping logic)
    static const int AudioBufferSize = 65536;
    static const int CommandQueueSize = 16;
    static const int CommandBufferSize = 4096;
}

// Command Queue Structure
struct CommandSlot
{
    std::atomic<bool> ready { false };
    char data[IPCConfig::CommandBufferSize];
};

struct SharedMemoryLayout
{
    // --- STATUS ---
    std::atomic<bool> isEngineRunning { false };
    std::atomic<bool> isPlaying { false };
    std::atomic<bool> hasFinished { false };
    // NEW: Window Visibility Flag
    std::atomic<bool> isWindowOpen { true };
    
    std::atomic<float> currentPosition { 0.0f };
    std::atomic<int64_t> currentLengthMs { 0 };
    std::atomic<double> currentCallbackTime { 0.0 };

    // --- AUDIO ---
    std::atomic<int> audioWritePos { 0 };
    std::atomic<int> audioReadPos { 0 };
    float audioBuffer[IPCConfig::AudioBufferSize * IPCConfig::NumChannels];

    // --- COMMANDS (QUEUE) ---
    std::atomic<int> commandWriteIndex { 0 };
    std::atomic<int> commandReadIndex { 0 };
    CommandSlot commands[IPCConfig::CommandQueueSize];
};

class SharedMemoryManager
{
public:
    enum class Mode { Plugin_Client, Engine_Server };

    SharedMemoryManager(Mode mode) : currentMode(mode) {}

    ~SharedMemoryManager()
    {
        mappedFile.reset();
    }

    bool initialize()
    {
        auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        auto sharedFile = tempDir.getChildFile(IPCConfig::SharedMemoryName);

        if (currentMode == Mode::Engine_Server)
        {
            // SERVER: Create file
            if (sharedFile.exists())
            {
                if (!sharedFile.deleteFile()) { }
            }

            // Ensure file exists with correct size
            if (sharedFile.create())
            {
                // Only fill if empty
                if (sharedFile.getSize() != sizeof(SharedMemoryLayout))
                {
                    sharedFile.deleteFile();
                    sharedFile.create();
                    juce::MemoryBlock zeros(sizeof(SharedMemoryLayout), true);
                    bool success = sharedFile.appendData(zeros.getData(), zeros.getSize());
                    if (!success) return false;
                }
            }
            else return false;
        }
        else
        {
            // CLIENT: Wait for file to exist
            if (!sharedFile.existsAsFile()) return false;
            // Wait for size to settle
            if (sharedFile.getSize() < sizeof(SharedMemoryLayout)) return false;
        }

        try
        {
            mappedFile = std::make_unique<juce::MemoryMappedFile>(
                sharedFile, 
                juce::MemoryMappedFile::AccessMode::readWrite
            );
            layout = static_cast<SharedMemoryLayout*>(mappedFile->getData());
        }
        catch (...)
        {
            return false;
        }

        // Initialize (Server only sets flag)
        if (currentMode == Mode::Engine_Server && layout)
        {
            layout->isEngineRunning.store(true);
        }

        return (layout != nullptr);
    }

    bool isConnected() const 
    { 
        return layout != nullptr && layout->isEngineRunning.load();
    }

    // ==============================================================================
    // AUDIO METHODS
    // ==============================================================================
    
    // FIX: New method to wipe the buffer clean on startup
    void flushAudioBuffer()
    {
        if (!layout) return;
        
        // Reset pointers
        layout->audioReadPos.store(0);
        layout->audioWritePos.store(0);
        
        // Zero out the entire buffer memory to prevent old audio bursts
        std::fill(std::begin(layout->audioBuffer), std::end(layout->audioBuffer), 0.0f);
    }

    void pushAudio(const float* const* channelData, int numChannels, int numSamples)
    {
        if (!layout) return;
        int writePos = layout->audioWritePos.load();
        int totalSize = IPCConfig::AudioBufferSize * IPCConfig::NumChannels;

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < IPCConfig::NumChannels; ++ch)
            {
                float sample = (ch < numChannels) ? channelData[ch][i] : 0.0f;
                layout->audioBuffer[writePos] = sample;
                writePos = (writePos + 1) % totalSize;
            }
        }
        
        layout->audioWritePos.store(writePos);
    }

    void popAudio(juce::AudioBuffer<float>& buffer)
    {
        if (!layout) { buffer.clear(); return; }

        int numSamples = buffer.getNumSamples();
        int readPos = layout->audioReadPos.load();
        int writePos = layout->audioWritePos.load();

        int totalSize = IPCConfig::AudioBufferSize * IPCConfig::NumChannels;
        int availableFloats = (writePos - readPos + totalSize) % totalSize;
        int availableFrames = availableFloats / IPCConfig::NumChannels;

        // Underrun protection
        if (availableFrames < numSamples)
        {
            buffer.clear();
            return; 
        }

        auto* l = buffer.getWritePointer(0);
        auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            float left = layout->audioBuffer[readPos];
            readPos = (readPos + 1) % totalSize;
            
            float right = layout->audioBuffer[readPos];
            readPos = (readPos + 1) % totalSize;

            l[i] = left;
            if (r) r[i] = right;
        }

        layout->audioReadPos.store(readPos);
    }

    // ==============================================================================
    // COMMAND METHODS - FIX FOR HEBREW/UNICODE
    // ==============================================================================

    void sendCommand(const juce::String& jsonCommand)
    {
        if (!layout) return;
        int writeIdx = layout->commandWriteIndex.load();
        int nextWriteIdx = (writeIdx + 1) % IPCConfig::CommandQueueSize;

        // Check if queue is full
        if (nextWriteIdx == layout->commandReadIndex.load()) return;

        auto& slot = layout->commands[writeIdx];
        
        // FIX: Use strlen to get actual UTF-8 byte count, not character count
        const char* utf8Data = jsonCommand.toRawUTF8();
        size_t utf8ByteLength = strlen(utf8Data);
        size_t bytesToCopy = juce::jmin(utf8ByteLength, (size_t)(IPCConfig::CommandBufferSize - 1));
        
        memset(slot.data, 0, IPCConfig::CommandBufferSize);
        memcpy(slot.data, utf8Data, bytesToCopy);
        
        slot.ready.store(true);
        layout->commandWriteIndex.store(nextWriteIdx);
    }

    juce::String getNextCommand()
    {
        if (!layout) return {};
        int readIdx = layout->commandReadIndex.load();
        int writeIdx = layout->commandWriteIndex.load();
        
        if (readIdx == writeIdx) return {};
        auto& slot = layout->commands[readIdx];
        if (!slot.ready.load()) return {};
        
        // FIX: Explicitly construct UTF-8 string
        juce::String cmd = juce::String::fromUTF8(slot.data);
        
        slot.ready.store(false);
        layout->commandReadIndex.store((readIdx + 1) % IPCConfig::CommandQueueSize);
        
        return cmd;
    }

    // ==============================================================================
    // STATUS SYNC
    // ==============================================================================
    
    // Updated setEngineStatus to include Window Visibility
    void setEngineStatus(bool playing, bool finished, bool winOpen, float pos, int64_t len)
    {
        if (!layout) return;
        layout->isPlaying.store(playing);
        layout->hasFinished.store(finished);
        layout->isWindowOpen.store(winOpen);
        layout->currentPosition.store(pos);
        layout->currentLengthMs.store(len);
    }

    struct EngineStatus { bool playing; bool finished; bool winOpen; float pos; int64_t len; };
    EngineStatus getEngineStatus()
    {
        if (!layout) return { false, false, false, 0.0f, 0 };
        return { 
            layout->isPlaying.load(), 
            layout->hasFinished.load(), 
            layout->isWindowOpen.load(),
            layout->currentPosition.load(), 
            layout->currentLengthMs.load() 
        };
    }

private:
    Mode currentMode;
    std::unique_ptr<juce::MemoryMappedFile> mappedFile;
    SharedMemoryLayout* layout = nullptr;
};