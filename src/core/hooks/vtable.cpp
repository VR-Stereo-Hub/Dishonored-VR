// core/hooks/vtable.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static void* PatchVtable(void* comObject, int index, void* newFn)
{
    void** vtbl = *(void***)comObject;
    void* old = vtbl[index];
    if (old == newFn) return NULL;
    DWORD oldProt;
    if (!VirtualProtect(&vtbl[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt)) {
        Log("VirtualProtect failed on vtable slot %d", index);
        return NULL;
    }
    vtbl[index] = newFn;
    VirtualProtect(&vtbl[index], sizeof(void*), oldProt, &oldProt);
    return old;
}
