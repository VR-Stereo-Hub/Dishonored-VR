#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
static uint32_t rows[20][6];
static int count=2;
static bool live=true, readable=true;
static const int kGoStrideDwords=6;
static bool IsLiveObject(uint8_t*) { return live; }
static uint32_t RflOffsetOf(const char*,const char*) { return 4; }
static bool RflArrayAt(uint8_t*,uint32_t,uint8_t** p,int32_t* n) { *p=(uint8_t*)rows;*n=count;return true; }
static bool RangeReadable(void*,size_t) { return readable; }
static void Log(const char*,...) {}
struct GoRaw {int32_t owner,id,type,value;bool ok;};
static bool BuildLiveSet() {return live;}
static int calls=0;
static bool nativeWorks=true;
static GoRaw GoReadRaw(uint8_t*,int id) {
 for(int i=0;i<count;++i) if(rows[i][0]==2 && rows[i][1]==(uint32_t)id)
  return {(int)rows[i][0],id,(int)rows[i][2],(int)rows[i][3],true};
 return {0,0,0,0,false};
}
static bool GoCallSettingChange(int id,double v) {
 ++calls;
 if(!nativeWorks) return false;
 for(int i=0;i<count;++i) if(rows[i][0]==2 && rows[i][1]==(uint32_t)id) {
  if(id==108) {float f=(float)v;memcpy(&rows[i][3],&f,4);} else rows[i][3]=(uint32_t)v;
 }
 return true;
}
static bool g_goStartupDone=false, g_goStartupPolicyRead=false, g_goDefaultsAtStartup=true;
static int policy=1;
static const int MAX_PATH=260;
static const char* g_dir="test";
static unsigned GetPrivateProfileIntA(const char*,const char*,int,const char*) {return policy;}
#include "game_opts_body.inc"
static int checks=0;
static void require(bool v) { ++checks; if(!v) { printf("FAIL %d\n",checks);exit(1); } }
int main() {
    uint8_t object=0; int32_t before=0; int n,a,r;
    rows[0][0]=1; rows[0][1]=121; rows[0][2]=1; rows[0][3]=7;
    rows[1][0]=2; rows[1][1]=121; rows[1][2]=1;
    require(GoVerifyStride(&object,&n,&a,&r)); // same id, different owner is legal
    require(GoWriteRaw(&object,121,1,&before));
    require(rows[0][3]==7 && rows[1][3]==1); // never change Live-owned entry
    live=false; require(!GoWriteRaw(&object,121,0,&before)); require(rows[1][3]==1); live=true;
    rows[1][2]=5; require(!GoWriteRaw(&object,121,0,&before)); require(rows[1][3]==1); rows[1][2]=1;
    rows[0][0]=2; require(!GoVerifyStride(&object,&n,&a,&r)); require(!GoWriteRaw(&object,121,0,&before)); rows[0][0]=1;
    rows[0][1]=154; require(!GoWriteRaw(&object,121,0,&before)); rows[0][1]=121;
    rows[0][2]=99; require(!GoWriteRaw(&object,121,0,&before)); rows[0][2]=1;
    require(!GoWriteRaw(&object,116,1,&before)); require(!GoWriteRaw(&object,117,0,&before));
    require(!GoWriteRaw(&object,108,1,&before)); require(!GoWriteRaw(&object,121,2,&before));
    readable=false; require(!GoWriteRaw(&object,121,0,&before)); readable=true;
    count=0; require(!GoVerifyStride(&object,&n,&a,&r));
    count=1;rows[0][0]=2;rows[0][1]=108;rows[0][2]=5;rows[0][3]=0;
    require(GoApplyWritesAndVerify(&object,"108=0.75"));
    float f;memcpy(&f,&rows[0][3],4);require(f==0.75f);
    int was=calls;require(GoApplyWritesAndVerify(&object,"108=0.75"));require(calls==was+1); // same value still applies
    nativeWorks=false;was=calls;require(!GoApplyWritesAndVerify(&object,"108=0"));require(calls==was+1);
    memcpy(&f,&rows[0][3],4);require(f==0.75f);nativeWorks=true; // no raw fallback
    const char* invalid[]={"108=nan","108=inf","108=100","108=0,","108=0,108=1","108=0,116=1","121=0.5","108=0,bad","108="};
    for(auto spec:invalid) {was=calls;require(!GoApplyWritesAndVerify(&object,spec));require(calls==was);}
    const int presetIds[]={105,108,109,99,81,83,120,121,122,123,116,117};
    count=12;
    for(int i=0;i<count;++i) {
        rows[i][0]=2;rows[i][1]=presetIds[i];rows[i][2]=i==1?5:1;rows[i][3]=7;
    }
    rows[9][2]=5; require(!GoWriteStartupDefaults(&object));
    for(int i=0;i<count;++i) require(rows[i][3]==7); // no partial writes
    rows[9][2]=1;
    live=false; require(!GoWriteStartupDefaults(&object));live=true;
    readable=false;require(!GoWriteStartupDefaults(&object));readable=true;
    require(GoWriteStartupDefaults(&object));
    for(int i=0;i<10;++i) require(rows[i][3]==(i==6||i==8?1u:0u));
    require(rows[10][3]==7 && rows[11][3]==7); // fullscreen/vsync untouched
    memcpy(&f,&rows[1][3],4);require(f==0.0f);
    rows[0][3]=7;rows[9][1]=124;
    require(!GoWriteStartupDefaults(&object));require(rows[0][3]==7); // missing target
    rows[9][1]=123;
    GoBeforeSettingsApply(&object,1);require(rows[0][3]==7 && !g_goStartupDone);
    live=false;GoBeforeSettingsApply(&object,0);require(!g_goStartupDone);live=true;
    GoBeforeSettingsApply(&object,0);require(!g_goStartupDone && rows[0][3]==0);
    rows[0][3]=1;GoBeforeSettingsApply(&object,0);require(rows[0][3]==0); // profile reloaded during startup
    g_goStartupDone=true;rows[0][3]=1;
    GoBeforeSettingsApply(&object,0);require(rows[0][3]==1); // gameplay closes startup window
    GoBeforeSettingsApply(&object,1);require(rows[0][3]==1); // manual edits survive
    g_goStartupDone=false;g_goStartupPolicyRead=false;policy=0;
    GoBeforeSettingsApply(&object,0);require(g_goStartupDone && rows[0][3]==1); // saved opt-out
    g_goStartupDone=false;g_goStartupPolicyRead=false;policy=1;
    GoBeforeSettingsApply(&object,0);require(rows[0][3]==0); // next launch resets again
    count=17;
    const int audioIds[]={126,127,128,129,133};
    for(int i=0;i<5;++i) {rows[12+i][0]=2;rows[12+i][1]=audioIds[i];rows[12+i][2]=i==4?1:5;rows[12+i][3]=i==4?2:0x3f400000;}
    require(GoWriteStartupDefaults(&object));
    for(int i=0;i<5;++i) require(rows[12+i][3]==(i==4?2u:0x3f400000u));
    printf("PASS %d production validation/write checks\n",checks);
}
