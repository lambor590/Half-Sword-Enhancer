#pragma once

#include <Windows.h>
#include <ShlObj.h>
#include <filesystem>
#include <string>

#include "Logger.h"

namespace hse {

    constexpr const char* APP_FOLDER_NAME = "Half Sword Enhancer";
    constexpr const char* DLL_FILENAME = "HSEnhancer.dll";

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