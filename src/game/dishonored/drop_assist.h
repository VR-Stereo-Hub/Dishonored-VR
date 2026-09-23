// game/dishonored/drop_assist.h - drop takedowns from above (the aerial stealth
// kill). The game decides per tick whether a drop kill is available; an attack
// pressed while that decision is still "no target" becomes an ordinary slash and
// the kill is lost. The assist holds such an attack for a moment while airborne
// and lets it through the tick the game finds a target. Implemented in
// drop_assist.cpp; the engine rule it works around is in ENGINE_NOTES
// "Drop takedown timing (VR-203)".
//
// Lanes: sample() runs on the script lane (from the anim tick); gate() runs on the
// present lane where the virtual pad is composed. They share one published record.
#pragma once
#include <cstdint>
namespace dvr { namespace status { class Writer; } }
namespace dvr::anim { struct Snapshot; }
namespace dvr::drop {
enum Gate { Pass = 0, Hold = 1, Press = 2 };
void sample(uint8_t* pawn, const dvr::anim::Snapshot& s);   // script lane, every anim tick
// Present lane: `attack` is the attack input as composed so far (trigger or swing
// pulse). Hold = withhold it this build, Press = press it, Pass = leave it alone.
Gate gate(bool attack, bool swingPulse, long padPolls);
void configure(const char* ini);
void save(const char* ini);
bool command(const char* args);   // the `drop` word
void status(dvr::status::Writer& w);
void draw_ui();                   // ImGui: only from the overlay's draw callback
}
