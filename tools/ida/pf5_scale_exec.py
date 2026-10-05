# pf5_scale_exec.py - series pf (performance), question 5.
#
# QUESTION: pf3 showed the image knows the engine's rendering switches by name
# (bAllowLightShafts, bAllowDownsampledTranslucency, UseHighQualityBloom, ScreenPercentage,
# ShadowTexelsPerPixel, ...). Stock UE3 can change those LIVE through the console command
# `SCALE` (`scale set <name> <value>`, `scale toggle <name>`, `scale lowend|highend|reset`).
# Does this executable still carry that handler? If it does, each switch can be an A/B plan row
# through the mod's `console` seam word, with no restart and no new engine write.
# Evidence asked for: the console words present, the function that references `SCALE` together
# with its sub-words, and which switch names that function (or the table it walks) reaches.
#
# Run:  .\tools\ida-run.ps1 tools\ida\pf5_scale_exec.py
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


def a(ea):
    return "0x%08X (+0x%X)" % (ea, ea - base)


got = ida_bytes.get_bytes(0x00470640, 5) or b""
if got != bytes.fromhex("558BEC6AFF"):
    w("*** REFUSED: known-good ProcessEvent bytes are %s ***" % got.hex(" "))
    done(3)
w("known-good ProcessEvent 0x00470640 bytes match")

WORDS = ["SCALE", "LOWEND", "HIGHEND", "TOGGLE", "ADJUST", "RESET", "DUMPINI", "Set", "SET", "Toggle", "DUMP",
         "ScreenPercentage", "bAllowLightShafts", "bAllowDownsampledTranslucency", "UseHighQualityBloom",
         "ShadowTexelsPerPixel", "DynamicShadows", "Bloom", "Distortion", "AllowRadialBlur", "MaxShadowResolution"]
strs = idautils.Strings()
strs.setup(strtypes=[ida_nalt.STRTYPE_C, ida_nalt.STRTYPE_C_16], minlen=3)
found = {}
for s in strs:
    try:
        t = str(s)
    except Exception:
        continue
    if t in WORDS:
        found.setdefault(t, []).append(s.ea)
w("")
w("console words and switch names (string address -> functions that reference it):")
funcs_by_word = {}
for word in WORDS:
    eas = found.get(word, [])
    if not eas:
        w("  %-30s NOT IN THE IMAGE" % word)
        continue
    fs = set()
    for ea in eas:
        for x in idautils.XrefsTo(ea):
            f = ida_funcs.get_func(x.frm)
            fs.add(f.start_ea if f else x.frm)
    funcs_by_word[word] = fs
    w("  %-30s %d string(s), referenced from %d place(s): %s" %
      (word, len(eas), len(fs), ", ".join(a(f) for f in sorted(fs)[:6]) + (" ..." if len(fs) > 6 else "")))

# The handler: a function that references SCALE and at least two of its sub-words.
cands = {}
for f in funcs_by_word.get("SCALE", set()):
    n = sum(1 for sub in ("LOWEND", "HIGHEND", "TOGGLE", "ADJUST", "RESET", "DUMPINI", "Set", "SET", "Toggle", "DUMP")
            if f in funcs_by_word.get(sub, set()))
    cands[f] = n
w("")
if not cands:
    w("*** no function references the word SCALE: the SCALE console command is NOT in this image ***")
for f, n in sorted(cands.items(), key=lambda kv: -kv[1]):
    fn = ida_funcs.get_func(f)
    w("function %s references SCALE and %d of its sub-words (%s bytes)" % (a(f), n, (fn.end_ea - fn.start_ea) if fn else "?"))
    if n >= 1 and fn and hr:
        try:
            c = ida_hexrays.decompile(fn.start_ea)
        except Exception as e:
            c = None
            w("  *** DECOMPILE FAILED: %s ***" % e)
        if c:
            lines = str(c).split("\n")
            keep = [ln for ln in lines if re.search(r'"[A-Za-z ]{3,}"|L"', ln)]
            w("  decompile is %d lines; the %d lines that carry a string literal:" % (len(lines), len(keep)))
            for ln in keep[:70]:
                w("    " + ln.strip()[:200])
            if len(keep) > 70:
                w("    ... %d more" % (len(keep) - 70))
w("")
w("END pf5")
done(0)
