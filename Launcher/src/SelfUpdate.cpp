#include "../include/SelfUpdate.h"

#include <Windows.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "../include/LauncherConfig.h"
#include "../include/Util.h"

namespace hse {
    namespace {
        constexpr std::wstring_view EXPECTED_PRODUCT_NAME = L"Half Sword Enhancer Launcher";
        constexpr DWORD PARENT_WAIT_MILLISECONDS = 120'000;
        constexpr DWORD WORKER_READY_MILLISECONDS = 10'000;
        constexpr DWORD CLEANUP_WAIT_MILLISECONDS = 60'000;

        struct SiblingArtifacts {
            std::filesystem::path backup;
            std::filesystem::path candidate;
        };

        [[nodiscard]] bool IsSafeToken(std::string_view token) noexcept {
            return !token.empty() && token.size() <= 64 && std::ranges::all_of(token, [](char value) {
                return (value >= '0' && value <= '9') || value == '-';
            });
        }

        [[nodiscard]] bool IsSafeBuildId(std::string_view buildId) noexcept {
            return buildId.size() <= 64 && std::ranges::all_of(buildId, [](char value) {
                       return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
                              (value >= '0' && value <= '9') || value == '-' || value == '_' || value == '.';
                   });
        }

        [[nodiscard]] std::optional<std::string> NarrowAscii(std::wstring_view value) {
            std::string result;
            result.reserve(value.size());
            for (const wchar_t character : value) {
                if (character > 0x7f) return std::nullopt;
                result.push_back(static_cast<char>(character));
            }
            return result;
        }

        void AppendQuoted(std::wstring& command, std::wstring_view value) {
            command.push_back(L'"');
            command.append(value);
            command.push_back(L'"');
        }

        void AppendQuotedAscii(std::wstring& command, std::string_view value) {
            command.push_back(L'"');
            for (const char character : value)
                command.push_back(static_cast<unsigned char>(character));
            command.push_back(L'"');
        }

        [[nodiscard]] std::optional<ScopedHandle> StartProcess(
            const std::filesystem::path& executable, std::wstring command, DWORD creationFlags
        ) {
            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION process{};
            if (!CreateProcessW(
                    executable.c_str(), command.data(), nullptr, nullptr, FALSE, creationFlags, nullptr, nullptr,
                    &startup, &process
                ))
                return std::nullopt;
            CloseHandle(process.hThread);
            return ScopedHandle(process.hProcess);
        }

        [[nodiscard]] SiblingArtifacts GetSiblingArtifacts(
            const std::filesystem::path& target, std::string_view token
        ) {
            SiblingArtifacts artifacts{.backup = target, .candidate = target};
            const std::wstring suffix(token.begin(), token.end());
            artifacts.backup += L".hse-backup-" + suffix;
            artifacts.candidate += L".hse-candidate-" + suffix;
            return artifacts;
        }

        void RemoveFile(const std::filesystem::path& path) noexcept {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }

        void RemoveTree(const std::filesystem::path& path) noexcept {
            try {
                std::error_code ignored;
                std::filesystem::remove_all(path, ignored);
            } catch (...) {
                OutputDebugStringW(L"Half Sword Enhancer: update directory cleanup failed.\n");
            }
        }

        [[nodiscard]] bool IsOwnedStagingDirectory(const std::filesystem::path& directory) {
            if (!IsSafeToken(directory.filename().string())) return false;
            std::error_code error;
            const auto root = getAppDataDirectory() / "updates" / "launcher";
            return std::filesystem::equivalent(directory.parent_path(), root, error) && !error;
        }

        [[nodiscard]] bool FlushPath(const std::filesystem::path& path) noexcept {
            const ScopedHandle file(CreateFileW(
                path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr
            ));
            return file && FlushFileBuffers(file.Get());
        }

        [[nodiscard]] std::optional<unsigned long> ParseUnsigned(std::wstring_view value) noexcept {
            if (value.empty()) return std::nullopt;
            unsigned long result = 0;
            for (const wchar_t character : value) {
                if (character < L'0' || character > L'9') return std::nullopt;
                const auto digit = static_cast<unsigned long>(character - L'0');
                if (result > ((std::numeric_limits<unsigned long>::max)() - digit) / 10) return std::nullopt;
                result = result * 10 + digit;
            }
            return result;
        }

        [[nodiscard]] bool HasExpectedProductName(void* versionData) noexcept {
            wchar_t* product = nullptr;
            UINT characters = 0;
            return VerQueryValueW(
                       versionData, L"\\StringFileInfo\\040904b0\\ProductName", reinterpret_cast<void**>(&product),
                       &characters
                   ) &&
                   product && characters > 0 && std::wstring_view(product, characters - 1) == EXPECTED_PRODUCT_NAME;
        }

        [[nodiscard]] std::wstring ReadyEventName(std::uint32_t processId, std::string_view token) {
            return L"Local\\HalfSwordEnhancer.SelfUpdate.Ready." + std::to_wstring(processId) + L"." +
                   std::wstring(token.begin(), token.end());
        }

        [[nodiscard]] std::wstring ConfirmationEventName(std::uint32_t processId, std::string_view token) {
            return L"Local\\HalfSwordEnhancer.SelfUpdate.Confirmed." + std::to_wstring(processId) + L"." +
                   std::wstring(token.begin(), token.end());
        }

        void CleanupArtifacts(
            const std::filesystem::path& stagingDirectory, const std::filesystem::path& target
        ) noexcept {
            try {
                if (!IsSafeToken(stagingDirectory.filename().string())) return;
                const auto artifacts = GetSiblingArtifacts(target, stagingDirectory.filename().string());
                RemoveFile(artifacts.backup);
                RemoveFile(artifacts.candidate);
                RemoveTree(stagingDirectory);
            } catch (...) {
                OutputDebugStringW(L"Half Sword Enhancer: update artifact cleanup failed.\n");
            }
        }

        [[nodiscard]] bool LaunchForCleanup(
            const std::filesystem::path& target, const std::filesystem::path& stagingDirectory,
            std::uint32_t workerProcessId
        ) {
            std::wstring command;
            command.reserve(target.native().size() + stagingDirectory.native().size() + 64);
            AppendQuoted(command, target.native());
            command.append(L" --hse-cleanup-update ");
            AppendQuoted(command, stagingDirectory.native());
            command.push_back(L' ');
            command.append(std::to_wstring(workerProcessId));
            return StartProcess(target, std::move(command), CREATE_NEW_CONSOLE).has_value();
        }

        enum class ConfirmationResult : std::uint8_t { Success, Failed, TimedOut };

        [[nodiscard]] ConfirmationResult RunConfirmation(
            const std::filesystem::path& target, const Version& expectedVersion, std::string_view buildId,
            const std::filesystem::path& stagingDirectory, std::uint32_t workerProcessId,
            std::chrono::milliseconds timeout
        ) {
            const auto token = stagingDirectory.filename().string();
            const auto eventName = ConfirmationEventName(GetCurrentProcessId(), token);
            SetLastError(ERROR_SUCCESS);
            const ScopedHandle confirmationEvent(CreateEventW(nullptr, TRUE, FALSE, eventName.c_str()));
            if (!confirmationEvent || GetLastError() == ERROR_ALREADY_EXISTS) return ConfirmationResult::Failed;

            const auto version = expectedVersion.ToString();
            std::wstring command;
            command.reserve(target.native().size() + stagingDirectory.native().size() + eventName.size() + 96);
            AppendQuoted(command, target.native());
            command.append(L" --hse-confirm-update ");
            AppendQuotedAscii(command, version);
            command.push_back(L' ');
            AppendQuotedAscii(command, buildId);
            command.push_back(L' ');
            AppendQuoted(command, eventName);
            command.push_back(L' ');
            AppendQuoted(command, stagingDirectory.native());
            command.push_back(L' ');
            command.append(std::to_wstring(workerProcessId));
            const DWORD creationFlags = workerProcessId == 0 ? CREATE_NO_WINDOW : CREATE_NEW_CONSOLE;
            auto confirmationProcess = StartProcess(target, std::move(command), creationFlags);
            if (!confirmationProcess) return ConfirmationResult::Failed;
            const std::array waits{confirmationEvent.Get(), confirmationProcess->Get()};
            const DWORD wait = WaitForMultipleObjects(
                static_cast<DWORD>(waits.size()), waits.data(), FALSE,
                timeout.count() > 0 ? static_cast<DWORD>(timeout.count()) : 0
            );
            if (wait == WAIT_OBJECT_0) return ConfirmationResult::Success;
            if (wait == WAIT_TIMEOUT) {
                TerminateProcess(confirmationProcess->Get(), 1);
                WaitForSingleObject(confirmationProcess->Get(), 5'000);
                return ConfirmationResult::TimedOut;
            }
            return ConfirmationResult::Failed;
        }

        [[nodiscard]] std::optional<int> RunApplyCommand(wchar_t** arguments, int argumentCount) {
            if (argumentCount != 7) return 1;
            const auto versionText = NarrowAscii(arguments[3]);
            const auto buildId = NarrowAscii(arguments[4]);
            const auto parentProcessId = ParseUnsigned(arguments[5]);
            std::filesystem::path workerExecutable;
            if (!versionText || !buildId || !parentProcessId || !TryGetCurrentExecutablePath(workerExecutable))
                return 1;

            const Version version(*versionText);
            const auto stagingDirectory = workerExecutable.parent_path();
            const auto token = stagingDirectory.filename().string();
            if (!version.IsValid() || !IsSafeBuildId(*buildId) || !IsOwnedStagingDirectory(stagingDirectory) ||
                std::wstring_view(arguments[6]) != ReadyEventName(*parentProcessId, token))
                return 1;

            const ScopedHandle parent(OpenProcess(SYNCHRONIZE, FALSE, *parentProcessId));
            const ScopedHandle ready(OpenEventW(EVENT_MODIFY_STATE, FALSE, arguments[6]));
            if (!parent || !ready || !SetEvent(ready.Get())) return 1;
            if (WaitForSingleObject(parent.Get(), PARENT_WAIT_MILLISECONDS) != WAIT_OBJECT_0) return 1;
            return ApplySelfUpdateWorker(
                       workerExecutable, arguments[2], version, *buildId, std::chrono::seconds(30),
                       GetCurrentProcessId()
                   )
                       ? 0
                       : 1;
        }

        [[nodiscard]] std::optional<int> RunConfirmationCommand(wchar_t** arguments, int argumentCount) {
            if (argumentCount != 7) return 1;
            const auto versionText = NarrowAscii(arguments[2]);
            const auto buildId = NarrowAscii(arguments[3]);
            const auto workerProcessId = ParseUnsigned(arguments[6]);
            std::filesystem::path currentExecutable;
            if (!versionText || !buildId || !workerProcessId || !TryGetCurrentExecutablePath(currentExecutable))
                return 1;
            const Version version(*versionText);
            const std::filesystem::path stagingDirectory(arguments[5]);
            if (!version.IsValid() || !IsSafeBuildId(*buildId) || !ValidateLauncherPayload(currentExecutable, version))
                return 1;
            if (*workerProcessId != 0 &&
                (!IsOwnedStagingDirectory(stagingDirectory) ||
                 std::wstring_view(arguments[4]) !=
                     ConfirmationEventName(*workerProcessId, stagingDirectory.filename().string())))
                return 1;
            if (!buildId->empty() &&
                !LauncherConfig::Instance().SetString("ExperimentalUpdate", "launcher_build", *buildId).has_value())
                return 1;

            const ScopedHandle confirmed(OpenEventW(EVENT_MODIFY_STATE, FALSE, arguments[4]));
            if (!confirmed || !SetEvent(confirmed.Get())) return 1;
            if (*workerProcessId == 0) return 0;
            const ScopedHandle worker(OpenProcess(SYNCHRONIZE, FALSE, *workerProcessId));
            if (worker) WaitForSingleObject(worker.Get(), CLEANUP_WAIT_MILLISECONDS);
            CleanupArtifacts(stagingDirectory, currentExecutable);
            return std::nullopt;
        }

        [[nodiscard]] std::optional<int> RunCleanupCommand(wchar_t** arguments, int argumentCount) {
            if (argumentCount != 4) return 1;
            const std::filesystem::path stagingDirectory(arguments[2]);
            const auto workerProcessId = ParseUnsigned(arguments[3]);
            std::filesystem::path currentExecutable;
            if (!workerProcessId || *workerProcessId == 0 || !IsOwnedStagingDirectory(stagingDirectory) ||
                !TryGetCurrentExecutablePath(currentExecutable))
                return 1;
            const ScopedHandle worker(OpenProcess(SYNCHRONIZE, FALSE, *workerProcessId));
            if (worker) WaitForSingleObject(worker.Get(), CLEANUP_WAIT_MILLISECONDS);
            CleanupArtifacts(stagingDirectory, currentExecutable);
            return std::nullopt;
        }
    }

    SelfUpdateStaging::SelfUpdateStaging(
        std::filesystem::path directory, std::filesystem::path target, std::string token
    )
        : directory_(std::move(directory)),
          payloadPath_(directory_ / "launcher-update.exe"),
          targetPath_(std::move(target)),
          token_(std::move(token)) {}

    SelfUpdateStaging::~SelfUpdateStaging() {
        if (ownsStaging_) CleanupArtifacts(directory_, targetPath_);
    }

    SelfUpdateStaging::SelfUpdateStaging(SelfUpdateStaging&& other) noexcept
        : directory_(std::move(other.directory_)),
          payloadPath_(std::move(other.payloadPath_)),
          targetPath_(std::move(other.targetPath_)),
          token_(std::move(other.token_)),
          ownsStaging_(std::exchange(other.ownsStaging_, false)) {}

    std::expected<SelfUpdateStaging, SelfUpdateError> CreateSelfUpdateStaging(
        const std::filesystem::path& appDataRoot, const std::filesystem::path& currentExecutable
    ) {
        std::error_code error;
        if (!std::filesystem::is_regular_file(currentExecutable, error) || error)
            return std::unexpected(SelfUpdateError::InvalidPath);
        const auto root = appDataRoot / "updates" / "launcher";
        std::filesystem::create_directories(root, error);
        if (error) return std::unexpected(SelfUpdateError::FileSystemError);

        static std::atomic<std::uint64_t> nextId{1};
        const auto token = std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()) + "-" +
                           std::to_string(nextId.fetch_add(1, std::memory_order_relaxed));
        const auto directory = root / token;
        if (!std::filesystem::create_directory(directory, error))
            return std::unexpected(SelfUpdateError::FileSystemError);
        return SelfUpdateStaging(directory, currentExecutable, token);
    }

    std::expected<void, SelfUpdateError> ValidateLauncherPayload(
        const std::filesystem::path& executable, const Version& expectedVersion
    ) {
        if (!expectedVersion.IsValid()) return std::unexpected(SelfUpdateError::VersionMismatch);
        const DWORD bytes = GetFileVersionInfoSizeW(executable.c_str(), nullptr);
        if (bytes == 0) return std::unexpected(SelfUpdateError::InvalidExecutable);
        std::vector<BYTE> versionData(bytes);
        if (!GetFileVersionInfoW(executable.c_str(), 0, bytes, versionData.data()))
            return std::unexpected(SelfUpdateError::InvalidExecutable);
        VS_FIXEDFILEINFO* fileInfo = nullptr;
        UINT fileInfoBytes = 0;
        if (!VerQueryValueW(versionData.data(), L"\\", reinterpret_cast<void**>(&fileInfo), &fileInfoBytes) ||
            !fileInfo || fileInfoBytes < sizeof(VS_FIXEDFILEINFO) || fileInfo->dwSignature != 0xFEEF04BD)
            return std::unexpected(SelfUpdateError::InvalidExecutable);
        if (!HasExpectedProductName(versionData.data())) return std::unexpected(SelfUpdateError::ProductMismatch);
        const Version actualVersion(
            static_cast<std::uint16_t>(HIWORD(fileInfo->dwFileVersionMS)),
            static_cast<std::uint16_t>(LOWORD(fileInfo->dwFileVersionMS)),
            static_cast<std::uint16_t>(HIWORD(fileInfo->dwFileVersionLS))
        );
        if (actualVersion != expectedVersion) return std::unexpected(SelfUpdateError::VersionMismatch);
        return {};
    }

    std::expected<void, SelfUpdateError> ApplySelfUpdateWorker(
        const std::filesystem::path& workerExecutable, const std::filesystem::path& target,
        const Version& expectedVersion, std::string_view buildId, std::chrono::milliseconds confirmationTimeout,
        std::uint32_t workerProcessId
    ) {
        const auto stagingDirectory = workerExecutable.parent_path();
        const auto token = stagingDirectory.filename().string();
        if (!IsSafeToken(token) || !IsSafeBuildId(buildId) || workerExecutable.filename() != L"launcher-update.exe")
            return std::unexpected(SelfUpdateError::InvalidPath);
        if (auto validation = ValidateLauncherPayload(workerExecutable, expectedVersion); !validation)
            return validation;

        const auto restart = [&] {
            if (workerProcessId != 0) (void)LaunchForCleanup(target, stagingDirectory, workerProcessId);
        };
        NamedPathMutex mutex(target, L"Local\\HalfSwordEnhancer.SelfUpdate.", PARENT_WAIT_MILLISECONDS);
        if (!mutex) {
            restart();
            return std::unexpected(SelfUpdateError::MutexFailed);
        }

        const auto artifacts = GetSiblingArtifacts(target, token);
        RemoveFile(artifacts.candidate);
        RemoveFile(artifacts.backup);
        if (!CopyFileW(workerExecutable.c_str(), artifacts.candidate.c_str(), TRUE) ||
            !FlushPath(artifacts.candidate)) {
            restart();
            return std::unexpected(SelfUpdateError::FileSystemError);
        }
        if (!ReplaceFileW(
                target.c_str(), artifacts.candidate.c_str(), artifacts.backup.c_str(), REPLACEFILE_WRITE_THROUGH,
                nullptr, nullptr
            )) {
            const DWORD replaceError = GetLastError();
            const bool targetAvailable =
                replaceError != ERROR_UNABLE_TO_MOVE_REPLACEMENT_2 ||
                MoveFileExW(
                    artifacts.backup.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
                ) != FALSE;
            RemoveFile(artifacts.candidate);
            if (targetAvailable) restart();
            return std::unexpected(SelfUpdateError::AtomicReplaceFailed);
        }

        const auto confirmation =
            RunConfirmation(target, expectedVersion, buildId, stagingDirectory, workerProcessId, confirmationTimeout);
        if (confirmation != ConfirmationResult::Success) {
            const bool restored =
                ReplaceFileW(
                    target.c_str(), artifacts.backup.c_str(), nullptr, REPLACEFILE_WRITE_THROUGH, nullptr, nullptr
                ) != FALSE;
            RemoveFile(artifacts.candidate);
            if (restored) RemoveFile(artifacts.backup);
            restart();
            if (!restored) return std::unexpected(SelfUpdateError::AtomicReplaceFailed);
            return std::unexpected(
                confirmation == ConfirmationResult::TimedOut ? SelfUpdateError::ConfirmationTimedOut
                                                             : SelfUpdateError::ConfirmationFailed
            );
        }

        RemoveFile(artifacts.backup);
        RemoveFile(artifacts.candidate);
        return {};
    }

    std::expected<void, SelfUpdateError> LaunchSelfUpdateWorker(
        const SelfUpdateStaging& staging, const Version& expectedVersion, std::string_view buildId,
        std::uint32_t launcherProcessId
    ) {
        if (launcherProcessId == 0 || !expectedVersion.IsValid() || !IsSafeBuildId(buildId))
            return std::unexpected(SelfUpdateError::InvalidExecutable);
        const auto eventName = ReadyEventName(launcherProcessId, staging.Token());
        SetLastError(ERROR_SUCCESS);
        const ScopedHandle ready(CreateEventW(nullptr, TRUE, FALSE, eventName.c_str()));
        if (!ready || GetLastError() == ERROR_ALREADY_EXISTS) return std::unexpected(SelfUpdateError::LaunchFailed);

        const auto version = expectedVersion.ToString();
        std::wstring command;
        command.reserve(
            staging.PayloadPath().native().size() + staging.TargetPath().native().size() + eventName.size() + 96
        );
        AppendQuoted(command, staging.PayloadPath().native());
        command.append(L" --hse-apply-update ");
        AppendQuoted(command, staging.TargetPath().native());
        command.push_back(L' ');
        AppendQuotedAscii(command, version);
        command.push_back(L' ');
        AppendQuotedAscii(command, buildId);
        command.push_back(L' ');
        command.append(std::to_wstring(launcherProcessId));
        command.push_back(L' ');
        AppendQuoted(command, eventName);
        auto worker = StartProcess(staging.PayloadPath(), std::move(command), CREATE_NO_WINDOW);
        if (!worker) return std::unexpected(SelfUpdateError::LaunchFailed);
        if (WaitForSingleObject(ready.Get(), WORKER_READY_MILLISECONDS) != WAIT_OBJECT_0) {
            TerminateProcess(worker->Get(), 1);
            WaitForSingleObject(worker->Get(), 5'000);
            return std::unexpected(SelfUpdateError::LaunchFailed);
        }
        return {};
    }

    std::optional<int> TryRunSelfUpdateCommand() {
        if (std::wstring_view(GetCommandLineW()).find(L"--hse-") == std::wstring_view::npos) return std::nullopt;
        int argumentCount = 0;
        wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
        if (!arguments) return 1;
        std::optional<int> result;
        if (argumentCount >= 2) {
            const std::wstring_view command(arguments[1]);
            if (command == L"--hse-apply-update")
                result = RunApplyCommand(arguments, argumentCount);
            else if (command == L"--hse-confirm-update")
                result = RunConfirmationCommand(arguments, argumentCount);
            else if (command == L"--hse-cleanup-update")
                result = RunCleanupCommand(arguments, argumentCount);
        }
        LocalFree(reinterpret_cast<HLOCAL>(arguments));
        return result;
    }
}
