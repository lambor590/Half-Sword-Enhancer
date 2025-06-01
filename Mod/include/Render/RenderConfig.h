#pragma once

#include <d3d11.h>
#include <d3d12.h>

namespace RenderConfig {
    constexpr UINT MAX_RENDER_TARGETS = 8;
    constexpr UINT SYNC_TIMEOUT_MS = 500;
    constexpr UINT CACHE_LINE_SIZE = 64;
    
    constexpr D3D12_DESCRIPTOR_HEAP_DESC RTV_HEAP_DESC = {
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        MAX_RENDER_TARGETS,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        0
    };
    
    constexpr D3D11_RESOURCE_FLAGS RT_FLAGS = { D3D11_BIND_RENDER_TARGET };
    
    constexpr D3D11_VIEWPORT CreateViewport(float width, float height) noexcept {
        return D3D11_VIEWPORT{ 0.0f, 0.0f, width, height, 0.0f, 1.0f };
    }
} 