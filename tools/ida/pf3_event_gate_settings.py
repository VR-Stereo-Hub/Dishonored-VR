# pf3_event_gate_settings.py - series pf (performance), question 3.
#
# QUESTION: pf2 found the stage-event constructor (0x004DA900 formats a name and calls the
# D3DPERF_BeginEvent wrapper 0x009B5EA0) but not what GATES its call sites. In the scene render
# function the constructor is called at 0x0086C1BB: which global does the code test before it,
# who writes that global, and what do the BeginEvent / SetOptions wrappers do? Part B repeats
# pf2's switch-name census with UTF-16 strings included (pf2 listed C strings only, and the
# engine's config keys are wide).
#
# Run:  .\tools\ida-run.ps1 tools\ida\pf3_event_gate_settings.py
import os, re
import idaapi, idautils, idc, ida_auto, ida_bytes, ida_funcs, ida_hexrays, ida_name, ida_nalt, ida_xref

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
    w("*** REFUSED: known-good ProcessEvent bytes are %s ***" % got.hex(" "))
    done(3)
w("known-good ProcessEvent 0x00470640 bytes match")


def decomp(ea, limit=80):
    f = ida_funcs.get_func(ea)
    if not f:
        w("*** NO FUNCTION AT %s ***" % a(ea))
        return
    try:
        c = ida_hexrays.decompile(f.start_ea)
    except Exception as e:
        w("*** DECOMPILE FAILED %s: %s ***" % (a(f.start_ea), e))
        return
    lines = str(c).split("\n")
    w("")
    w("##### DECOMPILE %s %s (%d lines%s)" % (a(f.start_ea), fname(f.start_ea), len(lines),
                                             "" if len(lines) <= limit else ", showing first %d" % limit))
    for ln in lines[:limit]:
        w("    " + ln)


def window(first, last):
    w("")
    w("##### DISASM %s .. %s" % (a(first), a(last)))
    ea = first
    while ea <= last and ea != idaapi.BADADDR:
        raw = ida_bytes.get_bytes(ea, idc.get_item_size(ea)) or b""
        w("  %s  %-24s %s" % (a(ea), raw.hex(" "), idc.generate_disasm_line(ea, 0)))
        ea = idc.next_head(ea)


# ---- A. the gate --------------------------------------------------------------------------
w("")
w("========== A. what gates the stage events ==========")
window(0x0086C0DA, 0x0086C215)

# Every data global the scene render function reads in a cmp/test before the event constructor.
cands = {}
f = ida_funcs.get_func(0x0086C060)
for it in idautils.FuncItems(f.start_ea):
    if idc.print_insn_mnem(it) in ("cmp", "test", "mov"):
        for d in idautils.DataRefsFrom(it):
            if 0x01269000 <= d < 0x01485000:   # the .data segments (analyze.py output)
                cands.setdefault(d, []).append(it)
w("")
w("data globals the scene render function reads (candidate gates): %d" % len(cands))
for d in sorted(cands):
    rd = wr = 0
    writers = []
    for x in idautils.XrefsTo(d):
        if x.type == ida_xref.dr_W:
            wr += 1
            wf = ida_funcs.get_func(x.frm)
            writers.append("%s in %s" % (a(x.frm), a(wf.start_ea) if wf else "?"))
        elif x.type == ida_xref.dr_R:
            rd += 1
    w("  %s %s: read here at %s | whole image: %d reads, %d writes%s" %
      (a(d), fname(d), ", ".join("0x%08X" % i for i in cands[d][:3]), rd, wr,
       (" | writers: " + "; ".join(writers[:5])) if writers else " | NO WRITER (initialised data, or written through a pointer)"))

decomp(0x009B5EA0, 40)    # the BeginEvent wrapper
decomp(0x009B5EC0, 30)    # the EndEvent wrapper
decomp(0x009BC1E0, 60)    # the only caller of D3DPERF_SetOptions

# ---- B. switch names, wide strings included -----------------------------------------------
w("")
w("========== B. rendering switch names in the image (C and UTF-16 strings) ==========")
strs = idautils.Strings()
strs.setup(strtypes=[ida_nalt.STRTYPE_C, ida_nalt.STRTYPE_C_16], minlen=5)
pat = re.compile(r"^(b(Allow|Use|Enable|Disable|Force|Only|Emit)[A-Z]\w+|(Max|Min)?\w*(Shadow|Bloom|Blur|Distortion|LightShaft|"
                 r"ScreenPercentage|Anisotropy|DetailMode|LODBias|DepthOfField|MotionBlur|AmbientOcclusion|Trilinear|MultiSample|"
                 r"MLAA|FXAA|Texel|OneFrameThreadLag|SmoothFrameRate|Subsurface|DrawEvents|ThreadedRendering|VSync)\w*)$")
names = {}
total = 0
for s in strs:
    total += 1
    try:
        t = str(s)
    except Exception:
        continue
    if 5 <= len(t) <= 44 and pat.match(t) and not t.startswith(("A", "U")) or (5 <= len(t) <= 44 and pat.match(t) and "exec" not in t):
        if "exec" in t or t.startswith("NX_"):
            continue
        n = sum(1 for _ in idautils.XrefsTo(s.ea))
        names[t] = max(names.get(t, 0), n)
w("%d strings scanned, %d distinct switch-like names (name:xrefs):" % (total, len(names)))
line = []
for t in sorted(names, key=lambda k: k.lower()):
    line.append("%s:%d" % (t, names[t]))
    if len(line) == 5:
        w("    " + "  ".join(line))
        line = []
if line:
    w("    " + "  ".join(line))
w("")
w("END pf3")
done(0)
