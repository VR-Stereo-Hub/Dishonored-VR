#define DVR_CAT ::dvr::log::Cat::core
#include "core/hooks/vtable.h"
#include "core/util/log.h"
#include <windows.h>

namespace dvr::hooks {

void* patch_vtable(void* comObject, int index, void* newFn)
{
    void** vtbl = *(void***)comObject;
    void* old = vtbl[index];
    if (old == newFn) return nullptr;
    DWORD oldProt;
    if (!VirtualProtect(&vtbl[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt)) {
        DVR_ERROR("VirtualProtect failed on vtable slot %d", index);
        return nullptr;
    }
    vtbl[index] = newFn;
    VirtualProtect(&vtbl[index], sizeof(void*), oldProt, &oldProt);
    return old;
}

} // namespace dvr::hooks
