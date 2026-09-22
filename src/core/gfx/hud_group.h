// core/gfx/hud_group.h - the pieces of one HUD widget share one layer (VR-186).
// Pure, host-tested (tools/hud-route-tests.cpp); no D3D, no engine, no logging.
//
// The mod cannot read which Scaleform clip a draw belongs to. What it CAN see
// is the order and the rectangles: Scaleform paints a clip's children back to
// back, so a widget (a button plate and its glyph, the sneak icon and its
// background, the dialogue A button and its plate) is a RUN of consecutive HUD
// draws whose rectangles overlap or touch. Each run's OWNER is its most
// strongly identified piece (the interaction group, then a measured row, then
// `default`), and a weakly identified piece in the run takes the owner's
// element, so every piece of the widget rides the owner's layer.
//
// A draw is routed the moment it is drawn, often before the piece that owns
// its widget, so the runs are built over one present and APPLIED from the
// next: a widget can be split for the single present it first appears on.
// A piece is recorded with its own decision, never the lifted one, so a group
// cannot feed its own owner back to itself.
#pragma once
#include <stdint.h>

namespace dvr::hudgroup {

// How strongly a draw's own route identifies it. Only a draw at or below
// kDefault can be lifted; anything with its own identity is an owner.
enum Strength : int { kWeak = 0, kDefault = 1, kRow = 2, kInteraction = 3 };

struct Group { float r[4]; int owner; int strength; int members; };

struct Builder {
    static const int kMax = 64;
    Group cur[kMax]{}; int curN = 0; bool open = false;
    Group prev[kMax]{}; int prevN = 0; uint32_t prevFrame = 0; bool prevOk = false;
    uint32_t frame = 0; bool started = false;
    unsigned overflow = 0;

    void clear() { *this = Builder{}; }
    static bool touches(const float a[4], const float b[4], float m) {
        return a[0] <= b[2] + m && a[2] >= b[0] - m && a[1] <= b[3] + m && a[3] >= b[1] - m;
    }
    // Call with every HUD draw's present number before add/lookup.
    void begin(uint32_t now) {
        if (started && now == frame) return;
        // A present with no HUD draws keeps the last groups (lookup ages them).
        if (started && curN > 0) {
            prevN = 0;
            for (int i = 0; i < curN; ++i) if (cur[i].members >= 2) prev[prevN++] = cur[i];
            prevFrame = frame; prevOk = true;
        }
        curN = 0; open = false; frame = now; started = true;
    }
    // A draw that must not join a neighbour's widget (a native marker, a
    // full-screen fill, a riding screen) ends the current run.
    void cut() { open = false; }
    void add(const float r[4], int element, int strength) {
        const float w = r[2] - r[0], h = r[3] - r[1];
        if (!(w >= 0 && h >= 0 && w < .6f && h < .6f)) { open = false; return; }
        if (open) {
            Group& g = cur[curN - 1];
            float u[4] = { r[0] < g.r[0] ? r[0] : g.r[0], r[1] < g.r[1] ? r[1] : g.r[1],
                           r[2] > g.r[2] ? r[2] : g.r[2], r[3] > g.r[3] ? r[3] : g.r[3] };
            // Two differently identified owners are two widgets, however close.
            const bool rival = strength >= kRow && g.strength >= kRow && element != g.owner;
            if (!rival && touches(r, g.r, .01f) && u[2] - u[0] <= .6f && u[3] - u[1] <= .4f) {
                for (int i = 0; i < 4; ++i) g.r[i] = u[i];
                ++g.members;
                if (strength > g.strength) { g.owner = element; g.strength = strength; }
                return;
            }
        }
        if (curN == kMax) { ++overflow; open = false; return; }
        Group& g = cur[curN++];
        for (int i = 0; i < 4; ++i) g.r[i] = r[i];
        g.owner = element; g.strength = strength; g.members = 1; open = true;
    }
    // The owner a weak draw should take: the smallest group of the previous
    // present that holds the draw and is owned more strongly than it. -1 = none.
    int lookup(const float r[4], int strength, int* groupOut = nullptr) const {
        if (!prevOk || strength > kDefault || frame - prevFrame > 3) return -1;
        int best = -1; float bestArea = 1e9f;
        for (int i = 0; i < prevN; ++i) {
            const Group& g = prev[i];
            if (g.strength <= strength) continue;
            if (r[0] < g.r[0] - .01f || r[1] < g.r[1] - .01f || r[2] > g.r[2] + .01f || r[3] > g.r[3] + .01f) continue;
            const float a = (g.r[2] - g.r[0]) * (g.r[3] - g.r[1]);
            if (a < bestArea) { bestArea = a; best = i; }
        }
        if (groupOut) *groupOut = best;
        return best >= 0 ? prev[best].owner : -1;
    }
};

} // namespace dvr::hudgroup
