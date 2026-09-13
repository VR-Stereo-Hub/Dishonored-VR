// Host tests for the VR-93 menu lifecycle (src/game/dishonored/hands/menu_keep.h),
// compiled against the production header by tools/menu-keep-host.ps1.
//
// The machine is driven through the state sequences the game actually produces
// (a pause reads MENU, then LOADING on resume, then GAMEPLAY) and through the
// ones that must still destroy identity: a real load, a lost pawn, a different
// pawn, a failed validation. The validation is driven with a fake object table
// that frees and reuses addresses on purpose.
#include "game/dishonored/hands/menu_keep.h"

#include <stdio.h>
#include <string.h>

using namespace dvr::menukeep;

static int g_fail = 0, g_checks = 0;
static void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) { ++g_fail; printf("FAIL: %s\n", what); }
}

enum St { GAMEPLAY, MENU, LOADING, CINEMATIC, NO_PAWN };

static Frame F(St s, bool lever = true, bool pawnLive = true, bool pawnChanged = false,
               bool loadAsked = false) {
    Frame f;
    f.leverOn = lever;
    f.gameplay = s == GAMEPLAY;
    f.menu = s == MENU;
    f.noPawn = s == NO_PAWN;
    f.pawnLive = s != NO_PAWN && pawnLive;
    f.pawnChanged = pawnChanged;
    f.loadAsked = loadAsked;
    return f;
}

static Machine InGameplay(bool lever = true) {
    Machine m;
    m.tick(F(GAMEPLAY, lever));
    return m;
}

// ---- a fake object table ----------------------------------------------------
struct FakeObj { void* addr; bool live; bool readable; void* cls; uint32_t name[2]; };
struct Table { FakeObj objs[8]; int n = 0; };

static FakeObj* Find(Table* t, void* a) {
    for (int i = 0; i < t->n; ++i) if (t->objs[i].addr == a) return &t->objs[i];
    return nullptr;
}
static bool Live(void* ctx, void* obj) { FakeObj* o = Find((Table*)ctx, obj); return o && o->live; }
static bool Read(void* ctx, void* obj, Identity* out) {
    FakeObj* o = Find((Table*)ctx, obj);
    if (!o || !o->readable) return false;
    out->obj = obj; out->cls = o->cls; out->name[0] = o->name[0]; out->name[1] = o->name[1];
    return true;
}
static Identity Id(void* a, void* cls, uint32_t n0, uint32_t n1) {
    Identity i; i.obj = a; i.cls = cls; i.name[0] = n0; i.name[1] = n1; return i;
}

int main() {
    // ---- the machine ---------------------------------------------------------

    {   // NEGATIVE CONTROL: lever off is the pre-VR-93 transition. A pause
        // invalidates, and so the retention case below FAILS with the lever off
        // for exactly the reason the ticket describes.
        Machine m = InGameplay(false);
        Step s = m.tick(F(MENU, false));
        check(s.action == Action::Invalidate, "lever off: a pause invalidates, as before VR-93");
        check(m.phase == Phase::Active, "lever off: nothing is suspended");
        s = m.tick(F(LOADING, false)); s = m.tick(F(GAMEPLAY, false));
        check(s.action == Action::None, "lever off: the resume asks for no validation");
    }
    {   // the ordinary pause, with the benign LOADING a resume reads
        Machine m = InGameplay();
        Step s = m.tick(F(MENU));
        check(s.action == Action::Suspend, "pause over a live pawn suspends");
        check(m.tick(F(MENU)).action == Action::None, "a held menu does nothing");
        check(m.tick(F(LOADING)).action == Action::None, "the resume's LOADING does not end the suspension");
        check(m.phase == Phase::Suspended, "still suspended through LOADING");
        s = m.tick(F(GAMEPLAY));
        check(s.action == Action::Validate, "back to GAMEPLAY asks for validation");
        check(m.phase == Phase::Validating, "validating");
        check(m.tick(F(GAMEPLAY)).action == Action::None, "no second validation request while pending");
        s = m.verdict(true, true);
        check(s.action == Action::None && m.phase == Phase::Active, "a pass keeps everything and returns to Active");
    }
    {   // a real load straight out of gameplay
        Machine m = InGameplay();
        check(m.tick(F(LOADING)).action == Action::Invalidate, "GAMEPLAY -> LOADING invalidates even with the lever on");
    }
    {   // the book: the view goes silent and the state reads LOADING, with the
        // note screen observed open
        Frame book = F(LOADING); book.screen = true;
        Machine m = InGameplay();
        Step s = m.tick(book);
        check(s.action == Action::Suspend && strstr(s.reason, "note screen"),
              "GAMEPLAY -> LOADING with the note screen open suspends");
        check(m.tick(book).action == Action::None, "a held book does nothing");
        check(m.tick(F(GAMEPLAY)).action == Action::Validate, "closing the book asks for validation");
        check(m.verdict(true, true).action == Action::None && m.phase == Phase::Active, "and a pass keeps it");
        Machine n = InGameplay();
        n.tick(F(MENU)); n.tick(F(GAMEPLAY));
        check(n.tick(book).action == Action::None && n.phase == Phase::Suspended,
              "Validating -> a book goes back to Suspended");
        Frame noLever = book; noLever.leverOn = false;
        Machine o = InGameplay(false);
        check(o.tick(noLever).action == Action::Invalidate, "NEGATIVE CONTROL: lever off, a book drops as before");
    }
    {
        Machine m = InGameplay();
        check(m.tick(F(CINEMATIC)).action == Action::Invalidate, "GAMEPLAY -> CINEMATIC invalidates");
    }
    {
        Machine m = InGameplay();
        check(m.tick(F(NO_PAWN)).action == Action::Invalidate, "GAMEPLAY -> NO_PAWN invalidates");
    }
    {
        Machine m = InGameplay();
        check(m.tick(F(MENU, true, false)).action == Action::Invalidate, "a menu with no live pawn invalidates");
    }
    {   // a save loaded from the pause menu: the pawn goes away
        Machine m = InGameplay();
        m.tick(F(MENU)); m.tick(F(LOADING));
        Step s = m.tick(F(NO_PAWN));
        check(s.action == Action::Invalidate && m.phase == Phase::Active, "NO_PAWN during the menu invalidates at once");
        check(m.tick(F(GAMEPLAY)).action == Action::None, "and the next GAMEPLAY validates nothing - there is nothing retained");
    }
    {   // a load fast enough never to read NO_PAWN, but a new pawn is latched
        Machine m = InGameplay();
        m.tick(F(MENU)); m.tick(F(LOADING));
        Step s = m.tick(F(LOADING, true, true, true));
        check(s.action == Action::Invalidate, "a different pawn during the menu invalidates");
    }
    {   // a load that reuses the pawn's address AND keeps its FName (the FName
        // part is measured): only the game's load event can catch it
        Machine m = InGameplay();
        m.tick(F(MENU));
        Step s = m.tick(F(MENU, true, true, false, true));
        check(s.action == Action::Invalidate && strstr(s.reason, "load-game"),
              "the load-game event during the menu invalidates");
        Machine n = InGameplay();
        n.tick(F(MENU)); n.tick(F(LOADING));
        check(n.tick(F(GAMEPLAY, true, true, false, true)).action == Action::Invalidate,
              "and it wins over the resume's validation request");
    }
    {
        Machine m = InGameplay();
        m.tick(F(MENU));
        check(m.tick(F(MENU, false)).action == Action::Invalidate, "switching the lever off mid-menu invalidates");
    }
    {
        Machine m = InGameplay();
        m.tick(F(MENU)); m.tick(F(GAMEPLAY));
        Step s = m.verdict(false, true);
        check(s.action == Action::Invalidate && m.phase == Phase::Active, "a failed validation invalidates");
    }
    {   // the menu reopens before the verdict arrives
        Machine m = InGameplay();
        m.tick(F(MENU)); m.tick(F(GAMEPLAY));
        check(m.tick(F(MENU)).action == Action::None && m.phase == Phase::Suspended,
              "Validating -> MENU goes back to Suspended without dropping");
        check(m.verdict(true, false).action == Action::None && m.phase == Phase::Suspended,
              "a pass that lands inside the menu stays Suspended");
        check(m.tick(F(GAMEPLAY)).action == Action::Validate, "and the next resume validates again");
    }
    {
        Machine m = InGameplay();
        m.tick(F(MENU)); m.tick(F(GAMEPLAY));
        check(m.tick(F(LOADING)).action == Action::Invalidate, "Validating -> LOADING invalidates");
    }
    {   // a tripwire beats a verdict that was already on its way
        Machine m = InGameplay();
        m.tick(F(MENU)); m.tick(F(NO_PAWN));
        check(m.verdict(true, false).action == Action::None && m.phase == Phase::Active,
              "a stale pass after a tripwire resurrects nothing");
    }

    // ---- the validation --------------------------------------------------------
    int a1, a2, a3, c1, c2;   // addresses and classes
    void* pawn = &a1; void* comp = &a2; void* comp2 = &a3;
    void* clsPawn = &c1; void* clsComp = &c2;
    Table t;
    t.objs[t.n++] = {pawn, true, true, clsPawn, {100, 0}};
    t.objs[t.n++] = {comp, true, true, clsComp, {200, 3}};
    t.objs[t.n++] = {comp2, true, true, clsComp, {200, 4}};
    Reader r; r.ctx = &t; r.live = Live; r.read = Read;
    const Identity pawnId = Id(pawn, clsPawn, 100, 0);
    const Identity compId = Id(comp, clsComp, 200, 3);
    const Identity comp2Id = Id(comp2, clsComp, 200, 4);

    {
        Verdict v;
        dvr::menukeep::check(r, pawnId, "pawn", true, &v);
        dvr::menukeep::check(r, compId, "comp", false, &v);
        dvr::menukeep::check(r, comp2Id, "comp2", false, &v);
        check(v.keep && v.checked == 3 && v.failed == 0, "same-world pause: every retained object passes");
    }
    {   // freed
        t.objs[1].live = false;
        Verdict v;
        dvr::menukeep::check(r, compId, "comp", false, &v);
        check(!v.keep && strstr(v.why, "not in the live-object table"), "a freed component fails as not live");
        t.objs[1].live = true;
    }
    {   // RECYCLED ADDRESS, same class, new FName number - the case a pointer
        // test and a class-name test both pass. NEGATIVE CONTROL for that
        // weaker rule: live and same class, so it would have been kept.
        t.objs[1].name[1] = 7;
        Verdict v;
        dvr::menukeep::check(r, compId, "comp", false, &v);
        const bool pointerAndClassWouldKeep = Live(&t, comp) && t.objs[1].cls == compId.cls;
        check(pointerAndClassWouldKeep, "control: a pointer + class test would keep the reused address");
        check(!v.keep && strstr(v.why, "FName changed"), "a reused address with a new FName number fails");
        t.objs[1].name[1] = 3;
    }
    {   // recycled address, different class
        int c3; t.objs[1].cls = &c3;
        Verdict v;
        dvr::menukeep::check(r, compId, "comp", false, &v);
        check(!v.keep && strstr(v.why, "class changed"), "a reused address with another class fails");
        t.objs[1].cls = clsComp;
    }
    {
        Verdict v;
        dvr::menukeep::check(r, Identity{}, "pawn", true, &v);
        check(!v.keep && strstr(v.why, "nothing was recorded"), "a required pawn with no record fails");
        Verdict w;
        dvr::menukeep::check(r, Identity{}, "comp", false, &w);
        check(w.keep && w.checked == 0, "an optional empty record retained nothing and passes");
    }
    {
        t.objs[2].readable = false;
        Verdict v;
        dvr::menukeep::check(r, comp2Id, "comp2", false, &v);
        check(!v.keep && strstr(v.why, "unreadable"), "a live but unreadable object fails");
        t.objs[2].readable = true;
    }
    {   // a partial snapshot: two failures, the FIRST reason is the one reported
        t.objs[1].live = false; t.objs[2].name[0] = 999;
        Verdict v;
        dvr::menukeep::check(r, pawnId, "pawn", true, &v);
        dvr::menukeep::check(r, compId, "comp", false, &v);
        dvr::menukeep::check(r, comp2Id, "comp2", false, &v);
        check(!v.keep && v.failed == 2 && v.checked == 3, "two of three fail and both are counted");
        check(strstr(v.why, "comp ") && strstr(v.why, "not in the live-object table"), "the first failure names itself");
        t.objs[1].live = true; t.objs[2].name[0] = 200;
    }

    if (g_fail) {
        printf("menu-keep host: %d of %d checks FAILED\n", g_fail, g_checks);
        return 1;
    }
    printf("menu-keep host: all %d checks passed\n", g_checks);
    return 0;
}
