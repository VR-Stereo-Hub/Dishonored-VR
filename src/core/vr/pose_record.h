// core/vr/pose_record.h - the pose an image was actually rendered with (VR-65).
//
// THE PROBLEM THIS EXISTS FOR. The pose submitted to OpenXR with an eye image is
// currently CHOSEN AT SUBMISSION TIME out of a global history, by a fixed lag
// setting. Nothing carries the pose that the image was actually rendered with.
// The chain is:
//
//   Present hook locates tracking and publishes a head rotation
//   -> the GAME thread applies it to the game camera
//   -> the game queues two viewport draws
//   -> the RENDER thread replays them; a FIFO plus camera-position heuristics
//      decide which eye each present is
//   -> capture delivers the previous present's texture slot (SharedWait=0)
//   -> submission picks a pose out of a history by g_poseLag
//
// Every step of that is a timing assumption, and the last one was calibrated
// against BioShock 1's SINGLE-THREADED renderer. Dishonored has a separate
// render thread and an extra delayed capture stage. Khronos is explicit that the
// compositor must be given the pose the submitted image was rendered with;
// giving it a different one makes world content move wrongly with the head,
// which is the reported symptom and is worst on a PHYSICAL turn - a thumbstick
// turn moves the game camera while the head barely moves, so the compositor's
// correction has almost nothing to do and the error hides.
//
// THIS HEADER CHANGES NO BEHAVIOUR. It is the instrument that has to come first.
// A record is opened where the camera is built, its id rides the existing eye
// tag through the ring and onto the capture slot, and submission can then ask
// what the DELIVERED texture was actually rendered with instead of guessing.
//
// WHY AN INSTRUMENT FIRST, AND WHY IT HAS A NEGATIVE CONTROL. The audit that
// exists today compares the submitted pose against the LATEST published camera
// yaw at submission time - not against the sample belonging to the captured
// image - so with two threads it can agree for the wrong reason. It reported
// near-zero error while this fault was present. Earlier in this project an audit
// read a global across two threads and produced a confident 39% that meant
// nothing. So this one ships with a deliberate-error mode: inject a known yaw
// offset into the record and the audit must report exactly that offset. An audit
// that cannot fail its own hypothesis is not evidence.
#pragma once

#include <stdint.h>

namespace dvr::pose {

// One rendered eye, as it actually was. Immutable once opened.
struct Record {
    uint32_t id;            // 0 = never filled; ids start at 1 and only grow
    uint32_t pairId;        // both eyes of one game tick share this
    int      eye;           // -1 left, +1 right, 0 mono / untagged

    // THE TRACKING SAMPLE THE CAMERA WAS BUILT FROM, taken as one coherent read
    // rather than as separate globals. Yaw/pitch/roll are the mod's own
    // published head angles in DEGREES; gen is the locate generation they came
    // from, and locateMs is when that locate happened.
    float    yawDeg, pitchDeg, rollDeg;
    uint32_t gen;
    double   locateMs;

    // The head POSITION in tracking space at that sample, and the game-space
    // camera position the eye write produced. The second is what the existing
    // c5 pairing already measures, so a record can be checked against it.
    float    headPos[3];
    bool     headPosOk;
    float    camPos[3];
    bool     camPosOk;
    float    injectedYawDeg;   // the negative control's offset, 0 when disarmed

    // When the record was opened, on the game thread.
    double   openedMs;
};

// The tracking sample, handed in by the game adapter as ONE argument so it
// cannot be assembled here out of separately-read globals. That is the whole
// point: the camera was built from one coherent sample and the record has to
// hold that same one. Angles in DEGREES.
struct Sample {
    float    yawDeg, pitchDeg, rollDeg;
    uint32_t gen;
    double   locateMs;
    float    headPos[3];
    bool     headPosOk;
};

// Open a record for the eye that is about to be drawn. Called on the GAME
// thread, from the same place the eye tag is pushed, so the sample it carries
// is the one the camera for THIS pass was built from. Returns the id to carry,
// or 0 if the ring refused (which is counted, never silent).
uint32_t open(int eye, uint32_t pairId, const Sample& s,
              const float camPos[3], bool camPosOk);

// Look one up by id. Returns nullptr when the id is 0, unknown, or has been
// overwritten by the ring - which is itself a finding and is counted.
const Record* get(uint32_t id);

// Start a new pair. Called once per doubled game tick, before either pass.
uint32_t next_pair();

// THE NEGATIVE CONTROL. While armed, every record opened is given a yaw offset
// by this many degrees, so the audit downstream MUST report an error of exactly
// that size. If it reports zero, the audit is measuring the wrong thing and no
// reassuring number from it can be believed. 0 disarms.
void  set_inject_yaw_deg(float deg);
float inject_yaw_deg();

// THE SELF TEST. The tester does not run commands, and a negative control that
// nobody arms proves nothing - so it arms ITSELF, once, a few seconds into a
// run: records opened between two counts carry a known wrong yaw, and the
// submission audit must report exactly that error for exactly that window. One
// ordinary play session then contains the proof that the audit can fail.
//
// Off by setting the window to 0. It injects a rotation error of a few degrees
// for about a second, which is visible but harmless, and it says so in the log
// at both edges so nobody reads the wobble as a fault.
void configure_self_test(uint32_t startAfter, uint32_t records, float deg);

// Counters for the 3 s line: records opened, lookups answered, lookups that
// arrived after the ring had wrapped, and lookups for an id nobody set.
struct Stats { uint32_t opened, hits, expired, missing, pairs; };
Stats stats();

// One line, from the present thread's beat. Prints the join for the most recent
// delivered record so a log reader can see the whole chain on one line.
void log_beat();

} // namespace dvr::pose
