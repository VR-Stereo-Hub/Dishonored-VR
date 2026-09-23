// tools/installer/sys/resources.cpp - see resources.h.
#include "sys/resources.h"
#include <windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <vector>

namespace dvr::setup::resources {

Blob rcdata(int id)
{
    Blob b;
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(id), (LPCWSTR)RT_RCDATA);
    if (!r) return b;
    HGLOBAL g = LoadResource(nullptr, r);
    if (!g) return b;
    b.data = (const uint8_t*)LockResource(g);
    b.size = SizeofResource(nullptr, r);
    return b;
}

ID3D11ShaderResourceView* image(ID3D11Device* device, int id, unsigned* width, unsigned* height)
{
    using Microsoft::WRL::ComPtr;
    const Blob blob = rcdata(id);
    if (!device || !blob.ok() || blob.size > MAXDWORD) return nullptr;
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                               IID_PPV_ARGS(&factory)))) return nullptr;
    ComPtr<IWICStream> stream;
    if (FAILED(factory->CreateStream(&stream)) ||
        FAILED(stream->InitializeFromMemory((BYTE*)blob.data, (DWORD)blob.size))) return nullptr;
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder))) return nullptr;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(decoder->GetFrame(0, &frame)) || FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
                                     nullptr, 0, WICBitmapPaletteTypeCustom))) return nullptr;
    UINT w = 0, h = 0;
    if (FAILED(converter->GetSize(&w, &h)) || !w || !h || w > 8192 || h > 8192) return nullptr;
    std::vector<BYTE> pixels((size_t)w * h * 4);
    if (FAILED(converter->CopyPixels(nullptr, w * 4, (UINT)pixels.size(), pixels.data()))) return nullptr;
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width=w; desc.Height=h; desc.MipLevels=1; desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_IMMUTABLE; desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data = { pixels.data(), w * 4, 0 };
    ComPtr<ID3D11Texture2D> texture;
    ID3D11ShaderResourceView* view = nullptr;
    if (FAILED(device->CreateTexture2D(&desc, &data, &texture)) ||
        FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, &view))) return nullptr;
    *width=w; *height=h;
    return view;
}

} // namespace dvr::setup::resources
