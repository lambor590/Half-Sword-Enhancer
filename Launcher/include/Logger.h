#pragma once
#include <iostream>
#include <string_view>
#include <Windows.h>

class Logger {
private:
    enum class LogColor : WORD {
        WHITE = 7,    // Default
        GREEN = 10,   // Info
        YELLOW = 14,  // Warning
        RED = 12      // Error
    };
    
    static inline HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    
    static void SetConsoleColor(LogColor color) {
        SetConsoleTextAttribute(hConsole, static_cast<WORD>(color));
    }
    
    static void Log(const char* prefix, std::string_view message, LogColor color) {
        std::cout << '[';
        SetConsoleColor(color);
        std::cout << prefix;
        SetConsoleColor(LogColor::WHITE);
        std::cout << "] " << message << '\n';
    }
    
public:
    static void info(std::string_view message) {
        Log("INFO", message, LogColor::GREEN);
    }
    
    static void warn(std::string_view message) {
        Log("WARN", message, LogColor::YELLOW);
    }
    
    static void error(std::string_view message) {
        Log("ERROR", message, LogColor::RED);
    }
};
