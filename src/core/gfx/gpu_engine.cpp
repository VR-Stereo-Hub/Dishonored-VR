// core/gfx/gpu_engine.cpp - see gpu_engine.h.
#define DVR_CAT ::dvr::log::Cat::perf
#include "core/gfx/gpu_engine.h"
#include "core/gfx/gpu_memory.h"
#include "core/util/log.h"

#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <atomic>
#pragma comment(lib, "pdh.lib")

namespace dvr::gpu_engine {
namespace {
std::atomic<bool> g_busy{false};
std::atomic<ULONGLONG> g_nextMs{0};
const char* volatile g_why = "";

// The engine types Windows names in the instance ("..._engtype_3D").
const char* const kTypes[] = { "3D", "Copy", "Compute", "VideoEncode", "VideoDecode", "Other" };
constexpr int kTypeN = 6;
int type_of(const char* inst) {
    const char* t = strstr(inst, "engtype_");
    if (!t) return kTypeN - 1;
    t += 8;
    for (int i = 0; i < kTypeN - 1; ++i) if (!_strnicmp(t, kTypes[i], strlen(kTypes[i]))) return i;
    if (!_strnicmp(t, "Compute", 7)) return 2;
    if (!_strnicmp(t, "VideoEncode", 11)) return 3;
    return kTypeN - 1;
}

struct Proc { DWORD pid; double pct[kTypeN]; double total; };
constexpr int kProcMax = 64;

void name_of(DWORD pid, char* out, size_t n) {
    _snprintf_s(out, n, _TRUNCATE, "pid %lu", (unsigned long)pid);
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return;
    char path[MAX_PATH]; DWORD len = MAX_PATH;
    if (QueryFullProcessImageNameA(h, 0, path, &len)) {
        const char* base = strrchr(path, '\\');
        _snprintf_s(out, n, _TRUNCATE, "%s", base ? base + 1 : path);
    }
    CloseHandle(h);
}

DWORD WINAPI run(void*) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    const char* why = g_why;
    PDH_HQUERY q = nullptr; PDH_HCOUNTER c = nullptr;
    PDH_STATUS st = PdhOpenQueryA(nullptr, 0, &q);
    if (st == ERROR_SUCCESS) st = PdhAddEnglishCounterA(q, "\\GPU Engine(*)\\Utilization Percentage", 0, &c);
    if (st == ERROR_SUCCESS) st = PdhCollectQueryData(q);
    if (st == ERROR_SUCCESS) { Sleep(500); st = PdhCollectQueryData(q); }
    DWORD bytes = 0, items = 0;
    PDH_FMT_COUNTERVALUE_ITEM_A* arr = nullptr;
    if (st == ERROR_SUCCESS) {
        st = PdhGetFormattedCounterArrayA(c, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &bytes, &items, nullptr);
        if (st == PDH_MORE_DATA) {
            arr = (PDH_FMT_COUNTERVALUE_ITEM_A*)malloc(bytes);
            st = arr ? PdhGetFormattedCounterArrayA(c, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &bytes, &items, arr) : PDH_MEMORY_ALLOCATION_FAILURE;
        }
    }
    if (st != ERROR_SUCCESS) {
        DVR_WARN("gpu/engines (%s): the per-process GPU counters refused (PDH 0x%08lx) - who used the GPU is not known for "
                 "this stall", why, (unsigned long)st);
    } else {
        Proc procs[kProcMax] = {}; int np = 0;
        double sum[kTypeN] = {};
        for (DWORD i = 0; i < items; ++i) {
            if (arr[i].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA && arr[i].FmtValue.CStatus != PDH_CSTATUS_NEW_DATA) continue;
            const double v = arr[i].FmtValue.doubleValue;
            if (!(v > 0.05)) continue;
            const char* p = strstr(arr[i].szName, "pid_");
            if (!p) continue;
            const DWORD pid = (DWORD)strtoul(p + 4, nullptr, 10);
            const int t = type_of(arr[i].szName);
            sum[t] += v;
            int k = 0;
            while (k < np && procs[k].pid != pid) ++k;
            if (k == np) { if (np == kProcMax) continue; procs[np].pid = pid; ++np; }
            procs[k].pct[t] += v; procs[k].total += v;
        }
        // The busiest six, by their total over every engine.
        for (int a = 0; a < np; ++a)
            for (int b = a + 1; b < np; ++b)
                if (procs[b].total > procs[a].total) { Proc t = procs[a]; procs[a] = procs[b]; procs[b] = t; }
        const DWORD self = GetCurrentProcessId();
        double selfPct[kTypeN] = {};
        for (int k = 0; k < np; ++k) if (procs[k].pid == self) memcpy(selfPct, procs[k].pct, sizeof(selfPct));
        char line[1600]; size_t at = 0;
        for (int k = 0; k < np && k < 6; ++k) {
            char nm[96]; name_of(procs[k].pid, nm, sizeof(nm));
            at += _snprintf_s(line + at, sizeof(line) - at, _TRUNCATE, "%s%s%s:", k ? " | " : "", nm,
                              procs[k].pid == self ? " (THIS GAME + the mod)" : "");
            for (int t = 0; t < kTypeN; ++t)
                if (procs[k].pct[t] >= 0.5)
                    at += _snprintf_s(line + at, sizeof(line) - at, _TRUNCATE, " %s %.0f%%", kTypes[t], procs[k].pct[t]);
            if (at >= sizeof(line) - 1) break;
        }
        DVR_INFO("gpu/engines (%s): over 500 ms, busiest processes by engine (100%% = one engine fully busy): %s",
                 why, np ? line : "no process above 0.05%");
        DVR_INFO("gpu/engines (%s): all processes 3D %.0f%%, Copy %.0f%%, Compute %.0f%%, VideoEncode %.0f%% | this game's "
                 "share 3D %.0f%%, Copy %.0f%%, Compute %.0f%%. A 3D total near 100%% with this game's share small means "
                 "another process holds the GPU; with this game's share near 100%%, the frame (D3D9 game, or the mod's "
                 "D3D11 work in the same process) is the load",
                 why, sum[0], sum[1], sum[2], sum[3], selfPct[0], selfPct[1], selfPct[2]);
    }
    free(arr);
    if (q) PdhCloseQuery(q);
    g_nextMs.store(GetTickCount64() + 10000);
    g_busy.store(false);
    return 0;
}
}

void request(const char* why) {
    if (!dvr::gpu_memory::enabled()) return;
    const ULONGLONG now = GetTickCount64();
    if (now < g_nextMs.load()) return;
    bool expected = false;
    if (!g_busy.compare_exchange_strong(expected, true)) return;
    g_why = why ? why : "";
    g_nextMs.store(now + 10000);
    HANDLE h = CreateThread(nullptr, 0, run, nullptr, 0, nullptr);
    if (h) CloseHandle(h);
    else g_busy.store(false);
}
}
