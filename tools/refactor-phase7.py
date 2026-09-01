"""Phase 6/7 wiring, one shot (kept as the record of the edit):

  1. src/game/dishonored/patterns.h - every fixed engine address, IAT slot and
     UE3 field offset moved out of the state chunks, verbatim with comments.
  2. src/legacy is compiled only with DVR_WITH_LEGACY. Helpers that the live
     code still uses move to live modules; the diagnostic ticks the live code
     calls get no-op stand-ins in src/legacy/legacy_stubs.inc.
Run from the repo root."""
import io
import os
import re
import glob

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def rd(p):
    return io.open(os.path.join(ROOT, p), encoding="utf-8", newline="").read().replace("\r\n", "\n")


def wr(p, s):
    os.makedirs(os.path.dirname(os.path.join(ROOT, p)), exist_ok=True)
    io.open(os.path.join(ROOT, p), "w", encoding="utf-8", newline="\n").write(s)


# ---------------------------------------------------------------- patterns.h
PATTERN_NAMES = [
    "kGObjHdr", "kNameOff", "kClassOff", "kOuterOff", "kCamRight", "kCamLoc0", "kCamLoc1", "kCamLoc2",
    "kGNamesData", "kGNamesNum", "kModBase", "kModEnd", "kDataStart", "kDataEnd",
    "kCamHookAt", "kProcessEvent",
    "kBlkAimHook", "kBlkAimBack", "kBlkAimOrig", "kBlkDstHook", "kBlkDstBack", "kBlkDstOrig",
    "kBlkDirHook", "kBlkDirBack", "kBlkDirOrig", "kBlkTrcHook", "kBlkTrcBack", "kBlkTrcOrig",
    "kXIGetSlot", "kXISetSlot",
    "kSkcName", "kSkcStr", "kSkcBools", "kSkcTrans", "kSkcTSpace", "kSkcRSpace", "kSkcRot", "kSkcScaleProp",
    "kSkcApplyTrans", "kSkcApplyRot", "kSkcAddTrans", "kSkcAddRot",
    "kMeshTrans", "kMeshRot", "kMeshScale", "kMeshScl3D",
    "kPcRotBase", "kCamRotBase", "kPovOffs", "kFovCands", "kLevCtrl", "kLevCam",
]
moved = {}
for chunk in sorted(glob.glob(os.path.join(ROOT, "src/mod/state/*.inc"))):
    rel = os.path.relpath(chunk, ROOT).replace("\\", "/")
    s = rd(rel)
    out = []
    for ln in s.split("\n"):
        m = re.match(r"^static const \w+\s+(k\w+)(\[\d*\])?\s*=", ln)
        if m and m.group(1) in PATTERN_NAMES and m.group(1) not in moved:
            moved[m.group(1)] = ln
            continue
        out.append(ln)
    if len(out) != len(s.split("\n")):
        wr(rel, "\n".join(out))
missing = [n for n in PATTERN_NAMES if n not in moved]
print(f"patterns: moved {len(moved)} constants; not found: {missing}")

groups = [
    ("Module image (no ASLR: base 0x400000)", ["kModBase", "kModEnd", "kDataStart", "kDataEnd"]),
    ("UE3 globals", ["kGObjHdr", "kGNamesData", "kGNamesNum"]),
    ("UObject layout", ["kNameOff", "kClassOff", "kOuterOff"]),
    ("Camera object", ["kCamRight", "kCamLoc0", "kCamLoc1", "kCamLoc2", "kPcRotBase", "kCamRotBase", "kPovOffs", "kFovCands", "kLevCtrl", "kLevCam"]),
    ("Engine code hooks (byte-verified before patching)", ["kProcessEvent", "kCamHookAt",
        "kBlkAimHook", "kBlkAimBack", "kBlkAimOrig", "kBlkDstHook", "kBlkDstBack", "kBlkDstOrig",
        "kBlkDirHook", "kBlkDirBack", "kBlkDirOrig", "kBlkTrcHook", "kBlkTrcBack", "kBlkTrcOrig"]),
    ("Import table slots", ["kXIGetSlot", "kXISetSlot"]),
    ("SkelControl (AnimTree bone-override node) fields", ["kSkcName", "kSkcStr", "kSkcBools", "kSkcTrans", "kSkcTSpace", "kSkcRSpace", "kSkcRot", "kSkcScaleProp",
        "kSkcApplyTrans", "kSkcApplyRot", "kSkcAddTrans", "kSkcAddRot"]),
    ("SkeletalMeshComponent fields", ["kMeshTrans", "kMeshRot", "kMeshScale", "kMeshScl3D"]),
]
hdr = """// game/dishonored/patterns.h - EVERY fixed address and engine layout number
// the mod relies on, in one place.
//
// Dishonored.exe (Steam, patch 1.4, UE3 build 9099) is a 32-bit image with no
// ASLR: it always loads at 0x400000, so the mod uses absolute addresses. Each
// code hook verifies the bytes it expects before patching and refuses on a
// mismatch - that refusal is the only build check there is, so a different exe
// build degrades to "feature off" instead of a crash. Every entry is documented
// with its derivation in docs/dishonored/ENGINE_NOTES.md; never add a number
// anywhere else, and never copy one from another game.
#pragma once
#include <stdint.h>

static_assert(sizeof(void*) == 4, "Dishonored is a 32-bit game; the addresses here are 32-bit");

"""
body = ""
for title, names in groups:
    body += f"// ---- {title} ----\n"
    for n in names:
        if n in moved:
            body += moved[n] + "\n"
    body += "\n"
wr("src/game/dishonored/patterns.h", hdr + body)

# the unity build includes patterns.h before the state chunks (they use the values)
u = rd("src/mod/dishonoredvr.cpp")
if 'game/dishonored/patterns.h' not in u:
    u = u.replace('#include "core/framework/status.h"\n', '#include "core/framework/status.h"\n#include "game/dishonored/patterns.h"\n', 1)

# ---------------------------------------------------------------- legacy gating
# helpers the live code uses: move them out of src/legacy into live modules
MOVE = {
    "legacy/vs_scan.cpp": ("core/framework/vs_const.cpp", ["IsAffineRowMajor", "IsAffineColMajor", "Finite16", "IsMirrored", "IsMainScenePass", "ShearVP", "LeanVP"]),
    "legacy/rtd_drive.cpp": ("game/dishonored/shared/ue_math.cpp", ["M3OrthoRows"]),
}
MOVE2 = {"legacy/rtd_drive.cpp": ("game/dishonored/hands/fp_mesh.cpp", ["FpWorldPos", "GtUE3Rot"])}


def extract(src, names):
    """Cut whole function definitions (with the comment block above) by name."""
    out = {}
    for n in names:
        m = re.search(r"((?:^//[^\n]*\n)*)^(?:static |extern \"C\" )?[\w\s\*&:<>]*?\b" + re.escape(n) + r"\s*\([^;{]*\)\s*\n\{", src, re.M)
        if not m:
            raise SystemExit(f"function {n} not found")
        start = m.start()
        i = m.end() - 1
        depth = 0
        while i < len(src):
            if src[i] == "{": depth += 1
            elif src[i] == "}":
                depth -= 1
                if depth == 0:
                    i += 1
                    break
            i += 1
        out[n] = src[start:i] + "\n"
        src = src[:start] + src[i:]
    return src, out


for legacy_rel, (dest_rel, names) in list(MOVE.items()) + list(MOVE2.items()):
    src = rd("src/" + legacy_rel)
    src, funcs = extract(src, names)
    wr("src/" + legacy_rel, src)
    dest = "src/" + dest_rel
    if os.path.exists(os.path.join(ROOT, dest)):
        d = rd(dest)
    else:
        d = (f"// {dest_rel} - included by src/mod/dishonoredvr.cpp (unity build). The\n"
             f"// view-projection helpers (affine test, shear, lean) the live present path\n"
             f"// uses; they came out of the retired VS-constant scanner.\n\n")
    for n in names:
        d = d.rstrip("\n") + "\n\n" + funcs[n]
    wr(dest, d)
    print(f"moved {names} -> {dest_rel}")

# no-op stand-ins for the diagnostic ticks the live code still calls
STUBS = ["AimWatchArm", "AimWatchReport", "CamPovProbe", "CamPovWiggle", "CamFovWiggle", "CamFovHunt", "CamSeamTick",
         "CameraTraceTick", "SpawnTraceTick", "OvlSceneFrame", "RtDriveUpdate", "RtdSnapshot", "GtCommandVM",
         "GtStop", "GtStart", "GtTick", "RtdMarkerTick", "BoneWigBuildMask", "SbApply", "SbTick", "BoneWigApply", "DbgProbeTick"]
fwd = rd("src/mod/fwd.h")
stubs = ("// legacy/legacy_stubs.inc - no-op stand-ins for the retired diagnostics the live\n"
         "// code still calls, compiled when DVR_WITH_LEGACY is OFF (the default). With it\n"
         "// ON the real bodies in src/legacy are compiled instead. Generated by\n"
         "// tools/refactor-phase7.py from the prototypes in src/mod/fwd.h.\n\n")
for n in STUBS:
    m = re.search(r"^(static [^\n]*\b" + re.escape(n) + r"\s*\([^;]*\));", fwd, re.M)
    if not m:
        raise SystemExit(f"prototype for {n} not found")
    proto = re.sub(r"\s*=\s*[^,)]+(?=[,)])", "", m.group(1))   # a stub may not repeat default arguments
    ret = re.match(r"static\s+(?:inline\s+)?(.+?)\s*\b" + re.escape(n), proto).group(1).strip()
    if ret == "void":
        stubs += proto + " {}\n"
    elif ret == "bool":
        stubs += proto + " { return false; }\n"
    else:
        stubs += proto + " { return 0; }\n"
wr("src/legacy/legacy_stubs.inc", stubs)
print(f"stubs: {len(STUBS)}")

# unity: legacy behind the option; new live modules included
u = re.sub(r'(#define DVR_CAT ::dvr::log::Cat::legacy\n#include "legacy/[^"]+"\n#undef DVR_CAT\n)', r'#if DVR_WITH_LEGACY\n\1#endif\n', u)
u = u.replace("// ---- function bodies by subsystem -------------------------------------------\n",
              "// ---- function bodies by subsystem -------------------------------------------\n"
              "#if !DVR_WITH_LEGACY\n#include \"legacy/legacy_stubs.inc\"\n#endif\n", 1)
if "core/framework/vs_const.cpp" not in u:
    u = u.replace('#define DVR_CAT ::dvr::log::Cat::present\n#include "core/framework/frame_hooks.cpp"\n#undef DVR_CAT\n',
                  '#define DVR_CAT ::dvr::log::Cat::present\n#include "core/framework/frame_hooks.cpp"\n#undef DVR_CAT\n'
                  '#define DVR_CAT ::dvr::log::Cat::present\n#include "core/framework/vs_const.cpp"\n#undef DVR_CAT\n', 1)
wr("src/mod/dishonoredvr.cpp", u)

# the XR bench arm in Direct3DCreate9 is legacy-only
e = rd("src/proxy/d3d9_exports.cpp")
if "#if DVR_WITH_LEGACY" not in e:
    e = re.sub(r"(    \{   // 37\.0: XR-1 bench, armed only by the env var \(xr_bench\.bat\)\n)", "#if DVR_WITH_LEGACY\n\\1", e, 1)
    # close the guard after that block: find the closing of the outer brace introduced above
    i = e.find("#if DVR_WITH_LEGACY\n")
    j = e.find("\n    }\n", i)
    e = e[:j + len("\n    }\n")] + "#endif\n" + e[j + len("\n    }\n"):]
    wr("src/proxy/d3d9_exports.cpp", e)

# CMake: the option value reaches the unity TU as a 0/1 define
c = rd("src/CMakeLists.txt")
if "DVR_WITH_LEGACY=0" not in c:
    c = c.replace("if(DVR_WITH_LEGACY)\n    target_compile_definitions(dvr_proxy PRIVATE DVR_WITH_LEGACY=1)\nendif()",
                  "if(DVR_WITH_LEGACY)\n    target_compile_definitions(dvr_proxy PRIVATE DVR_WITH_LEGACY=1)\nelse()\n    target_compile_definitions(dvr_proxy PRIVATE DVR_WITH_LEGACY=0)\nendif()")
    wr("src/CMakeLists.txt", c)
print("unity + CMake updated")
