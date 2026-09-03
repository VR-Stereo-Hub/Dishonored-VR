// core/util/log.cpp - see log.h.
#define DVR_CAT ::dvr::log::Cat::core
#include "core/util/log.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

namespace dvr::log {

uint8_t g_levels[(int)Cat::COUNT];

namespace {

const char* const kCatNames[(int)Cat::COUNT] = {
    "core", "proxy", "cfg", "d3d", "present", "capture", "vr", "openvr", "openxr", "pace", "xrinput",
    "pad", "overlay", "head", "hands", "graft", "blink", "aim", "melee", "crouch", "fov", "menu", "cine",
    "script", "console", "hud", "res", "cmd", "crash", "perf", "legacy", "device",
};
const char* const kLevelNames[] = { "error", "warn", "info", "debug", "trace" };
const char kLevelLetters[] = { 'E', 'W', 'I', 'D', 'T' };

CRITICAL_SECTION g_cs;
bool   g_csInit = false;
FILE*  g_file = nullptr;
char   g_path[MAX_PATH] = "";
DWORD  g_flushTick = 0;
const DWORD kFlushMs = 200;   // fflush per line was a measured microstutter source (original 38.90)

const size_t kRing = 512;
RingLine g_ring[kRing];
size_t   g_ringHead = 0;      // next slot to write
size_t   g_ringCount = 0;

struct LevelsInit { LevelsInit() { for (auto& l : g_levels) l = (uint8_t)Level::Info; } } g_levelsInit;

void lock()   { if (!g_csInit) { InitializeCriticalSection(&g_cs); g_csInit = true; } EnterCriticalSection(&g_cs); }
void unlock() { LeaveCriticalSection(&g_cs); }

} // namespace

void init(const char* dir, const char* base)
{
    lock();
    if (!g_file) {
        char prev[MAX_PATH];
        snprintf(g_path, sizeof(g_path), "%s\\%s.log", dir, base);
        snprintf(prev, sizeof(prev), "%s\\%s.prev.log", dir, base);
        MoveFileExA(g_path, prev, MOVEFILE_REPLACE_EXISTING);   // no-op on first run
        // _SH_DENYNO so tools\tail-log.ps1 can follow the file while the game runs
        g_file = _fsopen(g_path, "w", 0x40 /* _SH_DENYNO */);
        g_flushTick = GetTickCount();
    }
    unlock();
}

void shutdown()
{
    lock();
    if (g_file) { fclose(g_file); g_file = nullptr; }
    unlock();
}

const char* path() { return g_path; }

void writev(Cat cat, Level lvl, const char* fmt, va_list ap)
{
    char text[1024];
    vsnprintf(text, sizeof(text), fmt, ap);
    text[sizeof(text) - 1] = 0;
    DWORD now = GetTickCount();

    lock();
    RingLine& r = g_ring[g_ringHead];
    r.tick = now; r.cat = (uint8_t)cat; r.level = (uint8_t)lvl;
    strncpy(r.text, text, sizeof(r.text) - 1);
    r.text[sizeof(r.text) - 1] = 0;
    g_ringHead = (g_ringHead + 1) % kRing;
    if (g_ringCount < kRing) g_ringCount++;
    if (g_file) {
        fprintf(g_file, "[%10lu] [%c] [%s] %s\n", (unsigned long)now,
                kLevelLetters[(int)lvl], kCatNames[(int)cat], text);
        if (lvl == Level::Error || now - g_flushTick >= kFlushMs) { fflush(g_file); g_flushTick = now; }
    }
    unlock();
}

void write(Cat cat, Level lvl, const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    writev(cat, lvl, fmt, ap);
    va_end(ap);
}

void flush()
{
    lock();
    if (g_file) { fflush(g_file); g_flushTick = GetTickCount(); }
    unlock();
}

void set_all(Level lvl) { for (auto& l : g_levels) l = (uint8_t)lvl; }
void set_level(Cat cat, Level lvl) { g_levels[(int)cat] = (uint8_t)lvl; }
Level level(Cat cat) { return (Level)g_levels[(int)cat]; }
const char* level_name(Level lvl) { return kLevelNames[(int)lvl]; }
const char* cat_name(Cat cat) { return kCatNames[(int)cat]; }

bool parse_level(const char* s, Level* out)
{
    if (!s || !*s) return false;
    for (int i = 0; i < 5; i++)
        if (!_stricmp(s, kLevelNames[i]) || (s[1] == 0 && (s[0] | 0x20) == (kLevelLetters[i] | 0x20))) {
            *out = (Level)i; return true;
        }
    return false;
}

bool parse_cat(const char* s, Cat* out)
{
    if (!s || !*s) return false;
    for (int i = 0; i < (int)Cat::COUNT; i++)
        if (!_stricmp(s, kCatNames[i])) { *out = (Cat)i; return true; }
    return false;
}

void configure(const char* levelSpec, const char* catsSpec)
{
    Level all;
    if (parse_level(levelSpec, &all)) set_all(all);
    if (!catsSpec || !*catsSpec) return;
    char buf[512];
    strncpy(buf, catsSpec, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0;
    for (char* tok = strtok(buf, ",; "); tok; tok = strtok(nullptr, ",; ")) {
        char* colon = strchr(tok, ':');
        if (!colon) continue;
        *colon = 0;
        Cat c; Level l;
        if (parse_cat(tok, &c) && parse_level(colon + 1, &l)) set_level(c, l);
        else write(Cat::core, Level::Warn, "log: ignoring bad category spec '%s:%s'", tok, colon + 1);
    }
}

size_t ring_copy(RingLine* out, size_t max)
{
    lock();
    size_t n = g_ringCount < max ? g_ringCount : max;
    size_t start = (g_ringHead + kRing - n) % kRing;
    for (size_t i = 0; i < n; i++) out[i] = g_ring[(start + i) % kRing];
    unlock();
    return n;
}

} // namespace dvr::log
