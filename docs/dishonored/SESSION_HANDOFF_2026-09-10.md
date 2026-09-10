# Stability fixes and reticle-aim reset: handoff (VR-69)

Merge follow-up: the user subsequently explicitly authorized completing PR #33
and merging to VR-Main in this task. The earlier division of work below is
historical; the tested-build and integration distinctions remain applicable.

2026-09-10. Purpose: finish the PR/integration process without losing the exact
combination that was confirmed in the headset. No merge was performed in this
session. Reuse the existing draft PR #33 rather than creating a duplicate.

## Current installed result

The tester confirmed that the final clamp candidate eliminated the remaining
downward-motion flicker. World and weapon head-turn stability and weapon lock
through equipment changes had already been confirmed on its immediate parent.
Brief startup settling was explicitly accepted and was not changed.

Installed DLL: `vr33-hands-working-60-g417bfad9`.
Source commit: `417bfad9`, on local branch `codex/vr-69-downward-clamp`.
Source worktree: `build/vr69-bisect/lock-merge` within this repository.
SHA-256: `1781FDAA987233F5E2E9A0A448A4CB47E1A4AE595FD2D0A99AF1C9C7C722D762`.

After that successful run, only `[MotionAim] Enabled` in the installed ini was
changed from 1 to 0 to restore native reticle aiming. The DLL is unchanged.
Exactly one byte changed in the ini; all rendering, tracking, placement and
controller settings were preserved. The next shot test with reticle aiming is
still pending; do not describe that setting change as headset-confirmed yet.

## What worked together

| System | Provenance | Result / configuration |
|---|---|---|
| World head-turn stability | Existing runtime pose-lag selector; associated mainline work in [PR #27](https://github.com/VR-Stereo-Hub/Dishonored-VR/pull/27), merge `8edb4d5b` | `[Pace] Lag=2`, `[Stereo] LagAB=0`; confirmed in the controlled builds |
| Weapon head-turn stability | [PR #31](https://github.com/VR-Stereo-Hub/Dishonored-VR/pull/31), merge `ff55f9d9`; clean port `22f275ba` applied to the diagnostic base | Normalize the hand against head generation 2, matching the rendered view; confirmed |
| Candidate recovery / weapon ownership | [PR #26](https://github.com/VR-Stereo-Hub/Dishonored-VR/pull/26), merge `9fe2af45` | Retained candidate recollection and contract work; weapon swaps lock correctly in the reported run |
| Persistent stereo weapon flicker | Diagnostic `b3a1ff46`; integration `6ecda3a7` | Restore the pre-#26 eye decision while retaining the working pose/contract changes; confirmed |
| Flicker during downward character movement | Diagnostic `417bfad9`; integration `5581ed46` | Preserve camera offset ownership through the existing Z clamp; confirmed |
| Crossbow shooting upward/backward | Installed ini only | Disable `[MotionAim] Enabled`; native shot direction is no longer redirected to the hand ray on the next launch |

## Fix 1: queued draws were losing their eye correction

The first controlled build was #26 plus the clean weapon-pose port, pinned later
as `08cbb368`. It kept head turns stable but reproduced outward weapon flicker
in both eyes. Its log reported 8,538 unknown-eye placement evaluations out of
143,598 while weapon contracts remained active.

`MpEyeForPresent` was reading `g_sdDoublingNow`, the game thread's current
decision. A newer single-draw tick could clear the eye state and history while
an older stereo draw was still queued. This guard ran even when
`PaletteEyeAlternate=0`; that earlier toggle test had not excluded it.

The controlled fix restored `MpEyeForPresent` and `MpWorldTarget` to their exact
`b38519c3` function bodies. It retained the clean pose-history consumer. The
experimental eye functions remain under `src/legacy/vr69`; the inactive pass
audit was removed because it had no comparable population in the observed run.

The real production function fails five host assertions before restoration and
passes all nine afterward. The headset result confirmed stable lock through
weapon switches, with a residual specific to downward character movement.

## Fix 2: the Z clamp broke the offset writer's ownership check

The remaining flicker occurred during crouching, descending slopes and falling.
Moving the tracked head/controllers vertically, uphill movement and jumping did
not reproduce it in the report.

`FovLeverApply` clamps camera Z before `camera::apply_offsets` runs. The offset
writer recognizes its own previous write by comparing all three coordinates.
A Z-only clamp broke that match while X/Y still contained the old eye offset.
The next write treated the already-offset position as a new base and could stack
eye offsets. The clamp tightens downward and releases upward, matching the
reported directional distinction.

The correction records camera/field identity and reconciles a clamp only when
the entire pre-clamp value exactly equals the writer's previous value. It updates
the remembered Z and mod offset so the original base survives. Fresh engine
vectors and other objects/fields keep their previous treatment. No per-axis
guess, new eye heuristic, ceiling-height change or smoothing change was added.

Runtime changes are limited to camera.cpp, camera.h and one call in
fov_lever.cpp. Nineteen production-function tests pass; substituting the original
raw clamp fails six. The successful headset run identifies `417bfad9` and logs
all eight bounded `camera/clamp-rebase` entries, proving the protected sequence
executed. The tester then confirmed the visible result. The final palette-eye
summary still contains 10 unknown evaluations and one ambiguity, so acceptance
is visual stability, not forcing every counter to zero.

## Crossbow reset: configuration, not a code rollback

Diffing motion_aim.cpp between `b38519c3`, older `e8ca4682`, and installed
`417bfad9` shows no change. Later aim-ray commits `7b200e38` and `01fef2ac`
belong to the parked experimental branch and are not in the installed build.

The successful stability run resolves `motionaim=1`, left hand, pitch offset 40.
It records an arrow redirected from view `(-0.81,-0.59,-0.09)` to hand
`(-0.51,-0.05,0.86)`, then again to `(0.55,0.32,0.77)`, followed by velocity
steering. The upward/backward result is therefore supported by the log.

Set only:

```ini
[MotionAim]
Enabled=0
```

MotionAimTick returns immediately when disabled; the script fire-window arming
also requires it. On a fresh launch the steering window/queue starts empty.
This returns projectile aiming to the game, including the crossbow. It does not
disable controller-driven weapon placement or change `[Hands]`. Blink controller
aim was already off and was left unchanged. Other projectile weapons using this
same motion-aim override also return to their native aiming behavior.

The repository's generated MotionAim default remains 1. Carry the tested explicit
0 into the delivered configuration; do not assume a fresh/default ini matches
this machine. Changing global defaults was deliberately outside this final,
configuration-only reset. Confirm `config: motionaim=0` and normal reticle shots
on the next launch.

## Branches, PR and the integration boundary

Existing draft: [PR #33](https://github.com/VR-Stereo-Hub/Dishonored-VR/pull/33).
Integration branch: `claude/vr-69-combined-stability`, based on `ff55f9d9`.
Pushed commits before this handoff:

- `244ba729`: provenance review and first comparison documentation.
- `6ecda3a7`: restored render-side weapon eye decision and regression harness.
- `5581ed46`: downward-clamp correction, tests and evidence.

Diagnostic chain: `9fe2af45` -> `08cbb368` (clean weapon-pose port) ->
`b3a1ff46` (eye restore) -> `417bfad9` (clamp correction).
The diagnostic commits are local and reachable through the worktree branch.
The two behavior fixes are already ported to the integration branch; do not
cherry-pick them a second time.

**The installed, confirmed diagnostic is not the mainline integration binary.**
At handoff, `git diff --stat 417bfad9 5581ed46 -- src` includes 27 files,
2,364 insertions and 121 deletions. Later mainline includes pose/performance
instrumentation, resolution authority changes, and #27's stowed-contract
retention. The candidate/eye changes were mixed within #26, and additional
contract work also landed in #27; file-level provenance matters here.

Before merging, compare the intended integration with the confirmed build and
verify the chosen final binary/configuration. In particular, prevent automatic
diagnostics from changing the validated pose settings: mainline has a
`[Stereo] LagAB` fallback of 1 and generates `[Perf] Ab=1`; the successful profile
explicitly disables LagAB. Preserve `[Pace] Lag=2`, `[Hands] PoseLag=2`, keep
`PoseLagAb=0`, and keep performance A/B off for the stability comparison.
The installed build resolves PoseLag to 2 through its compiled fallback.

Update the existing PR's pending clamp verdict with this confirmed result, keep
the reticle shot verification distinct, and complete the final integration gate.
No branch was deleted. PR #32 was already closed and was not reopened.

## Artifacts and validation

All paths below are repository-relative and ignored local artifacts:

- `build/session-handoff-2026-09-10/`: confirmed DLL, successful log, pre-aim
  ini, `dishonored_vr.reticle.ini`, and a hash manifest.
- `build/vr69-bisect/eye-restore/`: the earlier confirmed `b3a1ff46` DLL.
- `build/vr69-bisect/downward-clamp/`: installed candidate and source manifest.
- `build/vr69-bisect/before-downward-clamp/`: pre-clamp rollback DLL/ini/log.
- `build/vr69-bisect/first-result/` and `eye-restore-result/`: previous evidence.

Successful log SHA-256:
`E2671E9DF85ACEC578B7BF67B31B1726BB282649E7A35AFA4F0E18A2E962DE15`.
Post-reset ini SHA-256:
`FF600F74A76CC1EC2C9E011BCAE47D1BA19A8886883CEF9F9BD266C15B87006A`.

Validation completed: candidate and integration release builds; legacy build for
the eye-restore baseline; nine eye tests; 19 clamp tests; 88 existing frame tests;
nine undecorated exports; lint and diff checks; installed x86/hash verification;
and the reported headset comparisons. No simulator game run is claimed.
The final aiming change needed no rebuild and passed an exact one-byte ini diff
plus unchanged-DLL hash check. Logs/captures are not to be committed.

Related detailed notes: LOCKON_REVIEW.md, DOWNWARD_CLAMP_REVIEW.md,
ENGINE_NOTES.md and VERIFICATION.md. Leave the accepted startup settling alone
while delivering this confirmed stability result.
