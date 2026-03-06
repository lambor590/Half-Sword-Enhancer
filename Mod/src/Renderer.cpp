#include <immintrin.h>

#include "Render/Renderer.h"

using namespace Microsoft::WRL;

void Renderer::OnPresent(IDXGISwapChain* pThis, UINT, UINT) noexcept
{
    if (state.needsInit) UNLIKELY {
        if (!InitD3DResources(pThis)) UNLIKELY return;
        state.needsInit = false;
    }

    if (!state.guiReady) UNLIKELY return;
    if (!Gui::NeedsRendering()) LIKELY return;

    (this->*state.renderFunc)();
}

void Renderer::OnResizeBuffers(IDXGISwapChain*, UINT, UINT width, UINT height, DXGI_FORMAT, UINT) noexcept
{
    if (state.isD3D12) LIKELY {
        if (d3d11Context) {
            d3d11Context->ClearState();
            d3d11Context->Flush();
        }
        if (fence && commandQueue)
            SignalAndWait();
        for (auto& wrappedBuffer : d3d11WrappedBackBuffers) wrappedBuffer.Reset();
        for (auto& rtv : d3d11RenderTargetViews) rtv.Reset();
    }

    ReleaseResources();

    window.width = width;
    window.height = height;
    window.viewport = RenderConfig::CreateViewport(static_cast<float>(width), static_cast<float>(height));
    window.viewportDirty = true;
    state.needsInit = true;
    state.bufferIndex = 0;
}

template<bool IsD3D12>
__forceinline void Renderer::RenderFrameImpl() noexcept
{
    if constexpr (IsD3D12) {
        const uint8_t bufferIdx = static_cast<uint8_t>(swapChain3->GetCurrentBackBufferIndex());
        state.bufferIndex = bufferIdx;

        ID3D11Resource* const __restrict wrappedBuffer = d3d11WrappedBackBuffers[bufferIdx].Get();
        ID3D11RenderTargetView* const __restrict renderTarget = d3d11RenderTargetViews[bufferIdx].Get();

        _mm_prefetch(reinterpret_cast<const char*>(wrappedBuffer), _MM_HINT_T0);
        _mm_prefetch(reinterpret_cast<const char*>(renderTarget), _MM_HINT_T0);

        d3d11On12Device->AcquireWrappedResources(&wrappedBuffer, 1);
        d3d11Context->OMSetRenderTargets(1, &renderTarget, nullptr);

        SetViewportIfDirty();
        Gui::Get().Render();

        d3d11On12Device->ReleaseWrappedResources(&wrappedBuffer, 1);
        d3d11Context->Flush();
    } else {
        ID3D11RenderTargetView* const __restrict renderTarget = d3d11RenderTargetViews[0].Get();
        d3d11Context->OMSetRenderTargets(1, &renderTarget, nullptr);

        SetViewportIfDirty();
        Gui::Get().Render();
    }
}

__forceinline void Renderer::RenderFrameD3D11() noexcept
{
    RenderFrameImpl<false>();
}

__forceinline void Renderer::RenderFrameD3D12() noexcept
{
    RenderFrameImpl<true>();
}

void Renderer::InitOrReinitImGui() noexcept {
    if (!state.guiReady) {
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(window.handle);
        ImGui_ImplDX11_Init(d3d11Device.Get(), d3d11Context.Get());
        Gui::Get().Init(d3d11Device, d3d11Context, window.handle);
        Gui::Get().Setup();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
        state.guiReady = true;
    } else {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplDX11_Init(d3d11Device.Get(), d3d11Context.Get());
    }
}

bool Renderer::InitD3DResources(IDXGISwapChain* sc) noexcept
{
    swapChain = sc;

    if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D11Device), (void**)&d3d11Device))) LIKELY {
        state.isD3D12 = false;
        state.renderFunc = &Renderer::RenderFrameD3D11;
        logger.Log("Initializing D3D11 renderer");
        return InitD3D11();
    }

    if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D12Device), (void**)&d3d12Device))) UNLIKELY {
        state.isD3D12 = true;
        state.renderFunc = &Renderer::RenderFrameD3D12;
        logger.Log("Initializing D3D12 renderer");
        return InitD3D12();
    }

    logger.Log("Failed to get D3D device from swap chain");
    return false;
}

static inline void GetWindowDimensions(HWND handle, int& width, int& height) noexcept {
    RECT clientRect;
    GetClientRect(handle, &clientRect);
    width = clientRect.right - clientRect.left;
    height = clientRect.bottom - clientRect.top;
}

bool Renderer::InitD3D11() noexcept
{
    d3d11Device->GetImmediateContext(&d3d11Context);

    DXGI_SWAP_CHAIN_DESC desc;
    swapChain->GetDesc(&desc);
    window.handle = desc.OutputWindow;

    GetWindowDimensions(window.handle, window.width, window.height);
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

    InitOrReinitImGui();
    logger.Log("D3D11 renderer initialized successfully (%dx%d)", window.width, window.height);
    return true;
}

bool Renderer::InitD3D12() noexcept
{
    if (commandQueueCallback && !commandQueue)
        commandQueueCallback();

    if (!commandQueue) {
        logger.Log("Command queue not set for D3D12");
        return false;
    }

    static constexpr auto createFlags = RenderConfig::D3D11_FLAGS;

    if (FAILED(d3d12Device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))) ||
        FAILED(d3d12Device->CreateDescriptorHeap(&RenderConfig::RTV_HEAP_DESC, IID_PPV_ARGS(&rtvHeap))) ||
        FAILED(D3D11On12CreateDevice(d3d12Device.Get(), createFlags, nullptr, 0,
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

    GetWindowDimensions(window.handle, window.width, window.height);
    window.viewport = RenderConfig::CreateViewport(static_cast<float>(window.width), static_cast<float>(window.height));
    window.viewportDirty = true;

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    const UINT stride = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    static constexpr auto rtState = RenderConfig::D3D12_RT_STATE;
    static constexpr auto presentState = RenderConfig::D3D12_PRESENT_STATE;

    for (uint8_t i = 0; i < state.bufferCount; ++i) {
        if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&d3d12RenderTargets[i]))) ||
            FAILED(d3d11On12Device->CreateWrappedResource(d3d12RenderTargets[i].Get(), &RenderConfig::RT_FLAGS,
                rtState, presentState, IID_PPV_ARGS(&d3d11WrappedBackBuffers[i]))) ||
            FAILED(d3d11Device->CreateRenderTargetView(d3d11WrappedBackBuffers[i].Get(), nullptr,
                &d3d11RenderTargetViews[i]))) UNLIKELY {
            logger.Log("Failed to create render target %d for D3D12", static_cast<int>(i));
            return false;
        }

        d3d12Device->CreateRenderTargetView(d3d12RenderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += stride;
    }

    InitOrReinitImGui();
    logger.Log("D3D12 renderer initialized successfully (%dx%d, %d buffers)", window.width, window.height, static_cast<int>(state.bufferCount));
    return true;
}

bool Renderer::SignalAndWait() noexcept
{
    ID3D12CommandQueue* const __restrict cmdQueue = commandQueue.Get();
    ID3D12Fence* const __restrict fencePtr = fence.Get();
    const UINT64 currentFence = fenceValue;

    if (FAILED(cmdQueue->Signal(fencePtr, currentFence)) ||
        (fencePtr->GetCompletedValue() < currentFence &&
            FAILED(fencePtr->SetEventOnCompletion(currentFence, fenceEvent)))) UNLIKELY {
           return false;
    }

    if (fencePtr->GetCompletedValue() < currentFence) UNLIKELY {
        WaitForSingleObject(fenceEvent, RenderConfig::SYNC_TIMEOUT_MS);
    }

    fenceValue += RenderConfig::FENCE_INCREMENT;
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
    if (d3d11Context) LIKELY {
        d3d11Context->ClearState();
        d3d11Context->Flush();
    }

    if (state.isD3D12 && fence && commandQueue) LIKELY {
        SignalAndWait();
    }

    for (auto& rtv : d3d11RenderTargetViews) rtv.Reset();
    for (auto& buf : d3d11WrappedBackBuffers) buf.Reset();
    for (auto& rt : d3d12RenderTargets) rt.Reset();

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