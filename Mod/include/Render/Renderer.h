#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <array>

#include "ID3DRenderer.h"
#include "Render/RenderConfig.h"
#include "Logger.h"
#include "Gui.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "imgui/backends/imgui_impl_win32.h"

#define LIKELY [[likely]]
#define UNLIKELY [[unlikely]]
class Renderer : public ID3DRenderer {
public:
    void OnPresent(IDXGISwapChain* pThis, UINT, UINT) noexcept override;
    void OnResizeBuffers(IDXGISwapChain*, UINT, UINT width, UINT height, DXGI_FORMAT, UINT) noexcept override;

    __forceinline void SetCommandQueue(ID3D12CommandQueue* newQueue) noexcept override {
        if (commandQueue.Get() != newQueue) UNLIKELY {
                commandQueue = newQueue;
            }
    }

    void SetGetCommandQueueCallback(void (*callback)()) noexcept override { commandQueueCallback = callback; }

    void Cleanup() noexcept override;

private:
    Logger logger{"Renderer"};

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

    Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11Context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;

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

    void (*commandQueueCallback)() = nullptr;

    template <bool IsD3D12> __forceinline void RenderFrameImpl() noexcept;

    __forceinline void RenderFrameD3D11() noexcept;
    __forceinline void RenderFrameD3D12() noexcept;
    bool SignalAndWait() noexcept;

    bool InitD3DResources(IDXGISwapChain* sc) noexcept;
    bool InitD3D11() noexcept;
    bool InitD3D12() noexcept;
    void ReleaseResources() noexcept;
    void InitOrReinitImGui() noexcept;

    __forceinline void SetViewportIfDirty() noexcept {
        if (window.viewportDirty) UNLIKELY {
                d3d11Context->RSSetViewports(1, &window.viewport);
                window.viewportDirty = false;
            }
    }
};
