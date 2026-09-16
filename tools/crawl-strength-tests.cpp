#include <cstdint>
#include <cstring>
#include <cstdio>
#include <set>
static unsigned kClassOff=0x30, g_graftOffStr=0x64, g_graftOffSTgt=0x78;
static uint8_t* g_skcPlayer[8]; static uint32_t g_skcObjIdx[8]; static void* g_skcObjCls[8];
static int g_skcPlayerN=3, g_skcStale=0, g_skcCamIdx=-1; static bool g_crawlTuckCamera=false;
struct Header {void** data; uint32_t num,cap;};
static void* objects[16]; static Header header={objects,16,16};
static uintptr_t kGObjHdr=(uintptr_t)&header;
static std::set<const void*> live; static bool refreshOk=true; static int refreshes=0;
static const void* unreadable=nullptr;
static bool BuildLiveSet(){++refreshes;live.clear();if(!refreshOk)return false;for(auto p:objects)if(p)live.insert(p);return true;}
static bool IsLiveObject(const void* p){return p && live.count(p)!=0;}
static bool RangeReadable(const void* p,size_t){return p && p!=unreadable;}
static void Log(const char*,...){}
#include "crawl_strength_body.inc"
alignas(4) static uint8_t memory[3][256];
static int failed=0,checks=0;
static void check(bool ok,const char* msg){++checks;if(!ok){++failed;printf("FAIL %s\n",msg);}}
static void reset(){
 memset(memory,0x5a,sizeof(memory));memset(objects,0,sizeof(objects));
 live.clear();refreshOk=true;refreshes=0;unreadable=nullptr;g_skcStale=0;g_skcCamIdx=-1;g_crawlTuckCamera=false;
 for(int i=0;i<3;++i){g_skcPlayer[i]=memory[i];g_skcObjIdx[i]=i+1;
 g_skcObjCls[i]=(void*)0x12340000;*(void**)(memory[i]+kClassOff)=g_skcObjCls[i];objects[i+1]=memory[i];}
}
static float val(int i,unsigned off){float f;memcpy(&f,memory[i]+off,4);return f;}
int main(int argc,char**){
 bool legacy=argc>1;
 auto apply=[&](float v){if(legacy)LegacySetCrawlStrength(v);else SkcSetCrawlStrength(v);};
 reset();apply(0);check(legacy || refreshes==1,"fresh table built before edge");
 for(int i=0;i<3;++i)check(val(i,0x64)==0 && val(i,0x78)==0,"live controls released on tuck");
 apply(1);for(int i=0;i<3;++i)check(val(i,0x64)==1 && val(i,0x78)==1,"live controls restored on untuck");
 // Exact crash class: same three addresses, now unrelated live upgrades.
 reset();memset(objects,0,sizeof(objects));
 for(int i=0;i<3;++i){objects[i+8]=memory[i];*(void**)(memory[i]+kClassOff)=(void*)0x56780000;}
 uint8_t saved[sizeof(memory)];memcpy(saved,memory,sizeof(memory));apply(1);
 check(memcmp(saved,memory,sizeof(memory))==0,"recycled upgrades not overwritten with float 1.0");
 check(legacy || g_skcStale==1,"recycled cache scheduled for rediscovery");
 reset();*(void**)(memory[0]+kClassOff)=(void*)0x56780000;memcpy(saved,memory,sizeof(memory));apply(1);
 check(memcmp(saved,memory[0],256)==0,"same GObjects slot reused by different class refused");
 reset();objects[1]=nullptr;memcpy(saved,memory,sizeof(memory));apply(0);
 check(memcmp(saved,memory[0],256)==0,"readable freed allocation untouched");
 reset();refreshOk=false;memcpy(saved,memory,sizeof(memory));apply(1);
 check(legacy || memcmp(saved,memory,sizeof(memory))==0,"failed fresh table prevents all writes");
 reset();unreadable=memory[0]+0x78;memcpy(saved,memory,sizeof(memory));apply(0);
 check(legacy || memcmp(saved,memory[0],256)==0,"second field unreadable prevents partial write");
 // VR-122: the camera's look-at control is not released with the hands.
 reset();g_skcCamIdx=0;memcpy(saved,memory,sizeof(memory));apply(0);
 check(legacy || memcmp(saved,memory[0],256)==0,"camera control left alone on tuck");
 check(val(1,0x64)==0 && val(2,0x64)==0 && val(1,0x78)==0 && val(2,0x78)==0,"hand controls still released with a camera slot present");
 {float z=0.0f;memcpy(memory[0]+0x64,&z,4);memcpy(memory[0]+0x78,&z,4);}apply(1);
 check(val(0,0x64)==1 && val(0,0x78)==1,"a release restores a camera an earlier tuck zeroed");
 reset();g_skcCamIdx=0;g_crawlTuckCamera=true;apply(0);
 check(val(0,0x64)==0 && val(0,0x78)==0,"CrawlTuckCamera=1 releases the camera control (the old behaviour)");
 reset();g_skcCamIdx=-1;apply(0);for(int i=0;i<3;++i)check(val(i,0x64)==0,"no camera slot: every control released");
 printf("crawl strength %s: %d checks, %d failures\n",legacy?"legacy":"current",checks,failed);
 return failed ? 1 : 0;
}
