#define DVR_CAT ::dvr::log::Cat::core
#include "core/framework/status.h"
#include "core/util/log.h"
#include "core/util/paths.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>

namespace dvr::status {

void Writer::put(const char* s)
{
    size_t n = strlen(s);
    if (len_ + n >= sizeof(buf_) - 1) return;   // truncate silently; the file stays valid up to here
    memcpy(buf_ + len_, s, n);
    len_ += n;
    buf_[len_] = 0;
}

void Writer::str(const char* s)
{
    put("\"");
    char esc[8];
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { esc[0] = '\\'; esc[1] = (char)c; esc[2] = 0; put(esc); }
        else if (c < 0x20) { snprintf(esc, sizeof(esc), "\\u%04x", c); put(esc); }
        else { esc[0] = (char)c; esc[1] = 0; put(esc); }
    }
    put("\"");
}

void Writer::sep() { if (!first_) put(","); first_ = false; }
void Writer::key(const char* k) { sep(); str(k); put(":"); }
void Writer::begin() { len_ = 0; buf_[0] = 0; first_ = true; put("{"); }
void Writer::end() { put("}\n"); }
void Writer::obj(const char* k) { key(k); put("{"); first_ = true; }
void Writer::end_obj() { put("}"); first_ = false; }
void Writer::arr(const char* k) { key(k); put("["); first_ = true; }
void Writer::end_arr() { put("]"); first_ = false; }
void Writer::kv(const char* k, int v) { char t[32]; snprintf(t, sizeof(t), "%d", v); key(k); put(t); }
void Writer::kv(const char* k, unsigned long v) { char t[32]; snprintf(t, sizeof(t), "%lu", v); key(k); put(t); }
void Writer::kv(const char* k, double v)
{
    char t[48];
    if (v != v || v > 1e300 || v < -1e300) snprintf(t, sizeof(t), "null");
    else snprintf(t, sizeof(t), "%.4f", v);
    key(k); put(t);
}
void Writer::kv(const char* k, bool v) { key(k); put(v ? "true" : "false"); }
void Writer::kv(const char* k, const char* v) { key(k); if (v) str(v); else put("null"); }
void Writer::item(double v) { char t[48]; snprintf(t, sizeof(t), "%.4f", v); sep(); put(t); }
void Writer::item(const char* v) { sep(); str(v); }

namespace {
Provider g_provider = nullptr;
double   g_lastMs = 0.0;
char     g_path[MAX_PATH] = "";
Writer   g_writer;
void write_text(const char* text, size_t len);   // VR-160: defined with the writer thread, below
}

void set_provider(Provider p) { g_provider = p; }

const char* path()
{
    if (!g_path[0]) dvr::paths::in_data_dir(g_path, "status.json");
    return g_path;
}

void write_now()
{
    if (!g_provider) return;
    g_writer.begin();
    g_provider(g_writer);
    g_writer.end();
    write_text(g_writer.text(), g_writer.length());
}

// VR-160: the 1 Hz write, off the present thread. The JSON is still BUILT on the present thread
// (the provider reads state that belongs to it), but fopen / fwrite / fclose / MoveFileEx went to
// a worker: together with the seam poll they measured 0.2 ms per present averaged = about 20 ms
// once a second on the thread that presents. write_now() stays synchronous for the `status` word
// and the F10 button, whose callers read the file straight after.
namespace {
SRWLOCK     g_ioLock = SRWLOCK_INIT;
std::string g_ioPending;
bool        g_ioHave = false;
HANDLE      g_ioEvent = nullptr;
bool        g_ioFailed = false;

void write_text(const char* text, size_t len)
{
    char tmp[MAX_PATH];
    dvr::paths::in_data_dir(tmp, "status.json.tmp");
    static SRWLOCK fileLock = SRWLOCK_INIT;   // write_now and the worker share one tmp file
    AcquireSRWLockExclusive(&fileLock);
    FILE* f = fopen(tmp, "wb");
    if (f) {
        fwrite(text, 1, len, f);
        fclose(f);
        MoveFileExA(tmp, path(), MOVEFILE_REPLACE_EXISTING);   // readers never see a torn file
    }
    ReleaseSRWLockExclusive(&fileLock);
}

DWORD WINAPI io_thread(void*)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    std::string mine;
    for (;;) {
        WaitForSingleObject(g_ioEvent, INFINITE);
        AcquireSRWLockExclusive(&g_ioLock);
        const bool have = g_ioHave;
        if (have) { mine.swap(g_ioPending); g_ioHave = false; }
        ReleaseSRWLockExclusive(&g_ioLock);
        if (have) write_text(mine.data(), mine.size());
    }
}
}

void tick(double nowMs)
{
    if (nowMs - g_lastMs < 1000.0) return;
    g_lastMs = nowMs;
    if (!g_provider) return;
    if (!g_ioEvent && !g_ioFailed) {
        g_ioEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        HANDLE h = g_ioEvent ? CreateThread(nullptr, 0, io_thread, nullptr, 0, nullptr) : nullptr;
        if (h) CloseHandle(h);
        else { g_ioFailed = true; DVR_WARN("status: no writer thread (%lu) - status.json is written on the present thread", GetLastError()); }
    }
    if (g_ioFailed) { write_now(); return; }
    g_writer.begin();
    g_provider(g_writer);
    g_writer.end();
    // TryAcquire: the present thread never waits for the disk, not even for this lock. A busy
    // writer means this second's snapshot is skipped and the next one lands.
    if (!TryAcquireSRWLockExclusive(&g_ioLock)) return;
    g_ioPending.assign(g_writer.text(), g_writer.length());
    g_ioHave = true;
    ReleaseSRWLockExclusive(&g_ioLock);
    SetEvent(g_ioEvent);
}

} // namespace dvr::status
