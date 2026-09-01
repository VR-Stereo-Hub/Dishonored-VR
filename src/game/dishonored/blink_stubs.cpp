// game/dishonored/blink_stubs.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// MSVC naked stub (ported from the MinGW AT&T listing): save flags, GPRs and
// xmm0-7, call the C handler with the engine's frame (ebp) and object (esi),
// restore, execute the stolen instruction(s), jump back.
extern "C" __declspec(naked) void BlinkAimStub(void)
{
    __asm {
        pushfd
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
        push   ebp                        ; framePtr
        push   esi                        ; self (the power component)
        call   BlinkAimHook
        add    esp, 8
        movups xmm0, [esp+00h]
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
        movss  xmm0, dword ptr [ebp-0Ch]  ; the stolen instruction (f3 0f 10 45 f4)
        jmp    dword ptr [g_blkRet]
    }
}


// MSVC naked stub (ported from the MinGW AT&T listing): save flags, GPRs and
// xmm0-7, call the C handler with the engine's frame (ebp) and object (esi),
// restore, execute the stolen instruction(s), jump back.
extern "C" __declspec(naked) void BlinkDirStub(void)
{
    __asm {
        pushfd
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
        mov    eax, [esp+9Ch]             ; the EAX pushad saved = engine's vector
        push   eax
        push   ebp
        push   esi
        call   BlinkDirHook
        add    esp, 0Ch
        movups xmm0, [esp+00h]
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
        mov    eax, dword ptr [g_blkDirUse] ; engine's pointer, or ours
        mov    ecx, [eax]                 ; stolen: mov ecx,[eax]
        mov    [ebp-4Ch], ecx             ; stolen: mov [ebp-0x4c],ecx
        jmp    dword ptr [g_blkDirRet]
    }
}


// MSVC naked stub (ported from the MinGW AT&T listing): save flags, GPRs and
// xmm0-7, call the C handler with the engine's frame (ebp) and object (esi),
// restore, execute the stolen instruction(s), jump back.
extern "C" __declspec(naked) void BlinkDestStub(void)
{
    __asm {
        pushfd
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
        push   ebp
        push   esi
        call   BlinkDestHook
        add    esp, 8
        movups xmm0, [esp+00h]
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
        lea    eax, [ebp-0D0h]            ; the stolen instruction
        jmp    dword ptr [g_blkDstRet]
    }
}


// MSVC naked stub (ported from the MinGW AT&T listing): save flags, GPRs and
// xmm0-7, call the C handler with the engine's frame (ebp) and object (esi),
// restore, execute the stolen instruction(s), jump back.
extern "C" __declspec(naked) void BlinkTraceStub(void)
{
    __asm {
        movss  dword ptr [ebp-28h], xmm2  ; the stolen store, FIRST
        pushfd
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
        push   ebp
        push   esi
        call   BlinkTraceHook
        add    esp, 8
        movups xmm0, [esp+00h]
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
        jmp    dword ptr [g_blkTrcRet]
    }
}
