# VR-70: cinematic physical head tracking

Branch: codex/vr-70-cinematic-head-tracking, based on current VR-Main cccb1815
(PR #53). Ticket verified In Progress; VR-43 and open PR #12 are related.
No permission to merge this work.

## Completed before the first boat test

- Reconciled Linear: VR-96 already Done with High priority retained; initial
  fault owner and nine refused/57 valid updates/11 clean pauses recorded.
- VR-98 remains Done; ten note cycles across reload, 0-16 ms, recorded.
- Created completed follow-ups VR-100 (startup mono gate) and VR-101 (reload
  right-eye R/0 repair), linked to merged PR #53. These fixes are not reopened.
- VR-102 tracks the still-open weapon startup freeze. CacheNameLookups stays off.
- Posted one batch project update. No release or milestone created.
- Read PR #12 body and history; no comments/reviews exist. Its presentation
  controls do not implement head tracking, and its later menus-on-panel
  commit is not part of this work.
- Derived native final camera cache fields independently through both getters.
  Added a read-only trace. No new camera writer or change of gameplay policy.

## Measured boat run (2026-09-13)

Installed build 218-ge5c7653f, compiled 19:34:31, was matched to the banner
before interpreting the log. Current and previous logs are archived under
build/cinematic/playtest-20260913-194011 (ignored). Current log SHA256:
e5b8e05a003223450a14fe6225fc75231980e39acf3092186b6bd466c5e6e69e.

The tester turned the head in all directions; deliberate lean and right-stick
comparison were not performed. Do not infer acceptance for either.

The uninterrupted initial full-animation interval is 281 samples (#39-319),
29.907 seconds, with menus clear, StatePlayerMasterSoiree, influence 1/0/0,
projection on and runtime quad off. All have live head/controller/cache samples
and 6-11 PVR writes per sample. Head pitch spans -38.82 to +23.73 degrees,
controller pitch -38.82 to +23.72, but cache pitch only -0.16 to -0.14.
Head yaw spans -90.27 to +64.74, cache yaw -72.91 to -63.14. Authored camera
translation continues. The controller receives head rotation; the authored
cache bypasses it. The cinematic latch stays off. All 25 pre-cinematic clear-menu
Walk samples have equal controller/cache rotation at printed precision.

Exclude later menu resume samples #331-335 and #387-390 from this conclusion:
zero PVR writes and runtime quad fallback make them stale. Sample #336 starts
recovering but is still on quad. Prior-render position is not same-draw evidence.

## Candidate 219 result and reference-lifetime correction

Build219-g0ebd7a3e, compile19:54:54, banner matched before interpretation.
Archive:build/cinematic/playtest-20260913-200152. Partial head look was reported,
with repeated return to the initial gaze and weaker vertical tracking. Head look
was only noticed after the right-stick prompt; no prompt event is in the trace.

The implementation incorrectly reset g_chReference on every non-double draw.
The run has33 entries:initial anchor plus32 avoidable reanchors with clear menus,
quad off,same camera/controller/pawn and unchanged load epoch.20 resets align
exactly with logged SINGLE(no present since the previous draw); rate-limited
logs cannot independently identify each of the remaining12. All2311 scoped
writes restored successfully with zero refusals. The one-second retry delay
also applied after successful captures and extended the untracked gaps.

A separate earlier override exists:Walk samples157-159 show cache pitch fixed
at-26.59 while controller/head pitch moves8.59 to7.90, despite influence0/1/0.
Other Walk samples match PC/cache, including later boat samples195-212. Therefore
Walk or player influence alone cannot prove normal camera ownership. Look-lock
flags are added read-only to the next trace; this earlier phase remains open.

## Current candidate and next test

[Cine] HeadLook remains default off and enabled in the installed test profile.
F10 View and cinehead on/off provide live A/B. Trace stays on.

Reference lifetime is now independent of draw cadence. Single and double scene
draws both apply head rotation; a single uses centered eye separation. Missing
scene/runtime/pose evidence holds the reference without writing. Actual menus,
owner changes,disable or known return from animation ownership reset it.
Successful capture has no one-second throttle; failed refreshes still back off.
The positive full-animation influence selects the overlay regardless of the
pawn state name or tutorial prompt. Partial blends hold; player-only ownership
keeps the existing writer, with the earlier override still under investigation.

Authored * inverse(entry head) * current head preserves authored movement. One
head sample and composed stereo right axis serve both draws; existing position
tracking keeps its frame and a frozen request. Fresh-entry/resume live table,
current object slots,retained class/full FName and possession guard every new
write/restore. Scope end restores fields/provenance only if still ours.

One question:once the boat scene is visible, does looking left/right and up/down
now stay where the head points instead of repeatedly returning to the starting gaze?

Begin looking gently as soon as the boat scene appears,keep the stick untouched,
and quit normally afterward. No deliberate lean is needed.
- Stable directional looking supports the reference-lifetime correction.
- Continued recentering or weak pitch rejects that correction as sufficient.
- An earlier locked interval with stable later tracking isolates the additional
  camera-override phase; inspect the new reflected input-lock trace.
The agent checks the new installed banner and reads/archives the logs. Stick,
lean,normal gameplay,transitions and other cinematic paths remain unverified.
No merge without explicit approval.

## Remaining acceptance work

Use the positive boat/scripted camera state and verified owner. Exclude pause,
notes, load and attract/menu scenes; runtime quad fallback is not that identity.
Compose physical head motion over the authored camera and preserve stick input,
authored pitch/roll/translation and cuts. The draw-scoped cache overlay is the
candidate; restore before the next authored update, use one coherent sample
for both eyes and preserve pose-record provenance. Validate current IsLiveObject
and retained identity on every engine writer, refreshing for new load objects.
Do not lift the direct controller fallback guard or modify pawn locomotion.

Ship the behavioral lever off with a live A/B. Test production composition
math/policy and identity/restore cases offline, then build/install and ask one
focused headset question. Normal gameplay and transition checks follow as
separate launches. Keep VR-70 In Progress until behavior is confirmed. Never
merge without explicit approval.

## Candidate validation

`tools/cinematic-head-host.ps1`:24 rotation/policy checks and10 scope checks pass.
The new regression drives32 temporary holds with changing yaw/pitch and retains
one anchor; a real menu/resume captures exactly one new reference.
The scope harness extracts the production begin/end bodies and mocks only
engine dependencies; it checks restoration, foreign writes, dead/replaced
identity, wrong-thread cleanup and failed preparation. This does not prove
engine rendering consumes the scoped rotation.

32-bit RelWithDebInfo build,9 undecorated exports,lint and INI golden pass.
Standalone xr_hello32 simulator smoke:60 frames,0 errors; no game launch.
Existing DVR_CAT macro warnings remain. The installed HeadLook override and
complete INI byte/config diff are recorded in build/cinematic/latest-install.json.
