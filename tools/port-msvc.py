"""One-shot port of the original single-file proxy (src/dllmain.cpp, MinGW) to MSVC.

Kept in tools/ as the record of exactly what Phase 0 of the refactor changed:
  - <intrin.h> and <d3d10.h> includes
  - __builtin_return_address(0) -> _ReturnAddress()
  - the MinGW-only __WIN32 undef before openvr_capi.h removed
  - __declspec(dllexport) dropped (src/proxy/d3d9.def names the exports undecorated)
  - the hand-declared ID3D10Multithread twin replaced by the real interface
  - the four AT&T naked stubs rewritten as __declspec(naked) Intel-syntax functions
Run once from the repo root: python tools/port-msvc.py
"""
import io
import re

p = "src/dllmain.cpp"
s = io.open(p, encoding="utf-8", newline="").read()
s = s.replace("\r\n", "\n")
orig_len = len(s)

# 1. includes: intrin.h for _ReturnAddress, d3d10.h for ID3D10Multithread
s2 = s.replace("#include <windows.h>\n#include <d3d9.h>\n#include <d3d11.h>\n",
               "#include <windows.h>\n#include <intrin.h>\n#include <d3d9.h>\n#include <d3d10.h>\n#include <d3d11.h>\n", 1)
assert s2 != s, "include block not found"
s = s2

# 2. __builtin_return_address(0) -> _ReturnAddress()
n = s.count("__builtin_return_address(0)")
s = s.replace("__builtin_return_address(0)", "_ReturnAddress()")
print("return-address sites:", n)

# 3. drop the MinGW __WIN32 hack (MSVC defines only _WIN32)
m = re.search(r"// openvr_capi\.h does `typedef char bool`.*?#ifdef __WIN32\n#undef __WIN32\n#endif\n", s, re.S)
assert m, "win32 hack block not found"
s = s[:m.start()] + "// openvr_capi.h (C API). MSVC defines only _WIN32, so no MinGW __WIN32 fixup is needed.\n" + s[m.end():]

# 4. exports: the .def file names them undecorated; dllexport would add _Name@N twins
n = s.count('extern "C" __declspec(dllexport) ')
s = s.replace('extern "C" __declspec(dllexport) ', 'extern "C" ')
print("dllexport removed:", n)

# 5. XrMtItf -> the real ID3D10Multithread
m = re.search(r"struct XrMtItf : public IUnknown \{.*?\};\nstatic const GUID kIID_D3D10Mt =\n.*?\};\n", s, re.S)
assert m, "XrMtItf block not found"
s = s[:m.start()] + "typedef ID3D10Multithread XrMtItf;   // works on a D3D11 immediate context\n#define kIID_D3D10Mt __uuidof(ID3D10Multithread)\n" + s[m.end():]

# 6. the four AT&T naked stubs -> MSVC __declspec(naked) Intel syntax
SAVE = """        pushfd
        pushad
        sub    esp, 80h
        movups [esp+00h], xmm0
        movups [esp+10h], xmm1
        movups [esp+20h], xmm2
        movups [esp+30h], xmm3
        movups [esp+40h], xmm4
        movups [esp+50h], xmm5
        movups [esp+60h], xmm6
        movups [esp+70h], xmm7
"""
RESTORE = """        movups xmm0, [esp+00h]
        movups xmm1, [esp+10h]
        movups xmm2, [esp+20h]
        movups xmm3, [esp+30h]
        movups xmm4, [esp+40h]
        movups xmm5, [esp+50h]
        movups xmm6, [esp+60h]
        movups xmm7, [esp+70h]
        add    esp, 80h
        popad
        popfd
"""
stubs = {
    "BlinkAimStub": ("", """        push   ebp                        ; framePtr
        push   esi                        ; self (the power component)
        call   BlinkAimHook
        add    esp, 8
""", """        movss  xmm0, dword ptr [ebp-0Ch]  ; the stolen instruction (f3 0f 10 45 f4)
        jmp    dword ptr [g_blkRet]
"""),
    "BlinkDirStub": ("", """        mov    eax, [esp+9Ch]             ; the EAX pushad saved = engine's vector
        push   eax
        push   ebp
        push   esi
        call   BlinkDirHook
        add    esp, 0Ch
""", """        mov    eax, dword ptr [g_blkDirUse] ; engine's pointer, or ours
        mov    ecx, [eax]                 ; stolen: mov ecx,[eax]
        mov    [ebp-4Ch], ecx             ; stolen: mov [ebp-0x4c],ecx
        jmp    dword ptr [g_blkDirRet]
"""),
    "BlinkDestStub": ("", """        push   ebp
        push   esi
        call   BlinkDestHook
        add    esp, 8
""", """        lea    eax, [ebp-0D0h]            ; the stolen instruction
        jmp    dword ptr [g_blkDstRet]
"""),
    "BlinkTraceStub": ("""        movss  dword ptr [ebp-28h], xmm2  ; the stolen store, FIRST
""", """        push   ebp
        push   esi
        call   BlinkTraceHook
        add    esp, 8
""", """        jmp    dword ptr [g_blkTrcRet]
"""),
}
count = 0


def repl(m):
    global count
    body = m.group(0)
    name = re.search(r'"\.globl _(\w+)\\+n"', body).group(1)
    pre, call, post = stubs[name]
    count += 1
    return ("// MSVC naked stub (ported from the MinGW AT&T listing): save flags, GPRs and\n"
            "// xmm0-7, call the C handler with the engine's frame (ebp) and object (esi),\n"
            "// restore, execute the stolen instruction(s), jump back.\n"
            f'extern "C" __declspec(naked) void {name}(void)\n{{\n    __asm {{\n'
            + pre + SAVE + call + RESTORE + post + "    }\n}\n")


s = re.sub(r'__asm__\(\n"\.text\\+n"\n.*?\n\);\n', repl, s, flags=re.S)
print("asm stubs replaced:", count)
assert count == 4
io.open(p, "w", encoding="utf-8", newline="").write(s)
print("bytes", orig_len, "->", len(s))
