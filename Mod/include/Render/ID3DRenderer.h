#pragma once

#include <d3d11.h>
#include <dxgi1_4.h>

#include "IRenderCallback.h"

class ID3DRenderer
{
public:
    virtual void OnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags) noexcept = 0;
    virtual void OnResizeBuffers(IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags) noexcept {};
    virtual void AddRenderCallback(IRenderCallback* object) noexcept {};
    virtual void SetCommandQueue(ID3D12CommandQueue* commandQueue) noexcept {};
    virtual void SetGetCommandQueueCallback(void (*callback)()) noexcept {};
    virtual void Cleanup() noexcept {};
    
    virtual ~ID3DRenderer() = default;
};