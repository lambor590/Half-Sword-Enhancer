#pragma once

namespace UpdateChecker {
    [[nodiscard]] bool IsEnabled();
    void SetEnabled(bool enabled);

    // Worker thread. Blocks until GitHub answers or the request times out.
    void CheckOnStartup() noexcept;

    // Null until an update is known, then its short name ("v1.2.3", or "New build" on the experimental channel).
    // The text stays valid for the process lifetime.
    [[nodiscard]] const char* AvailableUpdate() noexcept;
}
