#pragma once

namespace UpdateChecker {
    [[nodiscard]] bool IsEnabled();
    void SetEnabled(bool enabled);

    // Worker thread. Blocks until GitHub answers or the request times out.
    void CheckOnStartup() noexcept;

    // Null until a newer stable release is known. The text then stays valid for the process lifetime.
    [[nodiscard]] const char* AvailableVersion() noexcept;
}
