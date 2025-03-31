#pragma once

#include <string>
#include <cstdarg>

#include "ConfigManager.h"

class Logger
{
public:
    Logger(const char* prefix)
    {
        printPrefix = prefix;

        FILE* logFile = GetLogFile();
        if (logFile == nullptr)
        {
            std::filesystem::path logPath = ConfigManager::GetAppDataPath() / "HS-Enhancer_logs.txt";
            fopen_s(&logFile, logPath.string().c_str(), "w");
            GetLogFile(logFile);
        }
    }

    void Log(std::string msg, ...) const
    {
        va_list args;
        va_start(args, msg);
        vprintf(std::string(printPrefix + " > " + msg + "\n").c_str(), args);
        if (GetLogFile() != nullptr)
        {
            vfprintf(GetLogFile(), std::string(printPrefix + " > " + msg + "\n").c_str(), args);
            fflush(GetLogFile());
        }
        va_end(args);
    }

private:
    std::string printPrefix = "";

    static FILE* GetLogFile(FILE* newLogFile = nullptr)
    {
        static FILE* logFile = nullptr;
        if (newLogFile != nullptr)
        {
            logFile = newLogFile;
        }
        return logFile;
    }
};