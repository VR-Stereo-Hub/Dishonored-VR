#define DVR_CAT ::dvr::log::Cat::cmd
#include "core/framework/command.h"
#include "core/framework/perf.h"
#include "core/framework/status.h"
#include "core/util/log.h"
#include "core/util/paths.h"
#include "core/util/diag.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

namespace dvr::command {
namespace {

GameHandler g_game = nullptr;
double      g_lastPollMs = 0.0;
uint32_t    g_seq = 0;
uint32_t    g_lines = 0;
uint32_t    g_unknown = 0;
FILETIME    g_startTime = {};
FILETIME    g_lastWrite = {};
bool        g_haveStart = false;

bool read_and_truncate(char* buf, size_t cap)
{
    char path[MAX_PATH];
    dvr::paths::in_data_dir(path, "command.txt");
    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD got = 0;
    BOOL ok = ReadFile(h, buf, (DWORD)cap - 1, &got, nullptr);
    buf[ok ? got : 0] = 0;
    // consume: a command must never re-apply on the next poll or the next boot
    SetFilePointer(h, 0, nullptr, FILE_BEGIN);
    SetEndOfFile(h);
    CloseHandle(h);
    return ok && got > 0;
}

void write_ack(const char* text)
{
    char path[MAX_PATH];
    dvr::paths::in_data_dir(path, "ack.txt");
    FILE* f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "seq %lu\n%s", (unsigned long)g_seq, text);
    fclose(f);
}

} // namespace

void set_game_handler(GameHandler h) { g_game = h; }
uint32_t sequence() { return g_seq; }

bool core_command(const char* cmd, const char* args)
{
    if (!strcmp(cmd, "status")) {
        dvr::status::write_now();
        DVR_INFO("status written to %s", dvr::status::path());
        return true;
    }
    if (!strcmp(cmd, "log")) {
        char a[32] = "", b[32] = "", c[32] = "";
        sscanf(args, "%31s %31s %31s", a, b, c);
        dvr::log::Level lvl; dvr::log::Cat cat;
        if (!strcmp(a, "flush")) { dvr::log::flush(); return true; }
        if (!strcmp(a, "level") && dvr::log::parse_level(b, &lvl)) {
            dvr::log::set_all(lvl);
            DVR_INFO("log level -> %s (all categories)", dvr::log::level_name(lvl));
            return true;
        }
        if (!strcmp(a, "cat") && dvr::log::parse_cat(b, &cat) && dvr::log::parse_level(c, &lvl)) {
            dvr::log::set_level(cat, lvl);
            DVR_INFO("log category %s -> %s", dvr::log::cat_name(cat), dvr::log::level_name(lvl));
            return true;
        }
        DVR_WARN("log: usage - log level <lvl> | log cat <cat> <lvl> | log flush");
        return true;
    }
    if (!strcmp(cmd, "cmd")) {
        DVR_INFO("cmd: seq=%lu lines=%lu unknown=%lu data=%s log=%s",
                 (unsigned long)g_seq, (unsigned long)g_lines, (unsigned long)g_unknown,
                 dvr::paths::data_dir(), dvr::log::path());
        return true;
    }
    if (!strcmp(cmd, "skip")) {
        DVR_INFO("skip: DVR_SKIP says %s is %s", args, dvr::diag::skip(args) ? "SKIPPED" : "installed");
        return true;
    }
    if (!strcmp(cmd, "perf")) {   // 41.1 (session 8): the tick budget
        if (!args[0] || !strcmp(args, "status")) { dvr::perf::log_status(); return true; }
        if (!strcmp(args, "on"))  { dvr::perf::set_enabled(true); return true; }
        if (!strcmp(args, "off")) { dvr::perf::set_enabled(false); return true; }
        if (!strcmp(args, "gpu on"))  { dvr::perf::set_gpu_enabled(true); return true; }
        if (!strcmp(args, "gpu off")) { dvr::perf::set_gpu_enabled(false); return true; }
        DVR_WARN("perf: usage - perf on|off|status|gpu on|off (the tick line and the gpu line, every 3 s)");
        return true;
    }
    return false;
}

void dispatch_line(const char* line)
{
    char buf[512];
    strncpy(buf, line, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0;
    char* p = buf;
    while (*p == ' ' || *p == '\t') p++;
    char* end = p + strlen(p);
    while (end > p && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) *--end = 0;
    if (!*p || *p == '#') return;
    char* args = p;
    while (*args && *args != ' ' && *args != '\t') args++;
    if (*args) { *args++ = 0; while (*args == ' ' || *args == '\t') args++; }
    for (char* q = p; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 32;
    g_lines++;
    DVR_INFO("> %s %s", p, args);
    if (g_game && g_game(p, args)) return;
    if (core_command(p, args)) return;
    g_unknown++;
    DVR_WARN("cmd: unknown command '%s' (see core/framework/command.h and game/dishonored/commands.cpp)", p);
}

void poll(double nowMs)
{
    if (nowMs - g_lastPollMs < 1000.0) return;
    g_lastPollMs = nowMs;
    if (!g_haveStart) { GetSystemTimeAsFileTime(&g_startTime); g_haveStart = true; }

    // 40.2 EVERY REFUSED GUARD SAYS WHY. This function had five silent
    // returns, and when the seam went deaf mid-session it reported nothing at
    // all: two `dump eyes` commands did nothing and left no trace, while the
    // present path logged happily at 62 fps. Six hypotheses were spent
    // narrowing it by inference (the clock, the 1 Hz gate, the data dir, the
    // start-time compare, a stale mtime) because the code refused to name the
    // branch it took. It names it now. Rate-limited, and NEVER on the path
    // that succeeds, so a healthy seam still costs nothing.
    char path[MAX_PATH];
    dvr::paths::in_data_dir(path, "command.txt");
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fa)) {
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 10000,
            "cmd: GetFileAttributesEx failed on %s (err %lu) - the seam is DEAF; "
            "no command can arrive", path, (unsigned long)GetLastError());
        return;
    }
    if (fa.nFileSizeLow == 0 && fa.nFileSizeHigh == 0) return;   // idle: normal
    if (CompareFileTime(&fa.ftLastWriteTime, &g_lastWrite) == 0) {
        // The file has bytes in it but its write time has not moved since we
        // last looked. A command sitting here unread means the writer changed
        // the contents without the timestamp advancing, or we recorded a time
        // we never actually consumed.
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 10000,
            "cmd: %s holds %lu byte(s) but its write time has not changed since "
            "the last poll - REFUSING to re-read it, so this command will never "
            "run. Rewrite the file to bump its timestamp.",
            path, (unsigned long)fa.nFileSizeLow);
        return;
    }
    g_lastWrite = fa.ftLastWriteTime;
    if (CompareFileTime(&fa.ftLastWriteTime, &g_startTime) < 0) {
        DVR_WARN("cmd: command.txt is older than this process - ignoring a stale file (cleared)");
        char dummy[8];
        read_and_truncate(dummy, sizeof(dummy));
        return;
    }
    static char text[8192];
    if (!read_and_truncate(text, sizeof(text))) {
        // The last silent return, and the leading suspect for the seam going
        // deaf: the attributes said there were bytes, but the open-read-
        // truncate did not complete. A sharing violation from the writer, or
        // a file that vanished between the two calls, both land here.
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 10000,
            "cmd: %s reported %lu byte(s) but could not be read and cleared "
            "(err %lu) - the command is LOST and the seam will keep refusing "
            "until the file is rewritten",
            path, (unsigned long)fa.nFileSizeLow, (unsigned long)GetLastError());
        return;
    }
    g_seq++;
    char* ctx = nullptr;
    for (char* line = strtok_s(text, "\n", &ctx); line; line = strtok_s(nullptr, "\n", &ctx))
        dispatch_line(line);
    write_ack(text);
}

} // namespace dvr::command
