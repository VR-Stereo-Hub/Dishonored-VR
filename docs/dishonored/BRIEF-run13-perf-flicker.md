# BRIEF: run 13 - the frame-rate drop, AFW's general flicker, the arms lens (VR-39)

For a new session. Evidence is saved locally (game output, never committed) in
`build\playtest-candidates\vr-39-aer\run13-evidence\`: the logs of builds 189, 191 and 195, and the ini.

## 1. The frame-rate drop - FIXED in `claude/vr-39-hands-world-fov` (needs the headset to confirm)

**Measured.**
- Same place, same settings (re-entry, DLAA off, the 1.0.2 rendering defaults):
  - build 191 (`v1.0.1-191-g931409ef9`): 233-250 presents/s (about 120 fps);
  - build 195 (`v1.0.1-195-g1287f7931`): about 180 presents/s (about 90 fps).
- `git diff 931409ef9 1287f7931 -- src`: only `arm_follow.cpp` (+92, `ArmsLensTick`), `fov_lever.cpp` (the
  AFW feed) and `fwd.h`.

**Cause.**
- `ArmsLensTick` ran inside `ArmFollowTick`, which runs on every ProcessEvent dispatch.
- Every 128th dispatch it searched a 4096-object GObjects slice, each object with `RangeReadable` and a
  class-name lookup.
- It never found a `DishonoredPlayerSkeletalComponent` instance: no `armslens:` line in the whole run.
  The filter skips objects whose name number is 0, which may be why. So it never stopped: hundreds of
  slices a second on the game thread.

**Fix.**
- The search runs only while `[Screen] HandsAtWorldFov` is on, at one slice per second at most.
- After one full sweep without a match it stops and logs
  `armslens: no DishonoredPlayerSkeletalComponent instance found in a full GObjects sweep`.

**Verify.**
- Same spot, same settings: presents/s back to 230-250 under re-entry.
- One `armslens:` line either way: it found the lens, or it gave up.

**Rule for next time.** Nothing may search GObjects from the ProcessEvent path without a time budget and a
stop condition. The arm-follow lens code (`ArmFovTick`) had the same shape, but it only runs with the
counter-yaw lever on.

## 2. The hands at the world's FOV - does not work yet

**Measured.**
- Moving the camera's FOV target to 103 (build 191) did not change the hands (reported, and the lever
  target toggled 103 <-> 108 in the log).
- Replay of the run-12 capture: the hand band is right at 108.07 (2.10% differ) and wrong at 103 (12.46%).
  So the arms stayed at the headset-derived FOV.

**Why build 195's lens write did nothing:** the component was never found (above).

**Next.**
- Find the arms' lens differently. Either:
  - drop the `nnum == 0` filter;
  - take the component from the pawn (the player mesh is the pawn's `Mesh`; `hands/fp_mesh.cpp` already
    finds it: `FpCollect`); or
  - use `FindPropOffset("DishonoredPlayerSkeletalComponent", ...)` on the pawn's own component pointer.
- Then log `m_bUseFOV` and `m_FOV`.
- If `m_bUseFOV` is 0, the arms use neither the camera FOV nor the lens. Find what projects the foreground
  DPG instead: `tools/ue3-natives.py ... natives --grep FOV`, and `propwatch` on the pawn.

**Safety net already in place.** AFW is fed the headset-derived FOV for the arms while their lens is
unknown and the switch is on, so the oscillation cannot return.

**F10:** the switch is Comfort > Field of view > "Hands and weapon at the gameplay FOV" (reported as hard
to find: move it to Display, next to the FOV slider, or make its effect visible).

## 3. AFW's general flicker (build 195)

**Reported:** the hands no longer flicker, but there is a general flicker.

**Changed since the last good AFW report (build 184/189):**
1. The basis limit went from 10 to 75 deg and the turn limit to 5 + 2 x yaw (`b122bd7e2`). A matrix pair
   that is genuinely wrong now passes, so the world could jump between the two models per present.
2. The one-source disocclusion fill (`b122bd7e2`).
3. The camera's FOV target at 103 while the arms stay at 108. The foreground depth limit then classifies
   by one FOV and reprojects by another.
4. MSW code is present but off.

**Plan.**
- Take a capture during the flicker (F10, "Capture AFW frames") and replay it with `-Tint`.
- Look at `matrixVerdict` per present in the dump headers, and at the `afw/warp: beat` "world by the GAME
  matrices N, refused ..." counts in the log.
- A/B each change: `afw matrices off`; `HandsAtWorldFov=0`; revert the fill.
- Record the result in FLICKER_REFERENCE (symptom row, measured cause) in the same commit as the fix.

## Branches (all drafts, nothing merged)

| PR | Branch | What |
|---|---|---|
| #159 | `claude/vr-39-afw-polish` | AFW fixes |
| #161 | `claude/vr-39-mod-spacewarp` | MSW |
| #162 | `claude/vr-39-dlss-object-motion` | DLSS object motion |
| #163 | `claude/vr-39-hands-world-fov` | hands FOV, off `staging` |

The installed test build is the LOCAL branch `local/test-vr39-msw-objmotion` (all four merged; not pushed).
