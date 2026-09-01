"""Phase 1 of the refactor: cut the single-file proxy into a module tree, verbatim.

The original mod is one 23k-line translation unit. This script splits it into
the planned directory layout WITHOUT changing a single function body:

  src/mod/state/NN_<slug>.inc   everything that is not a function body (includes,
                                types, globals, macros, forward decls), in the
                                ORIGINAL order, chunked at module boundaries
  src/mod/fwd.h                 a prototype for every function, so the bodies can
                                live in any file and any order
  src/<module>.cpp              the function bodies, grouped by subsystem
  src/mod/dishonoredvr.cpp      the unity translation unit that includes all of
                                the above in that order

Compilation is byte-for-byte the same program (one TU, same symbols, same
static linkage); only the text moved. Later phases turn each module into a real
translation unit with its own header. --check re-parses the output tree and
proves every function body is identical to the original.

Usage (repo root):  python tools/split-source.py [--check]
"""
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src", "dllmain.cpp")
OUT = os.path.join(ROOT, "src")

# --------------------------------------------------------------------------
# Module map. Ranges are ORIGINAL (build 38.92, commit 48766c07) line numbers;
# the ported file is re-anchored onto them by function name. Later entries
# win, so put specific sub-ranges after the broad range they carve out.
# --------------------------------------------------------------------------
RANGES = [
    (1, 73, "mod/prelude"),
    (74, 126, "proxy/proxy_state"),
    (127, 186, "legacy/vs_scan"),
    (187, 223, "core/vr/openvr_backend"),
    (224, 462, "core/gfx/d3d11_device"),
    (463, 475, "core/gfx/d3d9_capture"),
    (476, 538, "game/dishonored/head_track"),
    (539, 561, "game/dishonored/patterns"),
    (562, 586, "game/dishonored/fov_lever"),
    (587, 612, "legacy/spacebases"),
    (613, 807, "legacy/rtd_drive"),
    (808, 1088, "core/gfx/hand_mesh"),
    (1089, 1518, "game/dishonored/hands/skelcontrol"),
    (1519, 2027, "game/dishonored/game_state"),
    (2028, 2062, "core/util/log"),
    (2063, 3157, "core/config/config"),
    (3158, 3216, "proxy/d3d9_exports"),
    (3217, 3286, "core/gfx/d3d11_device"),
    (3287, 3547, "core/gfx/eye_quads"),
    (3548, 4106, "core/gfx/hand_mesh"),
    (4107, 4190, "core/gfx/d3d11_device"),
    (4191, 4274, "core/window/game_window"),
    (4275, 4568, "core/config/config"),
    (4569, 5177, "core/ui/overlay"),
    (5178, 5426, "core/gfx/hud_panel"),
    (5427, 5527, "legacy/ovl_scene"),
    (5528, 5595, "core/gfx/eye_quads"),
    (5596, 5831, "core/gfx/present"),
    (5832, 5960, "game/dishonored/ue3/uobject"),
    (5961, 6136, "legacy/ue3_probe"),
    (6137, 6204, "game/dishonored/head_track"),
    (6205, 6467, "legacy/camera_tracer"),
    (6468, 6765, "legacy/fire_tracer"),
    (6766, 6831, "game/dishonored/motion_aim"),
    (6832, 6939, "legacy/aim_watch"),
    (6940, 6945, "core/util/clock"),
    (6946, 7078, "game/dishonored/hands/hand_pose"),
    (7079, 7527, "game/dishonored/motion_aim"),
    (7528, 7650, "legacy/fp_mesh"),
    (7651, 7706, "legacy/camera_hook"),
    (7707, 7743, "game/dishonored/head_track"),
    (7744, 8082, "game/dishonored/head_track"),
    (8083, 8158, "game/dishonored/ue3/uobject"),
    (8159, 8316, "legacy/ue3_probe"),
    (8317, 8468, "game/dishonored/head_track"),
    (8469, 8552, "legacy/camera_hook"),
    (8553, 8681, "core/input/hotkeys"),
    (8682, 8793, "legacy/vs_scan"),
    (8794, 9425, "legacy/fp_mesh"),
    (9426, 9633, "game/dishonored/ue3/uobject"),
    (9634, 9760, "legacy/fp_mesh"),
    (9761, 10013, "game/dishonored/hands/fp_mesh"),
    (10014, 10127, "game/dishonored/shared/ue_math"),
    (10128, 10141, "game/dishonored/hands/fp_mesh"),
    (10142, 10781, "legacy/rtd_drive"),
    (10782, 11363, "game/dishonored/hands/fp_mesh"),
    (11364, 11430, "game/dishonored/hands/arms_hide"),
    (11431, 11569, "game/dishonored/ue3/uobject"),
    (11570, 11690, "game/dishonored/block_state"),
    (11691, 11763, "game/dishonored/crouch"),
    (11764, 11869, "game/dishonored/hands/arms_hide"),
    (11870, 12015, "legacy/ue3_probe"),
    (12016, 12042, "core/gfx/hand_mesh"),
    (12043, 12087, "game/dishonored/hands/skelcontrol"),
    (12088, 12350, "game/dishonored/hands/graft"),
    (12351, 12707, "game/dishonored/hands/skelcontrol"),
    (12708, 14040, "game/dishonored/blink"),
    (14041, 14063, "game/dishonored/hands/skelcontrol"),
    (14064, 14577, "game/dishonored/crouch"),
    (14578, 14597, "game/dishonored/hands/arms_hide"),
    (14598, 15230, "game/dishonored/hands/skelcontrol"),
    (15231, 15372, "legacy/cam_seam"),
    (15373, 15429, "core/window/res_spoof"),
    (15430, 15673, "legacy/cam_seam"),
    (15674, 15711, "legacy/ue3_probe"),
    (15712, 15859, "game/dishonored/hands/skelcontrol"),
    (15860, 16006, "game/dishonored/head_track"),
    (16007, 16155, "game/dishonored/fov_lever"),
    (16156, 16418, "legacy/spacebases"),
    (16419, 16495, "game/dishonored/hands/skelcontrol"),
    (16496, 16560, "legacy/spacebases"),
    (16561, 16773, "game/dishonored/console"),
    (16774, 17388, "game/dishonored/ue3/process_event"),
    (17389, 17709, "core/framework/frame_hooks"),
    (17710, 17743, "legacy/vs_scan"),
    (17744, 17763, "game/dishonored/game_state"),
    (17764, 18168, "game/dishonored/head_track"),
    (18169, 18367, "core/input/pad_bridge"),
    (18368, 18410, "core/vr/openvr_backend"),
    (18411, 18531, "game/dishonored/melee"),
    (18532, 19126, "core/input/pad_bridge"),
    (19127, 19161, "core/hooks/iat"),
    (19162, 19552, "core/window/res_spoof"),
    (19553, 19579, "core/input/pad_bridge"),
    (19580, 19596, "core/util/ini"),
    (19597, 19982, "core/vr/openvr_backend"),
    (19983, 20075, "legacy/rtd_drive"),
    (20076, 20513, "core/gfx/present"),
    (20514, 20561, "core/window/res_spoof"),
    (20562, 20979, "core/framework/frame_hooks"),
    (20980, 21078, "core/window/res_spoof"),
    (21079, 21133, "core/framework/frame_hooks"),
    (21134, 21225, "core/window/res_spoof"),
    (21226, 21312, "core/framework/frame_hooks"),
    (21313, 21411, "core/vr/openxr_loader"),
    (21412, 21524, "core/vr/openxr_backend"),
    (21525, 21773, "core/vr/openxr_input"),
    (21774, 21845, "core/util/crash"),
    (21846, 22334, "core/vr/openxr_backend"),
    (22335, 22640, "core/vr/openxr_pace"),
    (22641, 22827, "legacy/xr_bench"),
    (22828, 22901, "proxy/d3d9_exports"),
    (22902, 22999, "proxy/dllmain"),
]

# Functions whose home is not the range they sit in.
NAME_OVERRIDES = {
    "SkcAlive": "game/dishonored/hands/skelcontrol",
    "GraftDonorAlive": "game/dishonored/hands/graft",
    "GraftEmergencyRestore": "game/dishonored/hands/graft",
    "PatchVtable": "core/hooks/vtable",
    "EnsureRealD3D9": "proxy/d3d9_exports",
    "RangeReadable": "core/util/mem",
    "SafeRead32": "core/util/mem",
    "WalkVEH": "legacy/ue3_probe",
    "MaimNowMs": "core/util/clock",
    "RotAbout": "game/dishonored/shared/ue_math",
    "RecenterHead": "game/dishonored/head_track",
    "UpdateHeadInject": "game/dishonored/head_track",
    "PeLatch": "game/dishonored/ue3/process_event",
    "BlockCfgLoad": "game/dishonored/block_state",
    "BlockCfgSave": "game/dishonored/block_state",
    "BlockFingerprint": "game/dishonored/block_state",
    "HmPickModels": "core/gfx/hand_mesh",
    "SkcSaveNeutral": "game/dishonored/hands/skelcontrol",
    "DumpVSConstScan": "legacy/vs_scan",
    "MaimHaptic": "core/vr/openvr_backend",
    "WriteTextFile": "core/util/ini",
    "IniFloat": "core/util/ini",
    "Log": "core/util/log",
    "LogFlush": "core/util/log",
    "XrVeh": "core/util/crash",
    "XrLoadRuntime": "core/vr/openxr_loader",
    "XrBenchThread": "legacy/xr_bench",
    "DllMain": "proxy/dllmain",
    "BlinkAimStub": "game/dishonored/blink_stubs",
    "BlinkDirStub": "game/dishonored/blink_stubs",
    "BlinkDestStub": "game/dishonored/blink_stubs",
    "BlinkTraceStub": "game/dishonored/blink_stubs",
    "InjectCameraMatrix": "legacy/camera_hook",
    "MenuStep": "game/dishonored/game_state",
    "CineActive": "game/dishonored/game_state",
    "SprintBit": "game/dishonored/game_state",
    "SlideAssist": "game/dishonored/game_state",
    "CylTruthLive": "game/dishonored/game_state",
    "FocusGuardTick": "core/window/res_spoof",
    "RenderSizeTick": "core/window/res_spoof",
    "ObjClassName": "game/dishonored/ue3/uobject",
    "ObjNearCamVec": "legacy/fp_mesh",
    "WeaponObjScan": "legacy/fp_mesh",
    "RtdMarkerTick": "legacy/rtd_drive",
    "StereoUpdate": "core/input/hotkeys",
    "FindPropOffset": "game/dishonored/ue3/uobject",
    "FindBoolProp": "game/dishonored/ue3/uobject",
    "FindNameIdx": "game/dishonored/ue3/uobject",
    "FindFunctionObj": "game/dishonored/ue3/uobject",
    "hkSetVSConstF": "core/framework/frame_hooks",
    "hkSetRenderTarget": "core/framework/frame_hooks",
    "hkPresent": "core/framework/frame_hooks",
    "hkReset": "core/framework/frame_hooks",
    "hkCreateDevice": "core/framework/frame_hooks",
    "hkGetAdapterDisplayMode": "core/window/res_spoof",
    "hkGetAdapterModeCount": "core/window/res_spoof",
    "hkEnumAdapterModes": "core/window/res_spoof",
    "VrExtraMode": "core/window/res_spoof",
    "UncapPresent": "core/window/res_spoof",
    "ForceRes": "core/window/res_spoof",
    "ApplyRenderWindowSize": "core/window/res_spoof",
}

ORIGIN_NUMBERING_NOTE = ("Line numbers in comments and docs refer to the original single file "
                         "(src/dllmain.cpp at commit 48766c07, proxy build 38.92).")


# --------------------------------------------------------------------------
# Lexer: mark every character as code / string / char / comment so brace and
# semicolon matching ignores literals and comments.
# --------------------------------------------------------------------------
def code_mask(text):
    n = len(text)
    mask = bytearray(b"c" * n)
    i = 0
    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if ch == "/" and nxt == "/":
            j = text.find("\n", i)
            j = n if j < 0 else j
            mask[i:j] = b"/" * (j - i)
            i = j
        elif ch == "/" and nxt == "*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            mask[i:j] = b"*" * (j - i)
            i = j
        elif ch == "R" and nxt == '"' and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == "_")):
            # raw string literal R"delim( ... )delim"
            p = text.find("(", i + 2)
            delim = text[i + 2:p]
            j = text.find(")" + delim + '"', p)
            j = n if j < 0 else j + len(delim) + 2
            mask[i:j] = b"s" * (j - i)
            i = j
        elif ch == '"' or ch == "'":
            q = ch
            j = i + 1
            while j < n:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == q or text[j] == "\n":
                    break
                j += 1
            j = min(j + 1, n)
            mask[i:j] = b"s" * (j - i)
            i = j
        else:
            i += 1
    return mask


def parse_items(text):
    """Yield (kind, start_line, end_line, name) over the file. Lines are 0-based,
    end exclusive. kind is 'func' or 'other'. Whitespace/comment runs between
    items are folded into the following item."""
    mask = code_mask(text)
    lines = text.split("\n")
    # line start offsets
    offs = [0]
    for ln in lines:
        offs.append(offs[-1] + len(ln) + 1)
    items = []
    i = 0
    n = len(lines)
    pending = 0  # first line of the pending comment/blank run
    while i < n:
        ln = lines[i]
        st = ln.strip()
        if st == "" or st.startswith("//") or st.startswith("/*") or st.startswith("*"):
            if st.startswith("/*"):
                # block comment at column 0: skip to its end
                while i < n and "*/" not in lines[i]:
                    i += 1
            i += 1
            continue
        if ln.startswith("#"):
            j = i
            while lines[j].rstrip().endswith("\\"):
                j += 1
            items.append(("other", pending, j + 1, lines[i].split()[0]))
            i = j + 1
            pending = i
            continue
        if ln[0] in " \t}":
            raise SystemExit(f"unexpected column-0 text at line {i + 1}: {ln[:60]}")
        # a declaration/definition starts here; scan to ';' at depth 0 or the body '{'
        pos = offs[i]
        depth = 0
        paren = 0
        head_end = None
        kind = None
        k = pos
        while k < len(text):
            if mask[k] != ord("c"):
                k += 1
                continue
            c = text[k]
            if c == "(":
                paren += 1
            elif c == ")":
                paren -= 1
            elif c == ";" and paren == 0 and depth == 0:
                kind = "other"
                head_end = k
                break
            elif c == "{" and paren == 0:
                if depth == 0:
                    head = text[pos:k]
                    head_code = strip_comments(head, mask, pos)
                    first = head_code.strip().split()[0] if head_code.strip() else ""
                    before_paren = head_code.split("(", 1)[0] if "(" in head_code else head_code
                    is_func = ("(" in head_code and "=" not in before_paren
                               and first not in ("struct", "enum", "class", "union", "typedef", "namespace"))
                    if is_func:
                        kind = "func"
                        head_end = k
                        # find the matching brace
                        d = 0
                        m = k
                        while m < len(text):
                            if mask[m] == ord("c"):
                                if text[m] == "{":
                                    d += 1
                                elif text[m] == "}":
                                    d -= 1
                                    if d == 0:
                                        break
                            m += 1
                        k = m
                        break
                depth += 1
            elif c == "}" and paren == 0:
                depth -= 1
            k += 1
        if kind is None:
            raise SystemExit(f"could not delimit item at line {i + 1}")
        end_line = text.count("\n", 0, k) + 1  # line index after the item's last line
        if kind == "func":
            head = text[pos:head_end]
            head_code = strip_comments(head, mask, pos)
            m = list(re.finditer(r"([A-Za-z_]\w*)\s*\(", head_code))
            name = m[0].group(1)
            for mm in m:
                if mm.group(1) not in ("WINAPI", "__stdcall", "__cdecl", "__declspec", "__fastcall"):
                    name = mm.group(1)
                    break
            items.append(("func", pending, end_line, name, head_code.strip(), head))
        else:
            first = lines[i].split()[0] if lines[i].split() else ""
            items.append(("other", pending, end_line, first))
        i = end_line
        pending = i
    if pending < n:
        items.append(("other", pending, n, "<tail>"))
    return items, lines


def strip_comments(head, mask, pos):
    """Blank the comment characters of a head, keep strings (extern "C" matters)."""
    return "".join(" " if mask[pos + q] in (ord("/"), ord("*")) else ch for q, ch in enumerate(head))


def strip_defaults(head):
    """Remove default arguments from a parameter list: a prototype carries them,
    so the definition must not repeat them (C++ default-argument rule)."""
    i = head.find("(")
    if i < 0:
        return head
    params = head[i:]
    params = re.sub(r"\s*=\s*[^,)]+(?=[,)])", "", params)
    return head[:i] + params


def module_for(orig_line, name):
    if name in NAME_OVERRIDES:
        return NAME_OVERRIDES[name]
    mod = None
    for lo, hi, m in RANGES:
        if lo <= orig_line <= hi:
            mod = m
    return mod or "mod/unsorted"


def slug(mod):
    return mod.replace("/", "_")


def prototype(head):
    h = re.sub(r"\s+", " ", head).strip()
    return h + ";"


def main():
    check = "--check" in sys.argv
    import subprocess
    if os.path.exists(SRC):
        text = io.open(SRC, encoding="utf-8", newline="").read().replace("\r\n", "\n")
    else:
        # the single file is gone from the tree after Phase 1; the MSVC-ported
        # version it was split from is the last commit that still had it
        text = subprocess.run(["git", "show", "e361a449:src/dllmain.cpp"], cwd=ROOT,
                              capture_output=True, check=True).stdout.decode("utf-8").replace("\r\n", "\n")
    orig = subprocess.run(["git", "show", "48766c07:src/dllmain.cpp"], cwd=ROOT,
                          capture_output=True, check=True).stdout.decode("utf-8").replace("\r\n", "\n")
    items, lines = parse_items(text)
    oitems, _ = parse_items(orig)
    # anchor: function name -> original 1-based start line of its head
    def head_line(it, ls):
        s = it[1]
        while ls[s].strip() == "" or ls[s].startswith("//"):
            s += 1
        return s + 1
    olines = orig.split("\n")
    oanchor = {it[3]: head_line(it, olines) for it in oitems if it[0] == "func"}
    anchors = []  # (ported_line, delta)
    for it in items:
        if it[0] == "func" and it[3] in oanchor:
            hl = head_line(it, lines)
            anchors.append((hl, oanchor[it[3]] - hl))
    anchors.sort()

    def orig_line_of(ported_line):
        d = 0
        for pl, dd in anchors:
            if pl <= ported_line:
                d = dd
            else:
                break
        return ported_line + d

    funcs = [it for it in items if it[0] == "func"]
    print(f"items: {len(items)}  functions: {len(funcs)}  others: {len(items) - len(funcs)}")

    # ---- assign ----
    state_chunks = []   # list of (slug, [text])
    modules = {}        # mod -> [text]
    protos = []
    last_state_mod = None
    for it in items:
        kind, s, e = it[0], it[1], it[2]
        body = "\n".join(lines[s:e]) + "\n"
        if kind == "func":
            name = it[3]
            hl = head_line(it, lines)
            mod = module_for(orig_line_of(hl), name)
            head = it[4]
            raw_head = it[5]
            if strip_defaults(raw_head) != raw_head:
                # the prototype carries the defaults; the definition may not repeat them
                body = body.replace(raw_head, strip_defaults(raw_head), 1)
            modules.setdefault(mod, []).append(body)
            if not head.startswith('extern "C"'):
                protos.append(prototype(head))
        else:
            hl = s + 1
            mod = module_for(orig_line_of(hl), "")
            if mod != last_state_mod:
                state_chunks.append((mod, []))
                last_state_mod = mod
            state_chunks[-1][1].append(body)

    if check:
        return verify(items, lines, modules)

    # ---- write ----
    written = []
    state_dir = os.path.join(OUT, "mod", "state")
    os.makedirs(state_dir, exist_ok=True)
    unity = ["// Dishonored VR - unity translation unit (Phase 1 of the refactor).",
             "//",
             "// The mod was one 23k-line file. tools/split-source.py cut it into the module",
             "// tree below without changing a single function body: state (includes, types,",
             "// globals) in the original order, then a prototype for every function, then",
             "// the bodies grouped by subsystem. Everything is still ONE translation unit",
             "// with the same static linkage, so the program is byte-for-byte the same;",
             "// later phases give each module its own header and translation unit and",
             "// drop it from this list.",
             "//",
             "// " + ORIGIN_NUMBERING_NOTE,
             "",
             "// ---- state: includes, types, globals, macros (original order) ----------"]
    for idx, (mod, bodies) in enumerate(state_chunks):
        fn = f"{idx:02d}_{slug(mod)}.inc"
        path = os.path.join(state_dir, fn)
        hdr = (f"// State chunk {idx:02d} for {mod} - included by src/mod/dishonoredvr.cpp.\n"
               f"// Original order is preserved across chunks; do not reorder.\n\n")
        io.open(path, "w", encoding="utf-8", newline="\n").write(hdr + "".join(bodies))
        unity.append(f'#include "mod/state/{fn}"')
        written.append(path)
    fwd = os.path.join(OUT, "mod", "fwd.h")
    io.open(fwd, "w", encoding="utf-8", newline="\n").write(
        "// Prototypes for every function of the unity build, generated by\n"
        "// tools/split-source.py so function bodies can live in any file.\n#pragma once\n\n"
        + "\n".join(protos) + "\n")
    unity.append("")
    unity.append("// ---- every function, so the bodies below can be in any order --------------")
    unity.append('#include "mod/fwd.h"')
    unity.append("")
    unity.append("// ---- function bodies by subsystem -------------------------------------------")
    for mod in sorted(modules):
        path = os.path.join(OUT, mod + ".cpp")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        hdr = (f"// {mod}.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this\n"
               f"// module gets its own header and translation unit. Bodies are verbatim from\n"
               f"// the original single file; {ORIGIN_NUMBERING_NOTE}\n\n")
        io.open(path, "w", encoding="utf-8", newline="\n").write(hdr + "\n".join(modules[mod]))
        unity.append(f'#include "{mod}.cpp"')
        written.append(path)
    io.open(os.path.join(OUT, "mod", "dishonoredvr.cpp"), "w", encoding="utf-8", newline="\n").write(
        "\n".join(unity) + "\n")
    print(f"wrote {len(written) + 2} files; state chunks {len(state_chunks)}, modules {len(modules)}, prototypes {len(protos)}")
    for mod in sorted(modules):
        print(f"  {mod}.cpp: {len(modules[mod])} functions, {sum(b.count(chr(10)) for b in modules[mod])} lines")
    return 0


def verify(items, lines, modules):
    """Re-parse every generated module file and compare function bodies."""
    import subprocess
    orig = subprocess.run(["git", "show", "48766c07:src/dllmain.cpp"], cwd=ROOT,
                          capture_output=True, check=True).stdout.decode("utf-8").replace("\r\n", "\n")
    oitems, olines = parse_items(orig)
    def bodies(its, ls):
        out = {}
        for it in its:
            if it[0] != "func":
                continue
            s = it[1]
            while ls[s].strip() == "" or ls[s].startswith("//"):
                s += 1
            t = "\n".join(ls[s:it[2]])
            t = t.replace(it[5], strip_defaults(it[5]), 1)
            # the em-dash sweep (repo rule: hyphens only) is not a body change
            t = re.sub("\\s*\u2014\\s*", " - ", t)
            t = re.sub(r"\s*-\s*", " - ", t)
            out[it[3]] = t
        return out
    ob = bodies(oitems, olines)
    # functions replaced by a real module with its own API (Phase 2+): their
    # original bodies are gone on purpose
    CONVERTED = {"Log", "LogFlush", "XrVeh", "MaimNowMs", "RangeReadable", "SafeRead32",
                 "IniFloat", "WriteTextFile", "PatchVtable", "FindIatSlotIn", "FindIatSlot"}
    nb = {}
    import glob as _glob
    files = set(os.path.join(OUT, mod + ".cpp") for mod in modules)
    files |= set(_glob.glob(os.path.join(OUT, "core", "**", "*.cpp"), recursive=True))
    files |= set(_glob.glob(os.path.join(OUT, "game", "**", "*.cpp"), recursive=True))
    files |= set(_glob.glob(os.path.join(OUT, "legacy", "*.cpp")))
    for path in sorted(files):
        if not os.path.exists(path):
            continue
        t = io.open(path, encoding="utf-8", newline="").read().replace("\r\n", "\n")
        try:
            its, ls = parse_items(t)
        except SystemExit:
            continue   # a real module (namespaces): not unity-style, nothing to compare
        nb.update(bodies(its, ls))
    changed = [n for n in ob if n in nb and ob[n] != nb[n]]
    missing = [n for n in ob if n not in nb and n not in CONVERTED]
    extra = [n for n in nb if n not in ob]
    print(f"check-move: {len(ob)} original functions, {len(nb)} in tree; "
          f"changed {len(changed)}, missing {len(missing)}, extra {len(extra)}")
    for n in changed:
        print("  CHANGED:", n)
    for n in missing:
        print("  MISSING:", n)
    for n in extra:
        print("  EXTRA:", n)
    return 0 if not (changed or missing) else 1


if __name__ == "__main__":
    sys.exit(main())
