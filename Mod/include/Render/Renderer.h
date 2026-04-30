#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <vector>
#include <wrl/client.h>

#include "Render/RenderConfig.h"
#include "Logger.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "imgui/backends/imgui_impl_win32.h"

using Present = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffers = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
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
        RenderBackend backend = RenderBackend::Unknown;
        uint8_t bufferIndex = 0;
        uint8_t bufferCount = 0;
    };

    struct D3D12FrameTarget {
        Microsoft::WRL::ComPtr<ID3D12Resource> backbuffer;
        Microsoft::WRL::ComPtr<ID3D11Resource> wrappedBuffer;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTarget;
    };

    Logger logger{"Renderer"};

    // Addresses of the original methods after MemoryUtils installs the detours.
    uintptr_t presentAddress = 0;
    uintptr_t presentReturnAddress = 0;
    uintptr_t resizeBuffersAddress = 0;
    uintptr_t resizeBuffersReturnAddress = 0;
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
    Microsoft::WRL::ComPtr<ID3D11On12Device> d3d11On12Device;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;

    UINT64 fenceValue = 0;
    HANDLE fenceEvent = nullptr;
    std::vector<D3D12FrameTarget> d3d12FrameTargets;

    IDXGISwapChain* CreateDummySwapChain();
    void HookSwapChain(
        IDXGISwapChain* dummySwapChain, uintptr_t presentDetourFunction, uintptr_t resizeBuffersDetourFunction,
        uintptr_t* outPresentReturn, uintptr_t* outResizeReturn
    );
    ID3D12CommandQueue* CreateDummyCommandQueue();
    void HookCommandQueue(
        ID3D12CommandQueue* dummyCommandQueue, uintptr_t executeCommandListsDetourFunction, uintptr_t* outExecReturn
    );
    void UnhookCommandQueue() const;

    template <bool IsD3D12> void RenderFrameImpl() noexcept;
    void RenderFrameD3D11() noexcept;
    void RenderFrameD3D12() noexcept;

    bool InitD3DResources(IDXGISwapChain* sc) noexcept;
    bool InitD3D11() noexcept;
    bool InitD3D12() noexcept;
    void InitOrReinitImGui() noexcept;
    bool CreateRenderTargets() noexcept;
    bool CreateD3D11RenderTarget() noexcept;
    bool CreateD3D12RenderTargets() noexcept;
    void ReleaseRenderTargets() noexcept;

    void ReleaseContextState() noexcept;
    void ReleaseGraphicsResources() noexcept;
    bool SignalAndWait() noexcept;

    void OnPresent(IDXGISwapChain* pThis) noexcept;
    void BeforeResizeBuffers(
        IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags
    ) noexcept;
    void AfterResizeBuffers(UINT width, UINT height, HRESULT result) noexcept;
    void SetCommandQueue(ID3D12CommandQueue* newQueue) noexcept;
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
    friend void __fastcall HookOnExecuteCommandLists(
        ID3D12CommandQueue* pThis, UINT numCommandLists, const ID3D12CommandList** ppCommandLists
    ) noexcept;
    friend void HookGetCommandQueue();
};
