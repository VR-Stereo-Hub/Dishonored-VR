// Production ownership trace: disabled cost, finite capture and logger size.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
static bool enabled=false,logging=true;
namespace dvr::log { enum class Level {Info}; bool enabled(int,Level){return logging;} }
#define DVR_CAT 0
static unsigned logs=0,captures=0,maxLine=0,errors=0;
static uint32_t testFrame=0;
bool owner_trace_enabled(){return enabled;}
namespace dvr::frame { uint32_t count(){return testFrame;} }
long long qpc_now(){LARGE_INTEGER t;QueryPerformanceCounter(&t);return t.QuadPart;}
static void log_line(const char* f,...){
    char line[2048];va_list args;va_start(args,f);
    const int n=vsnprintf(line,sizeof(line),f,args);va_end(args);
    if(n<0 || n>=1024)++errors;
    if(n>0 && unsigned(n)>maxLine)maxLine=unsigned(n);
    ++logs;
}
static USHORT capture_stack(ULONG skip,ULONG count,void** out,ULONG* hash){
    ++captures;return CaptureStackBackTrace(skip,count,out,hash);
}
#undef CaptureStackBackTrace
#define CaptureStackBackTrace capture_stack
#define DVR_INFO log_line
struct Probe {
    bool ok=true;uint64_t drawKey=0x1234;
    unsigned vertices=8,primitives=10;
    float bbox[4]{.524f,.471f,.774f,.592f},raw[4]{-20,-20,20,20};
    float c0[4]{1,0,0,0},c1[4]{0,1,0,0},c3[4]{0,0,0,1};
};
#include "hud_owner_trace.inc"
namespace dvr::hudclass { bool owner_trace_enabled(){return enabled;} }
#include "hud_identity_fields.inc"
static uint8_t arena[512]{};
static unsigned reads=0;
static uint32_t nowMs=1000;
static bool RangeReadable(const void* p,size_t n){
    ++reads;const auto at=(uintptr_t)p,first=(uintptr_t)arena;
    return at>=first && at-first<=sizeof(arena) && n<=sizeof(arena)-(at-first);
}
static uint32_t trace_now(){return nowMs;}
#define GetTickCount trace_now
#define Log log_line
#include "hud_native_identity.inc"
#undef GetTickCount

#undef CaptureStackBackTrace
static unsigned checks=0;
static void check(bool good,const char* name){++checks;if(!good){++errors;printf("FAIL %s\n",name);}}
int main(){
    Probe p;
    for(unsigned i=0;i<1000;++i){testFrame=i;owner_trace(p,3,-1);}
    check(captures==0 && logs==0,"disabled capture performs no stack walk or log");
    enabled=true;logging=false;owner_trace(p,3,-1);
    check(captures==0 && logs==0,"disabled log category performs no stack work");logging=true;
    owner_trace(p,-1,-1);owner_trace(p,32,-1);p.ok=false;owner_trace(p,0,-1);p.ok=true;
    check(captures==0,"unknown owner and failed probe refuse");
    owner_trace(p,3,-1);owner_trace(p,4,-1);
    check(captures==1,"one stack per Present even for different families");
    ++testFrame;owner_trace(p,3,-1);owner_trace(p,4,-1);
    check(captures==2,"repeat family skipped while new family is admitted");
    for(unsigned i=0;i<64;++i){++testFrame;owner_trace(p,int(i/2),i%2?-1:0);}
    check(captures==16 && logs==32,"process lifetime capture limit survives all route families");
    enabled=false;++testFrame;owner_trace(p,31,-1);enabled=true;++testFrame;owner_trace(p,31,-1);
    check(captures==16,"toggle cannot rearm exhausted budget");
    const auto start=qpc_now();
    for(unsigned i=0;i<1000000;++i){testFrame=i;owner_trace(p,int(i%32),-1);}
    LARGE_INTEGER freq;QueryPerformanceFrequency(&freq);
    const double ns=(qpc_now()-start)*1e9/double(freq.QuadPart)/1000000;
    check(captures==16 && logs==32,"steady state does no further capture or formatting");
    check(maxLine<1024,"all complete records fit production logger");
    const unsigned renderLogs=logs;
    enabled=false;MarkerIdentityTrace(arena,0);
    check(reads==0 && logs==renderLogs,"disabled native probe does no engine read or log");
    enabled=true;
    *(uint32_t*)(arena+kMarkerGfxType)=8;
    *(void**)(arena+kMarkerGfxHandle)=arena+64;
    *(void**)(arena+64+kGfxResolvedCharacter)=arena+80;
    *(void**)(arena+80)=arena+96;
    MarkerIdentityTrace(arena,0);
    check(logs==renderLogs+6,"one validated native family prints attempt, identity and four slot records");
    const unsigned firstReads=reads,firstLogs=logs;
    for(unsigned i=0;i<10000;++i){nowMs+=1000;MarkerIdentityTrace(arena,0);}
    check(reads==firstReads && logs==firstLogs,"successful native family never rereads or logs again");
    *(uint32_t*)(arena+kMarkerGfxType)=1;
    for(unsigned i=0;i<10000;++i){nowMs+=1;MarkerIdentityTrace(arena,1);}
    check(logs==firstLogs+4 && reads==firstReads+4,"unresolved native type has four attempts total with a one-second gate");
    *(uint32_t*)(arena+kMarkerGfxType)=8;
    *(void**)(arena+kMarkerGfxHandle)=nullptr;
    const unsigned beforeNull=logs;
    for(unsigned i=0;i<10;++i){nowMs+=1000;MarkerIdentityTrace(arena,2);}
    check(logs==beforeNull+4,"null GFx handle refuses and exhausts its finite budget");
    const unsigned afterNative=logs;
    MarkerIdentityTrace(arena,3);
    check(logs==afterNative,"unknown native family cannot index the capture table");
    printf("hud identity: %u checks, %u failures, %u stacks, %u logs, max line %u; exhausted gate %.2f ns/call (host only)\n",checks,errors,captures,logs,maxLine,ns);
    return errors?1:0;
}
