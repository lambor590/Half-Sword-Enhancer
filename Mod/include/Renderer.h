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
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <vector>
#include <comdef.h>

#include "ID3DRenderer.h"
#include "IRenderCallback.h"
#include "Logger.h"
#include "Gui.h"
#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

// D3D11 renderer with support for D3D12 using D3D11On12
class Renderer : public ID3DRenderer
{
public:
    void OnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags);
    void OnResizeBuffers(IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags);
    void SetCommandQueue(ID3D12CommandQueue* commandQueue);
    void SetGetCommandQueueCallback(void (*callback)());

private:
    Logger logger{ "Renderer" };
    HWND window = 0;
    Gui Gui;

    IRenderCallback* GUI = &Gui;
    void (*callbackGetCommandQueue)();
    bool mustInitializeD3DResources = true;
    bool firstTimeInitPerformed = false;
    bool isDeviceRetrieved = false;
    bool isRunningD3D12 = false;
    bool getCommandQueueCalled = false;
    bool GUIInitialized = false;
    int windowWidth = 0;
    int windowHeight = 0;
    UINT bufferIndex = 0;
    UINT bufferCount = 0;

    Microsoft::WRL::ComPtr<ID3D12Device> d3d12Device = nullptr;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11Context = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device = nullptr;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue = nullptr;
    Microsoft::WRL::ComPtr<ID3D11On12Device> d3d11On12Device = nullptr;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> d3d12RenderTargets;
    std::vector<Microsoft::WRL::ComPtr<ID3D11Resource>> d3d11WrappedBackBuffers;
    std::vector<Microsoft::WRL::ComPtr<ID3D11RenderTargetView>> d3d11RenderTargetViews;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain = nullptr;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain3 = nullptr;
    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    D3D11_VIEWPORT viewport;

    bool InitD3DResources(IDXGISwapChain* swapChain);
    bool RetrieveD3DDeviceFromSwapChain();
    bool WaitForCommandQueueIfRunningD3D12();
    bool InitD3D();
    void ReleaseViewsBuffersAndContext();
    inline bool CheckSuccess(HRESULT hr) { return SUCCEEDED(hr); }
};