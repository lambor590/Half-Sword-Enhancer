#pragma once

#include <string>
#include <cstdarg>
#include <string_view>
#include <filesystem>

#include "ConfigManager.h"

class Logger {
public:
    static constexpr size_t MAX_LOG_SIZE = 512;

    explicit Logger(std::string_view prefix) noexcept : printPrefix(prefix) {
        if (GetLogFile() == nullptr) {
            std::filesystem::path logPath = ConfigManager::GetAppDataPath() / "logs.log";
            FILE* newFile = nullptr;
            fopen_s(&newFile, logPath.string().c_str(), "a");
            GetLogFile(newFile);
        }
    }

    template <typename... Args> void Log(std::string_view format, Args&&... args) const noexcept {
        thread_local char buffer[MAX_LOG_SIZE];
        thread_local char formatBuffer[MAX_LOG_SIZE];

        const int prefixLen = static_cast<int>(printPrefix.size());
        constexpr const char* TEMPLATE_STR = " > ";
        constexpr int TEMPLATE_LEN = 3;

        size_t pos = 0;
        if (pos + prefixLen < MAX_LOG_SIZE) {
            std::memcpy(formatBuffer + pos, printPrefix.data(), prefixLen);
            pos += prefixLen;
        }
        if (pos + TEMPLATE_LEN < MAX_LOG_SIZE) {
            std::memcpy(formatBuffer + pos, TEMPLATE_STR, TEMPLATE_LEN);
            pos += TEMPLATE_LEN;
        }
        if (pos + format.size() < MAX_LOG_SIZE - 2) {
            std::memcpy(formatBuffer + pos, format.data(), format.size());
            pos += format.size();
        }
        if (pos < MAX_LOG_SIZE - 1) {
            formatBuffer[pos++] = '\n';
        }
        formatBuffer[pos] = '\0';

        const int result = std::snprintf(buffer, MAX_LOG_SIZE, formatBuffer, std::forward<Args>(args)...);
        if (result > 0) [[likely]] {
            printf("%s", buffer);
            if (FILE* logFile = GetLogFile(); logFile != nullptr) [[likely]] {
                fputs(buffer, logFile);
                fflush(logFile);
            }
        }
    }

private:
    std::string_view printPrefix;

    static FILE* GetLogFile(FILE* newLogFile = nullptr) noexcept {
        static FILE* logFile = nullptr;
        if (newLogFile != nullptr) {
            logFile = newLogFile;
        }
        return logFile;
    }
};
