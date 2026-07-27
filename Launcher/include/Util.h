#pragma once

#include <Windows.h>
#include <ShlObj.h>
#include <KnownFolders.h>
#include <filesystem>
#include <string>
#include <string_view>
#include <cstdint>
#include <utility>
#include <vector>

#include "Logger.h"
#include "GameEdition.h"

namespace hse {

    constexpr const char* APP_FOLDER_NAME = "Half Sword Enhancer";
    constexpr const char* MOD_FILENAME = "HSEnhancer.dll";
    constexpr const char* PROXY_FILENAME = "winmm.dll";
    constexpr const char* UE4SS_BRIDGE_FILENAME = "main.dll";
    constexpr const char* LAUNCHER_FILENAME = "HSEnhancerLauncher.exe";
    constexpr const char* PACKAGE_FILENAME = "HSEnhancer.zip";
    constexpr const char* BUNDLE_FILES_DIRECTORY = "Manual Install";
    constexpr const char* PACKAGE_MANIFEST_FILENAME = "package.ini";
    constexpr const char* UE4SS_MOD_NAME = "HSEnhancer";

    [[nodiscard]] inline std::string PathToUtf8(const std::filesystem::path& path) {
        const auto text = path.u8string();
        return {text.begin(), text.end()};
    }

    [[nodiscard]] inline std::filesystem::path PathFromUtf8(std::string_view text) {
        return std::filesystem::path(std::u8string(text.begin(), text.end()));
    }

    class ScopedHandle final {
    public:
        explicit ScopedHandle(HANDLE initialValue) noexcept : value(initialValue) {}
        ~ScopedHandle() {
            if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value);
        }

        ScopedHandle(const ScopedHandle&) = delete;
        ScopedHandle& operator=(const ScopedHandle&) = delete;
        ScopedHandle(ScopedHandle&& other) noexcept : value(std::exchange(other.value, nullptr)) {}
        ScopedHandle& operator=(ScopedHandle&&) = delete;

        [[nodiscard]] HANDLE Get() const noexcept { return value; }
        [[nodiscard]] explicit operator bool() const noexcept {
            return value && value != INVALID_HANDLE_VALUE;
        }

    private:
        HANDLE value = nullptr;
    };

    class ScopedDirectory final {
    public:
        explicit ScopedDirectory(std::filesystem::path initialPath) : path(std::move(initialPath)) {}
        ~ScopedDirectory() {
            if (!owns) return;
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }

        ScopedDirectory(const ScopedDirectory&) = delete;
        ScopedDirectory& operator=(const ScopedDirectory&) = delete;
        ScopedDirectory(ScopedDirectory&& other) noexcept : path(std::move(other.path)), owns(other.owns) {
            other.owns = false;
        }
        ScopedDirectory& operator=(ScopedDirectory&&) = delete;

        [[nodiscard]] const std::filesystem::path& Path() const noexcept { return path; }
        void Release() noexcept { owns = false; }

    private:
        std::filesystem::path path;
        bool owns = true;
    };

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
