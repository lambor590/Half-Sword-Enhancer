#pragma once

#include <Windows.h>
#include <ShlObj.h>
#include <KnownFolders.h>
#include <filesystem>
#include <string>
#include <string_view>
#include <cstdint>
#include <vector>

#include "Logger.h"
#include "GameEdition.h"

namespace hse {

    constexpr const char* GameEditionName(GameEdition edition) noexcept {
        return DescribeGameEdition(edition).displayName.data();
    }

    constexpr const char* SteamUrl(GameEdition edition) noexcept {
        return DescribeGameEdition(edition).steamUrl.data();
    }

    constexpr const char* APP_FOLDER_NAME = "Half Sword Enhancer";
    constexpr const char* MOD_FILENAME = "HSEnhancer.dll";
    constexpr const char* PROXY_FILENAME = "winmm.dll";
    constexpr const char* UE4SS_BRIDGE_FILENAME = "main.dll";
    constexpr const char* UE4SS_MOD_NAME = "HSEnhancer";

    class NamedPathMutex {
    public:
        NamedPathMutex(const std::filesystem::path& path, std::wstring_view prefix, DWORD timeout) {
            std::error_code error;
            auto normalized = std::filesystem::absolute(path, error).lexically_normal().native();
            if (error) return;
            if (!normalized.empty()) CharLowerBuffW(normalized.data(), static_cast<DWORD>(normalized.size()));

            std::uint64_t hash = 14695981039346656037ULL;
            for (const wchar_t codeUnit : normalized) {
                hash ^= static_cast<std::uint16_t>(codeUnit);
                hash *= 1099511628211ULL;
            }

            std::wstring name(prefix);
            name.append(std::to_wstring(hash));
            handle = CreateMutexW(nullptr, FALSE, name.c_str());
            if (!handle) return;
            const DWORD wait = WaitForSingleObject(handle, timeout);
            locked = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
        }

        ~NamedPathMutex() {
            if (locked) ReleaseMutex(handle);
            if (handle) CloseHandle(handle);
        }

        NamedPathMutex(const NamedPathMutex&) = delete;
        NamedPathMutex& operator=(const NamedPathMutex&) = delete;

        [[nodiscard]] explicit operator bool() const noexcept { return locked; }

    private:
        HANDLE handle = nullptr;
        bool locked = false;
    };

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
            PWSTR appDataPath = nullptr;
            if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appDataPath))) {
                fail("Could not find your Windows settings folder.");
            }

            auto appPath = std::filesystem::path(appDataPath) / APP_FOLDER_NAME;
            CoTaskMemFree(appDataPath);
            std::error_code ec;
            std::filesystem::create_directories(appPath, ec);
            if (ec) {
                fail(std::string("Could not create the Half Sword Enhancer settings folder: ") + ec.message());
            }
            return appPath;
        }();

        return fullPath;
    }

    [[nodiscard]] inline bool TryGetCurrentExecutablePath(std::filesystem::path& path) noexcept {
        try {
            std::vector<wchar_t> buffer(MAX_PATH);
            for (int attempt = 0; attempt < 8; ++attempt) {
                const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
                if (length == 0) return false;
                if (length < buffer.size()) {
                    path = std::filesystem::path(std::wstring_view(buffer.data(), length));
                    return true;
                }
                buffer.resize(buffer.size() * 2);
            }
        } catch (...) {
            return false;
        }
        return false;
    }

}
