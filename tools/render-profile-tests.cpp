#include <cassert>
#include <cstdio>
#include <cstdarg>
#include <string>
#include "render-profile-stubs/windows.h"
DWORD testThread=7;
static double timeMs=1;
static std::string output;
namespace dvr::clock { double now_ms() { timeMs+=.001; return timeMs; } }
void test_log(const char* fmt,...) {
    char line[2048]; va_list a; va_start(a,fmt); vsnprintf(line,sizeof(line),fmt,a); va_end(a);
    output+=line; output+='\n';
}
#include "../src/core/framework/render_profile.cpp"
int main() {
    using namespace dvr::render_profile;
    Stats s; unsigned chosen=0;
    for(int i=0;i<640000;++i) if(s.visit()) { ++chosen; s.record(.25); }
    assert(s.calls==640000 && chosen>9500 && chosen<10500);
    assert(s.samples==chosen && s.mean()==.25 && s.maxMs==.25);
    const auto seed=s.random; s.clear(); assert(!s.calls&&!s.samples&&!s.sumMs&&s.random==seed);
    tick(true); { Scope off(LayoutRefresh); } assert(!stats[LayoutRefresh].calls);
    set_enabled(true); tick(true);
    for(int i=0;i<64000;++i) { Scope p(LayoutRefresh); p.finish(); p.finish(); }
    assert(stats[LayoutRefresh].calls==64000 && stats[LayoutRefresh].samples>900 && stats[LayoutRefresh].samples<1100);
    assert(stats[LayoutRefresh].mean()>.0009 && stats[LayoutRefresh].mean()<.0011);
    testThread=8; { Scope other(LayoutRefresh); } assert(stats[LayoutRefresh].calls==64000); testThread=7;
    timeMs+=3000; tick(true); assert(!stats[LayoutRefresh].calls);
    assert(output.find("calls=64000")!=std::string::npos);
    { Scope p(Reflection); } assert(stats[Reflection].calls==1);
    tick(false); assert(!stats[Reflection].calls); // menu transition discards partial window
    set_enabled(false); tick(false); { Scope p(Bytecode); } assert(!stats[Bytecode].calls);
    puts("PASS: sampling population, bounded aggregation, idempotent close, off/owner guards and transition reset");
}
