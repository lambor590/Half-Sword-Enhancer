#include <bit>
#include <immintrin.h>

#include "Render/Renderer.h"
#include "Core/ModContext.h"
#include "MemoryUtils.h"
#include "Gui.h"

using namespace Microsoft::WRL;

// ---------------------------------------------------------------------------
// VMT offset constants (formerly in DXHookConstants)
// ---------------------------------------------------------------------------
namespace {
    constexpr int VMT_PRESENT_OFFSET = 8;
    constexpr int VMT_RESIZE_BUFFERS_OFFSET = 13;
    constexpr int VMT_EXECUTE_COMMAND_LISTS_OFFSET = 10;
    constexpr size_t PTR_SIZE = sizeof(size_t);

    constexpr size_t VMT_PRESENT_BYTE_OFFSET = PTR_SIZE * VMT_PRESENT_OFFSET;
    constexpr size_t VMT_RESIZE_BUFFERS_BYTE_OFFSET = PTR_SIZE * VMT_RESIZE_BUFFERS_OFFSET;

    // The single Renderer instance, set from DllMain before hooking
    Renderer* g_Renderer = nullptr;
}

// ---------------------------------------------------------------------------
// Static hook trampolines (no __forceinline -- these are function-pointer targets)
// ---------------------------------------------------------------------------
static HRESULT __fastcall HookOnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags) noexcept {
    g_Renderer->OnPresent(pThis);
    return std::bit_cast<Present>(g_Renderer->presentReturnAddress)(pThis, syncInterval, flags);
}

static HRESULT __fastcall HookOnResizeBuffers(
    IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags
) noexcept {
    g_Renderer->OnResizeBuffers(width, height);
    return std::bit_cast<ResizeBuffers>(g_Renderer->resizeBuffersReturnAddress
    )(pThis, bufferCount, width, height, newFormat, swapChainFlags);
}

static void __fastcall HookOnExecuteCommandLists(
    ID3D12CommandQueue* pThis, UINT numCommandLists, const ID3D12CommandList** ppCommandLists
) noexcept {
    if (pThis->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) [[likely]] {
        g_Renderer->SetCommandQueue(pThis);
    }
    std::bit_cast<ExecuteCommandLists>(g_Renderer->executeCommandListsReturnAddress
    )(pThis, numCommandLists, ppCommandLists);
}

static void HookGetCommandQueue() {
    ID3D12CommandQueue* cmdQueue = g_Renderer->CreateDummyCommandQueue();
    g_Renderer->HookCommandQueue(
        cmdQueue, (uintptr_t)&HookOnExecuteCommandLists, &g_Renderer->executeCommandListsReturnAddress
    );
}

// ---------------------------------------------------------------------------
// Hook() -- entry point: creates dummy swap chain, installs VMT hooks
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// OnPresent / OnResizeBuffers / SetCommandQueue
// ---------------------------------------------------------------------------
void Renderer::OnPresent(IDXGISwapChain* pThis) noexcept {
    if (state.needsInit) [[unlikely]] {
        if (!InitD3DResources(pThis)) [[unlikely]]
            return;
        state.needsInit = false;
    }

    if (!state.guiReady) [[unlikely]]
        return;
    if (!Gui::NeedsRendering()) [[likely]]
        return;

    ModContext::Get().RefreshCache();
    (this->*state.renderFunc)();
}

void Renderer::OnResizeBuffers(UINT width, UINT height) noexcept {
    if (state.isD3D12) [[likely]] {
        if (d3d11Context) {
            d3d11Context->ClearState();
            d3d11Context->Flush();
        }
        if (fence && commandQueue) SignalAndWait();
    }

    ReleaseRenderTargets();

    window.width = width;
    window.height = height;
    window.viewport = RenderConfig::CreateViewport(static_cast<float>(width), static_cast<float>(height));
    window.viewportDirty = true;
    state.needsInit = true;
    state.bufferIndex = 0;
}

void Renderer::SetCommandQueue(ID3D12CommandQueue* newQueue) noexcept {
    if (commandQueue.Get() != newQueue) [[unlikely]] {
        commandQueue = newQueue;
    }
}

// ---------------------------------------------------------------------------
// RenderFrameImpl -- template for DX11/DX12
// ---------------------------------------------------------------------------
template <bool IsD3D12> void Renderer::RenderFrameImpl() noexcept {
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

void Renderer::RenderFrameD3D11() noexcept {
    RenderFrameImpl<false>();
}

void Renderer::RenderFrameD3D12() noexcept {
    RenderFrameImpl<true>();
}

// ---------------------------------------------------------------------------
// ImGui init
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// DX resource initialization
// ---------------------------------------------------------------------------
bool Renderer::InitD3DResources(IDXGISwapChain* sc) noexcept {
    swapChain = sc;

    if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D11Device), (void**)&d3d11Device))) [[likely]] {
        state.isD3D12 = false;
        state.renderFunc = &Renderer::RenderFrameD3D11;
        logger.Log("Initializing D3D11 renderer");
        return InitD3D11();
    }

    if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D12Device), (void**)&d3d12Device))) [[unlikely]] {
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

bool Renderer::InitD3D11() noexcept {
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
        FAILED(d3d11Device->CreateRenderTargetView(backbuffer.Get(), nullptr, &d3d11RenderTargetViews[0])))
        [[unlikely]] {
        logger.Log("Failed to create D3D11 render target view");
        return false;
    }

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
        FAILED(d3d12Device->CreateDescriptorHeap(&RenderConfig::RTV_HEAP_DESC, IID_PPV_ARGS(&rtvHeap))) ||
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

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    const UINT stride = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    static constexpr auto RT_STATE = RenderConfig::D3D12_RT_STATE;
    static constexpr auto PRESENT_STATE = RenderConfig::D3D12_PRESENT_STATE;

    for (uint8_t i = 0; i < state.bufferCount; ++i) {
        if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&d3d12RenderTargets[i]))) ||
            FAILED(d3d11On12Device->CreateWrappedResource(
                d3d12RenderTargets[i].Get(), &RenderConfig::RT_FLAGS, RT_STATE, PRESENT_STATE,
                IID_PPV_ARGS(&d3d11WrappedBackBuffers[i])
            )) ||
            FAILED(d3d11Device
                       ->CreateRenderTargetView(d3d11WrappedBackBuffers[i].Get(), nullptr, &d3d11RenderTargetViews[i])))
            [[unlikely]] {
            logger.Log("Failed to create render target %d for D3D12", static_cast<int>(i));
            return false;
        }

        d3d12Device->CreateRenderTargetView(d3d12RenderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += stride;
    }

    InitOrReinitImGui();
    logger.Log(
        "D3D12 renderer initialized successfully (%dx%d, %d buffers)", window.width, window.height,
        static_cast<int>(state.bufferCount)
    );
    return true;
}

// ---------------------------------------------------------------------------
// Fence sync
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// ReleaseRenderTargets -- shared cleanup helper (no duplication)
// ---------------------------------------------------------------------------
void Renderer::ReleaseRenderTargets() noexcept {
    if (d3d11Context) [[likely]] {
        d3d11Context->ClearState();
        d3d11Context->Flush();
    }

    for (auto& rtv : d3d11RenderTargetViews)
        rtv.Reset();
    for (auto& buf : d3d11WrappedBackBuffers)
        buf.Reset();
    for (auto& rt : d3d12RenderTargets)
        rt.Reset();

    state.needsInit = true;
    state.bufferIndex = 0;
}

// ---------------------------------------------------------------------------
// Cleanup -- full teardown (calls shared helper + releases devices/ImGui)
// ---------------------------------------------------------------------------
void Renderer::Cleanup() noexcept {
    if (state.isD3D12 && fence && commandQueue) [[likely]] {
        SignalAndWait();
    }

    ReleaseRenderTargets();

    d3d11Context.Reset();
    d3d11Device.Reset();
    d3d12Device.Reset();
    commandQueue.Reset();
    swapChain.Reset();
    swapChain3.Reset();
    d3d11On12Device.Reset();
    rtvHeap.Reset();
    fence.Reset();

    if (fenceEvent) [[unlikely]] {
        CloseHandle(fenceEvent);
        fenceEvent = nullptr;
    }

    if (window.handle) [[likely]] {
        ImGui_ImplWin32_Shutdown();
        ImGui_ImplDX11_Shutdown();
        ImGui::DestroyContext();
        window.handle = nullptr;
    }

    if (executeCommandListsAddress) {
        UnhookCommandQueue();
    }

    state = {};
    g_Renderer = nullptr;
}

// ---------------------------------------------------------------------------
// Dummy swap chain creation (for VMT hooking)
// ---------------------------------------------------------------------------
IDXGISwapChain* Renderer::CreateDummySwapChain() {
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

// ---------------------------------------------------------------------------
// Dummy command queue creation (for DX12 VMT hooking)
// ---------------------------------------------------------------------------
ID3D12CommandQueue* Renderer::CreateDummyCommandQueue() {
    ID3D12Device* device = nullptr;
    D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ID3D12CommandQueue* queue = nullptr;
    device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue));
    device->Release();

    return queue;
}

// ---------------------------------------------------------------------------
// VMT hook installation
// ---------------------------------------------------------------------------
void Renderer::HookSwapChain(
    IDXGISwapChain* dummySwapChain, uintptr_t presentDetourFunction, uintptr_t resizeBuffersDetourFunction,
    uintptr_t* outPresentReturn, uintptr_t* outResizeReturn
) {
    auto* vmt = *reinterpret_cast<uintptr_t**>(dummySwapChain);
    uintptr_t presentAddress = vmt[VMT_PRESENT_BYTE_OFFSET / sizeof(uintptr_t)];
    uintptr_t resizeBuffersAddress = vmt[VMT_RESIZE_BUFFERS_BYTE_OFFSET / sizeof(uintptr_t)];

    MemoryUtils::PlaceHook(presentAddress, presentDetourFunction, outPresentReturn);
    MemoryUtils::PlaceHook(resizeBuffersAddress, resizeBuffersDetourFunction, outResizeReturn);

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
