#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <vector>
#include <wrl/client.h>

#include "Render/RenderConfig.h"
#include "Logger.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "imgui/backends/imgui_impl_dx12.h"
#include "imgui/backends/imgui_impl_win32.h"

using Present = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffers = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using ResizeBuffers1 =
    HRESULT(__stdcall*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT*, IUnknown* const*);
using ExecuteCommandLists = void(__stdcall*)(ID3D12CommandQueue*, UINT, const ID3D12CommandList**);

class Renderer {
public:
    void Hook();
    void Cleanup() noexcept;

private:
    enum class RenderBackend : std::uint8_t { Unknown, D3D11, D3D12 };

    struct RenderState {
        void (Renderer::*renderFunc)() noexcept = nullptr;
        bool needsInit = true;
        bool imguiContextReady = false;
        bool imguiRendererReady = false;
        bool inResize = false;
        bool commandQueueCaptured = false;
        bool commandQueueHookInstalled = false;
        bool dx12QueueMismatchLogged = false;
        bool dx12FirstDrawLogged = false;
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

    // Addresses of the original methods after MemoryUtils installs the detours.
    uintptr_t presentAddress = 0;
    uintptr_t presentReturnAddress = 0;
    uintptr_t resizeBuffersAddress = 0;
    uintptr_t resizeBuffersReturnAddress = 0;
    uintptr_t resizeBuffers1Address = 0;
    uintptr_t resizeBuffers1ReturnAddress = 0;
    uintptr_t executeCommandListsAddress = 0;
    uintptr_t executeCommandListsReturnAddress = 0;

    RenderState state;

    struct {
        HWND handle = nullptr;
        D3D11_VIEWPORT viewport = {};
        int width = 0, height = 0;
        bool viewportDirty = true;
    } window;

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
    UINT d3d12SkipLogCount = 0;
    DXGI_FORMAT d3d12RenderTargetFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT imguiD3D12RenderTargetFormat = DXGI_FORMAT_UNKNOWN;
    uint8_t imguiD3D12BufferCount = 0;
    std::vector<D3D12FrameTarget> d3d12FrameTargets;
    std::vector<UINT> d3d12FreeSrvDescriptors;

    IDXGISwapChain* CreateDummySwapChain();
    void HookSwapChain(
        IDXGISwapChain* dummySwapChain, uintptr_t presentDetourFunction, uintptr_t resizeBuffersDetourFunction,
        uintptr_t resizeBuffers1DetourFunction, uintptr_t* outPresentReturn, uintptr_t* outResizeReturn,
        uintptr_t* outResize1Return
    );
    ID3D12CommandQueue* CreateDummyCommandQueue();
    bool HookCommandQueue(
        ID3D12CommandQueue* dummyCommandQueue, uintptr_t executeCommandListsDetourFunction, uintptr_t* outExecReturn
    );
    void UnhookCommandQueue() noexcept;

    void RenderFrameD3D11() noexcept;
    void RenderFrameD3D12() noexcept;
    void RenderGuiToTarget(ID3D11RenderTargetView* renderTargetView) noexcept;

    bool InitD3DResources(IDXGISwapChain* sc) noexcept;
    bool InitD3D11() noexcept;
    bool InitD3D12() noexcept;
    bool InitOrReinitImGui() noexcept;
    bool CreateRenderTargets() noexcept;
    bool CreateD3D11RenderTarget() noexcept;
    bool CreateD3D12RenderTargets() noexcept;
    bool CreateD3D12RtvHeap() noexcept;
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
    void BeforeResizeBuffers(
        IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags
    ) noexcept;
    void AfterResizeBuffers(UINT width, UINT height, HRESULT result) noexcept;
    bool CaptureCommandQueue(ID3D12CommandQueue* newQueue) noexcept;
    inline void SetViewportIfDirty() noexcept {
        if (window.viewportDirty) [[unlikely]] {
            d3d11Context->RSSetViewports(1, &window.viewport);
            window.viewportDirty = false;
        }
    }

    friend HRESULT __fastcall HookOnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags) noexcept;
    friend HRESULT __fastcall HookOnResizeBuffers(
        IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags
    ) noexcept;
    friend HRESULT __fastcall HookOnResizeBuffers1(
        IDXGISwapChain3* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags,
        const UINT* creationNodeMask, IUnknown* const* presentQueue
    ) noexcept;
    friend void __fastcall HookOnExecuteCommandLists(
        ID3D12CommandQueue* pThis, UINT numCommandLists, const ID3D12CommandList** ppCommandLists
    ) noexcept;
    friend void HookGetCommandQueue();
};
