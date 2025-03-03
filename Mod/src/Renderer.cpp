#include "Render/Renderer.h"

using namespace Microsoft::WRL;

void Renderer::OnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags)
{
    if (state.mustInitResources && !InitD3DResources(pThis)) {
        return;
    }
    
    SignalFenceAndWait();
    RenderFrame();
    
    state.mustInitResources = false;
}

void Renderer::RenderFrame()
{
    if (!devices.d3d11Device || !devices.d3d11Context) return;
    
    if (state.isD3D12) {
        if (!PrepareD3D12Resources()) return;
    }
    
    if (state.bufferIndex >= resources.d3d11RenderTargetViews.size() || 
        !resources.d3d11RenderTargetViews[state.bufferIndex]) {
        state.mustInitResources = true;
        return;
    }
    
    devices.d3d11Context->OMSetRenderTargets(1, 
        resources.d3d11RenderTargetViews[state.bufferIndex].GetAddressOf(), nullptr);
        
    UpdateViewportIfNeeded();
    
    if (GUIInitialized) GUI->Render();
    
    if (state.isD3D12) {
        FinalizeD3D12Rendering();
    }
}

bool Renderer::PrepareD3D12Resources()
{
    if (!devices.swapChain3) return false;
    
    state.bufferIndex = devices.swapChain3->GetCurrentBackBufferIndex();
    
    if (state.bufferIndex >= resources.d3d11WrappedBackBuffers.size() || 
        !resources.d3d11WrappedBackBuffers[state.bufferIndex]) {
        state.mustInitResources = true;
        return false;
    }
    
    devices.d3d11On12Device->AcquireWrappedResources(
        resources.d3d11WrappedBackBuffers[state.bufferIndex].GetAddressOf(), 1);
    
    return true;
}

void Renderer::FinalizeD3D12Rendering()
{
    if (!resources.d3d11WrappedBackBuffers[state.bufferIndex]) return;
    
    devices.d3d11On12Device->ReleaseWrappedResources(
        resources.d3d11WrappedBackBuffers[state.bufferIndex].GetAddressOf(), 1);
    devices.d3d11Context->Flush();
    SignalFenceAndWait();
}

void Renderer::UpdateViewportIfNeeded()
{
    if (window.viewport.Width == window.cachedViewport.Width && 
        window.viewport.Height == window.cachedViewport.Height) {
        return;
    }
    
    devices.d3d11Context->RSSetViewports(1, &window.viewport);
    window.cachedViewport = window.viewport;
}

bool Renderer::InitializeGUI()
{
    if (!devices.d3d11Device || !devices.d3d11Context || !window.handle)
        return false;

    GUI->Init(devices.d3d11Device, devices.d3d11Context, window.handle);
    GUI->Setup();
    
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
    
    logger.Log("GUI initialized");
    GUIInitialized = true;
    return true;
}

void Renderer::OnResizeBuffers(IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags)
{
    SignalFenceAndWait();

    if (GUIInitialized) ImGui_ImplDX11_InvalidateDeviceObjects();
    
    ReleaseViewsBuffersAndContext();

    window.width = width;
    window.height = height;
    window.viewport = CD3D11_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));

    state.mustInitResources = true;
}

void Renderer::SetCommandQueue(ID3D12CommandQueue* commandQueue)
{
    devices.commandQueue = commandQueue;
}

void Renderer::SetGetCommandQueueCallback(void (*callback)())
{
    commandQueueCallback = callback;
}

bool Renderer::InitD3DResources(IDXGISwapChain* swapChain)
{
    if (!state.deviceRetrieved) {
        devices.swapChain = swapChain;
        state.deviceRetrieved = RetrieveD3DDeviceFromSwapChain();
        if (!state.deviceRetrieved) return false;
    }

    if (state.isD3D12 && WaitForD3D12CommandQueue()) return false;

    ConfigureSwapChainAndWindow(swapChain);

    if (!InitD3D()) return false;

    state.firstTimeInitialized = true;
    
    if (!GUIInitialized) InitializeGUI();
    
    return true;
}

void Renderer::ConfigureSwapChainAndWindow(IDXGISwapChain* swapChain)
{
    swapChain->GetDesc(&state.swapChainDesc);
    state.bufferCount = state.isD3D12 ? state.swapChainDesc.BufferCount : 1;

    RECT hwndRect;
    GetClientRect(state.swapChainDesc.OutputWindow, &hwndRect);
    
    window.width = hwndRect.right - hwndRect.left;
    window.height = hwndRect.bottom - hwndRect.top;
    window.handle = state.swapChainDesc.OutputWindow;
    window.viewport = CD3D11_VIEWPORT(0.0f, 0.0f,
        static_cast<float>(window.width),
        static_cast<float>(window.height));
}

bool Renderer::RetrieveD3DDeviceFromSwapChain()
{
    if (SUCCEEDED(devices.swapChain->GetDevice(__uuidof(ID3D11Device), (void**)devices.d3d11Device.GetAddressOf())))
        return true;

    if (SUCCEEDED(devices.swapChain->GetDevice(__uuidof(ID3D12Device), (void**)devices.d3d12Device.GetAddressOf()))) {
        state.isD3D12 = true;
        return true;
    }

    return false;
}

bool Renderer::InitD3D()
{
    return state.isD3D12 ? InitD3D12() : InitD3D11();
}

bool Renderer::InitD3D11()
{
    if (!state.firstTimeInitialized && !InitD3D11Device())
        return false;

    ComPtr<ID3D11Texture2D> backbuffer;
    if (!CheckSuccess(devices.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backbuffer.GetAddressOf()))) {
        logger.Log("Failed to get backbuffer");
        return false;
    }

    resources.d3d11RenderTargetViews.resize(1);
    
    HRESULT hr = devices.d3d11Device->CreateRenderTargetView(
        backbuffer.Get(), 
        nullptr, 
        resources.d3d11RenderTargetViews[0].GetAddressOf());
        
    if (!CheckSuccess(hr)) {
        logger.Log("Failed to create render target view");
        return false;
    }

    return true;
}

bool Renderer::InitD3D11Device()
{
    devices.d3d11Device->GetImmediateContext(&devices.d3d11Context);

    if (!window.handle) return false;
    
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(window.handle);
    ImGui_ImplDX11_Init(devices.d3d11Device.Get(), devices.d3d11Context.Get());

    return true;
}

bool Renderer::InitD3D12()
{
    return (!state.firstTimeInitialized && !InitD3D12Device()) 
        ? false 
        : CreateD3D12Resources();
}

bool Renderer::InitD3D12Device()
{
    logger.Log("Initializing D3D12 device...");
    
    HRESULT hr = devices.d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fenceSync.fence));
    if (!CheckSuccess(hr)) {
        logger.Log("Failed to create fence");
        return false;
    }

    fenceSync.event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fenceSync.event) {
        logger.Log("Failed to create fence event");
        return false;
    }
    
    fenceSync.currentValue = 1;

    if (!CreateRtvDescriptorHeap()) {
        return false;
    }

    if (!CreateD3D11On12Device()) {
        return false;
    }

    if (!state.firstTimeInitialized && window.handle) {
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(window.handle);
        ImGui_ImplDX11_Init(devices.d3d11Device.Get(), devices.d3d11Context.Get());
    }

    window.cachedViewport = CD3D11_VIEWPORT(0.0f, 0.0f, 0.0f, 0.0f);
    logger.Log("D3D12 device initialized successfully");
    return true;
}

bool Renderer::CreateRtvDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        8,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        0
    };

    HRESULT hr = devices.d3d12Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&resources.rtvHeap));
    if (!CheckSuccess(hr)) {
        logger.Log("Failed to create static RTV heap");
        return false;
    }

    resources.rtvDescriptorSize = devices.d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    return true;
}

bool Renderer::CreateD3D11On12Device()
{
    HRESULT hr = D3D11On12CreateDevice(
        devices.d3d12Device.Get(),
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,
        0,
        reinterpret_cast<IUnknown**>(devices.commandQueue.GetAddressOf()),
        1,
        0,
        devices.d3d11Device.GetAddressOf(),
        devices.d3d11Context.GetAddressOf(),
        nullptr);
    
    if (!CheckSuccess(hr)) {
        logger.Log("Failed to create D3D11On12 device");
        return false;
    }

    return CheckSuccess(devices.d3d11Device.As(&devices.d3d11On12Device)) && 
           CheckSuccess(devices.swapChain->QueryInterface(IID_PPV_ARGS(&devices.swapChain3)));
}

bool Renderer::CreateD3D12Resources()
{
    resources.d3d12RenderTargets.resize(state.bufferCount);
    resources.d3d11WrappedBackBuffers.resize(state.bufferCount);
    resources.d3d11RenderTargetViews.resize(state.bufferCount);

    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        state.bufferCount,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        0
    };

    if (!CheckSuccess(devices.d3d12Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)))) {
        logger.Log("Failed to create descriptor heap");
        return false;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
    bool success = true;
    
    for (UINT i = 0; i < state.bufferCount && success; i++) {
        success = CreateD3D12BufferResources(i, rtvHandle);
        rtvHandle.ptr += resources.rtvDescriptorSize;
    }

    logger.Log(success ? "Successfully created D3D12 resources" : "Failed to create some D3D12 resources");
    return success;
}

bool Renderer::CreateD3D12BufferResources(UINT index, D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle)
{
    if (!CheckSuccess(devices.swapChain->GetBuffer(index, IID_PPV_ARGS(&resources.d3d12RenderTargets[index])))) {
        return false;
    }

    devices.d3d12Device->CreateRenderTargetView(resources.d3d12RenderTargets[index].Get(), nullptr, rtvHandle);

    D3D11_RESOURCE_FLAGS d3d11Flags = { D3D11_BIND_RENDER_TARGET };

    bool createWrappedSuccess = CheckSuccess(devices.d3d11On12Device->CreateWrappedResource(
        resources.d3d12RenderTargets[index].Get(),
        &d3d11Flags,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT,
        IID_PPV_ARGS(&resources.d3d11WrappedBackBuffers[index])));
        
    bool createRtvSuccess = createWrappedSuccess && CheckSuccess(devices.d3d11Device->CreateRenderTargetView(
        resources.d3d11WrappedBackBuffers[index].Get(),
        nullptr,
        resources.d3d11RenderTargetViews[index].GetAddressOf()));
        
    return createWrappedSuccess && createRtvSuccess;
}

bool Renderer::WaitForD3D12CommandQueue()
{
    if (!devices.commandQueue) {
        if (!state.commandQueueCallbackCalled && commandQueueCallback) {
            commandQueueCallback();
            state.commandQueueCallbackCalled = true;
        }
        return true;
    }

    SignalFenceAndWait();
    return false;
}

bool Renderer::SignalFenceAndWait(UINT64 fenceValueToSignal)
{
    if (!state.isD3D12 || !fenceSync.fence || !devices.commandQueue) 
        return false;

    UINT64 valueToSignal = (fenceValueToSignal > 0) ? fenceValueToSignal : fenceSync.currentValue;
    
    bool success = 
        SUCCEEDED(devices.commandQueue->Signal(fenceSync.fence.Get(), valueToSignal)) && 
        fenceSync.fence->GetCompletedValue() < valueToSignal &&
        SUCCEEDED(fenceSync.fence->SetEventOnCompletion(valueToSignal, fenceSync.event));
        
    if (success) {
        WaitForSingleObject(fenceSync.event, INFINITE);
        fenceSync.currentValue++;
    }
    
    return success;
}

void Renderer::ReleaseViewsBuffersAndContext()
{
    if (devices.d3d11Context) {
        devices.d3d11Context->ClearState();
        devices.d3d11Context->Flush();
    }

    SignalFenceAndWait();
    
    resources.d3d11RenderTargetViews.clear();
    
    if (state.isD3D12) {
        resources.d3d11WrappedBackBuffers.clear();
        resources.d3d12RenderTargets.clear();
    }
    
    if (devices.d3d11Context) {
        devices.d3d11Context->Flush();
    }
    
    state.bufferIndex = 0;
    state.mustInitResources = true;

    logger.Log("All D3D resources have been successfully released");
}

void Renderer::Cleanup()
{
    ReleaseViewsBuffersAndContext();

    if (fenceSync.event) {
        CloseHandle(fenceSync.event);
        fenceSync.event = nullptr;
    }

    if (window.handle) {
        ImGui_ImplWin32_Shutdown();
        ImGui_ImplDX11_Shutdown();
        ImGui::DestroyContext();
    }
}