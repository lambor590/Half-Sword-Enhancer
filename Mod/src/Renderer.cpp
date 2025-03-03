#include "Render/Renderer.h"

using namespace Microsoft::WRL;

void Renderer::OnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags)
{
    if (!mustInitializeD3DResources) {
        if (isRunningD3D12 && fence && commandQueue) {
            const UINT64 fenceValue = currentFenceValue;
            if (SUCCEEDED(commandQueue->Signal(fence.Get(), fenceValue)) && 
                fence->GetCompletedValue() < fenceValue &&
                SUCCEEDED(fence->SetEventOnCompletion(fenceValue, fenceEvent))) {
                WaitForSingleObject(fenceEvent, INFINITE);
            }
            currentFenceValue++;
        }

        RenderFrame();
        return;
    }

    if (!InitD3DResources(pThis)) return;

    if (!GUIInitialized && InitializeGUI()) {
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
        GUIInitialized = true;
    }

    RenderFrame();
    
    mustInitializeD3DResources = false;
}

void Renderer::RenderFrame()
{
    if (!d3d11Device || !d3d11Context || mustInitializeD3DResources) return;

    if (isRunningD3D12) {
        if (!swapChain3) return;

        bufferIndex = swapChain3->GetCurrentBackBufferIndex();

        if (bufferIndex >= d3d11WrappedBackBuffers.size() || !d3d11WrappedBackBuffers[bufferIndex]) {
            mustInitializeD3DResources = true;
            return;
        }

        d3d11On12Device->AcquireWrappedResources(d3d11WrappedBackBuffers[bufferIndex].GetAddressOf(), 1);
    }

    if (bufferIndex >= d3d11RenderTargetViews.size() || !d3d11RenderTargetViews[bufferIndex]) {
        mustInitializeD3DResources = true;
        return;
    }

    d3d11Context->OMSetRenderTargets(1, d3d11RenderTargetViews[bufferIndex].GetAddressOf(), nullptr);

    if (viewport.Width != cachedViewport.Width || viewport.Height != cachedViewport.Height) {
        d3d11Context->RSSetViewports(1, &viewport);
        cachedViewport = viewport;
    }

    if (GUIInitialized) GUI->Render();

    if (isRunningD3D12 && d3d11WrappedBackBuffers[bufferIndex]) {
        d3d11On12Device->ReleaseWrappedResources(d3d11WrappedBackBuffers[bufferIndex].GetAddressOf(), 1);
        d3d11Context->Flush();

        if (fence && commandQueue && SUCCEEDED(commandQueue->Signal(fence.Get(), currentFenceValue))) 
            currentFenceValue++;
    }
}

bool Renderer::InitializeGUI()
{
    if (!d3d11Device || !d3d11Context || !window)
        return false;

    GUI->Init(d3d11Device, d3d11Context, window);
    GUI->Setup();

    logger.Log("GUI initialized");
    return true;
}

void Renderer::OnResizeBuffers(IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags)
{
    if (isRunningD3D12 && fence && commandQueue) {
        const UINT64 fenceValue = currentFenceValue;
        if (SUCCEEDED(commandQueue->Signal(fence.Get(), fenceValue)) && 
            fence->GetCompletedValue() < fenceValue &&
            SUCCEEDED(fence->SetEventOnCompletion(fenceValue, fenceEvent))) {
                WaitForSingleObject(fenceEvent, INFINITE);
        }
        currentFenceValue++;
    }

    if (GUIInitialized) ImGui_ImplDX11_InvalidateDeviceObjects();
    
    ReleaseViewsBuffersAndContext();

    windowWidth = width;
    windowHeight = height;
    viewport = CD3D11_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));

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
        if (!RetrieveD3DDeviceFromSwapChain()) return false;
        isDeviceRetrieved = true;
    }

    if (WaitForCommandQueueIfRunningD3D12()) return false;

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

    if (!InitD3D()) return false;

    firstTimeInitPerformed = true;
    
    if (!GUIInitialized && InitializeGUI()) {
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
        GUIInitialized = true;
    }
    
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
    
    HRESULT hr = d3d11Device->CreateRenderTargetView(backbuffer.Get(), nullptr, d3d11RenderTargetViews[0].GetAddressOf());
    if (!CheckSuccess(hr)) {
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
    logger.Log("Initializing D3D12 device...");
    
    HRESULT hr = d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if (!CheckSuccess(hr)) {
        logger.Log("Failed to create fence");
        return false;
    }

    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent) {
        logger.Log("Failed to create fence event");
        return false;
    }
    
    currentFenceValue = 1;

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        8,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        0
    };

    hr = d3d12Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));
    if (!CheckSuccess(hr)) {
        logger.Log("Failed to create static RTV heap");
        return false;
    }

    rtvDescriptorSize = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    hr = D3D11On12CreateDevice(
        d3d12Device.Get(),
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,
        0,
        reinterpret_cast<IUnknown**>(commandQueue.GetAddressOf()),
        1,
        0,
        d3d11Device.GetAddressOf(),
        d3d11Context.GetAddressOf(),
        nullptr);
    
    if (!CheckSuccess(hr)) {
        logger.Log("Failed to create D3D11On12 device");
        return false;
    }

    if (!CheckSuccess(d3d11Device.As(&d3d11On12Device)) || 
        !CheckSuccess(swapChain->QueryInterface(IID_PPV_ARGS(&swapChain3)))) {
        return false;
    }

    if (!firstTimeInitPerformed) {
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(window);
        ImGui_ImplDX11_Init(d3d11Device.Get(), d3d11Context.Get());
    }

    cachedViewport = CD3D11_VIEWPORT(0.0f, 0.0f, 0.0f, 0.0f);
    logger.Log("D3D12 device initialized successfully");
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
    
    for (UINT i = 0; i < bufferCount; i++) {
        if (!CreateD3D12BufferResources(i, rtvHandle))
            return false;
        rtvHandle.ptr += rtvDescriptorSize;
    }

    logger.Log("Successfully created D3D12 resources");
    return true;
}

bool Renderer::CreateD3D12BufferResources(UINT index, D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle)
{
    if (!CheckSuccess(swapChain->GetBuffer(index, IID_PPV_ARGS(&d3d12RenderTargets[index])))) {
        return false;
    }

    d3d12Device->CreateRenderTargetView(d3d12RenderTargets[index].Get(), nullptr, rtvHandle);

    D3D11_RESOURCE_FLAGS d3d11Flags = { D3D11_BIND_RENDER_TARGET };

    if (!CheckSuccess(d3d11On12Device->CreateWrappedResource(
        d3d12RenderTargets[index].Get(),
        &d3d11Flags,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT,
        IID_PPV_ARGS(&d3d11WrappedBackBuffers[index]))) ||
        !CheckSuccess(d3d11Device->CreateRenderTargetView(
            d3d11WrappedBackBuffers[index].Get(),
            nullptr,
            d3d11RenderTargetViews[index].GetAddressOf()))) {
        return false;
    }

    return true;
}

bool Renderer::WaitForCommandQueueIfRunningD3D12()
{
    if (!isRunningD3D12)
        return false;
        
    if (!commandQueue) {
        if (!getCommandQueueCalled && callbackGetCommandQueue) {
            callbackGetCommandQueue();
            getCommandQueueCalled = true;
        }
        return true;
    }

    if (fence && commandQueue) {
        const UINT64 fenceValue = currentFenceValue;
        if (SUCCEEDED(commandQueue->Signal(fence.Get(), fenceValue)) && 
            fence->GetCompletedValue() < fenceValue &&
            SUCCEEDED(fence->SetEventOnCompletion(fenceValue, fenceEvent))) {
            WaitForSingleObject(fenceEvent, INFINITE);
            currentFenceValue++;
        }
    }
    
    return false;
}

void Renderer::ReleaseViewsBuffersAndContext()
{
    if (d3d11Context) {
        d3d11Context->ClearState();
        d3d11Context->Flush();
    }

    if (isRunningD3D12 && fence && commandQueue) {
        const UINT64 fenceValue = currentFenceValue;
        if (SUCCEEDED(commandQueue->Signal(fence.Get(), fenceValue)) && 
            fence->GetCompletedValue() < fenceValue &&
            SUCCEEDED(fence->SetEventOnCompletion(fenceValue, fenceEvent))) {
            WaitForSingleObject(fenceEvent, INFINITE);
            currentFenceValue++;
        }
    }
    
    d3d11RenderTargetViews.clear();
    
    if (isRunningD3D12) {
        d3d11WrappedBackBuffers.clear();
        d3d12RenderTargets.clear();
    }
    
    if (d3d11Context) {
        d3d11Context->Flush();
    }
    
    bufferIndex = 0;
    mustInitializeD3DResources = true;

    logger.Log("All D3D resources have been successfully released");
}