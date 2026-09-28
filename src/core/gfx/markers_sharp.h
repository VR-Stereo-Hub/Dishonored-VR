#pragma once
#include <d3d9.h>
#include <d3d11.h>
#include <cstdint>
namespace dvr::markersharp {
bool enabled();
void set_enabled(bool on,const char* owner);
void prepare(IDirect3DDevice9*,ID3D11Device*,ID3D11DeviceContext*,uint32_t,uint32_t);
bool begin(IDirect3DDevice9*,IDirect3DSurface9* gameRt,const D3DVIEWPORT9&);
void end(IDirect3DDevice9*,IDirect3DSurface9* gameRt,const D3DVIEWPORT9&);
void seal(uint32_t serial,int eye);
void composite(ID3D11DeviceContext*,ID3D11RenderTargetView*,uint32_t,uint32_t,uint32_t serial,int eye);
void reset();
}
