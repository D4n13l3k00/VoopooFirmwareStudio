#pragma once

#include <d3d11.h>
#include <cstdint>
#include <vector>

class Dx11Texture {
public:
    Dx11Texture() = default;
    ~Dx11Texture() { Release(); }

    Dx11Texture(const Dx11Texture&) = delete;
    Dx11Texture& operator=(const Dx11Texture&) = delete;

    void Release() {
        if (m_srv) { m_srv->Release(); m_srv = nullptr; }
        if (m_tex) { m_tex->Release(); m_tex = nullptr; }
        m_width = 0;
        m_height = 0;
    }

    bool Update(ID3D11Device* device, ID3D11DeviceContext* context, const uint32_t* rgba, uint32_t width, uint32_t height) {
        if (!device || !context || !rgba || width == 0 || height == 0) {
            Release();
            return false;
        }

        if (m_tex && m_width == width && m_height == height) {
            context->UpdateSubresource(m_tex, 0, nullptr, rgba, width * 4, 0);
            return true;
        }

        Release();

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = rgba;
        initData.SysMemPitch = width * 4;

        HRESULT hr = device->CreateTexture2D(&desc, &initData, &m_tex);
        if (FAILED(hr)) return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        hr = device->CreateShaderResourceView(m_tex, &srvDesc, &m_srv);
        if (FAILED(hr)) {
            Release();
            return false;
        }

        m_width = width;
        m_height = height;
        return true;
    }

    ID3D11ShaderResourceView* GetSRV() const { return m_srv; }
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    bool IsValid() const { return m_srv != nullptr; }

private:
    ID3D11Texture2D* m_tex = nullptr;
    ID3D11ShaderResourceView* m_srv = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

