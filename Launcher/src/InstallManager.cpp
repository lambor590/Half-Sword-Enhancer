#include <cctype>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "../include/InstallManager.h"
#include "../include/Logger.h"
#include "../include/Util.h"

namespace hse {
    namespace {

        [[nodiscard]] std::filesystem::path Ue4ssModsPath(const std::filesystem::path& gameBinPath) {
            return gameBinPath / "ue4ss" / "Mods";
        }

        [[nodiscard]] std::filesystem::path Ue4ssBridgePath(const std::filesystem::path& gameBinPath) {
            return Ue4ssModsPath(gameBinPath) / UE4SS_MOD_NAME / "dlls" / UE4SS_BRIDGE_FILENAME;
        }

        [[nodiscard]] std::filesystem::path ModsTxtPath(const std::filesystem::path& gameBinPath) {
            return Ue4ssModsPath(gameBinPath) / "mods.txt";
        }

        [[nodiscard]] bool PathExists(const std::filesystem::path& path, const char* label) noexcept {
            std::error_code ec;
            const bool exists = std::filesystem::exists(path, ec);
            if (ec) Logger::warn("Failed to inspect %s: %s", label, ec.message().c_str());
            return exists;
        }

        [[nodiscard]] bool IsHseModLine(std::string_view line) noexcept {
            size_t start = 0;
            while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start])))
                ++start;

            constexpr std::string_view modName{UE4SS_MOD_NAME};
            if (line.substr(start, modName.size()) != modName) return false;

            const size_t end = start + modName.size();
            return end == line.size() || line[end] == ':' || std::isspace(static_cast<unsigned char>(line[end]));
        }

        [[nodiscard]] std::expected<void, InstallError> RemoveIfExists(
            const std::filesystem::path& path, const char* label
        ) noexcept {
            std::error_code ec;
            if (!std::filesystem::exists(path, ec)) {
                if (ec) {
                    Logger::error("Failed to inspect %s: %s", label, ec.message().c_str());
                    return std::unexpected(InstallError::CopyFailed);
                }
                return {};
            }

            std::filesystem::remove(path, ec);
            if (!ec) return {};

            Logger::error("Failed to remove %s: %s", label, ec.message().c_str());
            return std::unexpected(
                ec == std::errc::permission_denied ? InstallError::PermissionDenied : InstallError::CopyFailed
            );
        }

        [[nodiscard]] std::expected<void, InstallError> CopyFromSource(
            const std::filesystem::path& sourcePath, const std::filesystem::path& destination, const char* filename,
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

            std::filesystem::create_directories(destination.parent_path(), ec);
            if (ec) {
                Logger::error(
                    "Failed to create %s: %s", destination.parent_path().string().c_str(), ec.message().c_str()
                );
                return std::unexpected(InstallError::CopyFailed);
            }

            std::filesystem::copy_file(sourceFile, destination, std::filesystem::copy_options::overwrite_existing, ec);
            if (!ec) {
                return {};
            }

            Logger::error("Failed to copy %s: %s", filename, ec.message().c_str());
            return std::unexpected(
                ec == std::errc::permission_denied ? InstallError::PermissionDenied : InstallError::CopyFailed
            );
        }

        [[nodiscard]] std::expected<void, InstallError> EnableUe4ssMod(
            const std::filesystem::path& gameBinPath
        ) noexcept {
            const auto modsTxt = ModsTxtPath(gameBinPath);
            std::error_code ec;
            std::filesystem::create_directories(modsTxt.parent_path(), ec);
            if (ec) {
                Logger::error("Failed to create UE4SS Mods directory: %s", ec.message().c_str());
                return std::unexpected(InstallError::CopyFailed);
            }

            std::vector<std::string> lines;
            {
                std::ifstream input(modsTxt);
                std::string line;
                while (std::getline(input, line))
                    lines.push_back(std::move(line));
            }

            bool found = false;
            for (auto& line : lines) {
                if (!IsHseModLine(line)) continue;
                if (!found) {
                    line = std::string(UE4SS_MOD_NAME) + " : 1";
                    found = true;
                } else {
                    line.clear();
                }
            }
            if (!found) lines.emplace_back(std::string(UE4SS_MOD_NAME) + " : 1");

            std::ofstream output(modsTxt, std::ios::binary | std::ios::trunc);
            if (!output) {
                Logger::error("Failed to open UE4SS mods.txt for writing");
                return std::unexpected(InstallError::CopyFailed);
            }

            for (const auto& line : lines) {
                if (!line.empty()) output << line << '\n';
            }

            if (!output) {
                Logger::error("Failed to write UE4SS mods.txt");
                return std::unexpected(InstallError::CopyFailed);
            }

            return {};
        }

        [[nodiscard]] std::expected<void, InstallError> DisableUe4ssMod(
            const std::filesystem::path& gameBinPath
        ) noexcept {
            const auto modsTxt = ModsTxtPath(gameBinPath);
            std::error_code ec;
            if (!std::filesystem::exists(modsTxt, ec)) {
                if (ec) {
                    Logger::error("Failed to inspect UE4SS mods.txt: %s", ec.message().c_str());
                    return std::unexpected(InstallError::CopyFailed);
                }
                return RemoveIfExists(Ue4ssBridgePath(gameBinPath), "UE4SS bridge");
            }

            std::vector<std::string> lines;
            bool changed = false;
            {
                std::ifstream input(modsTxt);
                std::string line;
                while (std::getline(input, line)) {
                    if (IsHseModLine(line)) {
                        changed = true;
                        continue;
                    }
                    lines.push_back(std::move(line));
                }
            }

            if (changed) {
                if (lines.empty()) {
                    return RemoveIfExists(modsTxt, "UE4SS mods.txt");
                }

                std::ofstream output(modsTxt, std::ios::binary | std::ios::trunc);
                if (!output) {
                    Logger::error("Failed to open UE4SS mods.txt for writing");
                    return std::unexpected(InstallError::CopyFailed);
                }

                for (const auto& line : lines)
                    output << line << '\n';

                if (!output) {
                    Logger::error("Failed to write UE4SS mods.txt");
                    return std::unexpected(InstallError::CopyFailed);
                }
            }

            return RemoveIfExists(Ue4ssBridgePath(gameBinPath), "UE4SS bridge");
        }

        [[nodiscard]] std::expected<void, InstallError> InstallStandaloneFiles(
            const std::filesystem::path& sourcePath, const std::filesystem::path& gameBinPath
        ) noexcept {
            if (auto disableResult = DisableUe4ssMod(gameBinPath); !disableResult) {
                return disableResult;
            }

            if (auto proxyResult = CopyFromSource(sourcePath, gameBinPath / PROXY_FILENAME, PROXY_FILENAME, false);
                !proxyResult) {
                return proxyResult;
            }

            return CopyFromSource(sourcePath, gameBinPath / MOD_FILENAME, MOD_FILENAME, true);
        }

        [[nodiscard]] std::expected<void, InstallError> InstallUe4ssFiles(
            const std::filesystem::path& sourcePath, const std::filesystem::path& gameBinPath
        ) noexcept {
            const auto modsPath = Ue4ssModsPath(gameBinPath);
            Logger::info("Using UE4SS Mods directory: %s", modsPath.string().c_str());

            if (auto removeProxyResult = RemoveIfExists(gameBinPath / PROXY_FILENAME, "standalone proxy");
                !removeProxyResult) {
                return removeProxyResult;
            }

            if (auto modResult = CopyFromSource(sourcePath, gameBinPath / MOD_FILENAME, MOD_FILENAME, true);
                !modResult) {
                return modResult;
            }

            if (auto bridgeResult =
                    CopyFromSource(sourcePath, Ue4ssBridgePath(gameBinPath), UE4SS_BRIDGE_FILENAME, true);
                !bridgeResult) {
                return bridgeResult;
            }

            return EnableUe4ssMod(gameBinPath);
        }

    } // namespace

    InstallMode DetectInstallMode(const std::filesystem::path& gameBinPath) noexcept {
        if (PathExists(gameBinPath / "dwmapi.dll", "dwmapi.dll") ||
            PathExists(gameBinPath / "ue4ss" / "UE4SS.dll", "ue4ss/UE4SS.dll") ||
            PathExists(ModsTxtPath(gameBinPath), "ue4ss/Mods/mods.txt")) {
            return InstallMode::Ue4ss;
        }

        return InstallMode::Standalone;
    }

    bool IsInstallationComplete(const std::filesystem::path& gameBinPath, InstallMode mode) noexcept {
        std::error_code ec;

        const bool proxyInstalled = std::filesystem::exists(gameBinPath / PROXY_FILENAME, ec);
        if (ec) {
            Logger::error("Failed to check proxy installation status: %s", ec.message().c_str());
            ec.clear();
        }

        const bool modInstalled = std::filesystem::exists(gameBinPath / MOD_FILENAME, ec);
        if (ec) {
            Logger::error("Failed to check mod installation status: %s", ec.message().c_str());
            ec.clear();
        }

        const bool bridgeInstalled = std::filesystem::exists(Ue4ssBridgePath(gameBinPath), ec);
        if (ec) {
            Logger::error("Failed to check UE4SS bridge installation status: %s", ec.message().c_str());
        }

        return modInstalled && (mode == InstallMode::Ue4ss ? bridgeInstalled : proxyInstalled);
    }

    std::expected<void, InstallError> InstallFiles(
        const std::filesystem::path& sourcePath, const std::filesystem::path& gameBinPath, InstallMode mode
    ) noexcept {
        auto installResult = mode == InstallMode::Ue4ss ? InstallUe4ssFiles(sourcePath, gameBinPath)
                                                        : InstallStandaloneFiles(sourcePath, gameBinPath);
        if (!installResult) {
            return installResult;
        }

        Logger::info(
            "Files installed successfully (%s)", mode == InstallMode::Ue4ss ? "UE4SS mode" : "standalone mode"
        );
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
