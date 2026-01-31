/*
  ==============================================================================

    AppLogger.h
    Playlisted2

    VSTi Fix: Logs are now saved to AppData/Playlisted/Logs instead of the Desktop.

  ==============================================================================
*/

#pragma once
#include <juce_core/juce_core.h>
#include <fstream>
#include <mutex>

class AppLogger
{
public:
    enum class Level { Info, Warning, Error, Debug };
    static AppLogger& getInstance()
    {
        static AppLogger instance;
        return instance;
    }

    void log(Level level, const juce::String& message)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!logFile.is_open()) tryOpenLogFile();
        
        if (!logFile.is_open())
        {
            DBG("[LOGFILE FAILED] " + message);
            return;
        }
            
        auto time = juce::Time::getCurrentTime().toString(true, true, true, true);
        juce::String levelStr;
        switch (level)
        {
            case Level::Info:    levelStr = "INFO"; break;
            case Level::Warning: levelStr = "WARN"; break;
            case Level::Error:   levelStr = "ERROR"; break;
            case Level::Debug:   levelStr = "DEBUG"; break;
        }
        
        juce::String logLine = "[" + time + "] [" + levelStr + "] " + message;
        logFile << logLine.toStdString() << std::endl;
        logFile.flush(); 
        DBG(logLine);
    }

    void logInfo(const juce::String& message)    { log(Level::Info, message); }
    void logWarning(const juce::String& message) { log(Level::Warning, message); }
    void logError(const juce::String& message)   { log(Level::Error, message); }
    void logDebug(const juce::String& message)   { log(Level::Debug, message); }

private:
    AppLogger() { tryOpenLogFile(); }

    ~AppLogger()
    {
        if (logFile.is_open()) logFile.close();
    }
    
    void tryOpenLogFile()
    {
        // FIX: Store logs in AppData, not Desktop
        auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
        auto logDir = appData.getChildFile("Playlisted").getChildFile("Logs");
        
        if (!logDir.exists()) logDir.createDirectory();
        
        logFilePath = logDir.getChildFile("Playlisted_VST_Log.txt");
        logFile.open(logFilePath.getFullPathName().toStdString(), std::ios::out | std::ios::app);
        
        if (logFile.is_open()) {
            logFile << "\n=== NEW SESSION ===\n" << std::endl;
        }
    }

    std::ofstream logFile;
    std::mutex mutex;
    juce::File logFilePath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppLogger)
};

#define LOG_INFO(msg)    AppLogger::getInstance().logInfo(msg)
#define LOG_WARNING(msg) AppLogger::getInstance().logWarning(msg)
#define LOG_ERROR(msg)   AppLogger::getInstance().logError(msg)
#define LOG_DEBUG(msg)   AppLogger::getInstance().logDebug(msg)