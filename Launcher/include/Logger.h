#pragma once

#include <iostream>
#include <string_view>
#include <Windows.h>

namespace hse {

    class Logger {
    private:
        enum class LogColor : WORD {
            WHITE = 7,
            GREEN = 10,
            YELLOW = 14,
            RED = 12
        };

        static inline HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

        static void SetConsoleColor(LogColor color) noexcept {
            SetConsoleTextAttribute(hConsole, static_cast<WORD>(color));
        }

        static void Log(const char* prefix, std::string_view message, LogColor color) noexcept {
            std::cout << '[';
            SetConsoleColor(color);
            std::cout << prefix;
            SetConsoleColor(LogColor::WHITE);
            std::cout << "] " << message << '\n';
        }

    public:
        static void info(std::string_view message) noexcept {
            Log("INFO", message, LogColor::GREEN);
        }

        static void warn(std::string_view message) noexcept {
            Log("WARN", message, LogColor::YELLOW);
        }

        static void error(std::string_view message) noexcept {
            Log("ERROR", message, LogColor::RED);
        }
    };
}
