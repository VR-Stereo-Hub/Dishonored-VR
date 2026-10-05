# pf1_render_stages.py - series pf (performance), question 1.
#
# QUESTION: what are the stages of the scene render function that holds the two sampled
# return addresses RVA 0046C1F4 and 0046C208 (37 % and 33 % of render-thread CPU samples,
# PERFORMANCE.md "Representative CPU evidence"), in call order, and what does the executable
# itself call them? The exe keeps its own stage labels as strings (InitViews,
# ProcessViewFrustumCulling, the World/Foreground pass labels: ENGINE_NOTES), so each call
# site is named by the strings its callee references, never by a guess.
#
# Run:  .\tools\ida-run.ps1 tools\ida\pf1_render_stages.py
# Output is game-derived text: it stays in the IDA workspace, never in the repo.
import os
import idaapi, idautils, idc, ida_auto, ida_bytes, ida_funcs, ida_hexrays, ida_name, ida_nalt, ida_ua

SAMPLED = [0x0086C1F4, 0x0086C208, 0x0086C0C1]   # return VAs (RVA + 0x400000); the third is the known InitViews return
PASS_FUNCS = [0x0086BF00, 0x00864290]            # the four-pass loop's callees (ENGINE_NOTES)
KNOWN_INITVIEWS = 0x008662A0

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


# Known-good 1: UObject::ProcessEvent prologue. Known-good 2: the published InitViews signature.
for va, sig, what in ((0x00470640, "558BEC6AFF", "ProcessEvent"),
                      (KNOWN_INITVIEWS, "538BDC83EC0883E4F083C404558B6B04", "InitViews (patterns.h kSceneInitViewsPrefix)")):
    got = ida_bytes.get_bytes(va, len(sig) // 2) or b""
    if got != bytes.fromhex(sig):
        w("*** REFUSED: known-good %s bytes at 0x%08X are %s - wrong binary or a game update ***" % (what, va, got.hex(" ")))
        done(3)
    w("known-good %s 0x%08X bytes match" % (what, va))


def str_at(ea):
    """ASCII or UTF-16 literal at ea, or None. Printable, at least 4 characters."""
    for kind in (0, 1):   # 0 = C string, 1 = UTF-16LE
        try:
            raw = ida_bytes.get_strlit_contents(ea, -1, kind)
        except Exception:
            raw = None
        if raw and len(raw) >= 4:
            try:
                s = raw.decode("utf-8", "replace")
            except Exception:
                continue
            if all(32 <= ord(c) < 127 for c in s):
                return s
    return None


def strings_of(func_ea, limit=14):
    """String literals a function references directly (data refs from its instructions)."""
    f = ida_funcs.get_func(func_ea)
    out = []
    if not f:
        return out
    for it in idautils.FuncItems(f.start_ea):
        for d in idautils.DataRefsFrom(it):
            s = str_at(d)
            if s and s not in out:
                out.append(s)
                if len(out) >= limit:
                    return out
    return out


def callees(func_ea):
    """(call site, target or None) for every call instruction, in address order."""
    f = ida_funcs.get_func(func_ea)
    res = []
    if not f:
        return res
    for it in idautils.FuncItems(f.start_ea):
        if idc.print_insn_mnem(it) != "call":
            continue
        tgt = None
        for x in idautils.CodeRefsFrom(it, 0):
            tgt = x
            break
        res.append((it, tgt))
    return res


def describe(func_ea, title, depth_strings=True):
    f = ida_funcs.get_func(func_ea)
    if not f:
        w("*** NO FUNCTION AT %s ***" % a(func_ea))
        return None
    w("")
    w("##### %s: function %s .. 0x%08X, %d bytes, %d instructions" %
      (title, a(f.start_ea), f.end_ea, f.end_ea - f.start_ea, len(list(idautils.FuncItems(f.start_ea)))))
    own = strings_of(f.start_ea, 40)
    w("  strings referenced directly: %s" % (" | ".join(own) if own else "NONE"))
    cs = callees(f.start_ea)
    w("  %d call instructions, in address order (-> = direct target; indirect calls show the operand):" % len(cs))
    for site, tgt in cs:
        ret = site + idc.get_item_size(site)
        mark = "  <== SAMPLED RETURN %s" % a(ret) if ret in SAMPLED else ""
        if tgt is None:
            w("    %s  call %s (indirect)%s" % (a(site), idc.print_operand(site, 0), mark))
            continue
        tf = ida_funcs.get_func(tgt)
        size = (tf.end_ea - tf.start_ea) if tf else 0
        labels = strings_of(tgt, 6) if depth_strings else []
        w("    %s  -> %s %s (%d bytes)%s%s" % (a(site), a(tgt), fname(tgt), size,
                                              ("  strings: " + " | ".join(labels)) if labels else "", mark))
    return f


seen = set()
for ret in SAMPLED:
    f = ida_funcs.get_func(ret)
    if not f:
        w("*** NO FUNCTION CONTAINS SAMPLED RETURN %s ***" % a(ret))
        continue
    prev = idc.prev_head(ret)
    w("")
    w("sampled return %s: previous instruction %s  %s  (a return address only counts if this is a call)" %
      (a(ret), a(prev), idc.generate_disasm_line(prev, 0)))
    if f.start_ea in seen:
        w("  (same function as above: %s)" % a(f.start_ea))
        continue
    seen.add(f.start_ea)
    describe(f.start_ea, "THE SCENE RENDER FUNCTION holding %s" % a(ret))
    w("")
    w("  callers of this function:")
    n = 0
    for x in idautils.XrefsTo(f.start_ea):
        cf = ida_funcs.get_func(x.frm)
        n += 1
        w("    %s in %s %s" % (a(x.frm), a(cf.start_ea) if cf else "?", fname(cf.start_ea) if cf else "(no function)"))
    if not n:
        w("    NONE (reached through a vtable or a pointer)")

# One level down, for the two callees that hold the samples and the two pass functions.
targets = []
for ret in SAMPLED[:2]:
    prev = idc.prev_head(ret)
    for x in idautils.CodeRefsFrom(prev, 0):
        targets.append((x, "callee returning to %s" % a(ret)))
        break
    else:
        w("")
        w("*** the call before %s is indirect (%s): its target cannot be read statically ***" %
          (a(ret), idc.print_operand(prev, 0)))
for p in PASS_FUNCS:
    targets.append((p, "pass function (ENGINE_NOTES four-pass loop)"))
done_funcs = set()
for ea, why in targets:
    f = ida_funcs.get_func(ea)
    if not f or f.start_ea in done_funcs:
        continue
    done_funcs.add(f.start_ea)
    describe(f.start_ea, why)

w("")
w("END pf1: %d functions described" % (len(seen) + len(done_funcs)))
done(0)
