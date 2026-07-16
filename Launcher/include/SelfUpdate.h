#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "Version.h"

namespace hse {
    enum class SelfUpdateError : std::uint8_t {
        InvalidPath,
        FileSystemError,
        LaunchFailed,
        InvalidExecutable,
        ProductMismatch,
        VersionMismatch,
        MutexFailed,
        AtomicReplaceFailed,
        ConfirmationFailed,
        ConfirmationTimedOut,
    };

    class SelfUpdateStaging {
    public:
        ~SelfUpdateStaging();
        SelfUpdateStaging(const SelfUpdateStaging&) = delete;
        SelfUpdateStaging& operator=(const SelfUpdateStaging&) = delete;
        SelfUpdateStaging(SelfUpdateStaging&& other) noexcept;
        SelfUpdateStaging& operator=(SelfUpdateStaging&&) = delete;

        [[nodiscard]] const std::filesystem::path& Directory() const noexcept { return directory_; }
        [[nodiscard]] const std::filesystem::path& PayloadPath() const noexcept { return payloadPath_; }
        [[nodiscard]] const std::filesystem::path& TargetPath() const noexcept { return targetPath_; }
        [[nodiscard]] const std::string& Token() const noexcept { return token_; }

        void Release() noexcept { ownsStaging_ = false; }

    private:
        friend std::expected<SelfUpdateStaging, SelfUpdateError> CreateSelfUpdateStaging(
            const std::filesystem::path&, const std::filesystem::path&
        );

        SelfUpdateStaging(std::filesystem::path directory, std::filesystem::path target, std::string token);

        std::filesystem::path directory_;
        std::filesystem::path payloadPath_;
        std::filesystem::path targetPath_;
        std::string token_;
        bool ownsStaging_ = true;
    };

    [[nodiscard]] std::expected<SelfUpdateStaging, SelfUpdateError> CreateSelfUpdateStaging(
        const std::filesystem::path& appDataRoot, const std::filesystem::path& currentExecutable
    );
    [[nodiscard]] std::expected<void, SelfUpdateError> ValidateLauncherPayload(
        const std::filesystem::path& executable, const Version& expectedVersion
    );
    [[nodiscard]] std::expected<void, SelfUpdateError> ApplySelfUpdateWorker(
        const std::filesystem::path& workerExecutable, const std::filesystem::path& target,
        const Version& expectedVersion, std::string_view timestamp = {},
        std::chrono::milliseconds confirmationTimeout = std::chrono::seconds(30), std::uint32_t workerProcessId = 0
    );
    [[nodiscard]] std::expected<void, SelfUpdateError> LaunchSelfUpdateWorker(
        const SelfUpdateStaging& staging, const Version& expectedVersion, std::string_view timestamp,
        std::uint32_t launcherProcessId
    );
    [[nodiscard]] std::optional<int> TryRunSelfUpdateCommand();
}
