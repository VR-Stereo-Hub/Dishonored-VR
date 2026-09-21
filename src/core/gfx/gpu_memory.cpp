// core/gfx/gpu_memory.cpp - see gpu_memory.h.
#define DVR_CAT ::dvr::log::Cat::perf
#include "core/gfx/gpu_memory.h"
#include "core/gfx/d3d9ex.h"
#include "core/util/log.h"

#include <dxgi1_4.h>
#include <psapi.h>
#include <stdint.h>
#include <atomic>

namespace dvr::gpu_memory {
namespace {
IDXGIAdapter3* g_adapter = nullptr;
bool g_tried = false, g_refused = false;
ULONGLONG g_nextBeat = 0;
// VR-160: the sampler thread. The ring is written by it and read by log_now
// (any thread), so both take g_lock; nothing else is shared.
std::atomic<bool> g_on{true}, g_threadUp{false};
SRWLOCK g_lock = SRWLOCK_INIT;
std::atomic<uint32_t> g_sampleUs{0}, g_sampleMaxUs{0};
const DWORD kPeriodMs = 1000;
struct Sample { ULONGLONG ms; uint64_t localUse, localBudget, nonLocalUse, nonLocalBudget, priv, largestFree; };
constexpr int kRing = 16;
Sample g_ring[kRing];
int g_n = 0;
uint64_t g_lastNonLocal = 0;

void find_adapter() {
    g_tried = true;
    LUID want = {};
    if (!dvr::d3d9ex::adapter_luid(&want)) { g_tried = false; return; }   // not known yet: retry later
    HMODULE dxgi = LoadLibraryA("dxgi.dll");
    typedef HRESULT (WINAPI *PFN)(REFIID, void**);
    PFN mk = dxgi ? (PFN)GetProcAddress(dxgi, "CreateDXGIFactory1") : nullptr;
    IDXGIFactory1* fac = nullptr;
    if (!mk || FAILED(mk(__uuidof(IDXGIFactory1), (void**)&fac)) || !fac) {
        g_refused = true; DVR_WARN("gpumem: no DXGI factory - video memory is not sampled"); return;
    }
    IDXGIAdapter1* a = nullptr;
    for (UINT i = 0; fac->EnumAdapters1(i, &a) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 d;
        if (SUCCEEDED(a->GetDesc1(&d)) && d.AdapterLuid.LowPart == want.LowPart && d.AdapterLuid.HighPart == want.HighPart) {
            if (FAILED(a->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&g_adapter))) g_adapter = nullptr;
            a->Release();
            break;
        }
        a->Release();
    }
    fac->Release();
    if (!g_adapter) { g_refused = true; DVR_WARN("gpumem: no IDXGIAdapter3 for the D3D9 adapter LUID %08lx-%08lx - not sampled",
                                                 (unsigned long)want.HighPart, (unsigned long)want.LowPart); return; }
    DVR_INFO("gpumem: sampling the D3D9 adapter (LUID %08lx-%08lx) at 1 Hz: LOCAL = VRAM, NON_LOCAL = system memory the GPU "
             "reads over the bus. Usage at or over budget means the OS is paging this process's allocations. Sampled on "
             "its own low-priority thread, NOT the present thread (`gpumem off` stops it).",
             (unsigned long)want.HighPart, (unsigned long)want.LowPart);
}

bool sample(Sample* s) {
    s->ms = GetTickCount64();
    DXGI_QUERY_VIDEO_MEMORY_INFO l = {}, n = {};
    if (!g_adapter || FAILED(g_adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &l)) ||
        FAILED(g_adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &n))) return false;
    s->localUse = l.CurrentUsage; s->localBudget = l.Budget; s->nonLocalUse = n.CurrentUsage; s->nonLocalBudget = n.Budget;
    PROCESS_MEMORY_COUNTERS_EX pm = {}; pm.cb = sizeof(pm);
    s->priv = K32GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pm, sizeof(pm)) ? pm.PrivateUsage : 0;
    // Largest free range below 4 GB: a 32-bit process runs out of ADDRESS
    // space long before it runs out of memory. One VirtualQuery walk, 1 Hz.
    uint64_t largest = 0; uintptr_t p = 0; MEMORY_BASIC_INFORMATION mbi;
    while (VirtualQuery((void*)p, &mbi, sizeof(mbi)) == sizeof(mbi)) {
        if (mbi.State == MEM_FREE && mbi.RegionSize > largest) largest = mbi.RegionSize;
        const uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (next <= p) break; p = next;
    }
    s->largestFree = largest;
    return true;
}
inline double mb(uint64_t v) { return v / 1048576.0; }
} // namespace

// One sample, on the sampler thread. Times itself: an instrument that costs
// 21 ms must say so, and this one ran unnoticed on the present thread.
static void sample_once() {
    if (g_refused) return;
    const ULONGLONG now = GetTickCount64();
    if (!g_tried) find_adapter();
    if (!g_adapter) return;
    LARGE_INTEGER f, t0, t1;
    QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t0);
    Sample s;
    if (!sample(&s)) return;
    QueryPerformanceCounter(&t1);
    const uint32_t us = f.QuadPart ? (uint32_t)((t1.QuadPart - t0.QuadPart) * 1000000 / f.QuadPart) : 0;
    g_sampleUs = us;
    if (us > g_sampleMaxUs) g_sampleMaxUs = us;
    AcquireSRWLockExclusive(&g_lock);
    g_ring[g_n % kRing] = s; ++g_n;
    ReleaseSRWLockExclusive(&g_lock);
    // A jump in NON_LOCAL usage is allocations moving out of VRAM (or being
    // created there): log it the moment it happens.
    const int64_t dn = (int64_t)s.nonLocalUse - (int64_t)g_lastNonLocal;
    if (g_lastNonLocal && (dn > 64ll << 20 || dn < -(64ll << 20)))
        DVR_INFO("gpumem: NON_LOCAL usage moved %+.0f MB to %.0f MB (VRAM %.0f of %.0f MB budget)", dn / 1048576.0,
                 mb(s.nonLocalUse), mb(s.localUse), mb(s.localBudget));
    g_lastNonLocal = s.nonLocalUse;
    if (now >= g_nextBeat) { g_nextBeat = now + 10000; log_now("beat"); }
}

static DWORD WINAPI sampler_thread(void*) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    for (;;) {
        Sleep(kPeriodMs);
        if (g_on) sample_once();
    }
}

void set_enabled(bool on) {
    if (on == g_on) return;
    g_on = on;
    DVR_INFO("gpumem: sampling %s (%s)", on ? "ON" : "off",
             on ? "1 Hz on its own low-priority thread; the last sample's cost is on the gpumem line"
                : "the thread idles; gap and stand-up lines print the last samples taken");
}
bool enabled() { return g_on; }

// Present thread, every present. It must stay this cheap: two atomic loads.
void tick() {
    if (!g_on || g_threadUp) return;
    if (g_threadUp.exchange(true)) return;
    HANDLE h = CreateThread(nullptr, 0, sampler_thread, nullptr, 0, nullptr);
    if (h) CloseHandle(h);
    else { g_refused = true; DVR_WARN("gpumem: could not start the sampler thread (%lu) - not sampled", GetLastError()); }
}

void log_now(const char* why) {
    if (!g_adapter) return;
    if (!::dvr::log::enabled(DVR_CAT, ::dvr::log::Level::Info)) return;
    // A snapshot under the lock: the sampler thread writes the ring.
    Sample ring[kRing]; int total;
    AcquireSRWLockShared(&g_lock);
    memcpy(ring, g_ring, sizeof(ring)); total = g_n;
    ReleaseSRWLockShared(&g_lock);
    if (!total) return;
    const Sample& c = ring[(total - 1) % kRing];
    uint64_t lmin = ~0ull, lmax = 0, nmin = ~0ull, nmax = 0, fmin = ~0ull;
    const int k = total < kRing ? total : kRing;
    for (int i = 0; i < k; ++i) {
        const Sample& s = ring[i];
        if (s.localUse < lmin) lmin = s.localUse;
        if (s.localUse > lmax) lmax = s.localUse;
        if (s.nonLocalUse < nmin) nmin = s.nonLocalUse;
        if (s.nonLocalUse > nmax) nmax = s.nonLocalUse;
        if (s.largestFree < fmin) fmin = s.largestFree;
    }
    const double spanS = k * (kPeriodMs / 1000.0);
    DVR_INFO("gpumem (%s): VRAM %.0f / %.0f MB budget (%.0f%%; last %.0f s %.0f..%.0f) | system-backed %.0f / %.0f MB (last %.0f s "
             "%.0f..%.0f) | process private %.0f MB, largest free address range %.1f MB (min %.1f in %.0f s). Over-budget VRAM or a "
             "moving system-backed figure means paging; a small free range means 32-bit address pressure. | sampler %s, the "
             "last sample cost %.1f ms (max %.1f) on its OWN thread, sample age %.1f s",
             why, mb(c.localUse), mb(c.localBudget), c.localBudget ? 100.0 * c.localUse / c.localBudget : 0.0, spanS, mb(lmin),
             mb(lmax), mb(c.nonLocalUse), mb(c.nonLocalBudget), spanS, mb(nmin), mb(nmax), mb(c.priv), mb(c.largestFree), mb(fmin),
             spanS, g_on ? "ON" : "off", g_sampleUs.load() / 1000.0, g_sampleMaxUs.load() / 1000.0,
             (GetTickCount64() - c.ms) / 1000.0);
}
} // namespace dvr::gpu_memory
