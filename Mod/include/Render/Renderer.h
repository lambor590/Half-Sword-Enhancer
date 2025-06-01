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
#define CACHE_ALIGN __declspec(align(RenderConfig::CACHE_LINE_SIZE))

class Renderer : public ID3DRenderer
{
public:
    void OnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags) noexcept override;
    void OnResizeBuffers(IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags) noexcept override;
    
    __forceinline void SetCommandQueue(ID3D12CommandQueue* commandQueue) noexcept override {
        this->commandQueue = commandQueue;
    }
    
    void Cleanup() noexcept override;

private:
    Logger logger{ "Renderer" };
    
    CACHE_ALIGN struct {
        bool isD3D12 = false;
        bool needsInit = true;
        bool guiReady = false;
        uint8_t bufferIndex = 0;
        uint8_t bufferCount = 0;
    } state;
    
    CACHE_ALIGN struct {
        HWND handle = nullptr;
        D3D11_VIEWPORT viewport = {};
        int width = 0, height = 0;
        bool viewportDirty = true;
    } window;

    CACHE_ALIGN Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11Context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
    
    Microsoft::WRL::ComPtr<ID3D12Device> d3d12Device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain3;
    Microsoft::WRL::ComPtr<ID3D11On12Device> d3d11On12Device;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    
    CACHE_ALIGN std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, RenderConfig::MAX_RENDER_TARGETS> d3d12RenderTargets;
    std::array<Microsoft::WRL::ComPtr<ID3D11Resource>, RenderConfig::MAX_RENDER_TARGETS> d3d11WrappedBackBuffers;
    std::array<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>, RenderConfig::MAX_RENDER_TARGETS> d3d11RenderTargetViews;
    
    UINT64 fenceValue = 0;
    HANDLE fenceEvent = nullptr;
    
    void (Renderer::*renderFunc)() noexcept = nullptr;

    template<bool IsD3D12>
    __forceinline void RenderFrameImpl() noexcept;
    
    __forceinline void RenderFrameD3D11() noexcept;
    __forceinline void RenderFrameD3D12() noexcept;
    __forceinline bool SignalAndWait() noexcept;
    
    bool InitD3DResources(IDXGISwapChain* swapChain) noexcept;
    bool InitD3D11() noexcept;
    bool InitD3D12() noexcept;
    void ReleaseResources() noexcept;
    
    __forceinline void SetViewportIfDirty() noexcept {
        if (window.viewportDirty) UNLIKELY {
            d3d11Context->RSSetViewports(1, &window.viewport);
            window.viewportDirty = false;
        }
    }
};