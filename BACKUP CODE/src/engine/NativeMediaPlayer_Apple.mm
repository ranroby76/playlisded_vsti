/*
  ==============================================================================

    NativeMediaPlayer_Apple.mm
    Playlisted2 Engine

    Implementation using AVFoundation (Shared for macOS).
    - Audio: Handled by AVAssetReader for video files, JUCE for audio-only
    - Video: Handled by AVPlayerItemVideoOutput (polled for frames)

  ==============================================================================
*/

#include "NativeMediaPlayer_Apple.h"
#include <cstdint>
#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>

// ==============================================================================
// AVFoundation Audio Reader (for video files)
// ==============================================================================
class AVAudioExtractor
{
public:
    AVAudioExtractor() {}
    ~AVAudioExtractor() { cleanUp(); }
    
    bool loadFile(const juce::String& path)
    {
        cleanUp();
        
        // FIX: Proper UTF-8 to NSString conversion for Hebrew/Unicode filenames
        NSString* nsPath = [[NSString alloc] initWithBytes:path.toRawUTF8() 
                                                    length:strlen(path.toRawUTF8()) 
                                                  encoding:NSUTF8StringEncoding];
        NSURL* url = [NSURL fileURLWithPath:nsPath];
        
        asset = [AVURLAsset assetWithURL:url];
        if (!asset) return false;
        
        // Find audio track
        NSArray* audioTracks = [asset tracksWithMediaType:AVMediaTypeAudio];
        if (audioTracks.count == 0) return false;
        
        AVAssetTrack* audioTrack = [audioTracks firstObject];
        
        NSError* error = nil;
        assetReader = [[AVAssetReader alloc] initWithAsset:asset error:&error];
        if (error || !assetReader) return false;
        
        // Audio output settings (PCM Float 32, Stereo, Native Sample Rate)
        NSDictionary* outputSettings = @{
            AVFormatIDKey: @(kAudioFormatLinearPCM),
            AVLinearPCMBitDepthKey: @(32),
            AVLinearPCMIsFloatKey: @(YES),
            AVLinearPCMIsBigEndianKey: @(NO),
            AVLinearPCMIsNonInterleaved: @(NO),
            AVNumberOfChannelsKey: @(2)
        };
        
        readerOutput = [[AVAssetReaderTrackOutput alloc] initWithTrack:audioTrack outputSettings:outputSettings];
        [assetReader addOutput:readerOutput];
        
        // Get audio format info
        NSArray* formatDescriptions = audioTrack.formatDescriptions;
        if (formatDescriptions.count > 0)
        {
            CMAudioFormatDescriptionRef desc = (__bridge CMAudioFormatDescriptionRef)formatDescriptions[0];
            const AudioStreamBasicDescription* asbd = CMAudioFormatDescriptionGetStreamBasicDescription(desc);
            if (asbd)
            {
                sampleRate = asbd->mSampleRate;
                numChannels = asbd->mChannelsPerFrame;
            }
        }
        
        // Calculate duration
        CMTime duration = asset.duration;
        durationSeconds = CMTimeGetSeconds(duration);
        totalSamples = (int64_t)(durationSeconds * sampleRate);
        
        [assetReader startReading];
        return true;
    }
    
    void fillBuffer(float* leftChannel, float* rightChannel, int numSamples, double playbackRate)
    {
        if (!assetReader || assetReader.status != AVAssetReaderStatusReading)
        {
            // Fill with silence if reader is done or failed
            memset(leftChannel, 0, numSamples * sizeof(float));
            memset(rightChannel, 0, numSamples * sizeof(float));
            return;
        }
        
        int samplesWritten = 0;
        
        while (samplesWritten < numSamples)
        {
            // Use cached sample if available
            if (!currentSampleBuffer && cachedBufferPos >= cachedBufferSize)
            {
                currentSampleBuffer = [readerOutput copyNextSampleBuffer];
                cachedBufferPos = 0;
                cachedBufferSize = 0;
                
                if (currentSampleBuffer)
                {
                    CMBlockBufferRef blockBuffer = CMSampleBufferGetDataBuffer(currentSampleBuffer);
                    if (blockBuffer)
                    {
                        size_t lengthAtOffset, totalLength;
                        char* dataPointer;
                        CMBlockBufferGetDataPointer(blockBuffer, 0, &lengthAtOffset, &totalLength, &dataPointer);
                        
                        cachedBuffer = (float*)dataPointer;
                        cachedBufferSize = (int)(totalLength / sizeof(float) / 2); // Stereo interleaved
                    }
                }
            }
            
            if (currentSampleBuffer && cachedBufferPos < cachedBufferSize)
            {
                int remaining = numSamples - samplesWritten;
                int available = cachedBufferSize - cachedBufferPos;
                int toCopy = juce::jmin(remaining, available);
                
                // Deinterleave stereo data
                for (int i = 0; i < toCopy; ++i)
                {
                    leftChannel[samplesWritten + i] = cachedBuffer[(cachedBufferPos + i) * 2];
                    rightChannel[samplesWritten + i] = cachedBuffer[(cachedBufferPos + i) * 2 + 1];
                }
                
                cachedBufferPos += toCopy;
                samplesWritten += toCopy;
                currentReadPosition += toCopy;
            }
            else
            {
                // Release current buffer
                if (currentSampleBuffer)
                {
                    CFRelease(currentSampleBuffer);
                    currentSampleBuffer = nullptr;
                }
                
                // No more data available
                if (assetReader.status != AVAssetReaderStatusReading)
                {
                    // Fill remaining with silence
                    int remaining = numSamples - samplesWritten;
                    memset(leftChannel + samplesWritten, 0, remaining * sizeof(float));
                    memset(rightChannel + samplesWritten, 0, remaining * sizeof(float));
                    break;
                }
            }
        }
    }
    
    void seek(double timeSeconds)
    {
        cleanUp();
        
        // Would need to recreate the asset reader at the new position
        // For now, seeking is limited - this is a known AVAssetReader limitation
        currentReadPosition = 0;
    }
    
    double getSampleRate() const { return sampleRate; }
    double getDuration() const { return durationSeconds; }
    int64_t getTotalSamples() const { return totalSamples; }
    int64_t getCurrentPosition() const { return currentReadPosition; }
    bool hasFinished() const { return assetReader && assetReader.status == AVAssetReaderStatusCompleted; }
    
    void cleanUp()
    {
        if (currentSampleBuffer)
        {
            CFRelease(currentSampleBuffer);
            currentSampleBuffer = nullptr;
        }
        assetReader = nil;
        readerOutput = nil;
        asset = nil;
        cachedBuffer = nullptr;
        cachedBufferPos = 0;
        cachedBufferSize = 0;
        currentReadPosition = 0;
    }
    
private:
    AVAsset* asset = nil;
    AVAssetReader* assetReader = nil;
    AVAssetReaderTrackOutput* readerOutput = nil;
    CMSampleBufferRef currentSampleBuffer = nullptr;
    
    float* cachedBuffer = nullptr;
    int cachedBufferPos = 0;
    int cachedBufferSize = 0;
    
    double sampleRate = 44100.0;
    int numChannels = 2;
    double durationSeconds = 0.0;
    int64_t totalSamples = 0;
    int64_t currentReadPosition = 0;
};

// ==============================================================================
// Video Frame Extractor
// ==============================================================================
class VideoFrameExtractor
{
public:
    VideoFrameExtractor() {}
    ~VideoFrameExtractor() { cleanUp(); }

    void loadVideo(const juce::String& path)
    {
        cleanUp();
        
        // FIX: Proper UTF-8 to NSString conversion for Hebrew/Unicode filenames
        NSString* nsPath = [[NSString alloc] initWithBytes:path.toRawUTF8() 
                                                    length:strlen(path.toRawUTF8()) 
                                                  encoding:NSUTF8StringEncoding];
        NSURL* url = [NSURL fileURLWithPath:nsPath];
        
        asset = [AVURLAsset assetWithURL:url];
        if (!asset) return;
        NSDictionary* settings = @{ (id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA) };
        output = [[AVPlayerItemVideoOutput alloc] initWithPixelBufferAttributes:settings];
        
        playerItem = [AVPlayerItem playerItemWithAsset:asset];
        [playerItem addOutput:output];
        player = [AVPlayer playerWithPlayerItem:playerItem];
        player.muted = YES; 
        player.actionAtItemEnd = AVPlayerActionAtItemEndPause;
    }

    juce::Image getFrameAtTime(double seconds)
    {
        if (!output) return juce::Image();
        CMTime time = CMTimeMakeWithSeconds(seconds, 600);

        if ([output hasNewPixelBufferForItemTime:time])
        {
            CMTime actualTime;
            CVPixelBufferRef buffer = [output copyPixelBufferForItemTime:time itemTimeForDisplay:&actualTime];
            
            if (buffer)
            {
                juce::Image img = convertPixelBufferToImage(buffer);
                CVPixelBufferRelease(buffer); 
                return img;
            }
        }
        return juce::Image();
    }

    void cleanUp() {
        player = nil;
        playerItem = nil;
        output = nil;
        asset = nil;
    }

private:
    AVPlayer* player = nil;
    AVPlayerItem* playerItem = nil;
    AVPlayerItemVideoOutput* output = nil;
    AVAsset* asset = nil;

    juce::Image convertPixelBufferToImage(CVPixelBufferRef buffer)
    {
        CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
        int width = (int)CVPixelBufferGetWidth(buffer);
        int height = (int)CVPixelBufferGetHeight(buffer);
        
        juce::uint8* srcData = (juce::uint8*)CVPixelBufferGetBaseAddress(buffer);
        size_t bytesPerRow = CVPixelBufferGetBytesPerRow(buffer);
        
        juce::Image image(juce::Image::ARGB, width, height, true);
        juce::Image::BitmapData destData(image, juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < height; ++y)
        {
            const juce::uint8* srcRow = srcData + (y * bytesPerRow);
            juce::uint8* destRow = destData.getLinePointer(y);
            memcpy(destRow, srcRow, width * 4);
        }

        CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
        return image;
    }
};

// ==============================================================================
// NativeMediaPlayer_Apple Implementation
// ==============================================================================

NativeMediaPlayer_Apple::NativeMediaPlayer_Apple()
{
    formatManager.registerBasicFormats(); 
    videoExtractor = std::make_unique<VideoFrameExtractor>();
    avAudioExtractor = std::make_unique<AVAudioExtractor>();
}

NativeMediaPlayer_Apple::~NativeMediaPlayer_Apple()
{
    transportSource.setSource(nullptr);
}

bool NativeMediaPlayer_Apple::prepareToPlay(int samplesPerBlock, double sampleRate)
{
    currentSampleRate = sampleRate;
    resampleSource.prepareToPlay(samplesPerBlock, sampleRate);
    return true;
}

void NativeMediaPlayer_Apple::releaseResources()
{
    transportSource.releaseResources();
    resampleSource.releaseResources();
}

bool NativeMediaPlayer_Apple::loadFile(const juce::String& path)
{
    transportSource.stop();
    transportSource.setSource(nullptr);
    readerSource.reset();
    videoExtractor->cleanUp();
    avAudioExtractor->cleanUp();
    isVideoLoaded = false;
    isUsingAVAudio = false;
    currentVideoImage = juce::Image();

    juce::File file(path);
    
    // Check if this is a video file
    bool isVideoFile = path.endsWithIgnoreCase(".mp4") || 
                       path.endsWithIgnoreCase(".mov") || 
                       path.endsWithIgnoreCase(".m4v") || 
                       path.endsWithIgnoreCase(".avi");
    
    if (isVideoFile)
    {
        // Use AVFoundation for video files (both audio and video)
        if (avAudioExtractor->loadFile(path))
        {
            isUsingAVAudio = true;
            originalSampleRate = avAudioExtractor->getSampleRate();
            currentDurationSeconds = avAudioExtractor->getDuration();
            
            // Load video track
            videoExtractor->loadVideo(path);
            isVideoLoaded = true;
            
            return true;
        }
        return false;
    }
    else
    {
        // Use JUCE for audio-only files
        auto* reader = formatManager.createReaderFor(file);
        
        if (reader != nullptr)
        {
            originalSampleRate = reader->sampleRate;
            currentDurationSeconds = reader->lengthInSamples / reader->sampleRate;
            readerSource.reset(new juce::AudioFormatReaderSource(reader, true));
            transportSource.setSource(readerSource.get(), 32768, nullptr, reader->sampleRate);
            
            double ratio = (originalSampleRate / currentSampleRate) / (double)currentRate;
            resampleSource.setResamplingRatio(ratio);
            
            return true;
        }
        return false;
    }
}

void NativeMediaPlayer_Apple::play() 
{ 
    if (isUsingAVAudio)
        isPlayingAV = true;
    else
        transportSource.start();
}

void NativeMediaPlayer_Apple::pause() 
{ 
    if (isUsingAVAudio)
        isPlayingAV = false;
    else
        transportSource.stop(); 
}

void NativeMediaPlayer_Apple::stop()  
{ 
    if (isUsingAVAudio)
    {
        isPlayingAV = false;
        avAudioExtractor->seek(0.0);
    }
    else
    {
        transportSource.stop(); 
        transportSource.setPosition(0);
    }
    
    if (currentVideoImage.isValid())
        currentVideoImage.clear(currentVideoImage.getBounds(), juce::Colours::black);
}

void NativeMediaPlayer_Apple::setVolume(float newVolume) 
{ 
    currentVolume = newVolume;
    if (!isUsingAVAudio)
        transportSource.setGain(newVolume);
}

float NativeMediaPlayer_Apple::getVolume() const 
{ 
    return isUsingAVAudio ? currentVolume : transportSource.getGain(); 
}

void NativeMediaPlayer_Apple::setRate(float newRate)
{
    currentRate = newRate;
    if (!isUsingAVAudio && currentSampleRate > 0 && originalSampleRate > 0)
    {
        double ratio = (originalSampleRate / currentSampleRate) / (double)newRate;
        resampleSource.setResamplingRatio(ratio);
    }
}

float NativeMediaPlayer_Apple::getRate() const 
{ 
    return currentRate;
}

bool NativeMediaPlayer_Apple::hasFinished() const 
{ 
    if (isUsingAVAudio)
        return avAudioExtractor->hasFinished();
    else
        return transportSource.hasStreamFinished(); 
}

bool NativeMediaPlayer_Apple::isPlaying() const 
{ 
    if (isUsingAVAudio)
        return isPlayingAV;
    else
        return transportSource.isPlaying();
}

float NativeMediaPlayer_Apple::getPosition() const
{
    if (isUsingAVAudio)
    {
        if (currentDurationSeconds > 0)
        {
            double currentPos = avAudioExtractor->getCurrentPosition() / avAudioExtractor->getSampleRate();
            return (float)(currentPos / currentDurationSeconds);
        }
        return 0.0f;
    }
    else
    {
        if (transportSource.getLengthInSeconds() > 0)
            return (float)(transportSource.getCurrentPosition() / transportSource.getLengthInSeconds());
        return 0.0f;
    }
}

void NativeMediaPlayer_Apple::setPosition(float pos)
{
    if (isUsingAVAudio)
    {
        double timeSeconds = pos * currentDurationSeconds;
        avAudioExtractor->seek(timeSeconds);
    }
    else
    {
        if (transportSource.getLengthInSeconds() > 0)
            transportSource.setPosition(pos * transportSource.getLengthInSeconds());
    }
}

int64_t NativeMediaPlayer_Apple::getLengthMs() const
{
    if (isUsingAVAudio)
        return (int64_t)(currentDurationSeconds * 1000.0);
    else
        return (int64_t)(transportSource.getLengthInSeconds() * 1000.0);
}

void NativeMediaPlayer_Apple::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    if (isUsingAVAudio)
    {
        if (!isPlayingAV)
        {
            info.clearActiveBufferRegion();
            return;
        }
        
        // Fill from AVAudioExtractor
        auto* leftChannel = info.buffer->getWritePointer(0, info.startSample);
        auto* rightChannel = info.buffer->getNumChannels() > 1 ? 
                            info.buffer->getWritePointer(1, info.startSample) : nullptr;
        
        avAudioExtractor->fillBuffer(leftChannel, 
                                     rightChannel ? rightChannel : leftChannel, 
                                     info.numSamples, 
                                     currentRate);
        
        // Apply volume
        if (currentVolume != 1.0f)
        {
            info.buffer->applyGain(info.startSample, info.numSamples, currentVolume);
        }
    }
    else
    {
        resampleSource.getNextAudioBlock(info);
    }
}

juce::Image NativeMediaPlayer_Apple::getCurrentVideoFrame()
{
    if (!isVideoLoaded) return juce::Image();

    double currentSeconds;
    if (isUsingAVAudio)
        currentSeconds = avAudioExtractor->getCurrentPosition() / avAudioExtractor->getSampleRate();
    else
        currentSeconds = transportSource.getCurrentPosition();
    
    juce::Image frame = videoExtractor->getFrameAtTime(currentSeconds);
    if (frame.isValid())
        currentVideoImage = frame;
        
    return currentVideoImage;
}