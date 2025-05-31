#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <array>
#include <type_traits>

#include "ID3DRenderer.h"
#include "IRenderCallback.h"
#include "Logger.h"
#include "Gui.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "imgui/backends/imgui_impl_win32.h"

constexpr UINT MAX_BUFFERS = 8;
constexpr UINT TIMEOUT_MS = 500;

class Renderer : public ID3DRenderer
{
public:
    void OnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags) override;
    void OnResizeBuffers(IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags) override;
    void SetCommandQueue(ID3D12CommandQueue* commandQueue);
    void SetGetCommandQueueCallback(void (*callback)());
    void Cleanup();

private:
    Logger logger{ "Renderer" };
    IRenderCallback* GUI = &Gui::Get();
    
    struct {
        HWND handle = 0;
        int width = 0, height = 0;
        D3D11_VIEWPORT viewport = {};
        bool viewportDirty = true;
    } window;

    struct {
        bool mustInitResources = true;
        bool isD3D12 = false;
        bool GUIInitialized = false;
        UINT bufferIndex = 0;
        UINT bufferCount = 0;
    } state;

    Microsoft::WRL::ComPtr<ID3D12Device> d3d12Device;
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11Context;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain3;
    Microsoft::WRL::ComPtr<ID3D11On12Device> d3d11On12Device;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, MAX_BUFFERS> d3d12RenderTargets;
    std::array<Microsoft::WRL::ComPtr<ID3D11Resource>, MAX_BUFFERS> d3d11WrappedBackBuffers;
    std::array<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>, MAX_BUFFERS> d3d11RenderTargetViews;
    
    UINT64 fenceValue = 0;
    HANDLE fenceEvent = nullptr;
    void (*commandQueueCallback)() = nullptr;

    void RenderFrame();
    bool InitD3DResources(IDXGISwapChain* swapChain);
    bool InitD3D11();
    bool InitD3D12();
    void UpdateViewport();
    bool SignalAndWait();
    void ReleaseResources();
    
    template<typename T>
    constexpr bool IsValid(T handle) const noexcept {
        if constexpr (std::is_same_v<T, HANDLE>) {
            return handle && handle != INVALID_HANDLE_VALUE;
        } else {
            return handle != nullptr;
        }
    }
};