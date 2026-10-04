# Full-arm IK

Implemented locally on `codex/ik-full-arms`, based on staging
`957322031d6c67aebd4f0910261f682501b43074`. Updated 2026-10-04. Host and
Blender validation pass with the deformation limits below. Runtime palette
mapping and headset behavior still require the first user-launched run.
No ticket, push or PR: work remains local per maintainer instruction.

## Behavior and controls

The dedicated **IK** tab sits beside Hands in the L3+R3/F10 menu, including the
Basic view. Shared XYZ and total width set nominal shoulder positions. Each
shoulder independently slides toward/away from its own wrist when the wrist
is outside the fork's near/far reach limits. It returns to its nominal anchor
when reach allows. This supersedes the initial fixed-shoulder design.

| `[Hands]` setting | Default | Range / meaning |
|---|---:|---|
| `ArmIK` | 0 | Full-arm rendering, default off; live menu toggle |
| `ArmShoulderForwardCm` | -6 | -50..50; positive moves both forward |
| `ArmShoulderRightCm` | 0 | -50..50; positive shifts their shared center right |
| `ArmShoulderUpCm` | -20 | -80..20; positive raises both |
| `ArmShoulderWidthCm` | 36 | 10..80; total separation, split equally |
| `ArmLengthScale` | 1 | 0.5..2; longitudinal arm length, separate from hand size |
| `ArmElbowOut` | 0.6 | 0..2; mirrored outward component of the elbow pole |

Controls save when released; reset changes these six adjustments without
switching IK off. Reload arm reference retries a missing/changed local rig.
Hand/grip settings and animation choices remain on Hands. Sleeve cuts and
RigidWrist apply to the floating-hand fallback.

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
reference joint heads/hierarchy and reference vertex positions/weights. This
local prototype requires preparation; it does not extract installed packages
automatically. The tool is committed; PSK, binary rig, Blender files, captures
and other extracted game content are not. Ignore rules protect the rig/PSK.

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
source match gates use of the reference joint heads. Current runtime mesh and
palette evidence is still owed; synthetic mapping tests cannot establish that
the game emits the expected coordinate convention in a launch.

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

## Verification, 2026-10-04

- 1,049 host checks pass using the production headers. Cases include shoulder
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
  maximum segment-length error 0.0000211 and wrist-join error 0.0000324 units.
- Blender bakes those SAME matrices through the original skin weights into an
  animated local scene. Reference joint heads independently agree with the
  imported PSK within 0.000020 units. All mesh frames are finite; 18 key poses
  rendered and inspected. No triangle exceeded 10x reference area. Two frames
  in a severe transition each compress one triangle below 1% reference area;
  visible sleeve compression at extreme poses remains a deformation limitation.
  There is no collision/torso constraint, so crossed arms may intersect.
- The existing hand/weapon `frame_test` suite passes. Optimized Win32 proxy
  compiles; the 11-name undecorated proxy export contract passes.
- Blender is offline verification, not proof of live shader mapping, stereo,
  game animation routing or menu usability. No game was launched by the agent.

Evidence stays under `build/arm-ik-test/`: `sweep.json`, host executables,
`blender/Arm-IK-Pose-Sweep.blend`, `blender/verification.json`, key renders and
`blender/IK-pose-contact-sheet.png`. The original Blender workspace is preserved.

## First headset test

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
