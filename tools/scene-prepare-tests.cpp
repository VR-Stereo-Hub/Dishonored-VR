#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string>
std::string output;
void test_log(const char* fmt,...) {
 char b[2048];va_list a;va_start(a,fmt);vsnprintf(b,sizeof(b),fmt,a);va_end(a);
 output+=b;output+='\n';
}
double testNow=1000;
namespace dvr::clock { double now_ms() { testNow+=0.00025;return testNow; } }
#include "core/hooks/detour.cpp"
#include "core/framework/scene_prepare_profile.cpp"
// Same stack-realignment prefix as InitViews; ECX and stack must survive.
__declspec(naked) uint32_t __fastcall synthetic(void*,void*) {
 __asm {
  push ebx
  mov ebx,esp
  sub esp,8
  and esp,0FFFFFFF0h
  add esp,4
  push ebp
  mov ebp,[ebx+4]
  mov [esp+4],ebp
  mov ebp,esp
  mov eax,ecx
  xor eax,12345678h
  pop ebp
  mov esp,ebx
  pop ebx
  ret
 }
}
DWORD WINAPI otherThread(void*) {
 assert(synthetic((void*)5,nullptr)==(5^0x12345678));return 0;
}
int main() {
 using namespace dvr::scene_prepare;
 const uint8_t prefix[]={0x53,0x8b,0xdc,0x83,0xec,0x08,0x83,0xe4,0xf0,0x83,0xc4,0x04,0x55,0x8b,0x6b,0x04};
 uint8_t bad[16]={};configure(true);install((uintptr_t)bad,prefix,0,16);assert(!hook.on && !enabled());
 configure(true);install((uintptr_t)&synthetic,prefix,0,16);assert(hook.on && enabled());
 end_frame(-1,true);
 for(unsigned i=0;i<10000;++i) assert(synthetic((void*)i,nullptr)==(i^0x12345678));
 uint64_t count=0;for(const auto& r:pending) count+=r.calls;assert(count==10000);
 HANDLE thread=CreateThread(nullptr,0,otherThread,nullptr,0,nullptr);assert(thread);
 WaitForSingleObject(thread,INFINITE);CloseHandle(thread);assert(foreign.load()==1);
 end_frame(1,true);count=0;for(const auto& r:window) {count+=r.calls;if(r.calls) assert(r.eye==1);}
 assert(count==10000);
 set_enabled(false);assert(synthetic((void*)9,nullptr)==(9^0x12345678));
 end_frame(0,false);for(const auto& r:window) assert(r.calls==0);
 set_enabled(true);end_frame(-1,true);
 assert(synthetic((void*)1,nullptr)==(1^0x12345678));
 testNow+=3100;end_frame(-1,true);
 assert(output.find("InitViews callerRVA=")!=std::string::npos);
 assert(output.find("overflow=0")!=std::string::npos);
 dvr::hooks::detour_remove(hook,"host");assert(memcmp((void*)&synthetic,prefix,6)==0);
 assert(synthetic((void*)77,nullptr)==(77^0x12345678));
 puts("PASS: signature refusal, stack-realignment trampoline, zero-stack-argument ABI, exact ECX/result, eye attribution, foreign thread, toggle/reset and removal");
}
