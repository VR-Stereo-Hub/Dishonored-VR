#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static uint32_t rows[8][6];
static int count=2;
static bool live=true, readable=true;
static const int kGoStrideDwords=6;
static bool IsLiveObject(uint8_t*) { return live; }
static uint32_t RflOffsetOf(const char*,const char*) { return 4; }
static bool RflArrayAt(uint8_t*,uint32_t,uint8_t** p,int32_t* n) { *p=(uint8_t*)rows;*n=count;return true; }
static bool RangeReadable(void*,size_t) { return readable; }
static void Log(const char*,...) {}
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
    printf("PASS %d production validation/write checks\n",checks);
}
