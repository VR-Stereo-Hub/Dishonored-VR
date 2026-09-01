"""Phase 2 wiring: log, crash, clock, mem, ini, paths and hook utilities become
real translation units; the unity build includes their headers, tags every
module with a log category, and loses the prototypes and globals they replace.
One-shot, kept as the record of the edit. Run from the repo root."""
import io
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def rd(p):
    return io.open(os.path.join(ROOT, p), encoding="utf-8", newline="").read().replace("\r\n", "\n")


def wr(p, s):
    io.open(os.path.join(ROOT, p), "w", encoding="utf-8", newline="\n").write(s)


def drop_lines(p, patterns):
    s = rd(p)
    out = []
    dropped = 0
    for ln in s.split("\n"):
        if any(re.search(pat, ln) for pat in patterns):
            dropped += 1
            continue
        out.append(ln)
    wr(p, "\n".join(out))
    print(f"{p}: dropped {dropped} lines")


# 1. prototypes now provided by headers
drop_lines("src/mod/fwd.h", [
    r"\bLog\(const char\* fmt, \.\.\.\);", r"\bLogFlush\(void\);", r"\bXrVeh\(EXCEPTION_POINTERS\*",
    r"\bMaimNowMs\(\);", r"\bRangeReadable\(const void\*", r"\bSafeRead32\(uintptr_t",
    r"\bIniFloat\(const char\*", r"\bWriteTextFile\(const char\*", r"\bPatchVtable\(void\*",
    r"\bFindIatSlotIn\(HMODULE", r"\bFindIatSlot\(const char\*",
])

# 2. globals the modules own now
drop_lines("src/mod/state/01_proxy_proxy_state.inc", [r"^static FILE\*\s+g_log\s", r"^static CRITICAL_SECTION g_logLock;"])
drop_lines("src/mod/state/13_game_dishonored_game_state.inc", [r"^static DWORD g_logFlushTick = 0;"])
drop_lines("src/mod/state/04_core_gfx_d3d11_device.inc", [r"^static LONG WINAPI XrVeh\(EXCEPTION_POINTERS\*\);", r"^static bool g_vehOn = false;"])

# 3. the two VEH install sites -> the crash module
for p in ("src/core/framework/frame_hooks.cpp", "src/core/vr/openxr_backend.cpp"):
    s = rd(p)
    old = "    if (!g_vehOn) { g_vehOn = true; AddVectoredExceptionHandler(1, XrVeh); }\n"
    assert old in s, p
    s = s.replace(old, "    dvr::crash::install();   // fingerprint VEH + minidump filter, idempotent\n")
    wr(p, s)
    print(p, "crash install wired")

# 4. thread identity for the fingerprint
p = "src/core/gfx/present.cpp"
s = rd(p)
old = "    if (!g_presentTid) g_presentTid = GetCurrentThreadId();   // 38.11\n"
assert old in s
s = s.replace(old, old + "    if (!g_presentNamed) { g_presentNamed = true; dvr::crash::register_thread(\"present\", g_presentTid); dvr::crash::rearm(); }\n")
wr(p, s)
p = "src/mod/state/04_core_gfx_d3d11_device.inc"
s = rd(p)
old = "static DWORD         g_presentTid = 0;\n"
assert old in s
s = s.replace(old, old + "static bool          g_presentNamed = false;\n")
wr(p, s)
p = "src/core/vr/openxr_pace.cpp"
s = rd(p)
old = "    g_xrPaceTid = GetCurrentThreadId();     // 38.11\n"
assert old in s
s = s.replace(old, old + "    dvr::crash::register_thread(\"xr-pace\", g_xrPaceTid);\n")
wr(p, s)
print("thread identities wired")

# 5. [Log] Level / Cats from the ini (env already applied in DllMain; env wins)
p = "src/core/config/config.cpp"
s = rd(p)
m = re.search(r"static void EnsureConfig\(\)\n\{.*?LoadConfig\(\);\n", s, re.S)
assert m
block = ("""        {   // [Log] Level=info  Cats=blink:debug,openxr:trace - env DVR_LOG / DVR_LOG_CATS win
            char ini[MAX_PATH], lv[32] = "", cats[512] = "";
            _snprintf(ini, MAX_PATH, "%s\\\\dishonored_vr.ini", g_dir);
            GetPrivateProfileStringA("Log", "Level", "", lv, sizeof(lv), ini);
            GetPrivateProfileStringA("Log", "Cats", "", cats, sizeof(cats), ini);
            if (!GetEnvironmentVariableA("DVR_LOG", NULL, 0)) dvr::log::configure(lv, "");
            if (!GetEnvironmentVariableA("DVR_LOG_CATS", NULL, 0)) dvr::log::configure("", cats);
        }
""")
s = s[:m.end()] + block + s[m.end():]
wr(p, s)
print("config: [Log] keys wired")

# 6. the unity file: headers after the prelude, categories per module, real TUs dropped
REAL = {"core/util/log", "core/util/crash", "core/util/clock", "core/util/mem", "core/util/ini",
        "core/util/paths", "core/hooks/vtable", "core/hooks/iat", "core/hooks/detour"}
CATS = {
    "proxy/": "proxy", "core/config/": "cfg", "core/framework/": "present", "core/gfx/d3d9_capture": "capture",
    "core/gfx/hand_mesh": "hands", "core/gfx/hud_panel": "hud", "core/gfx/": "present", "core/input/": "pad",
    "core/ui/": "overlay", "core/vr/openvr": "openvr", "core/vr/openxr_pace": "pace", "core/vr/openxr_input": "xrinput",
    "core/vr/": "openxr", "core/window/": "res", "game/dishonored/blink": "blink", "game/dishonored/block_state": "hands",
    "game/dishonored/console": "console", "game/dishonored/crouch": "crouch", "game/dishonored/fov_lever": "fov",
    "game/dishonored/game_state": "menu", "game/dishonored/hands/graft": "graft", "game/dishonored/hands/": "hands",
    "game/dishonored/head_track": "head", "game/dishonored/melee": "melee", "game/dishonored/motion_aim": "aim",
    "game/dishonored/shared/": "core", "game/dishonored/ue3/process_event": "script", "game/dishonored/ue3/": "script",
    "legacy/": "legacy",
}


def cat_for(mod):
    best = ""
    for prefix, cat in CATS.items():
        if mod.startswith(prefix) and len(prefix) > len(best):
            best = prefix
    return CATS[best] if best else "core"


p = "src/mod/dishonoredvr.cpp"
s = rd(p)
out = []
for ln in s.split("\n"):
    m = re.match(r'#include "(.*)\.cpp"', ln)
    if m:
        mod = m.group(1)
        if mod in REAL:
            continue
        out.append(f"#define DVR_CAT ::dvr::log::Cat::{cat_for(mod)}")
        out.append(ln)
        out.append("#undef DVR_CAT")
        continue
    out.append(ln)
    if ln == '#include "mod/state/00_mod_prelude.inc"':
        out.append("")
        out.append("// ---- modules with their own translation unit: headers only ------------------")
        out.append('#include "dvr_version.h"')
        out.append('#include "core/util/log.h"')
        out.append('#include "core/util/clock.h"')
        out.append('#include "core/util/mem.h"')
        out.append('#include "core/util/ini.h"')
        out.append('#include "core/util/paths.h"')
        out.append('#include "core/util/diag.h"')
        out.append('#include "core/util/crash.h"')
        out.append('#include "core/hooks/vtable.h"')
        out.append('#include "core/hooks/iat.h"')
        out.append('#include "core/hooks/detour.h"')
        out.append("")
wr(p, "\n".join(out))
print("unity file updated")

# 7. CMake: the real translation units
p = "src/CMakeLists.txt"
s = rd(p)
old = """add_library(dvr_proxy SHARED
    mod/dishonoredvr.cpp
    proxy/d3d9.def
    ${DVR_MODULE_SOURCES}
)
# The module files are included by the unity TU, not compiled on their own.
set_source_files_properties(${DVR_MODULE_SOURCES} PROPERTIES HEADER_FILE_ONLY ON)"""
new = """# Modules that already have their own header and translation unit.
set(DVR_REAL_SOURCES
    core/util/log.cpp        core/util/log.h
    core/util/crash.cpp      core/util/crash.h
    core/util/clock.cpp      core/util/clock.h
    core/util/mem.cpp        core/util/mem.h
    core/util/ini.cpp        core/util/ini.h
    core/util/paths.cpp      core/util/paths.h
    core/util/diag.h
    core/hooks/vtable.cpp    core/hooks/vtable.h
    core/hooks/iat.cpp       core/hooks/iat.h
    core/hooks/detour.cpp    core/hooks/detour.h
)
foreach(_f ${DVR_REAL_SOURCES})
    list(REMOVE_ITEM DVR_MODULE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${_f}")
endforeach()
add_library(dvr_proxy SHARED
    mod/dishonoredvr.cpp
    proxy/d3d9.def
    ${DVR_REAL_SOURCES}
    ${DVR_MODULE_SOURCES}
)
# The unity-included module files are not compiled on their own.
set_source_files_properties(${DVR_MODULE_SOURCES} PROPERTIES HEADER_FILE_ONLY ON)"""
assert old in s
s = s.replace(old, new)
s = s.replace("    proxy/*.cpp core/*.cpp game/*.cpp legacy/*.cpp mod/state/*.inc mod/fwd.h)",
              "    proxy/*.cpp core/*.cpp game/*.cpp legacy/*.cpp mod/state/*.inc mod/fwd.h core/*.h game/*.h)")
wr(p, s)
print("CMake updated")
