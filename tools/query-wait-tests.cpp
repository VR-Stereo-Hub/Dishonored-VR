#include <windows.h>
#include <d3d9.h>
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
namespace dvr::clock { double now_ms() { testNow+=0.25;return testNow; } }
#include "core/hooks/detour.cpp"
#include "core/framework/query_wait_profile.cpp"
__declspec(naked) uint32_t __fastcall synthetic(void*,void*,IDirect3DQuery9*,void*,uint32_t,uint32_t) {
 __asm {
  push ebp
  mov ebp,esp
  sub esp,1Ch
  mov eax,[ebp+14h]
  mov edx,[ebp+0Ch]
  test edx,edx
  jz noData
  mov [edx],ecx
 noData:
  mov esp,ebp
  pop ebp
  ret 10h
 }
}
D3DQUERYTYPE __stdcall typeOf(IDirect3DQuery9*) { return D3DQUERYTYPE_EVENT; }
DWORD WINAPI otherThread(void*) {
 assert(synthetic((void*)5,nullptr,nullptr,nullptr,4,7)==7);return 0;
}
int main() {
 using namespace dvr::query_profile;
 const uint8_t prefix[]={0x55,0x8b,0xec,0x83,0xec,0x1c};
 uint8_t bad[6]={};configure(true);install((uintptr_t)bad,prefix,0);assert(!hook.on && !enabled());
 configure(true);install((uintptr_t)&synthetic,prefix,0);assert(hook.on && enabled());
 end_frame(-1,true); // owner established, first partial interval discarded
 void* vt[8]={};vt[4]=(void*)&typeOf;void** object=vt;
 auto* query=reinterpret_cast<IDirect3DQuery9*>(&object);
 uint32_t data=0;
 assert(synthetic((void*)123,nullptr,query,&data,4,42)==42 && data==123);
 assert(synthetic((void*)456,nullptr,query,&data,4,0)==0 && data==456);
 uint64_t count=0;for(const auto& r:pending) {count+=r.calls;if(r.calls) assert(r.type==8 && r.total==0.25);}
 assert(count==2);
 HANDLE thread=CreateThread(nullptr,0,otherThread,nullptr,0,nullptr);assert(thread);
 WaitForSingleObject(thread,INFINITE);CloseHandle(thread);assert(foreign.load()==1);
 end_frame(1,true);count=0;for(const auto& r:window) {count+=r.calls;if(r.calls) assert(r.eye==1);}
 assert(count==2);
 set_enabled(false);assert(synthetic(nullptr,nullptr,query,nullptr,4,9)==9);
 end_frame(0,false);for(const auto& r:window) assert(r.calls==0);
 set_enabled(true);end_frame(-1,true);
 assert(synthetic(nullptr,nullptr,query,nullptr,4,1)==1);
 testNow+=3100;end_frame(-1,true);
 assert(output.find("callerRVA=")!=std::string::npos);
 assert(output.find("overflow=0")!=std::string::npos);
 dvr::hooks::detour_remove(hook,"host");assert(memcmp((void*)&synthetic,prefix,6)==0);
 assert(synthetic(nullptr,nullptr,nullptr,nullptr,4,77)==77);
 puts("PASS: signature refusal, trampoline/fastcall four-argument ABI, exact result/data, type, eye interval, foreign thread, toggle/reset and removal");
}
