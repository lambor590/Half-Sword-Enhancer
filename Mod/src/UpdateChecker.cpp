#include "Utils/UpdateChecker.h"

#include <Windows.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

#include "ConfigManager.h"
#include "Logger.h"
#include "Version.h"

namespace {
    constexpr char CONFIG_SECTION[] = "Updates";
    constexpr char CONFIG_KEY[] = "check_on_startup";
    constexpr wchar_t USER_AGENT[] = L"Half Sword Enhancer/" HSE_VERSION;
    constexpr wchar_t API_HOST[] = L"api.github.com";
    constexpr int TIMEOUT_MS = 3'000;
    constexpr std::size_t MAX_RESPONSE_BYTES = 1'048'576;

    enum class ReleaseState : std::uint8_t { Unreadable, Current, Newer };

    struct InternetHandleCloser {
        void operator()(HINTERNET handle) const noexcept { WinHttpCloseHandle(handle); }
    };
    using InternetHandle = std::unique_ptr<void, InternetHandleCloser>;

    Logger logger{"UpdateChecker"};
    std::array<char, 32> availableUpdate{};
    std::atomic<bool> updateAvailable{false};

#ifdef EXPERIMENTAL_VERSION
    constexpr wchar_t RELEASE_PATH[] = L"/repos/lambor590/Half-Sword-Enhancer/releases/tags/experimental-latest";
    constexpr std::string_view INSTALLED_BUILD = HSE_STR(HSE_PACKAGE_BUILD_ID);
    constexpr std::size_t BUILD_ID_LENGTH = 40;

    [[nodiscard]] bool IsBuildId(std::string_view value) noexcept {
        return value.size() == BUILD_ID_LENGTH && std::ranges::all_of(value, [](char digit) {
            return (digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f');
        });
    }

    // The experimental workflow republishes one tag and names its commit in a marker inside the release notes.
    [[nodiscard]] ReleaseState CompareRelease(std::string_view json) {
        constexpr std::string_view MARKER = "hse-package:";
        const auto position = json.find(MARKER);
        if (position == std::string_view::npos) return ReleaseState::Unreadable;

        const auto published = json.substr(position + MARKER.size(), BUILD_ID_LENGTH);
        if (!IsBuildId(published)) return ReleaseState::Unreadable;
        if (published == INSTALLED_BUILD) return ReleaseState::Current;

        std::snprintf(availableUpdate.data(), availableUpdate.size(), "New build");
        return ReleaseState::Newer;
    }
#else
    // GitHub answers this with the newest release that is neither a draft nor a pre-release.
    constexpr wchar_t RELEASE_PATH[] = L"/repos/lambor590/Half-Sword-Enhancer/releases/latest";

    using Version = std::tuple<unsigned, unsigned, unsigned>;
    constexpr Version INSTALLED_VERSION{HSE_VERSION_MAJOR, HSE_VERSION_MINOR, HSE_VERSION_PATCH};

    // Only "vMAJOR.MINOR.PATCH" tags count.
    [[nodiscard]] std::optional<Version> ParseReleaseTag(std::string_view json) {
        constexpr std::string_view KEY = "\"tag_name\"";
        auto position = json.find(KEY);
        if (position == std::string_view::npos) return std::nullopt;
        position = json.find_first_not_of(" \t\r\n:", position + KEY.size());
        if (position == std::string_view::npos || json[position] != '"') return std::nullopt;

        const char* cursor = json.data() + position + 1;
        const char* const end = json.data() + json.size();
        if (cursor != end && *cursor == 'v') ++cursor;

        std::array<unsigned, 3> parts{};
        for (std::size_t index = 0; index < parts.size(); ++index) {
            const auto [next, error] = std::from_chars(cursor, end, parts[index]);
            const char separator = index + 1 < parts.size() ? '.' : '"';
            if (error != std::errc{} || next == end || *next != separator) return std::nullopt;
            cursor = next + 1;
        }
        return Version{parts[0], parts[1], parts[2]};
    }

    [[nodiscard]] ReleaseState CompareRelease(std::string_view json) {
        const auto latest = ParseReleaseTag(json);
        if (!latest) return ReleaseState::Unreadable;
        if (*latest <= INSTALLED_VERSION) return ReleaseState::Current;

        const auto [major, minor, patch] = *latest;
        std::snprintf(availableUpdate.data(), availableUpdate.size(), "v%u.%u.%u", major, minor, patch);
        return ReleaseState::Newer;
    }
#endif

    [[nodiscard]] std::optional<std::string> FetchRelease() {
        const InternetHandle session(WinHttpOpen(
            USER_AGENT, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0
        ));
        if (!session) return std::nullopt;
        WinHttpSetTimeouts(session.get(), TIMEOUT_MS, TIMEOUT_MS, TIMEOUT_MS, TIMEOUT_MS);

        const InternetHandle connection(WinHttpConnect(session.get(), API_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0));
        if (!connection) return std::nullopt;

        const InternetHandle request(WinHttpOpenRequest(
            connection.get(), L"GET", RELEASE_PATH, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE
        ));
        if (!request ||
            !WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(request.get(), nullptr))
            return std::nullopt;

        DWORD status = 0;
        DWORD statusSize = sizeof(status);
        if (!WinHttpQueryHeaders(
                request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                &status, &statusSize, WINHTTP_NO_HEADER_INDEX
            ) ||
            status != HTTP_STATUS_OK)
            return std::nullopt;

        std::string body;
        std::array<char, 8192> buffer{};
        for (;;) {
            DWORD bytesRead = 0;
            if (!WinHttpReadData(request.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead))
                return std::nullopt;
            if (bytesRead == 0) return body;
            if (body.size() + bytesRead > MAX_RESPONSE_BYTES) return std::nullopt;
            body.append(buffer.data(), bytesRead);
        }
    }
}

bool UpdateChecker::IsEnabled() {
    return ConfigManager::Get().GetBool(CONFIG_SECTION, CONFIG_KEY, true);
}

void UpdateChecker::SetEnabled(bool enabled) {
    ConfigManager::Get().SetBool(CONFIG_SECTION, CONFIG_KEY, enabled);
}

void UpdateChecker::CheckOnStartup() noexcept {
    try {
        if (!IsEnabled()) return;
#ifdef EXPERIMENTAL_VERSION
        // A build made outside the release workflow has no published commit to compare with.
        if (!IsBuildId(INSTALLED_BUILD)) return;
#endif

        const auto release = FetchRelease();
        switch (release ? CompareRelease(*release) : ReleaseState::Unreadable) {
            case ReleaseState::Unreadable: logger.Log("Could not read the latest release from GitHub."); break;
            case ReleaseState::Current: break;
            case ReleaseState::Newer:
                logger.Log("An update is available: %s.", availableUpdate.data());
                updateAvailable.store(true, std::memory_order_release);
                break;
        }
    } catch (...) {
        logger.Log("The update check failed.");
    }
}

const char* UpdateChecker::AvailableUpdate() noexcept {
    return updateAvailable.load(std::memory_order_acquire) ? availableUpdate.data() : nullptr;
}
