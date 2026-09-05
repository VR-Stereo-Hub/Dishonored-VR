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
// VR-15: twins released while they had never carried one successful
// UpdateTexture. A texture the game filled through its twin and dropped
// with zero uploads never reached the GPU at all - it draws as its created
// contents, which is black. Counted at Release, so the population is closed.
uint32_t g_shadowDroppedNeverUpdated = 0;
// VR-15: the mip-level lane. The reported fault is distance-dependent, so
// which LEVEL an unlock carried is the question, and until now it was thrown
// away at the door: shadow_unlocked took only the texture.
bool     g_fullCopy = false;              // [Device] ShadowFullCopy, default OFF
uint32_t g_shadowSubLevelUnlocks = 0;     // unlocks that carried a level > 0
int      g_shadowMaxLevelSeen = -1;
uint32_t g_shadowLevelCopies = 0, g_shadowLevelCopyFailed = 0;
HRESULT  g_shadowLevelFirstHr = S_OK;
uint64_t g_shadowBytes = 0;

// the twin map: real -> twin, open addressing. Removal leaves a tombstone
// (real = the map itself, never a texture pointer) that lookups skip and
// puts REUSE: the 2026-09-03 headset run filled 8192 slots with tombstones
// across repeated quickloads (2400 live), a texture then got no twin, its
// lock was refused and the game died inside D3D9 on it. 32768 slots (256 KB)
// hold two levels' textures during a transition.
constexpr int kMap = 32768;
constexpr int kProbe = 512;
struct Ent { void* real; IDirect3DBaseTexture9* twin; uint32_t updates; };
Ent g_map[kMap];
int g_mapCount = 0;
int g_mapTombs = 0;
uint32_t g_mapFull = 0;
CRITICAL_SECTION g_cs;
bool g_csInit = false;

void cs_init() { if (!g_csInit) { InitializeCriticalSection(&g_cs); g_csInit = true; } }

inline uint32_t hash_ptr(void* p) { return (uint32_t)((uintptr_t)p >> 4) * 2654435761u; }
void* const kTomb = (void*)&g_map;

bool map_put(void* real, IDirect3DBaseTexture9* twin) {
    const uint32_t h = hash_ptr(real);
    Ent* tomb = nullptr;
    for (int i = 0; i < kProbe; ++i) {
        Ent& e = g_map[(h + i) % kMap];
        if (e.real == real) { e.twin = twin; e.updates = 0; return true; }
        if (e.real == kTomb) { if (!tomb) tomb = &e; continue; }
        if (e.real == nullptr) {
            Ent& slot = tomb ? *tomb : e;
            if (tomb) --g_mapTombs;
            slot.real = real; slot.twin = twin; slot.updates = 0;
            ++g_mapCount;
            return true;
        }
    }
    if (tomb) { tomb->real = real; tomb->twin = twin; tomb->updates = 0; --g_mapTombs; ++g_mapCount; return true; }
    ++g_mapFull;
    return false;
}
Ent* map_find(void* real) {
    const uint32_t h = hash_ptr(real);
    for (int i = 0; i < kProbe; ++i) {
        Ent& e = g_map[(h + i) % kMap];
        if (e.real == real) return &e;
        if (e.real == nullptr) return nullptr;
    }
    return nullptr;
}
void map_remove(Ent* e) { e->real = kTomb; e->twin = nullptr; e->updates = 0; --g_mapCount; ++g_mapTombs; }

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

// VR-15: the per-level push. UpdateTexture takes no level and copies what
// D3D9 believes is dirty; the reported fault is distance-dependent (black far
// away, correct up close), which is a MIP-LEVEL fault, and the census counts
// 50189 locks on level>0 against 0 AddDirtyRect calls. UpdateSurface is the
// operation that cannot be vague about which level it copied: it names the
// two surfaces. It fails soft - on a refusal the whole-texture UpdateTexture
// still runs, so the lever can never make the picture worse than it is.
bool update_one_level(void* real, IDirect3DBaseTexture9* twin, int level, int face) {
    if (level < 0) return false;
    IDirect3DSurface9 *src = nullptr, *dst = nullptr;
    if (face < 0) {
        ((IDirect3DTexture9*)twin)->GetSurfaceLevel((UINT)level, &src);
        ((IDirect3DTexture9*)real)->GetSurfaceLevel((UINT)level, &dst);
    } else {
        ((IDirect3DCubeTexture9*)twin)->GetCubeMapSurface((D3DCUBEMAP_FACES)face, (UINT)level, &src);
        ((IDirect3DCubeTexture9*)real)->GetCubeMapSurface((D3DCUBEMAP_FACES)face, (UINT)level, &dst);
    }
    bool ok = false;
    if (src && dst) {
        const HRESULT hr = g_dev->UpdateSurface(src, nullptr, dst, nullptr);
        if (SUCCEEDED(hr)) { ++g_shadowLevelCopies; ok = true; }
        else {
            ++g_shadowLevelCopyFailed;
            if (g_shadowLevelFirstHr == S_OK) g_shadowLevelFirstHr = hr;
            DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Warn, 5,
                            "device/shadow: UpdateSurface level %d of twin %p -> real %p refused 0x%08lx - falling back "
                            "to the whole-texture UpdateTexture for this unlock", level, (void*)twin, real,
                            (unsigned long)hr);
        }
    }
    if (src) src->Release();
    if (dst) dst->Release();
    return ok;
}

void shadow_unlocked(void* real, int level, int face) {
    IDirect3DBaseTexture9* twin = shadow_twin(real);
    if (!twin || !g_dev) return;
    if (level > g_shadowMaxLevelSeen) g_shadowMaxLevelSeen = level;
    if (level > 0) ++g_shadowSubLevelUnlocks;
    // The lever: push exactly the level that was written, then fall through to
    // UpdateTexture only if that refused.
    if (g_fullCopy && update_one_level(real, twin, level, face)) {
        if (g_csInit) {
            EnterCriticalSection(&g_cs);
            if (Ent* e = map_find(real)) ++e->updates;
            LeaveCriticalSection(&g_cs);
        }
        return;
    }
    // The dirty regions the lock marked on the twin go to the real texture.
    const HRESULT hr = g_dev->UpdateTexture(twin, (IDirect3DBaseTexture9*)real);
    if (FAILED(hr)) {
        ++g_shadowUpdateFailed;
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Error, 5,
                        "device/shadow: UpdateTexture twin %p -> real %p refused 0x%08lx - the write did not reach the "
                        "GPU (a corrupt or stale texture follows)", (void*)twin, real, (unsigned long)hr);
    } else {
        ++g_shadowUpdates;
        // VR-15: mark THIS twin as having carried a real upload. The count of
        // twins still at zero is the population of candidate black textures -
        // and it is a number that can come back zero and falsify the shadow.
        if (g_csInit) {
            EnterCriticalSection(&g_cs);
            if (Ent* e = map_find(real)) ++e->updates;
            LeaveCriticalSection(&g_cs);
        }
    }
}

void shadow_released(void* real) {
    if (!g_csInit || g_mapCount == 0) return;
    EnterCriticalSection(&g_cs);
    Ent* e = map_find(real);
    IDirect3DBaseTexture9* twin = e ? e->twin : nullptr;
    if (e && e->updates == 0) ++g_shadowDroppedNeverUpdated;
    if (e) map_remove(e);
    LeaveCriticalSection(&g_cs);
    if (twin) { twin->Release(); ++g_shadowReleased; }
}

// VR-15: the twin population, walked on demand only (32768 slots is one
// pass and this is never on a per-frame path). `live` is every twin the map
// still holds; `neverUpdated` is how many of those have carried no
// successful UpdateTexture since they were made. A live twin at zero is a
// texture whose pixels are still only in system memory.
void shadow_population(int* live, int* neverUpdated, uint32_t* droppedNeverUpdated) {
    int l = 0, n = 0;
    if (g_csInit) {
        EnterCriticalSection(&g_cs);
        for (int i = 0; i < kMap; ++i) {
            const Ent& e = g_map[i];
            if (!e.real || e.real == kTomb) continue;
            ++l;
            if (e.updates == 0) ++n;
        }
        LeaveCriticalSection(&g_cs);
    }
    if (live) *live = l;
    if (neverUpdated) *neverUpdated = n;
    if (droppedNeverUpdated) *droppedNeverUpdated = g_shadowDroppedNeverUpdated;
}

bool shadow_active() { return translating() && g_managed == Managed::Shadow; }

// VR-15: the per-level push lever.
void set_full_copy(bool on) {
    g_fullCopy = on;
    DVR_INFO("device/shadow: [Device] ShadowFullCopy=%d - an unlock now pushes %s. The reported black-at-distance "
             "fault is a MIP fault, and %u unlocks so far have carried a level > 0 (deepest level %d). If this "
             "removes black surfaces at distance, UpdateTexture was not carrying sub-level writes.",
             on ? 1 : 0,
             on ? "exactly the level it wrote, with UpdateSurface (UpdateTexture is the fallback if that refuses)"
                : "the whole texture with UpdateTexture, which takes no level",
             g_shadowSubLevelUnlocks, g_shadowMaxLevelSeen);
}
bool full_copy() { return g_fullCopy; }
void shadow_levels(uint32_t* subLevelUnlocks, int* maxLevel, uint32_t* copies, uint32_t* copyFailed, HRESULT* firstHr) {
    if (subLevelUnlocks) *subLevelUnlocks = g_shadowSubLevelUnlocks;
    if (maxLevel) *maxLevel = g_shadowMaxLevelSeen;
    if (copies) *copies = g_shadowLevelCopies;
    if (copyFailed) *copyFailed = g_shadowLevelCopyFailed;
    if (firstHr) *firstHr = g_shadowLevelFirstHr;
}

void log_status() {
    DVR_INFO("device: [Device] Ex=%d Managed=%s | Direct3DCreate9 calls %d (Ex objects %d) | device %s (%s) | "
             "translated tex=%u (dynamic %u) buf=%u | shadow twins made=%u failed=%u live=%d tombstones=%d of %d slots "
             "(%.1f MB asked, uncompressed) updates=%u failed=%u released=%u mapFull=%u",
             g_exWanted ? 1 : 0, managed_name(g_managed), g_createCalls, g_exCount,
             !g_deviceLive ? "not created yet" : g_deviceIsEx ? "IS 9Ex" : "is NOT 9Ex", g_route, g_texTranslated,
             g_texDynamic, g_bufTranslated, g_shadowMade, g_shadowFailed, g_mapCount, g_mapTombs, kMap,
             g_shadowBytes / 1048576.0, g_shadowUpdates, g_shadowUpdateFailed, g_shadowReleased, g_mapFull);
}

void status(dvr::status::Writer& w) {
    w.kv("exWanted", g_exWanted);
    w.kv("managed", managed_name(g_managed));
    w.kv("deviceIsEx", g_deviceIsEx);
    w.kv("route", g_route);
    w.kv("texTranslated", (unsigned long)g_texTranslated);
    w.kv("bufTranslated", (unsigned long)g_bufTranslated);
    w.kv("shadowLive", g_mapCount);
    w.kv("shadowTombstones", g_mapTombs);
    w.kv("shadowMapFull", (unsigned long)g_mapFull);
    w.kv("shadowMB", (double)(g_shadowBytes / 1048576.0));
    w.kv("shadowUpdates", (unsigned long)g_shadowUpdates);
    w.kv("shadowUpdateFailed", (unsigned long)g_shadowUpdateFailed);
    w.kv("shadowFailed", (unsigned long)g_shadowFailed);
    int live = 0, never = 0; uint32_t dropped = 0;
    shadow_population(&live, &never, &dropped);
    w.kv("shadowLiveNeverUpdated", never);
    w.kv("shadowDroppedNeverUpdated", (unsigned long)dropped);
}

} // namespace dvr::d3d9ex
