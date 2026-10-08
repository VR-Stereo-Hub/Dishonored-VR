# cb1_fulldecomp.py - a THIRD-PARTY DLL staged in the IDA workspace (ida-run.ps1 -Module <x.dll>): full decompile
# of every function to <script>_<module>_decomp.c, plus an index: string -> functions, immediates in the
# game exe range (the engine addresses the DLL hard-codes), function sizes. The outputs are the
# third party's copyrighted code: they stay in the IDA workspace's out folder (gitignored) and are summarised in docs.
import os, time
import idaapi, idautils, idc, ida_auto, ida_bytes, ida_funcs, ida_hexrays, ida_name, ida_nalt, ida_ua

OUT_PATH = os.environ.get("DVR_IDA_OUT")
if not OUT_PATH:
    print("*** DVR_IDA_OUT not set ***"); idc.qexit(2)
t0 = time.time()
ida_auto.auto_wait()
base = idaapi.get_imagebase()
hr = ida_hexrays.init_hexrays_plugin()
outdir = os.path.dirname(OUT_PATH)
IDX = []; w = IDX.append
w("input %s md5 %s imagebase 0x%X hexrays %s auto_wait %.0fs" % (ida_nalt.get_root_filename(), (ida_nalt.retrieve_input_file_md5() or b"").hex(), base, "OK" if hr else "MISSING", time.time()-t0))
def a(ea): return "0x%08X (+0x%X)" % (ea, ea-base)
def fname(ea): return ida_name.get_name(ea) or ("sub_%X" % ea)
funcs = list(idautils.Functions())
w("functions %d" % len(funcs))
# strings -> referencing functions
w(""); w("##### STRING XREFS (string -> functions)")
sx = {}
for s in idautils.Strings():
    txt = str(s)
    if len(txt) < 4: continue
    for x in idautils.DataRefsTo(s.ea):
        f = ida_funcs.get_func(x)
        if f: sx.setdefault(f.start_ea, []).append(txt)
    users = sorted(set(ida_funcs.get_func(x).start_ea for x in idautils.DataRefsTo(s.ea) if ida_funcs.get_func(x)))
    if users: w("  %-60r <- %s" % (txt[:60], " ".join(a(u) for u in users)))
# immediates in game exe range
w(""); w("##### GAME-ADDRESS IMMEDIATES (0x00400000..0x01600000) by function")
GLO, GHI = 0x00401000, 0x01600000
imm = {}
for f in funcs:
    for it in idautils.FuncItems(f):
        insn = ida_ua.insn_t()
        if not ida_ua.decode_insn(insn, it): continue
        for op in insn.ops:
            if op.type in (ida_ua.o_imm, ida_ua.o_mem) and GLO <= op.value <= GHI and op.type == ida_ua.o_imm:
                imm.setdefault(f, set()).add(op.value)
            elif op.type == ida_ua.o_mem and GLO <= op.addr <= GHI:
                imm.setdefault(f, set()).add(op.addr)
for f in sorted(imm):
    w("  %s %-30s %s" % (a(f), fname(f)[:30], " ".join("0x%08X" % v for v in sorted(imm[f]))))
# also data-segment dwords in that range (tables of engine addresses)
w(""); w("##### DATA DWORDS IN GAME RANGE (.data/.rdata)")
for seg in idautils.Segments():
    nm = idc.get_segm_name(seg)
    if nm not in (".data", ".rdata"): continue
    ea = seg; end = idc.get_segm_end(seg)
    while ea + 4 <= end:
        v = ida_bytes.get_dword(ea)
        if GLO <= v <= GHI and v % 2 == 0:
            w("  %s %s = 0x%08X" % (nm, a(ea), v))
        ea += 4
# full decompile
w(""); w("##### FUNCTION INDEX (ea size name -> see the _decomp.c beside this file)")
C = []
fails = 0
for f in funcs:
    fn = ida_funcs.get_func(f)
    w("  %s %6d %s" % (a(f), fn.end_ea-fn.start_ea, fname(f)))
    C.append("// ===== %s %s size %d  strings: %s" % (a(f), fname(f), fn.end_ea-fn.start_ea, " | ".join(t[:50] for t in sx.get(f, [])[:8])))
    if hr:
        try:
            C.append(str(ida_hexrays.decompile(f)))
        except Exception as e:
            fails += 1; C.append("// DECOMPILE FAILED: %s" % e)
            for it in idautils.FuncItems(f):
                raw = ida_bytes.get_bytes(it, idc.get_item_size(it)) or b""
                C.append("//   %s  %-20s %s" % (a(it), raw.hex(" "), idc.generate_disasm_line(it, 0)))
    C.append("")
w("decompile failures %d  elapsed %.0fs" % (fails, time.time()-t0))
with open(os.path.join(outdir, "cb1_fulldecomp_%s_decomp.c" % os.path.splitext(ida_nalt.get_root_filename())[0]), "w", encoding="utf-8") as fh: fh.write("\n".join(C))
with open(OUT_PATH, "w", encoding="utf-8") as fh: fh.write("\n".join(IDX)+"\n")
idc.qexit(0)
