// game/dishonored/hands/menu_keep.h - VR-93: keep weapon identity across a menu.
//
// QUESTION: when a menu opens over a live pawn and closes again, may the weapon
// contracts and the candidate list learned before it be used after it?
//
// Before VR-93 every exit from GAMEPLAY was processed as a level load: the
// contracts, the candidate list and the UI observer were all thrown away, so an
// ordinary pause relearned the weapons on every resume. A load DOES destroy
// what they point at, so the answer cannot be "keep everything on a menu": a
// save can be loaded from the pause menu, and a resume can briefly read LOADING
// without any load at all.
//
// So a menu SUSPENDS instead of invalidating, and the first script tick that
// could use the retained records VALIDATES them first:
//
//   present lane  Machine::tick once per frame. Leaving GAMEPLAY for a MENU over
//                 a live pawn suspends; anything else invalidates, exactly as
//                 before. While suspended, NO_PAWN or a different pawn
//                 invalidates at once. Returning to GAMEPLAY asks for validation.
//   script lane   validate() against a live-object table rebuilt for the
//                 purpose, BEFORE the candidate rebuild and the component
//                 snapshot. Every retained object must still be live and still
//                 carry the class and FName it had when it was recorded.
//   present lane  Machine::verdict consumes the answer. A failure invalidates.
//
// A retained record is identity only. Every correction still needs a component
// snapshot and a hand correction from the present that draws it, so nothing
// measured before the menu is ever consumed after it.
//
// Ships OFF: [Hands] AttachKeepOnMenu=0 is the pre-VR-93 behaviour exactly.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace dvr::menukeep {

// What an engine object WAS when it was recorded. A pointer alone is not
// identity: freed memory is reused. The class and the FName (index and number)
// are read from the object itself.
struct Identity {
    void*    obj     = nullptr;
    void*    cls     = nullptr;
    uint32_t name[2] = {0, 0};
};

enum class Phase  : uint8_t { Active, Suspended, Validating };
enum class Action : uint8_t { None, Invalidate, Suspend, Validate };

struct Step {
    Action      action = Action::None;
    const char* reason = "";
};

// The game state the present lane decided this frame, reduced to what the
// lifecycle needs.
struct Frame {
    bool leverOn     = false;   // [Hands] AttachKeepOnMenu, read live
    bool gameplay    = false;
    bool menu        = false;
    // A same-world screen that the state machine does NOT call a menu: the
    // book/note screen silences the view and reads LOADING. Set only while that
    // screen is observed open AND [Hands] AttachKeepOnNote=1.
    bool screen      = false;
    bool noPawn      = false;
    bool pawnLive    = false;   // a pawn is latched and the cylinder reads
    bool pawnChanged = false;   // the latched pawn is not the recorded one
    bool loadAsked   = false;   // the game's load-game event fired during the menu
};

struct Machine {
    Phase phase       = Phase::Active;
    bool  wasGameplay = false;

    Step tick(const Frame& f)
    {
        Step s;
        const bool left = wasGameplay && !f.gameplay;
        const bool back = !wasGameplay && f.gameplay;
        wasGameplay = f.gameplay;
        const bool menuLike = f.menu || f.screen;
        if (phase == Phase::Active) {
            if (!left) return s;
            if (f.leverOn && menuLike && f.pawnLive) {
                phase = Phase::Suspended;
                s.action = Action::Suspend;
                s.reason = f.menu ? "a menu opened over a live pawn"
                                  : "the note screen opened over a live pawn";
            } else {
                s.action = Action::Invalidate;
                s.reason = !f.leverOn ? "the game left gameplay (AttachKeepOnMenu=0)"
                         : !menuLike  ? "the game left gameplay for something that is not a menu"
                                      : "a menu opened with no live pawn";
            }
            return s;
        }
        // Suspended or Validating: the tripwires come first, every frame.
        const char* trip = !f.leverOn    ? "AttachKeepOnMenu was switched off during the menu"
                         : f.noPawn      ? "the pawn went away during the menu (NO_PAWN)"
                         : f.pawnChanged ? "a different pawn was latched during the menu"
                         // A respawned pawn keeps its FName (measured: 10783_0 before
                         // and after a save load), so a load that reused the address
                         // would pass the identity check. The game's own event is
                         // the one signal that does not depend on an address. It
                         // fires when the save browser OPENS, so backing out of it
                         // also drops - the safe direction.
                         : f.loadAsked   ? "the load-game screen was opened during the menu"
                                         : nullptr;
        if (trip) {
            phase = Phase::Active;
            s.action = Action::Invalidate;
            s.reason = trip;
            return s;
        }
        if (back) {
            phase = Phase::Validating;
            s.action = Action::Validate;
            s.reason = "gameplay resumed";
            return s;
        }
        // Validating and the game leaves gameplay again before the verdict: a
        // menu goes back to waiting, anything else is today's invalidation. A
        // LOADING or CINEMATIC while SUSPENDED is not an exit from gameplay and
        // changes nothing - the benign LOADING a resume reads is exactly that.
        if (phase == Phase::Validating && left) {
            if (menuLike) { phase = Phase::Suspended; return s; }
            phase = Phase::Active;
            s.action = Action::Invalidate;
            s.reason = "the game left gameplay for something that is not a menu before validation";
        }
        return s;
    }

    // The script lane's answer. Only meaningful while a validation is pending
    // or the menu that asked for it is still open.
    Step verdict(bool keep, bool gameplayNow)
    {
        Step s;
        if (phase == Phase::Active) return s;   // superseded by a tripwire
        if (!keep) {
            phase = Phase::Active;
            s.action = Action::Invalidate;
            s.reason = "the retained records failed validation";
            return s;
        }
        phase = gameplayNow ? Phase::Active : Phase::Suspended;
        return s;
    }
};

// ---- validation, with the engine reads injected so a host can drive it -----

struct Reader {
    void* ctx = nullptr;
    bool (*live)(void* ctx, void* obj) = nullptr;           // IsLiveObject
    bool (*read)(void* ctx, void* obj, Identity* out) = nullptr;
};

struct Verdict {
    bool keep    = true;
    int  checked = 0;
    int  failed  = 0;
    char why[200] = {0};
};

// One retained object. A null record retained nothing and passes; `required`
// makes a null a failure (the pawn).
inline void check(const Reader& r, const Identity& was, const char* what,
                  bool required, Verdict* v)
{
    if (!was.obj) {
        if (required) {
            v->keep = false; ++v->failed;
            if (!v->why[0]) snprintf(v->why, sizeof(v->why), "%s: nothing was recorded", what);
        }
        return;
    }
    ++v->checked;
    Identity now;
    const char* fail = nullptr;
    if (!r.live || !r.live(r.ctx, was.obj))            fail = "not in the live-object table";
    else if (!r.read || !r.read(r.ctx, was.obj, &now)) fail = "unreadable";
    else if (now.cls != was.cls)                       fail = "its class changed - the address was reused";
    else if (now.name[0] != was.name[0] || now.name[1] != was.name[1])
                                                       fail = "its FName changed - the address was reused";
    if (!fail) return;
    v->keep = false; ++v->failed;
    if (!v->why[0])
        snprintf(v->why, sizeof(v->why), "%s %p %s (FName %u_%u -> %u_%u)", what, was.obj, fail,
                 was.name[0], was.name[1], now.name[0], now.name[1]);
}

}  // namespace dvr::menukeep
