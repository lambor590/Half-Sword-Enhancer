#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <vector>
#include <functional>

#include "ID3DRenderer.h"
#include "IRenderCallback.h"
#include "Logger.h"
#include "Gui.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "imgui/backends/imgui_impl_win32.h"

// D3D11 renderer with support for D3D12 using D3D11On12
class Renderer : public ID3DRenderer
{
public:
    Renderer() = default;
    ~Renderer() {
        Cleanup();
    }

    void OnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags) override;
    void OnResizeBuffers(IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags) override;
    void SetCommandQueue(ID3D12CommandQueue* commandQueue);
    void SetGetCommandQueueCallback(void (*callback)());

private:
    Logger logger{ "Renderer" };

    struct {
        HWND handle = 0;
        int width = 0;
        int height = 0;
        D3D11_VIEWPORT viewport;
        D3D11_VIEWPORT cachedViewport;
    } window;

    Gui Gui;
    IRenderCallback* GUI = &Gui;
    bool GUIInitialized = false;

    struct {
        bool mustInitResources = true;
        bool firstTimeInitialized = false;
        bool deviceRetrieved = false;
        bool isD3D12 = false;
        bool commandQueueCallbackCalled = false;
        UINT bufferIndex = 0;
        UINT bufferCount = 0;
        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    } state;

    struct {
        Microsoft::WRL::ComPtr<ID3D12Device> d3d12Device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11Context;
        Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
        Microsoft::WRL::ComPtr<ID3D11On12Device> d3d11On12Device;
        Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
        Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain3;
    } devices;

    struct {
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> d3d12RenderTargets;
        std::vector<Microsoft::WRL::ComPtr<ID3D11Resource>> d3d11WrappedBackBuffers;
        std::vector<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>> d3d11RenderTargetViews;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
        UINT rtvDescriptorSize = 0;
    } resources;

    struct {
        Microsoft::WRL::ComPtr<ID3D12Fence> fence;
        UINT64 currentValue = 0;
        HANDLE event = nullptr;
    } fenceSync;
    
    std::function<void()> commandQueueCallback = nullptr;

    bool InitD3DResources(IDXGISwapChain* swapChain);
    bool RetrieveD3DDeviceFromSwapChain();
    bool InitD3D();
    bool InitD3D11();
    bool InitD3D11Device();
    bool InitD3D12();
    bool InitD3D12Device();
    bool CreateRtvDescriptorHeap();
    bool CreateD3D11On12Device();
    bool CreateD3D12Resources();
    bool CreateD3D12BufferResources(UINT index, D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle);
    bool InitializeGUI();
    void ConfigureSwapChainAndWindow(IDXGISwapChain* swapChain);

    void RenderFrame();
    bool PrepareD3D12Resources();
    void FinalizeD3D12Rendering();
    void UpdateViewportIfNeeded();
    
    bool WaitForD3D12CommandQueue();
    bool SignalFenceAndWait(UINT64 fenceValueToSignal = 0);
    void ReleaseViewsBuffersAndContext();
    void Cleanup();

    inline bool CheckSuccess(HRESULT hr) {
        if (FAILED(hr)) {
            logger.Log("DirectX operation failed");
            return false;
        }
        return true;
    }
};