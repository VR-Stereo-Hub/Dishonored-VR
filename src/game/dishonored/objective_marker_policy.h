#pragma once
#include <cmath>
#include <cstdint>
namespace dvr::objectivemarkers {
// Only visible offscreen task markers move. Preserve the ray from screen center.
inline bool inset_position(float& x,float& y,int w,int h,uint32_t flags,float inset) {
    if((flags&9)!=9 || !std::isfinite(x) || !std::isfinite(y) ||
       !std::isfinite(inset) || inset<.05f || inset>.30f || w<64 || h<64 || w>16384 || h>16384) return false;
    const float dx=x-w*.5f,dy=y-h*.5f;
    float t=1;
    if(std::fabs(dx)>w*(.5f-inset)) t=w*(.5f-inset)/std::fabs(dx);
    if(std::fabs(dy)>h*(.5f-inset)) t=std::fmin(t,h*(.5f-inset)/std::fabs(dy));
    if(!(t<1)) return false;
    x=w*.5f+dx*t;y=h*.5f+dy*t;return true;
}
inline bool rune_symbol(const wchar_t* text,int count) {
    const wchar_t expected[]=L"runeMarker";
    if(!text || count!=sizeof(expected)/sizeof(expected[0])) return false;
    for(int i=0;i<count;++i) if(text[i]!=expected[i]) return false;
    return true;
}
// VR-149: the Heart's marker code is SHARED between runes and bone charms
// (ENGINE_NOTES, VR-129) - one vtable, one update, one parent call, and the
// only thing that tells them apart is the Flash symbol the borrowed settings
// carry. `runeMarker` is the one that was measured; the bone charm's name has
// never been read, because it is authored in the SWF and appears nowhere in
// the image (searched as both ANSI and UTF-16, and the packages are
// LZO-compressed). So it is not guessed here. Instead:
//   - heart_symbol_ascii copies the borrowed symbol into a bounded ASCII buffer
//     so the log can NAME every Heart symbol a run sees, including the ones
//     that are refused. One run with a bone charm on the Heart prints it.
//   - heart_symbol_layout accepts the bounded shape any Heart symbol has,
//     rather than the exactly-11 count that only `runeMarker` can satisfy.
// A symbol that is neither is still refused unless the all-symbols lever is on.
inline bool heart_symbol_layout(int count,int capacity) {
    return count>=2 && count<=64 && capacity>=count && capacity<=4096;
}
// Returns the number of characters written (never more than n-1), 0 on refusal.
// Non-ASCII is written as '?' so a hostile or corrupt buffer cannot smuggle
// control bytes into the log line.
inline int heart_symbol_ascii(const wchar_t* text,int count,char* out,int n) {
    if(!text || !out || n<2 || count<1) { if(out&&n>0)out[0]=0; return 0; }
    int w=0;
    for(int i=0;i<count && w<n-1;++i) {
        const wchar_t c=text[i];
        if(!c) break;
        out[w++]=(c>=0x20 && c<0x7f)?(char)c:'?';
    }
    out[w]=0;return w;
}
bool rune_enabled();
bool rune_ownership();
bool heart_all_symbols();
void configure_rune_ownership(bool on);
void configure_heart_all_symbols(bool on);
bool match_rune_draw(const float* rect,float targetW,float targetH,float* pivot);
void clear_rune_positions();
float rune_inset();
void configure_runes(bool on,float margin);
bool enabled();
float inset();
void configure(bool on,float margin);
// VR-148: the awareness meter over an enemy head. Its own family, its own
// vtable, and no symbol test - the vtable is not shared with anything else.
bool awareness_enabled();
void configure_awareness(bool on);
bool match_awareness_draw(const float* rect,float targetW,float targetH,float* pivot);
void clear_awareness_positions();
}
