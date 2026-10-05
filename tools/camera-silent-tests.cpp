// tools/camera-silent-tests.cpp - the camera-silent gate's two baselines, on the host.
//
// Compiles the production header (core/gfx/draw_present_progress.h) and drives it with
// game-thread schedules: per tick the c5 upload serial at the draw's ENTRY, the verdict,
// then the serial at the draw's RETURN. `uploads` are the camera uploads the render thread
// made while the draw call ran and in the idle interval after it.
//
// The shipped rule (grace off) is the negative control: it must FAIL the stalled and
// in-draw schedules for the reason the headset log gave (2026-10-05: a SINGLE tick after
// a 36 ms xrEndFrame stall, and after a 1 ms catch-up tick), or these checks could not
// see the fault. Built and run by tools/camera-silent-host.ps1. Never launches the game.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "core/gfx/draw_present_progress.h"

static int g_checks = 0;
static void check(bool ok, const char* why) {
    ++g_checks;
    if (!ok) { printf("FAIL %s\n", why); exit(1); }
}

// One game thread, as scene_draw.cpp runs it.
struct Game {
    dvr::stereo::DrawPresentProgress camera;
    uint32_t serial = 0, lastReturn = 0;
    bool grace = false;
    uint32_t live = 0, silent = 0, graceOnly = 0;
    // One tick: `idle` uploads arrived since the previous draw returned, `inDraw` arrive
    // while this draw call runs. Returns true when the gate says the scene is drawing.
    bool tick(uint32_t idle, uint32_t inDraw) {
        serial += idle;
        camera.begin(serial);
        const bool uploadSinceReturn = serial != lastReturn;
        const bool isSilent = dvr::stereo::camera_silent(grace, uploadSinceReturn, camera);
        if (isSilent) ++silent; else { ++live; if (!uploadSinceReturn) ++graceOnly; }
        serial += inDraw;
        lastReturn = serial;
        return !isSilent;
    }
};

static uint32_t rng(uint32_t& s) { s = s * 1664525u + 1013904223u; return s >> 8; }

int main() {
    // 1. Steady gameplay: uploads in every idle interval. Both rules agree, the lever adds nothing.
    for (int g = 0; g < 2; ++g) {
        Game a; a.grace = g != 0;
        a.tick(30, 0);
        for (int i = 0; i < 1000; ++i) check(a.tick(30, 2), "steady gameplay is live under both rules");
        check(a.graceOnly == 0, "steady gameplay never needs the grace");
    }
    // 2. Uploads only while the draw call runs (the paused-world shape, and any tick whose idle
    //    interval the render thread spent inside Present). The shipped rule goes silent on every
    //    tick: the fault. Entry to entry sees them.
    {
        Game off, on; on.grace = true;
        off.tick(30, 30); on.tick(30, 30);
        for (int i = 0; i < 200; ++i) {
            check(!off.tick(0, 30), "NEGATIVE CONTROL: the return baseline discards in-draw uploads (silent)");
            check(on.tick(0, 30), "entry-to-entry counts in-draw uploads");
        }
        check(on.graceOnly == 200 && on.silent == 0, "all 200 in-draw ticks kept stereo by the lever");
    }
    // 3. A present-thread stall: steady uploads, then ONE tick with none since the previous entry
    //    (the 36 ms xrEndFrame stall), then steady again.
    {
        Game off, on; on.grace = true;
        for (int i = 0; i < 50; ++i) { off.tick(30, 0); on.tick(30, 0); }
        check(!off.tick(0, 0), "NEGATIVE CONTROL: one stalled interval makes the shipped rule go single");
        check(on.tick(0, 0), "one quiet interval after observed uploads is allowed");
        check(off.tick(30, 0) && on.tick(30, 0), "both recover with the next upload");
        check(on.graceOnly == 1, "exactly one tick was the grace's");
    }
    // 4. A catch-up tick: a 1 ms game frame right after a long one, before the render thread
    //    has started the queued scene. Same shape as 3, repeated: every isolated quiet interval
    //    is forgiven, because each follows observed uploads.
    {
        Game off, on; on.grace = true;
        off.tick(30, 0); on.tick(30, 0);
        uint32_t offSingles = 0;
        for (int i = 0; i < 300; ++i) {
            const bool quiet = (i % 3) == 2;
            if (!off.tick(quiet ? 0 : 30, 0)) ++offSingles;
            check(on.tick(quiet ? 0 : 30, 0), "isolated quiet intervals stay stereo");
        }
        check(offSingles == 100, "NEGATIVE CONTROL: the shipped rule went single on all 100 catch-up ticks");
    }
    // 5. A load screen: the uploads stop for good. The lever allows exactly ONE more tick and
    //    then refuses for as long as the silence lasts; the first upload brings the scene back.
    {
        Game on; on.grace = true;
        for (int i = 0; i < 50; ++i) on.tick(30, 0);
        check(on.tick(0, 0), "load screen: the first quiet interval is allowed");
        for (int i = 0; i < 2000; ++i) check(!on.tick(0, 0), "load screen: refused from the second quiet interval on");
        check(on.tick(25, 0), "the scene is back with the first upload");
        Game off;
        for (int i = 0; i < 50; ++i) off.tick(30, 0);
        for (int i = 0; i < 2000; ++i) check(!off.tick(0, 0), "load screen: the shipped rule refuses every quiet tick");
    }
    // 6. Nothing observed yet (arming on a load screen or the main menu): no grace to give.
    {
        Game on; on.grace = true;
        for (int i = 0; i < 10; ++i) check(!on.tick(0, 0), "no uploads ever observed: the lever allows nothing");
        check(on.tick(1, 0), "the first observed upload is live");
        check(on.tick(0, 0), "and earns one quiet interval");
        check(!on.tick(0, 0), "but not two");
    }
    // 7. Two quiet intervals in a row are a refusal even in the middle of gameplay (bounded).
    {
        Game on; on.grace = true;
        for (int i = 0; i < 20; ++i) on.tick(30, 0);
        check(on.tick(0, 0) && !on.tick(0, 0) && !on.tick(0, 0), "one allowed, the second and third refused");
        check(on.tick(30, 0) && on.tick(0, 0), "after new uploads the single allowance is back");
    }
    // 8. The serial wraps. The record compares for inequality only.
    {
        Game on; on.grace = true;
        on.serial = on.lastReturn = 0xFFFFFFF0u;
        on.camera.begin(on.serial);
        for (int i = 0; i < 40; ++i) check(on.tick(3, 1), "live across the 32-bit wrap");
        check(on.serial < 0x1000u, "the serial did wrap");
        check(on.tick(0, 0), "the tick after in-draw uploads has progress since the previous entry");
        check(on.tick(0, 0), "then one quiet interval is allowed");
        check(!on.tick(0, 0), "and the next is refused");
    }
    // 9. Random schedules, 200,000 ticks: the lever only ever ADDS permission, and whatever it
    //    adds is bounded - a grace-only tick always has an upload within the last two intervals.
    {
        uint32_t seed = 20261005u;
        Game off, on; on.grace = true;
        uint32_t sinceUpload = 1000, addedByLever = 0;   // intervals (idle + draw) since the last upload
        for (int i = 0; i < 200000; ++i) {
            const uint32_t r = rng(seed);
            const bool burst = (r & 0x3FF) == 0;                 // now and then a long silence
            uint32_t idle = (r >> 10) % 4 == 0 ? 0 : (r >> 12) % 40;
            uint32_t inDraw = (r >> 18) % 3 == 0 ? (r >> 20) % 8 : 0;
            int repeat = burst ? 1 + (int)((r >> 4) % 12) : 1;
            for (int k = 0; k < repeat; ++k) {
                if (burst) { idle = 0; inDraw = 0; }
                const bool a = off.tick(idle, inDraw);
                // the lever's view of the SAME tick, before this tick's own in-draw uploads
                const uint32_t quietBefore = idle ? 0 : sinceUpload;
                const bool b = on.tick(idle, inDraw);
                check(!(a && !b), "the lever never refuses a tick the shipped rule passes");
                if (b && !a) {
                    ++addedByLever;
                    check(quietBefore <= 1, "a grace-only tick has an upload within the previous two intervals");
                }
                sinceUpload = inDraw ? 0 : (idle ? 1 : sinceUpload + 1);
            }
        }
        check(addedByLever > 1000, "the random schedules exercised the lever (not an empty pass)");
        check(off.live < on.live, "and it kept more ticks stereo than the shipped rule");
    }
    printf("camera silent: %d checks PASS\n", g_checks);
    return 0;
}
