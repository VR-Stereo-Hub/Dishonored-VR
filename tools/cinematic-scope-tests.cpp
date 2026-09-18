// Exercises production camera scope bodies extracted by cinematic-head-host.ps1.
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
using DWORD = uint32_t;
DWORD threadId=1;
DWORD GetCurrentThreadId() { return threadId; }
uint8_t memory[128]={};
constexpr uint32_t kPovOffs[]={32};
struct Field { uint32_t off; };
const Field kFields[]={{32}};
int g_field=0;
bool identityLive=true, readable=true, applyOk=true, baseOk=true;
int validates=0, dieOnValidate=0, applies=0;
bool validate(uint8_t* p) {
    ++validates;
    return identityLive && p==memory && (!dieOnValidate || validates<dieOnValidate);
}
bool RangeReadable(const void*,int) { return readable; }
struct Writer;
bool current_base(uint8_t*,uint32_t,Writer&,float*);
void position_offset_uu(float* p) { p[0]=11;p[1]=22;p[2]=33; }
void cinematic_position_offset_uu(float* p) { p[0]=1;p[1]=2;p[2]=3; }
bool apply_offsets(uint8_t*);
bool end_view_scope();
float g_scale=100;
namespace dvr::anim { float testRight=0; float view_right_metres(){return testRight;} }
#include "cinematic-scope-extracted.h"
bool current_base(uint8_t* p,uint32_t offset,Writer& prior,float* out) {
    if (!baseOk) return false;
    memcpy(out,p+offset,12);
    if(prior.lastOk && memcmp(out,prior.last,12)==0)
        for(int i=0;i<3;++i) out[i]-=prior.lastOff[i];
    return true;
}
bool apply_offsets(uint8_t* p) {
    ++applies;
    if (!applyOk) return false;
    float values[3];
    for(int i=0;i<3;++i) values[i]=g_viewScope.base[i]+g_viewScope.pos[i]+4;
    memcpy(p+32,values,12);
    g_eyeWriter.lastOk=true;g_eyeWriter.camera=p;g_eyeWriter.fieldOff=32;
    memcpy(g_eyeWriter.last,values,12);++g_eyeWriter.writes;
    return true;
}
int failures=0;
void check(bool ok,const char* name) {
    printf("%s %s\n",ok?"PASS":"FAIL",name);if(!ok)++failures;
}
const int32_t originalRot[]={12,34,56}, injected[]={70,80,90};
const float originalPos[]={10,20,30}, right[]={0,1,0};
void reset() {
    memset(memory,0,sizeof(memory));memcpy(memory,originalRot,12);memcpy(memory+32,originalPos,12);
    g_viewScope=ViewScope{};g_eyeWriter=Writer{};
    g_eyeWriter.lastOk=true;g_eyeWriter.camera=memory;g_eyeWriter.fieldOff=32;
    memcpy(g_eyeWriter.last,originalPos,12);g_eyeWriter.lastOff[0]=2;g_eyeWriter.writes=91;
    threadId=1;identityLive=readable=applyOk=baseOk=true;validates=dieOnValidate=applies=0;g_field=0;
}
bool begin() { return begin_view_scope(memory,0,injected,right,-1,validate,true,nullptr); }
bool fieldsOriginal() {return memcmp(memory,originalRot,12)==0 && memcmp(memory+32,originalPos,12)==0;}
bool writerSame(const Writer& a,const Writer& b) {
    return a.lastOk==b.lastOk && a.camera==b.camera && a.fieldOff==b.fieldOff &&
      a.writes==b.writes && memcmp(a.last,b.last,12)==0 && memcmp(a.lastOff,b.lastOff,12)==0;
}
int main() {
    reset();dvr::anim::testRight=.05f;begin();
    check(g_viewScope.animationRightUu==5,"animation alignment converts metres to world scale");
    dvr::anim::testRight=-.1f;
    check(g_viewScope.animationRightUu==5,"both eyes retain scope-entry alignment despite live setting change");
    check(end_view_scope() && fieldsOriginal(),"animation alignment restores incoming fields");
    dvr::anim::testRight=0;
    reset();Writer previous=g_eyeWriter;
    bool entered=begin();
    check(entered && g_viewScope.pos[0]==1 && g_viewScope.pos[1]==2 && g_viewScope.pos[2]==3,
          "authored scope freezes raw translation instead of gameplay cancellation");
    check(entered && memcmp(memory,injected,12)==0 && !fieldsOriginal(),"scope applies temporary rotation and position");
    check(end_view_scope() && fieldsOriginal() && writerSame(previous,g_eyeWriter) && !g_viewScope.thread,
          "scope restores incoming rotation position and Writer provenance");
    reset();previous=g_eyeWriter;identityLive=false;
    check(!begin() && fieldsOriginal() && applies==0 && writerSame(previous,g_eyeWriter),"dead identity refuses before camera writes");
    reset();dieOnValidate=2;
    check(!begin() && fieldsOriginal() && applies==0 && !g_viewScope.thread,"identity lost during preparation refuses before writes");
    reset();begin();uint8_t held[128];memcpy(held,memory,sizeof(held));identityLive=false;
    check(!end_view_scope() && memcmp(held,memory,sizeof(held))==0 && !g_eyeWriter.lastOk && !g_viewScope.thread,
          "identity changed during draw refuses restore and clears stale provenance");
    reset();begin();int32_t externalRot[]={101,102,103};float externalPos[]={41,42,43};
    memcpy(memory,externalRot,12);memcpy(memory+32,externalPos,12);
    check(!end_view_scope() && memcmp(memory,externalRot,12)==0 && memcmp(memory+32,externalPos,12)==0 && !g_eyeWriter.lastOk,
          "externally recomputed fields are not overwritten");
    reset();begin();memcpy(held,memory,sizeof(held));previous=g_eyeWriter;threadId=2;
    check(end_view_scope() && memcmp(held,memory,sizeof(held))==0 && writerSame(previous,g_eyeWriter) && g_viewScope.thread==1,
          "wrong thread end performs no writes or ownership changes");
    threadId=1;check(end_view_scope() && fieldsOriginal(),"owner thread can restore after wrong-thread attempt");
    reset();previous=g_eyeWriter;readable=false;
    check(!begin() && fieldsOriginal() && applies==0 && writerSame(previous,g_eyeWriter),"unreadable fields refuse before writes");
    reset();baseOk=false;
    check(!begin() && fieldsOriginal() && applies==0,"unavailable authored base refuses before writes");
    reset();
    check(begin_view_scope(memory,0,injected,right,-1,validate,false,nullptr) &&
          g_viewScope.pos[0]==11 && g_viewScope.pos[1]==22 && g_viewScope.pos[2]==33,
          "pitch-only scope preserves gameplay translation request");
    check(end_view_scope() && fieldsOriginal(),"pitch-only scope restores exact incoming fields");
    reset();const float menuPos[3]={7,8,9};
    check(begin_view_scope(memory,0,injected,right,-1,validate,false,menuPos) &&
          g_viewScope.pos[0]==7 && g_viewScope.pos[1]==8 && g_viewScope.pos[2]==9,
          "menu entry-relative translation overrides changing gameplay neck cancellation");
    check(end_view_scope() && fieldsOriginal(),"menu position override restores exact incoming fields");
    reset();const float invalidPos[3]={NAN,0,0};
    check(!begin_view_scope(memory,0,injected,right,-1,validate,false,invalidPos) && fieldsOriginal(),
          "nonfinite menu position refused before writing");
    printf("Cinematic scope: %d failure(s)\n",failures);return failures?1:0;
}
