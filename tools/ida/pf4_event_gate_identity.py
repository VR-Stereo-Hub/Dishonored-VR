# pf4_event_gate_identity.py - series pf (performance), question 4.
#
# QUESTION: pf3 found that the scene render function tests dword 0x0141B268 immediately before
# it builds the "DPG %s" stage event, that the image reads that dword 139 times and writes it
# once (0x006C7BBB in 0x006C75A0). Is it the draw-event switch and nothing else? Evidence asked
# for: (1) what follows each of the 139 reads (an event constructor / the BeginEvent wrapper, or
# something unrelated), (2) what the single writer is and what it writes, with the strings its
# function references, (3) the exact bytes of two readers, for a byte-verified write.
#
# Run:  .\tools\ida-run.ps1 tools\ida\pf4_event_gate_identity.py
import os
import idaapi, idautils, idc, ida_auto, ida_bytes, ida_funcs, ida_hexrays, ida_name, ida_nalt, ida_xref

GATE = 0x0141B268
EVENT_CTORS = {0x004DA900: "event ctor (formats a name, calls BeginEvent)", 0x009B5EA0: "BeginEvent wrapper",
               0x009B5EC0: "EndEvent thunk"}
WRITER_FUNC, WRITE_AT = 0x006C75A0, 0x006C7BBB

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

# (1) what follows each read: walk forward up to 40 instructions (following nothing), report the
# first call and whether it is an event function. A read that leads elsewhere is printed in full.
reads = [x.frm for x in idautils.XrefsTo(GATE) if x.type == ida_xref.dr_R]
writes = [x.frm for x in idautils.XrefsTo(GATE) if x.type == ida_xref.dr_W]
others = [x.frm for x in idautils.XrefsTo(GATE) if x.type not in (ida_xref.dr_R, ida_xref.dr_W)]
w("")
w("gate %s: %d reads, %d writes, %d other references" % (a(GATE), len(reads), len(writes), len(others)))
kinds = {}
odd = []
for r in reads:
    ea = r
    first_call = None
    for _ in range(40):
        ea = idc.next_head(ea)
        if ea == idaapi.BADADDR:
            break
        if idc.print_insn_mnem(ea) == "call":
            for t in idautils.CodeRefsFrom(ea, 0):
                first_call = t
                break
            if first_call is None:
                first_call = -1
            break
    label = EVENT_CTORS.get(first_call, None)
    if label is None:
        # one hop: a call to something that itself calls the BeginEvent wrapper counts
        hop = False
        if first_call and first_call > 0:
            f = ida_funcs.get_func(first_call)
            if f and (f.end_ea - f.start_ea) < 400:
                for it in idautils.FuncItems(f.start_ea):
                    if idc.print_insn_mnem(it) == "call" and any(t in EVENT_CTORS for t in idautils.CodeRefsFrom(it, 0)):
                        hop = True
                        break
        label = "calls a small function that calls an event function" if hop else "OTHER"
        if not hop:
            odd.append((r, first_call))
    kinds[label] = kinds.get(label, 0) + 1
for k in sorted(kinds, key=lambda k: -kinds[k]):
    w("  %4d reads are followed first by: %s" % (kinds[k], k))
w("  reads NOT followed by an event function within 40 instructions: %d" % len(odd))
for r, c in odd[:14]:
    f = ida_funcs.get_func(r)
    w("    read at %s in %s -> first call %s | %s" %
      (a(r), a(f.start_ea) if f else "?", ("0x%08X" % c) if c and c > 0 else ("indirect" if c == -1 else "none"),
       idc.generate_disasm_line(r, 0)))

# (2) the writer
w("")
w("writer: %s in function %s" % (a(WRITE_AT), a(WRITER_FUNC)))
ea = WRITE_AT
for _ in range(10):
    ea = idc.prev_head(ea)
ctx = []
for _ in range(22):
    raw = ida_bytes.get_bytes(ea, idc.get_item_size(ea)) or b""
    ctx.append("  %s  %-24s %s" % (a(ea), raw.hex(" "), idc.generate_disasm_line(ea, 0)))
    ea = idc.next_head(ea)
for ln in ctx:
    w(ln)
strs = []
f = ida_funcs.get_func(WRITER_FUNC)
if f:
    for it in idautils.FuncItems(f.start_ea):
        for d in idautils.DataRefsFrom(it):
            for kind in (0, 1):
                try:
                    raw = ida_bytes.get_strlit_contents(d, -1, kind)
                except Exception:
                    raw = None
                if raw and len(raw) >= 4:
                    s = raw.decode("utf-8", "replace")
                    if all(32 <= ord(ch) < 127 for ch in s) and s not in strs:
                        strs.append(s)
    w("  writer function is %d bytes; strings it references (%d): %s" % (f.end_ea - f.start_ea, len(strs), " | ".join(strs[:60])))
else:
    w("  *** NO FUNCTION AT THE WRITER ***")

# (3) bytes of two readers for a byte-verified write
w("")
for r in (0x0086C161,) + tuple(reads[:1]):
    raw = ida_bytes.get_bytes(r, 8) or b""
    w("reader bytes at %s: %s   %s" % (a(r), raw.hex(" "), idc.generate_disasm_line(r, 0)))
cur = ida_bytes.get_dword(GATE)
w("static value of the gate in the image: %d (is_loaded=%s)" % (cur, ida_bytes.is_loaded(GATE)))
w("")
w("END pf4")
done(0)
