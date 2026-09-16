#include <cassert>
#include <cstdio>
#include <cstdarg>
#include <string>
#include "native-profile-stubs/windows.h"
DWORD testThread=7;
static double timeMs=1;
static std::string output;
namespace dvr::clock { double now_ms() { timeMs+=.001; return timeMs; } }
void test_log(const char* fmt,...) {
    char line[2048]; va_list a; va_start(a,fmt); vsnprintf(line,sizeof(line),fmt,a); va_end(a);
    output+=line; output+='\n';
}
#include "../src/core/framework/native_profile.cpp"
int main() {
    using namespace dvr::native_profile;
    Stats s; unsigned chosen=0;
    for(int i=0;i<640000;++i) if(s.visit()) { ++chosen; s.record(.25); }
    assert(s.calls==640000 && chosen>9500 && chosen<10500);
    assert(s.samples==chosen && s.mean()==.25 && s.maxMs==.25);
    const auto seed=s.random; s.clear(); assert(!s.calls&&!s.samples&&!s.sumMs&&s.random==seed);
    tick(true); { Scope off(IndexedHook); } assert(!stats[IndexedHook].calls);
    set_enabled(true); tick(true);
    for(int i=0;i<64000;++i) { Scope p(IndexedHook); p.finish(); p.finish(); }
    assert(stats[IndexedHook].calls==64000 && stats[IndexedHook].samples>900 && stats[IndexedHook].samples<1100);
    assert(stats[IndexedHook].mean()>.0009 && stats[IndexedHook].mean()<.0011);
    testThread=8; { Scope other(IndexedHook); } assert(stats[IndexedHook].calls==64000); testThread=7;
    timeMs+=3000; tick(true); assert(!stats[IndexedHook].calls);
    assert(output.find("calls=64000")!=std::string::npos);
    { Scope p(NativeIndexed); } assert(stats[NativeIndexed].calls==1);
    tick(false); assert(!stats[NativeIndexed].calls); // menu transition discards partial window
    set_enabled(false); tick(false); { Scope p(NativePrimitive); } assert(!stats[NativePrimitive].calls);
    set_enabled(true); tick(true);
    for(int k=0;k<Count;++k) { assert(names[k] && names[k][0]); Scope p(static_cast<Kind>(k)); }
    for(int k=0;k<Count;++k) assert(stats[k].calls==1);
    puts("PASS: sampling population, bounded aggregation, idempotent close, off/owner guards and transition reset");
}
