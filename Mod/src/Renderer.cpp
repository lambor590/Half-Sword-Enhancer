#include "Render/Renderer.h"

using namespace Microsoft::WRL;

void Renderer::OnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags)
{
    if (state.mustInitResources) {
        if (!InitD3DResources(pThis)) return;
        state.mustInitResources = false;
    }
    
    if (!Gui::IsVisible()) return;
    
    if (state.isD3D12) SignalAndWait();
    RenderFrame();
}

void Renderer::OnResizeBuffers(IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags)
{
    if (state.isD3D12) SignalAndWait();
    if (state.GUIInitialized) ImGui_ImplDX11_InvalidateDeviceObjects();
    
    ReleaseResources();
    
    window.width = width;
    window.height = height;
    window.viewport = CD3D11_VIEWPORT(0.0f, 0.0f, (float)width, (float)height);
    window.viewportDirty = true;
    
    state.mustInitResources = true;
}

void Renderer::RenderFrame()
{
    if (!d3d11Device || !d3d11Context) return;
    
    if (state.isD3D12) {
        if (!swapChain3) return;
        state.bufferIndex = swapChain3->GetCurrentBackBufferIndex();
        if (state.bufferIndex >= state.bufferCount || !d3d11WrappedBackBuffers[state.bufferIndex]) {
            state.mustInitResources = true;
            return;
        }
        d3d11On12Device->AcquireWrappedResources(d3d11WrappedBackBuffers[state.bufferIndex].GetAddressOf(), 1);
    }
    
    if (state.bufferIndex >= state.bufferCount || !d3d11RenderTargetViews[state.bufferIndex]) {
        state.mustInitResources = true;
        return;
    }
    
    d3d11Context->OMSetRenderTargets(1, d3d11RenderTargetViews[state.bufferIndex].GetAddressOf(), nullptr);
    UpdateViewport();
    
    if (state.GUIInitialized) GUI->Render();
    
    if (state.isD3D12) {
        d3d11On12Device->ReleaseWrappedResources(d3d11WrappedBackBuffers[state.bufferIndex].GetAddressOf(), 1);
        d3d11Context->Flush();
        SignalAndWait();
    }
}

void Renderer::UpdateViewport()
{
    if (window.viewportDirty) {
        d3d11Context->RSSetViewports(1, &window.viewport);
        window.viewportDirty = false;
    }
}

void Renderer::SetCommandQueue(ID3D12CommandQueue* commandQueue)
{
    this->commandQueue = commandQueue;
}

void Renderer::SetGetCommandQueueCallback(void (*callback)())
{
    commandQueueCallback = callback;
}

bool Renderer::InitD3DResources(IDXGISwapChain* swapChain)
{
    this->swapChain = swapChain;
    
    if (SUCCEEDED(swapChain->GetDevice(__uuidof(ID3D11Device), (void**)&d3d11Device))) {
        return InitD3D11();
    }
    
    if (SUCCEEDED(swapChain->GetDevice(__uuidof(ID3D12Device), (void**)&d3d12Device))) {
        state.isD3D12 = true;
        
        if (!commandQueue) {
            if (commandQueueCallback) {
                commandQueueCallback();
            }
            if (!commandQueue) {
                return false;
            }
        }
        
        return InitD3D12();
    }
    
    return false;
}

bool Renderer::InitD3D11()
{
    d3d11Device->GetImmediateContext(&d3d11Context);
    
    DXGI_SWAP_CHAIN_DESC desc;
    swapChain->GetDesc(&desc);
    window.handle = desc.OutputWindow;
    
    RECT clientRect;
    GetClientRect(window.handle, &clientRect);
    window.width = clientRect.right - clientRect.left;
    window.height = clientRect.bottom - clientRect.top;
    window.viewport = CD3D11_VIEWPORT(0.0f, 0.0f, (float)window.width, (float)window.height);
    
    state.bufferCount = 1;
    
    ComPtr<ID3D11Texture2D> backbuffer;
    if (FAILED(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbuffer))) return false;
    if (FAILED(d3d11Device->CreateRenderTargetView(backbuffer.Get(), nullptr, &d3d11RenderTargetViews[0]))) return false;
    
    if (!state.GUIInitialized && IsValid(window.handle)) {
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(window.handle);
        ImGui_ImplDX11_Init(d3d11Device.Get(), d3d11Context.Get());
        GUI->Init(d3d11Device, d3d11Context, window.handle);
        GUI->Setup();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
        state.GUIInitialized = true;
    }
    
    return true;
}

bool Renderer::InitD3D12()
{
    if (!commandQueue) return false;
    
    if (FAILED(d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) return false;
    
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!IsValid(fenceEvent)) return false;
    fenceValue = 1;
    
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = MAX_BUFFERS;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    if (FAILED(d3d12Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)))) return false;
    
    if (FAILED(D3D11On12CreateDevice(d3d12Device.Get(), D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
        reinterpret_cast<IUnknown**>(commandQueue.GetAddressOf()), 1, 0, &d3d11Device, &d3d11Context, nullptr))) return false;
    
    if (FAILED(d3d11Device.As(&d3d11On12Device))) return false;
    if (FAILED(swapChain.As(&swapChain3))) return false;
    
    DXGI_SWAP_CHAIN_DESC desc;
    swapChain->GetDesc(&desc);
    window.handle = desc.OutputWindow;
    state.bufferCount = desc.BufferCount;
    
    RECT clientRect;
    GetClientRect(window.handle, &clientRect);
    window.width = clientRect.right - clientRect.left;
    window.height = clientRect.bottom - clientRect.top;
    window.viewport = CD3D11_VIEWPORT(0.0f, 0.0f, (float)window.width, (float)window.height);
    
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    UINT stride = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    
    for (UINT i = 0; i < state.bufferCount; i++) {
        if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&d3d12RenderTargets[i])))) return false;
        d3d12Device->CreateRenderTargetView(d3d12RenderTargets[i].Get(), nullptr, rtvHandle);
        
        D3D11_RESOURCE_FLAGS flags = { D3D11_BIND_RENDER_TARGET };
        if (FAILED(d3d11On12Device->CreateWrappedResource(d3d12RenderTargets[i].Get(), &flags,
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT, IID_PPV_ARGS(&d3d11WrappedBackBuffers[i])))) return false;
        
        if (FAILED(d3d11Device->CreateRenderTargetView(d3d11WrappedBackBuffers[i].Get(), nullptr, &d3d11RenderTargetViews[i]))) return false;
        
        rtvHandle.ptr += stride;
    }
    
    if (!state.GUIInitialized && IsValid(window.handle)) {
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(window.handle);
        ImGui_ImplDX11_Init(d3d11Device.Get(), d3d11Context.Get());
        GUI->Init(d3d11Device, d3d11Context, window.handle);
        GUI->Setup();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
        state.GUIInitialized = true;
    }
    
    return true;
}

bool Renderer::SignalAndWait()
{
    if (!state.isD3D12 || !fence || !commandQueue) return false;
    
    if (FAILED(commandQueue->Signal(fence.Get(), fenceValue))) return false;
    
    if (fence->GetCompletedValue() < fenceValue) {
        if (SUCCEEDED(fence->SetEventOnCompletion(fenceValue, fenceEvent))) {
            WaitForSingleObject(fenceEvent, TIMEOUT_MS);
        }
    }
    
    fenceValue++;
    return true;
}

void Renderer::ReleaseResources()
{
    if (d3d11Context) {
        d3d11Context->ClearState();
        d3d11Context->Flush();
    }
    
    for (auto& rtv : d3d11RenderTargetViews) rtv.Reset();
    for (auto& buf : d3d11WrappedBackBuffers) buf.Reset();
    for (auto& rt : d3d12RenderTargets) rt.Reset();
    
    state.bufferIndex = 0;
    state.mustInitResources = true;
}

void Renderer::Cleanup()
{
    ReleaseResources();
    
    if (state.isD3D12 && fence && commandQueue) {
        commandQueue->Signal(fence.Get(), fenceValue + 1);
    }
    
    d3d11Context.Reset();
    d3d11Device.Reset();
    d3d12Device.Reset();
    commandQueue.Reset();
    swapChain.Reset();
    swapChain3.Reset();
    d3d11On12Device.Reset();
    rtvHeap.Reset();
    fence.Reset();
    
    if (IsValid(fenceEvent)) {
        CloseHandle(fenceEvent);
        fenceEvent = nullptr;
    }
    
    if (IsValid(window.handle)) {
        ImGui_ImplWin32_Shutdown();
        ImGui_ImplDX11_Shutdown();
        ImGui::DestroyContext();
        window.handle = 0;
    }
    
    state = {};
    state.mustInitResources = true;
}