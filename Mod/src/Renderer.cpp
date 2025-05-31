#include <immintrin.h>

#include "Render/Renderer.h"

using namespace Microsoft::WRL;

void Renderer::OnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags)
{
    if (NeedsReinitialization()) UNLIKELY {
        if (!InitD3DResources(pThis)) UNLIKELY return;
        state.mustInitResources = false;
    }
    
    if (!Gui::IsVisible()) UNLIKELY return;
    
    if (IsD3D12Path()) LIKELY {
        SignalAndWait();
    }
    
    RenderFrame();
}

void Renderer::OnResizeBuffers(IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags)
{
    if (IsD3D12Path()) LIKELY {
        SignalAndWait();
    }
    
    if (state.GUIInitialized) LIKELY {
        ImGui_ImplDX11_InvalidateDeviceObjects();
    }
    
    ReleaseResources();
    
    window.width = width;
    window.height = height;
    window.viewport = CD3D11_VIEWPORT(0.0f, 0.0f, (float)width, (float)height);
    window.viewportDirty = true;
    
    MarkForReinitialization();
}

__forceinline void Renderer::RenderFrame() noexcept
{
    if (!d3d11Device || !d3d11Context) UNLIKELY return;
    
    if (IsD3D12Path()) LIKELY {
        if (!swapChain3) UNLIKELY return;
        
        state.bufferIndex = swapChain3->GetCurrentBackBufferIndex();
        
        if (state.bufferIndex >= state.bufferCount || 
            !d3d11WrappedBackBuffers[state.bufferIndex]) UNLIKELY {
            MarkForReinitialization();
            return;
        }
        
        _mm_prefetch(reinterpret_cast<const char*>(d3d11WrappedBackBuffers[state.bufferIndex].Get()), _MM_HINT_T0);
        d3d11On12Device->AcquireWrappedResources(d3d11WrappedBackBuffers[state.bufferIndex].GetAddressOf(), 1);
    }
    
    if (state.bufferIndex >= state.bufferCount || 
        !d3d11RenderTargetViews[state.bufferIndex]) UNLIKELY {
        MarkForReinitialization();
        return;
    }
    
    _mm_prefetch(reinterpret_cast<const char*>(d3d11RenderTargetViews[state.bufferIndex].Get()), _MM_HINT_T0);
    d3d11Context->OMSetRenderTargets(1, d3d11RenderTargetViews[state.bufferIndex].GetAddressOf(), nullptr);
    
    UpdateViewport();
    
    if (state.GUIInitialized) LIKELY {
        GUI->Render();
    }
    
    if (IsD3D12Path()) LIKELY {
        d3d11On12Device->ReleaseWrappedResources(d3d11WrappedBackBuffers[state.bufferIndex].GetAddressOf(), 1);
        d3d11Context->Flush();
        SignalAndWait();
    }
}

__forceinline void Renderer::UpdateViewport() noexcept
{
    if (window.viewportDirty) UNLIKELY {
        d3d11Context->RSSetViewports(1, &window.viewport);
        window.viewportDirty = false;
    }
}

bool Renderer::InitD3DResources(IDXGISwapChain* swapChain) noexcept
{
    this->swapChain = swapChain;
    
    if (SUCCEEDED(swapChain->GetDevice(__uuidof(ID3D11Device), (void**)&d3d11Device))) LIKELY {
        return InitD3D11();
    }
    
    if (SUCCEEDED(swapChain->GetDevice(__uuidof(ID3D12Device), (void**)&d3d12Device))) UNLIKELY {
        state.isD3D12 = true;
        
        if (!commandQueue) UNLIKELY {
            if (commandQueueCallback) LIKELY {
                commandQueueCallback();
            }
            if (!commandQueue) UNLIKELY {
                return false;
            }
        }
        
        return InitD3D12();
    }
    
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
    window.viewport = CD3D11_VIEWPORT(0.0f, 0.0f, (float)window.width, (float)window.height);
    
    state.bufferCount = 1;
    
    ComPtr<ID3D11Texture2D> backbuffer;
    if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbuffer))) UNLIKELY return false;
    if (FAILED(d3d11Device->CreateRenderTargetView(backbuffer.Get(), nullptr, &d3d11RenderTargetViews[0]))) UNLIKELY return false;
    
    if (!state.GUIInitialized && IsValid(window.handle)) LIKELY {
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(window.handle);
        ImGui_ImplDX11_Init(d3d11Device.Get(), d3d11Context.Get());
        GUI->Init(d3d11Device, d3d11Context, window.handle);
        GUI->Setup();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
        state.GUIInitialized = true;
    }
    
    return true;
}

bool Renderer::InitD3D12() noexcept
{
    if (!commandQueue) UNLIKELY return false;
    
    if (FAILED(d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) UNLIKELY return false;
    
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!IsValid(fenceEvent)) UNLIKELY return false;
    fenceValue = 1;
    
    constexpr D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        MAX_BUFFERS,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        0
    };
    
    if (FAILED(d3d12Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)))) UNLIKELY return false;
    
    if (FAILED(D3D11On12CreateDevice(d3d12Device.Get(), D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
        reinterpret_cast<IUnknown**>(commandQueue.GetAddressOf()), 1, 0, &d3d11Device, &d3d11Context, nullptr))) UNLIKELY return false;
    
    if (FAILED(d3d11Device.As(&d3d11On12Device))) UNLIKELY return false;
    if (FAILED(swapChain.As(&swapChain3))) UNLIKELY return false;
    
    DXGI_SWAP_CHAIN_DESC desc;
    swapChain->GetDesc(&desc);
    window.handle = desc.OutputWindow;
    state.bufferCount = desc.BufferCount;
    
    RECT clientRect;
    GetClientRect(window.handle, &clientRect);
    window.width = clientRect.right - clientRect.left;
    window.height = clientRect.bottom - clientRect.top;
    window.viewport = CD3D11_VIEWPORT(0.0f, 0.0f, (float)window.width, (float)window.height);
    
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    const UINT stride = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    
    for (UINT i = 0; i < state.bufferCount; ++i) {
        if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&d3d12RenderTargets[i])))) UNLIKELY return false;
        
        d3d12Device->CreateRenderTargetView(d3d12RenderTargets[i].Get(), nullptr, rtvHandle);
        
        constexpr D3D11_RESOURCE_FLAGS flags = { D3D11_BIND_RENDER_TARGET };
        if (FAILED(d3d11On12Device->CreateWrappedResource(d3d12RenderTargets[i].Get(), &flags,
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT, 
            IID_PPV_ARGS(&d3d11WrappedBackBuffers[i])))) UNLIKELY return false;
        
        if (FAILED(d3d11Device->CreateRenderTargetView(d3d11WrappedBackBuffers[i].Get(), nullptr, 
            &d3d11RenderTargetViews[i]))) UNLIKELY return false;
        
        rtvHandle.ptr += stride;
    }
    
    if (!state.GUIInitialized && IsValid(window.handle)) LIKELY {
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(window.handle);
        ImGui_ImplDX11_Init(d3d11Device.Get(), d3d11Context.Get());
        GUI->Init(d3d11Device, d3d11Context, window.handle);
        GUI->Setup();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
        state.GUIInitialized = true;
    }
    
    return true;
}

__forceinline bool Renderer::SignalAndWait() noexcept
{
    if (!fence || !commandQueue) UNLIKELY return false;
    
    if (FAILED(commandQueue->Signal(fence.Get(), fenceValue))) UNLIKELY return false;
    
    if (fence->GetCompletedValue() < fenceValue) UNLIKELY {
        if (SUCCEEDED(fence->SetEventOnCompletion(fenceValue, fenceEvent))) LIKELY {
            WaitForSingleObject(fenceEvent, TIMEOUT_MS);
        }
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
    
    for (auto& rtv : d3d11RenderTargetViews) {
        rtv.Reset();
    }
    
    for (auto& buf : d3d11WrappedBackBuffers) {
        buf.Reset();
    }
    
    for (auto& rt : d3d12RenderTargets) {
        rt.Reset();
    }
    
    state.bufferIndex = 0;
    MarkForReinitialization();
}

void Renderer::Cleanup()
{
    ReleaseResources();
    
    if (IsD3D12Path() && fence && commandQueue) LIKELY {
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
    
    if (IsValid(fenceEvent)) UNLIKELY {
        CloseHandle(fenceEvent);
        fenceEvent = nullptr;
    }
    
    if (IsValid(window.handle)) LIKELY {
        ImGui_ImplWin32_Shutdown();
        ImGui_ImplDX11_Shutdown();
        ImGui::DestroyContext();
        window.handle = 0;
    }
    
    state = {};
    MarkForReinitialization();
}