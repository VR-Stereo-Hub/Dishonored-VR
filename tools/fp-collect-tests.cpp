#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
static unsigned queries=0,checks=0;
static uint8_t* live=nullptr;
static bool IsLiveObject(uint8_t* p){return p && p==live;}
static bool RangeReadable(const void* p,size_t n) {
    ++queries;auto a=(uintptr_t)p;const auto end=a+n;
    while(a<end) {MEMORY_BASIC_INFORMATION m{};
        if(!VirtualQuery((void*)a,&m,sizeof(m)) || m.State!=MEM_COMMIT ||
            (m.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
        const auto stop=(uintptr_t)m.BaseAddress+m.RegionSize;
        if(stop<=a)return false;a=stop;
    }return true;
}
static void check(bool ok,const char* why){++checks;if(!ok){printf("FAIL %s\n",why);exit(1);}}
static std::vector<uint8_t*> scan(uint8_t* o) {
    std::vector<uint8_t*> found;
#include "fp_scan.inc"
        found.push_back(c);
    }
    return found;
}
int main() {
    uint8_t object[0x600]{};uint8_t target[32]{};live=target;
    memcpy(object+0x20,&live,4);memcpy(object+0x5fc,&live,4);
    uintptr_t garbage=0xffff0000;memcpy(object+0x40,&garbage,4);
    auto found=scan(object);
    check(found.size()==2,"both ends of complete scan range retained");
    check(queries==1,"one readability query for376 pointer slots");
    live=nullptr;check(scan(object).empty(),"retired and arbitrary pointers rejected without dereference");live=target;
    auto* pages=(uint8_t*)VirtualAlloc(nullptr,8192,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    check(pages!=nullptr,"fixture allocation");DWORD old=0;
    check(VirtualProtect(pages+4096,4096,PAGE_NOACCESS,&old)!=0,"guarded boundary fixture");
    auto* partial=pages+4096-256;memcpy(partial+0x20,&live,4);memcpy(partial+252,&live,4);
    found=scan(partial);check(found.size()==2,"partial readable region preserves fields before boundary and stops safely");
    VirtualFree(pages,0,MEM_RELEASE);
    printf("%u production pointer-scan checks passed\n",checks);
}
