#pragma once
#include <cstdint>
#include <cstring>
using UINT = unsigned int;
using HRESULT = long;
using ULONGLONG = unsigned long long;
using D3DFORMAT = int;
constexpr int D3DFMT_UNKNOWN=0, D3DMULTISAMPLE_NONE=0, D3DBACKBUFFER_TYPE_MONO=0, D3DTEXF_NONE=0;
constexpr bool FALSE=false;
#define SUCCEEDED(hr) ((hr) >= 0)
#define FAILED(hr) ((hr) < 0)
inline ULONGLONG fakeMs = 1;
inline ULONGLONG GetTickCount64() { return fakeMs; }
struct D3DSURFACE_DESC { UINT Width=100, Height=100; D3DFORMAT Format=21; };
struct IDirect3DSurface9 {
    D3DSURFACE_DESC desc;
    bool owned=false, initialized=false;
    int pixels=0;
    HRESULT GetDesc(D3DSURFACE_DESC* d) { *d=desc; return 0; }
    void Release() { if (owned) delete this; }
};
struct IDirect3DDevice9 {
    IDirect3DSurface9 bb;
    bool failCreate=false, failGet=false, failSnap=false, failBlit=false;
    unsigned uninitializedReads=0, creates=0, snaps=0, blits=0;
    HRESULT GetBackBuffer(int,int,int,IDirect3DSurface9** out) {
        if (failGet) { *out=nullptr; return -1; }
        *out=&bb; return 0;
    }
    HRESULT CreateRenderTarget(UINT w,UINT h,D3DFORMAT f,int,int,bool,IDirect3DSurface9** out,void*) {
        if (failCreate) { *out=nullptr; return -1; }
        *out=new IDirect3DSurface9; (*out)->owned=true; (*out)->desc={w,h,f}; ++creates; return 0;
    }
    HRESULT StretchRect(IDirect3DSurface9* src,void*,IDirect3DSurface9* dst,void*,int) {
        bool snap=src==&bb;
        if ((snap && failSnap) || (!snap && failBlit)) return -1;
        if (!src->initialized) ++uninitializedReads;
        dst->pixels=src->pixels; dst->initialized=src->initialized;
        if (snap) ++snaps; else ++blits;
        return 0;
    }
};
