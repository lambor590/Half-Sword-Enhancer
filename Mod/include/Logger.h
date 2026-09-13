#pragma once

#include <string>
#include <cstdarg>
#include <string_view>
#include <filesystem>

#include "ConfigManager.h"

class Logger {
public:
    static constexpr size_t MAX_LOG_SIZE = 512;
    enum class FlushMode { Buffered, Immediate };

    explicit Logger(std::string_view prefix, FlushMode mode = FlushMode::Buffered) noexcept
        : printPrefix(prefix), flushMode(mode) {
        (void)GetLogFile();
    }

    static void Flush() noexcept {
        if (FILE* logFile = GetLogFile()) fflush(logFile);
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
                if (flushMode == FlushMode::Immediate) fflush(logFile);
            }
        }
    }

private:
    std::string_view printPrefix;
    FlushMode flushMode;

    static FILE* GetLogFile() noexcept {
        static FILE* const LOG_FILE = [] {
            FILE* file = nullptr;
            try {
                const auto path = ConfigManager::GetAppDataPath() / "logs.log";
                fopen_s(&file, path.string().c_str(), "w");
            } catch (...) {}
            return file;
        }();
        return LOG_FILE;
    }
};
