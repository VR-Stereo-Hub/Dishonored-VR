#include "game/dishonored/fire_aim_math.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <initializer_list>
static int checks=0;
static void check(bool ok) { ++checks; if(!ok){std::printf("FAIL check %d\n",checks);std::exit(1);} }
static bool closeEnough(float a,float b,float e=0.003f) {return std::fabs(a-b)<e;}

// Execute the actual production bridge against a fake native stack. The callback
// deliberately destroys volatile integer, x87 and SSE state; continuation must
// observe the original state and the replayed arguments, not the callback's.
__declspec(align(16)) static unsigned char beforeFx[512],afterFx[512],originalFx[512];
__declspec(align(16)) static unsigned char fakeFrame[512];
__declspec(align(16)) static float xmmSeed[4]={1.25f,-3.5f,6.0f,99.0f};
static unsigned savedEsp,frameAddr,source=0x11223344,regs[7],argsSeen[3],flagsBefore,flagsAfter;
static unsigned handlerCalls;
static bool handlerWrite;
static void __cdecl FireAimHandler(unsigned context,unsigned char* frame) {
    ++handlerCalls;
    check(context==0x12345678);check(frame==(unsigned char*)frameAddr);
    if(handlerWrite) *(float*)(frame-0xAC)=0.75f;
    __asm {
        fninit
        fldz
        xorps xmm0,xmm0
        xorps xmm1,xmm1
        xorps xmm2,xmm2
        xorps xmm3,xmm3
        xorps xmm4,xmm4
        xorps xmm5,xmm5
        xorps xmm6,xmm6
        xorps xmm7,xmm7
    }
}
static uintptr_t g_fireAimResume;
#include "game/dishonored/fire_aim_stub.h"
__declspec(naked) static void Resume() {
    __asm {
        pushfd
        pop flagsAfter
        fxsave afterFx
        mov regs[0],eax
        mov regs[4],ebx
        mov regs[8],ecx
        mov regs[12],edx
        mov regs[16],esi
        mov regs[20],edi
        mov regs[24],ebp
        pop argsSeen[0]
        pop argsSeen[4]
        pop argsSeen[8]
        mov esp,savedEsp
        fxrstor originalFx
        popad
        popfd
        ret
    }
}
__declspec(naked) static void RunBridge() {
    __asm {
        pushfd
        pushad
        mov savedEsp,esp
        fxsave originalFx
        fninit
        fld1
        fldpi
        movaps xmm0,xmmSeed
        movaps xmm1,xmmSeed
        movaps xmm2,xmmSeed
        movaps xmm3,xmmSeed
        movaps xmm4,xmmSeed
        movaps xmm5,xmmSeed
        movaps xmm6,xmmSeed
        movaps xmm7,xmmSeed
        fxsave beforeFx
        mov eax,101
        mov ebx,202
        mov ecx,303
        mov edx,404
        mov esi,12345678h
        mov edi,606
        mov ebp,frameAddr
        stc
        pushfd
        pop flagsBefore
        jmp FireAimThunk
    }
}
int main() {
    using namespace dvr;
    aim::FireFrame f; f.ray.ok=true;f.ray.gen=7;f.ray.sampleMs=1000;
    f.headValid=true;f.headQuat[3]=1;f.ray.dirXr[2]=-1;f.distanceM=8;
    float camera[3]={15000,8000,2800},spawn[3]={15000,8054,2800};
    fireaim::Solution s;
    check(fireaim::solve(f,1010,0,0,camera,108,spawn,s));
    check(closeEnough(s.target[0],15864));check(closeEnough(s.target[1],8000));
    check(s.direction[1]<0); // converges left from a displaced muzzle
    const float travel=(s.target[0]-spawn[0])/s.direction[0];
    check(closeEnough(spawn[1]+travel*s.direction[1],s.target[1]));
    // Changing head yaw must not introduce a left/right mirror. Mapping a right
    // XR ray gives game +Y at yaw zero; up gives +Z, independently of muzzle.
    for(int yi=-6;yi<=6;++yi) for(int pi=-3;pi<=3;++pi) for(int sign=-1;sign<=1;sign+=2) {
        const float yaw=yi*0.4f,pitch=pi*0.2f;
        for(int axis=0;axis<2;++axis) {
            f.ray.dirXr[0]=axis==0?sign*0.5f:0;f.ray.dirXr[1]=axis==1?sign*0.5f:0;
            f.ray.dirXr[2]=-std::sqrt(0.75f);
            check(fireaim::solve(f,1010,yaw,pitch,camera,108,spawn,s));
            const float F[3]={std::cos(pitch)*std::cos(yaw),std::cos(pitch)*std::sin(yaw),std::sin(pitch)};
            const float R[3]={-std::sin(yaw),std::cos(yaw),0};
            const float U[3]={-std::sin(pitch)*std::cos(yaw),-std::sin(pitch)*std::sin(yaw),std::cos(pitch)};
            for(int a=0;a<3;++a) check(closeEnough(s.target[a],camera[a]+864*(std::sqrt(0.75f)*F[a]+sign*0.5f*(axis?U[a]:R[a])),0.005f));
            float to[3];for(int a=0;a<3;++a)to[a]=s.target[a]-spawn[a];
            fireaim::normalize(to);check(fireaim::dot(to,s.direction)>0.99999f);
        }
    }
    // Nonidentity XR head basis, with head and controller turned together.
    f.ray.dirXr[0]=-1;f.ray.dirXr[1]=0;f.ray.dirXr[2]=0;
    f.headQuat[1]=std::sqrt(0.5f);f.headQuat[3]=std::sqrt(0.5f);
    check(fireaim::solve(f,1010,0,0,camera,108,spawn,s));check(closeEnough(s.target[0],15864));check(closeEnough(s.target[1],8000));
    for(float scale: {50.0f,108.0f,200.0f}) {
        check(fireaim::solve(f,1010,0,0,camera,scale,spawn,s));check(closeEnough(s.target[0],camera[0]+8*scale));
    }
    const auto good=f;
    auto rejects=[&](){ fireaim::Solution untouched;untouched.target[0]=123;check(!fireaim::solve(f,1010,0,0,camera,108,spawn,untouched));check(untouched.target[0]==123);f=good;};
    f.ray.ok=false;rejects();f.headValid=false;rejects();f.ray.gen=0;rejects();
    f.ray.sampleMs=800;rejects();f.ray.sampleMs=2000;rejects();
    f.distanceM=std::numeric_limits<float>::quiet_NaN();rejects();
    f.ray.originXr[0]=100;rejects();f.headQuat[1]=0;f.headQuat[3]=0;rejects();
    frameAddr=(unsigned)(fakeFrame+256);*(unsigned*)(frameAddr-0x54)=source;
    g_fireAimResume=(uintptr_t)&Resume;
    for(int i=0;i<100;++i) {
        handlerWrite=(i&1)!=0;*(float*)(frameAddr-0xAC)=1;
        RunBridge();
        check(regs[0]==101&&regs[1]==202&&regs[2]==source&&regs[3]==404);
        check(regs[4]==0x12345678&&regs[5]==606&&regs[6]==frameAddr);
        check(argsSeen[0]==source&&argsSeen[1]==606&&argsSeen[2]==606);
        check(flagsBefore==flagsAfter);
        check(!std::memcmp(beforeFx,afterFx,8)); // x87 control/status/tag
        check(!std::memcmp(beforeFx+24,afterFx+24,8)); // MXCSR
        check(!std::memcmp(beforeFx+32,afterFx+32,256)); // x87 + XMM values
        check(*(float*)(frameAddr-0xAC)==(handlerWrite?0.75f:1.0f));
    }
    check(handlerCalls==100);
    std::printf("fire-aim: %d geometry, refusal and production x86 bridge checks passed\n",checks);
}
