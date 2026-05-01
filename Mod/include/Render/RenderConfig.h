#pragma once

#include <d3d11.h>

namespace RenderConfig {
    constexpr UINT SYNC_TIMEOUT_MS = 500;
    constexpr UINT64 FENCE_INCREMENT = 1;

    constexpr D3D11_VIEWPORT CreateViewport(float width, float height) noexcept {
        return D3D11_VIEWPORT{0.0f, 0.0f, width, height, 0.0f, 1.0f};
    }
}
