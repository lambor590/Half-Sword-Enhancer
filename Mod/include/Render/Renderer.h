#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <dxgi1_4.h>
#include <fstream>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>

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
        ReleaseViewsBuffersAndContext();

        if (fenceEvent) {
            CloseHandle(fenceEvent);
            fenceEvent = nullptr;
        }

        if (window) {
            ImGui_ImplWin32_Shutdown();
            ImGui_ImplDX11_Shutdown();
            ImGui::DestroyContext();
        }
    }

    void OnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags) override;
    void OnResizeBuffers(IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags) override;
    void SetCommandQueue(ID3D12CommandQueue* commandQueue);
    void SetGetCommandQueueCallback(void (*callback)());

private:
    Logger logger{ "Renderer" };

    HWND window = 0;
    int windowWidth = 0;
    int windowHeight = 0;

    Gui Gui;
    IRenderCallback* GUI = &Gui;
    bool GUIInitialized = false;

    bool mustInitializeD3DResources = true;
    bool firstTimeInitPerformed = false;
    bool isDeviceRetrieved = false;
    bool isRunningD3D12 = false;
    bool getCommandQueueCalled = false;
    UINT bufferIndex = 0;
    UINT bufferCount = 0;

    Microsoft::WRL::ComPtr<ID3D12Device> d3d12Device = nullptr;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11Context = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device = nullptr;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue = nullptr;
    Microsoft::WRL::ComPtr<ID3D11On12Device> d3d11On12Device = nullptr;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain = nullptr;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain3 = nullptr;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> d3d12RenderTargets;
    std::vector<Microsoft::WRL::ComPtr<ID3D11Resource>> d3d11WrappedBackBuffers;
    std::vector<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>> d3d11RenderTargetViews;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
    UINT rtvDescriptorSize = 0;
    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    D3D11_VIEWPORT viewport;
    D3D11_VIEWPORT cachedViewport;
    void (*callbackGetCommandQueue)() = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    UINT64 currentFenceValue = 0;
    HANDLE fenceEvent = nullptr;

    bool InitD3DResources(IDXGISwapChain* swapChain);
    bool RetrieveD3DDeviceFromSwapChain();
    bool InitializeGUI();
    bool InitD3D();
    bool InitD3D11();
    bool InitD3D11Device();
    bool InitD3D12();
    bool InitD3D12Device();
    bool CreateD3D12Resources();
    bool CreateD3D12BufferResources(UINT index, D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle);

    void RenderFrame();
    bool WaitForCommandQueueIfRunningD3D12();
    void ReleaseViewsBuffersAndContext();

    inline bool CheckSuccess(HRESULT hr) {
        if (FAILED(hr)) {
            logger.Log("DirectX operation failed");
            return false;
        }
        return true;
    }
};