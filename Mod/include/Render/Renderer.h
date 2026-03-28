#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <array>

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
    Logger logger{"Renderer"};

    // --- Hook state (formerly in DirectXHook) ---
    uintptr_t presentReturnAddress = 0;
    uintptr_t resizeBuffersReturnAddress = 0;
    uintptr_t executeCommandListsAddress = 0;
    uintptr_t executeCommandListsReturnAddress = 0;

    // --- Render state ---
    struct {
        void (Renderer::*renderFunc)() noexcept = nullptr;
        bool needsInit = true;
        bool guiReady = false;
        bool isD3D12 = false;
        uint8_t bufferIndex = 0;
        uint8_t bufferCount = 0;
    } state;

    struct {
        HWND handle = nullptr;
        D3D11_VIEWPORT viewport = {};
        int width = 0, height = 0;
        bool viewportDirty = true;
    } window;

    // --- D3D11 resources ---
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11Context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;

    // --- D3D12 resources ---
    Microsoft::WRL::ComPtr<ID3D12Device> d3d12Device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain3;
    Microsoft::WRL::ComPtr<ID3D11On12Device> d3d11On12Device;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;

    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, RenderConfig::MAX_RENDER_TARGETS> d3d12RenderTargets;
    std::array<Microsoft::WRL::ComPtr<ID3D11Resource>, RenderConfig::MAX_RENDER_TARGETS> d3d11WrappedBackBuffers;
    std::array<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>, RenderConfig::MAX_RENDER_TARGETS> d3d11RenderTargetViews;

    UINT64 fenceValue = 0;
    HANDLE fenceEvent = nullptr;

    // --- Hook setup (formerly DirectXHook) ---
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

    // --- Render frame (templated for DX11/DX12) ---
    template <bool IsD3D12> void RenderFrameImpl() noexcept;
    void RenderFrameD3D11() noexcept;
    void RenderFrameD3D12() noexcept;

    // --- DX resource init ---
    bool InitD3DResources(IDXGISwapChain* sc) noexcept;
    bool InitD3D11() noexcept;
    bool InitD3D12() noexcept;
    void InitOrReinitImGui() noexcept;

    // --- Shared cleanup helper ---
    void ReleaseRenderTargets() noexcept;
    bool SignalAndWait() noexcept;

    // --- Present/Resize callbacks (called from static hook trampolines) ---
    void OnPresent(IDXGISwapChain* pThis) noexcept;
    void OnResizeBuffers(UINT width, UINT height) noexcept;
    void SetCommandQueue(ID3D12CommandQueue* newQueue) noexcept;

    inline void SetViewportIfDirty() noexcept {
        if (window.viewportDirty) [[unlikely]] {
            d3d11Context->RSSetViewports(1, &window.viewport);
            window.viewportDirty = false;
        }
    }

    // Static hook trampolines need access to private members
    friend HRESULT __fastcall HookOnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags) noexcept;
    friend HRESULT __fastcall HookOnResizeBuffers(
        IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags
    ) noexcept;
    friend void __fastcall HookOnExecuteCommandLists(
        ID3D12CommandQueue* pThis, UINT numCommandLists, const ID3D12CommandList** ppCommandLists
    ) noexcept;
    friend void HookGetCommandQueue();
};
