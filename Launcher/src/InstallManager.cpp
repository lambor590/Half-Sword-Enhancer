#include <fstream>

#include "../include/InstallManager.h"
#include "../include/Logger.h"
#include "../include/Util.h"

namespace hse {
    namespace {

        [[nodiscard]] std::expected<void, InstallError> CopyIntoGameDirectory(
            const std::filesystem::path& sourcePath, const std::filesystem::path& gameBinPath, const char* filename,
            bool required
        ) noexcept {
            std::error_code ec;
            const auto sourceFile = sourcePath / filename;

            if (!std::filesystem::exists(sourceFile, ec)) {
                if (ec) {
                    Logger::error("Failed to inspect %s: %s", filename, ec.message().c_str());
                    return std::unexpected(InstallError::CopyFailed);
                }

                if (required) {
                    Logger::error("Required install file missing: %s", sourceFile.string().c_str());
                    return std::unexpected(InstallError::FileNotFound);
                }

                return {};
            }

            std::filesystem::copy_file(
                sourceFile, gameBinPath / filename, std::filesystem::copy_options::overwrite_existing, ec
            );
            if (!ec) {
                return {};
            }

            Logger::error("Failed to copy %s: %s", filename, ec.message().c_str());
            return std::unexpected(ec == std::errc::permission_denied ? InstallError::PermissionDenied
                                                                       : InstallError::CopyFailed);
        }

    }

    InstallStatus CheckInstallation(const std::filesystem::path& gameBinPath) noexcept {
        InstallStatus status;
        std::error_code ec;

        status.proxyInstalled = std::filesystem::exists(gameBinPath / PROXY_FILENAME, ec);
        if (ec) {
            Logger::error("Failed to check proxy installation status: %s", ec.message().c_str());
            ec.clear();
        }

        status.modInstalled = std::filesystem::exists(gameBinPath / MOD_FILENAME, ec);
        if (ec) {
            Logger::error("Failed to check mod installation status: %s", ec.message().c_str());
        }

        return status;
    }

    std::expected<void, InstallError> InstallFiles(const std::filesystem::path& sourcePath, const std::filesystem::path& gameBinPath
    ) noexcept {
        if (auto proxyResult = CopyIntoGameDirectory(sourcePath, gameBinPath, PROXY_FILENAME, false); !proxyResult) {
            return proxyResult;
        }

        if (auto modResult = CopyIntoGameDirectory(sourcePath, gameBinPath, MOD_FILENAME, true); !modResult) {
            return modResult;
        }

        Logger::info("Files installed successfully");
        return {};
    }

    std::expected<bool, InstallError> TestWritePermissions(const std::filesystem::path& gameBinPath) noexcept {
        std::error_code ec;
        const bool pathExists = std::filesystem::exists(gameBinPath, ec);
        if (ec) {
            Logger::error("Failed to access game directory: %s", ec.message().c_str());
            return std::unexpected(InstallError::PermissionDenied);
        }

        if (!pathExists) {
            return std::unexpected(InstallError::InvalidPath);
        }

        const auto testFile = gameBinPath / ".hse_write_test";
        std::ofstream ofs(testFile, std::ios::binary);
        if (!ofs) {
            return false;
        }
        ofs.close();

        std::filesystem::remove(testFile, ec);
        if (ec) {
            Logger::warn("Failed to remove write test file: %s", ec.message().c_str());
        }

        return true;
    }

}
