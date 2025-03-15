#include "Hooks/DirectXHook.h"

static HRESULT __stdcall OnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags)
{
    g_DirectXHook->renderer->OnPresent(pThis, syncInterval, flags);
    return ((Present)g_DirectXHook->presentReturnAddress)(pThis, syncInterval, flags);
}

static HRESULT __stdcall OnResizeBuffers(IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags)
{
    g_DirectXHook->renderer->OnResizeBuffers(pThis, bufferCount, width, height, newFormat, swapChainFlags);
    return ((ResizeBuffers)g_DirectXHook->resizeBuffersReturnAddress)(pThis, bufferCount, width, height, newFormat, swapChainFlags);
}

static void __stdcall OnExecuteCommandLists(ID3D12CommandQueue* pThis, UINT numCommandLists, const ID3D12CommandList** ppCommandLists)
{
    if (pThis->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
    {
        g_DirectXHook->renderer->SetCommandQueue(pThis);
    }
    ((ExecuteCommandLists)g_DirectXHook->executeCommandListsReturnAddress)(pThis, numCommandLists, ppCommandLists);
}

static void GetCommandQueue()
{
    ID3D12CommandQueue* cmdQueue = g_DirectXHook->CreateDummyCommandQueue();
    g_DirectXHook->HookCommandQueue(cmdQueue, (uintptr_t)&OnExecuteCommandLists, &g_DirectXHook->executeCommandListsReturnAddress);
}

DirectXHook::DirectXHook(ID3DRenderer* renderer) : renderer(renderer) {}

void DirectXHook::Hook()
{
    logger.Log("OnPresent: %p", &OnPresent);
    logger.Log("OnResizeBuffers: %p", &OnResizeBuffers);

    renderer->SetGetCommandQueueCallback(&GetCommandQueue);
    IDXGISwapChain* dummySwapChain = CreateDummySwapChain();
    HookSwapChain(dummySwapChain, (uintptr_t)&OnPresent, (uintptr_t)&OnResizeBuffers, &presentReturnAddress, &resizeBuffersReturnAddress);
}

IDXGISwapChain* DirectXHook::CreateDummySwapChain()
{
    static HWND dummyWindow = []() {
        WNDCLASSEX wc{ sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0, 0, GetModuleHandle(0), 0, 0, 0, 0, TEXT("DX"), 0 };
        RegisterClassEx(&wc);
        return CreateWindow(wc.lpszClassName, 0, 0, 0, 0, 1, 1, 0, 0, wc.hInstance, 0);
        }();

    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 1;
    desc.OutputWindow = dummyWindow;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    IDXGISwapChain* swapChain;
    ID3D11Device* device;
    D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0, 0, 0, 0, D3D11_SDK_VERSION, &desc, &swapChain, &device, 0, 0);

    if (device) device->Release();
    return swapChain;
}

ID3D12CommandQueue* DirectXHook::CreateDummyCommandQueue()
{
    ID3D12Device* device = nullptr;
    D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ID3D12CommandQueue* queue = nullptr;
    device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue));
    device->Release();
    
    return queue;
}

void DirectXHook::HookSwapChain(
    IDXGISwapChain* dummySwapChain,
    uintptr_t presentDetourFunction,
    uintptr_t resizeBuffersDetourFunction,
    uintptr_t* presentReturnAddress,
    uintptr_t* resizeBuffersReturnAddress)
{
    const int vmtPresentOffset = 8;
    const int vmtResizeBuffersOffset = 13;
    const size_t numBytes = sizeof(size_t);

    uintptr_t vmtBaseAddress = (*(uintptr_t*)dummySwapChain);
    uintptr_t vmtPresentIndex = (vmtBaseAddress + (numBytes * vmtPresentOffset));
    uintptr_t vmtResizeBuffersIndex = (vmtBaseAddress + (numBytes * vmtResizeBuffersOffset));

    MemoryUtils::ToggleMemoryProtection(false, vmtPresentIndex, numBytes);
    MemoryUtils::ToggleMemoryProtection(false, vmtResizeBuffersIndex, numBytes);

    uintptr_t presentAddress = (*(uintptr_t*)vmtPresentIndex);
    uintptr_t resizeBuffersAddress = (*(uintptr_t*)vmtResizeBuffersIndex);

    MemoryUtils::PlaceHook(presentAddress, presentDetourFunction, presentReturnAddress);
    MemoryUtils::PlaceHook(resizeBuffersAddress, resizeBuffersDetourFunction, resizeBuffersReturnAddress);

    MemoryUtils::ToggleMemoryProtection(true, vmtPresentIndex, numBytes);
    MemoryUtils::ToggleMemoryProtection(true, vmtResizeBuffersIndex, numBytes);

    dummySwapChain->Release();
}

void DirectXHook::HookCommandQueue(
    ID3D12CommandQueue* dummyCommandQueue,
    uintptr_t executeCommandListsDetourFunction,
    uintptr_t* executeCommandListsReturnAddress)
{
    if (!dummyCommandQueue) return;

    uintptr_t* vTable = *(uintptr_t**)dummyCommandQueue;
    size_t executeOffset = 10;

    uintptr_t executeAddr = vTable[executeOffset];
    executeCommandListsAddress = executeAddr;

    MemoryUtils::ToggleMemoryProtection(false, executeAddr, sizeof(void*));
    MemoryUtils::PlaceHook(executeAddr, executeCommandListsDetourFunction, executeCommandListsReturnAddress);
    MemoryUtils::ToggleMemoryProtection(true, executeAddr, sizeof(void*));

    dummyCommandQueue->Release();
}

void DirectXHook::UnhookCommandQueue() const
{
    MemoryUtils::Unhook(executeCommandListsAddress);
}