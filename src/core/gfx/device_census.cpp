// core/gfx/device_census.cpp - see device_census.h.
#define DVR_CAT ::dvr::log::Cat::device
#include "core/gfx/device_census.h"

#include "core/framework/status.h"
#include "core/hooks/vtable.h"
#include "core/util/log.h"

#include <stdio.h>
#include <string.h>

namespace dvr::census {
namespace {

// ---- the classification --------------------------------------------------------
enum Call { kTexture = 0, kVolume, kCube, kVertexBuffer, kIndexBuffer, kRenderTarget, kDepthStencil,
            kOffscreenPlain, kCallCount };
const char* const kCallNames[kCallCount] = {"CreateTexture", "CreateVolumeTexture", "CreateCubeTexture",
                                            "CreateVertexBuffer", "CreateIndexBuffer", "CreateRenderTarget",
                                            "CreateDepthStencilSurface", "CreateOffscreenPlainSurface"};
const char* const kPoolNames[4] = {"DEFAULT", "MANAGED", "SYSTEMMEM", "SCRATCH"};
// usage class bits
enum { kUsageRt = 1, kUsageDs = 2, kUsageDynamic = 4, kUsageWriteOnly = 8, kUsageAutoMip = 16, kUsageOther = 32 };
enum Fmt { kFmtDxt1 = 0, kFmtDxt35, kFmt32, kFmt16, kFmt8, kFmtFloat, kFmtDepth, kFmtFourcc, kFmtBuffer,
           kFmtOther, kFmtCount };
const char* const kFmtNames[kFmtCount] = {"DXT1", "DXT3/5", "32bpp", "16bpp", "8bpp", "float", "depth",
                                          "fourcc", "buffer", "other"};

inline uint32_t usage_class(DWORD usage) {
    uint32_t c = 0;
    if (usage & D3DUSAGE_RENDERTARGET) c |= kUsageRt;
    if (usage & D3DUSAGE_DEPTHSTENCIL) c |= kUsageDs;
    if (usage & D3DUSAGE_DYNAMIC) c |= kUsageDynamic;
    if (usage & D3DUSAGE_WRITEONLY) c |= kUsageWriteOnly;
    if (usage & D3DUSAGE_AUTOGENMIPMAP) c |= kUsageAutoMip;
    if (usage & ~(D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL | D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY |
                  D3DUSAGE_AUTOGENMIPMAP)) c |= kUsageOther;
    return c;
}

#ifndef MAKEFOURCC
#define MAKEFOURCC(a, b, c, d) ((DWORD)(BYTE)(a) | ((DWORD)(BYTE)(b) << 8) | ((DWORD)(BYTE)(c) << 16) | ((DWORD)(BYTE)(d) << 24))
#endif

// Bytes per pixel (or per 4x4 block for the compressed classes), and the class.
Fmt fmt_class(D3DFORMAT f, uint32_t* bytesPerUnit, bool* blockCompressed) {
    *blockCompressed = false;
    *bytesPerUnit = 4;
    switch (f) {
    case D3DFMT_DXT1: *blockCompressed = true; *bytesPerUnit = 8; return kFmtDxt1;
    case D3DFMT_DXT2: case D3DFMT_DXT3: case D3DFMT_DXT4: case D3DFMT_DXT5:
        *blockCompressed = true; *bytesPerUnit = 16; return kFmtDxt35;
    case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: case D3DFMT_A8B8G8R8: case D3DFMT_X8B8G8R8:
    case D3DFMT_A2R10G10B10: case D3DFMT_A2B10G10R10: case D3DFMT_G16R16: case D3DFMT_Q8W8V8U8:
    case D3DFMT_V16U16: case D3DFMT_X8L8V8U8: case D3DFMT_A2W10V10U10:
        *bytesPerUnit = 4; return kFmt32;
    case D3DFMT_R5G6B5: case D3DFMT_X1R5G5B5: case D3DFMT_A1R5G5B5: case D3DFMT_A4R4G4B4:
    case D3DFMT_X4R4G4B4: case D3DFMT_A8R3G3B2: case D3DFMT_L16: case D3DFMT_A8L8: case D3DFMT_V8U8:
    case D3DFMT_L6V5U5: case D3DFMT_CxV8U8:
        *bytesPerUnit = 2; return kFmt16;
    case D3DFMT_L8: case D3DFMT_A8: case D3DFMT_R3G3B2: case D3DFMT_A4L4: case D3DFMT_P8:
        *bytesPerUnit = 1; return kFmt8;
    case D3DFMT_R16F: *bytesPerUnit = 2; return kFmtFloat;
    case D3DFMT_G16R16F: case D3DFMT_R32F: *bytesPerUnit = 4; return kFmtFloat;
    case D3DFMT_A16B16G16R16F: case D3DFMT_G32R32F: case D3DFMT_A16B16G16R16: case D3DFMT_Q16W16V16U16:
        *bytesPerUnit = 8; return kFmtFloat;
    case D3DFMT_A32B32G32R32F: *bytesPerUnit = 16; return kFmtFloat;
    case D3DFMT_D16: case D3DFMT_D16_LOCKABLE: case D3DFMT_D15S1: *bytesPerUnit = 2; return kFmtDepth;
    case D3DFMT_D24S8: case D3DFMT_D24X8: case D3DFMT_D24X4S4: case D3DFMT_D32: case D3DFMT_D32F_LOCKABLE:
    case D3DFMT_D24FS8: case D3DFMT_D32_LOCKABLE: case D3DFMT_S8_LOCKABLE:
        *bytesPerUnit = 4; return kFmtDepth;
    default: break;
    }
    const DWORD v = (DWORD)f;
    if (v == MAKEFOURCC('I', 'N', 'T', 'Z') || v == MAKEFOURCC('D', 'F', '2', '4') || v == MAKEFOURCC('D', 'F', '1', '6') ||
        v == MAKEFOURCC('R', 'A', 'W', 'Z') || v == MAKEFOURCC('N', 'U', 'L', 'L') || v == MAKEFOURCC('R', 'E', 'S', 'Z')) {
        *bytesPerUnit = 4; return kFmtDepth;
    }
    if (v > 0xffff) { *bytesPerUnit = 4; return kFmtFourcc; }
    return kFmtOther;
}

uint64_t texture_bytes(UINT w, UINT h, UINT depth, UINT levels, D3DFORMAT f, UINT faces) {
    uint32_t bpu; bool bc;
    fmt_class(f, &bpu, &bc);
    if (levels == 0) {   // the full chain
        levels = 1;
        UINT m = w > h ? w : h;
        while (m > 1) { m >>= 1; ++levels; }
    }
    uint64_t total = 0;
    UINT lw = w, lh = h, ld = depth ? depth : 1;
    for (UINT i = 0; i < levels; ++i) {
        const uint64_t px = bc ? (uint64_t)((lw + 3) / 4) * ((lh + 3) / 4) : (uint64_t)lw * lh;
        total += px * bpu * ld;
        if (lw > 1) lw >>= 1;
        if (lh > 1) lh >>= 1;
        if (ld > 1) ld >>= 1;
    }
    return total * (faces ? faces : 1);
}

// ---- the histogram -------------------------------------------------------------
struct Row {
    uint32_t key = 0;          // call | pool << 4 | usage << 8 | fmt << 16 ; 0 = empty
    uint32_t count = 0, failures = 0;
    uint64_t bytes = 0;
    uint32_t seenCount = 0;    // at the last delta line
    HRESULT  firstFail = S_OK;
    char     firstFailAsk[96] = "";
};
constexpr int kRows = 512;
Row g_rows[kRows];
int g_rowCount = 0;
CRITICAL_SECTION g_cs;
bool g_csInit = false;

inline uint32_t make_key(int call, int pool, uint32_t usage, int fmt) {
    return 1u | ((uint32_t)call << 1) | ((uint32_t)pool << 5) | (usage << 8) | ((uint32_t)fmt << 16);
}
inline int key_call(uint32_t k) { return (k >> 1) & 15; }
inline int key_pool(uint32_t k) { return (k >> 5) & 7; }
inline uint32_t key_usage(uint32_t k) { return (k >> 8) & 255; }
inline int key_fmt(uint32_t k) { return (k >> 16) & 15; }

Row* row_for(uint32_t key) {
    uint32_t h = key * 2654435761u;
    for (int i = 0; i < kRows; ++i) {
        Row& r = g_rows[(h + i) % kRows];
        if (r.key == key) return &r;
        if (r.key == 0) { r.key = key; ++g_rowCount; return &r; }
    }
    return nullptr;   // full (512 distinct shapes: never for a UE3 title)
}

// ---- the object -> pool map (for the lock census) -------------------------------
constexpr int kMap = 8192;
struct MapEnt { void* obj; uint8_t pool; uint8_t cls; };
MapEnt g_map[kMap];
uint32_t g_mapOverflow = 0;

void map_put(void* obj, int pool, int cls) {
    uint32_t h = (uint32_t)((uintptr_t)obj >> 4) * 2654435761u;
    for (int i = 0; i < 64; ++i) {
        MapEnt& e = g_map[(h + i) % kMap];
        if (e.obj == obj || e.obj == nullptr) { e.obj = obj; e.pool = (uint8_t)pool; e.cls = (uint8_t)cls; return; }
    }
    // no slot within the probe window: overwrite the first (a reused pointer
    // is re-inserted by its creation hook, so the map self-corrects)
    MapEnt& e = g_map[h % kMap];
    e.obj = obj; e.pool = (uint8_t)pool; e.cls = (uint8_t)cls;
    ++g_mapOverflow;
}
int map_pool(void* obj) {
    uint32_t h = (uint32_t)((uintptr_t)obj >> 4) * 2654435761u;
    for (int i = 0; i < 64; ++i) {
        const MapEnt& e = g_map[(h + i) % kMap];
        if (e.obj == obj) return e.pool;
        if (e.obj == nullptr) return -1;
    }
    return -1;
}

// ---- the lock census --------------------------------------------------------------
enum LockClass { kLcTexture = 0, kLcCube, kLcVolume, kLcVb, kLcIb, kLcCount };
const char* const kLockClassNames[kLcCount] = {"Texture", "CubeTexture", "VolumeTexture", "VertexBuffer",
                                               "IndexBuffer"};
enum LockFlag { kLfPlain = 0, kLfReadOnly, kLfDiscard, kLfNoOverwrite, kLfNoSysLock, kLfPartial, kLfLevel, kLfUntracked,
                kLfCount };
const char* const kLockFlagNames[kLfCount] = {"plain", "READONLY", "DISCARD", "NOOVERWRITE", "NOSYSLOCK", "partial",
                                              "level>0", "untracked-object"};
uint32_t g_locks[kLcCount][4][kLfCount];   // [class][pool][flag]
uint32_t g_locksSeen[kLcCount][4][kLfCount];
uint32_t g_dirtyRects = 0, g_dirtyRectsOnManaged = 0;

void lock_count(int cls, void* obj, DWORD flags, bool partial, UINT level) {
    const int pool = map_pool(obj);
    if (!g_csInit) return;
    EnterCriticalSection(&g_cs);
    if (pool < 0 || pool > 3) { ++g_locks[cls][0][kLfUntracked]; LeaveCriticalSection(&g_cs); return; }
    if (flags & D3DLOCK_READONLY) ++g_locks[cls][pool][kLfReadOnly];
    else if (flags & D3DLOCK_DISCARD) ++g_locks[cls][pool][kLfDiscard];
    else if (flags & D3DLOCK_NOOVERWRITE) ++g_locks[cls][pool][kLfNoOverwrite];
    else if (flags & D3DLOCK_NOSYSLOCK) ++g_locks[cls][pool][kLfNoSysLock];
    else ++g_locks[cls][pool][kLfPlain];
    if (partial) ++g_locks[cls][pool][kLfPartial];
    if (level > 0) ++g_locks[cls][pool][kLfLevel];
    LeaveCriticalSection(&g_cs);
}

typedef HRESULT (__stdcall *PFN_TexLockRect)(IDirect3DTexture9*, UINT, D3DLOCKED_RECT*, const RECT*, DWORD);
typedef HRESULT (__stdcall *PFN_TexAddDirtyRect)(IDirect3DTexture9*, const RECT*);
typedef HRESULT (__stdcall *PFN_CubeLockRect)(IDirect3DCubeTexture9*, D3DCUBEMAP_FACES, UINT, D3DLOCKED_RECT*, const RECT*, DWORD);
typedef HRESULT (__stdcall *PFN_VolLockBox)(IDirect3DVolumeTexture9*, UINT, D3DLOCKED_BOX*, const D3DBOX*, DWORD);
typedef HRESULT (__stdcall *PFN_VbLock)(IDirect3DVertexBuffer9*, UINT, UINT, void**, DWORD);
typedef HRESULT (__stdcall *PFN_IbLock)(IDirect3DIndexBuffer9*, UINT, UINT, void**, DWORD);
PFN_TexLockRect     g_origTexLock = nullptr;
PFN_TexAddDirtyRect g_origTexDirty = nullptr;
PFN_CubeLockRect    g_origCubeLock = nullptr;
PFN_VolLockBox      g_origVolLock = nullptr;
PFN_VbLock          g_origVbLock = nullptr;
PFN_IbLock          g_origIbLock = nullptr;

HRESULT __stdcall hkTexLockRect(IDirect3DTexture9* self, UINT level, D3DLOCKED_RECT* lr, const RECT* rc, DWORD flags) {
    lock_count(kLcTexture, self, flags, rc != nullptr, level);
    return g_origTexLock(self, level, lr, rc, flags);
}
HRESULT __stdcall hkTexAddDirtyRect(IDirect3DTexture9* self, const RECT* rc) {
    ++g_dirtyRects;
    if (map_pool(self) == 1) ++g_dirtyRectsOnManaged;
    return g_origTexDirty(self, rc);
}
HRESULT __stdcall hkCubeLockRect(IDirect3DCubeTexture9* self, D3DCUBEMAP_FACES face, UINT level, D3DLOCKED_RECT* lr,
                                 const RECT* rc, DWORD flags) {
    lock_count(kLcCube, self, flags, rc != nullptr, level);
    return g_origCubeLock(self, face, level, lr, rc, flags);
}
HRESULT __stdcall hkVolLockBox(IDirect3DVolumeTexture9* self, UINT level, D3DLOCKED_BOX* lb, const D3DBOX* box, DWORD flags) {
    lock_count(kLcVolume, self, flags, box != nullptr, level);
    return g_origVolLock(self, level, lb, box, flags);
}
HRESULT __stdcall hkVbLock(IDirect3DVertexBuffer9* self, UINT off, UINT size, void** data, DWORD flags) {
    lock_count(kLcVb, self, flags, off != 0 || size != 0, 0);
    return g_origVbLock(self, off, size, data, flags);
}
HRESULT __stdcall hkIbLock(IDirect3DIndexBuffer9* self, UINT off, UINT size, void** data, DWORD flags) {
    lock_count(kLcIb, self, flags, off != 0 || size != 0, 0);
    return g_origIbLock(self, off, size, data, flags);
}

// Patch a class's Lock once, on the first object of that class we see. The
// slot numbers are counted from the SDK header (IDirect3DTexture9 /
// CubeTexture9 LockRect 19, AddDirtyRect 21; VolumeTexture9 LockBox 19;
// VertexBuffer9 / IndexBuffer9 Lock 11).
void patch_lock_class(int cls, void* obj) {
    static bool done[kLcCount] = {};
    if (done[cls] || !obj) return;
    done[cls] = true;
    void* old = nullptr;
    switch (cls) {
    case kLcTexture:
        old = PatchVtable(obj, 19, (void*)hkTexLockRect); if (old) g_origTexLock = (PFN_TexLockRect)old;
        old = PatchVtable(obj, 21, (void*)hkTexAddDirtyRect); if (old) g_origTexDirty = (PFN_TexAddDirtyRect)old;
        break;
    case kLcCube:   old = PatchVtable(obj, 19, (void*)hkCubeLockRect); if (old) g_origCubeLock = (PFN_CubeLockRect)old; break;
    case kLcVolume: old = PatchVtable(obj, 19, (void*)hkVolLockBox); if (old) g_origVolLock = (PFN_VolLockBox)old; break;
    case kLcVb:     old = PatchVtable(obj, 11, (void*)hkVbLock); if (old) g_origVbLock = (PFN_VbLock)old; break;
    case kLcIb:     old = PatchVtable(obj, 11, (void*)hkIbLock); if (old) g_origIbLock = (PFN_IbLock)old; break;
    default: break;
    }
    DVR_INFO("device/census: %s lock hook %s", kLockClassNames[cls], old ? "installed" : "NOT installed (slot already patched or VirtualProtect refused)");
}

// ---- the creation hooks ---------------------------------------------------------
typedef HRESULT (__stdcall *PFN_CreateTexture)(IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);
typedef HRESULT (__stdcall *PFN_CreateVolumeTexture)(IDirect3DDevice9*, UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DVolumeTexture9**, HANDLE*);
typedef HRESULT (__stdcall *PFN_CreateCubeTexture)(IDirect3DDevice9*, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DCubeTexture9**, HANDLE*);
typedef HRESULT (__stdcall *PFN_CreateVertexBuffer)(IDirect3DDevice9*, UINT, DWORD, DWORD, D3DPOOL, IDirect3DVertexBuffer9**, HANDLE*);
typedef HRESULT (__stdcall *PFN_CreateIndexBuffer)(IDirect3DDevice9*, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DIndexBuffer9**, HANDLE*);
typedef HRESULT (__stdcall *PFN_CreateRenderTarget)(IDirect3DDevice9*, UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE, DWORD, BOOL, IDirect3DSurface9**, HANDLE*);
typedef HRESULT (__stdcall *PFN_CreateDepthStencil)(IDirect3DDevice9*, UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE, DWORD, BOOL, IDirect3DSurface9**, HANDLE*);
typedef HRESULT (__stdcall *PFN_CreateOffscreenPlain)(IDirect3DDevice9*, UINT, UINT, D3DFORMAT, D3DPOOL, IDirect3DSurface9**, HANDLE*);
PFN_CreateTexture        g_origCreateTexture = nullptr;
PFN_CreateVolumeTexture  g_origCreateVolume = nullptr;
PFN_CreateCubeTexture    g_origCreateCube = nullptr;
PFN_CreateVertexBuffer   g_origCreateVb = nullptr;
PFN_CreateIndexBuffer    g_origCreateIb = nullptr;
PFN_CreateRenderTarget   g_origCreateRt = nullptr;
PFN_CreateDepthStencil   g_origCreateDs = nullptr;
PFN_CreateOffscreenPlain g_origCreateOffscreen = nullptr;

uint32_t g_creations = 0, g_failures = 0, g_managed = 0, g_managedAutoMip = 0;
uint64_t g_bytes = 0, g_managedBytes = 0;
uint32_t g_managedByCall[kCallCount];

void record(int call, D3DPOOL pool, DWORD usage, D3DFORMAT fmt, uint64_t bytes, HRESULT hr, const char* ask) {
    if (!g_csInit) return;
    uint32_t bpu; bool bc;
    const int fc = (call == kVertexBuffer || call == kIndexBuffer) ? kFmtBuffer : fmt_class(fmt, &bpu, &bc);
    const int p = ((int)pool >= 0 && (int)pool <= 3) ? (int)pool : 0;
    EnterCriticalSection(&g_cs);
    Row* r = row_for(make_key(call, p, usage_class(usage), fc));
    if (r) {
        ++r->count; r->bytes += bytes;
        if (FAILED(hr)) { ++r->failures; if (r->firstFail == S_OK) { r->firstFail = hr; strcpy_s(r->firstFailAsk, sizeof(r->firstFailAsk), ask); } }
    }
    ++g_creations; g_bytes += bytes;
    if (FAILED(hr)) ++g_failures;
    if (pool == D3DPOOL_MANAGED) {
        ++g_managed; g_managedBytes += bytes; ++g_managedByCall[call];
        if (usage & D3DUSAGE_AUTOGENMIPMAP) ++g_managedAutoMip;
    }
    LeaveCriticalSection(&g_cs);
}

HRESULT __stdcall hkCreateTexture(IDirect3DDevice9* self, UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool,
                                  IDirect3DTexture9** out, HANDLE* shared) {
    const HRESULT hr = g_origCreateTexture(self, w, h, levels, usage, fmt, pool, out, shared);
    char ask[96]; _snprintf(ask, sizeof(ask), "%ux%u lv=%u usage=0x%lx fmt=%d pool=%d", w, h, levels, (unsigned long)usage, (int)fmt, (int)pool);
    record(kTexture, pool, usage, fmt, texture_bytes(w, h, 1, levels, fmt, 1), hr, ask);
    if (SUCCEEDED(hr) && out && *out) { map_put(*out, (int)pool, kLcTexture); patch_lock_class(kLcTexture, *out); }
    return hr;
}
HRESULT __stdcall hkCreateVolumeTexture(IDirect3DDevice9* self, UINT w, UINT h, UINT d, UINT levels, DWORD usage, D3DFORMAT fmt,
                                        D3DPOOL pool, IDirect3DVolumeTexture9** out, HANDLE* shared) {
    const HRESULT hr = g_origCreateVolume(self, w, h, d, levels, usage, fmt, pool, out, shared);
    char ask[96]; _snprintf(ask, sizeof(ask), "%ux%ux%u lv=%u usage=0x%lx fmt=%d pool=%d", w, h, d, levels, (unsigned long)usage, (int)fmt, (int)pool);
    record(kVolume, pool, usage, fmt, texture_bytes(w, h, d, levels, fmt, 1), hr, ask);
    if (SUCCEEDED(hr) && out && *out) { map_put(*out, (int)pool, kLcVolume); patch_lock_class(kLcVolume, *out); }
    return hr;
}
HRESULT __stdcall hkCreateCubeTexture(IDirect3DDevice9* self, UINT edge, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool,
                                      IDirect3DCubeTexture9** out, HANDLE* shared) {
    const HRESULT hr = g_origCreateCube(self, edge, levels, usage, fmt, pool, out, shared);
    char ask[96]; _snprintf(ask, sizeof(ask), "cube %u lv=%u usage=0x%lx fmt=%d pool=%d", edge, levels, (unsigned long)usage, (int)fmt, (int)pool);
    record(kCube, pool, usage, fmt, texture_bytes(edge, edge, 1, levels, fmt, 6), hr, ask);
    if (SUCCEEDED(hr) && out && *out) { map_put(*out, (int)pool, kLcCube); patch_lock_class(kLcCube, *out); }
    return hr;
}
HRESULT __stdcall hkCreateVertexBuffer(IDirect3DDevice9* self, UINT len, DWORD usage, DWORD fvf, D3DPOOL pool,
                                       IDirect3DVertexBuffer9** out, HANDLE* shared) {
    const HRESULT hr = g_origCreateVb(self, len, usage, fvf, pool, out, shared);
    char ask[96]; _snprintf(ask, sizeof(ask), "vb %u bytes usage=0x%lx pool=%d", len, (unsigned long)usage, (int)pool);
    record(kVertexBuffer, pool, usage, D3DFMT_UNKNOWN, len, hr, ask);
    if (SUCCEEDED(hr) && out && *out) { map_put(*out, (int)pool, kLcVb); patch_lock_class(kLcVb, *out); }
    return hr;
}
HRESULT __stdcall hkCreateIndexBuffer(IDirect3DDevice9* self, UINT len, DWORD usage, D3DFORMAT fmt, D3DPOOL pool,
                                      IDirect3DIndexBuffer9** out, HANDLE* shared) {
    const HRESULT hr = g_origCreateIb(self, len, usage, fmt, pool, out, shared);
    char ask[96]; _snprintf(ask, sizeof(ask), "ib %u bytes usage=0x%lx fmt=%d pool=%d", len, (unsigned long)usage, (int)fmt, (int)pool);
    record(kIndexBuffer, pool, usage, D3DFMT_UNKNOWN, len, hr, ask);
    if (SUCCEEDED(hr) && out && *out) { map_put(*out, (int)pool, kLcIb); patch_lock_class(kLcIb, *out); }
    return hr;
}
HRESULT __stdcall hkCreateRenderTarget(IDirect3DDevice9* self, UINT w, UINT h, D3DFORMAT fmt, D3DMULTISAMPLE_TYPE ms, DWORD q,
                                       BOOL lockable, IDirect3DSurface9** out, HANDLE* shared) {
    const HRESULT hr = g_origCreateRt(self, w, h, fmt, ms, q, lockable, out, shared);
    char ask[96]; _snprintf(ask, sizeof(ask), "rt %ux%u fmt=%d ms=%d lockable=%d shared=%d", w, h, (int)fmt, (int)ms, (int)lockable, shared ? 1 : 0);
    record(kRenderTarget, D3DPOOL_DEFAULT, D3DUSAGE_RENDERTARGET | (lockable ? D3DUSAGE_DYNAMIC : 0), fmt, texture_bytes(w, h, 1, 1, fmt, 1), hr, ask);
    return hr;
}
HRESULT __stdcall hkCreateDepthStencil(IDirect3DDevice9* self, UINT w, UINT h, D3DFORMAT fmt, D3DMULTISAMPLE_TYPE ms, DWORD q,
                                       BOOL discard, IDirect3DSurface9** out, HANDLE* shared) {
    const HRESULT hr = g_origCreateDs(self, w, h, fmt, ms, q, discard, out, shared);
    char ask[96]; _snprintf(ask, sizeof(ask), "ds %ux%u fmt=%d ms=%d discard=%d", w, h, (int)fmt, (int)ms, (int)discard);
    record(kDepthStencil, D3DPOOL_DEFAULT, D3DUSAGE_DEPTHSTENCIL, fmt, texture_bytes(w, h, 1, 1, fmt, 1), hr, ask);
    return hr;
}
HRESULT __stdcall hkCreateOffscreenPlain(IDirect3DDevice9* self, UINT w, UINT h, D3DFORMAT fmt, D3DPOOL pool,
                                         IDirect3DSurface9** out, HANDLE* shared) {
    const HRESULT hr = g_origCreateOffscreen(self, w, h, fmt, pool, out, shared);
    char ask[96]; _snprintf(ask, sizeof(ask), "offscreen %ux%u fmt=%d pool=%d", w, h, (int)fmt, (int)pool);
    record(kOffscreenPlain, pool, 0, fmt, texture_bytes(w, h, 1, 1, fmt, 1), hr, ask);
    return hr;
}

// ---- the text --------------------------------------------------------------------
int usage_text(char* buf, int cap, uint32_t u) {
    int n = 0;
    buf[0] = 0;
    if (u & kUsageRt) n += _snprintf(buf + n, cap - n, "RT|");
    if (u & kUsageDs) n += _snprintf(buf + n, cap - n, "DS|");
    if (u & kUsageDynamic) n += _snprintf(buf + n, cap - n, "DYNAMIC|");
    if (u & kUsageWriteOnly) n += _snprintf(buf + n, cap - n, "WRITEONLY|");
    if (u & kUsageAutoMip) n += _snprintf(buf + n, cap - n, "AUTOGENMIP|");
    if (u & kUsageOther) n += _snprintf(buf + n, cap - n, "other|");
    if (n > 0) buf[n - 1] = 0; else strcpy_s(buf, cap, "static");
    return n;
}

void verdict_line(char* buf, int cap) {
    // What 9Ex refuses: MANAGED in every pool-taking creation. The locks say
    // whether DEFAULT + DYNAMIC can stand in (plain write locks) or whether a
    // streaming READONLY lock needs the shadow route.
    uint32_t plain = 0, ro = 0, discard = 0, noover = 0, partial = 0, level = 0;
    for (int c = 0; c < kLcCount; ++c) {
        plain += g_locks[c][1][kLfPlain] + g_locks[c][1][kLfNoSysLock];
        ro += g_locks[c][1][kLfReadOnly]; discard += g_locks[c][1][kLfDiscard];
        noover += g_locks[c][1][kLfNoOverwrite]; partial += g_locks[c][1][kLfPartial]; level += g_locks[c][1][kLfLevel];
    }
    _snprintf(buf, cap,
              "device/census: 9Ex would refuse %u of %u creations (%.1f of %.1f MB): MANAGED tex=%u cube=%u vol=%u vb=%u "
              "ib=%u; AUTOGENMIPMAP+MANAGED=%u | locks on MANAGED: plain=%u READONLY=%u DISCARD=%u NOOVERWRITE=%u "
              "partial=%u level>0=%u dirtyRects=%u | %s",
              g_managed, g_creations, g_managedBytes / 1048576.0, g_bytes / 1048576.0, g_managedByCall[kTexture],
              g_managedByCall[kCube], g_managedByCall[kVolume], g_managedByCall[kVertexBuffer], g_managedByCall[kIndexBuffer],
              g_managedAutoMip, plain, ro, discard, noover, partial, level, g_dirtyRectsOnManaged,
              g_managed == 0 ? "no MANAGED: an Ex device needs no translation"
              : ro == 0     ? "no READONLY lock on MANAGED so far: DEFAULT + DYNAMIC can stand in ([Device] Managed=dynamic)"
                            : "READONLY locks on MANAGED: a DYNAMIC stand-in reads VRAM through an uncached map (garbage after "
                              "streaming) - the shadow route is the safe translation");
}

} // namespace

void install(IDirect3DDevice9* dev, IDirect3D9* d3d, UINT adapter, D3DDEVTYPE type, DWORD createFlags,
             const D3DPRESENT_PARAMETERS* pp) {
    if (!g_csInit) { InitializeCriticalSection(&g_cs); g_csInit = true; }
    if (!dev) return;
    void* old;
    old = PatchVtable(dev, 23, (void*)hkCreateTexture);        if (old && !g_origCreateTexture) g_origCreateTexture = (PFN_CreateTexture)old;
    old = PatchVtable(dev, 24, (void*)hkCreateVolumeTexture);  if (old && !g_origCreateVolume) g_origCreateVolume = (PFN_CreateVolumeTexture)old;
    old = PatchVtable(dev, 25, (void*)hkCreateCubeTexture);    if (old && !g_origCreateCube) g_origCreateCube = (PFN_CreateCubeTexture)old;
    old = PatchVtable(dev, 26, (void*)hkCreateVertexBuffer);   if (old && !g_origCreateVb) g_origCreateVb = (PFN_CreateVertexBuffer)old;
    old = PatchVtable(dev, 27, (void*)hkCreateIndexBuffer);    if (old && !g_origCreateIb) g_origCreateIb = (PFN_CreateIndexBuffer)old;
    old = PatchVtable(dev, 28, (void*)hkCreateRenderTarget);   if (old && !g_origCreateRt) g_origCreateRt = (PFN_CreateRenderTarget)old;
    old = PatchVtable(dev, 29, (void*)hkCreateDepthStencil);   if (old && !g_origCreateDs) g_origCreateDs = (PFN_CreateDepthStencil)old;
    old = PatchVtable(dev, 36, (void*)hkCreateOffscreenPlain); if (old && !g_origCreateOffscreen) g_origCreateOffscreen = (PFN_CreateOffscreenPlain)old;
    DVR_INFO("device/census: creation hooks %s on device %p (CreateTexture/Volume/Cube/VB/IB/RT/DS/OffscreenPlain)",
             g_origCreateTexture ? "installed" : "NOT installed", (void*)dev);
    // What the game asked for, once: the flags that shape an Ex device's
    // behaviour and the caps that decide the translation.
    DVR_INFO("device/census: CreateDevice adapter=%u type=%d flags=0x%lx [%s%s%s%s%s%s] | pp %ux%u fmt=%d count=%u ms=%d/%lu "
             "swap=%d windowed=%d autoDS=%d dsFmt=%d flags=0x%lx refresh=%u interval=0x%lx",
             adapter, (int)type, (unsigned long)createFlags,
             (createFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING) ? "HWVP " : "",
             (createFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING) ? "SWVP " : "",
             (createFlags & D3DCREATE_MIXED_VERTEXPROCESSING) ? "MIXEDVP " : "",
             (createFlags & D3DCREATE_PUREDEVICE) ? "PURE " : "",
             (createFlags & D3DCREATE_MULTITHREADED) ? "MULTITHREADED " : "",
             (createFlags & D3DCREATE_FPU_PRESERVE) ? "FPU_PRESERVE " : "",
             pp ? pp->BackBufferWidth : 0, pp ? pp->BackBufferHeight : 0, pp ? (int)pp->BackBufferFormat : -1,
             pp ? pp->BackBufferCount : 0, pp ? (int)pp->MultiSampleType : -1, pp ? (unsigned long)pp->MultiSampleQuality : 0,
             pp ? (int)pp->SwapEffect : -1, pp ? (int)pp->Windowed : -1, pp ? (int)pp->EnableAutoDepthStencil : -1,
             pp ? (int)pp->AutoDepthStencilFormat : -1, pp ? (unsigned long)pp->Flags : 0,
             pp ? pp->FullScreen_RefreshRateInHz : 0, pp ? (unsigned long)pp->PresentationInterval : 0);
    D3DCAPS9 caps = {};
    const HRESULT hc = dev->GetDeviceCaps(&caps);
    DVR_INFO("device/census: GetAvailableTextureMem=%u MB | caps%s: DYNAMICTEXTURES=%d CANAUTOGENMIPMAP=%d | adapter LUID via 9Ex only",
             (unsigned)(dev->GetAvailableTextureMem() / 1048576u), SUCCEEDED(hc) ? "" : " (GetDeviceCaps FAILED)",
             (caps.Caps2 & D3DCAPS2_DYNAMICTEXTURES) ? 1 : 0, (caps.Caps2 & D3DCAPS2_CANAUTOGENMIPMAP) ? 1 : 0);
    (void)d3d;
}

void log_summary(const char* why) {
    if (!g_csInit) return;
    EnterCriticalSection(&g_cs);
    DVR_INFO("device/census: %s - %u creations, %u failed, %.1f MB asked; %d distinct shapes; lock map overflow %u",
             why ? why : "?", g_creations, g_failures, g_bytes / 1048576.0, g_rowCount, g_mapOverflow);
    // rows by bytes, descending (a small selection sort on 512 slots is fine here)
    int order[kRows]; int n = 0;
    for (int i = 0; i < kRows; ++i) if (g_rows[i].key) order[n++] = i;
    for (int i = 0; i < n; ++i) for (int j = i + 1; j < n; ++j) if (g_rows[order[j]].bytes > g_rows[order[i]].bytes) { int t = order[i]; order[i] = order[j]; order[j] = t; }
    const int cap = n < 40 ? n : 40;
    for (int i = 0; i < cap; ++i) {
        const Row& r = g_rows[order[i]];
        char u[64]; usage_text(u, sizeof(u), key_usage(r.key));
        DVR_INFO("device/census:   %-26s pool=%-9s usage=%-20s fmt=%-7s n=%-6u %8.2f MB fail=%u%s%s",
                 kCallNames[key_call(r.key)], kPoolNames[key_pool(r.key)], u, kFmtNames[key_fmt(r.key)], r.count,
                 r.bytes / 1048576.0, r.failures, r.failures ? " first=" : "", r.failures ? r.firstFailAsk : "");
        (void)r.firstFail;
    }
    if (n > cap) DVR_INFO("device/census:   ... %d more shapes (Debug deltas carry them)", n - cap);
    for (int c = 0; c < kLcCount; ++c)
        for (int p = 0; p < 4; ++p) {
            uint32_t any = 0;
            for (int f = 0; f < kLfCount; ++f) any += g_locks[c][p][f];
            if (!any) continue;
            DVR_INFO("device/census:   locks %-14s pool=%-9s plain=%u READONLY=%u DISCARD=%u NOOVERWRITE=%u NOSYSLOCK=%u partial=%u level>0=%u%s",
                     kLockClassNames[c], kPoolNames[p], g_locks[c][p][kLfPlain], g_locks[c][p][kLfReadOnly],
                     g_locks[c][p][kLfDiscard], g_locks[c][p][kLfNoOverwrite], g_locks[c][p][kLfNoSysLock],
                     g_locks[c][p][kLfPartial], g_locks[c][p][kLfLevel],
                     (p == 0 && g_locks[c][0][kLfUntracked]) ? " (+ locks on untracked objects counted under DEFAULT)" : "");
        }
    char v[640]; verdict_line(v, sizeof(v));
    DVR_INFO("%s", v);
    for (int i = 0; i < kRows; ++i) g_rows[i].seenCount = g_rows[i].count;
    memcpy(g_locksSeen, g_locks, sizeof(g_locks));
    LeaveCriticalSection(&g_cs);
}

void log_deltas() {
    if (!g_csInit || !::dvr::log::enabled(DVR_CAT, ::dvr::log::Level::Debug)) return;
    EnterCriticalSection(&g_cs);
    int moved = 0;
    for (int i = 0; i < kRows; ++i) {
        Row& r = g_rows[i];
        if (!r.key || r.count == r.seenCount) continue;
        ++moved;
        char u[64]; usage_text(u, sizeof(u), key_usage(r.key));
        DVR_DEBUG("device/census: +%u %s pool=%s usage=%s fmt=%s (now %u, %.2f MB)", r.count - r.seenCount,
                  kCallNames[key_call(r.key)], kPoolNames[key_pool(r.key)], u, kFmtNames[key_fmt(r.key)], r.count,
                  r.bytes / 1048576.0);
        r.seenCount = r.count;
    }
    uint32_t lockMoved = 0;
    for (int c = 0; c < kLcCount; ++c) for (int p = 0; p < 4; ++p) for (int f = 0; f < kLfCount; ++f)
        lockMoved += g_locks[c][p][f] - g_locksSeen[c][p][f];
    memcpy(g_locksSeen, g_locks, sizeof(g_locks));
    DVR_DEBUG("device/census: %d rows moved, %u locks since the last delta", moved, lockMoved);
    LeaveCriticalSection(&g_cs);
}

void log_status() {
    char v[640]; verdict_line(v, sizeof(v));
    DVR_INFO("%s", v);
}

void status(dvr::status::Writer& w) {
    w.kv("creations", (unsigned long)g_creations);
    w.kv("failures", (unsigned long)g_failures);
    w.kv("managed", (unsigned long)g_managed);
    w.kv("managedMB", (double)(g_managedBytes / 1048576.0));
    w.kv("totalMB", (double)(g_bytes / 1048576.0));
    w.kv("managedAutoMip", (unsigned long)g_managedAutoMip);
    w.kv("readonlyLocksOnManaged", (unsigned long)readonly_locks_on_managed());
    w.kv("shapes", g_rowCount);
}

uint32_t creations() { return g_creations; }
uint32_t managed_creations() { return g_managed; }
uint64_t managed_bytes() { return g_managedBytes; }
uint32_t readonly_locks_on_managed() {
    uint32_t ro = 0;
    for (int c = 0; c < kLcCount; ++c) ro += g_locks[c][1][kLfReadOnly];
    return ro;
}

} // namespace dvr::census
