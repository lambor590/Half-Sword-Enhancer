#include "Hooks/DirectXHook.h"
#include "GlobalDefinitions.h"
#include "MemoryUtils.h"

namespace DXHookConstants {
    constexpr int VMT_PRESENT_OFFSET = 8;
    constexpr int VMT_RESIZE_BUFFERS_OFFSET = 13;
    constexpr int VMT_EXECUTE_COMMAND_LISTS_OFFSET = 10;
    constexpr size_t PTR_SIZE = sizeof(size_t);

    constexpr size_t VMT_PRESENT_BYTE_OFFSET = PTR_SIZE * VMT_PRESENT_OFFSET;
    constexpr size_t VMT_RESIZE_BUFFERS_BYTE_OFFSET = PTR_SIZE * VMT_RESIZE_BUFFERS_OFFSET;
}

__forceinline static HRESULT __fastcall OnPresent(IDXGISwapChain* pThis, UINT syncInterval, UINT flags) noexcept
{
    g_DirectXHook->renderer->OnPresent(pThis, syncInterval, flags);
    return ((Present)g_DirectXHook->presentReturnAddress)(pThis, syncInterval, flags);
}

__forceinline static HRESULT __fastcall OnResizeBuffers(IDXGISwapChain* pThis, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags) noexcept
{
    g_DirectXHook->renderer->OnResizeBuffers(pThis, bufferCount, width, height, newFormat, swapChainFlags);
    return ((ResizeBuffers)g_DirectXHook->resizeBuffersReturnAddress)(pThis, bufferCount, width, height, newFormat, swapChainFlags);
}

__forceinline static void __fastcall OnExecuteCommandLists(ID3D12CommandQueue* pThis, UINT numCommandLists, const ID3D12CommandList** ppCommandLists) noexcept
{
    if (pThis->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) [[likely]]
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
    if (!dummySwapChain) {
        logger.Log("Failed to create dummy swap chain, hooking aborted");
        return;
    }
    HookSwapChain(dummySwapChain, (uintptr_t)&OnPresent, (uintptr_t)&OnResizeBuffers, &presentReturnAddress, &resizeBuffersReturnAddress);
}

IDXGISwapChain* DirectXHook::CreateDummySwapChain()
{
    static HWND dummyWindow = []() {
        WNDCLASSEX wc{ sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0, 0, GetModuleHandle(0), 0, 0, 0, 0, TEXT("DX"), 0 };
        RegisterClassEx(&wc);
        return CreateWindowEx(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            wc.lpszClassName, nullptr, 0, 0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
        }();

    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.OutputWindow = dummyWindow;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    IDXGISwapChain* swapChain = nullptr;
    ID3D11Device* device = nullptr;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0, 0, 0, 0, D3D11_SDK_VERSION, &desc, &swapChain, &device, 0, 0);

    if (FAILED(hr) || !swapChain) {
        logger.Log("D3D11CreateDeviceAndSwapChain failed: 0x%X", hr);
        if (device) device->Release();
        return nullptr;
    }

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
    using namespace DXHookConstants;

    uintptr_t vmtBaseAddress = (*(uintptr_t*)dummySwapChain);
    uintptr_t vmtPresentIndex = vmtBaseAddress + VMT_PRESENT_BYTE_OFFSET;
    uintptr_t vmtResizeBuffersIndex = vmtBaseAddress + VMT_RESIZE_BUFFERS_BYTE_OFFSET;

    uintptr_t presentAddress = (*(uintptr_t*)vmtPresentIndex);
    uintptr_t resizeBuffersAddress = (*(uintptr_t*)vmtResizeBuffersIndex);

    MemoryUtils::PlaceHook(presentAddress, presentDetourFunction, presentReturnAddress);
    MemoryUtils::PlaceHook(resizeBuffersAddress, resizeBuffersDetourFunction, resizeBuffersReturnAddress);

    dummySwapChain->Release();
}

void DirectXHook::HookCommandQueue(
    ID3D12CommandQueue* dummyCommandQueue,
    uintptr_t executeCommandListsDetourFunction,
    uintptr_t* executeCommandListsReturnAddress)
{
    if (!dummyCommandQueue) return;

    uintptr_t* vTable = *(uintptr_t**)dummyCommandQueue;
    constexpr size_t executeOffset = DXHookConstants::VMT_EXECUTE_COMMAND_LISTS_OFFSET;

    uintptr_t executeAddr = vTable[executeOffset];
    executeCommandListsAddress = executeAddr;

    MemoryUtils::PlaceHook(executeAddr, executeCommandListsDetourFunction, executeCommandListsReturnAddress);

    dummyCommandQueue->Release();
}

void DirectXHook::UnhookCommandQueue() const
{
    MemoryUtils::Unhook(executeCommandListsAddress);
}