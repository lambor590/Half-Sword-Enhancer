#include <bit>
#include "Render/Renderer.h"
#include "Core/ModContext.h"
#include "MemoryUtils.h"
#include "Gui.h"

using namespace Microsoft::WRL;

namespace {
    constexpr int VMT_PRESENT_OFFSET = 8;
    constexpr int VMT_RESIZE_BUFFERS_OFFSET = 13;
    constexpr int VMT_EXECUTE_COMMAND_LISTS_OFFSET = 10;
    constexpr size_t PTR_SIZE = sizeof(size_t);

    constexpr size_t VMT_PRESENT_BYTE_OFFSET = PTR_SIZE * VMT_PRESENT_OFFSET;
    constexpr size_t VMT_RESIZE_BUFFERS_BYTE_OFFSET = PTR_SIZE * VMT_RESIZE_BUFFERS_OFFSET;

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
}

HRESULT __fastcall HookOnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags) noexcept {
    g_Renderer->OnPresent(pThis);
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

void __fastcall HookOnExecuteCommandLists(
    ID3D12CommandQueue* pThis, UINT numCommandLists, const ID3D12CommandList** ppCommandLists
) noexcept {
    if (pThis->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) [[likely]] {
        g_Renderer->SetCommandQueue(pThis);
    }
    std::bit_cast<ExecuteCommandLists>(g_Renderer->executeCommandListsReturnAddress
    )(pThis, numCommandLists, ppCommandLists);
}

void HookGetCommandQueue() {
    // DX12 does not hand us the real queue from the swap chain, so we detour a
    // dummy queue first and capture the first direct queue that executes.
    ID3D12CommandQueue* cmdQueue = g_Renderer->CreateDummyCommandQueue();
    g_Renderer->HookCommandQueue(
        cmdQueue, (uintptr_t)&HookOnExecuteCommandLists, &g_Renderer->executeCommandListsReturnAddress
    );
}

static inline void GetWindowDimensions(HWND handle, int& width, int& height) noexcept {
    RECT clientRect;
    GetClientRect(handle, &clientRect);
    width = clientRect.right - clientRect.left;
    height = clientRect.bottom - clientRect.top;
}

void Renderer::Hook() {
    g_Renderer = this;

    logger.Log("HookOnPresent: %p", &HookOnPresent);
    logger.Log("HookOnResizeBuffers: %p", &HookOnResizeBuffers);

    IDXGISwapChain* dummySwapChain = CreateDummySwapChain();
    if (!dummySwapChain) {
        logger.Log("Failed to create dummy swap chain, hooking aborted");
        return;
    }
    HookSwapChain(
        dummySwapChain, (uintptr_t)&HookOnPresent, (uintptr_t)&HookOnResizeBuffers, &presentReturnAddress,
        &resizeBuffersReturnAddress
    );
}

void Renderer::OnPresent(IDXGISwapChain* pThis) noexcept {
    if (state.inResize) [[unlikely]]
        return;

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
}

void Renderer::SetCommandQueue(ID3D12CommandQueue* newQueue) noexcept {
    if (commandQueue.Get() != newQueue) [[unlikely]] {
        commandQueue = newQueue;
    }
}

void Renderer::BeforeResizeBuffers(
    IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags
) noexcept {
    state.inResize = true;

    DXGI_SWAP_CHAIN_DESC desc{};
    if (pThis && SUCCEEDED(pThis->GetDesc(&desc))) {
        logger.Log(
            "ResizeBuffers request: buffers=%u width=%u height=%u format=%u flags=0x%08X currentWindowed=%d "
            "currentBuffers=%u currentSwapEffect=%u",
            bufferCount, width, height, static_cast<UINT>(newFormat), swapChainFlags, desc.Windowed, desc.BufferCount,
            static_cast<UINT>(desc.SwapEffect)
        );
    } else {
        logger.Log(
            "ResizeBuffers request: buffers=%u width=%u height=%u format=%u flags=0x%08X", bufferCount, width, height,
            static_cast<UINT>(newFormat), swapChainFlags
        );
    }

    ReleaseGraphicsResources();
    logger.Log("ResizeBuffers overlay teardown completed");
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
        window.width = width;
        window.height = height;
    }

    window.viewport = RenderConfig::CreateViewport(static_cast<float>(window.width), static_cast<float>(window.height));
    window.viewportDirty = true;
    state.needsInit = true;
    logger.Log("ResizeBuffers completed: 0x%08X (%dx%d)", result, window.width, window.height);
}

template <bool IsD3D12> void Renderer::RenderFrameImpl() noexcept {
    if constexpr (IsD3D12) {
        const UINT bufferIdx = swapChain3->GetCurrentBackBufferIndex();
        state.bufferIndex = static_cast<uint8_t>(bufferIdx);

        if (bufferIdx >= d3d12FrameTargets.size()) [[unlikely]]
            return;

        D3D12FrameTarget& target = d3d12FrameTargets[bufferIdx];
        ID3D11Resource* wrappedResource = target.wrappedBuffer.Get();
        ID3D11RenderTargetView* renderTargetView = target.renderTarget.Get();

        d3d11On12Device->AcquireWrappedResources(&wrappedResource, 1);
        d3d11Context->OMSetRenderTargets(1, &renderTargetView, nullptr);

        SetViewportIfDirty();
        Gui::Get().Render();

        d3d11Context->OMSetRenderTargets(0, nullptr, nullptr);
        d3d11On12Device->ReleaseWrappedResources(&wrappedResource, 1);
        d3d11Context->Flush();
    } else {
        ID3D11RenderTargetView* renderTargetView = d3d11RenderTarget.Get();
        if (!renderTargetView) [[unlikely]]
            return;

        [[maybe_unused]] const D3D11OutputStateGuard stateGuard{d3d11Context.Get()};
        d3d11Context->OMSetRenderTargets(1, &renderTargetView, nullptr);

        SetViewportIfDirty();
        Gui::Get().Render();
    }
}

void Renderer::RenderFrameD3D11() noexcept {
    RenderFrameImpl<false>();
}

void Renderer::RenderFrameD3D12() noexcept {
    RenderFrameImpl<true>();
}

void Renderer::InitOrReinitImGui() noexcept {
    if (!state.imguiContextReady) {
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(window.handle);
        Gui::Get().Init(window.handle);
        Gui::Get().Setup();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
        state.imguiContextReady = true;
    }

    if (!state.imguiRendererReady) {
        ImGui_ImplDX11_Init(d3d11Device.Get(), d3d11Context.Get());
        state.imguiRendererReady = true;
    }
}

bool Renderer::CreateRenderTargets() noexcept {
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

bool Renderer::CreateD3D12RenderTargets() noexcept {
    d3d12FrameTargets.resize(state.bufferCount);

    for (UINT bufferIdx = 0; bufferIdx < state.bufferCount; ++bufferIdx) {
        D3D12FrameTarget& target = d3d12FrameTargets[bufferIdx];

        if (FAILED(swapChain->GetBuffer(bufferIdx, IID_PPV_ARGS(&target.backbuffer))) ||
            FAILED(d3d11On12Device->CreateWrappedResource(
                target.backbuffer.Get(), &RenderConfig::RT_FLAGS, RenderConfig::D3D12_RT_STATE,
                RenderConfig::D3D12_PRESENT_STATE, IID_PPV_ARGS(&target.wrappedBuffer)
            )) ||
            FAILED(d3d11Device->CreateRenderTargetView(target.wrappedBuffer.Get(), nullptr, &target.renderTarget)))
            [[unlikely]] {
            logger.Log("Failed to create D3D12 render target %u", bufferIdx);
            ReleaseRenderTargets();
            return false;
        }
    }

    return true;
}

void Renderer::ReleaseRenderTargets() noexcept {
    d3d11RenderTarget.Reset();
    d3d12FrameTargets.clear();
}

bool Renderer::InitD3DResources(IDXGISwapChain* sc) noexcept {
    swapChain = sc;

    if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D11Device), (void**)&d3d11Device))) [[likely]] {
        state.backend = RenderBackend::D3D11;
        state.renderFunc = &Renderer::RenderFrameD3D11;
        logger.Log("Initializing D3D11 renderer");
        return InitD3D11();
    }

    if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D12Device), (void**)&d3d12Device))) [[unlikely]] {
        state.backend = RenderBackend::D3D12;
        state.renderFunc = &Renderer::RenderFrameD3D12;
        logger.Log("Initializing D3D12 renderer");
        return InitD3D12();
    }

    logger.Log("Failed to get D3D device from swap chain");
    return false;
}

bool Renderer::InitD3D11() noexcept {
    d3d11Device->GetImmediateContext(&d3d11Context);

    DXGI_SWAP_CHAIN_DESC desc;
    swapChain->GetDesc(&desc);
    window.handle = desc.OutputWindow;
    state.bufferCount = static_cast<uint8_t>(desc.BufferCount);

    GetWindowDimensions(window.handle, window.width, window.height);
    window.viewport = RenderConfig::CreateViewport(static_cast<float>(window.width), static_cast<float>(window.height));
    window.viewportDirty = true;

    if (!CreateRenderTargets()) [[unlikely]]
        return false;

    InitOrReinitImGui();
    logger.Log("D3D11 renderer initialized successfully (%dx%d)", window.width, window.height);
    return true;
}

bool Renderer::InitD3D12() noexcept {
    if (!commandQueue) HookGetCommandQueue();

    if (!commandQueue) {
        logger.Log("Command queue not set for D3D12");
        return false;
    }

    static constexpr auto CREATE_FLAGS = RenderConfig::D3D11_FLAGS;

    if (FAILED(d3d12Device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))) ||
        FAILED(D3D11On12CreateDevice(
            d3d12Device.Get(), CREATE_FLAGS, nullptr, 0, reinterpret_cast<IUnknown**>(commandQueue.GetAddressOf()), 1,
            0, &d3d11Device, &d3d11Context, nullptr
        )) ||
        FAILED(d3d11Device.As(&d3d11On12Device)) || FAILED(swapChain.As(&swapChain3))) [[unlikely]] {
        logger.Log("Failed to initialize D3D12 core components");
        return false;
    }

    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent) [[unlikely]] {
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

    if (!CreateRenderTargets()) [[unlikely]]
        return false;

    InitOrReinitImGui();
    logger.Log(
        "D3D12 renderer initialized successfully (%dx%d, %d buffers)", window.width, window.height,
        static_cast<int>(state.bufferCount)
    );
    return true;
}

bool Renderer::SignalAndWait() noexcept {
    ID3D12CommandQueue* const __restrict cmdQueue = commandQueue.Get();
    ID3D12Fence* const __restrict fencePtr = fence.Get();
    const UINT64 currentFence = fenceValue;

    if (FAILED(cmdQueue->Signal(fencePtr, currentFence)) ||
        (fencePtr->GetCompletedValue() < currentFence &&
         FAILED(fencePtr->SetEventOnCompletion(currentFence, fenceEvent)))) [[unlikely]] {
        return false;
    }

    if (fencePtr->GetCompletedValue() < currentFence) [[unlikely]] {
        WaitForSingleObject(fenceEvent, RenderConfig::SYNC_TIMEOUT_MS);
    }

    fenceValue += RenderConfig::FENCE_INCREMENT;
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
    if (state.backend == RenderBackend::D3D12 && fence && commandQueue) [[likely]] {
        SignalAndWait();
    }

    ReleaseContextState();
    ReleaseRenderTargets();

    if (state.imguiRendererReady) {
        ImGui_ImplDX11_Shutdown();
        state.imguiRendererReady = false;
    }

    ReleaseContextState();

    if (state.backend == RenderBackend::D3D12 && fence && commandQueue) [[likely]] {
        SignalAndWait();
    }

    d3d11Context.Reset();
    d3d11Device.Reset();
    d3d12Device.Reset();
    swapChain.Reset();
    swapChain3.Reset();
    d3d11On12Device.Reset();
    fence.Reset();

    if (fenceEvent) {
        CloseHandle(fenceEvent);
        fenceEvent = nullptr;
    }

    state.backend = RenderBackend::Unknown;
    state.renderFunc = nullptr;
    state.bufferIndex = 0;
    state.bufferCount = 0;
    state.needsInit = true;
}

void Renderer::Cleanup() noexcept {
    if (presentAddress || resizeBuffersAddress) {
        if (presentAddress) MemoryUtils::Unhook(presentAddress);
        if (resizeBuffersAddress) MemoryUtils::Unhook(resizeBuffersAddress);
        presentAddress = 0;
        resizeBuffersAddress = 0;
        presentReturnAddress = 0;
        resizeBuffersReturnAddress = 0;
    }
    if (executeCommandListsAddress) {
        UnhookCommandQueue();
        executeCommandListsAddress = 0;
        executeCommandListsReturnAddress = 0;
    }

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

ID3D12CommandQueue* Renderer::CreateDummyCommandQueue() {
    ComPtr<ID3D12Device> device;
    HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (FAILED(hr) || !device) {
        logger.Log("D3D12CreateDevice failed: 0x%08X", hr);
        return nullptr;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ID3D12CommandQueue* queue = nullptr;
    hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue));
    if (FAILED(hr) || !queue) {
        logger.Log("CreateCommandQueue failed: 0x%08X", hr);
        return nullptr;
    }

    return queue;
}

void Renderer::HookSwapChain(
    IDXGISwapChain* dummySwapChain, uintptr_t presentDetourFunction, uintptr_t resizeBuffersDetourFunction,
    uintptr_t* outPresentReturn, uintptr_t* outResizeReturn
) {
    auto* vmt = *reinterpret_cast<uintptr_t**>(dummySwapChain);
    uintptr_t presentHookAddress = vmt[VMT_PRESENT_BYTE_OFFSET / sizeof(uintptr_t)];
    uintptr_t resizeBuffersHookAddress = vmt[VMT_RESIZE_BUFFERS_BYTE_OFFSET / sizeof(uintptr_t)];

    MemoryUtils::PlaceHook(presentHookAddress, presentDetourFunction, outPresentReturn);
    MemoryUtils::PlaceHook(resizeBuffersHookAddress, resizeBuffersDetourFunction, outResizeReturn);
    this->presentAddress = presentHookAddress;
    this->resizeBuffersAddress = resizeBuffersHookAddress;

    dummySwapChain->Release();
}

void Renderer::HookCommandQueue(
    ID3D12CommandQueue* dummyCommandQueue, uintptr_t executeCommandListsDetourFunction, uintptr_t* outExecReturn
) {
    if (!dummyCommandQueue) return;

    uintptr_t* vTable = *(uintptr_t**)dummyCommandQueue;
    constexpr size_t EXECUTE_OFFSET = VMT_EXECUTE_COMMAND_LISTS_OFFSET;

    uintptr_t executeAddr = vTable[EXECUTE_OFFSET];
    executeCommandListsAddress = executeAddr;

    MemoryUtils::PlaceHook(executeAddr, executeCommandListsDetourFunction, outExecReturn);

    dummyCommandQueue->Release();
}

void Renderer::UnhookCommandQueue() const {
    MemoryUtils::Unhook(executeCommandListsAddress);
}
