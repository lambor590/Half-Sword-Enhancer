#include "../include/InstallManager.h"

#include <Windows.h>

#include <array>
#include <cctype>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "../include/Logger.h"
#include "../include/Util.h"

namespace hse {
    namespace {
        constexpr std::array INSTALL_ARTIFACTS{
            InstallArtifactSpec{InstallArtifact::Proxy, PROXY_FILENAME, 10'000},
            InstallArtifactSpec{InstallArtifact::Mod, MOD_FILENAME, 30'000},
            InstallArtifactSpec{InstallArtifact::Ue4ssBridge, UE4SS_BRIDGE_FILENAME, 1'000},
        };
        constexpr std::uintmax_t MIN_UE4SS_RUNTIME_SIZE = std::uintmax_t{64} * 1024;
        constexpr std::string_view HSE_ENABLED_LINE = "HSEnhancer : 1";

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
            std::error_code error;
            const bool exists = std::filesystem::exists(path, error);
            if (error) Logger::warn("Could not check %s: %s", label, error.message().c_str());
            return exists;
        }

        [[nodiscard]] bool IsPlausibleUe4ssRuntime(const std::filesystem::path& loader) {
            std::error_code error;
            if (!std::filesystem::is_regular_file(loader, error)) return false;
            const auto fileSize = std::filesystem::file_size(loader, error);
            if (error || fileSize < MIN_UE4SS_RUNTIME_SIZE) return false;

            std::ifstream input(loader, std::ios::binary);
            IMAGE_DOS_HEADER dosHeader{};
            input.read(reinterpret_cast<char*>(&dosHeader), sizeof(dosHeader));
            if (!input || dosHeader.e_magic != IMAGE_DOS_SIGNATURE || dosHeader.e_lfanew <= 0) return false;

            const auto ntOffset = static_cast<std::uintmax_t>(dosHeader.e_lfanew);
            if (ntOffset > fileSize || fileSize - ntOffset < sizeof(IMAGE_NT_HEADERS64)) return false;
            input.seekg(dosHeader.e_lfanew, std::ios::beg);
            IMAGE_NT_HEADERS64 ntHeaders{};
            input.read(reinterpret_cast<char*>(&ntHeaders), sizeof(ntHeaders));
            return input && ntHeaders.Signature == IMAGE_NT_SIGNATURE &&
                   ntHeaders.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 &&
                   (ntHeaders.FileHeader.Characteristics & IMAGE_FILE_DLL) != 0 &&
                   ntHeaders.FileHeader.NumberOfSections > 0 &&
                   ntHeaders.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
                   ntHeaders.OptionalHeader.SizeOfImage >= MIN_UE4SS_RUNTIME_SIZE;
        }

        [[nodiscard]] bool IsUe4ssInstalled(const std::filesystem::path& gameBinPath) {
            const auto loader = gameBinPath / "ue4ss" / "UE4SS.dll";
            std::error_code error;
            const bool exists = std::filesystem::exists(loader, error);
            if (error && error != std::errc::no_such_file_or_directory) {
                Logger::warn("Could not check the existing UE4SS installation: %s", error.message().c_str());
                return false;
            }
            if (!exists) return false;
            const bool plausible = IsPlausibleUe4ssRuntime(loader);
            if (!plausible) Logger::warn("The existing UE4SS installation is incomplete and cannot be used");
            return plausible;
        }

        [[nodiscard]] std::optional<std::string_view> HseModValue(std::string_view line) noexcept {
            while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front())))
                line.remove_prefix(1);
            constexpr std::string_view MOD_NAME{UE4SS_MOD_NAME};
            if (!line.starts_with(MOD_NAME)) return std::nullopt;
            line.remove_prefix(MOD_NAME.size());
            if (!line.empty() && line.front() != ':' && !std::isspace(static_cast<unsigned char>(line.front())))
                return std::nullopt;
            while (!line.empty() && (line.front() == ':' || std::isspace(static_cast<unsigned char>(line.front()))))
                line.remove_prefix(1);
            return line;
        }

        [[nodiscard]] bool IsHseModEnabled(const std::filesystem::path& gameBinPath) {
            std::ifstream input(ModsTxtPath(gameBinPath), std::ios::binary);
            if (!input) return false;

            std::string line;
            while (std::getline(input, line)) {
                const auto value = HseModValue(line);
                if (value && value->starts_with('1')) return true;
            }
            return false;
        }

        [[nodiscard]] InstallError FileError(const std::error_code& error) noexcept {
            return error == std::errc::permission_denied ? InstallError::PermissionDenied : InstallError::CopyFailed;
        }

        class InstallTransaction {
        public:
            ~InstallTransaction() {
                if (applied && !finalized) Rollback();
                CleanupStaging();
            }

            InstallTransaction(const InstallTransaction&) = delete;
            InstallTransaction& operator=(const InstallTransaction&) = delete;
            InstallTransaction() = default;

            [[nodiscard]] std::expected<void, InstallError> AddFile(
                const std::filesystem::path& source, const std::filesystem::path& destination
            ) {
                auto change = MakeChange(destination, true);
                if (!change) return std::unexpected(change.error());

                std::error_code error;
                std::filesystem::create_directories(destination.parent_path(), error);
                if (error) return Fail("create a required folder", destination.parent_path(), error);

                std::filesystem::copy_file(source, change->staged, std::filesystem::copy_options::none, error);
                if (error) return Fail("prepare a downloaded file", source, error);
                changes[changeCount++] = std::move(*change);
                return {};
            }

            [[nodiscard]] std::expected<void, InstallError> AddText(
                const std::filesystem::path& destination, std::string_view content
            ) {
                auto change = MakeChange(destination, true);
                if (!change) return std::unexpected(change.error());

                std::error_code error;
                std::filesystem::create_directories(destination.parent_path(), error);
                if (error) return Fail("create a required folder", destination.parent_path(), error);

                std::ofstream output(change->staged, std::ios::binary | std::ios::trunc);
                if (!output) return std::unexpected(InstallError::CopyFailed);
                output.write(content.data(), static_cast<std::streamsize>(content.size()));
                output.close();
                if (!output) return std::unexpected(InstallError::CopyFailed);

                changes[changeCount++] = std::move(*change);
                return {};
            }

            [[nodiscard]] std::expected<void, InstallError> AddRemoval(const std::filesystem::path& destination) {
                auto change = MakeChange(destination, false);
                if (!change) return std::unexpected(change.error());
                changes[changeCount++] = std::move(*change);
                return {};
            }

            [[nodiscard]] std::expected<void, InstallError> Apply() {
                for (std::size_t index = 0; index < changeCount; ++index) {
                    auto& change = changes[index];
                    std::error_code error;
                    change.hadOriginal = std::filesystem::exists(change.destination, error);
                    if (error) return RollbackFailure("check an existing file", change.destination, error);

                    if (change.hadOriginal) {
                        std::filesystem::rename(change.destination, change.backup, error);
                        if (error) return RollbackFailure("prepare an existing file", change.destination, error);
                    }

                    change.applied = true;
                    applied = true;
                    if (!change.replace) continue;

                    std::filesystem::rename(change.staged, change.destination, error);
                    if (error) return RollbackFailure("install a downloaded file", change.destination, error);
                }
                return {};
            }

            void Finalize() noexcept {
                for (std::size_t index = 0; index < changeCount; ++index) {
                    auto& change = changes[index];
                    if (!change.hadOriginal) continue;
                    std::error_code error;
                    std::filesystem::remove(change.backup, error);
                    if (error) Logger::warn("Could not remove a temporary installation file (error %d)", error.value());
                }
                finalized = true;
            }

        private:
            struct Change {
                std::filesystem::path destination;
                std::filesystem::path staged;
                std::filesystem::path backup;
                bool replace = false;
                bool hadOriginal = false;
                bool applied = false;
            };

            std::array<Change, 4> changes{};
            std::size_t changeCount = 0;
            bool applied = false;
            bool finalized = false;

            [[nodiscard]] static std::expected<void, InstallError> Fail(
                const char* action, const std::filesystem::path& path, const std::error_code& error
            ) {
                Logger::error("Could not %s (%s): %s", action, path.string().c_str(), error.message().c_str());
                return std::unexpected(FileError(error));
            }

            [[nodiscard]] static std::expected<Change, InstallError> MakeChange(
                const std::filesystem::path& destination, bool replace
            ) {
                std::error_code error;
                const bool exists = std::filesystem::exists(destination, error);
                if (error) return std::unexpected(FileError(error));
                if (exists && !std::filesystem::is_regular_file(destination, error)) {
                    if (error) return std::unexpected(FileError(error));
                    Logger::error("A required file path is occupied by a folder: %s", destination.string().c_str());
                    return std::unexpected(InstallError::InvalidPath);
                }

                Change change{.destination = destination, .replace = replace};
                change.backup = destination;
                change.backup += L".hse-backup";
                if (replace) {
                    change.staged = destination;
                    change.staged += L".hse-stage";
                }
                return change;
            }

            [[nodiscard]] std::expected<void, InstallError> RollbackFailure(
                const char* action, const std::filesystem::path& path, const std::error_code& error
            ) {
                auto failure = Fail(action, path, error);
                Rollback();
                return failure;
            }

            void Rollback() noexcept {
                for (std::size_t index = changeCount; index-- > 0;) {
                    auto& change = changes[index];
                    if (!change.applied) continue;

                    std::error_code error;
                    if (change.replace) {
                        std::filesystem::remove(change.destination, error);
                        if (error)
                            Logger::error("Could not remove an incomplete installation file (error %d)", error.value());
                    }
                    if (change.hadOriginal) {
                        error.clear();
                        std::filesystem::rename(change.backup, change.destination, error);
                        if (error)
                            Logger::error("Could not restore the previous installation (error %d)", error.value());
                    }
                    change.applied = false;
                }
                applied = false;
            }

            void CleanupStaging() noexcept {
                for (std::size_t index = 0; index < changeCount; ++index) {
                    const auto& change = changes[index];
                    if (change.staged.empty()) continue;
                    std::error_code ignored;
                    std::filesystem::remove(change.staged, ignored);
                }
            }
        };

        [[nodiscard]] std::expected<std::optional<std::string>, InstallError> BuildUe4ssConfig(
            const std::filesystem::path& modsTxt, bool enabled
        ) {
            std::error_code error;
            if (!std::filesystem::exists(modsTxt, error)) {
                if (error) return std::unexpected(FileError(error));
                if (enabled) return std::string(HSE_ENABLED_LINE) + '\n';
                return std::nullopt;
            }

            std::ifstream input(modsTxt, std::ios::binary);
            if (!input) return std::unexpected(InstallError::CopyFailed);
            std::string output;
            std::string line;
            bool found = false;
            bool changed = false;
            while (std::getline(input, line)) {
                if (HseModValue(line)) {
                    changed = true;
                    if (enabled && !found) {
                        output.append(HSE_ENABLED_LINE);
                        output.push_back('\n');
                        found = true;
                    }
                } else if (!line.empty()) {
                    output.append(line);
                    output.push_back('\n');
                }
            }
            if (!input.eof()) return std::unexpected(InstallError::CopyFailed);
            if (enabled && !found) {
                output.append(HSE_ENABLED_LINE);
                output.push_back('\n');
                changed = true;
            }
            if (!changed) return std::nullopt;
            return output;
        }

        [[nodiscard]] std::expected<void, InstallError> ConfigureUe4ss(
            InstallTransaction& transaction, const std::filesystem::path& gameBinPath, bool enabled
        ) {
            const auto modsTxt = ModsTxtPath(gameBinPath);
            auto rewrite = BuildUe4ssConfig(modsTxt, enabled);
            if (!rewrite) return std::unexpected(rewrite.error());
            if (*rewrite) {
                const auto& text = **rewrite;
                auto configChange = text.empty() ? transaction.AddRemoval(modsTxt) : transaction.AddText(modsTxt, text);
                if (!configChange) return configChange;
            }
            if (!enabled) return transaction.AddRemoval(Ue4ssBridgePath(gameBinPath));
            return {};
        }

        [[nodiscard]] std::expected<void, InstallError> ValidateInstallSource(
            const std::filesystem::path& sourcePath, InstallMode mode
        ) {
            for (const auto& artifact : GetInstallPlan(mode)) {
                const auto path = sourcePath / std::filesystem::path(artifact.filename);
                std::error_code error;
                if (!std::filesystem::is_regular_file(path, error)) {
                    if (error)
                        Logger::error(
                            "Could not read the downloaded file %s: %s", path.string().c_str(), error.message().c_str()
                        );
                    else
                        Logger::error("A required downloaded file is missing: %s", path.string().c_str());
                    return std::unexpected(error ? FileError(error) : InstallError::FileNotFound);
                }

                const auto size = std::filesystem::file_size(path, error);
                if (error || size < artifact.minimumFileSize) {
                    Logger::error("A downloaded file is incomplete: %s", path.string().c_str());
                    return std::unexpected(error ? FileError(error) : InstallError::FileNotFound);
                }
            }
            return {};
        }

        [[nodiscard]] std::expected<void, InstallError> BuildInstallTransaction(
            InstallTransaction& transaction, const std::filesystem::path& sourcePath,
            const std::filesystem::path& gameBinPath, InstallMode mode
        ) {
            for (const auto& artifact : GetInstallPlan(mode)) {
                auto staged = transaction.AddFile(
                    sourcePath / std::filesystem::path(artifact.filename),
                    GetInstallDestination(gameBinPath, artifact.artifact)
                );
                if (!staged) return staged;
            }

            if (mode == InstallMode::Ue4ss) {
                if (auto removal = transaction.AddRemoval(gameBinPath / PROXY_FILENAME); !removal) return removal;
                return ConfigureUe4ss(transaction, gameBinPath, true);
            }

            if (auto disabled = ConfigureUe4ss(transaction, gameBinPath, false); !disabled) return disabled;
            return {};
        }
    } // namespace

    std::span<const InstallArtifactSpec, 2> GetInstallPlan(InstallMode mode) noexcept {
        const std::size_t offset = mode == InstallMode::Ue4ss ? 1 : 0;
        return std::span<const InstallArtifactSpec, 2>(INSTALL_ARTIFACTS.data() + offset, 2);
    }

    std::filesystem::path GetInstallDestination(const std::filesystem::path& gameBinPath, InstallArtifact artifact) {
        switch (artifact) {
            case InstallArtifact::Mod: return gameBinPath / MOD_FILENAME;
            case InstallArtifact::Proxy: return gameBinPath / PROXY_FILENAME;
            case InstallArtifact::Ue4ssBridge: return Ue4ssBridgePath(gameBinPath);
        }
        std::unreachable();
    }

    InstallMode DetectInstallMode(const std::filesystem::path& gameBinPath) {
        return IsUe4ssInstalled(gameBinPath) ? InstallMode::Ue4ss : InstallMode::Standalone;
    }

    bool IsInstallModeAvailable(const std::filesystem::path& gameBinPath, InstallMode mode) {
        return mode != InstallMode::Ue4ss || IsUe4ssInstalled(gameBinPath);
    }

    bool IsInstallationComplete(const std::filesystem::path& gameBinPath, InstallMode mode) {
        for (const auto& artifact : GetInstallPlan(mode)) {
            const auto path = GetInstallDestination(gameBinPath, artifact.artifact);
            std::error_code error;
            if (!std::filesystem::is_regular_file(path, error)) {
                if (error)
                    Logger::error(
                        "Could not check the installed file %s: %s", path.string().c_str(), error.message().c_str()
                    );
                return false;
            }
            const auto size = std::filesystem::file_size(path, error);
            if (error || size < artifact.minimumFileSize) return false;
        }
        const bool ue4ssEntryEnabled = IsHseModEnabled(gameBinPath);
        if (mode == InstallMode::Ue4ss) {
            return IsUe4ssInstalled(gameBinPath) && ue4ssEntryEnabled &&
                   !PathExists(gameBinPath / PROXY_FILENAME, "an old direct-install file");
        }
        return !ue4ssEntryEnabled && !PathExists(Ue4ssBridgePath(gameBinPath), "an old UE4SS integration file");
    }

    std::expected<void, InstallError> InstallFiles(
        const std::filesystem::path& sourcePath, const std::filesystem::path& gameBinPath, InstallMode mode
    ) {
        if (!IsInstallModeAvailable(gameBinPath, mode)) return std::unexpected(InstallError::InvalidPath);
        if (auto validation = ValidateInstallSource(sourcePath, mode); !validation) return validation;

        NamedPathMutex lock(gameBinPath, L"Local\\HalfSwordEnhancer.Install.", INFINITE);
        if (!lock) {
            Logger::error("Could not reserve the game folder for installation");
            return std::unexpected(InstallError::PermissionDenied);
        }

        InstallTransaction transaction;
        if (auto prepared = BuildInstallTransaction(transaction, sourcePath, gameBinPath, mode); !prepared)
            return prepared;
        if (auto applied = transaction.Apply(); !applied) return applied;
        if (!IsInstallationComplete(gameBinPath, mode)) {
            Logger::error("The installed files could not be verified. The previous installation will be restored.");
            return std::unexpected(InstallError::CopyFailed);
        }
        transaction.Finalize();

        Logger::info("Half Sword Enhancer installed successfully");
        return {};
    }

    std::expected<bool, InstallError> TestWritePermissions(const std::filesystem::path& gameBinPath) {
        std::error_code error;
        const bool pathExists = std::filesystem::exists(gameBinPath, error);
        if (error) {
            Logger::error("Could not access the game folder: %s", error.message().c_str());
            return std::unexpected(InstallError::PermissionDenied);
        }

        if (!pathExists) return std::unexpected(InstallError::InvalidPath);

        const auto testFile = gameBinPath / ".hse_write_test";
        std::ofstream output(testFile, std::ios::binary);
        if (!output) return false;
        output.close();

        std::filesystem::remove(testFile, error);
        if (error)
            Logger::warn(
                "Could not remove the temporary permission-check file %s: %s", testFile.string().c_str(),
                error.message().c_str()
            );
        return true;
    }
}
