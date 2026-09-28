// game/dishonored/snap_turn.h - snap turn (VR-219): a push of the right stick turns the
// player by a fixed step instead of smoothly. The step is written where the smooth turn
// already lands, in the view rotation the head writer hands the engine, and it is booked
// as BODY yaw, so the pawn, the hands, the aim ray and the HUD anchors turn with the view
// exactly as they do for the game's own stick turn. Implemented in snap_turn.cpp; the yaw
// bookkeeping it rides is ENGINE_NOTES "VR-30: body yaw vs head yaw" and yaw_book.h.
//
// Lanes: present_tick() runs on the present lane where the virtual pad is composed and
// detects the stick edge; take_pending() and the note_* calls run on the script lane
// inside the head writer's fresh branch. They share one atomic pending step.
#pragma once
#include <cstdint>
namespace dvr { namespace status { class Writer; } }
namespace dvr::snap {
bool enabled();
void set_enabled(bool on, const char* who);   // the live A/B
// PRESENT lane, once per UpdateVirtualPad, after every block that takes the right stick for
// navigation has zeroed it. `rxWouldTurn` = the composed RX is nonzero. Returns true when RX
// must be zeroed before it reaches the game (the step replaces the smooth turn).
bool present_tick(float rawX, bool rxWouldTurn, bool controllersActive, double nowMs);
// SCRIPT lane, the head writer's FRESH branch only. Returns the queued yaw (UE units) once;
// 0 when nothing is queued. Never call it from a replay or a re-stamp.
int32_t take_pending(int32_t viewInU, int32_t headDeltaU);
void note_written(int32_t viewOutU, int32_t bodyTargetU);          // after rot[1] is final
void note_incoming(int32_t incomingU, bool havePrevWrite, int32_t prevWriteU);   // the honour check
void drop_pending(const char* why);                                // the writer's early-outs
void fallback_owns_camera();                                       // RotInjectTick: inert here
void configure(const char* ini);
void save(const char* ini);
bool command(const char* args);   // the `snapturn` word
void status(dvr::status::Writer& w);
void draw_ui();                   // ImGui: only from the overlay's draw callback
}
