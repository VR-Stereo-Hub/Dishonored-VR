# analyze.py - the one-time auto-analysis of a staged binary, and a health report.
# Run by `.\tools\ida-run.ps1 -Stage` (or alone: `.\tools\ida-run.ps1 tools\ida\analyze.py`).
# IDA saves the .i64 when qexit runs; every later script opens that database and skips
# analysis. The report is what proves the headless pipeline works end to end: IDAPython
# ran, the decompiler is licensed for x86, and a published address decompiles.
import os, time
import idaapi, idautils, idc, ida_auto, ida_bytes, ida_funcs, ida_hexrays, ida_nalt

OUT_PATH = os.environ.get("DVR_IDA_OUT")
if not OUT_PATH:
    print("*** DVR_IDA_OUT is not set - run this through tools\\ida-run.ps1 ***")
    idc.qexit(2)

t0 = time.time()
ida_auto.auto_wait()
OUT = []
w = OUT.append
base = idaapi.get_imagebase()
w("input %s  md5 %s" % (ida_nalt.get_root_filename(), (ida_nalt.retrieve_input_file_md5() or b"").hex()))
w("imagebase 0x%X  bitness %d  auto_wait %.1fs" % (base, 64 if idaapi.inf_is_64bit() else 32, time.time() - t0))
for s in idautils.Segments():
    w("segment %-8s 0x%08X-0x%08X" % (idc.get_segm_name(s), s, idc.get_segm_end(s)))
w("functions %d" % sum(1 for _ in idautils.Functions()))
hr = ida_hexrays.init_hexrays_plugin()
w("hexrays %s" % ("OK" if hr else "*** MISSING ***"))

# Known-good: UObject::ProcessEvent (ENGINE_NOTES kProcessEvent, prologue 55 8B EC 6A FF).
pe = 0x00470640
got = ida_bytes.get_bytes(pe, 5) or b""
w("known-good ProcessEvent 0x%08X bytes %s %s" % (pe, got.hex(" "), "MATCH" if got == bytes.fromhex("558BEC6AFF") else "*** MISMATCH ***"))
f = ida_funcs.get_func(pe)
w("function at ProcessEvent: %s" % ("0x%08X size %d" % (f.start_ea, f.end_ea - f.start_ea) if f else "*** NONE ***"))
if hr and f:
    try:
        n = len(str(ida_hexrays.decompile(pe)).split("\n"))
        w("decompile ProcessEvent: OK, %d lines" % n)
    except Exception as e:
        w("decompile ProcessEvent: *** FAILED: %s ***" % e)

with open(OUT_PATH, "w", encoding="utf-8") as fh:
    fh.write("\n".join(OUT) + "\n")
idc.qexit(0)
