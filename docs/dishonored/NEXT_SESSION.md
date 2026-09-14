## Continuation update (2026-09-13, VR-70)

Linear reconciliation below is now complete. VR-96 is Done/High; VR-98 remains
Done; completed follow-ups VR-100/VR-101 and open freeze VR-102 are recorded.
VR-70 is In Progress on codex/vr-70-cinematic-head-tracking from cccb1815.
Read CINEMATIC_HEAD_TRACKING.md for the current test and evidence. Build219
partially worked but reset the gaze32 times because single draws discarded its
reference. The revised candidate retains it across pacing gaps and supports
single/double scene draws.34 host checks pass. Earlier player-influence camera
override remains open,with look-lock diagnostics added. Read latest-install.json
for actual installed build. Stick,lean and gameplay transitions remain pending.
No merge authorization. The older handoff follows for history.

# Next session: physical head movement during cinematics

Read ../../AGENTS.md and ../../CLAUDE.md first, then STATUS and the latest
FLICKER_REFERENCE. Personal rules also live in the global Codex AGENTS.md. The
tester launches the game and reports; the agent builds, installs, arms diagnostics,
verifies the banner and reads/archives the logs. Never launch the game yourself.
Use one question per launch. Preserve full installed INI bytes/CRLF and compare
all settings on every install. Engine writers require current IsLiveObject plus
retained identity checks. Commit/PR/merge text has no trailers or attribution.

## Starting state

The prior branch was merged as PR #53 (branch commit dbf61fa7). Start from the
updated VR-Main; confirm its merge commit from git/GitHub before beginning. VR-96's crawl-release corruptor is fixed
and headset-confirmed: nine stale updates refused, 57 valid updates, 11 pauses,
zero exceptions, clean exit. Startup stereo, fast notes and reload-eye repair
are also confirmed. Do not reopen the disproved tag-drain theory.

The complete installed profile is now release/dishonored_vr.ini and generated
by WriteDefaultIni, including saved F10 controls, calibration, diagnostics and
D:\dvr-data. Keep installed overrides. Name cache remains OFF. The first-fault
full-memory capture stays armed in case another GC signature recurs. Latest
install manifest is ignored build/crash-triage/latest-install.json; read its
actual build/hash before interpreting the next log. Full dumps are local only.

## Feature to implement

During cinematics the view stays at a fixed position when the tester physically
moves their head. Right-stick turning works. First make physical HMD movement
work during cinematics, including identifying whether position, orientation or
both are suppressed. Preserve the game's authored camera motion and existing
right-stick behavior; do not substitute an unrelated free-camera mode. No
cinematic implementation was made in the prior session.

Use the existing game-state instrumentation to establish which flags identify
the cinematic and which writer/layer owns the view. Trace game state -> authored
camera -> HMD offset -> capture/runtime pose. Existing cinematic policy may
already intentionally suppress the missing movement; measure before removing
safety gates. Do not treat pause menus, notes and loading as cinematics.

Relevant starting points:

- game/dishonored/commands.cpp: GameStateTick and CINEMATIC verdict.
- game/dishonored/head_track.cpp: cinematic hold / head tracking ownership.
- game/dishonored/anim_state.cpp and anim_state.h: current script state snapshots.
- core/vr/openxr_runtime.cpp: g_cineDrive, CineDrive::Authored, cinematic quad/
  stereo choice and submitted pose. Paths above are under src/.
- GAMEPLAY_STATE.md, ENGINE_NOTES.md, FLICKER_REFERENCE.md and DESKTOP_MIRROR.md.
- Existing GitHub PR #12 concerns HUD/cinematics. Inspect for overlapping work;
  do not merge it or reuse its older assumptions automatically.

Create or verify a Linear issue BEFORE assigning its number to code or commits.
Create a new codex/vr-<number>-<description> branch from updated VR-Main. The
prior permission to merge covered the finished load/reload branch only; it is
not permission to merge this future cinematic feature. No new milestone/release.

## Linear synchronization, first task after app restart

Linear was not exposed to the preceding task despite being connected by the
maintainer. Browser/native UI runtimes also failed. No board updates were claimed.
Read live issues and preserve existing metadata; search before creating anything.

1. VR-96: verify the merge integration closed it; otherwise mark Done after
   verifying the PR is merged. Keep the recurrence/history and High priority.
   Add the initial-fault owner evidence and successful guard test from ENGINE_NOTES.
2. VR-98: preserve its already merged status; record prompt notes across reload
   after observer-table reuse repair (five cycles before/five after, 0-16 ms).
3. Find/map startup stereo and reload right-eye follow-ups. Create missing records
   with project/milestone/Type/priority, linking the completed PR and measurements;
   do not invent new IDs or reopen VR-80's resolved cause. VR-77 and VR-99 remain
   context for underlying generation/timing, not proof of ownership of this fix.
4. Keep weapon startup freezing open. Name-cache optimization is implemented but
   untested on the headset and OFF; do not mark the freeze fixed.
5. Create/verify the cinematic physical-head-tracking issue, then branch and work.
6. Post one batch project update after reconciliation: stability fixes merged,
   complete tested defaults promoted, cinematic head motion next, startup freeze
   still open. Do not declare a release or invent a milestone.

Team VR, project Dishonored VR Mod, existing milestone
Stable - 6DOF, motion controls and alpha parity. Follow docs/LINEAR_AND_GITHUB.md.
The local prepared crash updates are under build/crash-triage; the committed
ENGINE_NOTES/FLICKER_REFERENCE contain the durable findings and hashes.
