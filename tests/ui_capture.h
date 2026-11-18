#pragma once
#include "imgui_impl_dx11.h"
#include "firmware_bitmap.h"
#include <wrl/client.h>
#include <cstring>

// Optional visual proof of real ImGui draw data through the software D3D device.
inline bool CaptureUiDrawData(ID3D11Device* device, ID3D11DeviceContext* gpu,
    const char* path, std::string& error) {
    using Microsoft::WRL::ComPtr;
    const auto size = ImGui::GetIO().DisplaySize;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = UINT(size.x); desc.Height = UINT(size.y);
    desc.MipLevels = desc.ArraySize = 1; desc.SampleDesc.Count = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> target, staging;
    ComPtr<ID3D11RenderTargetView> view;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &target)) ||
        FAILED(device->CreateRenderTargetView(target.Get(), nullptr, &view))) {
        error = "UI capture render target failed"; return false;
    }
    auto* renderTarget = view.Get();
    gpu->OMSetRenderTargets(1, &renderTarget, nullptr);
    const float clear[] = {.06f, .06f, .07f, 1};
    gpu->ClearRenderTargetView(renderTarget, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging))) {
        error = "UI capture staging texture failed"; return false;
    }
    gpu->CopyResource(staging.Get(), target.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(gpu->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
        error = "UI capture readback failed"; return false;
    }
    std::vector<uint32_t> rgba(size_t(desc.Width) * desc.Height);
    for (UINT y = 0; y < desc.Height; ++y)
        std::memcpy(rgba.data() + size_t(y) * desc.Width,
            static_cast<const uint8_t*>(mapped.pData) + size_t(y) * mapped.RowPitch, size_t(desc.Width) * 4);
    gpu->Unmap(staging.Get(), 0);
    gpu->OMSetRenderTargets(0, nullptr, nullptr);
    return FirmwareBitmap::SavePngRgb565(path, rgba.data(), desc.Width, desc.Height, error);
}
