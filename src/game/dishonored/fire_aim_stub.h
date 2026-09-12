// x86 mid-function bridge. Included by production and the offline ABI test.
// Preserve flags, all integer registers, x87 state, MXCSR and XMM0..7.
// ESI is the crossbow context and EBP its aligned native stack frame.
__declspec(naked) static void FireAimThunk()
{
    __asm {
        pushfd
        pushad
        mov edx, esp
        sub esp, 528
        and esp, -16
        fxsave [esp]
        fninit
        cld
        push edx
        push ebp
        push esi
        call FireAimHandler
        add esp, 8
        pop edx
        fxrstor [esp]
        mov esp, edx
        popad
        popfd
        // Replay exactly the six displaced bytes. No call or relative branch.
        mov ecx, [ebp-54h]
        push edi
        push edi
        push ecx
        jmp dword ptr [g_fireAimResume]
    }
}
