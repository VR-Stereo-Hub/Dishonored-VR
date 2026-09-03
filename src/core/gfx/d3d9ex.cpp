// core/gfx/d3d9ex.cpp - see d3d9ex.h.
#define DVR_CAT ::dvr::log::Cat::device
#include "core/gfx/d3d9ex.h"

#include "core/framework/status.h"
#include "core/util/log.h"

#include <stdio.h>
#include <string.h>

namespace dvr::d3d9ex {
namespace {

bool    g_exWanted = false;
Managed g_managed = Managed::Shadow;
const char* const kManagedNames[4] = {"none", "default", "dynamic", "shadow"};

IDirect3D9Ex* g_exObjects[4] = {};
int           g_exCount = 0;
IDirect3D9*   g_plain = nullptr;      // the unpatched fallback object (Ex=1 only)
int           g_createCalls = 0;
IDirect3DDevice9* g_dev = nullptr;
bool g_deviceLive = false;
bool g_deviceIsEx = false;
bool g_deviceFromEx = false;          // came out of CreateDeviceEx (not a fallback)
LUID g_luid = {};
bool g_luidOk = false;
char g_route[96] = "no device yet";

// counts
uint32_t g_texTranslated = 0, g_bufTranslated = 0, g_texDynamic = 0;
uint32_t g_shadowMade = 0, g_shadowFailed = 0, g_shadowUpdates = 0, g_shadowUpdateFailed = 0, g_shadowReleased = 0;
uint64_t g_shadowBytes = 0;

// the twin map: real -> twin, open addressing
constexpr int kMap = 8192;
struct Ent { void* real; IDirect3DBaseTexture9* twin; };
Ent g_map[kMap];
int g_mapCount = 0;
uint32_t g_mapFull = 0;
CRITICAL_SECTION g_cs;
bool g_csInit = false;

void cs_init() { if (!g_csInit) { InitializeCriticalSection(&g_cs); g_csInit = true; } }

inline uint32_t hash_ptr(void* p) { return (uint32_t)((uintptr_t)p >> 4) * 2654435761u; }

bool map_put(void* real, IDirect3DBaseTexture9* twin) {
    const uint32_t h = hash_ptr(real);
    for (int i = 0; i < 128; ++i) {
        Ent& e = g_map[(h + i) % kMap];
        if (e.real == real || e.real == nullptr) {
            if (e.real == nullptr) ++g_mapCount;
            e.real = real; e.twin = twin;
            return true;
        }
    }
    ++g_mapFull;
    return false;
}
Ent* map_find(void* real) {
    const uint32_t h = hash_ptr(real);
    for (int i = 0; i < 128; ++i) {
        Ent& e = g_map[(h + i) % kMap];
        if (e.real == real) return &e;
        if (e.real == nullptr) return nullptr;
    }
    return nullptr;
}
// Removal keeps the probe chains intact by leaving a tombstone (real = the
// map itself, never a texture pointer) that lookups skip and puts reuse.
void* const kTomb = (void*)&g_map;
void map_remove(Ent* e) { e->real = kTomb; e->twin = nullptr; --g_mapCount; }

} // namespace

bool parse_managed(const char* s, Managed* out) {
    if (!s || !out) return false;
    for (int i = 0; i < 4; ++i) if (!_stricmp(s, kManagedNames[i])) { *out = (Managed)i; return true; }
    return false;
}
const char* managed_name(Managed m) { return kManagedNames[(int)m & 3]; }

void set_config(bool ex, Managed m) {
    g_exWanted = ex;
    g_managed = m;
    DVR_INFO("config: [Device] Ex=%d Managed=%s (%s)", ex ? 1 : 0, managed_name(m),
             ex ? "the game's device is created as D3D9Ex; MANAGED creations are translated"
                : "the plain D3D9 device; Managed is inert; 'device ex on' asks for 9Ex at the next launch");
}
bool ex_wanted() { return g_exWanted; }
Managed managed_mode() { return g_managed; }

IDirect3D9* create_d3d(UINT sdk, PFN_Create9 plain, PFN_Create9Ex ex) {
    ++g_createCalls;
    if (!g_exWanted || !ex) {
        IDirect3D9* d = plain ? plain(sdk) : nullptr;
        DVR_INFO("device: Direct3DCreate9 call #%d -> plain IDirect3D9 %p ([Device] Ex=%d%s)", g_createCalls, (void*)d,
                 g_exWanted ? 1 : 0, (g_exWanted && !ex) ? ", but the system d3d9 exports no Direct3DCreate9Ex" : "");
        return d;
    }
    IDirect3D9Ex* e = nullptr;
    const HRESULT hr = ex(sdk, &e);
    if (FAILED(hr) || !e) {
        DVR_WARN("device: Direct3DCreate9Ex(sdk %u) refused 0x%08lx on call #%d - the plain IDirect3D9 this run (no "
                 "shared capture; the log's device lines say why)", sdk, (unsigned long)hr, g_createCalls);
        return plain ? plain(sdk) : nullptr;
    }
    if (g_exCount < 4) g_exObjects[g_exCount++] = e;
    if (!g_plain && plain) g_plain = plain(sdk);
    DVR_INFO("device: Direct3DCreate9 call #%d -> IDirect3D9Ex %p handed back as IDirect3D9 ([Device] Ex=1, "
             "Managed=%s; the plain fallback object is %p)", g_createCalls, (void*)e, managed_name(g_managed),
             (void*)g_plain);
    return e;
}

bool is_ex_object(IDirect3D9* d3d) {
    for (int i = 0; i < g_exCount; ++i) if ((IDirect3D9*)g_exObjects[i] == d3d) return true;
    return false;
}

HRESULT create_device(IDirect3D9* self, UINT adapter, D3DDEVTYPE type, HWND wnd, DWORD flags,
                      D3DPRESENT_PARAMETERS* pp, PFN_CreateDevice orig, IDirect3DDevice9** outDev) {
    cs_init();
    if (!is_ex_object(self)) {
        const HRESULT hr = orig(self, adapter, type, wnd, flags, pp, outDev);
        strcpy_s(g_route, sizeof(g_route), "plain CreateDevice on the plain object");
        if (SUCCEEDED(hr) && outDev && *outDev) { g_dev = *outDev; g_deviceLive = true; }
        return hr;
    }
    IDirect3D9Ex* ex = (IDirect3D9Ex*)self;
    D3DDISPLAYMODEEX mode = {};
    D3DDISPLAYMODEEX* pMode = nullptr;
    if (pp && !pp->Windowed) {
        mode.Size = sizeof(mode);
        mode.Width = pp->BackBufferWidth; mode.Height = pp->BackBufferHeight;
        mode.Format = pp->BackBufferFormat;
        mode.RefreshRate = pp->FullScreen_RefreshRateInHz;
        mode.ScanLineOrdering = D3DSCANLINEORDERING_PROGRESSIVE;
        D3DDISPLAYMODEEX cur = {};
        cur.Size = sizeof(cur);
        if (SUCCEEDED(ex->GetAdapterDisplayModeEx(adapter, &cur, nullptr))) {
            if (mode.RefreshRate == 0) mode.RefreshRate = cur.RefreshRate;   // a zero rate is the documented refusal
            if (mode.Format == D3DFMT_UNKNOWN) mode.Format = cur.Format;
        }
        pMode = &mode;
        DVR_INFO("device: CreateDeviceEx fullscreen mode %ux%u fmt=%d @%u Hz progressive (from pp; the adapter reads "
                 "%ux%u @%u)", mode.Width, mode.Height, (int)mode.Format, mode.RefreshRate, cur.Width, cur.Height,
                 cur.RefreshRate);
    }
    IDirect3DDevice9Ex* devEx = nullptr;
    HRESULT hr = ex->CreateDeviceEx(adapter, type, wnd, flags, pp, pMode, &devEx);
    DVR_INFO("device: CreateDeviceEx(adapter %u, type %d, flags 0x%lx, %ux%u windowed=%d) -> 0x%08lx dev=%p", adapter,
             (int)type, (unsigned long)flags, pp ? pp->BackBufferWidth : 0, pp ? pp->BackBufferHeight : 0,
             pp ? (int)pp->Windowed : -1, (unsigned long)hr, (void*)devEx);
    if (SUCCEEDED(hr) && devEx) {
        if (outDev) *outDev = devEx;
        g_deviceFromEx = true;
        strcpy_s(g_route, sizeof(g_route), "CreateDeviceEx on the Ex object");
    } else {
        DVR_WARN("device: CreateDeviceEx refused (0x%08lx) - trying the plain CreateDevice on the Ex object",
                 (unsigned long)hr);
        hr = orig(self, adapter, type, wnd, flags, pp, outDev);
        DVR_INFO("device: plain CreateDevice on the Ex object -> 0x%08lx", (unsigned long)hr);
        strcpy_s(g_route, sizeof(g_route), "plain CreateDevice on the Ex object");
        if (FAILED(hr) && g_plain) {
            DVR_WARN("device: refused again (0x%08lx) - the plain IDirect3D9 %p creates the device (no 9Ex this run)",
                     (unsigned long)hr, (void*)g_plain);
            hr = g_plain->CreateDevice(adapter, type, wnd, flags, pp, outDev);
            DVR_INFO("device: plain IDirect3D9::CreateDevice -> 0x%08lx", (unsigned long)hr);
            strcpy_s(g_route, sizeof(g_route), "plain CreateDevice on the plain fallback object");
        }
    }
    if (FAILED(hr) || !outDev || !*outDev) {
        DVR_ERROR("device: no device could be created (last 0x%08lx) - the game will fail on its own", (unsigned long)hr);
        return hr;
    }
    g_dev = *outDev;
    g_deviceLive = true;
    // The measurement: is the device the game got a 9Ex device? The
    // translation and the shared capture both key off this answer.
    IDirect3DDevice9Ex* q = nullptr;
    g_deviceIsEx = SUCCEEDED((*outDev)->QueryInterface(__uuidof(IDirect3DDevice9Ex), (void**)&q)) && q;
    if (q) q->Release();
    g_luidOk = SUCCEEDED(ex->GetAdapterLUID(adapter, &g_luid));
    DVR_INFO("device: the game's device %s IDirect3DDevice9Ex (route: %s) | adapter LUID %08lx-%08lx%s | MANAGED "
             "creations will be %s",
             g_deviceIsEx ? "IS" : "is NOT", g_route, (unsigned long)g_luid.HighPart, (unsigned long)g_luid.LowPart,
             g_luidOk ? "" : " (GetAdapterLUID failed)",
             !g_deviceIsEx ? "passed through (not an Ex device)"
             : g_managed == Managed::None ? "passed through and REFUSED by 9Ex (Managed=none: the measurement)"
             : g_managed == Managed::Default ? "DEFAULT (buffers lockable; textures LOSE their locks: the A/B)"
             : g_managed == Managed::Dynamic ? "DEFAULT + DYNAMIC on textures (READONLY locks read uncached VRAM)"
                                             : "DEFAULT with a SYSTEMMEM shadow twin per texture (locks redirected)");
    return hr;
}

bool device_is_ex() { return g_deviceIsEx; }
bool adapter_luid(LUID* out) { if (out) *out = g_luid; return g_luidOk; }

bool translating() { return g_deviceIsEx && g_managed != Managed::None; }

Translate translate_texture(DWORD* usage, D3DPOOL* pool) {
    if (!translating() || !pool || *pool != D3DPOOL_MANAGED) return Translate::Untouched;
    *pool = D3DPOOL_DEFAULT;
    if (g_managed == Managed::Dynamic && usage && !(*usage & D3DUSAGE_AUTOGENMIPMAP)) { *usage |= D3DUSAGE_DYNAMIC; ++g_texDynamic; }
    ++g_texTranslated;
    return Translate::Translated;
}
Translate translate_buffer(DWORD* usage, D3DPOOL* pool) {
    (void)usage;
    if (!translating() || !pool || *pool != D3DPOOL_MANAGED) return Translate::Untouched;
    *pool = D3DPOOL_DEFAULT;
    ++g_bufTranslated;
    return Translate::Translated;
}

namespace {
void shadow_put(void* real, IDirect3DBaseTexture9* twin, uint64_t bytes) {
    EnterCriticalSection(&g_cs);
    if (map_put(real, twin)) { ++g_shadowMade; g_shadowBytes += bytes; }
    else {
        ++g_shadowFailed;
        twin->Release();
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Error, 3,
                        "device/shadow: the twin map is full (%d live) - texture %p keeps no shadow and its locks will "
                        "FAIL (INVALIDCALL on a DEFAULT texture)", g_mapCount, real);
    }
    LeaveCriticalSection(&g_cs);
}
} // namespace

void shadow_register_texture(IDirect3DDevice9* dev, IDirect3DTexture9* real, UINT w, UINT h, UINT levels, D3DFORMAT fmt) {
    if (g_managed != Managed::Shadow || !dev || !real) return;
    IDirect3DTexture9* twin = nullptr;
    const HRESULT hr = dev->CreateTexture(w, h, levels, 0, fmt, D3DPOOL_SYSTEMMEM, &twin, nullptr);
    if (FAILED(hr) || !twin) {
        ++g_shadowFailed;
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Error, 5,
                        "device/shadow: SYSTEMMEM twin for texture %p (%ux%u lv=%u fmt=%d) refused 0x%08lx - its locks "
                        "will FAIL", (void*)real, w, h, levels, (int)fmt, (unsigned long)hr);
        return;
    }
    shadow_put(real, twin, (uint64_t)w * h * 4);
}
void shadow_register_cube(IDirect3DDevice9* dev, IDirect3DCubeTexture9* real, UINT edge, UINT levels, D3DFORMAT fmt) {
    if (g_managed != Managed::Shadow || !dev || !real) return;
    IDirect3DCubeTexture9* twin = nullptr;
    const HRESULT hr = dev->CreateCubeTexture(edge, levels, 0, fmt, D3DPOOL_SYSTEMMEM, &twin, nullptr);
    if (FAILED(hr) || !twin) {
        ++g_shadowFailed;
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Error, 5,
                        "device/shadow: SYSTEMMEM twin for cube texture %p (%u lv=%u fmt=%d) refused 0x%08lx - its "
                        "locks will FAIL", (void*)real, edge, levels, (int)fmt, (unsigned long)hr);
        return;
    }
    shadow_put(real, twin, (uint64_t)edge * edge * 4 * 6);
}
void shadow_register_volume(IDirect3DDevice9* dev, IDirect3DVolumeTexture9* real, UINT w, UINT h, UINT d, UINT levels, D3DFORMAT fmt) {
    if (g_managed != Managed::Shadow || !dev || !real) return;
    IDirect3DVolumeTexture9* twin = nullptr;
    const HRESULT hr = dev->CreateVolumeTexture(w, h, d, levels, 0, fmt, D3DPOOL_SYSTEMMEM, &twin, nullptr);
    if (FAILED(hr) || !twin) {
        ++g_shadowFailed;
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Error, 5,
                        "device/shadow: SYSTEMMEM twin for volume texture %p refused 0x%08lx - its locks will FAIL",
                        (void*)real, (unsigned long)hr);
        return;
    }
    shadow_put(real, twin, (uint64_t)w * h * d * 4);
}

IDirect3DBaseTexture9* shadow_twin(void* real) {
    if (!g_csInit || g_mapCount == 0) return nullptr;
    EnterCriticalSection(&g_cs);
    Ent* e = map_find(real);
    IDirect3DBaseTexture9* t = e ? e->twin : nullptr;
    LeaveCriticalSection(&g_cs);
    return t;
}

void shadow_unlocked(void* real) {
    IDirect3DBaseTexture9* twin = shadow_twin(real);
    if (!twin || !g_dev) return;
    // The dirty regions the lock marked on the twin go to the real texture.
    const HRESULT hr = g_dev->UpdateTexture(twin, (IDirect3DBaseTexture9*)real);
    if (FAILED(hr)) {
        ++g_shadowUpdateFailed;
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Error, 5,
                        "device/shadow: UpdateTexture twin %p -> real %p refused 0x%08lx - the write did not reach the "
                        "GPU (a corrupt or stale texture follows)", (void*)twin, real, (unsigned long)hr);
    } else ++g_shadowUpdates;
}

void shadow_released(void* real) {
    if (!g_csInit || g_mapCount == 0) return;
    EnterCriticalSection(&g_cs);
    Ent* e = map_find(real);
    IDirect3DBaseTexture9* twin = e ? e->twin : nullptr;
    if (e) map_remove(e);
    LeaveCriticalSection(&g_cs);
    if (twin) { twin->Release(); ++g_shadowReleased; }
}

void log_status() {
    DVR_INFO("device: [Device] Ex=%d Managed=%s | Direct3DCreate9 calls %d (Ex objects %d) | device %s (%s) | "
             "translated tex=%u (dynamic %u) buf=%u | shadow twins made=%u failed=%u live=%d (%.1f MB) updates=%u "
             "failed=%u released=%u mapFull=%u",
             g_exWanted ? 1 : 0, managed_name(g_managed), g_createCalls, g_exCount,
             !g_deviceLive ? "not created yet" : g_deviceIsEx ? "IS 9Ex" : "is NOT 9Ex", g_route, g_texTranslated,
             g_texDynamic, g_bufTranslated, g_shadowMade, g_shadowFailed, g_mapCount, g_shadowBytes / 1048576.0,
             g_shadowUpdates, g_shadowUpdateFailed, g_shadowReleased, g_mapFull);
}

void status(dvr::status::Writer& w) {
    w.kv("exWanted", g_exWanted);
    w.kv("managed", managed_name(g_managed));
    w.kv("deviceIsEx", g_deviceIsEx);
    w.kv("route", g_route);
    w.kv("texTranslated", (unsigned long)g_texTranslated);
    w.kv("bufTranslated", (unsigned long)g_bufTranslated);
    w.kv("shadowLive", g_mapCount);
    w.kv("shadowMB", (double)(g_shadowBytes / 1048576.0));
    w.kv("shadowUpdates", (unsigned long)g_shadowUpdates);
    w.kv("shadowUpdateFailed", (unsigned long)g_shadowUpdateFailed);
    w.kv("shadowFailed", (unsigned long)g_shadowFailed);
}

} // namespace dvr::d3d9ex
