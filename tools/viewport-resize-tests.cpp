// Exercise the production resize queue/guards/ABI against owned host fixtures.
// Never loads or launches Dishonored or an XR simulator.
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include "../src/game/dishonored/patterns.h"
using HWND=void*;
static unsigned long long nowMs=1000;
static uint32_t currentThread=7,windowThread=7;
static HWND g_gameWnd=(HWND)0x1234;
static bool g_resVirtual=false,liveRefresh=true,liveOwner=true;
static unsigned refreshes=0,calls=0,saves=0;
static uint32_t captureW=2750,captureH=2850;
static uint32_t nativeData[400]={},ownerData[32]={},otherOwner[32]={};
static uint8_t* objects[2000]={};
static uintptr_t header[3]={},primary[2]={},secondary[1]={};
static uint8_t signature[12]={},returnBytes[]={0xc2,0x18,0x00};
static void* calledSelf=nullptr;
static uint32_t calledW=0,calledH=0;
static int calledFull=0,calledOption=0,calledX=0,calledY=0;
static void __fastcall FakeResize(void* self,void*,uint32_t w,uint32_t h,int full,int option,int x,int y) {
    ++calls;calledSelf=self;calledW=w;calledH=h;calledFull=full;calledOption=option;calledX=x;calledY=y;
}
static uint32_t GetCurrentThreadId(){return currentThread;}
static uint32_t GetWindowThreadProcessId(HWND,void*){return windowThread;}
static unsigned long long GetTickCount64(){return nowMs;}
static bool RangeReadable(const void* p,size_t){return p!=nullptr;}
static bool BuildLiveSet(){++refreshes;return liveRefresh;}
static bool IsLiveObject(uint8_t* p){return liveOwner && refreshes && (p==(uint8_t*)ownerData || p==(uint8_t*)otherOwner);}
static const char* ObjClassName(uint8_t*){return "FixtureViewportClient";}
static void Log(const char*,...){ }
static void ResRequest(uint32_t,uint32_t,bool,const char*){++saves;}
namespace dvr::capture {static uint32_t width(){return captureW;} static uint32_t height(){return captureH;}}
// Replace only addresses with fixture storage; retain production layout constants.
#define kWindowsViewportVtable ((uintptr_t)primary)
#define kWindowsFViewportVtable ((uintptr_t)secondary)
#define kWindowsViewportResize ((uintptr_t)&FakeResize)
#define kWindowsViewportResizeReturn ((uintptr_t)returnBytes)
#define kWindowsViewportResizePrologue signature
#define kGObjHdr ((uintptr_t)header)
#include "../src/game/dishonored/viewport_resize.cpp"
static unsigned checks=0,failures=0;
static void check(bool ok,const char* what){++checks;if(!ok){++failures;std::printf("FAIL %s\n",what);}}
static uint8_t* view(){return (uint8_t*)nativeData+kWindowsFViewportBase;}
static void reset(){
    g_resLiveState=0;g_resLiveSize=0;g_resLiveStamp=0;
    calls=saves=refreshes=0;liveRefresh=liveOwner=true;currentThread=windowThread=7;
    nowMs=1000;captureW=2750;captureH=2850;
    memset(nativeData,0,sizeof(nativeData));memset(ownerData,0,sizeof(ownerData));memset(otherOwner,0,sizeof(otherOwner));memset(objects,0,sizeof(objects));
    primary[1]=(uintptr_t)&FakeResize;nativeData[0]=(uintptr_t)primary;*(uintptr_t*)view()=(uintptr_t)secondary;
    *(HWND*)((uint8_t*)nativeData+kWindowsViewportHwnd)=g_gameWnd;
    *(uint32_t*)(view()+kFViewportFlags)=2;
    *(int*)((uint8_t*)nativeData+kWindowsViewportPosX)=17;*(int*)((uint8_t*)nativeData+kWindowsViewportPosY)=23;
    *(void**)((uint8_t*)ownerData+kGameViewportNativeViewport)=view();
    objects[0]=(uint8_t*)ownerData;header[0]=(uintptr_t)objects;header[1]=2000;
    memcpy(signature,(void*)&FakeResize,sizeof(signature));returnBytes[1]=0x18;
}
static void request(){ResLiveQueue(3012,3122);}
int main(){
    reset();request();check(ResLiveState()==1 && calls==0 && saves==0,"queue does not call/save on UI lane");
    ResLiveQueue(2750,2850);check((uint32_t)(g_resLiveSize.load()>>32)==3012,"pending request cannot be overwritten");
    ResLiveApply(view());check(calls==1 && saves==1 && refreshes==1,"one engine call after fresh live-table validation");
    check(calledSelf==nativeData && calledW==3012 && calledH==3122,"native base adjustment and dimensions");
    check(calledFull==1 && calledOption==1 && calledX==17 && calledY==23,"all six stack arguments preserved");
    check(ResLiveState()==4,"native return does not claim success");
    ResLiveApply(view());check(calls==1,"request consumed exactly once");
    ResLivePoll();check(ResLiveState()==4,"old capture does not confirm");
    captureW=3012;captureH=3122;ResLivePoll();check(ResLiveState()==3,"matching downstream capture confirms");
    request();check(ResLiveState()==1,"new request allowed after confirmation");
    reset();ResLiveQueue(0,3122);check(ResLiveState()==-1 && calls==0,"invalid size refused");
    reset();request();windowThread=8;ResLiveApply(view());check(calls==0 && saves==0 && ResLiveState()==-1,"wrong thread refused");
    reset();request();signature[0]^=1;ResLiveApply(view());check(calls==0 && saves==0,"signature mismatch refused");
    reset();request();returnBytes[1]=0x10;ResLiveApply(view());check(calls==0,"wrong stack cleanup refused");
    reset();request();nativeData[0]=0;ResLiveApply(view());check(calls==0,"wrong native vtable refused");
    reset();request();*(HWND*)((uint8_t*)nativeData+kWindowsViewportHwnd)=nullptr;ResLiveApply(view());check(calls==0,"wrong window refused");
    reset();request();liveRefresh=false;ResLiveApply(view());check(calls==0 && saves==0,"failed live refresh refuses writes");
    reset();request();liveOwner=false;ResLiveApply(view());check(calls==0 && saves==0,"class name without liveness refuses writes");
    reset();request();*(void**)((uint8_t*)ownerData+kGameViewportNativeViewport)=nullptr;ResLiveApply(view());check(calls==0,"owner must point to this current viewport");
    reset();request();objects[1]=(uint8_t*)otherOwner;*(void**)((uint8_t*)otherOwner+kGameViewportNativeViewport)=view();ResLiveApply(view());check(calls==0,"ambiguous owners refused");
    reset();request();nowMs+=10001;ResLivePoll();ResLiveApply(view());check(ResLiveState()==-2 && calls==0,"expired queue never calls engine later");
    reset();request();ResLiveApply(view());nowMs+=10001;ResLivePoll();check(ResLiveState()==-2 && calls==1,"unhonoured size times out without retry");
    reset();request();ResLiveApply(view());captureW=3012;captureH=3122;ResLivePoll();liveOwner=false;request();ResLiveApply(view());check(calls==1 && refreshes==2,"new request revalidates owner instead of reusing retained identity");
    std::printf("viewport resize: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
