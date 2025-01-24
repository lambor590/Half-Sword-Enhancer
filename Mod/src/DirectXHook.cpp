#include "DirectXHook.h"

static DirectXHook* hookInstance = nullptr;

static HRESULT __stdcall OnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags)
{
    hookInstance->renderer->OnPresent(pThis, syncInterval, flags);
    return ((Present)hookInstance->presentReturnAddress)(pThis, syncInterval, flags);
}

static HRESULT __stdcall OnResizeBuffers(IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags)
{
    hookInstance->renderer->OnResizeBuffers(pThis, bufferCount, width, height, newFormat, swapChainFlags);
    return ((ResizeBuffers)hookInstance->resizeBuffersReturnAddress)(pThis, bufferCount, width, height, newFormat, swapChainFlags);
}

static void __stdcall OnExecuteCommandLists(ID3D12CommandQueue* pThis, UINT numCommandLists, const ID3D12CommandList** ppCommandLists)
{
    if (pThis->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
    {
        hookInstance->renderer->SetCommandQueue(pThis);
        //hookInstance->UnhookCommandQueue();
    }

    ((ExecuteCommandLists)hookInstance->executeCommandListsReturnAddress)(pThis, numCommandLists, ppCommandLists);
}

static void GetCommandQueue()
{
    ID3D12CommandQueue* dummyCommandQueue = hookInstance->CreateDummyCommandQueue();
    hookInstance->HookCommandQueue(dummyCommandQueue, (uintptr_t)&OnExecuteCommandLists, &hookInstance->executeCommandListsReturnAddress);
}

DirectXHook::DirectXHook(ID3DRenderer* renderer)
{
    this->renderer = renderer;
    hookInstance = this;
}

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
    WNDCLASSEX wc{ 0 };
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProc;
    wc.lpszClassName = TEXT("dummy class");
    RegisterClassExA(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, TEXT(""), WS_DISABLED, 0, 0, 0, 0, NULL, NULL, NULL, nullptr);

    DXGI_SWAP_CHAIN_DESC desc{ 0 };
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 1;
    desc.OutputWindow = hwnd;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_11_1
    };

    ID3D11Device* dummyDevice = nullptr;
    IDXGISwapChain* dummySwapChain = nullptr;
    HRESULT result = D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        0,
        featureLevels,
        1,
        D3D11_SDK_VERSION,
        &desc,
        &dummySwapChain,
        &dummyDevice,
        NULL,
        NULL);

    DestroyWindow(desc.OutputWindow);
    UnregisterClass(wc.lpszClassName, GetModuleHandle(nullptr));
    // dummySwapChain->Release();
    // dummyDevice->Release();

    if (FAILED(result))
    {
        _com_error error(result);
        logger.Log("D3D11CreateDeviceAndSwapChain failed: %s", error.ErrorMessage());
        return nullptr;
    }

    logger.Log("D3D11CreateDeviceAndSwapChain succeeded");
    return dummySwapChain;
}

ID3D12CommandQueue* DirectXHook::CreateDummyCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ID3D12Device* d12Device = nullptr;
    D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), (void**)&d12Device);

    ID3D12CommandQueue* dummyCommandQueue;
    d12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&dummyCommandQueue));

    logger.Log("Command queue: %p", dummyCommandQueue);

    return dummyCommandQueue;
}

void DirectXHook::HookSwapChain(
    IDXGISwapChain* dummySwapChain,
    uintptr_t presentDetourFunction,
    uintptr_t resizeBuffersDetourFunction,
    uintptr_t* presentReturnAddress,
    uintptr_t* resizeBuffersReturnAddress)
{
    int vmtPresentOffset = 8;
    int vmtResizeBuffersOffset = 13;
    size_t numBytes = sizeof(size_t);

    uintptr_t vmtBaseAddress = (*(uintptr_t*)dummySwapChain);
    uintptr_t vmtPresentIndex = (vmtBaseAddress + (numBytes * vmtPresentOffset));
    uintptr_t vmtResizeBuffersIndex = (vmtBaseAddress + (numBytes * vmtResizeBuffersOffset));

    logger.Log("SwapChain VMT base address: %p", vmtBaseAddress);
    logger.Log("SwapChain VMT Present index: %p", vmtPresentIndex);
    logger.Log("SwapChain VMT ResizeBuffers index: %p", vmtResizeBuffersIndex);

    MemoryUtils::ToggleMemoryProtection(false, vmtPresentIndex, numBytes);
    MemoryUtils::ToggleMemoryProtection(false, vmtResizeBuffersIndex, numBytes);

    uintptr_t presentAddress = (*(uintptr_t*)vmtPresentIndex);
    uintptr_t resizeBuffersAddress = (*(uintptr_t*)vmtResizeBuffersIndex);

    logger.Log("Present address: %p", presentAddress);
    logger.Log("ResizeBuffers address: %p", resizeBuffersAddress);

    MemoryUtils::ToggleMemoryProtection(true, vmtPresentIndex, numBytes);
    MemoryUtils::ToggleMemoryProtection(true, vmtResizeBuffersIndex, numBytes);

    MemoryUtils::PlaceHook(presentAddress, presentDetourFunction, presentReturnAddress);
    MemoryUtils::PlaceHook(resizeBuffersAddress, resizeBuffersDetourFunction, resizeBuffersReturnAddress);

    dummySwapChain->Release();
}

void DirectXHook::HookCommandQueue(
    ID3D12CommandQueue* dummyCommandQueue,
    uintptr_t executeCommandListsDetourFunction,
    uintptr_t* executeCommandListsReturnAddress)
{
    int vmtExecuteCommandListsOffset = 10;
    size_t numBytes = 8;

    uintptr_t vmtBaseAddress = (*(uintptr_t*)dummyCommandQueue);
    uintptr_t vmtExecuteCommandListsIndex = (vmtBaseAddress + (numBytes * vmtExecuteCommandListsOffset));

    logger.Log("CommandQueue VMT base address: %p", vmtBaseAddress);
    logger.Log("ExecuteCommandLists index: %p", vmtExecuteCommandListsIndex);

    MemoryUtils::ToggleMemoryProtection(false, vmtExecuteCommandListsIndex, numBytes);
    executeCommandListsAddress = (*(uintptr_t*)vmtExecuteCommandListsIndex);
    MemoryUtils::ToggleMemoryProtection(true, vmtExecuteCommandListsIndex, numBytes);

    logger.Log("ExecuteCommandLists address: %p", executeCommandListsAddress);

    bool hookIsPresent = MemoryUtils::IsAddressHooked(executeCommandListsAddress);
    if (hookIsPresent)
    {
        logger.Log("Hook already present in ExecuteCommandLists");
    }

    MemoryUtils::PlaceHook(executeCommandListsAddress, executeCommandListsDetourFunction, executeCommandListsReturnAddress);
    dummyCommandQueue->Release();
}

void DirectXHook::UnhookCommandQueue() const
{
    MemoryUtils::Unhook(executeCommandListsAddress);
}