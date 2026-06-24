#include <bit>
#include "Render/Renderer.h"
#include "Core/ModContext.h"
#include "MemoryUtils.h"
#include "Gui.h"

using namespace Microsoft::WRL;

namespace {
    constexpr int VMT_PRESENT_OFFSET = 8;
    constexpr int VMT_RESIZE_BUFFERS_OFFSET = 13;
    constexpr int VMT_RESIZE_BUFFERS1_OFFSET = 39;
    constexpr int VMT_CREATE_SWAP_CHAIN_OFFSET = 10;
    constexpr int VMT_CREATE_SWAP_CHAIN_FOR_HWND_OFFSET = 15;
    constexpr size_t PTR_SIZE = sizeof(size_t);

    constexpr size_t VMT_PRESENT_BYTE_OFFSET = PTR_SIZE * VMT_PRESENT_OFFSET;
    constexpr size_t VMT_RESIZE_BUFFERS_BYTE_OFFSET = PTR_SIZE * VMT_RESIZE_BUFFERS_OFFSET;
    constexpr size_t VMT_RESIZE_BUFFERS1_BYTE_OFFSET = PTR_SIZE * VMT_RESIZE_BUFFERS1_OFFSET;
    constexpr UINT D3D12_SRV_DESCRIPTOR_COUNT = 64;
    constexpr UINT D3D12_SKIP_LOG_LIMIT = 8;
    constexpr UINT SYNC_TIMEOUT_MS = 500;
    constexpr UINT64 FENCE_INCREMENT = 1;

    constexpr D3D11_VIEWPORT CreateViewport(float width, float height) noexcept {
        return D3D11_VIEWPORT{0.0f, 0.0f, width, height, 0.0f, 1.0f};
    }

    // Hook trampolines dispatch through this singleton-style instance.
    Renderer* g_Renderer = nullptr;

    struct D3D11OutputStateGuard {
        ID3D11DeviceContext* context = nullptr;
        ID3D11RenderTargetView* renderTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
        ID3D11DepthStencilView* depthStencil = nullptr;
        D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
        UINT viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;

        explicit D3D11OutputStateGuard(ID3D11DeviceContext* ctx) noexcept : context(ctx) {
            context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, renderTargets, &depthStencil);
            context->RSGetViewports(&viewportCount, viewports);
        }

        ~D3D11OutputStateGuard() noexcept {
            context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, renderTargets, depthStencil);
            context->RSSetViewports(viewportCount, viewports);

            for (ID3D11RenderTargetView* renderTarget : renderTargets) {
                if (renderTarget) renderTarget->Release();
            }
            if (depthStencil) depthStencil->Release();
        }

        D3D11OutputStateGuard(const D3D11OutputStateGuard&) = delete;
        D3D11OutputStateGuard& operator=(const D3D11OutputStateGuard&) = delete;
    };

    static inline D3D12_RESOURCE_BARRIER TransitionBarrier(
        ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after
    ) noexcept {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        return barrier;
    }
}

HRESULT __fastcall HookOnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags) noexcept {
    g_Renderer->OnPresent(pThis, flags);
    return std::bit_cast<Present>(g_Renderer->presentReturnAddress)(pThis, syncInterval, flags);
}

HRESULT __fastcall HookOnResizeBuffers(
    IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags
) noexcept {
    g_Renderer->BeforeResizeBuffers(pThis, bufferCount, width, height, newFormat, swapChainFlags);
    const HRESULT result = std::bit_cast<ResizeBuffers>(g_Renderer->resizeBuffersReturnAddress
    )(pThis, bufferCount, width, height, newFormat, swapChainFlags);
    g_Renderer->AfterResizeBuffers(width, height, result);
    return result;
}

HRESULT __fastcall HookOnResizeBuffers1(
    IDXGISwapChain3* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags,
    const UINT* creationNodeMask, IUnknown* const* presentQueue
) noexcept {
    g_Renderer->BeforeResizeBuffers(pThis, bufferCount, width, height, newFormat, swapChainFlags);
    const HRESULT result = std::bit_cast<ResizeBuffers1>(g_Renderer->resizeBuffers1ReturnAddress
    )(pThis, bufferCount, width, height, newFormat, swapChainFlags, creationNodeMask, presentQueue);
    if (SUCCEEDED(result) && presentQueue && bufferCount > 0) {
        g_Renderer->CaptureCommandQueue(presentQueue[0]);
    }
    g_Renderer->AfterResizeBuffers(width, height, result);
    return result;
}

HRESULT __fastcall HookOnCreateSwapChain(
    IDXGIFactory* pThis, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain
) noexcept {
    const HRESULT result = std::bit_cast<CreateSwapChain>(g_Renderer->createSwapChainReturnAddress
    )(pThis, pDevice, pDesc, ppSwapChain);
    if (SUCCEEDED(result)) g_Renderer->CaptureCommandQueue(pDevice);
    return result;
}

HRESULT __fastcall HookOnCreateSwapChainForHwnd(
    IDXGIFactory2* pThis, IUnknown* pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1* pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc, IDXGIOutput* pRestrictToOutput,
    IDXGISwapChain1** ppSwapChain
) noexcept {
    const HRESULT result = std::bit_cast<CreateSwapChainForHwnd>(g_Renderer->createSwapChainForHwndReturnAddress
    )(pThis, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
    if (SUCCEEDED(result)) g_Renderer->CaptureCommandQueue(pDevice);
    return result;
}

static inline void GetWindowDimensions(HWND handle, int& width, int& height) noexcept {
    RECT clientRect;
    GetClientRect(handle, &clientRect);
    width = clientRect.right - clientRect.left;
    height = clientRect.bottom - clientRect.top;
}

bool Renderer::Hook() {
    g_Renderer = this;

    IDXGISwapChain* dummySwapChain = CreateDummySwapChain();
    if (!dummySwapChain) {
        logger.Log("Failed to create dummy swap chain, hooking aborted");
        g_Renderer = nullptr;
        return false;
    }
    if (!HookSwapChain(
            dummySwapChain, (uintptr_t)&HookOnPresent, (uintptr_t)&HookOnResizeBuffers,
            (uintptr_t)&HookOnResizeBuffers1, &presentReturnAddress, &resizeBuffersReturnAddress,
            &resizeBuffers1ReturnAddress
        )) {
        logger.Log("Failed to hook swap chain");
        Cleanup();
        return false;
    }

    if (!HookFactory()) {
        logger.Log("DXGI factory hooks unavailable; D3D12 command queue capture may be unavailable");
    }

    return true;
}

void Renderer::OnPresent(IDXGISwapChain* pThis, UINT flags) noexcept {
    try {
        if (state.inResize) [[unlikely]]
            return;
        if (flags & DXGI_PRESENT_TEST) [[unlikely]]
            return;

        if (swapChain.Get() && pThis != swapChain.Get()) [[unlikely]] {
            logger.Log("Swap chain changed, recreating overlay resources");
            ReleaseD3DResourcesForResize();
        }

        if (state.needsInit) [[unlikely]] {
            if (!InitD3DResources(pThis)) [[unlikely]]
                return;
            state.needsInit = false;
        }

        if (!state.imguiRendererReady || !state.renderFunc) [[unlikely]]
            return;
        if (!Gui::NeedsRendering()) [[likely]]
            return;

        (this->*state.renderFunc)();
    } catch (...) {
        state.needsInit = true;
        ReleaseD3DResourcesForResize();
    }
}

bool Renderer::CaptureCommandQueue(ID3D12CommandQueue* newQueue) noexcept {
    if (!newQueue) [[unlikely]]
        return false;

    const D3D12_COMMAND_QUEUE_DESC desc = newQueue->GetDesc();
    if (desc.Type != D3D12_COMMAND_LIST_TYPE_DIRECT) return false;

    ComPtr<ID3D12Device> queueDevice;
    if (FAILED(newQueue->GetDevice(IID_PPV_ARGS(&queueDevice))) ||
        (d3d12Device.Get() && queueDevice.Get() != d3d12Device.Get())) {
        if (!state.dx12QueueMismatchLogged) {
            logger.Log("D3D12 command queue ignored: device mismatch");
            state.dx12QueueMismatchLogged = true;
        }
        return false;
    }

    if (state.commandQueueCaptured && commandQueue.Get() == newQueue) [[likely]]
        return false;

    commandQueue = newQueue;
    state.commandQueueCaptured = true;
    state.dx12QueueMismatchLogged = false;
    state.dx12QueueMissingLogged = false;
    return true;
}

bool Renderer::CaptureCommandQueue(IUnknown* queueCandidate) noexcept {
    if (!queueCandidate) return false;

    ComPtr<ID3D12CommandQueue> newQueue;
    if (FAILED(queueCandidate->QueryInterface(IID_PPV_ARGS(&newQueue)))) return false;
    return CaptureCommandQueue(newQueue.Get());
}

void Renderer::BeforeResizeBuffers(
    IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT
) noexcept {
    state.inResize = true;

    ReleaseD3DResourcesForResize();
}

void Renderer::AfterResizeBuffers(UINT width, UINT height, HRESULT result) noexcept {
    state.inResize = false;

    if (FAILED(result)) [[unlikely]] {
        logger.Log("ResizeBuffers failed: 0x%08X", result);
        state.needsInit = true;
        return;
    }

    if ((width == 0 || height == 0) && window.handle) {
        GetWindowDimensions(window.handle, window.width, window.height);
    } else {
        window.width = static_cast<int>(width);
        window.height = static_cast<int>(height);
    }

    window.viewport = CreateViewport(static_cast<float>(window.width), static_cast<float>(window.height));
    window.viewportDirty = true;
    state.needsInit = true;
}

void Renderer::RenderFrameD3D11() noexcept {
    ID3D11RenderTargetView* renderTargetView = d3d11RenderTarget.Get();
    if (!renderTargetView) [[unlikely]]
        return;

    [[maybe_unused]] const D3D11OutputStateGuard stateGuard{d3d11Context.Get()};
    RenderGuiToTarget(renderTargetView);
}

void Renderer::RenderFrameD3D12() noexcept {
    ID3D12CommandQueue* const queue = commandQueue.Get();
    IDXGISwapChain3* const sc = swapChain3.Get();
    ID3D12GraphicsCommandList* const commandList = d3d12CommandList.Get();
    ID3D12DescriptorHeap* const srvHeap = d3d12SrvHeap.Get();
    ID3D12Fence* const fencePtr = fence.Get();
    if (!queue || !sc || !commandList || !srvHeap || !fencePtr) [[unlikely]]
        return;

    const UINT bufferIdx = sc->GetCurrentBackBufferIndex();
    if (bufferIdx >= d3d12FrameTargets.size()) [[unlikely]] {
        if (d3d12SkipLogCount < D3D12_SKIP_LOG_LIMIT) {
            logger.Log(
                "D3D12 overlay skipped: buffer index %u outside %zu frame targets", bufferIdx, d3d12FrameTargets.size()
            );
            ++d3d12SkipLogCount;
        }
        return;
    }

    D3D12FrameTarget& target = d3d12FrameTargets[bufferIdx];
    if (!target.backbuffer.Get() || !target.commandAllocator.Get() || !target.renderTarget.ptr) [[unlikely]] {
        if (d3d12SkipLogCount < D3D12_SKIP_LOG_LIMIT) {
            logger.Log(
                "D3D12 overlay skipped: incomplete frame target %u backbuffer=%d allocator=%d rtv=%llu", bufferIdx,
                target.backbuffer.Get() != nullptr, target.commandAllocator.Get() != nullptr,
                static_cast<unsigned long long>(target.renderTarget.ptr)
            );
            ++d3d12SkipLogCount;
        }
        return;
    }

    if (target.fenceValue && fencePtr->GetCompletedValue() < target.fenceValue) [[unlikely]]
        return;

    const HRESULT allocatorResult = target.commandAllocator->Reset();
    const HRESULT listResult =
        SUCCEEDED(allocatorResult) ? commandList->Reset(target.commandAllocator.Get(), nullptr) : allocatorResult;
    if (FAILED(listResult)) [[unlikely]] {
        logger.Log(
            "Failed to reset D3D12 overlay command resources: 0x%08X device=0x%08X", listResult,
            d3d12Device.Get() ? d3d12Device->GetDeviceRemovedReason() : S_OK
        );
        return;
    }

    D3D12_RESOURCE_BARRIER barrier =
        TransitionBarrier(target.backbuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &barrier);
    commandList->OMSetRenderTargets(1, &target.renderTarget, FALSE, nullptr);

    ID3D12DescriptorHeap* descriptorHeaps[] = {srvHeap};
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    ImGui_ImplDX12_NewFrame();
    Gui::Get().Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

    barrier =
        TransitionBarrier(target.backbuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    commandList->ResourceBarrier(1, &barrier);

    HRESULT result = commandList->Close();
    if (FAILED(result)) [[unlikely]] {
        logger.Log(
            "Failed to close D3D12 overlay command list: 0x%08X device=0x%08X", result,
            d3d12Device.Get() ? d3d12Device->GetDeviceRemovedReason() : S_OK
        );
        return;
    }

    ID3D12CommandList* commandLists[] = {commandList};
    queue->ExecuteCommandLists(1, commandLists);

    ++fenceValue;
    result = queue->Signal(fencePtr, fenceValue);
    if (FAILED(result)) [[unlikely]] {
        logger.Log(
            "Failed to signal D3D12 overlay fence: 0x%08X device=0x%08X", result,
            d3d12Device.Get() ? d3d12Device->GetDeviceRemovedReason() : S_OK
        );
        return;
    }
    target.fenceValue = fenceValue;
}

void Renderer::RenderGuiToTarget(ID3D11RenderTargetView* renderTargetView) noexcept {
    d3d11Context->OMSetRenderTargets(1, &renderTargetView, nullptr);
    SetViewportIfDirty();
    ImGui_ImplDX11_NewFrame();
    Gui::Get().Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

bool Renderer::InitOrReinitImGui() noexcept {
    if (!state.imguiContextReady) {
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(window.handle);
        Gui::Get().Init(window.handle);
        Gui::Get().Setup();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
        state.imguiContextReady = true;
    }

    if (!state.imguiRendererReady) {
        bool rendererReady = false;
        if (state.backend == RenderBackend::D3D11) {
            rendererReady = ImGui_ImplDX11_Init(d3d11Device.Get(), d3d11Context.Get());
        } else {
            if (!d3d12SrvHeap.Get() && !CreateD3D12SrvHeap()) [[unlikely]]
                return false;

            ImGui_ImplDX12_InitInfo initInfo{};
            initInfo.Device = d3d12Device.Get();
            initInfo.CommandQueue = commandQueue.Get();
            initInfo.NumFramesInFlight = static_cast<int>(state.bufferCount);
            initInfo.RTVFormat = d3d12RenderTargetFormat;
            initInfo.UserData = this;
            initInfo.SrvDescriptorHeap = d3d12SrvHeap.Get();
            initInfo.SrvDescriptorAllocFn = &Renderer::AllocateD3D12SrvDescriptor;
            initInfo.SrvDescriptorFreeFn = &Renderer::FreeD3D12SrvDescriptor;
            rendererReady = ImGui_ImplDX12_Init(&initInfo);
            if (rendererReady) {
                imguiD3D12RenderTargetFormat = d3d12RenderTargetFormat;
                imguiD3D12BufferCount = state.bufferCount;
            }
        }
        if (!rendererReady) [[unlikely]] {
            logger.Log("Failed to initialize ImGui renderer backend");
            return false;
        }
        state.imguiRendererReady = true;
        state.imguiBackend = state.backend;
    }
    return true;
}

bool Renderer::CreateRenderTargets() {
    ReleaseRenderTargets();
    return state.backend == RenderBackend::D3D12 ? CreateD3D12RenderTargets() : CreateD3D11RenderTarget();
}

bool Renderer::CreateD3D11RenderTarget() noexcept {
    ComPtr<ID3D11Texture2D> backbuffer;
    if (FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(&backbuffer))) ||
        FAILED(d3d11Device->CreateRenderTargetView(backbuffer.Get(), nullptr, &d3d11RenderTarget))) [[unlikely]] {
        logger.Log("Failed to create D3D11 render target");
        return false;
    }

    return true;
}

bool Renderer::CreateD3D12RtvHeap() noexcept {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = state.bufferCount;
    if (FAILED(d3d12Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&d3d12RtvHeap)))) [[unlikely]] {
        logger.Log("Failed to create D3D12 RTV descriptor heap");
        return false;
    }
    d3d12RtvDescriptorSize = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    return true;
}

bool Renderer::CreateD3D12SrvHeap() noexcept {
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = D3D12_SRV_DESCRIPTOR_COUNT;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(d3d12Device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&d3d12SrvHeap)))) [[unlikely]] {
        logger.Log("Failed to create D3D12 SRV descriptor heap");
        return false;
    }
    d3d12SrvDescriptorSize = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    d3d12NextSrvDescriptor = 0;
    d3d12FreeSrvDescriptors.clear();
    return true;
}

bool Renderer::CreateD3D12RenderTargets() {
    if (!CreateD3D12RtvHeap()) [[unlikely]]
        return false;

    d3d12FrameTargets.resize(state.bufferCount);
    D3D12_CPU_DESCRIPTOR_HANDLE renderTarget = d3d12RtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT bufferIdx = 0; bufferIdx < state.bufferCount; ++bufferIdx) {
        D3D12FrameTarget& target = d3d12FrameTargets[bufferIdx];
        target.renderTarget = renderTarget;
        if (FAILED(swapChain->GetBuffer(bufferIdx, IID_PPV_ARGS(&target.backbuffer))) ||
            FAILED(d3d12Device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&target.commandAllocator)
            ))) [[unlikely]] {
            logger.Log("Failed to create D3D12 frame target %u", bufferIdx);
            ReleaseRenderTargets();
            return false;
        }
        d3d12Device->CreateRenderTargetView(target.backbuffer.Get(), nullptr, renderTarget);
        renderTarget.ptr += d3d12RtvDescriptorSize;
    }

    if (FAILED(d3d12Device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12FrameTargets[0].commandAllocator.Get(), nullptr,
            IID_PPV_ARGS(&d3d12CommandList)
        )) ||
        FAILED(d3d12CommandList->Close())) [[unlikely]] {
        logger.Log("Failed to create D3D12 overlay command list");
        ReleaseRenderTargets();
        return false;
    }

    return true;
}

void Renderer::ReleaseRenderTargets() noexcept {
    d3d11RenderTarget.Reset();
    d3d12CommandList.Reset();
    d3d12FrameTargets.clear();
    d3d12RtvHeap.Reset();
    d3d12SkipLogCount = 0;
    state.dx12QueueMismatchLogged = false;
}

bool Renderer::InitD3DResources(IDXGISwapChain* sc) {
    swapChain = sc;

    ComPtr<ID3D11Device> newD3D11Device;
    if (SUCCEEDED(sc->GetDevice(IID_PPV_ARGS(&newD3D11Device)))) [[likely]] {
        d3d11Device = newD3D11Device;
        state.backend = RenderBackend::D3D11;
        state.renderFunc = &Renderer::RenderFrameD3D11;
        return InitD3D11();
    }

    ComPtr<ID3D12Device> newD3D12Device;
    if (SUCCEEDED(sc->GetDevice(IID_PPV_ARGS(&newD3D12Device)))) [[unlikely]] {
        const bool deviceChanged = d3d12Device.Get() && d3d12Device.Get() != newD3D12Device.Get();
        if (deviceChanged) {
            ReleaseImGuiRenderer();
            d3d12SrvHeap.Reset();
            d3d12NextSrvDescriptor = 0;
            d3d12FreeSrvDescriptors.clear();
        }
        d3d12Device = newD3D12Device;
        state.backend = RenderBackend::D3D12;
        state.renderFunc = &Renderer::RenderFrameD3D12;
        return InitD3D12();
    }

    logger.Log("Failed to get D3D device from swap chain");
    return false;
}

bool Renderer::InitD3D11() {
    d3d11Device->GetImmediateContext(&d3d11Context);

    DXGI_SWAP_CHAIN_DESC desc;
    swapChain->GetDesc(&desc);
    window.handle = desc.OutputWindow;
    state.bufferCount = static_cast<uint8_t>(desc.BufferCount);

    GetWindowDimensions(window.handle, window.width, window.height);
    window.viewport = CreateViewport(static_cast<float>(window.width), static_cast<float>(window.height));
    window.viewportDirty = true;

    if (!CreateRenderTargets()) [[unlikely]]
        return false;

    if (!InitOrReinitImGui()) [[unlikely]]
        return false;
    return true;
}

bool Renderer::InitD3D12() {
    if (commandQueue.Get()) {
        ComPtr<ID3D12Device> queueDevice;
        if (FAILED(commandQueue->GetDevice(IID_PPV_ARGS(&queueDevice))) || queueDevice.Get() != d3d12Device.Get()) {
            logger.Log("D3D12 command queue reset: device changed");
            commandQueue.Reset();
            state.commandQueueCaptured = false;
            state.dx12QueueMismatchLogged = false;
        }
    }

    if (!commandQueue.Get()) {
        if (!state.dx12QueueMissingLogged) {
            logger.Log("D3D12 command queue not captured yet");
            state.dx12QueueMissingLogged = true;
        }
        return false;
    }

    if (!fence.Get() && FAILED(d3d12Device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
        [[unlikely]] {
        logger.Log("Failed to initialize D3D12 core components");
        ReleaseGraphicsResources();
        return false;
    }

    if (FAILED(swapChain.As(&swapChain3))) [[unlikely]] {
        logger.Log("Failed to initialize D3D12 swap chain");
        ReleaseGraphicsResources();
        return false;
    }

    if (!fenceEvent) fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent) [[unlikely]] {
        logger.Log("Failed to create fence event");
        ReleaseGraphicsResources();
        return false;
    }
    ++fenceValue;

    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swapChain->GetDesc(&desc)) || desc.BufferCount == 0 || desc.BufferDesc.Format == DXGI_FORMAT_UNKNOWN) {
        logger.Log("Invalid D3D12 swap chain description");
        ReleaseGraphicsResources();
        return false;
    }
    window.handle = desc.OutputWindow;
    state.bufferCount = static_cast<uint8_t>(desc.BufferCount);
    d3d12RenderTargetFormat = desc.BufferDesc.Format;
    const bool imguiBackendNeedsReset =
        state.imguiRendererReady &&
        (imguiD3D12RenderTargetFormat != d3d12RenderTargetFormat || imguiD3D12BufferCount != state.bufferCount);
    if (imguiBackendNeedsReset) ReleaseImGuiRenderer();

    GetWindowDimensions(window.handle, window.width, window.height);
    window.viewport = CreateViewport(static_cast<float>(window.width), static_cast<float>(window.height));
    window.viewportDirty = true;

    if (!d3d12SrvHeap.Get() && !CreateD3D12SrvHeap()) [[unlikely]] {
        ReleaseGraphicsResources();
        return false;
    }

    if (!CreateRenderTargets()) [[unlikely]] {
        ReleaseGraphicsResources();
        return false;
    }

    if (!InitOrReinitImGui()) [[unlikely]] {
        ReleaseGraphicsResources();
        return false;
    }

    d3d12SkipLogCount = 0;
    return true;
}

bool Renderer::SignalAndWait() noexcept {
    ID3D12CommandQueue* const __restrict cmdQueue = commandQueue.Get();
    ID3D12Fence* const __restrict fencePtr = fence.Get();
    if (!cmdQueue || !fencePtr || !fenceEvent) [[unlikely]]
        return false;

    const UINT64 currentFence = fenceValue;

    if (FAILED(cmdQueue->Signal(fencePtr, currentFence)) ||
        (fencePtr->GetCompletedValue() < currentFence &&
         FAILED(fencePtr->SetEventOnCompletion(currentFence, fenceEvent)))) [[unlikely]] {
        return false;
    }

    if (fencePtr->GetCompletedValue() < currentFence) [[unlikely]] {
        WaitForSingleObject(fenceEvent, SYNC_TIMEOUT_MS);
    }

    fenceValue += FENCE_INCREMENT;
    return true;
}

void Renderer::ReleaseContextState() noexcept {
    if (d3d11Context) [[likely]] {
        d3d11Context->OMSetRenderTargets(0, nullptr, nullptr);
        d3d11Context->ClearState();
        d3d11Context->Flush();
    }
}

void Renderer::ReleaseGraphicsResources() noexcept {
    ReleaseD3DResourcesForResize();
    ReleaseImGuiRenderer();
    commandQueue.Reset();
    d3d12Device.Reset();
    d3d12SrvHeap.Reset();
    d3d12NextSrvDescriptor = 0;
    d3d12FreeSrvDescriptors.clear();
    imguiD3D12RenderTargetFormat = DXGI_FORMAT_UNKNOWN;
    imguiD3D12BufferCount = 0;
    fence.Reset();
    if (fenceEvent) {
        CloseHandle(fenceEvent);
        fenceEvent = nullptr;
    }
    state.commandQueueCaptured = false;
}

void Renderer::ReleaseD3DResourcesForResize() noexcept {
    const RenderBackend backend = state.backend;
    if (backend == RenderBackend::D3D12 && fence && commandQueue && fenceEvent) [[likely]]
        SignalAndWait();

    if (state.imguiRendererReady && backend == RenderBackend::D3D12) {
        ImGui_ImplDX12_InvalidateDeviceObjects();
    } else if (state.imguiRendererReady) {
        ReleaseImGuiRenderer();
    }

    if (backend == RenderBackend::D3D11) ReleaseContextState();

    ReleaseRenderTargets();

    if (backend == RenderBackend::D3D11) {
        ReleaseContextState();
        d3d11Context.Reset();
        d3d11Device.Reset();
    }

    swapChain.Reset();
    swapChain3.Reset();

    state.backend = RenderBackend::Unknown;
    state.renderFunc = nullptr;
    state.bufferCount = 0;
    state.needsInit = true;
}

void Renderer::ReleaseImGuiRenderer() noexcept {
    if (!state.imguiRendererReady) return;

    if (state.imguiBackend == RenderBackend::D3D12) {
        ImGui_ImplDX12_Shutdown();
        imguiD3D12RenderTargetFormat = DXGI_FORMAT_UNKNOWN;
        imguiD3D12BufferCount = 0;
    } else {
        ImGui_ImplDX11_Shutdown();
    }
    state.imguiRendererReady = false;
    state.imguiBackend = RenderBackend::Unknown;
}

void Renderer::AllocateD3D12SrvDescriptor(
    ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle
) {
    auto* renderer = static_cast<Renderer*>(info->UserData);
    UINT descriptorIndex = 0;
    if (!renderer->d3d12FreeSrvDescriptors.empty()) {
        descriptorIndex = renderer->d3d12FreeSrvDescriptors.back();
        renderer->d3d12FreeSrvDescriptors.pop_back();
    } else {
        descriptorIndex = renderer->d3d12NextSrvDescriptor++;
    }

    if (descriptorIndex >= D3D12_SRV_DESCRIPTOR_COUNT) [[unlikely]] {
        renderer->logger.Log("D3D12 ImGui SRV descriptor heap exhausted");
        descriptorIndex = D3D12_SRV_DESCRIPTOR_COUNT - 1;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = renderer->d3d12SrvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = renderer->d3d12SrvHeap->GetGPUDescriptorHandleForHeapStart();
    cpuHandle.ptr += static_cast<SIZE_T>(descriptorIndex) * renderer->d3d12SrvDescriptorSize;
    gpuHandle.ptr += static_cast<UINT64>(descriptorIndex) * renderer->d3d12SrvDescriptorSize;
    *outCpuHandle = cpuHandle;
    *outGpuHandle = gpuHandle;
}

void Renderer::FreeD3D12SrvDescriptor(
    ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle
) {
    (void)gpuHandle;
    auto* renderer = static_cast<Renderer*>(info->UserData);
    const D3D12_CPU_DESCRIPTOR_HANDLE start = renderer->d3d12SrvHeap->GetCPUDescriptorHandleForHeapStart();
    if (cpuHandle.ptr < start.ptr || renderer->d3d12SrvDescriptorSize == 0) return;

    const UINT descriptorIndex = static_cast<UINT>((cpuHandle.ptr - start.ptr) / renderer->d3d12SrvDescriptorSize);
    if (descriptorIndex < D3D12_SRV_DESCRIPTOR_COUNT) renderer->d3d12FreeSrvDescriptors.push_back(descriptorIndex);
}

void Renderer::Cleanup() noexcept {
    if (presentAddress || resizeBuffersAddress || resizeBuffers1Address) {
        if (presentAddress) MemoryUtils::Unhook(presentAddress);
        if (resizeBuffersAddress) MemoryUtils::Unhook(resizeBuffersAddress);
        if (resizeBuffers1Address) MemoryUtils::Unhook(resizeBuffers1Address);
        presentAddress = 0;
        resizeBuffersAddress = 0;
        resizeBuffers1Address = 0;
        presentReturnAddress = 0;
        resizeBuffersReturnAddress = 0;
        resizeBuffers1ReturnAddress = 0;
    }
    UnhookFactory();

    ReleaseGraphicsResources();
    commandQueue.Reset();

    if (state.imguiContextReady) [[likely]] {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        state.imguiContextReady = false;
        window.handle = nullptr;
    }

    state = {};
    g_Renderer = nullptr;
}

IDXGISwapChain* Renderer::CreateDummySwapChain() {
    // Build a throwaway swap chain so we can read the DXGI vtable once and hook
    // the game's real swap chain later.
    static HWND dummyWindow = []() {
        WNDCLASSEX wc{sizeof(WNDCLASSEX),
                      CS_CLASSDC,
                      DefWindowProc,
                      0,
                      0,
                      GetModuleHandle(nullptr),
                      nullptr,
                      nullptr,
                      nullptr,
                      nullptr,
                      TEXT("DX"),
                      nullptr};
        RegisterClassEx(&wc);
        return CreateWindowEx(
            0, wc.lpszClassName, nullptr, WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr
        );
    }();

    if (!dummyWindow) {
        logger.Log("Failed to create dummy window (error: %lu)", GetLastError());
        return nullptr;
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 1;
    desc.OutputWindow = dummyWindow;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    IDXGISwapChain* swapChainResult = nullptr;
    ID3D11Device* device = nullptr;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &featureLevel, 1, D3D11_SDK_VERSION, &desc, &swapChainResult,
        &device, nullptr, nullptr
    );

    if (FAILED(hr)) {
        logger.Log("Hardware device failed (0x%08X), falling back to WARP", hr);
        if (device) {
            device->Release();
            device = nullptr;
        }
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, &featureLevel, 1, D3D11_SDK_VERSION, &desc, &swapChainResult,
            &device, nullptr, nullptr
        );
    }

    if (device) device->Release();

    if (FAILED(hr) || !swapChainResult) {
        logger.Log("D3D11CreateDeviceAndSwapChain failed: 0x%08X", hr);
        return nullptr;
    }

    return swapChainResult;
}

bool Renderer::HookSwapChain(
    IDXGISwapChain* dummySwapChain, uintptr_t presentDetourFunction, uintptr_t resizeBuffersDetourFunction,
    uintptr_t resizeBuffers1DetourFunction, uintptr_t* outPresentReturn, uintptr_t* outResizeReturn,
    uintptr_t* outResize1Return
) {
    if (!dummySwapChain) return false;

    auto* vmt = *reinterpret_cast<uintptr_t**>(dummySwapChain);
    uintptr_t presentHookAddress = vmt[VMT_PRESENT_BYTE_OFFSET / sizeof(uintptr_t)];
    uintptr_t resizeBuffersHookAddress = vmt[VMT_RESIZE_BUFFERS_BYTE_OFFSET / sizeof(uintptr_t)];

    if (!MemoryUtils::PlaceHook(presentHookAddress, presentDetourFunction, outPresentReturn)) {
        dummySwapChain->Release();
        return false;
    }
    this->presentAddress = presentHookAddress;

    if (!MemoryUtils::PlaceHook(resizeBuffersHookAddress, resizeBuffersDetourFunction, outResizeReturn)) {
        MemoryUtils::Unhook(presentAddress);
        presentAddress = 0;
        presentReturnAddress = 0;
        dummySwapChain->Release();
        return false;
    }
    this->resizeBuffersAddress = resizeBuffersHookAddress;

    IDXGISwapChain3* swapChain3Dummy = nullptr;
    if (SUCCEEDED(dummySwapChain->QueryInterface(IID_PPV_ARGS(&swapChain3Dummy))) && swapChain3Dummy) {
        auto* swapChain3Vmt = *reinterpret_cast<uintptr_t**>(swapChain3Dummy);
        uintptr_t resizeBuffers1HookAddress = swapChain3Vmt[VMT_RESIZE_BUFFERS1_BYTE_OFFSET / sizeof(uintptr_t)];
        if (MemoryUtils::PlaceHook(resizeBuffers1HookAddress, resizeBuffers1DetourFunction, outResize1Return)) {
            this->resizeBuffers1Address = resizeBuffers1HookAddress;
        } else {
            logger.Log("ResizeBuffers1 hook failed; continuing with Present and ResizeBuffers hooks");
        }
        swapChain3Dummy->Release();
    }

    dummySwapChain->Release();
    return true;
}

bool Renderer::HookFactory() {
    if (createSwapChainAddress || createSwapChainForHwndAddress) return true;

    ComPtr<IDXGIFactory2> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) {
        logger.Log("CreateDXGIFactory1 failed: 0x%08X", hr);
        return false;
    }

    auto* vmt = *reinterpret_cast<uintptr_t**>(factory.Get());
    bool hooked = false;
    auto hook = [&](uintptr_t address, uintptr_t detour, uintptr_t* outReturn, uintptr_t* outAddress,
                    const char* name) {
        if (MemoryUtils::PlaceHook(address, detour, outReturn)) {
            *outAddress = address;
            hooked = true;
            return;
        }
        logger.Log("Failed to hook %s", name);
    };

    hook(
        vmt[VMT_CREATE_SWAP_CHAIN_OFFSET], (uintptr_t)&HookOnCreateSwapChain, &createSwapChainReturnAddress,
        &createSwapChainAddress, "DXGI CreateSwapChain"
    );
    hook(
        vmt[VMT_CREATE_SWAP_CHAIN_FOR_HWND_OFFSET], (uintptr_t)&HookOnCreateSwapChainForHwnd,
        &createSwapChainForHwndReturnAddress, &createSwapChainForHwndAddress, "DXGI CreateSwapChainForHwnd"
    );
    return hooked;
}

void Renderer::UnhookFactory() noexcept {
    if (createSwapChainAddress) MemoryUtils::Unhook(createSwapChainAddress);
    if (createSwapChainForHwndAddress) MemoryUtils::Unhook(createSwapChainForHwndAddress);

    createSwapChainAddress = 0;
    createSwapChainReturnAddress = 0;
    createSwapChainForHwndAddress = 0;
    createSwapChainForHwndReturnAddress = 0;
}
