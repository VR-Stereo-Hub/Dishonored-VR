// core/gfx/hud_route.h - which ELEMENT a HUD draw belongs to (VR-120). Pure,
// host-tested (tools/hud-route-tests.cpp); no D3D, no engine, no logging.
//
// The identity of a HUD draw on this build is a PAIR (ENGINE_NOTES, "How the
// Scaleform HUD identifies its elements"): the UI owner CONTEXT while a screen
// rides (the pause menu, a note, the journal, the wheel, the store, mission
// stats: the screen routes as one element whatever its draws' rectangles),
// otherwise initially the draw's screen RECTANGLE (normalised backbuffer, y down),
// read from its vertices through the vertex shader's own transform. A row of
// the table names an element by one of the two; a draw no row claims routes
// to the DEFAULT row, so nothing is ever dropped and an unnamed element is
// still visible (and counted, so it can be named). StableRoutes retains that
// initial hint for unchanged local draw content moving through shader transforms.
#pragma once
#include <stdint.h>
#include <math.h>

namespace dvr::hudroute {

// Short-lived draw-content association. Position is only the initial hint;
// it cannot change an observed draw's owner while the same content moves.
// This is not semantic Scaleform identity: animated/rebuilt geometry can miss.
struct StableRoutes {
    struct Entry { uint64_t key=0; uint32_t frame=0; int owner=0; bool ambiguous=false; uint32_t ambiguousFrame=0; float x=0,y=0; } entries[2048]{};
    void clear() { for(auto& e:entries) e=Entry{}; }
    void adopt(uint64_t key,uint32_t frame,int owner) {
        if(!key) return;
        Entry& e=entries[(key^(key>>32))%2048];
        if(e.key==key && e.frame==frame && !e.ambiguous) e.owner=owner;
    }
    int resolve(uint64_t key,uint32_t frame,int initial,const float* rect=nullptr) {
        if(!key) return initial;
        Entry& e=entries[(key^(key>>32))%2048];
        if(e.key!=key || frame-e.frame>240) e={key,frame,initial};
        if(e.ambiguous && frame-e.ambiguousFrame>2) e.ambiguous=false;
        const float x=rect ? (rect[0]+rect[2])*.5f : 0;
        const float y=rect ? (rect[1]+rect[3])*.5f : 0;
        // Repeated identical sprites can share every byte and resource. Two
        // positions in one present prove this key is not a unique element.
        if(e.frame==frame && rect && (e.x!=0 || e.y!=0) &&
           ((e.x-x)*(e.x-x)+(e.y-y)*(e.y-y))>.000004f) {e.ambiguous=true;e.ambiguousFrame=frame;}
        e.x=x;e.y=y;e.frame=frame;return e.ambiguous ? initial : e.owner;
    }
};

struct Row {
    const char* name;
    int   context;        // >= 0: the screen's UI owner context (dvr::mono::Context); -1: a gameplay element
    float rect[4];        // the region that claims a draw whose CENTRE lies inside; all zero = unmeasured
    bool  vignette;       // the rule "wider AND taller than 60 % of the screen" instead of a rectangle
};

struct Identity {
    int   context;        // the riding screen's context, or -1
    bool  hasRect;
    float rect[4];
};

inline bool row_measured(const Row& r) { return r.rect[2] > r.rect[0] && r.rect[3] > r.rect[1]; }

// VR-127: opt-in candidates, not semantic Scaleform ownership. Marker size
// comes from the measured moving 0.033 x 0.032 draw in ENGINE_NOTES. The
// +/-0.003 envelope is a test tolerance, not another measured identity.
inline bool objective_shape(const float r[4],unsigned vertices,unsigned primitives) {
    const float w=r[2]-r[0],h=r[3]-r[1];
    return vertices>=4 && vertices<=8 && primitives==2 &&
        w>=.030f && w<=.036f && h>=.029f && h<=.035f;
}
// Only the measured centered reticle is exempt from interaction grouping.
// A small button glyph passing THROUGH this region is not a reticle.
inline bool centered_reticle(const float r[4],unsigned primitives) {
    const float cx=(r[0]+r[2])*.5f,cy=(r[1]+r[3])*.5f;
    return primitives==2 && cx>=.499f && cx<=.501f && cy>=.499f && cy<=.501f &&
        r[2]-r[0]<.05f && r[3]-r[1]<.05f;
}
// VR-166: a GAUGE centred on the screen - the grenade cook ring (measured: 0.071..0.118
// square, centre 0.502,0.498, present only while cooking) - is the reticle's, not a
// prompt's, even though it is larger than the dot and drawn with more triangles.
inline bool centered_gauge(const float r[4]) {
    const float cx=(r[0]+r[2])*.5f,cy=(r[1]+r[3])*.5f,w=r[2]-r[0],h=r[3]-r[1];
    return cx>=.494f && cx<=.506f && cy>=.494f && cy<=.506f &&
           w>=.04f && w<=.14f && h>=.04f && h<=.14f && fabsf(w-h)<.025f;
}
struct InteractionGroup {
    uint32_t frame=0,previousFrame=0;bool currentOk=false,previousOk=false;
    float current[4]{},previous[4]{};
    void clear() {*this=InteractionGroup{};}
    bool near_group(const float r[4],uint32_t now) const {
        auto touches=[&](const float b[4]) {return r[0]<=b[2]+.02f && r[2]>=b[0]-.02f && r[1]<=b[3]+.025f && r[3]>=b[1]-.025f;};
        return (currentOk && now-frame<=2 && touches(current)) ||
               (previousOk && now-previousFrame<=2 && touches(previous));
    }
    bool claim(const float r[4],uint32_t now,bool seed) {
        if(now!=frame) {
            previousOk=currentOk && now-frame<=2;
            if(previousOk) {for(int i=0;i<4;++i) previous[i]=current[i];previousFrame=frame;}
            currentOk=false;frame=now;
        }
        const float w=r[2]-r[0],h=r[3]-r[1];
        if(!(w>0 && h>0 && w<.50f && h<.25f)) return false;
        auto touches=[&](const float b[4]) {
            return r[0]<=b[2]+.02f && r[2]>=b[0]-.02f &&
                   r[1]<=b[3]+.025f && r[3]>=b[1]-.025f;
        };
        if(!seed && !(currentOk && touches(current)) &&
            !(previousOk && now-previousFrame<=2 && touches(previous))) return false;
        if(!currentOk) {for(int i=0;i<4;++i) current[i]=r[i];currentOk=true;}
        else {
            float next[4];
            for(int i=0;i<2;++i) {next[i]=r[i]<current[i]?r[i]:current[i];next[i+2]=r[i+2]>current[i+2]?r[i+2]:current[i+2];}
            if(next[2]-next[0]>.50f || next[3]-next[1]>.30f) return seed;
            for(int i=0;i<4;++i) current[i]=next[i];
        }
        return true;
    }
};

// The row index a draw routes to. `defaultRow` is where the unclaimed go.
inline int route(const Row* rows, int n, const Identity& id, int defaultRow) {
    if (id.context >= 0) {
        for (int i = 0; i < n; ++i) if (rows[i].context == id.context) return i;
        return defaultRow;
    }
    if (!id.hasRect) return defaultRow;
    const float w = id.rect[2] - id.rect[0], h = id.rect[3] - id.rect[1];
    const float cx = (id.rect[0] + id.rect[2]) * 0.5f, cy = (id.rect[1] + id.rect[3]) * 0.5f;
    if (w > 0.6f && h > 0.6f) {
        for (int i = 0; i < n; ++i) if (rows[i].vignette) return i;
        return defaultRow;
    }
    for (int i = 0; i < n; ++i) {
        const Row& r = rows[i];
        if (r.context >= 0 || r.vignette || !row_measured(r)) continue;
        if (cx >= r.rect[0] && cx <= r.rect[2] && cy >= r.rect[1] && cy <= r.rect[3]) return i;
    }
    return defaultRow;
}

} // namespace dvr::hudroute
