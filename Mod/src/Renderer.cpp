#include <immintrin.h>

#include "Render/Renderer.h"

using namespace Microsoft::WRL;

void Renderer::OnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags) noexcept
{
    if (state.needsInit) UNLIKELY {
        if (!InitD3DResources(pThis)) UNLIKELY return;
        state.needsInit = false;
    }
    
    if (!Gui::IsVisible() || !state.guiReady) UNLIKELY return;
    
    if (state.isD3D12) LIKELY {
        SignalAndWait();
    }
    
    RenderFrame();
}

void Renderer::OnResizeBuffers(IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags) noexcept
{
    if (state.isD3D12) LIKELY {
        SignalAndWait();
    }
    
    if (state.guiReady) LIKELY {
        ImGui_ImplDX11_InvalidateDeviceObjects();
    }
    
    ReleaseResources();
    
    window.width = width;
    window.height = height;
    window.viewport = RenderConfig::CreateViewport(static_cast<float>(width), static_cast<float>(height));
    window.viewportDirty = true;
    state.needsInit = true;
    state.bufferIndex = 0;
}

__forceinline void Renderer::RenderFrame() noexcept
{
    if (state.isD3D12) LIKELY {
        state.bufferIndex = static_cast<uint8_t>(swapChain3->GetCurrentBackBufferIndex());
        
        auto& wrappedBuffer = d3d11WrappedBackBuffers[state.bufferIndex];
        auto& renderTarget = d3d11RenderTargetViews[state.bufferIndex];
        
        _mm_prefetch(reinterpret_cast<const char*>(wrappedBuffer.Get()), _MM_HINT_T0);
        _mm_prefetch(reinterpret_cast<const char*>(renderTarget.Get()), _MM_HINT_T0);
        
        d3d11On12Device->AcquireWrappedResources(wrappedBuffer.GetAddressOf(), 1);
        d3d11Context->OMSetRenderTargets(1, renderTarget.GetAddressOf(), nullptr);
        
        if (window.viewportDirty) UNLIKELY {
            d3d11Context->RSSetViewports(1, &window.viewport);
            window.viewportDirty = false;
        }
        
        Gui::Get().Render();
        
        d3d11On12Device->ReleaseWrappedResources(wrappedBuffer.GetAddressOf(), 1);
        d3d11Context->Flush();
        SignalAndWait();
    } else {
        d3d11Context->OMSetRenderTargets(1, d3d11RenderTargetViews[0].GetAddressOf(), nullptr);
        
        if (window.viewportDirty) UNLIKELY {
            d3d11Context->RSSetViewports(1, &window.viewport);
            window.viewportDirty = false;
        }
        
        Gui::Get().Render();
    }
}

bool Renderer::InitD3DResources(IDXGISwapChain* swapChain) noexcept
{
    this->swapChain = swapChain;
    
    if (SUCCEEDED(swapChain->GetDevice(__uuidof(ID3D11Device), (void**)&d3d11Device))) LIKELY {
        state.isD3D12 = false;
        logger.Log("Initializing D3D11 renderer");
        return InitD3D11();
    }
    
    if (SUCCEEDED(swapChain->GetDevice(__uuidof(ID3D12Device), (void**)&d3d12Device))) UNLIKELY {
        state.isD3D12 = true;
        logger.Log("Initializing D3D12 renderer");
        return InitD3D12();
    }
    
    logger.Log("Failed to get D3D device from swap chain");
    return false;
}

bool Renderer::InitD3D11() noexcept
{
    d3d11Device->GetImmediateContext(&d3d11Context);
    
    DXGI_SWAP_CHAIN_DESC desc;
    swapChain->GetDesc(&desc);
    window.handle = desc.OutputWindow;
    
    RECT clientRect;
    GetClientRect(window.handle, &clientRect);
    window.width = clientRect.right - clientRect.left;
    window.height = clientRect.bottom - clientRect.top;
    window.viewport = RenderConfig::CreateViewport(static_cast<float>(window.width), static_cast<float>(window.height));
    window.viewportDirty = true;
    
    state.bufferCount = 1;
    state.bufferIndex = 0;
    
    ComPtr<ID3D11Texture2D> backbuffer;
    if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbuffer)) ||
        FAILED(d3d11Device->CreateRenderTargetView(backbuffer.Get(), nullptr, &d3d11RenderTargetViews[0]))) UNLIKELY {
        logger.Log("Failed to create D3D11 render target view");
        return false;
    }
    
    if (!state.guiReady) LIKELY {
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(window.handle);
        ImGui_ImplDX11_Init(d3d11Device.Get(), d3d11Context.Get());
        Gui::Get().Init(d3d11Device, d3d11Context, window.handle);
        Gui::Get().Setup();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
        state.guiReady = true;
        logger.Log("GUI initialized successfully");
    }
    
    logger.Log("D3D11 renderer initialized successfully ({}x{})", window.width, window.height);
    return true;
}

bool Renderer::InitD3D12() noexcept
{
    if (!commandQueue) {
        logger.Log("Command queue not set for D3D12");
        return false;
    }
    
    if (FAILED(d3d12Device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))) ||
        FAILED(d3d12Device->CreateDescriptorHeap(&RenderConfig::RTV_HEAP_DESC, IID_PPV_ARGS(&rtvHeap))) ||
        FAILED(D3D11On12CreateDevice(d3d12Device.Get(), D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
            reinterpret_cast<IUnknown**>(commandQueue.GetAddressOf()), 1, 0, &d3d11Device, &d3d11Context, nullptr)) ||
        FAILED(d3d11Device.As(&d3d11On12Device)) ||
        FAILED(swapChain.As(&swapChain3))) UNLIKELY {
        logger.Log("Failed to initialize D3D12 core components");
        return false;
    }
    
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent) UNLIKELY {
        logger.Log("Failed to create fence event");
        return false;
    }
    ++fenceValue;
    
    DXGI_SWAP_CHAIN_DESC desc;
    swapChain->GetDesc(&desc);
    window.handle = desc.OutputWindow;
    state.bufferCount = static_cast<uint8_t>(desc.BufferCount);
    
    RECT clientRect;
    GetClientRect(window.handle, &clientRect);
    window.width = clientRect.right - clientRect.left;
    window.height = clientRect.bottom - clientRect.top;
    window.viewport = RenderConfig::CreateViewport(static_cast<float>(window.width), static_cast<float>(window.height));
    window.viewportDirty = true;
    
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    const UINT stride = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    
    for (uint8_t i = 0; i < state.bufferCount; ++i) {
        if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&d3d12RenderTargets[i]))) ||
            FAILED(d3d11On12Device->CreateWrappedResource(d3d12RenderTargets[i].Get(), &RenderConfig::RT_FLAGS,
                D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT, 
                IID_PPV_ARGS(&d3d11WrappedBackBuffers[i]))) ||
            FAILED(d3d11Device->CreateRenderTargetView(d3d11WrappedBackBuffers[i].Get(), nullptr, 
                &d3d11RenderTargetViews[i]))) UNLIKELY {
            logger.Log("Failed to create render target {} for D3D12", i);
            return false;
        }
        
        d3d12Device->CreateRenderTargetView(d3d12RenderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += stride;
    }
    
    if (!state.guiReady) LIKELY {
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(window.handle);
        ImGui_ImplDX11_Init(d3d11Device.Get(), d3d11Context.Get());
        Gui::Get().Init(d3d11Device, d3d11Context, window.handle);
        Gui::Get().Setup();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
        state.guiReady = true;
        logger.Log("GUI initialized successfully");
    }
    
    logger.Log("D3D12 renderer initialized successfully ({}x{}, {} buffers)", window.width, window.height, state.bufferCount);
    return true;
}

__forceinline bool Renderer::SignalAndWait() noexcept
{
    if (FAILED(commandQueue->Signal(fence.Get(), fenceValue)) ||
        (fence->GetCompletedValue() < fenceValue && 
         FAILED(fence->SetEventOnCompletion(fenceValue, fenceEvent)))) UNLIKELY {
        return false;
    }
    
    if (fence->GetCompletedValue() < fenceValue) UNLIKELY {
        WaitForSingleObject(fenceEvent, RenderConfig::SYNC_TIMEOUT_MS);
    }
    
    ++fenceValue;
    return true;
}

void Renderer::ReleaseResources() noexcept
{
    if (d3d11Context) LIKELY {
        d3d11Context->ClearState();
        d3d11Context->Flush();
    }
    
    for (auto& rtv : d3d11RenderTargetViews) rtv.Reset();
    for (auto& buf : d3d11WrappedBackBuffers) buf.Reset();
    for (auto& rt : d3d12RenderTargets) rt.Reset();
    
    state.needsInit = true;
    state.bufferIndex = 0;
}

void Renderer::Cleanup() noexcept
{
    ReleaseResources();
    
    if (state.isD3D12 && fence && commandQueue) LIKELY {
        commandQueue->Signal(fence.Get(), fenceValue + 1);
    }
    
    d3d11Context.Reset();
    d3d11Device.Reset();
    d3d12Device.Reset();
    commandQueue.Reset();
    swapChain.Reset();
    swapChain3.Reset();
    d3d11On12Device.Reset();
    rtvHeap.Reset();
    fence.Reset();
    
    if (fenceEvent) UNLIKELY {
        CloseHandle(fenceEvent);
        fenceEvent = nullptr;
    }
    
    if (window.handle) LIKELY {
        ImGui_ImplWin32_Shutdown();
        ImGui_ImplDX11_Shutdown();
        ImGui::DestroyContext();
        window.handle = nullptr;
    }
    
    state = {};
}