#include "Renderer.h"

using namespace Microsoft::WRL;
using namespace DirectX;

void Renderer::OnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags)
{
    if (!mustInitializeD3DResources) {
        if (isRunningD3D12 && fence && commandQueue) {
            const UINT64 fenceValue = currentFenceValue;
            if (SUCCEEDED(commandQueue->Signal(fence.Get(), fenceValue))) {
                if (fence->GetCompletedValue() < fenceValue) {
                    fence->SetEventOnCompletion(fenceValue, fenceEvent);
                    WaitForSingleObject(fenceEvent, INFINITE);
                }
                currentFenceValue++;
            }
        }

        RenderFrame();
        return;
    }

    if (!InitD3DResources(pThis))
        return;

    if (!GUIInitialized && InitializeGUI()) {
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
        GUIInitialized = true;
    }

    RenderFrame();
    mustInitializeD3DResources = false;
}

void Renderer::RenderFrame()
{
    if (isRunningD3D12) {
        bufferIndex = swapChain3->GetCurrentBackBufferIndex();

        if (d3d11WrappedBackBuffers[bufferIndex]) {
            d3d11On12Device->AcquireWrappedResources(d3d11WrappedBackBuffers[bufferIndex].GetAddressOf(), 1);
        }
    }

    if (d3d11RenderTargetViews[bufferIndex]) {
        d3d11Context->OMSetRenderTargets(1, d3d11RenderTargetViews[bufferIndex].GetAddressOf(), nullptr);
    }

    if (viewport.Width != cachedViewport.Width || viewport.Height != cachedViewport.Height) {
        d3d11Context->RSSetViewports(1, &viewport);
        cachedViewport = viewport;
    }

    GUI->Render();

    if (isRunningD3D12 && d3d11WrappedBackBuffers[bufferIndex]) {
        d3d11On12Device->ReleaseWrappedResources(d3d11WrappedBackBuffers[bufferIndex].GetAddressOf(), 1);

        if (fence && commandQueue) {
            const UINT64 fenceValue = currentFenceValue;
            commandQueue->Signal(fence.Get(), fenceValue);
            currentFenceValue++;
        }
    }
}

bool Renderer::InitializeGUI()
{
    if (!d3d11Device || !d3d11Context || !window)
        return false;

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;

    io.ConfigWindowsResizeFromEdges = true;
    io.MouseDrawCursor = false;

    GUI->Init(d3d11Device, d3d11Context, window);
    GUI->Setup();

    logger.Log("GUI initialized");
    return true;
}

void Renderer::OnResizeBuffers(IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags)
{
    logger.Log("ResizeBuffers was called!");

    if (isRunningD3D12 && fence && commandQueue) {
        const UINT64 fenceValue = currentFenceValue;
        if (SUCCEEDED(commandQueue->Signal(fence.Get(), fenceValue))) {
            if (fence->GetCompletedValue() < fenceValue) {
                fence->SetEventOnCompletion(fenceValue, fenceEvent);
                WaitForSingleObject(fenceEvent, INFINITE);
            }
        }
    }

    ReleaseViewsBuffersAndContext();

    viewport = CD3D11_VIEWPORT(
        0.0f, 0.0f,
        static_cast<float>(width),
        static_cast<float>(height)
    );

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
    return isRunningD3D12 ? InitD3D12() : InitD3D11();
}

bool Renderer::InitD3D11()
{
    if (!firstTimeInitPerformed && !InitD3D11Device())
        return false;

    ComPtr<ID3D11Texture2D> backbuffer;
    if (!CheckSuccess(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backbuffer.GetAddressOf()))) {
        logger.Log("Failed to get backbuffer");
        return false;
    }

    d3d11RenderTargetViews.resize(1);
    if (!CheckSuccess(d3d11Device->CreateRenderTargetView(backbuffer.Get(), nullptr, d3d11RenderTargetViews[0].GetAddressOf()))) {
        logger.Log("Failed to create render target view");
        return false;
    }

    return true;
}

bool Renderer::InitD3D11Device()
{
    d3d11Device->GetImmediateContext(&d3d11Context);

    ImGui::CreateContext();
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(d3d11Device.Get(), d3d11Context.Get());

    return true;
}

bool Renderer::InitD3D12()
{
    if (!firstTimeInitPerformed && !InitD3D12Device())
        return false;

    return CreateD3D12Resources();
}

bool Renderer::InitD3D12Device()
{
    if (!CheckSuccess(d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) {
        logger.Log("Failed to create fence");
        return false;
    }

    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent) {
        logger.Log("Failed to create fence event");
        return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        8,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        0
    };

    if (!CheckSuccess(d3d12Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)))) {
        logger.Log("Failed to create static RTV heap");
        return false;
    }

    rtvDescriptorSize = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    if (!CheckSuccess(D3D11On12CreateDevice(
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
        !CheckSuccess(d3d11Device.As(&d3d11On12Device)) ||
        !CheckSuccess(swapChain->QueryInterface(IID_PPV_ARGS(&swapChain3))))
    {
        logger.Log("Failed to create D3D11On12 device");
        return false;
    }

    cachedViewport = CD3D11_VIEWPORT(0.0f, 0.0f, 0.0f, 0.0f);
    return true;
}

bool Renderer::CreateD3D12Resources()
{
    d3d12RenderTargets.resize(bufferCount);
    d3d11WrappedBackBuffers.resize(bufferCount);
    d3d11RenderTargetViews.resize(bufferCount);

    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        bufferCount,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        0
    };

    if (!CheckSuccess(d3d12Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)))) {
        logger.Log("Failed to create descriptor heap");
        return false;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
    UINT rtvDescriptorSize = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    for (UINT i = 0; i < bufferCount; i++) {
        if (!CreateD3D12BufferResources(i, rtvHandle))
            return false;
        rtvHandle.ptr += rtvDescriptorSize;
    }

    return true;
}

bool Renderer::CreateD3D12BufferResources(UINT index, D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle)
{
    if (!CheckSuccess(swapChain->GetBuffer(index, IID_PPV_ARGS(&d3d12RenderTargets[index])))) {
        logger.Log("Failed to get buffer");
        return false;
    }

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    d3d12Device->CreateRenderTargetView(d3d12RenderTargets[index].Get(), &rtvDesc, rtvHandle);

    D3D11_RESOURCE_FLAGS d3d11Flags = { D3D11_BIND_RENDER_TARGET };

    if (!CheckSuccess(d3d11On12Device->CreateWrappedResource(
        d3d12RenderTargets[index].Get(),
        &d3d11Flags,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COMMON,
        IID_PPV_ARGS(&d3d11WrappedBackBuffers[index]))))
    {
        logger.Log("Failed to create wrapped resource");
        return false;
    }

    D3D11_RENDER_TARGET_VIEW_DESC d3d11RtvDesc = {};
    d3d11RtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    d3d11RtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    if (!CheckSuccess(d3d11Device->CreateRenderTargetView(
        d3d11WrappedBackBuffers[index].Get(),
        &d3d11RtvDesc,
        d3d11RenderTargetViews[index].GetAddressOf())))
    {
        logger.Log("Failed to create render target view");
        return false;
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

    if (fence && commandQueue) {
        const UINT64 fenceValue = currentFenceValue;
        if (SUCCEEDED(commandQueue->Signal(fence.Get(), fenceValue))) {
            if (fence->GetCompletedValue() < fenceValue) {
                fence->SetEventOnCompletion(fenceValue, fenceEvent);
                WaitForSingleObject(fenceEvent, INFINITE);
            }
            currentFenceValue++;
        }
    }

    return true;
}

void Renderer::ReleaseViewsBuffersAndContext()
{
    if (d3d11Context) {
        d3d11Context->ClearState();
        d3d11Context->Flush();
    }

    if (isRunningD3D12) {
        for (auto& buffer : d3d11WrappedBackBuffers) {
            if (buffer) buffer.Reset();
        }
        d3d11WrappedBackBuffers.clear();

        for (auto& target : d3d12RenderTargets) {
            if (target) target.Reset();
        }
        d3d12RenderTargets.clear();
    }

    for (auto& view : d3d11RenderTargetViews) {
        if (view) view.Reset();
    }
    d3d11RenderTargetViews.clear();

    bufferIndex = 0;
    mustInitializeD3DResources = true;

    logger.Log("Released D3D resources");
}