// core/gfx/hud_route.h - which ELEMENT a HUD draw belongs to (VR-120). Pure,
// host-tested (tools/hud-route-tests.cpp); no D3D, no engine, no logging.
//
// The identity of a HUD draw on this build is a PAIR (ENGINE_NOTES, "How the
// Scaleform HUD identifies its elements"): the UI owner CONTEXT while a screen
// rides (the pause menu, a note, the journal, the wheel, the store, mission
// stats: the screen routes as one element whatever its draws' rectangles),
// and otherwise the draw's screen RECTANGLE (normalised backbuffer, y down),
// read from its vertices through the vertex shader's own transform. A row of
// the table names an element by one of the two; a draw no row claims routes
// to the DEFAULT row, so nothing is ever dropped and an unnamed element is
// still visible (and counted, so it can be named).
#pragma once

namespace dvr::hudroute {

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
