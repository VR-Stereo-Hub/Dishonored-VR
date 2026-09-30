# BRIEF: AFW next session - SSW with depth, DLSS object motion, the sword's shading (VR-39 follow-up)

Branch: `claude/vr-39-afw-polish` (off `staging` after #158). Linear could not take a new ticket (the
workspace hit its free issue limit on 2026-09-28), so this work refs VR-39 and lives here.

## Where things stand (build `v1.0.1-169` or later on this branch, headset pending)

**Merged to staging in #158:** AFW (alternate frame warping).
- The held eye is rebuilt from both images: seeded searches, ranked in the rebuilt eye's own depth.
- Stale test, game-matrix walking, and the foreground (arms and weapon) at the game camera's FOV.
- The diagnostic capture and the replay tool.

**This branch adds:**
1. **The foreground mask.**
   - The scene target is copied at the first crushed-depth viewport of each frame (MaxZ < 0.5: the
     foreground pass). Texels whose depth changed after it are the arms and weapon, stored as negative
     depth in each eye's depth copy.
   - It replaces the 0.30-unit depth limit. That limit put a far sword tip into the world ("flat
     doubled texture") and a near wall into the foreground.
   - Words: `afw fgmask on|off`, `afw fgdepth`. Log: `depthshare: ... pre-foreground copies ...` and
     `afw/warp: beat ... foreground MASK on N rebuilds`.
   - Host: a world bar at 0.35 m stays world (its control fails without the mask); a foreground at
     1.2 m keeps its projection.
2. **Running.**
   - The camera check refused the game matrices whenever the body moved more than about 6 uu in a
     present, so while running the held eye's world dropped to the XR model (no walking) on those
     presents. Blur, and jumps.
   - It is now a sliding vote over still presents (a flipped c5 is a permanent fault). Per present only
     a jump past 150 uu refuses.
   - The turn check's limit is 5 deg, not 0.5: the recorded body yaw lags up to 3 deg in fast turns.
   - Host: a 25 cm step in one tick is carried; the flipped-c5 vote latches after 32 still presents and
     a toggle clears it.
3. **DLSS: the hands and weapon always "trust the current colour".**
   - The foreground mask goes into the existing bias mask (`dlss fgbias on|off`, default on), which is
     DLSS's BiasCurrentColorMask and FSR's reactive mask.
   - Why it is needed: the mod's vectors are the camera's only, so the hands' history is wrong whenever
     they move.
   - Log: the `dlss:` line's `hands/weapon mask on N/s`.

## Open, in order

### 1. SSW while running (and more): submit depth - BUILT on this branch, headset A/B pending

Built as below (`[VR] SubmitDepth=1`, `vrpace depth on|off`; host-verified: see FLICKER_REFERENCE's run-8 entry).
The open question is only whether VD's SSW uses the depth. The rest of this section is the design as built.

- **What VDXR does with depth** (mbucchia/VirtualDesktop-OpenXR, `frame.cpp` and `session.cpp`): it
  forwards `XR_KHR_composition_layer_depth` to OVR as `ovrLayerType_EyeFovDepth` by default
  (`quirk_use_depth`, true unless built with IGNORE_DEPTH_SUBMISSION). The log already shows the
  extension OFFERED.
- **Why it should help:** with depth, VD's SSW can reproject positionally. Without it, it guesses motion
  from images, which fails when running.
- **Plan:**
  1. An ini key read before `xrCreateInstance` (the extension must be enabled there), default off.
  2. Per-eye depth swapchains (D32_FLOAT or D16_UNORM, whichever the runtime lists).
  3. Each present: the fresh eye's depth from its per-eye depth copy (abs, units * m-per-unit to device
     depth with the submitted near/far); the rebuilt eye's from the compose pass (SV_Depth into the held
     eye's depth image).
  4. Chain `XrCompositionLayerDepthInfoKHR` on each projection view.
- **Test on the simulator first:** does xrsim accept depth layers? Then one headset question: running
  with SSW, with and without depth.

### 2. DLSS on moving characters: object motion

- **Why it smears:** the game renders no velocity buffer. `[SystemSettings] MotionBlur=False` ships from
  the developers ("motion blur is unwanted"), and UE3 only draws velocities when motion blur is on. The
  camera-only vectors are wrong for every moving character, so DLSS smears them.
- **Options:**
  - (a) Turn on `DlssMask` (the colour-change bias mask, already built, off in the tester's ini) and
    A/B it: `dlss mask on`.
  - (b) Research: MotionBlur on with the blur amount at zero, so UE3 renders the velocity buffer. Find
    the G16R16F eye-size target (the depth probe lists one, fmt 112) and feed it to DLSS. Under AFW an
    eye's history is two ticks old, so object vectors need doubling (the UEVR AFW guide does exactly
    this).
  - (c) Hands only: vectors from the controller poses (the foreground mask names the texels).

### 3. The sword's shading

The held eye takes the hands from the other eye, whose view-dependent highlights differ.
- `afw ownhands` exists (off). It gained little on the run-7 captures, and the host test caught it
  lagging a moved weapon.
- Now that the foreground mask exists, a better validation is possible: the held-body candidate's mask
  AND the fresh mask at the same target texel, plus depth. Measure with the replay on a capture with the
  blade in view.

### 4. Smaller

- The crash fix covers `ObjClassName`/`FpAssetName`. Other raw reads in the hands scan still rely on
  `RangeReadable` alone (TRAPS: "A range check is not a read guard").
- The DLSS host test needs the NGX SDK (`tools\fetch-ngx.ps1`), which this worktree lacks.

## How to verify anything here

- **Captures:** F10 > Display > Stereo rendering > "Capture AFW frames for diagnosis" (16 presents, local
  `dumps\afw-*`).
- **Replay:** `tools\afw-replay.ps1 -Capture <dir> -Fg <deg> [-Tint]`. It scores the rebuild against the
  next native frame. Only still frames are comparable: head motion inflates the score.
- **Host:** `tools\afw-warp-host.ps1` (32 cases, ray-traced, with negative controls).
- **Everything measured so far:** `docs/dishonored/FLICKER_REFERENCE.md` (top entries),
  `docs/dishonored/PERFORMANCE.md`, `docs/dishonored/PLAN-afw-run5.md`.
