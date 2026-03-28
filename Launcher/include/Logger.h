#pragma once

#include <iostream>
#include <string_view>
#include <cstdio>
#include <Windows.h>

namespace hse {

    class Logger {
    private:
        enum class LogColor : WORD { WHITE = 7, GREEN = 10, YELLOW = 14, RED = 12 };

        static inline HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

        static void SetConsoleColor(LogColor color) noexcept {
            SetConsoleTextAttribute(hConsole, static_cast<WORD>(color));
        }

        static void Log(const char* level, LogColor color, std::string_view message) noexcept {
            std::cout << '[';
            SetConsoleColor(color);
            std::cout << level;
            SetConsoleColor(LogColor::WHITE);
            std::cout << "] " << message << '\n';
        }

        template <typename... Args>
        static void LogFormatted(const char* level, LogColor color, const char* format, Args&&... args) noexcept {
            char buffer[512];
            std::snprintf(buffer, sizeof(buffer), format, std::forward<Args>(args)...);
            Log(level, color, buffer);
        }

    public:
        static void info(std::string_view message) noexcept { Log("INFO", LogColor::GREEN, message); }

        static void warn(std::string_view message) noexcept { Log("WARN", LogColor::YELLOW, message); }

        static void error(std::string_view message) noexcept { Log("ERROR", LogColor::RED, message); }

        template <typename... Args> static void info(const char* format, Args&&... args) noexcept {
            LogFormatted("INFO", LogColor::GREEN, format, std::forward<Args>(args)...);
        }

        template <typename... Args> static void warn(const char* format, Args&&... args) noexcept {
            LogFormatted("WARN", LogColor::YELLOW, format, std::forward<Args>(args)...);
        }

        template <typename... Args> static void error(const char* format, Args&&... args) noexcept {
            LogFormatted("ERROR", LogColor::RED, format, std::forward<Args>(args)...);
        }
    };

}
