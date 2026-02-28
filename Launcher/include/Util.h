#pragma once

#include <Windows.h>
#include <ShlObj.h>
#include <filesystem>
#include <string>
#include <cstdint>

#include "Logger.h"

namespace hse {

    enum class GameEdition : std::uint8_t { FullGame, Demo };

    constexpr const char* GameEditionName(GameEdition edition) noexcept {
        return (edition == GameEdition::Demo) ? "Demo" : "Full Game";
    }

    constexpr const char* SteamUrl(GameEdition edition) noexcept {
        return (edition == GameEdition::Demo)
            ? "steam://rungameid/2642680"
            : "steam://rungameid/2397300";
    }

    constexpr const char* APP_FOLDER_NAME = "Half Sword Enhancer";
    constexpr const char* MOD_FILENAME = "HSEnhancer.dll";
    constexpr const char* PROXY_FILENAME = "winmm.dll";

    [[nodiscard]] inline bool IsGameRunning() noexcept {
        return FindWindowA("UnrealWindow", nullptr) != nullptr;
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
                fail(std::string("Failed to create directory in AppData: ") + e.what());
            }
        }

        return fullPath;
    }

}
