#pragma once
#include <cstdint>
#include <cstring>
using UINT = unsigned int;
using HRESULT = long;
using ULONGLONG = unsigned long long;
using D3DFORMAT = int;
using HWND = void*;
struct RECT { long left=0, top=0, right=0, bottom=0; };
struct RGNDATA {};
constexpr HRESULT D3D_OK=0, E_FAIL=-1;
constexpr int D3DSWAPEFFECT_DISCARD=1;
constexpr UINT D3DPRESENT_INTERVAL_IMMEDIATE=0x80000000u;
constexpr HRESULT S_FALSE=1;
constexpr int D3DQUERYTYPE_EVENT=1, D3DISSUE_END=1, D3DGETDATA_FLUSH=1;
struct IDirect3DQuery9 {
    bool failIssue=false, failGet=false;
    unsigned issues=0, flushes=0;
    HRESULT result=D3D_OK;
    HRESULT Issue(int) { ++issues; return failIssue ? E_FAIL : D3D_OK; }
    HRESULT GetData(void*,int,int flags) { if(flags==D3DGETDATA_FLUSH) ++flushes; return failGet ? E_FAIL : result; }
    void Release() { delete this; }
};
struct D3DPRESENT_PARAMETERS {
    bool Windowed=true;
    int SwapEffect=D3DSWAPEFFECT_DISCARD;
    UINT BackBufferCount=1;
    int MultiSampleType=0;
    UINT PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
};
struct IDirect3DSwapChain9 {
    D3DPRESENT_PARAMETERS pp;
    bool fail=false;
    HRESULT GetPresentParameters(D3DPRESENT_PARAMETERS* out) { *out=pp; return fail ? E_FAIL : D3D_OK; }
    void Release() {}
};
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
    IDirect3DSwapChain9 swap;
    bool failSwap=false;
    unsigned presents=0, captures=0;
    int frontPixels=0;
    HRESULT nextPresent=0;
    bool failQuery=false;
    IDirect3DQuery9* lastQuery=nullptr;
    HRESULT CreateQuery(int,IDirect3DQuery9** out) {
        *out=failQuery ? nullptr : new IDirect3DQuery9; lastQuery=*out;
        return failQuery ? E_FAIL : D3D_OK;
    }
    HRESULT GetSwapChain(int, IDirect3DSwapChain9** out) {
        *out=failSwap ? nullptr : &swap; return failSwap ? E_FAIL : D3D_OK;
    }
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
