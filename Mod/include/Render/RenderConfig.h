#pragma once

#include <d3d11.h>
#include <d3d12.h>

namespace RenderConfig {
    constexpr UINT SYNC_TIMEOUT_MS = 500;
    constexpr UINT CACHE_LINE_SIZE = 64;
    constexpr UINT64 FENCE_INCREMENT = 1;
    constexpr bool ENABLE_DEBUG_LAYER = false;

    constexpr D3D11_RESOURCE_FLAGS RT_FLAGS = {D3D11_BIND_RENDER_TARGET};

    constexpr D3D12_RESOURCE_STATES D3D12_RT_STATE = D3D12_RESOURCE_STATE_RENDER_TARGET;
    constexpr D3D12_RESOURCE_STATES D3D12_PRESENT_STATE = D3D12_RESOURCE_STATE_PRESENT;
    constexpr D3D11_CREATE_DEVICE_FLAG D3D11_FLAGS = static_cast<D3D11_CREATE_DEVICE_FLAG>(
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | (ENABLE_DEBUG_LAYER ? D3D11_CREATE_DEVICE_DEBUG : 0)
    );

    constexpr D3D11_VIEWPORT CreateViewport(float width, float height) noexcept {
        return D3D11_VIEWPORT{0.0f, 0.0f, width, height, 0.0f, 1.0f};
    }
}
