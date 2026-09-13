# ue3-natives.py - offline UE3 class and native-function resolver for a PE32 image.
#
# NOTHING IT PRINTS MAY BE COMMITTED. The output is game-derived content, which
# the project's hard rule forbids in the repo - summarize findings in the
# per-game ENGINE_NOTES instead. The TOOL is ours and is committed; its OUTPUT
# is 2K's and is not. Same footing as tools/disasm-rva.py and tools/pe-xref.ps1.
#
# WHY THIS EXISTS. Three seams have now been derived by hand-walking the same
# route - the per-eye camera, the crossbow fire seam (VR-57) and the pistol fire
# seam (VR-82) - and each one re-typed the same capstone snippets. This is that
# walk, kept, with the verification step built in.
#
#   class name (UTF-16 in .rdata)
#     -> the metadata struct that points at it
#       -> +0x14 is the constructor
#         -> the constructor's `mov [reg], imm32` installs the context vtable
#           -> a slot in that vtable is the native routine
#
# ALWAYS pass --verify. It re-derives a known-good class first and refuses to
# print anything if the published numbers do not come back. A route that cannot
# reproduce an answer already in ENGINE_NOTES is not evidence about a new one.
#
# The `natives` mode dumps UE3's native function registration table, which pairs
# an "A<Class>exec<Function>" name with the exec thunk's address. An exec thunk
# usually ends by dispatching through a vtable slot, so it is the cheapest known
# route from a function NAME to the virtual that implements it.
#
# Requires capstone (`pip install capstone`; 5.0.7 verified).
#
# Usage (all addresses hex, with or without 0x):
#   python tools/ue3-natives.py <exe> class DisItemContext_FirePistol [--slot 1B0]
#   python tools/ue3-natives.py <exe> natives --grep Interact
#   python tools/ue3-natives.py <exe> natives --name UDishonoredCheatManagerexecToggleUsableHighlight
#   python tools/ue3-natives.py <exe> vtable 01172E60 --slot 1B0 [--count 120]

import argparse
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
try:
    import importlib.util
    _spec = importlib.util.spec_from_file_location(
        "disasm_rva", os.path.join(os.path.dirname(os.path.abspath(__file__)), "disasm-rva.py"))
    _dr = importlib.util.module_from_spec(_spec)
    _spec.loader.exec_module(_dr)
except Exception as exc:                                   # pragma: no cover
    sys.exit("could not load tools/disasm-rva.py beside this script: %s" % exc)

Pe = _dr.Pe
find_pattern = _dr.find_pattern
md = _dr.md

# The class this project has published numbers for. --verify re-derives it and
# compares; see ENGINE_NOTES, "VR-57 native crossbow fire seam".
KNOWN = {
    "name": "DisItemContext_FireCrossbow",
    "metadata": 0x01361928,
    "ctor": 0x00C29DB0,
    "vtable": 0x01172C80,
    "slot": (0x1B0, 0x00C38230),
}


def parse_rva(s):
    return int(s, 16)


class Image(object):
    def __init__(self, path):
        self.pe = Pe(path)
        self.base = self.pe.image_base

    def dword(self, va):
        b = self.pe.read(va - self.base, 4)
        return struct.unpack("<I", b)[0] if len(b) == 4 else None

    def in_image(self, va):
        return va is not None and self.base <= va < self.base + self.pe.size_of_image

    def is_code(self, va):
        if not self.in_image(va):
            return False
        s = self.pe.section_of(va - self.base)
        return bool(s and s["exec"])

    def refs_to(self, va):
        return [self.base + r for _, r in find_pattern(self.pe, struct.pack("<I", va))]

    def utf16(self, name):
        return [self.base + r for _, r in
                find_pattern(self.pe, name.encode("utf-16-le") + b"\0\0")]

    def vtable_in(self, ctor, span=0x90):
        """The `mov dword ptr [reg], imm32` a UE3 constructor uses to install
        the object's own vtable. Only the no-displacement form: `[esi+0x38]` is
        a sub-object's vtable, not this one."""
        blob = self.pe.read(ctor - self.base, span)
        if not blob:
            return None
        for ins in md().disasm(blob, ctor):
            if (ins.mnemonic == "mov" and ins.op_str.startswith("dword ptr [e")
                    and "], 0x" in ins.op_str
                    and "+" not in ins.op_str.split("]")[0]):
                try:
                    return int(ins.op_str.split("], ")[1], 16)
                except ValueError:
                    return None
            if ins.mnemonic == "ret":
                return None
        return None

    def resolve_class(self, name):
        """Every (metadata, ctor, vtable) the name resolves to. More than one
        hit means the name is ambiguous and the caller must say which."""
        out = []
        for sva in self.utf16(name):
            for meta in self.refs_to(sva):
                ctor = self.dword(meta + 0x14)
                if not self.is_code(ctor):
                    continue
                out.append({"name_va": sva, "metadata": meta, "ctor": ctor,
                            "vtable": self.vtable_in(ctor)})
        return out


def verify(img):
    """Re-derive the published class. Returns a list of complaints."""
    got = img.resolve_class(KNOWN["name"])
    if len(got) != 1:
        return ["expected exactly one %s, resolved %d" % (KNOWN["name"], len(got))]
    g, bad = got[0], []
    for field in ("metadata", "ctor", "vtable"):
        if g[field] != KNOWN[field]:
            bad.append("%s: got 0x%08X, ENGINE_NOTES says 0x%08X"
                       % (field, g[field] or 0, KNOWN[field]))
    off, want = KNOWN["slot"]
    if g["vtable"]:
        slot = img.dword(g["vtable"] + off)
        if slot != want:
            bad.append("vtable +0x%X: got 0x%08X, ENGINE_NOTES says 0x%08X"
                       % (off, slot or 0, want))
    return bad


def natives(img):
    """UE3's native registration table: {const char* name, void* thunk} pairs.
    Found by locating the "A<Class>exec<Function>" names and reading the dword
    beside each pointer to one."""
    pat = re.compile(rb'[A-Za-z_][A-Za-z0-9_]{4,90}exec[A-Za-z0-9_]{2,60}\x00')
    out = []
    for s in img.pe.sections:
        blob = img.pe.data[s["rawptr"]:s["rawptr"] + s["rawsize"]]
        for m in pat.finditer(blob):
            va = img.base + s["vaddr"] + m.start()
            name = m.group()[:-1].decode("ascii", "replace")
            thunk = None
            for ref in img.refs_to(va):
                cand = img.dword(ref + 4)
                if img.is_code(cand):
                    thunk = cand
                    break
            out.append((va, name, thunk))
    return out


def cmd_class(img, a):
    for name in a.names:
        hits = img.resolve_class(name)
        print("=== %s ===" % name)
        if not hits:
            print("  no class metadata resolved - check the spelling, the name is UTF-16")
        for h in hits:
            print("  metadata 0x%08X  ctor 0x%08X  vtable %s"
                  % (h["metadata"], h["ctor"],
                     ("0x%08X" % h["vtable"]) if h["vtable"] else "NOT FOUND in the ctor"))
            if a.slot is not None and h["vtable"]:
                off = parse_rva(a.slot)
                print("    +0x%03X -> 0x%08X" % (off, img.dword(h["vtable"] + off) or 0))


def cmd_vtable(img, a):
    vt = parse_rva(a.vtable)
    if a.slot is not None:
        off = parse_rva(a.slot)
        print("0x%08X +0x%03X -> 0x%08X" % (vt, off, img.dword(vt + off) or 0))
        return
    for i in range(a.count):
        v = img.dword(vt + 4 * i)
        if v is None:
            break
        print("  [%3d] +0x%03X  0x%08X%s"
              % (i, i * 4, v, "  (code)" if img.is_code(v) else ""))


def cmd_natives(img, a):
    rows = natives(img)
    print("native exec registrations: %d" % len(rows))
    for va, name, thunk in rows:
        if a.name and name != a.name:
            continue
        if a.grep and a.grep.lower() not in name.lower():
            continue
        print("  name 0x%08X  thunk %s  %s"
              % (va, ("0x%08X" % thunk) if thunk else "?", name))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("exe")
    ap.add_argument("--verify", action="store_true",
                    help="re-derive the published known-good class first and refuse on a mismatch")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("class"); p.add_argument("names", nargs="+"); p.add_argument("--slot")
    p.set_defaults(fn=cmd_class)
    p = sub.add_parser("vtable"); p.add_argument("vtable"); p.add_argument("--slot")
    p.add_argument("--count", type=int, default=120); p.set_defaults(fn=cmd_vtable)
    p = sub.add_parser("natives"); p.add_argument("--grep"); p.add_argument("--name")
    p.set_defaults(fn=cmd_natives)

    a = ap.parse_args()
    img = Image(a.exe)
    bad = verify(img)
    if bad:
        for b in bad:
            print("VERIFY FAILED: %s" % b, file=sys.stderr)
        print("The route cannot reproduce a number already in ENGINE_NOTES, so nothing "
              "it says about a NEW class is evidence. Wrong exe build?", file=sys.stderr)
        return 2
    if a.verify:
        print("verify: %s re-derived exactly (metadata, ctor, vtable and slot +0x%X)"
              % (KNOWN["name"], KNOWN["slot"][0]))
    a.fn(img, a)
    return 0


if __name__ == "__main__":
    sys.exit(main())
