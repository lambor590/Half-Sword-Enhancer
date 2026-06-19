#pragma once

#include <Windows.h>
#include <ShlObj.h>
#include <array>
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
        return (edition == GameEdition::Demo) ? "steam://rungameid/2642680" : "steam://rungameid/2397300";
    }

    constexpr const char* APP_FOLDER_NAME = "Half Sword Enhancer";
    constexpr const char* MOD_FILENAME = "HSEnhancer.dll";
    constexpr const char* PROXY_FILENAME = "winmm.dll";
    constexpr const char* UE4SS_BRIDGE_FILENAME = "main.dll";
    constexpr const char* UE4SS_MOD_NAME = "HSEnhancer";

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

    [[nodiscard]] inline const std::filesystem::path& getAppDataDirectory() {
        static const std::filesystem::path fullPath = []() {
            char appDataPath[MAX_PATH];
            if (FAILED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) fail("Failed to get AppData path");

            auto appPath = std::filesystem::path(appDataPath) / APP_FOLDER_NAME;
            std::error_code ec;
            std::filesystem::create_directories(appPath, ec);
            if (ec) {
                fail(std::string("Failed to create directory in AppData: ") + ec.message());
            }
            return appPath;
        }();

        return fullPath;
    }

    [[nodiscard]] inline bool TryGetCurrentExecutablePath(std::filesystem::path& path) noexcept {
        std::array<char, MAX_PATH> filePath{};
        if (!GetModuleFileNameA(nullptr, filePath.data(), static_cast<DWORD>(filePath.size()))) {
            return false;
        }

        path = filePath.data();
        return true;
    }

}
