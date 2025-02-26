#pragma once

#include <d3d11.h>
#include <wrl/client.h>

class IRenderCallback
{
public:
    virtual void Setup() {};
    virtual void Render() = 0;
    void Init(
        Microsoft::WRL::ComPtr<ID3D11Device> device,
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
        HWND window)
    {
        this->device = device;
        this->context = context;
        this->window = window;
    }

protected:
    Microsoft::WRL::ComPtr<ID3D11Device> device = nullptr;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context = nullptr;
    HWND window = NULL;
};