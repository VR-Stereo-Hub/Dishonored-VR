// core/util/log.h - the mod's log: dishonored_vr.log next to the game exe.
//
// Every line carries the tick the original proxy printed ([%10lu]) so old
// reading habits and log-parsing scripts keep working, plus a level letter and
// a subsystem tag so a Quest user's log can be filtered to the lane under
// investigation without rebuilding. Lines also land in a fixed ring buffer that
// the crash handler and the F10 overlay's log tab read.
//
// Thread contract: write() may be called from any thread; it takes one
// critical section. flush() is batched (200 ms) except for Error lines.
#pragma once
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

namespace dvr::log {

enum class Level : uint8_t { Error = 0, Warn, Info, Debug, Trace };

// One tag per subsystem. Keep in sync with kCatNames in log.cpp.
enum class Cat : uint8_t {
    core, proxy, cfg, d3d, present, capture, vr, openvr, openxr, pace, xrinput,
    pad, overlay, head, hands, graft, blink, aim, melee, crouch, fov, menu, cine,
    script, console, hud, res, cmd, crash, perf, legacy, device, armfollow,
    COUNT
};

// Opens <dir>\<base>.log, rotating an existing one to <base>.prev.log so the
// run that crashed survives the relaunch that reports it. Safe under the
// loader lock: kernel32 + the static CRT only.
void init(const char* dir, const char* base);
void shutdown();

void write(Cat cat, Level lvl, const char* fmt, ...);
void writev(Cat cat, Level lvl, const char* fmt, va_list ap);
void flush();

// Per-category thresholds. set_all() applies one level everywhere;
// configure() parses "info" and "blink:debug,openxr:trace" (ini or env form).
void  set_all(Level lvl);
void  set_level(Cat cat, Level lvl);
Level level(Cat cat);
void  configure(const char* levelSpec, const char* catsSpec);
bool  parse_level(const char* s, Level* out);
bool  parse_cat(const char* s, Cat* out);
const char* level_name(Level lvl);
const char* cat_name(Cat cat);
const char* path();                 // full path of the open log, "" if none

// The ring: newest last. Copies up to max lines under the log lock.
struct RingLine { uint32_t tick; uint8_t cat, level; char text[212]; };
size_t ring_copy(RingLine* out, size_t max);

extern uint8_t g_levels[(int)Cat::COUNT];
inline bool enabled(Cat cat, Level lvl) { return (uint8_t)lvl <= g_levels[(int)cat]; }

} // namespace dvr::log

// Each source file names its subsystem before including this header (the
// unity build does it per module); anything else logs as core.
#ifndef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::core
#endif

#define DVR_LOG(cat, lvl, ...) \
    do { if (::dvr::log::enabled(cat, lvl)) ::dvr::log::write(cat, lvl, __VA_ARGS__); } while (0)
#define DVR_LOG_ONCE(cat, lvl, ...) \
    do { static bool dvr_once_ = false; if (!dvr_once_) { dvr_once_ = true; DVR_LOG(cat, lvl, __VA_ARGS__); } } while (0)
#define DVR_LOG_EVERY_MS(cat, lvl, ms, ...) \
    do { static unsigned long dvr_last_ = 0; unsigned long dvr_now_ = GetTickCount(); \
         if (dvr_last_ == 0 || dvr_now_ - dvr_last_ >= (unsigned long)(ms)) { dvr_last_ = dvr_now_; DVR_LOG(cat, lvl, __VA_ARGS__); } } while (0)
#define DVR_LOG_FIRST_N(cat, lvl, n, ...) \
    do { static int dvr_cnt_ = 0; if (dvr_cnt_ < (n)) { dvr_cnt_++; DVR_LOG(cat, lvl, __VA_ARGS__); } } while (0)

#define DVR_ERROR(...) DVR_LOG(DVR_CAT, ::dvr::log::Level::Error, __VA_ARGS__)
#define DVR_WARN(...)  DVR_LOG(DVR_CAT, ::dvr::log::Level::Warn,  __VA_ARGS__)
#define DVR_INFO(...)  DVR_LOG(DVR_CAT, ::dvr::log::Level::Info,  __VA_ARGS__)
#define DVR_DEBUG(...) DVR_LOG(DVR_CAT, ::dvr::log::Level::Debug, __VA_ARGS__)
#define DVR_TRACE(...) DVR_LOG(DVR_CAT, ::dvr::log::Level::Trace, __VA_ARGS__)

// Compatibility with the original single-file proxy: its 800 Log(...) calls
// compile unchanged as Info lines tagged with the including module's DVR_CAT.
#define Log(...)   DVR_LOG(DVR_CAT, ::dvr::log::Level::Info, __VA_ARGS__)
#define LogFlush() ::dvr::log::flush()
