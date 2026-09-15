// tools/hud-route-tests.cpp - the HUD element routing, on the host (VR-120).
// Build and run: tools\hud-route-host.ps1. Pure: core/gfx/hud_route.h only.
#include "core/gfx/hud_route.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned checks = 0;
static void check(bool value, const char* why) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}

using dvr::hudroute::Identity;
using dvr::hudroute::Row;

static Identity rect_id(float x0, float y0, float x1, float y1) {
    Identity id; id.context = -1; id.hasRect = true;
    id.rect[0] = x0; id.rect[1] = y0; id.rect[2] = x1; id.rect[3] = y1;
    return id;
}

int main() {
    // The shipped table's shape: default, three measured rows, one unmeasured,
    // the vignette rule, two screens.
    const Row rows[] = {
        { "default",   -1, {0, 0, 0, 0},                 false },
        { "vitals",    -1, {0.0f, 0.0f, 0.20f, 0.27f},   false },
        { "reticle",   -1, {0.47f, 0.47f, 0.53f, 0.53f}, false },
        { "prompt",    -1, {0.52f, 0.46f, 0.80f, 0.62f}, false },
        { "equipment", -1, {0, 0, 0, 0},                 false },
        { "vignette",  -1, {0, 0, 0, 0},                 true },
        { "pause",      3, {0, 0, 0, 0},                 false },
        { "wheel",      6, {0, 0, 0, 0},                 false },
    };
    const int n = sizeof(rows) / sizeof(rows[0]);
    enum { Default = 0, Vitals, Reticle, Prompt, Equipment, Vignette, Pause, Wheel };

    // ---- a known identity routes to its row -------------------------------
    check(dvr::hudroute::route(rows, n, rect_id(0.050f, 0.065f, 0.101f, 0.210f), Default) == Vitals,
          "the health fill (centre 0.076,0.138) routes to vitals");
    check(dvr::hudroute::route(rows, n, rect_id(0.015f, 0.074f, 0.148f, 0.220f), Default) == Vitals,
          "the shared vitals background (centre 0.081,0.147) routes to vitals");
    check(dvr::hudroute::route(rows, n, rect_id(0.497f, 0.497f, 0.503f, 0.503f), Default) == Reticle,
          "the reticle dot routes to reticle");
    check(dvr::hudroute::route(rows, n, rect_id(0.480f, 0.481f, 0.520f, 0.519f), Default) == Reticle,
          "the grown reticle routes to reticle");
    check(dvr::hudroute::route(rows, n, rect_id(0.524f, 0.481f, 0.774f, 0.602f), Default) == Prompt,
          "the interaction plate (centre 0.649,0.541) routes to prompt");
    check(dvr::hudroute::route(rows, n, rect_id(0.538f, 0.493f, 0.636f, 0.514f), Default) == Prompt,
          "the prompt's text run routes to prompt");

    // ---- an unknown identity routes to default ---------------------------
    check(dvr::hudroute::route(rows, n, rect_id(0.182f, 0.401f, 0.215f, 0.433f), Default) == Default,
          "the objective marker (a square that moves) routes to default");
    check(dvr::hudroute::route(rows, n, rect_id(0.90f, 0.90f, 0.95f, 0.95f), Default) == Default,
          "a rectangle no row claims routes to default");
    {
        Identity id; id.context = -1; id.hasRect = false; std::memset(id.rect, 0, sizeof(id.rect));
        check(dvr::hudroute::route(rows, n, id, Default) == Default, "a draw with no readable rectangle routes to default");
    }

    // ---- the vignette rule beats the rectangles ---------------------------
    check(dvr::hudroute::route(rows, n, rect_id(0.0f, 0.0f, 1.0f, 1.0f), Default) == Vignette,
          "a full-screen fill routes to vignette");
    check(dvr::hudroute::route(rows, n, rect_id(0.1f, 0.1f, 0.75f, 0.75f), Default) == Vignette,
          "wider AND taller than 60 % routes to vignette even though its centre is inside no row");
    check(dvr::hudroute::route(rows, n, rect_id(0.0f, 0.45f, 1.0f, 0.55f), Default) == Reticle,
          "a full-width band 10 % tall is not a vignette: its centre decides (the reticle here)");

    // ---- an unmeasured row never claims ------------------------------------
    check(!dvr::hudroute::row_measured(rows[Equipment]), "an all-zero region is unmeasured");
    {
        // Rows are visited in order: an unmeasured row before a measured one
        // is skipped, not matched.
        const Row two[] = {
            { "default", -1, {0, 0, 0, 0}, false },
            { "unmeasured", -1, {0, 0, 0, 0}, false },
            { "vitals", -1, {0.0f, 0.0f, 0.20f, 0.27f}, false },
        };
        check(dvr::hudroute::route(two, 3, rect_id(0.05f, 0.05f, 0.10f, 0.10f), 0) == 2,
              "an unmeasured row earlier in the table does not claim; the measured one after it does");
    }

    // ---- a screen context routes regardless of the rectangle --------------
    {
        Identity id = rect_id(0.050f, 0.065f, 0.101f, 0.210f);   // the health fill's rectangle
        id.context = 3;
        check(dvr::hudroute::route(rows, n, id, Default) == Pause, "while the pause menu rides, a vitals-shaped draw routes to pause");
        id.context = 6; id.hasRect = false;
        check(dvr::hudroute::route(rows, n, id, Default) == Wheel, "the wheel's context routes to wheel with no rectangle at all");
        id.context = 4;   // a note: no row in this table
        check(dvr::hudroute::route(rows, n, id, Default) == Default, "a riding screen with no row routes to default");
    }

    // ---- a renamed row keeps its route (the name is not the key) -----------
    {
        Row renamed[n];
        std::memcpy(renamed, rows, sizeof(rows));
        renamed[Vitals].name = "status";
        check(dvr::hudroute::route(renamed, n, rect_id(0.050f, 0.065f, 0.101f, 0.210f), Default) == Vitals,
              "renaming a row changes nothing about what it claims");
    }

    // ---- the centre rule, at the edges ------------------------------------
    check(dvr::hudroute::route(rows, n, rect_id(0.15f, 0.20f, 0.45f, 0.40f), Default) == Default,
          "a draw that overlaps vitals but whose centre (0.30,0.30) lies outside routes to default");
    check(dvr::hudroute::route(rows, n, rect_id(0.19f, 0.26f, 0.21f, 0.28f), Default) == Vitals,
          "a centre exactly on the region's far corner (0.20,0.27) is inside");

    std::printf("%u hud-route checks passed\n", checks);
    return 0;
}
