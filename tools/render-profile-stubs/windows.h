#pragma once
using DWORD=unsigned long;
extern DWORD testThread;
inline DWORD GetCurrentThreadId() { return testThread; }
