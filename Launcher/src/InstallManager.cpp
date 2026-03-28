#include <fstream>

#include "../include/InstallManager.h"
#include "../include/Logger.h"
#include "../include/Util.h"

namespace hse {

    InstallStatus InstallManager::CheckInstallation(const std::filesystem::path& gameBinPath) const noexcept {
        InstallStatus status;
        try {
            status.proxyInstalled = std::filesystem::exists(gameBinPath / PROXY_FILENAME);
            status.modInstalled = std::filesystem::exists(gameBinPath / MOD_FILENAME);
        } catch (...) {
            Logger::error("Failed to check installation status");
        }
        return status;
    }

    std::expected<void, InstallError> InstallManager::InstallFiles(
        const std::filesystem::path& sourcePath, const std::filesystem::path& gameBinPath
    ) noexcept {
        try {
            std::error_code ec;
            const auto sourceProxy = sourcePath / PROXY_FILENAME;
            if (std::filesystem::exists(sourceProxy)) {
                std::filesystem::copy_file(
                    sourceProxy, gameBinPath / PROXY_FILENAME, std::filesystem::copy_options::overwrite_existing, ec
                );
                if (ec) {
                    Logger::error("Failed to copy proxy: %s", ec.message().c_str());
                    return std::unexpected(InstallError::CopyFailed);
                }
            }

            std::filesystem::copy_file(
                sourcePath / MOD_FILENAME, gameBinPath / MOD_FILENAME,
                std::filesystem::copy_options::overwrite_existing, ec
            );
            if (ec) {
                Logger::error("Failed to copy mod: %s", ec.message().c_str());
                return std::unexpected(InstallError::CopyFailed);
            }

            Logger::info("Files installed successfully");
            return {};
        } catch (const std::filesystem::filesystem_error& e) {
            Logger::error("Filesystem error during installation: %s", e.what());
            return std::unexpected(InstallError::CopyFailed);
        } catch (...) {
            Logger::error("Unknown error during installation");
            return std::unexpected(InstallError::CopyFailed);
        }
    }

    std::expected<bool, InstallError> InstallManager::TestWritePermissions(const std::filesystem::path& gameBinPath
    ) const noexcept {
        try {
            if (!std::filesystem::exists(gameBinPath)) {
                return std::unexpected(InstallError::InvalidPath);
            }

            const auto testFile = gameBinPath / ".hse_write_test";
            std::ofstream ofs(testFile, std::ios::binary);
            if (!ofs) {
                return false;
            }
            ofs.close();

            std::filesystem::remove(testFile);
            return true;
        } catch (...) {
            return std::unexpected(InstallError::PermissionDenied);
        }
    }

}
