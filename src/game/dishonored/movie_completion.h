#pragma once
#include <windows.h>
namespace dvr::movie {
// Caller verifies this is the engine's manual-reset completion event. Owning
// a duplicate protects the observation from handle closure; never wait for it.
inline bool observe_completion(HANDLE source,bool& pending) {
    HANDLE owned=nullptr;
    if(!DuplicateHandle(GetCurrentProcess(),source,GetCurrentProcess(),&owned,SYNCHRONIZE,FALSE,0)) return false;
    const DWORD result=WaitForSingleObject(owned,0);
    CloseHandle(owned);
    if(result!=WAIT_OBJECT_0 && result!=WAIT_TIMEOUT) return false;
    pending=result==WAIT_TIMEOUT;
    return true;
}
}
