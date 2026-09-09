// core/vr/pose_record.h - three records, kept apart on purpose (VR-65).
//
// THE MISTAKE THE FIRST VERSION MADE, recorded because the number it produced
// was quoted before it was checked. It kept ONE record and called it "the pose
// the image was rendered with". It was not: it read the LIVE head globals at
// draw time, which are whatever the Present thread last wrote, and compared them
// against a pose derived from that same input. Two values derived from one input
// agree by construction. Its near-zero result says nothing about whether
// rendering honoured the camera, and its "+51 degrees of error" was arithmetic -
// it differenced a UE world yaw against an XR tracking yaw and then doubled the
// answer through a sign convention.
//
// So the question is split into three, and they are never mixed:
//
//   CAMERA      what the camera was TOLD to use, published as one unit at the
//               camera write itself, together with the camera it produced
//   RENDER      what rendering ACTUALLY consumed, read off the shader constants
//               on the render thread, independent of anything above
//   SUBMISSION  what OpenXR was TOLD the image represents, per eye
//
// and three separate checks:
//
//   CAMERA vs RENDER      did the intended camera reach rendering?
//   RENDER vs SUBMISSION  does the submitted metadata describe this image?
//   TRANSPORT             did the right record stay with the right eye image?
//
// Only the middle one can clear the leading hypothesis. Only the third can say
// whether the other two were looking at the same frame. A successful record
// lookup proves availability, not correspondence.
//
// CONVENTIONS, STATED ONCE. The tracking sample is stored in OpenXR convention:
// quaternion plus position, right +X, up +Y, forward -Z, metres. The game camera
// is stored separately in UE world Euler degrees. THEY ARE NOT COMPARABLE - the
// game camera's yaw contains body and thumbstick rotation the tracking pose
// never sees. Anything that differences them directly is measuring nothing.
#pragma once

#include <stdint.h>

namespace dvr::pose {

// ---- CAMERA: what the camera was told, and what it produced -----------------

// The tracking sample, in OpenXR convention. One coherent read, taken where the
// camera is computed - never reassembled afterwards out of separate globals.
struct Track {
    float    qx, qy, qz, qw;
    float    px, py, pz;      // metres, XR LOCAL space
    uint32_t gen;             // the runtime's locate generation this came from
    double   locateMs;        // when the LOCATE happened, not when we wrote
    bool     ok;
};

// The game camera that sample produced, in UE world convention.
struct Cam {
    float  yawDeg, pitchDeg, rollDeg;
    float  pos[3];            // the eye position written, engine units
    bool   posOk;
    double writeMs;           // when the camera write happened
    int    writer;            // 1 script dispatch, 2 direct fallback, 0 none
    bool   ok;
};

// Published together, as one unit, at each camera construction. Both writer
// paths call it: a run that has fallen back must stay measurable, or the
// instrument goes quiet exactly when something is already wrong.
void publish_camera(const Track& t, const Cam& c);

// Copy the latest published pair out under the lock. Never hands back a pointer
// into shared storage.
bool camera_snapshot(Track* t, Cam* c);


// ---- the per-view record that travels with the image ------------------------

struct Record {
    uint32_t id;
    uint32_t pairId;
    int      eye;             // -1 left, +1 right, 0 mono / untagged
    Track    track;           // a COPY of the published sample, not a re-read
    Cam      cam;             // a COPY of the camera it produced
    double   openedMs;
    bool     secondPassReuse; // this view reused pass 1's camera, deliberately
};

uint32_t next_pair();

// Open a record for the view about to be drawn. GAME thread. It COPIES the
// published camera pair rather than sampling anything itself, so the record
// cannot disagree with the camera that was actually written.
uint32_t open(int eye, uint32_t pairId, bool secondPassReuse);

// COPY a record out. The ring can be overwritten while a reader works, so there
// is no pointer accessor: this takes the lock, checks the id, and copies.
// False for an id nobody set (missing) or one since overwritten (expired) -
// counted apart, because they mean different things.
bool copy(uint32_t id, Record* out);


// ---- RENDER: what the draw actually consumed --------------------------------
//
// Read off the vertex-shader constants on the render thread. The only evidence
// in the chain that does not descend from the mod's own intent.
//
// The matrix layout is NOT assumed. note_render_vp stores the block; the solver
// validates the multiplication convention against the camera position the same
// upload carried and reports which one answered. A register number is not proof
// of a layout, so the layout is measured.
void note_render_vp(const float vp16[16], const float camPos[3], bool camPosOk);

// The world yaw the observed view-projection actually implies, UE degrees,
// found by projecting probe directions around the observed camera position and
// taking the one that lands on the screen centre. No decomposition.
// False when no usable block has been seen or neither convention validated -
// which is a finding, logged, never treated as agreement.
bool render_yaw_deg(float* outDeg, uint32_t* outSerial);


// ---- the controls -----------------------------------------------------------
//
// Each proves ONE thing and the log says which. They operate on a DIAGNOSTIC
// COPY, never on anything that reaches submission, so an armed control cannot
// move the picture. The first version perturbed the real record and warned the
// tester to expect a wobble; that was the wrong design and the warning with it.
enum Control {
    CTRL_NONE = 0,
    CTRL_YAW,        // a known angular perturbation: does the comparison respond?
    CTRL_OLD_REC,    // an older record substituted: is generation association live?
    CTRL_WRONG_EYE,  // the eyes swapped: is eye association actually checked?
    CTRL_COUNT
};
void configure_controls(uint32_t startAfter, uint32_t eachLen, float yawDeg);

// Run the controls against a real observation, reporting pass or fail against an
// explicit numeric tolerance.
void check_controls(const Record& real, float observedYawDeg);


struct Stats {
    uint32_t opened, copies, expired, missing, pairs;
    uint32_t camPublished, renderBlocks;
    uint32_t ctrlPass, ctrlFail;
};
Stats stats();
void  log_beat();

} // namespace dvr::pose
