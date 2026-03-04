#pragma once

#include <d3d11.h>
#include <d3d12.h>
#include <Windows.h>

#include "Render/ID3DRenderer.h"
#include "Logger.h"
#include "imgui/backends/imgui_impl_win32.h"

using Present = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffers = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using ExecuteCommandLists = void(__stdcall*)(ID3D12CommandQueue*, UINT, const ID3D12CommandList**);

// Hooks DirectX 11 and DirectX 12
class DirectXHook
{
public:
    ID3DRenderer* renderer;
    uintptr_t executeCommandListsAddress = 0;
    uintptr_t presentReturnAddress = 0;
    uintptr_t resizeBuffersReturnAddress = 0;
    uintptr_t executeCommandListsReturnAddress = 0;

    DirectXHook(ID3DRenderer* renderer);
    void Hook();
    ID3D12CommandQueue* CreateDummyCommandQueue();
    void HookCommandQueue(ID3D12CommandQueue* dummyCommandQueue, uintptr_t executeCommandListsDetourFunction, uintptr_t* executeCommandListsReturnAddress);
    void UnhookCommandQueue() const;

private:
    Logger logger{ "DirectXHook" };

    IDXGISwapChain* CreateDummySwapChain();
    void HookSwapChain(IDXGISwapChain* dummySwapChain, uintptr_t presentDetourFunction, uintptr_t resizeBuffersDetourFunction, uintptr_t* presentReturnAddress, uintptr_t* resizeBuffersReturnAddress);
};