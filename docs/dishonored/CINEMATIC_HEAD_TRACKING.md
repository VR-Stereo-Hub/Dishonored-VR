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

## Current candidate and next test

[Cine] HeadLook defaults off, enabled only in the installed test INI. F10 View
has a live checkbox; `cinehead on|off` provides the command equivalent. Trace
remains on. The candidate requires a double draw, fresh tracked pose, clear
menus/runtime projection, the Soiree master state and full animation influence.
It composes authored * inverse(entry head) * current head. Both eyes use that
same sample and the composed right axis. The existing position tracking frame
is preserved, with its request frozen across the two draws. No controller or
pawn movement writer changes. This is a rotation candidate for the measured
boat state, not a claim that all cinematics or physical translation are fixed.

A fresh live-object table is built at entry/resume; camera, controller and pawn
retain class/FName and GObjects slot identity, rechecked with IsLiveObject and
possession before writes/restores. Scope end restores the original rotation,
location and offset provenance only when fields still match our writes. Engine
recomputations and dead/reused objects are refused. Each draw derives from the
restored authored input, so head movement cannot accumulate. Pose records name
the new writer (3) and retain the exact tracked sample, not fresh globals.

One question for the next launch: does physical looking around move the view
freely while the opening boat continues its scripted movement?

Start the opening boat ride, leave the stick untouched, turn left/right and
look up/down, then quit normally. No deliberate lean is needed for this test.
- Free head look with continuous boat movement supports the rotation candidate.
- A locked view means the scoped write was refused or not consumed by rendering.
- Snapping, drift or lost boat motion rejects composition/restore behavior.
The agent checks the installed banner, archives the logs and reads the scope
write/restore/refusal evidence. Stick, leaning, normal gameplay and transitions
remain later separate tests. Do not merge without explicit authorization.

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

`tools/cinematic-head-host.ps1`:12 rotation checks and10 scope checks pass.
The scope harness extracts the production begin/end bodies and mocks only
engine dependencies; it checks restoration, foreign writes, dead/replaced
identity, wrong-thread cleanup and failed preparation. This does not prove
engine rendering consumes the scoped rotation.

32-bit RelWithDebInfo build,9 undecorated exports,lint and INI golden pass.
Standalone xr_hello32 simulator smoke:60 frames,0 errors; no game launch.
Existing DVR_CAT macro warnings remain. The installed HeadLook override and
complete INI byte/config diff are recorded in build/cinematic/latest-install.json.
