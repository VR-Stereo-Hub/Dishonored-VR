// game/dishonored/z_account.h - the VR-78 vertical accounting probe.
//
// QUESTION: when the head pitches, what moves the rendered camera beyond the
// tracked head's own translation, and which owner put it there?
//
// Four owners touch the camera's height every tick (docs/dishonored/VR-78-PLAN.md
// section 2): the engine (its eye and its own neck arc), the 38.24 eye clamp in
// FovLeverApply, the seam writer's offset (eye, raw head, neck term), and the
// writer's final ceiling cap. They run on two lanes, and a present is not a
// camera write: the script lane calls the writer on every dispatch, the second
// eye is written again inside the re-entered draw, and the render thread draws
// a tick later. A pile of latest-value floats cannot close an equation across
// that, so this probe carries RECORDS:
//
//   Head   PRESENT thread. The raw displacement, the neck term and the triple
//          handed to the seam, published as one unit under a lock where they
//          are composed.
//   Clamp  SCRIPT lane. What FovLeverApply read and wrote on each of its four
//          fields this dispatch, with the pawn and both ceilings.
//   Write  SCRIPT lane. One per writer call: the head snapshot it consumed (and
//          whether that snapshot is the triple it actually used), the clamp it
//          ran after, the field before the write, the base the writer
//          recovered and whether the field was still our own write, each term
//          in world axes, the cap and the written position.
//
// A Write is PINNED when scene_draw pushes that draw's eye tag, and its id rides
// the tag through the reentry ring. The present that pops the tag hands back the
// id with its own c5 and the eye the pairing finally chose, so the analysis only
// ever joins a render sample to the write that produced it. Anything that
// cannot be joined is counted as UNMATCHED by reason, never filled in.
//
// Ships OFF: [PosTrack] ZAccount=0, `camera zaccount on|off|reset|status`.
// No engine writes, no object discovery. Off costs one lock-free read per
// writer call and per present.
#pragma once
#include <stdint.h>

namespace dvr::zacct {

struct Head {
    uint32_t seq = 0;            // the present it was composed on
    double   ms = 0.0;
    float    raw[3] = {0, 0, 0};    // right, up, forward (uu): the tracked head displacement
    float    neck[3] = {0, 0, 0};   // the neck term as SUPPLIED to the seam (cancel: minus the arc)
    float    pos[3] = {0, 0, 0};    // the triple handed to set_position_offset_uu
    float    pitchDeg = 0.0f, yawDeg = 0.0f, rollDeg = 0.0f;
    float    neckBelowM = 0.0f, neckBehindM = 0.0f, scale = 0.0f;
    int      neckMode = 0;
    bool     posTrack = false, projection = false, ok = false;
};

constexpr int kClampFields = 4;
struct Clamp {
    uint32_t seq = 0;
    double   ms = 0.0;
    bool     ran = false;        // the clamp block ran: live pawn, fresh cylinder, readable location
    float    pawn[3] = {0, 0, 0};
    float    cyl = 0.0f;
    double   cylAgeMs = 0.0;
    float    ceilRaw = 0.0f, ceilEased = 0.0f;
    uint32_t fieldOff[kClampFields] = {0, 0, 0, 0};
    bool     readable[kClampFields] = {false, false, false, false};
    float    pre[kClampFields] = {0, 0, 0, 0};    // Z before the clamp: NOT the engine eye, it may hold our write
    float    post[kClampFields] = {0, 0, 0, 0};
    bool     ours[kClampFields] = {false, false, false, false};   // the clamp reconciled an exact prior writer value
};

struct Write {
    uint32_t id = 0;             // assigned when pinned
    uint32_t seq = 0;            // writer call counter
    double   ms = 0.0;
    uint8_t* cam = nullptr;      // identity only, never dereferenced here
    uint32_t fieldOff = 0;
    float    sign = 1.0f, c5Sign = 1.0f;
    int      eye = 0;
    bool     secondPass = false;
    bool     wrote = false;
    const char* skip = "";       // a static string: why the call did not write
    bool     otherTest = false, projection = false, laneCamera = false;
    Clamp    clamp;
    bool     clampOk = false;
    Head     head;
    bool     headOk = false;
    bool     torn = false;       // the locked snapshot's triple is not the triple the writer used
    float    fieldNow[3] = {0, 0, 0};
    bool     persisted = false;  // the field still held our last write, so the base was RECOVERED
    float    base[3] = {0, 0, 0};
    float    priorOff[3] = {0, 0, 0};
    float    eyeW[3] = {0, 0, 0}, posW[3] = {0, 0, 0};   // world-axis terms, position form
    bool     posLive = false, posDropped = false;
    float    heading[2] = {1.0f, 0.0f};   // the yaw-only forward the position used
    float    camPitchDeg = 0.0f;          // the camera's own pitch, from its forward row
    bool     basisOk = false;
    float    candZ = 0.0f, capDelta = 0.0f;
    bool     capOn = false;
    float    written[3] = {0, 0, 0};      // world position the field now holds
    bool     tagPosMatch = true;          // the tag's position equals this write (set at pin)
};

double now_ms();                         // one clock for every record (QPC)

// ---- the lever -----------------------------------------------------------------
void set_enabled(bool on, const char* source);
bool enabled();
void reset(const char* why);

// ---- producers -----------------------------------------------------------------
void publish_head(const Head& h);        // present thread
bool head_snapshot(Head* out);           // script lane
void note_clamp(const Clamp& c);         // script lane (FovLeverApply)
bool clamp_latest(Clamp* out);           // script lane
void note_write(const Write& w);         // script lane: the latest writer call
// Script lane, where scene_draw pushes a tag: pin the latest write under a new
// id. `tagPos` is the position the tag carries (c5 form) or null. 0 = nothing
// to pin (off, or no writer call since the probe was armed).
uint32_t pin_for_tag(const float* tagPos);

// ---- consumer (present thread) ---------------------------------------------------
// ringEye: the eye the ring delivered; finalEye: the eye the pairing chose.
void on_present(int ringEye, int finalEye, bool tagged, uint32_t id, bool haveC5, const float c5[3],
                uint32_t c5Serial, double nowMs);
void tick(double nowMs);                 // progress lines, rate limited
void flush(const char* why);             // close the episode and print it (any thread)
void log_status();

} // namespace dvr::zacct
