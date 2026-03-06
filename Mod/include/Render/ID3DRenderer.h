#pragma once

#include <d3d11.h>
#include <dxgi1_4.h>

#include "IRenderCallback.h"

class ID3DRenderer
{
public:
    virtual void OnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags) noexcept = 0;
    virtual void OnResizeBuffers(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT) noexcept {};
    virtual void SetCommandQueue(ID3D12CommandQueue*) noexcept {};
    virtual void SetGetCommandQueueCallback(void (*)()) noexcept {};
    virtual void Cleanup() noexcept {};
    
    virtual ~ID3DRenderer() = default;
};