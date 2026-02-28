#pragma once

#include <expected>
#include <filesystem>
#include <cstdint>

namespace hse {

    enum class InstallError : std::uint8_t {
        FileNotFound,
        CopyFailed,
        PermissionDenied,
        InvalidPath
    };

    struct InstallStatus {
        bool proxyInstalled = false;
        bool modInstalled = false;
        [[nodiscard]] bool fullyInstalled() const noexcept { return proxyInstalled && modInstalled; }
    };

    class InstallManager {
    public:
        static InstallManager& Instance() noexcept {
            static InstallManager instance;
            return instance;
        }

        [[nodiscard]] InstallStatus CheckInstallation(
            const std::filesystem::path& gameBinPath
        ) const noexcept;

        [[nodiscard]] std::expected<void, InstallError> InstallFiles(
            const std::filesystem::path& sourcePath,
            const std::filesystem::path& gameBinPath
        ) noexcept;

        [[nodiscard]] std::expected<bool, InstallError> TestWritePermissions(
            const std::filesystem::path& gameBinPath
        ) const noexcept;

    private:
        InstallManager() = default;
    };

}
