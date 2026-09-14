## Current state: head-based movement239 installed

Installed239-gadebc947 (Sep14 07:44:40), HeadBasedMovement=1. Full INI
diff adds that key only; hashes/CRLF verified and both logs archived.

Cinematic237-g563e14d6 confirms the mono handoff fix. Character movement drift
persists; VR-109 remains open for that part. VR-110 adds a selectable, saved
head-based mode that uses native view facing, preserving the character option.
Read CINEMATIC_FOV_AND_HANDS.md latest section for evidence and the single test.
Current branch codex/vr-50-cinematic-fov-and-hands, draft PR56. No merge approval.
PR57/58 are preserved and need the accepted parent correction before new tests.
Never launch the game; no subagents. Agent builds/installs and archives logs.
Canonical active build: build/playtest-candidates/installed.json.

## Earlier records

## Current state: VR-109 correction installed, headset test pending

Installed: 237-g563e14d6, Sep14 07:27:03 (cinematic correction, NOT the
standing candidate237-gfd7a830d). Full INI diff empty; both logs archived.

2026-09-14. Verified build234 (23:11:43 Sep13) playtest confirms cinematic roll/
pitch comfort, FOV and free look. New VR-109 tracks brief mono exits and diagonal
movement after cinematic ownership. Fix extends draft PR56 on
codex/vr-50-cinematic-fov-and-hands. No merge approval. Read the latest section
of docs/dishonored/CINEMATIC_FOV_AND_HANDS.md before continuing.

PR57/build237 and PR58/build240 remain preserved, unmerged and untested. They
predate this correction: propagate an accepted parent fix before their next
builds/tests. Never launch the game. No subagents. One question per launch.
Agent archives both logs, verifies the banner and installed hash, builds/installs
and checks the full INI diff and CRLF. Canonical install manifest is
build/playtest-candidates/installed.json. Mantling still lacks explicit acceptance.

## Previous session records

## Current candidate: upright cinematic tracking

Branch codex/vr-50-cinematic-fov-and-hands; draft PR56 remains unmerged.
Build232 confirms the FOV exit improvement. Mantling remains UNTESTED.
VR-105 now includes authored roll suppression and correction of tilted-axis
swivel while holding the Empress. Scripted dialogue tracking uses a single
final camera owner across influence blends. Physical pitch/roll, native yaw,
and camera height remain. Read CINEMATIC_FOV_AND_HANDS.md latest follow-up.
Exact install identity: build/cinematic-fov/latest-install.json.
Next test: Empress scene, physical left/right and up/down look without orbit,
roll forcing or yaw lock. One question per launch. Agent reads/archives logs.
Never launch the game. No subagents and no merge approval.


Installed build vr33-hands-working-234-gcd0ee5f9, compile 23:11:43.
DLL SHA256 b0f312b4e777bdac59d1e513fa29e704050c7bf007a1e9b22e32d2daf80180f4.
Full INI diff adds only Cine.LockRoll=1; CRLF verified. Prior DLL/INI/both logs
archived in build/cinematic-fov/install-20260913-231230. Release,39 head math/
ownership checks,13 scope checks, lint, exports, golden INI and standalone XR
60 frames FOCUSED/zero errors pass. No game launched. Headset test pending.

## Previous records (latest candidate above supersedes earlier plans)

## Combined build steering, 2026-09-13

The user explicitly requests both FOV suppression and native cinematic hands/arms
in the same build. LockFov and CinematicHandBack are implemented and installed
ON together for the next test. The earlier separate-build plan below is superseded.
Current branch codex/vr-50-cinematic-fov-and-hands; VR-50 and VR-104 In Progress.
Read CINEMATIC_FOV_AND_HANDS.md (under docs/dishonored) for current scope and test.
No new merge approval. Never launch the game; no subagents.

## Latest continuation: cinematic FOV and hands, 2026-09-13

PR55 merged823c53d5; VR-103 Done and headset-confirmed. VR-50 branch is
codex/vr-50-cinematic-fov-and-hands. Read CINEMATIC_FOV_AND_HANDS.md first for the
FOV candidate and queued VR-104 native handback. FOV is the only first-test
behavior change. Latest install: build/cinematic-fov/latest-install.json.
No subagents, never launch the game, no merge approval for new work.

## Current state (2026-09-13): PR54 merged; VR-103 candidate

Cinematic head look, pitch and black borders are headset-confirmed and merged
in PR54 (39a68a50); VR-70 is Done. New VR-103 is In Progress on
codex/vr-103-stereo-state-transitions from that merge. The candidate separates
stereo scene eligibility from gameplay/input locks using decompiled state
names and live scene activity. It is default-off and awaiting headset testing.
See STEREO_STATE_TRANSITIONS.md for evidence and the one-question
dialogue transition test. Latest install manifest: build/stereo-state/latest-install.json.
Never launch the game; read/archive logs yourself. No subagents this session.
No merge permission for VR-103. PR54 approval does not extend to this work.

## Historical handoff (superseded by current state above)

## Latest steering (2026-09-13)

PR54 head look,pitch and border removal are accepted and explicitly authorized
for merge. VR-103 is the new In Progress ticket for dialogue-choice mono and
stereo/mono handoffs. Start its new branch from the merged VR-Main,read decompiled
state declarations,retain confirmed camera behavior. No subagents this session.
Approval to merge PR54 does not authorize merging the new state work.

## Continuation update (2026-09-13, VR-70)

Linear reconciliation below is now complete. VR-96 is Done/High; VR-98 remains
Done; completed follow-ups VR-100/VR-101 and open freeze VR-102 are recorded.
VR-70 is In Progress on codex/vr-70-cinematic-head-tracking from cccb1815.
Read CINEMATIC_HEAD_TRACKING.md for the current border-removal test.
Build221 natural pitch is headset-confirmed,2770 restores,zero refusals. The
new HideBorders control intercepts only the verified native stripe query and
preserves all HUD mask values.136 x86 checks pass. Check latest-install.json
for exact installed identity. No more subagents this session. No merge
permission; PR54 remains draft. The older handoff follows for history.

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

## Deferred headset queue, 2026-09-13

Testing resumes in the next session. PR56 stays draft and unmerged. Candidate234
is preserved independently under build/playtest-candidates/cinematic-234 with
DLL, exact CRLF INI and manifest. Test this candidate first: tilted Empress scene
and free head look. Mantle remains a separate untested acceptance item.
VR-106 tracks a longstanding standing-only pitched-head roll arc on a child
branch from this branch. It must not be confused with authored cinematic roll.
The agent swaps builds, compares the entire installed INI and reads/archives
both logs; the tester only launches and reports observations. One question per
launch. No game launch or merge is authorized. Both candidates await testing.
