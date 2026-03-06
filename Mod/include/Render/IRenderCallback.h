#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <utility>

class IRenderCallback
{
public:
    virtual void Setup() {};
    virtual void Render() = 0;
    
    void Init(
        Microsoft::WRL::ComPtr<ID3D11Device> newDevice,
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> newContext,
        HWND newWindow) noexcept
    {
        device = std::move(newDevice);
        context = std::move(newContext);
        window = newWindow;
    }
    
    bool IsInitialized() const noexcept {
        return device && context && window;
    }

protected:
    Microsoft::WRL::ComPtr<ID3D11Device> device = nullptr;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context = nullptr;
    HWND window = nullptr;
};