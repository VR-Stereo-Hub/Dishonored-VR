## Current: menu-motion correction and hand-following readers (2026-09-16)

- Keep local on codex/hud-weapon-dial; no push or main merge authorized.
- Verified374 report: wheel appearance/selection good; head motion flickers on hands/weapons;
  separately appears to translate menus/cutscenes. Both logs and saved INI archived under
  build/playtest-candidates/vr126-dial-immersion/reported-head-motion.
- Candidate tracks menu single scene draws, uses composed yaw for scoped translation,
  rebases entry neck correction between yaw frames, and snapshots position with head
  orientation. Accepted image orientation/stereo synchronization unchanged.
- Adds independent wheel alpha gain/floor/gamma and camera-parallel hand-following
  Note/Journal panels with independent width/distance. Existing F10 settings preserved.
- Installed376-g35a50573b from clean35a50573b; candidate vr126-menu-motion.
  Both logs/INI archived at installs/20260916-015200-720588. Full INI adds9 keys
  only, retaining all saved settings; hashes/CRLF verified.
- Check installed.json for current candidate. Read only latest HUD_ANCHORS and
  FLICKER_REFERENCE follow-up sections for evidence, limitations and launch question.
- One question: does turning the head left/right with Wheel held open and controllers
  still stop hand/weapon flicker? Headset verdict pending. Read menu/hands delayed
  independent eye comparison; old all-unknown eyecheck did not clear the classifier.
- No subagents, game/simulator launch, branch deletion or publication.

## Current handoff: VR-126 refined dial and menu immersion (2026-09-16)

- Branch codex/hud-weapon-dial; PR67/68 already merged, no HUD merge authorized.
- Build372 dial accepted and tuned: width0.350m,travel0.040m,crop0.400x0.400.
  Verified logs/INI archived in build/playtest-candidates/vr126-weapon-dial/accepted-tuning.
- New work: tiny-motion direction selection, camera-parallel panel, true circular
  feathered crop, closer/farther slider, per-menu live head look and UI blur controls.
- Installed374 (`vr33-hands-working-374-ga51e1799f`), clean source a51e1799f.
  Candidate build/playtest-candidates/vr126-dial-immersion; both logs and previous
  DLL/INI archived in build/playtest-candidates/installs/20260916-010814-146048.
  Full INI diff:16 new controls only; every previous value retained. Hashes/CRLF
  verified. HeadLookWheel/Note=1,NoBlurWheel=1,direction-only/circle=1,deadzone2mm,
  distance0; tuned width0.350m/crop0.400x0.400 retained. Not headset-tested yet.
- Current candidate identity is in build/playtest-candidates/installed.json.
  Preserve all installed settings/CRLF; compare the entire INI and archive both logs.
- Read only the current VR-126 refinement section in [HUD_ANCHORS](HUD_ANCHORS.md) for
  details, evidence, failed hypotheses, validation and the single launch question.
- Next question: with Wheel open, does turning reveal fresh world scenery beyond
  the old FOV rectangle and return normally on release? Camera and UI-blur fixes
  remain headset-unverified. Scope and effect diagnostics distinguish the outcomes.
- Standalone2185 dial checks, actual GPU mask test,16 production menu lifecycle
  checks, existing camera/FOV and HUD tests pass. No game/simulator launched.
- No subagents, source branch deletion or new main merge. User launches only.

### Minimal next-chat prompt

Continue VR-126 on codex/hud-weapon-dial. Read AGENTS/CLAUDE, current STATUS and
HUD_ANCHORS current refinement section; FLICKER_REFERENCE current menu-world entry
for camera work.372 dial accepted; preserve tuned35cm width and0.4x0.4 crop. New
candidate adds2mm direction-only selection,camera-plane orientation,circle mask,
distance offset,and per-menu head-look/UI-blur controls. Check installed.json and
verify log banner before analysis; archive both logs and preserve full INI/CRLF.
User checks head turns beyond old FOV while Wheel open. Inspect menu/head and
menu/blur; a write is not visual acceptance. No subagents or game/simulator launches.

## Performance research shelved, 2026-09-15

Historical shelving checkpoint; the VR-50 exception above supersedes installation and
playtest status. Unfinished performance tickets
are Backlog and unassigned. [PERFORMANCE.md](PERFORMANCE.md) is the compact canonical
record of measured results, rejected routes, useful tools and resumption options.
Ordinary per-eye preparation measured 1.322 ms/pair including 1.082 ms culling;
no safe work-sharing optimization is established. Default-off profilers and the
measured desktop throughput/tail experiment are retained. Failed nonblocking
Present and invalid coarse CPU-ms instrumentation are removed.
Exact installed307 remains unchanged. Research cleanup is build/host-validated;
no new install or headset acceptance is claimed. Source branches are preserved.
The user authorized a consolidation PR and merge to VR-Main; this is research
preservation, not promotion of experimental rendering settings.

## Completed merge and installed state: 2026-09-14

World-only PR #61 is integrated into VR-Main at a60516c4b.
Installed build 272-ge3bb7ac2a, compiled 14:27:16, has source identical to
that merge apart from commit/build metadata. DLL SHA256
473bca2b298a988d48307ac23a2a2b257d53f00ac2f8df83dcfa95149c3d373b.
The installed, release and golden INIs are byte-identical CRLF, SHA256
de20bd794ab0917ca5450178207a333bd2a28066f3edfac253a754b153ac5ce5.
The full installation comparison has ZERO INI changes. Both prior logs archived
at build/playtest-candidates/installs/20260914-142912-415128.

Only default promotion differs in source from the repeatedly accepted original
world candidate: no hand-normalization experiment is included. Release build,
frame tests, exports, lint and golden checks passed. No game launched.
Retain both feature branches and the exact original world checkpoint.
Next work is deferred: investigate left-eye hand jumps after opening-cutscene
exit (VR-95); sewer-save loading was reported essentially clean. Do not infer
that all save loads or all cutscenes reproduce it.

## Earlier acceptance and scope

## Current acceptance: world smoothness approved for main, 2026-09-14

The exact original world candidate 271-g8cd27652 was restored and independently
reconfirmed on the headset. Exceptional world stability returned. Its installed
INI is now copied byte-for-byte to the release and golden INIs, and the default
writer matches it. `Pace.ImageOrientation=1` is the generated and missing-key
default by explicit user request after acceptance; explicit 0 remains respected.

This merge contains the WORLD fix from 8cd27652 only. The unaccepted hand
normalization code from 12a974134 is absent. The branch
`codex/vr-116-flicker-fix-patch` preserves that experiment for later investigation.
The accepted DLL/INI bundle and `vr-116-world-smooth-271` tag remain recoverable.
The user explicitly approved publishing this accepted state to VR-Main.

Latest symptom refinement: residual left-eye hand flicker was observed after
exiting the opening cutscene, while loading a sewer save was essentially clean.
This makes transition state relevant; it does not prove the precise cause.
Track the deferred hand issue with VR-95 and preserve its historical predictor
regression. Do not resume or include hand changes in this merge.

Reconfirmation logs/INI: `build/flicker-120/reconfirmed-world`; banner and DLL
hash match 271-g8cd27652, compiled 13:46:59. Source changes beyond that checkpoint
are documentation and INI/default promotion only. No engine-memory writer,
camera transform, hand pose or frame pacing change is introduced by promotion.

Next: complete and verify the authorized main merge; hand/cutscene work waits.
Historical pending, publication-blocked and candidate text below records earlier
stages and does not override this scope or the new explicit merge authorization.

## Earlier records

## Current state: VR-116 120Hz physical-head-turn flicker investigation

VR-115 desktop branch is parked at committed1d6558a3; no uncommitted changes
needed a stash. Its build271 was never installed. Do not mix that experiment
into this investigation. New branch codex/vr-116-120hz-head-motion from main255d1c91.
Verified main build266 run shows120Hz, world/hand lag2, automatic A/Bs off.
Physical head turns alone trigger severe world flicker; stick turns and still
head do not. Both cinematic and gameplay scenes affected. Both logs archived
at build/flicker-120/20260914-133932. Ring/eye summaries remain healthy; uneven
cadence and occasional pose disagreement are measured, causality unresolved.

Default-off Pace.ImageOrientation candidate uses same-eye captured camera-record
orientation for submission, with numeric lag fallback; positions and game/hand
behavior unchanged. F10 View provides live A/B and Save. Eight new host checks
and existing frame suites pass; release,exports,lint,golden pass. No game launch.
Exact findings, rejected claims and one-question120Hz test:
[HEAD_MOTION_120HZ.md](HEAD_MOTION_120HZ.md).
Build/install identity will be recorded after packaging. No merge authorized.

## Earlier records

## Current state: standing roll accepted; VR-87 remains open

Build245 confirms roll while looking up/down. Remaining inverse vertical motion
is the existing eye-ceiling limitation VR-87, supported by joined log accounting.
See STANDING_PITCH_ROLL_ARC.md for evidence. PR57 ready, unmerged; PR58 next.
Installed build remains245. No new install or INI change. Preserve all three PRs
unmerged until remaining tests complete. Never launch the game; no subagents.

## Earlier records

## Current state: standing camera-roll candidate after accepted PR56

Installed245-g0cd7b263 (Sep14 07:57:48), standing-roll fix and ZAccount ON.
Full INI diff contains only those two changes; hashes/CRLF verified.

PR56 is ready for review and unmerged. Build239-gadebc947 confirmed head-based
movement, alongside the accepted cinematic comfort/FOV and mono handoff fixes.
PR57 now includes that parent through cb77328d. Its next build retains all
accepted settings and enables Neck.UprightPitchArc=1 plus PosTrack.ZAccount=1.
Read docs/dishonored/STANDING_PITCH_ROLL_ARC.md for the one-question test.
PR58 mono UI work remains a separate later candidate. Merge all three only after
the remaining tests finish. Character-mode drift (VR-109) remains open.
Never launch the game, no subagents. Agent installs and reads/archives both logs.
Canonical installed identity: build/playtest-candidates/installed.json.

## Earlier records

## Current state: three stacked candidates, no merge approval

The new branch codex/vr-107-mono-anchors-and-ui-state starts at d27fdf6c on
PR57's standing-camera branch. VR-107 adds configurable mono anchoring;
VR-108 and existing VR-74/VR-71 cover loading/menu stereo and input ownership.
Read docs/dishonored/MONO_ANCHOR_UI_STATE.md for implementation and test plan.
Both new levers default off; preserved build240 enables them together.

PR56 (cinematic build234) and PR57 (standing build237) remain draft/unmerged.
Keep build234 installed first; build237 and the new cumulative UI candidate
remain separately archived for agent-managed swaps. No headset testing has
occurred on these candidates. One question per launch; never launch the game.
No subagents. Build240 was installed and hash-verified, then build234 restored and verified.
The next step is deferred headset testing in order234,237,240. The agent archives both
logs and checks full INI diffs/CRLF every swap. installed.json under
build/playtest-candidates is the active installation authority.

## Earlier session records

## Current state: two candidates awaiting separate headset tests
## Current state: PR56 accepted; next is standing-roll PR57

Verified239-gadebc947 confirms head-based movement. PR56 finalized for review;
keep all three PRs unmerged until the remaining tests finish. Character-oriented
drift remains open under VR-109. Next rebuild PR57 with this accepted parent,
enable UprightPitchArc and ZAccount, and test standing pitched-head roll versus
crouching. Preserve HeadBasedMovement=1. Agent handles installs/logs, no game
launch, no subagents, one question per launch. See the latest cinematic notes.

## Previous records

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

Cinematic PR56 is draft/unmerged; build234 is preserved for the first test.
Draft PR57 targets the cinematic branch (PR56). Both remain unmerged.
Current branch codex/vr-106-standing-pitch-roll-arc was created from parent
codex/vr-50-cinematic-fov-and-hands at6f85417a. VR-106 adds a default-off fix
for the standing steep-pitch roll arc. Read STANDING_PITCH_ROLL_ARC.md in
docs/dishonored for evidence, candidate, controls and the two-build test order.
The agent manages installs and archives both logs, with full INI diffs/CRLF.
Canonical active installation: build/playtest-candidates/installed.json once
swaps run; preserved bundles are under build/playtest-candidates. Build234
must be left installed first for tomorrow. No test tonight, no game launch,
no subagents, no merge approval. Mantling still untested.

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

## Final artifacts and installation, 2026-09-13

Standing candidate237: vr33-hands-working-237-gfd7a830d, compile23:34:10.
DLL SHA256 b9cca3b0e0b77212424d25158807df66178a689f617f4d3d067e5efb975671a5.
Bundle: build/playtest-candidates/standing-arc-237 (DLL, CRLF INI, manifest).
The candidate was installed and hash-verified; full INI diff only added
Neck.UprightPitchArc=1 and changed PosTrack.ZAccount=0 to1. Both logs and prior
files archived at build/playtest-candidates/installs/20260913-233449-048351.
Then cinematic234 was restored for the first deferred test, with both hashes
verified and the exact inverse full INI diff. Restore archive ends233449-390590.
Active install is234, NOT237. Canonical record: build/playtest-candidates/installed.json.
No new game launch or game log exists for either pending candidate.

Final checks:12 x86 standing-arc regressions,39 parent math/ownership checks,
13 camera-scope checks, Release build, lint, nine exports, golden INI and standalone
XR60 frames FOCUSED/zero errors pass. Headset acceptance remains pending.
