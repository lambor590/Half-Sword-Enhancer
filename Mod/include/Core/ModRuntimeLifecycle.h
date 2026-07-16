#pragma once

namespace ModRuntimeLifecycle {
    void StartAsync() noexcept;
    [[nodiscard]] bool Stop() noexcept;
}
