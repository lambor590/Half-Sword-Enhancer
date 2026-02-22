#pragma once

#include <Windows.h>
#include <ShlObj.h>
#include <algorithm>
#include <filesystem>
#include <string>
#include <cstdint>

#include "Logger.h"

namespace hse {

    enum class GameMode : std::uint8_t { Demo, FullGame };

    constexpr const char* GameModeName(GameMode mode) noexcept {
        return (mode == GameMode::Demo) ? "Demo" : "Full Game";
    }

    constexpr const char* SteamUrl(GameMode mode) noexcept {
        return (mode == GameMode::Demo)
            ? "steam://rungameid/2642680"
            : "steam://rungameid/2397300";
    }

    enum class DllSource : std::uint8_t { Official, Custom };

    constexpr const char* APP_FOLDER_NAME = "Half Sword Enhancer";
    constexpr const char* CACHE_FOLDER_NAME = "cache";
    constexpr const char* CUSTOM_DLL_FILENAME = "HSEnhancer_custom.dll";
    constexpr const char* LEGACY_DLL_FILENAME = "HSEnhancer.dll";

    [[nodiscard]] inline std::string SanitizeTimestamp(std::string_view timestamp) {
        std::string result(timestamp);
        std::ranges::replace(result, ':', '-');
        return result;
    }

    [[noreturn]] inline void fail(const std::string& msg) noexcept {
        MessageBoxA(nullptr, msg.c_str(), "Error", MB_ICONERROR);
        exit(1);
    }

    inline void showError(const std::string& msg) noexcept {
        MessageBoxA(nullptr, msg.c_str(), "Error", MB_ICONERROR);
    }

    inline bool logAndShowError(const std::string& logMsg, const std::string& userMsg) noexcept {
        Logger::error(logMsg);
        showError(userMsg);
        return false;
    }

    [[nodiscard]] inline const std::string& getAppDataPath() {
        static std::string fullPath;

        if (fullPath.empty()) {
            char appDataPath[MAX_PATH];
            if (FAILED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath)))
                fail("Failed to get AppData path");

            fullPath = std::string(appDataPath) + "\\" + APP_FOLDER_NAME;
            try {
                std::filesystem::create_directories(fullPath);
            }
            catch (const std::filesystem::filesystem_error& e) {
                fail((std::string("Failed to create directory in AppData: ") + e.what()).c_str());
            }
        }

        return fullPath;
    }

}