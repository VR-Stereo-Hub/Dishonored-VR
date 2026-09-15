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
__declspec(naked) uint32_t __cdecl syntheticCull(void*) {
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
  mov eax,[ebx+8]
  xor eax,12345678h
  pop ebp
  mov esp,ebx
  pop ebx
  ret
 }
}
// Exact realignment prefix; calls the one-argument cdecl child with ECX.
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
  push ecx
  call syntheticCull
  add esp,4
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
 configure(true);install((uintptr_t)&synthetic,prefix,0,16);assert(hook.on && !enabled());
 set_enabled(true);assert(!enabled());
 install_culling((uintptr_t)bad,prefix,16,0,0);assert(!cullHook.on && !enabled());
 install_culling((uintptr_t)&syntheticCull,prefix,16,0,0);assert(cullHook.on && enabled());
 uint32_t flag=1;void* family=&flag;void* receiver=&family;
 assert(reflection_class(nullptr)==-1 && reflection_class((void*)1)==-1);
 assert(reflection_class(receiver)==1);
 end_frame(-1,true);
 assert(synthetic(receiver,nullptr)==((uint32_t)receiver^0x12345678));
 flag=0;assert(synthetic(receiver,nullptr)==((uint32_t)receiver^0x12345678));
 assert(pending[0].reflection==1 && pending[0].order==1 && pending[0].childCalls==1);
 assert(pending[1].reflection==0 && pending[1].order==2 && pending[1].childCalls==1);
 assert(pending[0].childMs>0 && pending[0].childMs<pending[0].total);
 assert(!activeInvocation);
 end_frame(-1,true);assert(ordinal==0);
 assert(synthetic(receiver,nullptr)==((uint32_t)receiver^0x12345678));
 end_frame(1,true);
 assert(window[0].eye==-1 && window[2].eye==1 && window[2].order==1);
 HANDLE thread=CreateThread(nullptr,0,otherThread,nullptr,0,nullptr);assert(thread);
 WaitForSingleObject(thread,INFINITE);CloseHandle(thread);
 assert(foreign.load()==1 && foreignCull.load()==1);
 syntheticCull(receiver);assert(unmatchedCull==1);
 Invocation mismatch{receiver,1};activeInvocation=&mismatch;
 syntheticCull(receiver);activeInvocation=nullptr;
 assert(classificationChanged==1 && mismatch.childCalls==1);
 clear();
 for(unsigned i=0;i<10000;++i) assert(synthetic(receiver,nullptr)==((uint32_t)receiver^0x12345678));
 uint64_t count=0,children=0;for(const auto& r:pending) {count+=r.calls;children+=r.childCalls;}
 assert(count==10000 && children==10000 && ordinalClamped==9992 && overflow==0);
 set_enabled(false);assert(synthetic((void*)9,nullptr)==(9^0x12345678));
 end_frame(0,false);for(const auto& r:window) assert(r.calls==0);
 set_enabled(true);end_frame(-1,true);
 assert(synthetic(receiver,nullptr)==((uint32_t)receiver^0x12345678));
 testNow+=3100;end_frame(-1,true);
 assert(output.find("reflectionBranch=0 ordinal=1 cullCalls=1")!=std::string::npos);
 assert(output.find("overflow=0")!=std::string::npos);
 dvr::hooks::detour_remove(cullHook,"cull-host");
 dvr::hooks::detour_remove(hook,"host");assert(memcmp((void*)&synthetic,prefix,6)==0);
 assert(synthetic((void*)77,nullptr)==(77^0x12345678));
 puts("PASS: two-hook fail-soft, aligned thiscall/cdecl ABIs, exact receiver/result, reflection/unknown classification, nested timing, ordinal reset/clamp, eye/foreign/unmatched accounting, 10000 calls, toggles and removal");
}
