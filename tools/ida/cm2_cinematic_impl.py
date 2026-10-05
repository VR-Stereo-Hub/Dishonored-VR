# CM-2 (static, Dishonored.exe = the staged copy): WHAT does SetCinematicMode hide when
# bHidePlayer is set - the pawn (bHidden), the first-person mesh component, the body mode?
# Targets: the implementations behind the CM-1 thunks: DishonoredPlayerController vtable
# 0x01118738 slot +0x580 (PreSetCinematicMode) -> 0x00AA33A0 and +0x584 (SetCinematicMode) -> 0x00AAF150,
# plus every direct callee one hop down. Run: .\tools\ida-run.ps1 tools\ida\cm2_cinematic_impl.py
#
# Rules this skeleton encodes (docs/IDA_WORKFLOW.md says why each one exists):
# - auto_wait() first, qexit(0) last: without qexit headless IDA never exits.
# - The output path comes from DVR_IDA_OUT, set by ida-run.ps1. It is OUTSIDE the repo:
#   a decompile is game-derived text and is never committed. Findings go to ENGINE_NOTES.
# - Print every address as VA AND RVA. The exe has no ASLR (base 0x400000), so
#   patterns.h stores VAs; the RE tools (disasm-rva.py, pe-xref.ps1) speak RVAs.
# - Every negative path prints something: NO FUNCTION AT, DECOMPILE FAILED, NONE, CUT.
# - Print instruction BYTES: a hook byte-verifies its target before it writes.
# - The known-good check below re-derives an address ENGINE_NOTES already publishes and
#   refuses on a mismatch, like ue3-natives.py --verify. Keep it in every copy.
# - 32-bit: vtable slots are 4 bytes; MSVC x86 RTTI stores ABSOLUTE pointers.
# - Save without a UTF-8 BOM (IDA refuses it: SyntaxError U+FEFF). ida-run.ps1 checks.
import os
import idaapi, idautils, idc, ida_auto, ida_bytes, ida_funcs, ida_hexrays, ida_name, ida_nalt

TARGETS = [0x00AA33A0, 0x00AAF150]

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
w("imagebase 0x%X  hexrays %s" % (base, "OK" if hr else "*** MISSING (no x86 decompiler licence?) ***"))


def a(ea):
    return "0x%08X (+0x%X)" % (ea, ea - base)


def fname(ea):
    return ida_name.get_name(ea) or ("sub_%X" % ea)


# Known-good: UObject::ProcessEvent, ENGINE_NOTES "kProcessEvent", prologue 55 8B EC 6A FF.
KNOWN_VA, KNOWN_BYTES = 0x00470640, bytes.fromhex("558BEC6AFF")
got = ida_bytes.get_bytes(KNOWN_VA, len(KNOWN_BYTES)) or b""
if got != KNOWN_BYTES:
    w("*** REFUSED: known-good ProcessEvent bytes at 0x%08X are %s, expected %s - wrong binary or a game update ***"
      % (KNOWN_VA, got.hex(" "), KNOWN_BYTES.hex(" ")))
    done(3)
w("known-good ProcessEvent 0x%08X bytes match" % KNOWN_VA)


def decomp(ea, first=1, limit=400):
    f = ida_funcs.get_func(ea)
    if not f:
        w("*** NO FUNCTION AT %s ***" % a(ea))
        return
    w("")
    w("##### DECOMPILE %s (function %s %s)" % (a(ea), a(f.start_ea), fname(f.start_ea)))
    try:
        lines = str(ida_hexrays.decompile(f.start_ea)).split("\n")
    except Exception as e:
        lines = ["DECOMPILE FAILED: %s" % e]
    window = lines[first - 1:first - 1 + limit]
    for i, l in enumerate(window):
        w("  %4d| %s" % (first + i, l))
    if len(lines) > first - 1 + limit or first > 1:
        w("  ... showing lines %d-%d of %d" % (first, first - 1 + len(window), len(lines)))


def disasm(ea):
    f = ida_funcs.get_func(ea)
    if not f:
        w("*** NO FUNCTION AT %s ***" % a(ea))
        return
    items = list(idautils.FuncItems(f.start_ea))
    w("")
    w("##### DISASM %s (%d instructions)" % (a(f.start_ea), len(items)))
    for it in items:
        raw = ida_bytes.get_bytes(it, idc.get_item_size(it)) or b""
        w("  %s  %-24s %s" % (a(it), raw.hex(" "), idc.generate_disasm_line(it, 0)))


def vtable_class(slot_ea):
    # Walk back to the vtable start (first dword whose predecessor is not a function
    # pointer). MSVC x86 RTTI: CompleteObjectLocator pointer at vt-4, TypeDescriptor
    # pointer (absolute) at COL+12, mangled name at TD+8.
    ea = slot_ea
    while ida_funcs.get_func(ida_bytes.get_dword(ea - 4)) is not None:
        ea -= 4
    col = ida_bytes.get_dword(ea - 4)
    try:
        td = ida_bytes.get_dword(col + 12)
        name = (idc.get_strlit_contents(td + 8) or b"*** COL UNREADABLE ***").decode(errors="replace")
    except Exception as e:
        name = "*** COL UNREADABLE (%s) ***" % e
    return ea, (slot_ea - ea) // 4, name


def refs(ea):
    f = ida_funcs.get_func(ea)
    if not f:
        return
    n = 0
    w("")
    w("##### REFERENCES TO %s" % a(f.start_ea))
    for x in idautils.XrefsTo(f.start_ea):
        n += 1
        cf = ida_funcs.get_func(x.frm)
        if cf:
            w("  code %s in %s %s" % (a(x.frm), a(cf.start_ea), fname(cf.start_ea)))
        elif ida_bytes.get_dword(x.frm) != f.start_ea:
            w("  data %s in %s - NOT a pointer slot, not a vtable" % (a(x.frm), idc.get_segm_name(x.frm)))
        else:
            vt, slot, cls = vtable_class(x.frm)
            w("  data %s = vtable %s slot %d (+0x%X) class %s" % (a(x.frm), a(vt), slot, 4 * slot, cls))
    if n == 0:
        w("  NONE (no code or data reference found)")


def callees(ea):
    f = ida_funcs.get_func(ea)
    out = []
    if not f:
        return out
    for it in idautils.FuncItems(f.start_ea):
        if idc.print_insn_mnem(it) == "call" and idc.get_operand_type(it, 0) in (idc.o_near, idc.o_far):
            tgt = idc.get_operand_value(it, 0)
            if ida_funcs.get_func(tgt) and tgt not in out:
                out.append(tgt)
    return out


for t in TARGETS:
    decomp(t, limit=200)
    for c in callees(t):
        w("")
        w("---- callee of %s:" % a(t))
        decomp(c, limit=260)

done(0)
