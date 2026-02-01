#pragma once

#include <string>
#include <expected>
#include <chrono>
#include <memory>
#include <cstdint>
#include <Windows.h>

namespace hse {

    enum class ProcessError : std::uint8_t {
        GameNotFound = 1,
        GameStartFailed = 2,
        ProcessOpenFailed = 3,
        MemoryAllocationFailed = 4,
        DllPathWriteFailed = 5,
        ThreadCreationFailed = 6,
        InjectionTimeout = 7,
        InvalidDllPath = 8,
        DllLoadFailed = 9
    };

    class ProcessHandle {
    public:
        explicit ProcessHandle(HANDLE handle = nullptr) noexcept : handle_(handle) {}
        ~ProcessHandle() noexcept { if (handle_ && handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_); }

        ProcessHandle(const ProcessHandle&) = delete;
        ProcessHandle& operator=(const ProcessHandle&) = delete;

        ProcessHandle(ProcessHandle&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
        ProcessHandle& operator=(ProcessHandle&& other) noexcept {
            if (this != &other) {
                if (handle_ && handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
                handle_ = std::exchange(other.handle_, nullptr);
            }
            return *this;
        }

        HANDLE get() const noexcept { return handle_; }
        explicit operator bool() const noexcept { return handle_ && handle_ != INVALID_HANDLE_VALUE; }
        HANDLE release() noexcept { return std::exchange(handle_, nullptr); }

    private:
        HANDLE handle_;
    };

    class ProcessManager {
    public:
        static ProcessManager& Instance() noexcept {
            static ProcessManager instance;
            return instance;
        }

        [[nodiscard]] std::expected<DWORD, ProcessError> LocateOrStartGame() noexcept;
        [[nodiscard]] std::expected<void, ProcessError> InjectDLL(
            DWORD processId,
            std::string_view dllPath
        ) noexcept;

    private:
        static constexpr std::string_view GAME_WINDOW_CLASS = "UnrealWindow";
        static constexpr std::string_view STEAM_GAME_URL = "steam://rungameid/2397300";
        static constexpr std::chrono::milliseconds INJECTION_TIMEOUT{ 10000 };
        static constexpr std::chrono::seconds MAX_GAME_WAIT_TIME{ 60 };

        ProcessManager() = default;
        ~ProcessManager() = default;
        ProcessManager(const ProcessManager&) = delete;
        ProcessManager& operator=(const ProcessManager&) = delete;
        ProcessManager(ProcessManager&&) = delete;
        ProcessManager& operator=(ProcessManager&&) = delete;

        [[nodiscard]] std::expected<DWORD, ProcessError> FindGameProcess() const noexcept;
        [[nodiscard]] std::expected<void, ProcessError> StartGameViaStream() const noexcept;
        [[nodiscard]] std::expected<ProcessHandle, ProcessError> OpenGameProcess(DWORD pid) const noexcept;
    };

}