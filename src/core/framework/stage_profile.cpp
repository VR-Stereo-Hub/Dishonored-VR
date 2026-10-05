// core/framework/stage_profile.cpp - see stage_profile.h.
#include "core/framework/stage_profile.h"
#include "core/util/log.h"
#include <windows.h>
#include <atomic>
#include <cstring>
#include <cstdio>
#include <algorithm>
namespace dvr::stageprof {
unsigned g_draws = 0;
namespace {
std::atomic<bool> g_on{false}, g_gpuOn{false}, g_restart{false};
constexpr int kStages = 192, kStack = 24, kQueries = 2048, kPending = 1024, kGpuDepth = 2;
struct Stage {
    wchar_t name[48]; uint32_t hash; int depth;
    uint64_t calls, draws, gpuSamples, gpuTicks; int64_t cpuTicks;
};
Stage g_stage[kStages]; int g_n = 0;
struct Open { int stage; int64_t t0; unsigned draws0; int qBegin; };
Open g_stack[kStack]; int g_depth = 0;
DWORD g_tid = 0;
uint64_t g_overflowDepth = 0, g_overflowTable = 0, g_foreign = 0, g_unbalanced = 0;
// GPU side
IDirect3DDevice9* g_dev = nullptr;
IDirect3DQuery9* g_q[kQueries] = {};
bool g_qBusy[kQueries] = {};
int g_qNext = 0;
IDirect3DQuery9* g_freqQ = nullptr; uint64_t g_freq = 0;
struct Pending { int stage, qb, qe; };
Pending g_pending[kPending]; int g_pHead = 0, g_pCount = 0;
uint64_t g_gpuSkipped = 0, g_gpuRefused = 0;
bool g_gpuFailed = false;
int64_t g_qpf = 0, g_windowT0 = 0;

int64_t now() { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; }
uint32_t hash_of(const wchar_t* s) { uint32_t h = 2166136261u; for (; *s; ++s) { h ^= (uint16_t)*s; h *= 16777619u; } return h ? h : 1; }

int stage_for(const wchar_t* name, int depth) {
    wchar_t key[48]; int i = 0;
    for (; name && name[i] && i < 47; ++i) key[i] = name[i];
    key[i] = 0;
    const uint32_t h = hash_of(key);
    for (int k = 0; k < g_n; ++k) if (g_stage[k].hash == h && !wcscmp(g_stage[k].name, key)) return k;
    if (g_n >= kStages) { ++g_overflowTable; return -1; }
    Stage& s = g_stage[g_n]; memset(&s, 0, sizeof(s));
    wcscpy_s(s.name, key); s.hash = h; s.depth = depth;
    return g_n++;
}

// One timestamp query, issued now. -1 when GPU timing is off, unavailable or the ring is full.
int stamp() {
    if (!g_gpuOn.load(std::memory_order_relaxed) || g_gpuFailed || !g_dev) return -1;
    for (int tries = 0; tries < 8; ++tries) {
        const int i = g_qNext; g_qNext = (g_qNext + 1) % kQueries;
        if (g_qBusy[i]) continue;
        if (!g_q[i] && FAILED(g_dev->CreateQuery(D3DQUERYTYPE_TIMESTAMP, &g_q[i]))) {
            g_q[i] = nullptr; g_gpuFailed = true; ++g_gpuRefused;
            DVR_LOG(::dvr::log::Cat::perf, ::dvr::log::Level::Warn,
                    "perf/stages: this device refuses timestamp queries - GPU time per stage is unavailable, CPU time and draws continue");
            return -1;
        }
        if (FAILED(g_q[i]->Issue(D3DISSUE_END))) { ++g_gpuRefused; return -1; }
        g_qBusy[i] = true;
        return i;
    }
    ++g_gpuSkipped;
    return -1;
}
void resolve() {
    if (!g_freq && g_dev && !g_gpuFailed) {
        if (!g_freqQ && SUCCEEDED(g_dev->CreateQuery(D3DQUERYTYPE_TIMESTAMPFREQ, &g_freqQ)) && g_freqQ) g_freqQ->Issue(D3DISSUE_END);
        uint64_t f = 0;
        if (g_freqQ && g_freqQ->GetData(&f, sizeof(f), 0) == S_OK && f) g_freq = f;
    }
    while (g_pCount) {
        Pending& p = g_pending[g_pHead];
        uint64_t b = 0, e = 0;
        if (!g_q[p.qb] || !g_q[p.qe]) { g_pHead = (g_pHead + 1) % kPending; --g_pCount; continue; }
        if (g_q[p.qb]->GetData(&b, sizeof(b), 0) != S_OK || g_q[p.qe]->GetData(&e, sizeof(e), 0) != S_OK) break;   // in order: stop at the first not ready
        if (p.stage >= 0 && p.stage < g_n && e >= b) { g_stage[p.stage].gpuTicks += e - b; ++g_stage[p.stage].gpuSamples; }
        g_qBusy[p.qb] = g_qBusy[p.qe] = false;
        g_pHead = (g_pHead + 1) % kPending; --g_pCount;
    }
}
void report(int64_t t) {
    const double s = g_qpf ? double(t - g_windowT0) / g_qpf : 0;
    if (s <= 0) return;
    resolve();
    int order[kStages];
    for (int i = 0; i < g_n; ++i) order[i] = i;
    std::sort(order, order + g_n, [](int a, int b) {
        if (g_stage[a].gpuTicks != g_stage[b].gpuTicks) return g_stage[a].gpuTicks > g_stage[b].gpuTicks;
        return g_stage[a].cpuTicks > g_stage[b].cpuTicks;
    });
    uint64_t top = 0;
    for (int i = 0; i < g_n; ++i) if (g_stage[i].depth == 0) top += g_stage[i].calls;
    DVR_LOG(::dvr::log::Cat::perf, ::dvr::log::Level::Info,
        "perf/stages: %.1f s window, %d stage names, %.0f top-level stage calls/s | times are INCLUSIVE ms per SECOND (divide by the pair rate "
        "on the `stereo: beat` line for ms per pair; a parent contains its children, d = depth) | gpu: %s, clock %llu ticks/s, %d sample(s) "
        "still pending, %llu skipped (query ring full), %llu refused | dropped: %llu deeper than %d, %llu past the %d-name table, %llu from "
        "another thread, %llu unbalanced ends",
        s, g_n, top / s, g_gpuOn.load() ? (g_gpuFailed ? "REFUSED by the device" : "on") : "off (`stages gpu on`)",
        (unsigned long long)g_freq, g_pCount, (unsigned long long)g_gpuSkipped, (unsigned long long)g_gpuRefused,
        (unsigned long long)g_overflowDepth, kStack, (unsigned long long)g_overflowTable, kStages,
        (unsigned long long)g_foreign, (unsigned long long)g_unbalanced);
    int printed = 0;
    for (int k = 0; k < g_n && printed < 28; ++k) {
        const Stage& st = g_stage[order[k]];
        if (!st.calls) continue;
        char name[64]; int i = 0;
        for (; st.name[i] && i < 63; ++i) name[i] = (st.name[i] >= 32 && st.name[i] < 127) ? (char)st.name[i] : '?';
        name[i] = 0;
        char gpuText[48];
        if (st.gpuSamples && g_freq)
            _snprintf_s(gpuText, _TRUNCATE, "gpu %7.2f ms/s (%.3f ms each, %llu samples)", st.gpuTicks * 1000.0 / g_freq / s,
                        st.gpuTicks * 1000.0 / g_freq / st.gpuSamples, (unsigned long long)st.gpuSamples);
        else _snprintf_s(gpuText, _TRUNCATE, "gpu n/a");
        DVR_LOG(::dvr::log::Cat::perf, ::dvr::log::Level::Info,
            "perf/stages:   d%d %-34s %6.0f calls/s | cpu %7.2f ms/s (%.3f ms each) | %s | %6.0f draws/s",
            st.depth, name, st.calls / s, st.cpuTicks * 1000.0 / g_qpf / s, st.cpuTicks * 1000.0 / g_qpf / st.calls, gpuText, st.draws / s);
        ++printed;
    }
    for (int i = 0; i < g_n; ++i) { Stage& st = g_stage[i]; st.calls = st.draws = st.gpuSamples = st.gpuTicks = 0; st.cpuTicks = 0; }
    g_overflowDepth = g_overflowTable = g_foreign = g_unbalanced = g_gpuSkipped = g_gpuRefused = 0;
    g_windowT0 = t;
}
}  // namespace

void note_device(IDirect3DDevice9* dev) { g_dev = dev; }
bool enabled() { return g_on.load(std::memory_order_relaxed); }
bool gpu() { return g_gpuOn.load(std::memory_order_relaxed); }
void set_gpu(bool on) {
    g_gpuOn.store(on);
    DVR_LOG(::dvr::log::Cat::perf, ::dvr::log::Level::Info, "perf/stages: GPU time per stage %s (timestamp queries at each stage of depth <= %d)",
            on ? "ON" : "off", kGpuDepth);
}
void set_enabled(bool on) {
    g_restart.store(true);   // the owning thread drops any half-open stack at its next event
    g_on.store(on);
    DVR_LOG(::dvr::log::Cat::perf, ::dvr::log::Level::Info,
            "perf/stages: %s - the engine's own stage events (D3DPERF) are %s; a table follows every 5 s while the engine emits them "
            "(no table = the engine's draw-event switch is off: `stages on` sets it)", on ? "ON" : "off", on ? "timed" : "ignored");
}
void release_gpu(const char* why) {
    int n = 0;
    for (auto& q : g_q) if (q) { q->Release(); q = nullptr; ++n; }
    memset(g_qBusy, 0, sizeof(g_qBusy));
    if (g_freqQ) { g_freqQ->Release(); g_freqQ = nullptr; }
    g_pHead = g_pCount = 0; g_qNext = 0; g_gpuFailed = false; g_dev = nullptr;
    for (int i = 0; i < g_depth && i < kStack; ++i) g_stack[i].qBegin = -1;
    if (n) DVR_LOG(::dvr::log::Cat::perf, ::dvr::log::Level::Info, "perf/stages: %d timestamp queries released (%s)", n, why ? why : "asked");
}
void begin(const wchar_t* name) {
    if (!g_on.load(std::memory_order_relaxed)) return;
    const DWORD tid = GetCurrentThreadId();
    if (g_restart.exchange(false)) { g_depth = 0; g_tid = 0; }
    if (!g_tid) { g_tid = tid; LARGE_INTEGER f; QueryPerformanceFrequency(&f); g_qpf = f.QuadPart; g_windowT0 = now(); }
    if (tid != g_tid) { ++g_foreign; return; }
    if (g_depth >= kStack) { ++g_depth; ++g_overflowDepth; return; }
    Open& o = g_stack[g_depth];
    o.stage = stage_for(name ? name : L"(null)", g_depth);
    o.draws0 = g_draws;
    o.qBegin = g_depth <= kGpuDepth ? stamp() : -1;
    o.t0 = now();
    ++g_depth;
}
void end() {
    if (!g_on.load(std::memory_order_relaxed) || GetCurrentThreadId() != g_tid) return;
    const int64_t t = now();
    if (g_depth <= 0) { ++g_unbalanced; return; }
    if (--g_depth >= kStack) return;
    Open& o = g_stack[g_depth];
    if (o.stage >= 0) {
        Stage& st = g_stage[o.stage];
        ++st.calls; st.cpuTicks += t - o.t0; st.draws += g_draws - o.draws0;
        if (o.qBegin >= 0) {
            const int qe = stamp();
            if (qe >= 0 && g_pCount < kPending) { g_pending[(g_pHead + g_pCount) % kPending] = {o.stage, o.qBegin, qe}; ++g_pCount; }
            else { g_qBusy[o.qBegin] = false; if (qe >= 0) g_qBusy[qe] = false; ++g_gpuSkipped; }
        }
    }
    if (g_depth == 0) {
        if ((g_stage[0].calls & 15) == 0) resolve();
        if (g_qpf && t - g_windowT0 >= 5 * g_qpf) report(t);
    }
}
}  // namespace dvr::stageprof
