// core/gfx/d3d9ex.cpp - see d3d9ex.h.
#define DVR_CAT ::dvr::log::Cat::device
#include "core/gfx/d3d9ex.h"

#include "core/framework/status.h"
#include "core/util/log.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>

namespace dvr::d3d9ex {
namespace {

bool    g_exWanted = false;
Managed g_managed = Managed::Shadow;
const char* const kManagedNames[5] = {"none", "default", "dynamic", "shadow", "paged"};

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
// Unlocks whose lock was READONLY, so the twin did not change and there was
// nothing to push. Pure saved work - this game takes 12408 of them per load.
uint32_t g_shadowSkippedReadOnly = 0;
uint64_t g_shadowBytes = 0;
uint64_t g_pagedBytes = 0;
uint32_t g_pagedSections = 0, g_pagedLocks = 0, g_pagedMapFailed = 0;
uint64_t g_pagedMappedBytes = 0, g_pagedMappedPeak = 0, g_pagedUploadBytes = 0;
uint64_t g_pagedMapUs = 0, g_pagedUploadUs = 0, g_pagedMaxUploadUs = 0;
uint32_t g_stageHits = 0, g_stageMisses = 0;
uint64_t elapsed_us(LARGE_INTEGER start) {
    LARGE_INTEGER now, frequency; QueryPerformanceCounter(&now); QueryPerformanceFrequency(&frequency);
    return (uint64_t)((now.QuadPart - start.QuadPart) * 1000000 / frequency.QuadPart);
}

// the twin map: real -> twin, open addressing. Removal leaves a tombstone
// (real = the map itself, never a texture pointer) that lookups skip and
// puts REUSE: the 2026-09-03 headset run filled 8192 slots with tombstones
// across repeated quickloads (2400 live), a texture then got no twin, its
// lock was refused and the game died inside D3D9 on it. 32768 slots (256 KB)
// hold two levels' textures during a transition.
constexpr int kMap = 32768;
constexpr int kProbe = 512;
// roMask: bit N set = level N's most recent lock was READONLY, so its unlock
// has nothing to push. surfaceRefused: this texture's format already refused
// UpdateSurface once; do not pay for the attempt again.
enum class ShadowKind : uint8_t { Legacy = 0, Paged2D = 1, PagedCube = 2 };
struct Ent {
    void* real;
    IDirect3DBaseTexture9* twin;          // legacy shadow (and paged volume fallback)
    HANDLE section;                      // TEXTURE-MEM1 pagefile backing for 2D/cube
    void* activeView;                    // only non-null while a paged mip is locked
    BYTE* activeLevelBase;               // level start inside activeView
    SIZE_T activeViewBytes;
    uint64_t sectionBytes;
    UINT w, h, d, levels;
    D3DFORMAT fmt;
    uint32_t updates;
    uint64_t serial; // changes on every resource registration, including pointer reuse
    uint16_t roMask;
    uint8_t surfaceRefused;
    uint8_t kind;
    uint16_t activeLocks;
    int16_t activeLevel;
    int8_t activeFace;
    uint8_t activeReadOnly;
};
Ent g_map[kMap];
uint64_t g_resourceSerial=0;
int g_mapCount = 0;
int g_mapTombs = 0;
uint32_t g_mapFull = 0;

// TEXTURE-MEM1.2: Dishonored may keep more than one mip of the SAME texture
// locked at once while streaming. TEST1/1.1 stored only one active mapping in
// Ent, so the second LockRect returned D3DERR_INVALIDCALL even though it was a
// different mip. Track short-lived mappings independently by texture/face/mip.
// The table is bounded and holds only currently locked views; it is not a
// persistent shadow copy and therefore does not recreate the 32-bit VA leak.
constexpr int kPagedLockMap = 8192;
constexpr int kPagedLockProbe = 128;
struct PagedLockEnt {
    void* real;
    void* view;
    BYTE* levelBase;
    SIZE_T viewBytes;
    int16_t level;
    int8_t face;
    uint8_t readOnly;
    bool unlocking;
};
PagedLockEnt g_pagedLockMap[kPagedLockMap] = {};
void* const kPagedLockTomb = (void*)&g_pagedLockMap;
uint32_t g_pagedLockMapFull = 0, g_pagedConcurrent = 0, g_pagedConcurrentMax = 0;

inline uint32_t paged_lock_hash(void* real, int level, int face) {
    uint32_t h = (uint32_t)((uintptr_t)real >> 4) * 2654435761u;
    h ^= (uint32_t)(level + 1) * 0x9e3779b9u;
    h ^= (uint32_t)(face + 2) * 0x85ebca6bu;
    return h;
}
PagedLockEnt* paged_lock_find(void* real, int level, int face) {
    const uint32_t h = paged_lock_hash(real, level, face);
    for (int i = 0; i < kPagedLockProbe; ++i) {
        PagedLockEnt& e = g_pagedLockMap[(h + i) % kPagedLockMap];
        if (e.real == real && e.level == level && e.face == face) return &e;
        if (e.real == nullptr) return nullptr;
    }
    return nullptr;
}
PagedLockEnt* paged_lock_alloc(void* real, int level, int face) {
    const uint32_t h = paged_lock_hash(real, level, face);
    PagedLockEnt* tomb = nullptr;
    for (int i = 0; i < kPagedLockProbe; ++i) {
        PagedLockEnt& e = g_pagedLockMap[(h + i) % kPagedLockMap];
        if (e.real == real && e.level == level && e.face == face) return nullptr;
        if (e.real == kPagedLockTomb) { if (!tomb) tomb = &e; continue; }
        if (e.real == nullptr) { PagedLockEnt* dst = tomb ? tomb : &e; *dst = {}; dst->real=real; dst->level=(int16_t)level; dst->face=(int8_t)face; return dst; }
    }
    if (tomb) { *tomb = {}; tomb->real=real; tomb->level=(int16_t)level; tomb->face=(int8_t)face; return tomb; }
    ++g_pagedLockMapFull;
    return nullptr;
}
void paged_lock_remove(PagedLockEnt* e) {
    if (!e) return;
    e->real = kPagedLockTomb; e->view=nullptr; e->levelBase=nullptr; e->viewBytes=0;
    e->level=-1; e->face=-1; e->readOnly=0; e->unlocking=false;
}
CRITICAL_SECTION g_cs;
CRITICAL_SECTION g_stageCs;
bool g_csInit = false;
bool g_stageCsInit = false;

void cs_init() { if (!g_csInit) { InitializeCriticalSection(&g_cs); g_csInit = true; } if(!g_stageCsInit){InitializeCriticalSection(&g_stageCs);g_stageCsInit=true;} }

inline uint32_t hash_ptr(void* p) { return (uint32_t)((uintptr_t)p >> 4) * 2654435761u; }
void* const kTomb = (void*)&g_map;

void ent_reset_payload(Ent& e) {
    e.serial=++g_resourceSerial;
    e.twin = nullptr; e.section = nullptr; e.activeView = nullptr; e.activeLevelBase = nullptr;
    e.activeViewBytes = 0; e.sectionBytes = 0; e.w = e.h = e.d = e.levels = 0; e.fmt = D3DFMT_UNKNOWN;
    e.activeLocks = 0; e.updates = 0; e.roMask = 0; e.surfaceRefused = 0; e.kind = (uint8_t)ShadowKind::Legacy;
    e.activeLevel = -1; e.activeFace = -1; e.activeReadOnly = 0;
}
bool map_put(void* real, IDirect3DBaseTexture9* twin) {
    const uint32_t h = hash_ptr(real);
    Ent* tomb = nullptr;
    for (int i = 0; i < kProbe; ++i) {
        Ent& e = g_map[(h + i) % kMap];
        if (e.real == real) { ent_reset_payload(e); e.twin = twin; return true; }
        if (e.real == kTomb) { if (!tomb) tomb = &e; continue; }
        if (e.real == nullptr) {
            Ent& slot = tomb ? *tomb : e;
            if (tomb) --g_mapTombs;
            slot.real = real; ent_reset_payload(slot); slot.twin = twin; ++g_mapCount; return true;
        }
    }
    if (tomb) { tomb->real = real; ent_reset_payload(*tomb); tomb->twin = twin; --g_mapTombs; ++g_mapCount; return true; }
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
void map_remove(Ent* e) { e->real = kTomb; ent_reset_payload(*e); --g_mapCount; ++g_mapTombs; }

// PERF (2026-09-18): the streaming time series. The census only counted
// uploads since load, which cannot say whether uploads line up with a hitch.
// 100 ms buckets, 64 of them (6.4 s), indexed by tick/100. Writers are the
// render thread (unlock, create, release); the reader is the present thread's
// gap and beat lines. Races only blur a counter, never touch memory.
struct StreamBucket { volatile LONG slot; volatile LONG ups, creates, releases; volatile LONG upKB, createKB, upUs; };
constexpr int kStreamBuckets = 64;
StreamBucket g_stream[kStreamBuckets];
StreamBucket* stream_bucket() {
    const LONG slot = (LONG)(GetTickCount64() / 100);
    StreamBucket& b = g_stream[slot % kStreamBuckets];
    if (b.slot != slot) {   // a bucket from 6.4 s ago: reuse it (a race blurs one count)
        b.ups = b.creates = b.releases = 0; b.upKB = b.createKB = b.upUs = 0;
        InterlockedExchange(&b.slot, slot);
    }
    return &b;
}
// Bytes of one w x h level of fmt (block formats round up to 4x4 blocks).
uint32_t fmt_bpp(D3DFORMAT fmt);
uint64_t level_bytes(D3DFORMAT fmt, UINT w, UINT h) {
    if (!w) w = 1; if (!h) h = 1;
    switch ((DWORD)fmt) {
    case D3DFMT_DXT1: return (uint64_t)((w + 3) / 4) * ((h + 3) / 4) * 8;
    case D3DFMT_DXT2: case D3DFMT_DXT3: case D3DFMT_DXT4: case D3DFMT_DXT5:
    case MAKEFOURCC('A','T','I','2'): return (uint64_t)((w + 3) / 4) * ((h + 3) / 4) * 16;
    case D3DFMT_A16B16G16R16F: case D3DFMT_A16B16G16R16: case D3DFMT_G32R32F: return (uint64_t)w * h * 8;
    case D3DFMT_A32B32G32R32F: return (uint64_t)w * h * 16;
    case D3DFMT_L8: case D3DFMT_A8: case D3DFMT_P8: return (uint64_t)w * h;
    case D3DFMT_R5G6B5: case D3DFMT_A1R5G5B5: case D3DFMT_X1R5G5B5: case D3DFMT_A4R4G4B4:
    case D3DFMT_X4R4G4B4: case D3DFMT_A8R3G3B2: case D3DFMT_A8P8: case D3DFMT_L16: case D3DFMT_A8L8: case D3DFMT_V8U8: case D3DFMT_R16F: return (uint64_t)w * h * 2;
    default: return (uint64_t)w * h * fmt_bpp(fmt);
    }
}
uint64_t chain_bytes(D3DFORMAT fmt, UINT w, UINT h, UINT levels) {
    uint64_t sum = 0;
    if (!levels) levels = 32;
    for (UINT i = 0; i < levels; ++i) {
        sum += level_bytes(fmt, w, h);
        if (w == 1 && h == 1) break;
        w = w > 1 ? w / 2 : 1; h = h > 1 ? h / 2 : 1;
    }
    return sum;
}

// ---- TEXTURE-MEM1: pagefile-backed managed texture shadow ----------------------
// The old SYSTEMMEM twin is correct but permanently maps every copy into this
// 32-bit process. Large packs can exhaust address ranges even with ample
// physical/VRAM. Managed=paged keeps the CPU copy in
// a pagefile section object and maps only the mip currently being locked.
uint32_t fmt_block_bytes(D3DFORMAT fmt) {
    switch ((DWORD)fmt) {
    case D3DFMT_DXT1: return 8;
    case D3DFMT_DXT2: case D3DFMT_DXT3: case D3DFMT_DXT4: case D3DFMT_DXT5:
    case MAKEFOURCC('A','T','I','2'): return 16;
    default: return 0;
    }
}
uint32_t fmt_bpp(D3DFORMAT fmt) {
    switch ((DWORD)fmt) {
    case D3DFMT_L8: case D3DFMT_A8: case D3DFMT_P8: case D3DFMT_A4L4: return 1;
    case D3DFMT_R8G8B8: return 3;
    case D3DFMT_R5G6B5: case D3DFMT_A1R5G5B5: case D3DFMT_X1R5G5B5: case D3DFMT_A4R4G4B4:
    case D3DFMT_X4R4G4B4: case D3DFMT_A8R3G3B2: case D3DFMT_A8P8: case D3DFMT_L16: case D3DFMT_A8L8: case D3DFMT_V8U8: case D3DFMT_R16F: return 2;
    case D3DFMT_A16B16G16R16F: case D3DFMT_A16B16G16R16: case D3DFMT_G32R32F: return 8;
    case D3DFMT_A32B32G32R32F: return 16;
    case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: case D3DFMT_A8B8G8R8: case D3DFMT_X8B8G8R8:
    case D3DFMT_A2R10G10B10: case D3DFMT_A2B10G10R10: case D3DFMT_G16R16: case D3DFMT_V16U16:
    case D3DFMT_Q8W8V8U8: case D3DFMT_X8L8V8U8: case D3DFMT_A2W10V10U10: case D3DFMT_R32F: case D3DFMT_G16R16F: return 4;
    case D3DFMT_R3G3B2: return 1;
    case D3DFMT_Q16W16V16U16: return 8;
    default: return 0;
    }
}
void level_layout(D3DFORMAT fmt, UINT w, UINT h, UINT level, UINT* outW, UINT* outH, UINT* pitch, UINT* rows, uint64_t* off, uint64_t* bytes) {
    uint64_t o = 0;
    for (UINT i = 0; i < level; ++i) { o += level_bytes(fmt, w, h); w = w > 1 ? w/2 : 1; h = h > 1 ? h/2 : 1; }
    const uint32_t block = fmt_block_bytes(fmt);
    const UINT p = block ? ((w + 3) / 4) * block : w * fmt_bpp(fmt);
    const UINT r = block ? ((h + 3) / 4) : h;
    if (outW) *outW = w; if (outH) *outH = h; if (pitch) *pitch = p; if (rows) *rows = r;
    if (off) *off = o; if (bytes) *bytes = (uint64_t)p * r;
}
uint64_t paged_total_bytes(ShadowKind kind, D3DFORMAT fmt, UINT w, UINT h, UINT levels) {
    const uint64_t face = chain_bytes(fmt, w, h, levels);
    return kind == ShadowKind::PagedCube ? face * 6 : face;
}
HANDLE make_page_section(uint64_t bytes) {
    if (!bytes) bytes = 1;
    return CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, (DWORD)(bytes >> 32), (DWORD)bytes, nullptr);
}
bool paged_register(void* real, ShadowKind kind, UINT w, UINT h, UINT levels, D3DFORMAT fmt) {
    DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 1,
        "device/paged: TEXTURE-MEM1.2 ACTIVE - pagefile-backed managed textures; concurrent mip/face locks supported; staging cache target 128 MiB (one larger texture may exceed it)");
    if (!fmt_block_bytes(fmt) && !fmt_bpp(fmt)) {
        ++g_shadowFailed; DVR_ERROR("device/paged: unsupported texture layout fmt=%u", (unsigned)fmt); return false;
    }
    const uint64_t bytes = paged_total_bytes(kind, fmt, w, h, levels);
    HANDLE sec = make_page_section(bytes);
    if (!sec) {
        ++g_shadowFailed;
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Error, 5,
            "device/paged: CreateFileMapping for texture %p %.1f MB refused winerr=%lu", real, bytes/1048576.0, GetLastError());
        return false;
    }
    EnterCriticalSection(&g_cs);
    const bool ok = map_put(real, nullptr);
    Ent* e = ok ? map_find(real) : nullptr;
    if (e) { e->section=sec; e->sectionBytes=bytes; e->kind=(uint8_t)kind; e->w=w; e->h=h; e->d=1; e->levels=levels; e->fmt=fmt; ++g_shadowMade; ++g_pagedSections; g_pagedBytes += bytes; }
    LeaveCriticalSection(&g_cs);
    if (!e) { CloseHandle(sec); ++g_shadowFailed; return false; }
    return true;
}
// TEXTURE-MEM1.1: bounded FULL-CHAIN staging cache.
//
// The community patch notes describe an earlier one-level SYSTEMMEM texture whose base dimensions were the
// locked mip dimensions. That looks equivalent, but some D3D9 drivers reject
// UpdateSurface from that standalone surface into a sublevel of a larger
// compressed texture (TEST1 failed immediately on level 6 with 0x8876017c).
// The proven Managed=shadow path uploads from the SAME mip level of a full
// SYSTEMMEM twin. Mirror that exact topology here, but keep only a small global
// cache instead of one permanent twin per game texture.
struct StageEnt {
    IDirect3DBaseTexture9* tex;
    ShadowKind kind;
    UINT w, h, levels;
    D3DFORMAT fmt;
    uint64_t bytes;
    ULONGLONG stamp;
};
constexpr int kStageCount = 8;
constexpr uint64_t kStageCapBytes = 128ull * 1024 * 1024;
StageEnt g_stage[kStageCount] = {};
uint64_t g_stageBytes = 0;

void stage_drop(int i) {
    if (i < 0 || i >= kStageCount || !g_stage[i].tex) return;
    g_stage[i].tex->Release();
    if (g_stageBytes >= g_stage[i].bytes) g_stageBytes -= g_stage[i].bytes;
    g_stage[i] = {};
}

IDirect3DBaseTexture9* stage_get(ShadowKind kind, UINT w, UINT h, UINT levels, D3DFORMAT fmt) {
    const ULONGLONG now = GetTickCount64();
    for (int i = 0; i < kStageCount; ++i) {
        if (g_stage[i].tex && g_stage[i].kind == kind && g_stage[i].w == w && g_stage[i].h == h &&
            g_stage[i].levels == levels && g_stage[i].fmt == fmt) {
            g_stage[i].stamp = now;
            ++g_stageHits;
            return g_stage[i].tex;
        }
    }

    ++g_stageMisses;
    const uint64_t need = paged_total_bytes(kind, fmt, w, h, levels);
    // Keep the cache bounded. A single unusually large texture is allowed, but
    // all older staging entries are discarded before creating it.
    while (g_stageBytes && (g_stageBytes + need > kStageCapBytes)) {
        int oldest = -1; ULONGLONG os = ~0ull;
        for (int i = 0; i < kStageCount; ++i)
            if (g_stage[i].tex && g_stage[i].stamp < os) { oldest = i; os = g_stage[i].stamp; }
        if (oldest < 0) break;
        stage_drop(oldest);
    }

    int slot = -1;
    for (int i = 0; i < kStageCount; ++i) if (!g_stage[i].tex) { slot = i; break; }
    if (slot < 0) {
        ULONGLONG os = ~0ull;
        for (int i = 0; i < kStageCount; ++i) if (g_stage[i].stamp < os) { slot = i; os = g_stage[i].stamp; }
        if (slot >= 0) stage_drop(slot);
    }
    if (slot < 0) return nullptr;

    IDirect3DBaseTexture9* base = nullptr;
    HRESULT hr = D3DERR_INVALIDCALL;
    if (kind == ShadowKind::PagedCube) {
        IDirect3DCubeTexture9* t = nullptr;
        hr = g_dev->CreateCubeTexture(w, levels, 0, fmt, D3DPOOL_SYSTEMMEM, &t, nullptr);
        base = t;
    } else {
        IDirect3DTexture9* t = nullptr;
        hr = g_dev->CreateTexture(w, h, levels, 0, fmt, D3DPOOL_SYSTEMMEM, &t, nullptr);
        base = t;
    }
    if (FAILED(hr) || !base) {
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Error, 8,
            "device/paged: full-chain staging create %ux%u levels=%u fmt=%u kind=%u refused 0x%08lx",
            w, h, levels, (unsigned)fmt, (unsigned)kind, (unsigned long)hr);
        if (base) base->Release();
        return nullptr;
    }

    g_stage[slot].tex = base;
    g_stage[slot].kind = kind;
    g_stage[slot].w = w;
    g_stage[slot].h = h;
    g_stage[slot].levels = levels;
    g_stage[slot].fmt = fmt;
    g_stage[slot].bytes = need;
    g_stage[slot].stamp = now;
    g_stageBytes += need;
    return base;
}

HRESULT paged_upload_level(Ent& snap) {
    if (!g_dev || !snap.activeLevelBase || snap.activeLevel < 0) return D3DERR_INVALIDCALL;
    const ShadowKind kind = (ShadowKind)snap.kind;
    if (kind != ShadowKind::Paged2D && kind != ShadowKind::PagedCube) return D3DERR_INVALIDCALL;

    UINT lw=0, lh=0, pitch=0, rows=0; uint64_t off=0, bytes=0;
    level_layout(snap.fmt, snap.w, snap.h, (UINT)snap.activeLevel, &lw, &lh, &pitch, &rows, &off, &bytes);

    EnterCriticalSection(&g_stageCs);
    IDirect3DBaseTexture9* base = stage_get(kind, snap.w, snap.h, snap.levels, snap.fmt);
    if (!base) { LeaveCriticalSection(&g_stageCs); return D3DERR_OUTOFVIDEOMEMORY; }

    D3DLOCKED_RECT tr = {};
    HRESULT hr = D3DERR_INVALIDCALL;
    if (kind == ShadowKind::PagedCube) {
        hr = ((IDirect3DCubeTexture9*)base)->LockRect((D3DCUBEMAP_FACES)snap.activeFace,
                                                      (UINT)snap.activeLevel, &tr, nullptr, 0);
    } else {
        hr = ((IDirect3DTexture9*)base)->LockRect((UINT)snap.activeLevel, &tr, nullptr, 0);
    }

    if (SUCCEEDED(hr)) {
        const BYTE* src = snap.activeLevelBase;
        BYTE* dst = (BYTE*)tr.pBits;
        const UINT copyPitch = (tr.pBits && tr.Pitch >= (INT)pitch) ? pitch : 0;
        for (UINT y = 0; y < rows; ++y)
            memcpy(dst + (size_t)y * tr.Pitch, src + (size_t)y * pitch, copyPitch);

        if (kind == ShadowKind::PagedCube)
            ((IDirect3DCubeTexture9*)base)->UnlockRect((D3DCUBEMAP_FACES)snap.activeFace, (UINT)snap.activeLevel);
        else
            ((IDirect3DTexture9*)base)->UnlockRect((UINT)snap.activeLevel);

        if (!copyPitch) { LeaveCriticalSection(&g_stageCs); return D3DERR_INVALIDCALL; }
        IDirect3DSurface9 *ss = nullptr, *ds = nullptr;
        if (kind == ShadowKind::PagedCube) {
            ((IDirect3DCubeTexture9*)base)->GetCubeMapSurface((D3DCUBEMAP_FACES)snap.activeFace,
                                                              (UINT)snap.activeLevel, &ss);
            ((IDirect3DCubeTexture9*)snap.real)->GetCubeMapSurface((D3DCUBEMAP_FACES)snap.activeFace,
                                                                   (UINT)snap.activeLevel, &ds);
        } else {
            ((IDirect3DTexture9*)base)->GetSurfaceLevel((UINT)snap.activeLevel, &ss);
            ((IDirect3DTexture9*)snap.real)->GetSurfaceLevel((UINT)snap.activeLevel, &ds);
        }
        if (ss && ds) hr = g_dev->UpdateSurface(ss, nullptr, ds, nullptr);
        else hr = D3DERR_INVALIDCALL;
        if (ss) ss->Release();
        if (ds) ds->Release();
    }

    LeaveCriticalSection(&g_stageCs);
    return hr;
}

} // namespace

void clear_staging() {
    if (!g_stageCsInit) return;
    EnterCriticalSection(&g_stageCs);
    for (int i=0; i<kStageCount; ++i) stage_drop(i);
    LeaveCriticalSection(&g_stageCs);
}

bool parse_managed(const char* s, Managed* out) {
    if (!s || !out) return false;
    for (int i = 0; i < 5; ++i) if (!_stricmp(s, kManagedNames[i])) { *out = (Managed)i; return true; }
    return false;
}
const char* managed_name(Managed m) { const int i = (int)m; return kManagedNames[(i >= 0 && i < 5) ? i : 0]; }

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
             : g_managed == Managed::Paged ? "DEFAULT with TEXTURE-MEM1 pagefile shadow (only the locked mip is mapped)"
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
// Failure-only diagnostic. No eviction of live CPU copies: mip streaming reads them.
void shadow_memory_failure(HRESULT hr) {
    static unsigned reports=0;if(reports++>=3)return;
    MEMORYSTATUSEX memory={};memory.dwLength=sizeof(memory);
    const bool known=GlobalMemoryStatusEx(&memory)!=FALSE;
    uint64_t freeBytes=0,largest=0,committed=0;uintptr_t cursor=0;
    MEMORY_BASIC_INFORMATION region={};
    while(VirtualQuery((void*)cursor,&region,sizeof(region))==sizeof(region)) {
        const uint64_t bytes=region.RegionSize;
        if(region.State==MEM_FREE){freeBytes+=bytes;if(bytes>largest)largest=bytes;}
        if(region.State==MEM_COMMIT)committed+=bytes;
        const uintptr_t next=(uintptr_t)region.BaseAddress+region.RegionSize;
        if(next<=cursor)break;cursor=next;
    }
    DVR_ERROR("device/shadow-memory: hr=0x%08lx virtualFree=%.1fMiB largestFree=%.1fMiB committed=%.1fMiB systemKnown=%d availableCommit=%.1fMiB physicalAvailable=%.1fMiB liveTwins=%d; allocation failure, no successful lock claimed",
        (unsigned long)hr,freeBytes/1048576.0,largest/1048576.0,committed/1048576.0,int(known),
        memory.ullAvailPageFile/1048576.0,memory.ullAvailPhys/1048576.0,g_mapCount);
}
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
    if ((g_managed != Managed::Shadow && g_managed != Managed::Paged) || !dev || !real) return;
    if (g_managed == Managed::Paged && (fmt_block_bytes(fmt) || fmt_bpp(fmt))) { paged_register(real, ShadowKind::Paged2D, w, h, real->GetLevelCount(), fmt); return; }
    IDirect3DTexture9* twin = nullptr;
    const HRESULT hr = dev->CreateTexture(w, h, levels, 0, fmt, D3DPOOL_SYSTEMMEM, &twin, nullptr);
    if (FAILED(hr) || !twin) {
        ++g_shadowFailed;
        shadow_memory_failure(hr);
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Error, 5,
                        "device/shadow: SYSTEMMEM twin for texture %p (%ux%u lv=%u fmt=%d) refused 0x%08lx - its locks "
                        "will FAIL", (void*)real, w, h, levels, (int)fmt, (unsigned long)hr);
        return;
    }
    shadow_put(real, twin, (uint64_t)w * h * 4);
    StreamBucket* b = stream_bucket();
    InterlockedIncrement(&b->creates);
    InterlockedExchangeAdd(&b->createKB, (LONG)(chain_bytes(fmt, w, h, levels) / 1024));
}
void shadow_register_cube(IDirect3DDevice9* dev, IDirect3DCubeTexture9* real, UINT edge, UINT levels, D3DFORMAT fmt) {
    if ((g_managed != Managed::Shadow && g_managed != Managed::Paged) || !dev || !real) return;
    if (g_managed == Managed::Paged && (fmt_block_bytes(fmt) || fmt_bpp(fmt))) { paged_register(real, ShadowKind::PagedCube, edge, edge, real->GetLevelCount(), fmt); return; }
    IDirect3DCubeTexture9* twin = nullptr;
    const HRESULT hr = dev->CreateCubeTexture(edge, levels, 0, fmt, D3DPOOL_SYSTEMMEM, &twin, nullptr);
    if (FAILED(hr) || !twin) {
        ++g_shadowFailed;
        shadow_memory_failure(hr);
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Error, 5,
                        "device/shadow: SYSTEMMEM twin for cube texture %p (%u lv=%u fmt=%d) refused 0x%08lx - its "
                        "locks will FAIL", (void*)real, edge, levels, (int)fmt, (unsigned long)hr);
        return;
    }
    shadow_put(real, twin, (uint64_t)edge * edge * 4 * 6);
    StreamBucket* b = stream_bucket();
    InterlockedIncrement(&b->creates);
    InterlockedExchangeAdd(&b->createKB, (LONG)(chain_bytes(fmt, edge, edge, levels) * 6 / 1024));
}
void shadow_register_volume(IDirect3DDevice9* dev, IDirect3DVolumeTexture9* real, UINT w, UINT h, UINT d, UINT levels, D3DFORMAT fmt) {
    // TEXTURE-MEM1 keeps volume textures on the proven legacy twin path: D3D9
    // has no UpdateSurface equivalent for IDirect3DVolume9 sublevels.
    if ((g_managed != Managed::Shadow && g_managed != Managed::Paged) || !dev || !real) return;
    IDirect3DVolumeTexture9* twin = nullptr;
    const HRESULT hr = dev->CreateVolumeTexture(w, h, d, levels, 0, fmt, D3DPOOL_SYSTEMMEM, &twin, nullptr);
    if (FAILED(hr) || !twin) {
        ++g_shadowFailed;
        shadow_memory_failure(hr);
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Error, 5,
                        "device/shadow: SYSTEMMEM twin for volume texture %p refused 0x%08lx - its locks will FAIL",
                        (void*)real, (unsigned long)hr);
        return;
    }
    shadow_put(real, twin, (uint64_t)w * h * d * 4);
}

// No native reference is retained. Consumers hold their own short GetTexture
// reference, then use this incarnation/write stamp to validate a content cache.
bool texture_stamp(void* real,uint64_t* serial,uint32_t* writes) {
    if(!g_csInit||!real||!serial||!writes)return false;
    EnterCriticalSection(&g_cs);Ent* e=map_find(real);
    const bool ok=e&&e->serial;
    if(ok){*serial=e->serial;*writes=e->updates;}
    LeaveCriticalSection(&g_cs);return ok;
}
bool paged_active() { return translating() && g_managed == Managed::Paged; }
bool shadow_tracked(void* real) {
    if (!g_csInit || g_mapCount==0) return false; EnterCriticalSection(&g_cs); const bool yes=map_find(real)!=nullptr; LeaveCriticalSection(&g_cs); return yes;
}
bool paged_dirty(void* real) {
    if (!paged_active() || !g_csInit) return false; EnterCriticalSection(&g_cs); Ent* e=map_find(real);
    const bool yes=e && ((ShadowKind)e->kind==ShadowKind::Paged2D || (ShadowKind)e->kind==ShadowKind::PagedCube); LeaveCriticalSection(&g_cs); return yes;
}
HRESULT paged_lock_rect(void* real, int level, int face, D3DLOCKED_RECT* lr, const RECT* rc, DWORD flags) {
    if (!paged_active() || !g_csInit) return S_FALSE;
    EnterCriticalSection(&g_cs);
    Ent* e = map_find(real);
    if (!e || ((ShadowKind)e->kind != ShadowKind::Paged2D && (ShadowKind)e->kind != ShadowKind::PagedCube)) {
        LeaveCriticalSection(&g_cs); return S_FALSE;
    }
    if (!lr || level < 0 || (UINT)level >= e->levels ||
        ((ShadowKind)e->kind == ShadowKind::PagedCube ? (face < 0 || face > 5) : face != -1) ||
        (flags & (D3DLOCK_DISCARD | D3DLOCK_NOOVERWRITE)) || paged_lock_find(real, level, face)) {
        LeaveCriticalSection(&g_cs); return D3DERR_INVALIDCALL;
    }
    UINT lw=0, lh=0, pitch=0, rows=0; uint64_t offset=0, bytes=0;
    level_layout(e->fmt, e->w, e->h, (UINT)level, &lw, &lh, &pitch, &rows, &offset, &bytes);
    const uint32_t block = fmt_block_bytes(e->fmt);
    if (rc && (rc->left < 0 || rc->top < 0 || rc->right <= rc->left || rc->bottom <= rc->top ||
        (UINT)rc->right > lw || (UINT)rc->bottom > lh ||
        (block && ((rc->left % 4) || (rc->top % 4) ||
            ((rc->right % 4) && (UINT)rc->right != lw) || ((rc->bottom % 4) && (UINT)rc->bottom != lh))))) {
        LeaveCriticalSection(&g_cs); return D3DERR_INVALIDCALL;
    }
    if ((ShadowKind)e->kind == ShadowKind::PagedCube)
        offset += chain_bytes(e->fmt, e->w, e->h, e->levels) * (uint64_t)face;
    SYSTEM_INFO si{}; GetSystemInfo(&si);
    const uint64_t gran = si.dwAllocationGranularity;
    const uint64_t aligned = offset - offset % gran;
    const uint64_t viewBytes = offset - aligned + bytes;
    if (!bytes || offset > e->sectionBytes || bytes > e->sectionBytes - offset ||
        viewBytes > SIZE_MAX || pitch > INT_MAX) {
        LeaveCriticalSection(&g_cs); return D3DERR_INVALIDCALL;
    }
    PagedLockEnt* le = paged_lock_alloc(real, level, face);
    if (!le) { LeaveCriticalSection(&g_cs); return D3DERR_OUTOFVIDEOMEMORY; }
    // Keep identity and the section alive under the map lock through publication.
    // Only an OS mapping occurs here, never a D3D call while holding this lock.
    LARGE_INTEGER start; QueryPerformanceCounter(&start);
    void* view = MapViewOfFile(e->section, FILE_MAP_ALL_ACCESS, (DWORD)(aligned >> 32), (DWORD)aligned, (SIZE_T)viewBytes);
    g_pagedMapUs += elapsed_us(start);
    if (!view) {
        const DWORD error = GetLastError(); ++g_pagedMapFailed; paged_lock_remove(le);
        LeaveCriticalSection(&g_cs);
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Error, 8,
            "device/paged: map texture %p level %d face %d bytes=%llu refused winerr=%lu", real, level, face, viewBytes, error);
        return D3DERR_OUTOFVIDEOMEMORY;
    }
    BYTE* base = (BYTE*)view + (SIZE_T)(offset - aligned);
    const size_t within = !rc ? 0 : block ? (size_t)(rc->top / 4) * pitch + (size_t)(rc->left / 4) * block
                                                   : (size_t)rc->top * pitch + (size_t)rc->left * fmt_bpp(e->fmt);
    le->view=view; le->levelBase=base; le->viewBytes=(SIZE_T)viewBytes; le->readOnly=(flags & D3DLOCK_READONLY) != 0;
    ++e->activeLocks; ++g_pagedLocks; ++g_pagedConcurrent;
    if (g_pagedConcurrent > g_pagedConcurrentMax) g_pagedConcurrentMax = g_pagedConcurrent;
    g_pagedMappedBytes += viewBytes;
    if (g_pagedMappedBytes > g_pagedMappedPeak) g_pagedMappedPeak = g_pagedMappedBytes;
    lr->Pitch=(INT)pitch; lr->pBits=base+within;
    LeaveCriticalSection(&g_cs);
    return S_OK;
}

HRESULT paged_unlock_rect(void* real, int level, int face) {
    if (!paged_active() || !g_csInit) return S_FALSE;
    Ent snap{}; PagedLockEnt lock{};
    EnterCriticalSection(&g_cs);
    Ent* e = map_find(real);
    if (!e || !e->section) { LeaveCriticalSection(&g_cs); return S_FALSE; }
    PagedLockEnt* le = paged_lock_find(real, level, face);
    if (!le || !le->view || le->unlocking) { LeaveCriticalSection(&g_cs); return D3DERR_INVALIDCALL; }
    le->unlocking = true;
    snap=*e; lock=*le;
    snap.activeView=lock.view; snap.activeLevelBase=lock.levelBase; snap.activeViewBytes=lock.viewBytes;
    snap.activeLevel=(int16_t)level; snap.activeFace=(int8_t)face; snap.activeReadOnly=lock.readOnly;
    LeaveCriticalSection(&g_cs);

    LARGE_INTEGER start; QueryPerformanceCounter(&start);
    const HRESULT hr = lock.readOnly ? S_OK : paged_upload_level(snap);
    const uint64_t uploadUs = lock.readOnly ? 0 : elapsed_us(start);
    // The caller owns the COM texture through UnlockRect. Keep the subresource
    // claimed until the upload completes, so a second lock cannot race its data.
    EnterCriticalSection(&g_cs);
    le=paged_lock_find(real,level,face);
    if (le && le->view==lock.view) {
        UnmapViewOfFile(lock.view); paged_lock_remove(le);
        --g_pagedConcurrent; g_pagedMappedBytes -= lock.viewBytes;
        e=map_find(real);
        if (e && e->section==snap.section) {
            if (e->activeLocks) --e->activeLocks;
            if (SUCCEEDED(hr) && !lock.readOnly) ++e->updates;
        }
    }
    if (lock.readOnly) ++g_shadowSkippedReadOnly;
    else {
        g_pagedUploadUs += uploadUs;
        if (uploadUs > g_pagedMaxUploadUs) g_pagedMaxUploadUs = uploadUs;
        if (SUCCEEDED(hr)) {
            ++g_shadowUpdates;
            UINT w=0,h=0,p=0,r=0; uint64_t off=0,bytes=0;
            level_layout(snap.fmt,snap.w,snap.h,(UINT)level,&w,&h,&p,&r,&off,&bytes);
            g_pagedUploadBytes += bytes;
            StreamBucket* b=stream_bucket(); InterlockedIncrement(&b->ups);
            InterlockedExchangeAdd(&b->upKB,(LONG)(bytes/1024));
            InterlockedExchangeAdd(&b->upUs,(LONG)uploadUs);
        } else ++g_shadowUpdateFailed;
    }
    LeaveCriticalSection(&g_cs);
    if (FAILED(hr)) DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Error, 8,
        "device/paged: upload texture %p level %d face %d refused 0x%08lx",real,level,face,(unsigned long)hr);
    return hr;
}

// VR-15: the lock hooks already had to look the twin up, so recording what
// KIND of lock this is rides along for free - no extra critical section and no
// extra hash probe on a path this game takes 60000 times a load. bit N of
// roMask says level N's most recent lock was READONLY.
IDirect3DBaseTexture9* shadow_twin_for_lock(void* real, int level, DWORD flags) {
    if (!g_csInit || g_mapCount == 0) return nullptr;
    EnterCriticalSection(&g_cs);
    Ent* e = map_find(real);
    IDirect3DBaseTexture9* t = nullptr;
    if (e) {
        t = e->twin;
        const uint16_t bit = (uint16_t)(1u << ((level >= 0 && level < 16) ? level : 15));
        if (flags & D3DLOCK_READONLY) e->roMask |= bit;
        else                          e->roMask = (uint16_t)(e->roMask & ~bit);
    }
    LeaveCriticalSection(&g_cs);
    return t;
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
    if (level < 0 || face == kNoLevelSurface) return false;
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
        LARGE_INTEGER t0, t1, f; QueryPerformanceCounter(&t0);
        const HRESULT hr = g_dev->UpdateSurface(src, nullptr, dst, nullptr);
        QueryPerformanceCounter(&t1); QueryPerformanceFrequency(&f);
        if (SUCCEEDED(hr)) {
            ++g_shadowLevelCopies; ok = true;
            D3DSURFACE_DESC d;
            StreamBucket* b = stream_bucket();
            InterlockedIncrement(&b->ups);
            if (SUCCEEDED(dst->GetDesc(&d))) InterlockedExchangeAdd(&b->upKB, (LONG)((level_bytes(d.Format, d.Width, d.Height) + 1023) / 1024));
            InterlockedExchangeAdd(&b->upUs, (LONG)((t1.QuadPart - t0.QuadPart) * 1000000 / f.QuadPart));
        }
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
    // ONE critical section for the whole lookup: the twin, whether this level's
    // last lock was READONLY, and whether this texture has already had an
    // UpdateSurface refused. The Ent pointer is kept and its counters are
    // written after the lock is dropped - a texture released concurrently can
    // only make a COUNTER wrong, never touch freed memory (the entries live in
    // a static array), and holding a lock across a D3D call is worse.
    Ent* e = nullptr;
    IDirect3DBaseTexture9* twin = nullptr;
    bool readOnly = false, surfaceRefused = false;
    if (g_csInit && g_mapCount) {
        EnterCriticalSection(&g_cs);
        e = map_find(real);
        if (e) {
            twin = e->twin;
            const int b = (level >= 0 && level < 16) ? level : 15;
            readOnly = (e->roMask & (uint16_t)(1u << b)) != 0;
            surfaceRefused = e->surfaceRefused != 0;
        }
        LeaveCriticalSection(&g_cs);
    }
    if (!twin || !g_dev) return;
    if (level > g_shadowMaxLevelSeen) g_shadowMaxLevelSeen = level;
    if (level > 0) ++g_shadowSubLevelUnlocks;

    // A READONLY lock did not write anything, so there is nothing to push. The
    // old code pushed on EVERY unlock, and this game takes 12408 READONLY locks
    // on MANAGED textures in one load (its mip streaming reads the old texture
    // to fill the new one). Every one of those was a whole-texture GPU copy for
    // no change at all. This is a pure removal, independent of the lever.
    if (readOnly) { ++g_shadowSkippedReadOnly; return; }

    // The lever: push exactly the level that was written. A texture whose
    // format has already refused UpdateSurface once is not asked again - that
    // refusal is a property of the format, and retrying it every unlock means
    // paying for the failure AND the fallback, forever.
    if (g_fullCopy && !surfaceRefused) {
        if (update_one_level(real, twin, level, face)) { if (e) ++e->updates; return; }
        if (level >= 0 && face != kNoLevelSurface && e) e->surfaceRefused = 1;
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
        if (e) ++e->updates;
    }
}

void shadow_released(void* real) {
    if (!g_csInit || g_mapCount == 0) return;
    EnterCriticalSection(&g_cs);
    Ent* e = map_find(real);
    IDirect3DBaseTexture9* twin = e ? e->twin : nullptr; HANDLE sec=e?e->section:nullptr; const uint64_t secBytes=e?e->sectionBytes:0;
    // Normally COM cannot release a texture while one of its subresources is
    // locked. Still clean any outstanding MEM1.2 views defensively so a bad
    // engine path cannot strand VA or a section handle.
    for(int i=0; e && e->activeLocks && i<kPagedLockMap; ++i){
        PagedLockEnt& le=g_pagedLockMap[i];
        if(le.real==real){
            if(le.view) { UnmapViewOfFile(le.view); g_pagedMappedBytes -= le.viewBytes; }
            paged_lock_remove(&le); if(g_pagedConcurrent)--g_pagedConcurrent; --e->activeLocks;
        }
    }
    if (e && e->updates == 0) ++g_shadowDroppedNeverUpdated;
    if (e) map_remove(e);
    if(sec){CloseHandle(sec); if(g_pagedSections) --g_pagedSections; if(g_pagedBytes>=secBytes) g_pagedBytes-=secBytes;}
    LeaveCriticalSection(&g_cs);
    if (twin) twin->Release();
    if(twin||sec){ ++g_shadowReleased; InterlockedIncrement(&stream_bucket()->releases); }
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

bool shadow_active() { return translating() && (g_managed == Managed::Shadow || g_managed == Managed::Paged); }

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
// A plain counter read - no map walk, so a caller on the present thread can
// poll it. shadow_population() is the one that costs 32768 slots.
uint32_t dropped_never_updated() { return g_shadowDroppedNeverUpdated; }
uint32_t skipped_readonly() { return g_shadowSkippedReadOnly; }
void shadow_levels(uint32_t* subLevelUnlocks, int* maxLevel, uint32_t* copies, uint32_t* copyFailed, HRESULT* firstHr) {
    if (subLevelUnlocks) *subLevelUnlocks = g_shadowSubLevelUnlocks;
    if (maxLevel) *maxLevel = g_shadowMaxLevelSeen;
    if (copies) *copies = g_shadowLevelCopies;
    if (copyFailed) *copyFailed = g_shadowLevelCopyFailed;
    if (firstHr) *firstHr = g_shadowLevelFirstHr;
}

void log_status() {
    DVR_INFO("device: [Device] Ex=%d Managed=%s | Direct3DCreate9 calls %d (Ex objects %d) | device %s (%s) | "
             "translated tex=%u (dynamic %u) buf=%u | shadow backings made=%u failed=%u live=%d tombstones=%d of %d slots "
             "(%.1f MB asked, uncompressed) updates=%u failed=%u released=%u mapFull=%u",
             g_exWanted ? 1 : 0, managed_name(g_managed), g_createCalls, g_exCount,
             !g_deviceLive ? "not created yet" : g_deviceIsEx ? "IS 9Ex" : "is NOT 9Ex", g_route, g_texTranslated,
             g_texDynamic, g_bufTranslated, g_shadowMade, g_shadowFailed, g_mapCount, g_mapTombs, kMap,
             g_shadowBytes / 1048576.0, g_shadowUpdates, g_shadowUpdateFailed, g_shadowReleased, g_mapFull);
    if (g_managed == Managed::Paged) {
        // Staging -> map matches uploads/releases; snapshot under both locks so
        // the x86 64-bit timing counters cannot tear during streaming.
        if (g_stageCsInit) EnterCriticalSection(&g_stageCs);
        if (g_csInit) EnterCriticalSection(&g_cs);
        DVR_INFO("device/paged: TEXTURE-MEM1.2 sections live=%u backing=%.1f MB (pagefile, not permanent VA) locks=%u concurrent=%u maxConcurrent=%u lockMapFull=%u mapFailures=%u fullChainStaging=%.1f MB/%d slots",
                 g_pagedSections, g_pagedBytes/1048576.0, g_pagedLocks, g_pagedConcurrent, g_pagedConcurrentMax, g_pagedLockMapFull, g_pagedMapFailed, g_stageBytes/1048576.0, kStageCount);
        MEMORYSTATUSEX memory{}; memory.dwLength=sizeof(memory); GlobalMemoryStatusEx(&memory);
        DVR_INFO("device/paged-cost: mapped=%.2fMiB peak=%.2fMiB mapTotal=%.2fms upload=%.2fMiB/%.2fms maxUpload=%.2fms stageHits=%u misses=%u availableCommit=%.1fMiB physicalAvailable=%.1fMiB (faults or disk IO not inferred from these totals)",
            g_pagedMappedBytes/1048576.0, g_pagedMappedPeak/1048576.0, g_pagedMapUs/1000.0,
            g_pagedUploadBytes/1048576.0, g_pagedUploadUs/1000.0, g_pagedMaxUploadUs/1000.0,
            g_stageHits,g_stageMisses,memory.ullAvailPageFile/1048576.0,memory.ullAvailPhys/1048576.0);
        if (g_csInit) LeaveCriticalSection(&g_cs);
        if (g_stageCsInit) LeaveCriticalSection(&g_stageCs);
    }
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
    w.kv("pagedSections", (unsigned long)g_pagedSections);
    w.kv("pagedBackingMB", (double)(g_pagedBytes / 1048576.0));
    w.kv("pagedLocks", (unsigned long)g_pagedLocks);
    w.kv("pagedMapFailed", (unsigned long)g_pagedMapFailed);
    w.kv("pagedStageMB", (double)(g_stageBytes / 1048576.0));
    int live = 0, never = 0; uint32_t dropped = 0;
    shadow_population(&live, &never, &dropped);
    w.kv("shadowLiveNeverUpdated", never);
    w.kv("shadowDroppedNeverUpdated", (unsigned long)dropped);
}


// PERF (2026-09-18): the last `n` 100 ms buckets, oldest first, as one line.
// Called at every frame gap: uploads that arrive in a burst right before a
// stall are the texture-streaming signature; an empty ring clears streaming.
void stream_log_recent(const char* why, int n) {
    // Twenty _snprintf calls into 1400 bytes: not paid for a line that will not print.
    if (!::dvr::log::enabled(DVR_CAT, ::dvr::log::Level::Info)) return;
    if (n > kStreamBuckets - 1) n = kStreamBuckets - 1;
    const LONG now = (LONG)(GetTickCount64() / 100);
    char line[1400]; int o = 0; LONG tu = 0, tc = 0, tr = 0, tkb = 0, tckb = 0, tus = 0;
    for (int k = n - 1; k >= 0 && o < (int)sizeof(line) - 64; --k) {
        const StreamBucket& b = g_stream[(now - k) % kStreamBuckets];
        const bool live = b.slot == now - k;
        const LONG u = live ? b.ups : 0, kb = live ? b.upKB : 0, c = live ? b.creates : 0, ckb = live ? b.createKB : 0;
        const LONG r = live ? b.releases : 0, us = live ? b.upUs : 0;
        tu += u; tc += c; tr += r; tkb += kb; tckb += ckb; tus += us;
        o += _snprintf(line + o, sizeof(line) - o, " %.1f/%.1f", kb / 1024.0, ckb / 1024.0);
    }
    line[sizeof(line) - 1] = 0;
    DVR_INFO("device/stream (%s): last %d x 100 ms, oldest first, uploaded/created MB:%s | totals uploads %ld (%.1f MB, %.1f ms CPU "
             "in UpdateSurface) creates %ld (%.1f MB) releases %ld. A burst of MB right before a stall is texture streaming; "
             "zeros clear it.", why, n, line, (long)tu, tkb / 1024.0, tus / 1000.0, (long)tc, tckb / 1024.0, (long)tr);
}
} // namespace dvr::d3d9ex
