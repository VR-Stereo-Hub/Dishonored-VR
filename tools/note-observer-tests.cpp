// Production movie-observer regression: reloads must not exhaust tracking slots.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <set>
#define UI_INST_MAX 128
#define DVR_CAT 0
#define DVR_LOG_ONCE(...) ((void)0)
#define DVR_LOG(...) ((void)0)
static const int kClassOff = 8, kNameOff = 16;
static LONG g_uiInstN = 0;
static int g_uiGen = 0;
static bool g_uiNoteOpen = false;
static std::set<uint8_t*> liveObjects;
static uint8_t* unreadable = nullptr;
static bool IsLiveObject(uint8_t* p) { return liveObjects.count(p) != 0; }
static bool RangeReadable(void* p, size_t) { return p && p != unreadable; }
static double nowMs = 1000.0, g_uiPollMs = 0.0, g_scriptHeadMs = 0.0;
static bool g_uiNoteFastMono = true, g_scriptHeadOK = true;
static bool g_menuOpen = false, g_inMenu = false;
static double MaimNowMs() { return nowMs; }
static void Log(const char*, ...) {}
static void UiObjName(uint8_t*, char* out, size_t n) { strcpy_s(out, n, "pNote"); }
static const char* ObjClassName(uint8_t*) { return "DisGFxMoviePlayerNote"; }
#include "note_observer_body.inc"
static int failed = 0, checked = 0;
static void check(bool ok, const char* why) {
    ++checked;
    if (!ok) { ++failed; std::printf("FAIL: %s\n", why); }
}
struct alignas(8) Object { uint8_t bytes[40] = {}; };
static Object objects[UI_INST_MAX + 8];
static uint8_t cls[16];
static uint8_t* init(int i, unsigned name) {
    uint8_t* p = objects[i].bytes;
    *(uint8_t**)(p + kClassOff) = cls;
    *(uint32_t*)(p + kNameOff) = name;
    *(uint32_t*)(p + kNameOff + 4) = 1;
    liveObjects.insert(p); return p;
}
int main() {
    for (int i = 0; i < UI_INST_MAX; ++i) UiAddInstance(init(i, i + 1), cls);
    check(g_uiInstN == UI_INST_MAX, "full simultaneous population retained");
    UiAddInstance(objects[0].bytes, cls);
    check(g_uiInstN == UI_INST_MAX, "duplicate is not appended");
    auto old = objects[17].bytes;
    liveObjects.erase(old); // stale bytes and class remain readable
    auto note = init(UI_INST_MAX, 500);
    UiAddInstance(note, cls);
    check(g_uiInst[17].obj == note, "dead entry reclaimed when full despite unchanged old class");
    check(g_uiInstN == UI_INST_MAX, "reclaim keeps bounded population");
    check(UiInstanceLive(&g_uiInst[17]), "replacement identity recorded");
    *(uint32_t*)(note + kNameOff + 4) = 2;
    check(!UiInstanceLive(&g_uiInst[17]), "FName instance number reuse invalidates pointer");
    UiAddInstance(note, cls);
    check(UiInstanceLive(&g_uiInst[17]) && g_uiInst[17].open == -1,
          "same pointer with new identity is freshly observed");
    unreadable = note;
    check(!UiInstanceLive(&g_uiInst[17]), "unreadable identity rejected");
    unreadable = nullptr;
    auto extra = init(UI_INST_MAX + 1, 501);
    UiAddInstance(extra, cls);
    check(g_uiInstN == UI_INST_MAX, "full live population is not overwritten");
    bool repeated = true;
    for (int generation = 1; generation <= 300; ++generation) {
        liveObjects.clear(); ++g_uiGen; g_uiNoteOpen = true;
        UiResetInstances();
        repeated &= g_uiInstN == 0 && !g_uiNoteOpen;
        for (int i = 0; i < 46; ++i) UiAddInstance(init(i, generation * 1000 + i), cls);
        auto newNote = init(47, generation * 1000 + 999);
        UiAddInstance(newNote, cls);
        repeated &= g_uiInstN == 47 && g_uiInst[46].obj == newNote &&
            g_uiInst[46].gen == g_uiGen && UiInstanceLive(&g_uiInst[46]);
    }
    check(repeated, "300 reloads rediscover the new note without retaining old population");
    liveObjects.erase(objects[47].bytes);
    check(!UiInstanceLive(&g_uiInst[46]), "freed note cannot vote even with identical bytes");
    nowMs = 10000; g_scriptHeadMs = nowMs; g_uiPollMs = nowMs;
    g_uiNoteOpen = true;
    check(!DvrScriptViewLive(), "fresh open note enters mono before the silence timeout");
    nowMs += 20; g_scriptHeadMs = nowMs; g_uiNoteOpen = false;
    check(DvrScriptViewLive(), "note close resumes stereo with a fresh dispatch without one-second hold");
    g_uiNoteOpen = true; g_uiPollMs = nowMs;
    check(!DvrScriptViewLive(), "second note can reopen immediately");
    nowMs += 600; g_scriptHeadMs = nowMs;
    check(DvrScriptViewLive(), "expired observer lease cannot indefinitely hold mono");
    g_uiNoteOpen = false; nowMs += 1000;
    check(!DvrScriptViewLive(), "unrelated view silence retains the loading gate");
    nowMs += 20; g_scriptHeadMs = nowMs;
    check(!DvrScriptViewLive(), "ordinary loading resume retains the one-second safety wait");
    nowMs += 1001; g_scriptHeadMs = nowMs;
    check(DvrScriptViewLive(), "continuous dispatch restores ordinary gameplay");
    std::printf("note observer: %d checks, %d failures\n", checked, failed);
    return failed ? 1 : 0;
}
