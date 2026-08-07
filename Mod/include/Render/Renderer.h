#pragma once

#include <Windows.h>
#include <array>
#include <atomic>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <vector>
#include <wrl/client.h>

#include "Logger.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "imgui/backends/imgui_impl_dx12.h"
#include "imgui/backends/imgui_impl_win32.h"

using Present = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffers = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using ResizeBuffers1 =
    HRESULT(__stdcall*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT*, IUnknown* const*);
using CreateSwapChain = HRESULT(__stdcall*)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
using CreateSwapChainForHwnd = HRESULT(__stdcall*)(
    IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*,
    IDXGISwapChain1**
);

class Renderer {
public:
    [[nodiscard]] bool Hook();
    void Cleanup() noexcept;
    [[nodiscard]] bool IsInCallback() const noexcept { return currentCallbackDepth > 0; }

private:
    class CallbackLease {
    public:
        explicit CallbackLease(Renderer& owner) noexcept : owner(owner), dispatchHooks(owner.BeginCallback()) {
            ++currentCallbackDepth;
        }
        ~CallbackLease() {
            --currentCallbackDepth;
            owner.EndCallback();
        }
        [[nodiscard]] bool DispatchHooks() const noexcept { return dispatchHooks; }

        CallbackLease(const CallbackLease&) = delete;
        CallbackLease& operator=(const CallbackLease&) = delete;

    private:
        Renderer& owner;
        bool dispatchHooks = false;
    };

    static constexpr std::uint64_t CALLBACK_PHASE_SHIFT = 61;
    static constexpr std::uint64_t CALLBACK_COUNT_MASK = (std::uint64_t{1} << CALLBACK_PHASE_SHIFT) - 1;
    enum class CallbackPhase : std::uint64_t { Running, Draining, Exclusive, Unhooked, Installing, Retiring };
    static constexpr std::uint64_t CallbackState(CallbackPhase phase, std::uint64_t count = 0) noexcept {
        return (static_cast<std::uint64_t>(phase) << CALLBACK_PHASE_SHIFT) | count;
    }
    static constexpr CallbackPhase CallbackPhaseOf(std::uint64_t state) noexcept {
        return static_cast<CallbackPhase>(state >> CALLBACK_PHASE_SHIFT);
    }

    [[nodiscard]] bool BeginCallback() noexcept;
    void EndCallback() noexcept;
    void TransitionCallbackPhase(CallbackPhase phase) noexcept;
    void BeginInstall() noexcept;
    void CompleteInstall(bool success) noexcept;
    void QuiesceCallbacks() noexcept;
    void CompleteUnhook() noexcept;

    enum class RenderBackend : std::uint8_t { Unknown, D3D11, D3D12 };

    struct RenderState {
        bool needsInit = true;
        bool imguiContextReady = false;
        bool imguiRendererReady = false;
        bool inResize = false;
        bool dx12QueueMismatchLogged = false;
        bool dx12QueueMissingLogged = false;
        RenderBackend backend = RenderBackend::Unknown;
        RenderBackend imguiBackend = RenderBackend::Unknown;
        uint8_t bufferCount = 0;
    };

    struct D3D12FrameTarget {
        Microsoft::WRL::ComPtr<ID3D12Resource> backbuffer;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
        D3D12_CPU_DESCRIPTOR_HANDLE renderTarget = {};
        UINT64 fenceValue = 0;
    };

    Logger logger{"Renderer"};
    std::atomic<std::uint64_t> callbackState{CallbackState(CallbackPhase::Unhooked)};
    static thread_local std::uint32_t currentCallbackDepth;

    // Addresses of the original methods after MemoryUtils installs the detours.
    uintptr_t presentAddress = 0;
    uintptr_t presentReturnAddress = 0;
    uintptr_t resizeBuffersAddress = 0;
    uintptr_t resizeBuffersReturnAddress = 0;
    uintptr_t resizeBuffers1Address = 0;
    uintptr_t resizeBuffers1ReturnAddress = 0;
    uintptr_t createSwapChainAddress = 0;
    uintptr_t createSwapChainReturnAddress = 0;
    uintptr_t createSwapChainForHwndAddress = 0;
    uintptr_t createSwapChainForHwndReturnAddress = 0;

    RenderState state;

    HWND windowHandle = nullptr;

    Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11Context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> d3d11RenderTarget;

    Microsoft::WRL::ComPtr<ID3D12Device> d3d12Device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain3;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> d3d12CommandList;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> d3d12RtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> d3d12SrvHeap;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;

    UINT64 fenceValue = 0;
    HANDLE fenceEvent = nullptr;
    UINT d3d12RtvDescriptorSize = 0;
    UINT d3d12SrvDescriptorSize = 0;
    UINT d3d12NextSrvDescriptor = 0;
    static constexpr UINT D3D12_SRV_DESCRIPTOR_COUNT = 64;
    UINT d3d12SkipLogCount = 0;
    DXGI_FORMAT d3d12RenderTargetFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT imguiD3D12RenderTargetFormat = DXGI_FORMAT_UNKNOWN;
    uint8_t imguiD3D12BufferCount = 0;
    std::vector<D3D12FrameTarget> d3d12FrameTargets;
    std::array<UINT, D3D12_SRV_DESCRIPTOR_COUNT> d3d12FreeSrvDescriptors{};
    UINT d3d12FreeSrvDescriptorCount = 0;

    IDXGISwapChain* CreateDummySwapChain();
    [[nodiscard]] bool HookSwapChain(IDXGISwapChain* dummySwapChain);
    [[nodiscard]] bool HookFactory();
    void UnhookFactory() noexcept;

    void RenderFrameD3D11() noexcept;
    void RenderFrameD3D12() noexcept;

    bool InitD3DResources(IDXGISwapChain* sc);
    bool InitD3D11();
    bool InitD3D12();
    bool InitOrReinitImGui() noexcept;
    bool CreateD3D12RenderTargets();
    bool CreateD3D12SrvHeap() noexcept;
    void ReleaseRenderTargets() noexcept;

    void ReleaseContextState() noexcept;
    void ReleaseGraphicsResources() noexcept;
    void ReleaseD3DResourcesForResize() noexcept;
    void ReleaseImGuiRenderer() noexcept;
    bool SignalAndWait() noexcept;

    void OnPresent(IDXGISwapChain* pThis, UINT flags) noexcept;
    static void AllocateD3D12SrvDescriptor(
        ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle
    );
    static void FreeD3D12SrvDescriptor(
        ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle
    );
    void BeforeResizeBuffers() noexcept;
    void AfterResizeBuffers(HRESULT result) noexcept;
    bool CaptureCommandQueue(IUnknown* queueCandidate) noexcept;
    bool CaptureCommandQueue(ID3D12CommandQueue* newQueue) noexcept;

    friend HRESULT __fastcall HookOnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags) noexcept;
    friend HRESULT __fastcall HookOnResizeBuffers(
        IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags
    ) noexcept;
    friend HRESULT __fastcall HookOnResizeBuffers1(
        IDXGISwapChain3* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags,
        const UINT* creationNodeMask, IUnknown* const* presentQueue
    ) noexcept;
    friend HRESULT __fastcall HookOnCreateSwapChain(
        IDXGIFactory* pThis, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain
    ) noexcept;
    friend HRESULT __fastcall HookOnCreateSwapChainForHwnd(
        IDXGIFactory2* pThis, IUnknown* pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1* pDesc,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc, IDXGIOutput* pRestrictToOutput,
        IDXGISwapChain1** ppSwapChain
    ) noexcept;
};
