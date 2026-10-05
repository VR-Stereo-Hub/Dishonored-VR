# pf2_draw_events_settings.py - series pf (performance), question 2.
#
# QUESTION (two parts, one theme: what the engine already offers for free):
#  A. pf1 showed the scene renderer brackets every stage with D3DPERF events ("PrePass",
#     "BasePass", "ShadowedLights", "LightShafts %s", ...), and the game imports
#     D3DPERF_BeginEvent / EndEvent / GetStatus from d3d9.dll, which is the mod's proxy. What
#     gates the events (which global, who writes it, is D3DPERF_GetStatus consulted, is there
#     a console word)? If the gate can be opened, the proxy receives a named begin/end for
#     every render stage with no hook and no address: a per-stage profiler.
#  B. Which rendering switches does this executable itself know by name (the SystemSettings
#     style keys: bAllow..., ...Shadow..., ...Bloom..., ScreenPercentage, ...)? A key that is
#     not in the image is dead; one that is can be tried from the game's own ini.
#
# Run:  .\tools\ida-run.ps1 tools\ida\pf2_draw_events_settings.py
import os, re
import idaapi, idautils, idc, ida_auto, ida_bytes, ida_funcs, ida_hexrays, ida_name, ida_nalt

OUT_PATH = os.environ.get("DVR_IDA_OUT")
OUT = []


def w(s=""):
    OUT.append(str(s))


def done(code=0):
    if OUT_PATH:
        with open(OUT_PATH, "w", encoding="utf-8") as fh:
            fh.write("\n".join(OUT) + "\n")
    idc.qexit(code)


if not OUT_PATH:
    print("*** DVR_IDA_OUT is not set - run this through tools\\ida-run.ps1 ***")
    idc.qexit(2)

ida_auto.auto_wait()
base = idaapi.get_imagebase()
hr = ida_hexrays.init_hexrays_plugin()
w("input %s  md5 %s" % (ida_nalt.get_root_filename(), (ida_nalt.retrieve_input_file_md5() or b"").hex()))
w("imagebase 0x%X  hexrays %s" % (base, "OK" if hr else "*** MISSING ***"))


def a(ea):
    return "0x%08X (+0x%X)" % (ea, ea - base)


def fname(ea):
    return ida_name.get_name(ea) or ("sub_%X" % ea)


got = ida_bytes.get_bytes(0x00470640, 5) or b""
if got != bytes.fromhex("558BEC6AFF"):
    w("*** REFUSED: known-good ProcessEvent bytes are %s - wrong binary or a game update ***" % got.hex(" "))
    done(3)
w("known-good ProcessEvent 0x00470640 bytes match")


def decomp(ea, limit=120):
    f = ida_funcs.get_func(ea)
    if not f:
        w("*** NO FUNCTION AT %s ***" % a(ea))
        return
    try:
        c = ida_hexrays.decompile(f.start_ea)
    except Exception as e:
        c = None
        w("*** DECOMPILE FAILED %s: %s ***" % (a(f.start_ea), e))
    if not c:
        return
    lines = str(c).split("\n")
    w("")
    w("##### DECOMPILE %s %s (%d lines%s)" % (a(f.start_ea), fname(f.start_ea), len(lines),
                                             "" if len(lines) <= limit else ", showing first %d" % limit))
    for ln in lines[:limit]:
        w("    " + ln)


# ---- A. the draw-event gate ---------------------------------------------------------------
w("")
w("========== A. D3DPERF imports and what gates the stage events ==========")
imports = {}


def imp_cb(ea, name, ordinal):
    if name and name.startswith("D3DPERF"):
        imports[name] = ea
    return True


for i in range(ida_nalt.get_import_module_qty()):
    mod = ida_nalt.get_import_module_name(i) or ""
    if mod.lower().startswith("d3d9"):
        ida_nalt.enum_import_names(i, imp_cb)
if not imports:
    w("*** NO D3DPERF IMPORTS FOUND ***")
for name in sorted(imports):
    iat = imports[name]
    users = []
    for x in idautils.XrefsTo(iat):
        f = ida_funcs.get_func(x.frm)
        users.append((x.frm, f.start_ea if f else None))
    w("import %s IAT %s: %d reference(s)" % (name, a(iat), len(users)))
    for frm, fs in users[:8]:
        w("    from %s in %s" % (a(frm), (a(fs) + " " + fname(fs)) if fs else "(no function: a thunk)"))
    # A thunk (jmp [iat]) has its own callers: one hop more.
    for frm, fs in users[:4]:
        thunk = fs if fs else frm
        callers = list(idautils.CodeRefsTo(thunk, 0))
        if callers:
            fset = {}
            for c in callers:
                cf = ida_funcs.get_func(c)
                if cf:
                    fset[cf.start_ea] = fset.get(cf.start_ea, 0) + 1
            w("    thunk %s has %d call site(s) in %d function(s)%s" %
              (a(thunk), len(callers), len(fset),
               ": " + ", ".join("%s x%d" % (a(k), v) for k, v in sorted(fset.items())[:6]) if len(fset) <= 6 else ""))

# The BeginEvent wrapper pf1 saw before every stage (sub_4DA900, 100 bytes) and GetStatus's users.
decomp(0x004DA900, 60)
if "D3DPERF_GetStatus" in imports:
    seen = set()
    for x in idautils.XrefsTo(imports["D3DPERF_GetStatus"]):
        f = ida_funcs.get_func(x.frm)
        thunk = f.start_ea if f else x.frm
        for c in idautils.CodeRefsTo(thunk, 0):
            cf = ida_funcs.get_func(c)
            if cf and cf.start_ea not in seen:
                seen.add(cf.start_ea)
                w("")
                w("D3DPERF_GetStatus is called from %s in %s" % (a(c), a(cf.start_ea)))
                decomp(cf.start_ea, 70)
        if f and f.start_ea not in seen and (f.end_ea - f.start_ea) > 8:
            seen.add(f.start_ea)
            decomp(f.start_ea, 70)
    if not seen:
        w("D3DPERF_GetStatus: NO CALLER FOUND (imported but unused)")

# Console words and config keys that could open the gate.
w("")
w("strings naming the gate (UTF-16 and ASCII):")
hits = 0
for s in idautils.Strings():
    try:
        t = str(s)
    except Exception:
        continue
    if re.search(r"(?i)drawevents|emitdraw|D3DPERF|PIXEvents|profilegpu", t):
        hits += 1
        refs = [x.frm for x in idautils.XrefsTo(s.ea)]
        w("    %s  '%s'  refs: %s" % (a(s.ea), t[:80], ", ".join(a(r) for r in refs[:4]) or "NONE"))
if not hits:
    w("    NONE")

# ---- B. rendering switches the image knows by name ------------------------------------------
w("")
w("========== B. rendering switch names present in the image ==========")
pat = re.compile(r"^(b(Allow|Use|Enable|Disable|Force|Only)[A-Z]\w+|\w*(Shadow|Bloom|Blur|Distortion|LightShaft|Fog|Decal|"
                 r"ScreenPercentage|Anisotropy|DetailMode|LODBias|Occlusion|DepthOfField|MotionBlur|AmbientOcclusion|Reflection|"
                 r"Trilinear|MultiSample|MSAA|MLAA|FXAA|Translucen|Texel|Resolution|Quality|OneFrameThreadLag|SmoothFrameRate|"
                 r"Subsurface|SoftMask|Particle)\w*)$")
names = {}
for s in idautils.Strings():
    try:
        t = str(s)
    except Exception:
        continue
    if 4 <= len(t) <= 48 and pat.match(t) and " " not in t and "%" not in t:
        n = sum(1 for _ in idautils.XrefsTo(s.ea))
        names[t] = max(names.get(t, 0), n)
w("%d distinct names (name:xrefs; 0 xrefs = a name only reached through a table):" % len(names))
line = []
for t in sorted(names, key=lambda k: k.lower()):
    line.append("%s:%d" % (t, names[t]))
    if len(line) == 6:
        w("    " + "  ".join(line))
        line = []
if line:
    w("    " + "  ".join(line))
w("")
w("END pf2")
done(0)
