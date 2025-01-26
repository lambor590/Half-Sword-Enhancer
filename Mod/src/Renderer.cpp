#include "Renderer.h"

using namespace Microsoft::WRL;
using namespace DirectX;

void Renderer::OnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags)
{
    if (mustInitializeD3DResources && !InitD3DResources(pThis))
        return;
    
    if (!GUIInitialized) {
        GUI->Init(d3d11Device, d3d11Context, window);
        GUI->Setup();
        GUIInitialized = true;
    }

    if (isRunningD3D12) {
        bufferIndex = swapChain3->GetCurrentBackBufferIndex();
        d3d11On12Device->AcquireWrappedResources(d3d11WrappedBackBuffers[bufferIndex].GetAddressOf(), 1);
    }

    d3d11Context->OMSetRenderTargets(1, d3d11RenderTargetViews[bufferIndex].GetAddressOf(), nullptr);
    d3d11Context->RSSetViewports(1, &viewport);

    GUI->Render();

    if (isRunningD3D12) {
        d3d11On12Device->ReleaseWrappedResources(d3d11WrappedBackBuffers[bufferIndex].GetAddressOf(), 1);
        commandQueue->Signal(0, 0);
    }

    mustInitializeD3DResources = false;
}

void Renderer::OnResizeBuffers(IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags)
{
    logger.Log("ResizeBuffers was called!");
    ReleaseViewsBuffersAndContext();
    mustInitializeD3DResources = true;
}

void Renderer::SetCommandQueue(ID3D12CommandQueue* commandQueue)
{
    this->commandQueue = commandQueue;
}

void Renderer::SetGetCommandQueueCallback(void (*callback)())
{
    callbackGetCommandQueue = callback;
}

bool Renderer::InitD3DResources(IDXGISwapChain* swapChain)
{
    if (!isDeviceRetrieved) {
        this->swapChain = swapChain;
        if (!RetrieveD3DDeviceFromSwapChain())
            return false;
        isDeviceRetrieved = true;
    }

    if (WaitForCommandQueueIfRunningD3D12())
        return false;

    swapChain->GetDesc(&swapChainDesc);
    bufferCount = isRunningD3D12 ? swapChainDesc.BufferCount : 1;

    RECT hwndRect;
    GetClientRect(swapChainDesc.OutputWindow, &hwndRect);
    windowWidth = hwndRect.right - hwndRect.left;
    windowHeight = hwndRect.bottom - hwndRect.top;
    window = swapChainDesc.OutputWindow;

    viewport = CD3D11_VIEWPORT(0.0f, 0.0f,
        static_cast<float>(windowWidth),
        static_cast<float>(windowHeight));

    if (!InitD3D())
        return false;

    firstTimeInitPerformed = true;
    logger.Log("Successfully initialized D3D resources");
    return true;
}

bool Renderer::RetrieveD3DDeviceFromSwapChain()
{
    if (SUCCEEDED(swapChain->GetDevice(__uuidof(ID3D11Device), (void**)d3d11Device.GetAddressOf())))
        return true;
        
    if (SUCCEEDED(swapChain->GetDevice(__uuidof(ID3D12Device), (void**)d3d12Device.GetAddressOf()))) {
        isRunningD3D12 = true;
        return true;
    }
    
    return false;
}

bool Renderer::InitD3D()
{
    if (!isRunningD3D12) {
        // D3D11 initialization
        if (!firstTimeInitPerformed) {
            d3d11Device->GetImmediateContext(&d3d11Context);
        }

        ComPtr<ID3D11Texture2D> backbuffer;
        if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backbuffer.GetAddressOf())))
            return false;

        d3d11RenderTargetViews.resize(1);
        if (FAILED(d3d11Device->CreateRenderTargetView(backbuffer.Get(), nullptr, d3d11RenderTargetViews[0].GetAddressOf())))
            return false;

        if (!firstTimeInitPerformed) {
            ImGui::CreateContext();
            ImGui_ImplWin32_Init(window);
            ImGui_ImplDX11_Init(d3d11Device.Get(), d3d11Context.Get());
        }
    } else {
        // D3D12 initialization
        if (!firstTimeInitPerformed) {
            if (FAILED(D3D11On12CreateDevice(
                d3d12Device.Get(),
                NULL,
                nullptr,
                0,
                reinterpret_cast<IUnknown**>(commandQueue.GetAddressOf()),
                1,
                0,
                d3d11Device.GetAddressOf(),
                d3d11Context.GetAddressOf(),
                nullptr)) ||
                FAILED(d3d11Device.As(&d3d11On12Device)) ||
                FAILED(swapChain->QueryInterface(IID_PPV_ARGS(&swapChain3))))
            {
                return false;
            }
        }

        // Create D3D12 resources
        d3d12RenderTargets.resize(bufferCount);
        d3d11WrappedBackBuffers.resize(bufferCount);
        d3d11RenderTargetViews.resize(bufferCount);

        ComPtr<ID3D12DescriptorHeap> rtvHeap;
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = bufferCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        if (FAILED(d3d12Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap))))
            return false;

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
        UINT rtvDescriptorSize = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        for (UINT i = 0; i < bufferCount; i++) {
            if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&d3d12RenderTargets[i]))))
                return false;

            d3d12Device->CreateRenderTargetView(d3d12RenderTargets[i].Get(), nullptr, rtvHandle);

            D3D11_RESOURCE_FLAGS d3d11Flags = { D3D11_BIND_RENDER_TARGET };
            if (FAILED(d3d11On12Device->CreateWrappedResource(
                d3d12RenderTargets[i].Get(),
                &d3d11Flags,
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PRESENT,
                IID_PPV_ARGS(&d3d11WrappedBackBuffers[i]))))
            {
                return false;
            }

            if (FAILED(d3d11Device->CreateRenderTargetView(
                d3d11WrappedBackBuffers[i].Get(),
                nullptr,
                d3d11RenderTargetViews[i].GetAddressOf())))
            {
                return false;
            }

            rtvHandle.ptr += rtvDescriptorSize;
        }
    }

    return true;
}

bool Renderer::WaitForCommandQueueIfRunningD3D12()
{
    if (!isRunningD3D12 || commandQueue)
        return false;
        
    if (!getCommandQueueCalled && callbackGetCommandQueue) {
        callbackGetCommandQueue();
        getCommandQueueCalled = true;
    }
    return true;
}

void Renderer::ReleaseViewsBuffersAndContext()
{
    if (d3d11Context)
        d3d11Context->Flush();

    d3d11RenderTargetViews.clear();
    
    if (isRunningD3D12) {
        d3d12RenderTargets.clear();
        d3d11WrappedBackBuffers.clear();
    }
    
    mustInitializeD3DResources = true;
}