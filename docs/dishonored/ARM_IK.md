## Menu visibility correction, 2026-10-04

The weapon-wheel disappearance report exposed a broad IK-only menu gate.
Input-owning menus are now allowed to use full-arm IK whenever the existing
qualified arm draw and hand-placement guards succeed. The main title screen
remains blocked. Mesh-source changes still invalidate the copied rig and
current palette/view/tracking checks run on every draw; no retained UObject
identity or engine-memory write is added. Menu context is included in IK
telemetry. Weapon-wheel and possible pause-menu loss are separate from the
unclassified minor arm flicker. No new playtest has run. The next launch
asks only whether full arms stay visible through weapon-wheel open/use/close;
roll and pause acceptance follow separately.

# Full-arm IK

Implemented locally on `codex/ik-full-arms`, based on staging
`957322031d6c67aebd4f0910261f682501b43074`. Updated 2026-10-04. Host and
Blender validation pass with the deformation limits below. Runtime palette
mapping and basic arm tracking were accepted on v1.0.3-11-g8eee77252.
Minor flicker and forearm collapse on wrist roll led to the follow-up below.
No ticket, push or PR: work remains local per maintainer instruction.

## Behavior and controls

The dedicated **IK** tab sits beside Hands in the L3+R3/F10 menu, including the
Basic view. Shared XYZ and total width set nominal shoulder positions. Each
shoulder independently slides toward/away from its own wrist when the wrist
is outside the fork's near/far reach limits. It returns to its nominal anchor
when reach allows. This supersedes the initial fixed-shoulder design.

| `[Hands]` setting | Default | Range / meaning |
|---|---:|---|
| `ArmIK` | 1 | Full-arm rendering, default on since 1.0.4; live menu toggle |
| `ArmShoulderForwardCm` | -16 | -50..50; positive moves both forward |
| `ArmShoulderRightCm` | 0 | -50..50; positive shifts their shared center right |
| `ArmShoulderUpCm` | -25 | -80..20; positive raises both |
| `ArmShoulderWidthCm` | 38.1 | 10..80; total separation, split equally |
| `ArmLengthScale` | 1.27 | 0.5..2; longitudinal arm length, separate from hand size |
| `ArmElbowOut` | 0.6 | 0..2; mirrored outward component of the elbow pole |

The four fit defaults (forward, up, width, length) are the fit tuned in a headset
on 2026-10-06; before that they were -6, -20, 36 and 1. `[Meta] DefaultsRev=2`
moves an existing ini to them once, key by key, and only where a key still holds
the old default: a fit set in F10 stays. The log line `config: defaults revision 2`
names which keys moved and which were kept.

Controls save when released; reset changes these six adjustments without
switching IK off. Reload arm reference retries a missing/changed local rig.
Hand/grip settings and animation choices remain on Hands. Full-arm IK draws
the entire original mesh independently of sleeve cuts. Sleeve controls are
disabled while IK is enabled; saved cuts and RigidWrist apply only to fallback
hands or normal IK-off rendering. Arm length changes reach, not visibility.

Nominal anchors share one body frame and center:

```text
C       = bodyOrigin + unitsPerCm * (forwardCm*F + rightCm*R + upCm*U)
S_left  = C - unitsPerCm * widthCm/2 * R
S_right = C + unitsPerCm * widthCm/2 * R
```

Only half-width is mirrored, not the shared right offset. The fork's body yaw
has a 25-degree head-yaw deadzone and 1.5-second relaxation. Both arms use it,
carried through the same tracking-to-draw bridge as existing hands. Head pitch
and roll are canceled by that bridge. Like the fork, translation follows the
head-relative shoulder center; this is not a separate tracked torso.

For segment lengths a/b, use max reach `(a+b)*0.995`, and minimum
`max(abs(a-b)*1.05 + 0.5 cm, (a+b)*0.40)`. Clamp distance and slide only that
shoulder along its shoulder-wrist direction, preserving the final wrist and
segment lengths. Zero distance has a deterministic pole/outward fallback.
Extreme reach can visibly separate shoulder roots from a torso: this is the
requested independent-reach behavior, not a body mesh attachment solver.

## Animation boundary and rendering

`arm_ik_draw.inc` replaces the qualified native arm draw with a mod-owned copy
of the complete mesh and its original weights. It creates a single palette
containing independent transforms for both arms. No engine bone/actor memory
is written, and no engine UObject identity is retained.

`MpWorldTarget` still owns each hand's correction, including `anim::blend` and
weapon publication. Empty right-hand mirroring still runs. The IK endpoint is
the named wrist transformed by that FINAL hand palette, after animation
blending. It is not the earlier `g_mpPalmTarget`. Wrist/finger descendants keep
the existing corrected native animation; upper-arm, forearm, sleeve and helper
slots receive reference-to-IK skin matrices. Forearm helper roll is distributed
by its reference position along the forearm, with continuous wrist twist and
the fork's bounded elbow swivel.

The `MsDraw` native-full-arm passthrough is suppressed while IK is enabled.
Native hand animation can still own the wrist, and the IK arm follows it.
Gameplay/action eligibility and camera animation are unchanged. If mapping or
tracking refuses, existing split hands render and the IK tab/log names why.
The original behavior remains when IK is off.

`RigidWrist=1` already protects clipped wrist/cap geometry by redirecting its
forearm influences to the wrist bone while fingers animate. It is retained for
fallback, not applied to the complete arm mesh. Full arms preserve original
skin weights so the shoulder, elbow and forearm can deform.

Elbow/pole and twist history is separate per hand, in body axes. An eight-entry
pose-generation cache shares the pre-update history across eyes and passes;
old queued generations cannot rewind current history. Tracking gaps, menus,
device resets, mesh-generation changes and explicit reload discard history.
The draw saves and restores the original palette, VB, IB and changed viewport.

## Local reference preparation and validation

`tools/prepare-arm-rig.py` reads a locally extracted PSK and writes
`dishonored_vr_arm_rig.bin` in the mod's resolved data directory (Paths/DataDir,
DVR_DATA_DIR, otherwise LocalAppData/DishonoredVR). The file contains named
reference joint heads/hierarchy and reference vertex positions/weights in
ENGINE mesh coordinates (`DVRIK002`). Preparation reverses UModel's PSK Y
reflection for both composed heads and vertices. Version 1 is rejected. This
tool still prepares it from a locally extracted mesh. Since 2026-10-05 the prepared
rig is shipped: `assets/vr/dishonored_vr_arm_rig.bin` is committed (the owner's
decision, recorded in CLAUDE.md), embedded in the proxy and written into the data
directory on start (`core/util/embedded_assets.cpp`), so players need no setup.
PSKs, Blender files, captures and other extracted content stay out of the tree.

There are no copied BioShock indices or new engine offsets. At mesh build,
all runtime vertex positions must match reference points within 0.03 model
units. Each active palette slot must then uniquely match a reference bone's
weight field over the entire vertex population. Quantized weights and UV seam
duplicates are supported. Ambiguous fields, unexpected mesh/triangle layout,
out-of-range indices, missing named chains or disagreement with the established
hand wrist slots refuse the full-arm path. Successful mapping logs every slot,
reference name, maximum errors, counts and source generation.

This is an alternative to reading an unverified native RefSkeleton/BoneMap
layout. Weighted centroids are not used as joint positions. The complete local
source match gates use of the reference joint heads. Build 11's verified
headset run mapped all 2,771 vertices and 48 palette slots with zero position
and weight error; full articulated arms were accepted. See the live evidence
below for the initial failed coordinate convention and its correction.

## Reference versions and adopted behavior

| Reference | Reviewed revision | Relevant functions |
|---|---|---|
| [BioShock IK improvements](https://github.com/VR-Stereo-Hub/bioshock-trilogy-vr/tree/feat/bs1-ik-improvements) | `e929cfe9513c6f9f7f22fa4136b19128cf999843` | `bones.cpp`: `solve_arm`, driven/free-hand callers, `reapply` |
| [BioShock left-hand fork](https://github.com/Owloeb/bioshock-trilogy-vr-lefthand) | `3b5b818ed5cdb4e372320d0521e5b26f7a277c5e` | `hands.cpp`: `publish_arm_targets`; `bones.cpp`: `arm_ik`, reference frame rotation, twist and cached writes |

Use the fork's paired nominal anchors, independent reach correction, stable
pole projection, reference frame rotations, twist continuity and elbow swivel.
Use the upstream lessons about matched spaces/scales, body-relative elbow
history, own-wrist twist input and authored orientation, and stereo replay.
The runtime writes and bone/helper indices are specific to BioShock and were
not copied. The MIT notice is in `docs/licenses/bioshock-arm-ik.txt`.

## Accepted activation; twist and timing follow-up, 2026-10-04

The v1.0.3-11-g8eee77252 banner and installed proxy hash match. Runtime
validated all 2,771 vertices, all 48 active palette slots and 4,448 triangles,
with zero position/weight error. Full articulated arms were accepted. The
remaining reported issues are a small, not yet classified arm flicker and
forearm collapse during wrist roll. Whole-arm disappearance was not the
reported flicker. Controller-tracking-loss fallbacks also exist in the log;
they do not establish the cause of the smaller visual artifact. The accepted
DLL, rig, INI and both logs are preserved under `build/arm-ik-test/live-working/`.

**Timing candidate:** the arm path already uses the normal hands' draw-owned
view matching, eye correction and final animated wrist. Its additional IK
history used `rec.track.gen` on matched views but `pose.gen` on fallback views.
The latter counts hand publications, not XR locates. The log alternates values
around 58,000 and 88,000; one fallback made later matched samples appear old,
discarding elbow/twist history and the body-yaw filter. Each stored head matrix
now carries its actual locate generation, including the lagged fallback.
Both branches use that one domain. A matched/fallback/matched regression
reproduces history loss in the old control and retains it in the corrected
path. The log now labels locate/publication separately and counts history
advance/reuse/old. This is a code/host-confirmed defect; the headset flicker's
causal attribution remains pending. See FLICKER_REFERENCE for the route.

**Twist candidate:** the mesh blends lower-arm and sleeve bones across much
of the shaft, including nearly 50/50 vertices. The original zero-roll lower
arm and position-weighted sleeve roll differed by about 127 degrees, reducing
shaft radius to 44.5% in the production roll sweep. Forearm bones now share
70% of axial roll at the elbow and ramp to the wrist. They use the continuous
post-swivel roll instead of independently clamping only the sleeve/helpers
while the hand keeps turning. Elbow/wrist positions and native hand/finger
matrices remain exact. No mesh hiding, reference-asset edits or weight edits.

On the same 260-frame sweep at the player's current 1.2 length multiplier,
the old control fails an 85% shaft-radius floor (44.5% minimum); the candidate
passes (88.1%). The radius test separates the shaft from vertices influenced
by the native wrist, where bending legitimately creases the cuff. Those cuff
measurements are still reported, not excluded from the evidence. Across all
260 frames there are zero solve failures, maximum joint-length error 0.000033
and wrist join error 0.000035 units. Finger-only animation keeps the arms
stable. One triangle drops below 1% area in an extreme crossed-pose transition
(frame 48); no triangle exceeds 10x reference area. No collision constraint.

Both versions were simulated in Blender at length 1.2. Baseline: 260 baked
frames, five comparison renders. Candidate: 260 frames, 18 inspected key
poses. Evidence: `twist-baseline/length120-sweep.json`,
`twist-final-length120-sweep.json`, the `*-volume.json` reports,
`blender-twist-baseline/` and `blender-twist-final/` under `build/arm-ik-test/`.
The original Blender workspace and accepted installed rig remain unchanged.

**Capture control:** L3+R3/F10 > IK has an always-visible `Capture 16 frames
(5 second delay)` button; Display > Frame capture exposes it in Basic too.
It closes the panel. In AFW it calls the existing rich AFW capture. In native
stereo it writes 16 full-resolution eye BMPs and `frames.csv` under a unique
`dumps/frames-*` directory. Rows identify the delivered serial/record/eye and
previous-present source. Source repeats are ignored, worker backpressure gaps
are counted, the queue is capped at 96 MiB/three jobs and a 15-second capture
deadline prevents indefinite arming. Worker write errors cannot report success.
Readback can perturb cadence; these are not guaranteed consecutive presents
or final compositor/VD images. This is diagnostic pixel evidence, not a
nonintrusive performance recording. See PERFORMANCE for capture cost notes.

Host checks: 1,083 arm/history/capture-schedule assertions pass. A standalone
32-bit D3D11 WARP test compiles the same asynchronous image writer, verifies
32 BMPs pixel-for-pixel in RGBA and BGRA formats, confirms PNG compatibility
and checks failed-write cleanup. No game was launched for these tests.
The button's in-game invocation and both visual fixes await the next launch.

## First live run and coordinate-boundary correction, 2026-10-04

Banner and installed SHA matched `v1.0.3-9-g012ddce9a`. The player reported
clipped arms and no apparent IK response. The log shows seven mesh-build
refusals, `runtime position absent from local reference`, and zero solves.
Changing the sleeve cut from -10 to -30 and other presets only changed the
fallback mesh. No conclusion about live IK deformation is supported by this run.
Logs and INI are archived under `build/arm-ik-test/live-first/`.

Cause: the initial preparation retained the PSK export coordinate system.
[UModel ExportPsk.cpp](https://github.com/gildor2/UEViewer/blob/master/Exporters/ExportPsk.cpp)
reflects Y on vertices and skeletal transforms (`MIRROR_MESH`, points lines
95-108, bones lines 386-389). The corrected reference reverses that reflection
on vertices AND composed joint heads. Its rounded bounds now exactly match
all three runtime bounds: (-58.4,-152.5,-14.0) to (58.4,-87.9,17.3).
The Blender check originally compared two PSK-space inputs, so it could not
expose this engine/export boundary. That limitation is now covered by an
independent synthetic PSK with explicit expected engine vertices and nested
rotated joint heads, plus an old-convention negative mapping test.

Full-arm drawing already bypasses cut geometry when IK activates. Sleeve
controls are now disabled while IK is on and explain this ownership. The IK
tab keeps the last activation/refusal visible when the menu opens instead of
overwriting it with a waiting message. Mapping failures report vertex index,
position, match count and nearest-reference distance. Guard thresholds remain
unchanged; no unsafe mapping bypass was added.

## Verification, 2026-10-04

- 1,052 host checks pass using the production headers. Cases include shoulder
  translation/width, near/far/zero reach, preserved lengths, arbitrary frame
  transforms, invalid data, twist wrap, view/head cancellation, separate hand
  history, once-per-pose stereo updates, stale queued views and tracking gaps.
- Local reference mapping fixture: all 48 active bones recovered exactly from
  shuffled palette slots, packed weight quantization and duplicated UV seams
  (2,830 synthetic runtime vertices from 2,264 local reference points).
- Production `pose_arm` exports 260 frames covering relaxed/forward/extended,
  close/crossed/raised/wide/down/behind, asymmetric reach, 0..360 wrist roll,
  a finger-only clip, short/long arms, zero shoulder-wrist distance, the installed
  0.85 hand scale, and independently extended arms. Zero solve failures;
  maximum segment-length error 0.0000172 and wrist-join error 0.0000335 units.
- Blender bakes those SAME matrices through the original skin weights into an
  animated local scene. Reference joint heads independently agree with the
  imported PSK within 0.000020 units. All mesh frames are finite; 18 key poses
  rendered and inspected. No triangle exceeded 10x reference area. Frames 87 and 88
  in a severe transition each compress one triangle below 1% reference area;
  visible sleeve compression at extreme poses remains a deformation limitation.
  There is no collision/torso constraint, so crossed arms may intersect.
- The existing hand/weapon `frame_test` suite passes. Optimized Win32 proxy
  compiles; the 11-name undecorated proxy export contract passes.
- Blender is offline verification, not proof of live shader mapping, stereo,
  game animation routing or menu usability. No game was launched by the agent.

Evidence stays under `build/arm-ik-test/`: `fixed-sweep.json`, host executables,
`blender-fixed/Arm-IK-Pose-Sweep.blend`, `blender-fixed/verification.json` and
18 key renders. The first candidate's evidence remains in `blender/`. The original Blender workspace is preserved.

## Next headset test

One question: does the forearm retain its shape through the wrist-roll angle
that previously pinched it? Hold the arm in view, use IK > Capture 16 frames
(5 second delay), then slowly roll the left hand palm-up and palm-down after
the menu closes. A full shaft supports the twist correction. A remaining
pinch means the captured wrist/reach pose needs another deformation check.
Use the same sequence to inspect the unclassified flicker offline, without a
second perceptual question in this launch. A brief capture hitch is expected.
Verify the installed banner before interpreting the log or images.

## Original first-run question (IK did not activate)

One question: in a loaded save, with the body and right controller stationary,
does the left sleeve remain connected to its hand through a slow close-to-far
reach, while the right arm remains still? Left shoulder movement at the reach
limits is expected. Connection plus an unchanged opposite arm supports the
reach/ownership path. A detached wrist points to runtime mapping/scale or final
endpoint disagreement. Opposite-arm movement indicates shared-state/frame
coupling. Only floating hands means the guarded full-arm path refused; the log
must distinguish its reason rather than infer it from appearance.

Before interpreting the launch, verify the log banner against the installed
build. Read `ik/map`, `ik: reference validated`, `ik: ACTIVE` or fallback lines.
Archive current and previous logs before a relaunch. Separate later launches
cover native hand animation, tab persistence, rapid head turns and menu/load.

## The game's own arm during game animations, 2026-10-04

`[Hands] ArmIKGameArmInAnim=1` (F10 IK): while a game animation owns a hand, that arm's slots
blend from the IK solution to the game's own skin matrices by the hand-back weight. At full
ownership the arm is the game's exactly. Accepted in a headset for the choke.

`[Hands] ArmIKGameArmShoulder` (F10 IK, under it; default 0): the game poses its arm for its own
camera and body, so its shoulder is not where the IK shoulder is, and it was reported in front
of it in every takedown but the choke. 1 re-seats the game's arm on the IK shoulder about the
game's own wrist (`shoulder_fit`: a stretch along the shoulder-wrist line within 0.80..1.25,
then the smallest rotation within 45 degrees), in every game animation except the choke; 2
includes the choke. The hand stays where the clip put it. `ik/gamearm:` logs the offset between
the two shoulders in body axes and what the re-seat did, with the lever on or off.
Headset-confirmed at 1 on 2026-10-04 (two front fatalities re-seated, two chokes left alone;
the shoulders measured 8 to 27 uu apart, the IK shoulder 5 to 14 uu higher). Detail:
ANIM-HANDOFF-PLAN.md, "Third headset run" and "Fourth headset run".
