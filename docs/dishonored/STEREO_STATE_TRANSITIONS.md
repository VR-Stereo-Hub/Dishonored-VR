# VR-103: cinematic and dialogue stereo states

## Baseline and evidence, 2026-09-13

PR #54 merged as 39a68a50 after headset acceptance of cinematic head look,
natural pitch and native border removal. VR-70 is Done. This work branches from
that current VR-Main as codex/vr-103-stereo-state-transitions. No approval to
merge VR-103 has been given.

Surface: both headset eyes lose scene disparity when a cinematic or dialogue
is classified as non-gameplay. This is the mono-interruption route in
FLICKER_REFERENCE, not an eye-tag ordering diagnosis.

Accepted build222 cf0d00dc, compile20:37:01, archived in ignored
build/cinematic/playtest-20260913-205002. At tick26282156 strict permission
became false solely from the cinematic latch. InDialog followed; the runtime
quad came on26282187. At boat handoff26260515, a false latch parked stereo;
Soiree pending Walk became Walk26261281, with no engine locks. The two-second
latch expiry restored strict permission26262515. Another handoff repeated it.
During the dialogue, PVR also went silent while InDialog and animated camera
influences remained current; it was classified LOADING26302531. Strict returned
26308750. These are measured gates; assigning each instant to a visible prompt
still requires a focused playtest.

## Script findings and candidate

Local decompiled declarations: StatePlayerMasterSoiree carries Matinee and
Soiree control state. StatePlayerMasterInDialog and InScriptedChoice inherit
Choice_Base, whose m_DialogState distinguishes Listening(0) and Choosing(1).
InDialog additionally carries active choice and look-at constraints. InStore
shares the base but is intentionally excluded. These are declarations and
native-backed defaults, not native function bodies. No game-derived files
are committed.

[Cine] StereoState=0 ships off. F10 View: Stereo cinematic/dialogue states;
live seam: cinestereo on|off; Save As Defaults persists it. When enabled,
DvrSceneVerdict serves both runtime presentation and the once-per-tick scene
reentry decision. Strict gameplay remains the input/aim/performance gate.
The runtime implementation and eye-tag pairing order are unchanged.

A live pawn and no menu/note are mandatory. Strict gameplay passes as before.
Otherwise a fresh live-object-checked FSM snapshot must name Soiree, InDialog,
or InScriptedChoice with a live view or recently advancing scene-camera
uploads. Walk requires the normal live-view qualification and only avoids the
cinematic latch's delayed clear. Unknown states and stores receive no override.
Scene activity expires after150ms, matching the existing FSM freshness bound;
this is a conservative policy limit, not a measured game constant. Menus or
invalid owners erase activity freshness. The per-tick c5 and Present guards
still apply. The old broad SceneLive fallback is unavailable while this new
policy is enabled so it cannot defeat the explicit exclusions.

m_DialogState is resolved by property name and read from the already validated
current state object. It is diagnostic, not a new input permission. Logs print
master, enum, strict verdict, view liveness, cinematic latch and c5 freshness
on changes. No new engine-memory writer or retained engine pointer is added.

## Validation and next test

24 x86 production-policy checks cover the three cinematic states, PVR silence,
stale state, missing pawn, menus/notes, Walk handoff, store and unknown states.
Build/lint/export/golden and standalone simulator results are recorded in the
PR and install manifest. No game launch by the agent.

First playtest: enter the previously failing conversation, wait at its dialogue
choice for a few seconds, select it, and let the scene return to gameplay.
One question: does the world retain stereo depth throughout that whole sequence?
Continuous depth supports the shared presentation policy. A mono interval means
another state or liveness guard still owns part of the transition; inspect the
state logs before widening it. Confirm the installed banner and archive both
logs before the next launch. Boat/Emily and menu/load controls follow separately.

Latest install identity will be in build/stereo-state/latest-install.json.
New candidate is not yet headset-confirmed. VR-75's broader cinematic behavior,
VR-102's startup weapon freeze and general state/lifecycle replacement are not
part of this candidate.

## Installed candidate225

Commit d015b1aa, build vr33-hands-working-225-gd015b1aa, compile21:05:03.
Release build passed; lint clean;9 undecorated exports; golden INI unchanged.
Standalone x86 xr_hello ran60 frames, FOCUSED, zero errors. This is runtime
bring-up validation, not an in-game stereo assertion. Installed DLL SHA256:
4f287a1af28bb9d00bb3bfb994848ecd6c091665bfacf5483a0f60e719ad9618.
Full INI byte/settings comparison finds only Cine.StereoState absent->1;
CRLF verified. Previous DLL, INI and both logs archived at ignored
build/stereo-state/install-20260913-210627. No new launch/banner yet.

## VR-103 headset acceptance, 2026-09-13

Candidate225 d015b1aa21:05:03 verified against installed manifest. The reported
boat, Emily prompt, dialogue-choice and gameplay handoffs are headset-confirmed
continuous stereo. Log shows InDialog Listening0 and Choosing1 remain STEREO,
including view=0 with fresh scene uploads. Walk handoff stays STEREO despite
latch=1. Runtime quad transitions later correspond to notes and menus. Clean
unload. Both logs archived in build/stereo-state/playtest-20260913-212231.
The user explicitly approved PR55 merge. Cinematic FOV shrinking and native
hand/arm animation are separate follow-up work, not failures of this result.
