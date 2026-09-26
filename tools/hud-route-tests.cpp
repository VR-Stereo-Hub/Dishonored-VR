// tools/hud-route-tests.cpp - the HUD element routing, on the host (VR-120).
// Build and run: tools\hud-route-host.ps1. Pure: hud_route.h, hud_group.h (VR-186), hud_native_rune.h (VR-185).
#include "core/gfx/hud_route.h"
#include "core/gfx/hud_group.h"
#include "core/gfx/hud_native_rune.h"
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

    // A moving draw must not acquire a different owner at a region boundary.
    dvr::hudroute::StableRoutes stable;
    check(stable.resolve(123,1,Prompt)==Prompt,"initial prompt association");
    for(unsigned frame=2;frame<250;++frame)
        check(stable.resolve(123,frame,frame%2?Default:Reticle)==Prompt,"moving prompt retains owner");
    check(stable.resolve(456,250,Default)==Default,"unknown draw starts on default");
    check(stable.resolve(456,251,Prompt)==Default,"unknown draw crossing prompt remains default");
    check(stable.resolve(0,251,Reticle)==Reticle,"unreadable content keeps spatial fallback");
    check(stable.resolve(123,600,Vitals)==Vitals,"stale association expires");
    stable.clear();
    check(stable.resolve(123,601,Default)==Default,"configuration/menu reset clears association");
    check(stable.resolve(123+2048,602,Prompt)==Prompt,"bounded cache collision replaces identity");
    check(stable.resolve(123,603,Reticle)==Reticle,"collision never borrows another key owner");
    const float atPrompt[4]={.55f,.50f,.60f,.55f},atVitals[4]={.05f,.05f,.10f,.10f};
    check(stable.resolve(777,610,Prompt,atPrompt)==Prompt,"distinct sprite initially associated");
    check(stable.resolve(777,610,Vitals,atVitals)==Vitals,"same content at two positions is ambiguous");
    check(stable.resolve(777,611,Default,atPrompt)==Default,"ambiguous key falls back instead of borrowing another owner");
    check(stable.resolve(777,615,Prompt,atPrompt)==Prompt,"transient stereo/stance ambiguity expires");
    {
        using dvr::hudroute::objective_shape;
        for(int step=0;step<80;++step) {
            const float x=step*.012f,y=.10f+step*.005f;
            const float r[4]={x,y,x+.033f,y+.032f};
            check(objective_shape(r,4,2),"measured marker shape is independent of screen position");
        }
        const float grown[4]={.480f,.481f,.520f,.519f},glyph[4]={.54f,.5f,.55f,.52f};
        const float marker[4]={.3f,.3f,.333f,.332f},full[4]={0,0,1,1};
        check(!objective_shape(grown,4,2),"grown reticle is not objective");
        check(!objective_shape(glyph,4,2),"ordinary text glyph is not objective");
        check(!objective_shape(marker,24,12),"text batch is not a marker quad");
        check(!objective_shape(full,4,2),"full-screen fill is not objective");
        dvr::hudroute::InteractionGroup group;
        const float unrelated[4]={.85f,.9f,.95f,.94f};
        for(unsigned frame=1;frame<=30;++frame) {
            const float x=.53f-(frame-1)*.008f;
            const float title[4]={x,.445f,x+.18f,.470f};
            const float action[4]={x+.01f,.485f,x+.14f,.515f};
            if(frame==1) check(group.claim(action,frame,true),"seed prompt action");
            check(group.claim(title,frame,false),"title joins action across original region boundary");
            check(group.claim(action,frame,false),"action remains with moving title");
            check(!group.claim(unrelated,frame,false),"unrelated distant text does not join interaction");
        }
        const float old[4]={.30f,.45f,.40f,.48f};
        check(!group.claim(old,40,false),"stale interaction neighborhood expires");
        group.clear();check(!group.claim(old,41,false),"menu/device reset forgets interaction");
        check(!group.claim(full,41,true),"large fill cannot seed an interaction group");
    }
    {
        dvr::hudroute::StableRoutes retained;dvr::hudroute::InteractionGroup group;
        const float title[4]={.54f,.49f,.68f,.52f},crouched[4]={.25f,.29f,.39f,.32f};
        check(retained.resolve(99,1,Default,title)==Default,"new content initially defaults");
        retained.adopt(99,1,Prompt);
        check(retained.resolve(99,2,Default,crouched)==Prompt,"group ownership survives crouch-sized movement");
        check(group.claim(crouched,2,true),"retained owner reseeds moved neighborhood");
        const float button[4]={.26f,.335f,.30f,.373f};
        check(group.claim(button,2,false),"button joins moved title");
        const float centralButton[4]={.480f,.481f,.520f,.519f};
        check(!dvr::hudroute::centered_reticle(centralButton,10),"observed 10-primitive button is not the reticle");
        check(dvr::hudroute::centered_reticle(centralButton,2),"measured centered reticle remains protected");
        retained.resolve(99,2,Default,button);retained.adopt(99,2,Prompt);
        check(retained.resolve(99,3,Default,title)==Default,"ambiguous shared sprites cannot be adopted");
    }
    // ---- VR-186: widget groups from back-to-back touching draws -----------
    {
        using namespace dvr::hudgroup;
        Builder g;
        const float plate[4]={.40f,.80f,.44f,.84f}, glyph[4]={.405f,.805f,.435f,.835f};
        const float far[4]={.10f,.50f,.12f,.52f};
        g.begin(1); g.add(plate,Default,kDefault); g.add(glyph,Prompt,kRow);
        g.begin(2);
        check(g.prevN==1 && g.prev[0].members==2 && g.prev[0].owner==Prompt,"plate and glyph are one widget owned by the glyph");
        check(g.lookup(plate,kDefault)==Prompt,"the plate takes its widget owner (the fault: plate on another layer)");
        check(g.lookup(plate,kWeak)==Prompt,"an isolated-icon plate takes its widget owner too");
        check(g.lookup(glyph,kRow)==-1,"a piece with its own row is never lifted");
        check(g.lookup(far,kDefault)==-1,"a draw outside every widget keeps its own route");
        // Draw order matters: the plate drawn FIRST still gets the owner (next present).
        g.add(glyph,Prompt,kRow); g.add(plate,Default,kDefault);
        g.begin(3);
        check(g.lookup(plate,kDefault)==Prompt,"owner drawn before the plate groups the same way");
        // Not touching, or cut by a native draw: separate widgets.
        g.add(plate,Default,kDefault); g.add(far,Prompt,kRow);
        g.begin(4);
        check(g.lookup(plate,kDefault)==-1,"consecutive but distant draws are not one widget");
        g.add(plate,Default,kDefault); g.cut(); g.add(glyph,Prompt,kRow);
        g.begin(5);
        check(g.lookup(plate,kDefault)==-1,"a cut (native marker, vignette) ends the run");
        // Two differently identified owners never merge, even touching.
        const float bar[4]={.05f,.20f,.15f,.25f}, sneakIcon[4]={.14f,.24f,.17f,.27f}, sneakBack[4]={.139f,.239f,.171f,.271f};
        g.add(bar,Vitals,kRow); g.add(sneakIcon,Prompt,kRow); g.add(sneakBack,Default,kDefault);
        g.begin(6);
        check(g.prevN==1 && g.prev[0].owner==Prompt,"a second row starts a new widget");
        check(g.lookup(sneakBack,kDefault)==Prompt,"the background joins the icon it touches, not the neighbour row");
        // The owner is the STRONGEST piece: the interaction group outranks a row.
        const float title[4]={.52f,.48f,.70f,.52f};
        g.add(title,Prompt,kInteraction); g.add(glyph,Default,kDefault);
        g.begin(7);
        check(g.lookup(glyph,kDefault)==-1,"distant default after an interaction title is its own widget");
        // A widget owned only by `default` lifts an isolated icon onto default.
        g.add(plate,Default,kDefault); g.add(glyph,Default,kWeak);
        g.begin(8);
        check(g.lookup(glyph,kWeak)==Default,"a guessed-marker icon in a default widget rides default, not the image");
        check(g.lookup(plate,kDefault)==-1,"default does not lift default");
        // Groups age out after three presents without HUD draws.
        g.begin(12);
        check(g.lookup(glyph,kWeak)==-1,"groups older than three presents are ignored");
        // A full-screen draw is not a widget piece.
        const float screen[4]={0,0,1,1};
        g.add(screen,Default,kDefault); g.add(plate,Default,kDefault);
        g.begin(13);
        check(g.prevN==0,"a full-screen draw joins nothing");
        // Size cap: a run cannot grow past 0.6 x 0.4 of the screen.
        const float a1[4]={.0f,.0f,.3f,.1f}, a2[4]={.3f,.0f,.61f,.1f};
        g.add(a1,Prompt,kRow); g.add(a2,Default,kDefault);
        g.begin(14);
        check(g.lookup(a2,kDefault)==-1,"a run past the size cap splits");
    }
    // ---- VR-185: task markers claimed by their published position --------
    {
        dvr::hudnative::TaskPositions t;
        const float W=1920,H=1080;          // 16:9: 1 authoring px = 1/1280 of the width
        t.update(1,640,360,1280,720,1,1000);
        float pivot[4]{},off[2]{};
        const float icon[4]={.5f-20/1280.f,.5f-20/720.f,.5f+20/1280.f,.5f+20/720.f};
        check(t.match(icon,1010,W,H,pivot,off)==dvr::hudnative::TaskPositions::kIcon,"the icon at the published point is the marker's");
        check(pivot[0]==.5f && pivot[1]==.5f,"the pivot is the published point");
        const float title[4]={.5f-85/1280.f,.5f-80/720.f,.5f+85/1280.f,.5f-40/720.f};
        check(t.match(title,1010,W,H,pivot,off)==dvr::hudnative::TaskPositions::kText,"the title above it is the marker's");
        check(off[1]<-50 && off[1]>-70,"the offset is reported in authoring px");
        const float dist[4]={.5f-30/1280.f,.5f+30/720.f,.5f+30/1280.f,.5f+54/720.f};
        check(t.match(dist,1010,W,H,pivot)==dvr::hudnative::TaskPositions::kText,"the distance below it is the marker's");
        const float prompt[4]={.5f+150/1280.f,.5f-20/720.f,.5f+350/1280.f,.5f+20/720.f};
        check(!t.match(prompt,1010,W,H,pivot),"an interaction title right of the reticle is not claimed");
        check(!t.match(icon,1200,W,H,pivot),"a sample older than 100 ms claims nothing");
        t.update(1,640,360,1280,720,0,1300);
        check(!t.match(icon,1310,W,H,pivot),"a hidden marker withdraws its point");
        // Load while looking at the marker: nothing was ever seen at an edge,
        // and the first draw is still claimed (the VR-185 trigger).
        dvr::hudnative::TaskPositions fresh;
        fresh.update(7,640,360,1280,720,1,5000);
        check(fresh.match(icon,5001,W,H,pivot)==dvr::hudnative::TaskPositions::kIcon,"a marker in view on its first present is claimed");
        // Two markers equally close cannot both own a draw.
        dvr::hudnative::TaskPositions two;
        two.update(1,620,360,1280,720,1,1000); two.update(2,660,360,1280,720,1,1000);
        check(!two.match(icon,1010,W,H,pivot),"an equidistant draw between two markers is refused");
        check(two.ambiguous==1,"and counted as ambiguous");
        // A square target (the eye texture) letterboxes the canvas.
        dvr::hudnative::TaskPositions sq; sq.update(1,1280*.25f,360,1280,720,1,1000);
        const float sqIcon[4]={.25f-20/1280.f,.5f-20/1280.f,.25f+20/1280.f,.5f+20/1280.f};
        check(sq.match(sqIcon,1001,2000,2000,pivot)==dvr::hudnative::TaskPositions::kIcon,"the fit is honoured on a square target");
    }
    // VR-186: replay the recorded prompt/task-text ownership transition.
    {
        dvr::hudroute::StableRoutes cache;
        dvr::hudnative::TaskPositions task;
        const uint64_t key=0xc07faf8d815a2b72ull;
        const float before[4]={.538f,.470f,.597f,.494f};
        const float overlap[4]={.538f,.493f,.597f,.516f};
        task.update(1,720,390,1280,720,1,1000);
        float matchedPivot[4]{};
        const int kind=task.match(overlap,1010,1920,1080,matchedPivot);
        check(kind==2,"negative control: recorded prompt rectangle is claimed by broad task text window");
        cache.resolve(key,1,Prompt,before);
        check(!cache.prefer_interaction(key,1,kind),"a row hint alone cannot override a native text candidate");
        cache.adopt(key,1,Prompt,true);
        cache.resolve(key,2,Default,overlap);
        check(cache.prefer_interaction(key,2,kind),"observed prompt stays on its panel while crossing task text window");
        check(!cache.prefer_interaction(key,2,1) && !cache.prefer_interaction(key,2,3),"native task icons and continuity always win");
        cache.resolve(key,4,Default,overlap);
        check(!cache.prefer_interaction(key,4,kind),"unrefreshed observation expires after two presents");
        cache.adopt(key,4,Prompt,true);
        const float elsewhere[4]={.1f,.1f,.2f,.12f};
        cache.resolve(key,4,Default,elsewhere);
        check(!cache.prefer_interaction(key,4,kind),"same-content draws in different places refuse identity");
        cache.resolve(key,7,Default,overlap);
        check(!cache.prefer_interaction(key,7,kind),"clearing ambiguity does not resurrect old interaction evidence");
        cache.adopt(key,7,Prompt,true);
        cache.clear();cache.resolve(key,8,Prompt,overlap);
        check(!cache.prefer_interaction(key,8,kind),"load or menu reset discards observed ownership");
        cache.adopt(key,8,Prompt,true);
        cache.resolve(key+2048,8,Default,elsewhere);
        check(!cache.prefer_interaction(key,8,kind),"cache collisions discard old ownership");
        check(!cache.prefer_interaction(0,8,kind),"missing draw key never overrides native text");
    }
    std::printf("%u hud-route checks passed\n", checks);
    return 0;
}
