# Final status, 2026-09-14

Cinematic comfort/FOV/free look, native hands/arms and mantle handback, continuous stereo, and head-based movement are accepted. VR-50 broader FOV cases and VR-109 character-mode drift remain open.

Merge is explicitly authorized. The complete tested defaults are promoted in
PR58. See [stack acceptance](STACK_ACCEPTANCE.md) for final settings, validation,
remaining issues and all findings. Earlier sections below are historical; their
default-off and pending-test statements do not describe the accepted stack.

---

# Cinematic FOV and native hands, 2026-09-13

## Starting point

PR55 merged as823c53d5 after explicit approval. VR-103 is verified Done.
Accepted225 d015b1aa21:05:03 covers boat, Emily prompt, conversation choices and
stereo gameplay handoffs. Archive build/stereo-state/playtest-20260913-212231
contains both logs and the installed INI/manifest. Clean unload. New branch:
codex/vr-50-cinematic-fov-and-hands from merged current VR-Main.

VR-50 already tracks shrinking FOV; it is now In Progress. VR-104 separately
tracks cinematic hands/arms. No merge approval for this branch. No subagents.

## FOV evidence and implementation

The accepted log's204 dialogue FOV handoff samples span36.6 to107.9 degrees,
against a108.1-degree VR target. Narrowest sample tick28139156. The runtime
publishes the shrinking readback. This measures narrowing, not the viewport's
pixel rectangle; a final-cache write still requires rendered/headset acceptance.

Local script declarations:
- DisConv_PlayerLookAtSpeaker defines DisConvLookAtConstraints with m_bUseZoom
  and m_fZoomPercentOnScreen. InDialog retains those constraints; actor look-at
  uses the same struct. Class defaults disable zoom, but individual native
  conversation actions may request it.
- DishonoredPlayerCamera exposes FOV target priorities, with ControllerLook
  above Item/Action/Locomotion; m_fCurFOV and m_fCurFOV_Arms are separate.
- Camera.CameraCache is a TCameraCache with TPOV containing FOV, Location,
  Rotation. This names the final scene value without guessing an offset.
- DishonoredCamera.ini exposes default75 FOV and blend speed5, but no global
  dialogue zoom suppression was found in Camera/Conversation INIs. Changing
  defaults is not a scene-specific override. Decompiled native bodies absent.

Candidate: default-off [Cine] LockFov, F10 View Suppress cinematic FOV zoom,
seam cinefov on|off, Save As Defaults. A fresh recognized cinematic state and
live projection scene permit a scoped write to reflected CameraCache.POV.FOV.
Target is the existing VR aspect-derived FOV, not a new fixed angle. Both eye
draws use it. A nested FOV lever dispatch respects the active scope's target.
After the draws, restore only if current identity and value still match.

Each ownership interval rebuilds the live-object table. Camera/controller/pawn
identity includes current GObjects slot, full FName and class; links and load
epoch are checked before each write/restore. Menu exit cannot reuse a retained
interval. No persistent game zoom target or script constraint is changed.
The host publishes the successful draw FOV to its existing FOV handoff, expiring
at150ms. Otherwise it uses the original sensor. This is a draw-request claim,
not proof of measured projection consumption; no runtime policy changes.
Frame-exact FOV metadata across queued transitions remains a limitation of the
existing host handoff and must not be described as validated by a scope test.

30 x86 FOV/handback policy and scope checks cover enable/state/menu/projection/target guards,
write and restore, nested refusal, missing identity, changed field and invalid
values. Compile/lint/golden checks pass. Headset validation pending.

## Native hands implementation, VR-104

The user explicitly requests FOV and hands together in the same build. This
supersedes the earlier separate-build plan. Both levers ship off and will be
armed in the installed INI for one combined cinematic playtest.

[Anim] CinematicHandBack, F10 View Native cinematic hands and arms, seam
cinehands on|off extends VR-88 classification to Soiree, InDialog and
InScriptedChoice. The existing250ms release and150ms blend provide native
hands/weapon placement, mesh-split/draw-suppression bypass, bone/material
visibility restoration and persistent SkelControl release. Store is excluded;
Walk resumes controller placement. StateWatch and HandBack remain prerequisites.

Control restoration now records full FName/class/slot identity. Ownership,
menu and load edges rebuild the live table. Reused retained controls request
rediscovery instead of receiving saved values. Arm cull and mesh transform
restores require IsLiveObject plus the collected candidate's full identity.
Existing measured draw-distance pair +0x1bc is named kArmDrawDistancePair in
patterns.h. No new layout is guessed. Native hands/arms remain unconfirmed
until the combined playtest; no merge approval has been given.

## Next test

Installed candidate identity: build/cinematic-fov/latest-install.json. Verify its
banner before reading the next run; archive both logs before relaunch. Never
launch the game. One question: in the previously affected scene, does the view stay full-size
while the visible arms/hands follow the game's animation instead of the controllers?
Full-size plus native animation supports both changes. Shrinking isolates a
remaining FOV/render path; controller-driven or missing arms isolates handback.
Check gameplay return in the same sequence. VR-50 broader kill-cam and manual
FOV cases remain unverified. Neither ticket closes merely from building.

## Installed combined candidate230

Both changes are enabled in build vr33-hands-working-230-g47c626a5,
compile21:40:15. 30 FOV/handback checks,43 identity lifecycle checks, release
build, lint,9 exports, golden INI and60-frame standalone XR simulator pass.
Full installed INI diff contains only Cine.LockFov=1 and Anim.CinematicHandBack=1;
CRLF preserved. DLL hash matches the build. Prior DLL, full INI and both logs
archived in build/cinematic-fov/install-20260913-214151. Manifest:
build/cinematic-fov/latest-install.json. No game launch; headset test pending.

## Combined follow-up: FOV exit, pitch comfort, mantle hands

Build230 47c626a521:40:15 is headset-confirmed for cinematic FOV suppression
and native hands/arms, with a residual shrinking square on exit. Archive:
build/cinematic-fov/playtest-20260913-220129. All31202 FOV scopes restored,
zero refusals. At tick30273250 the override released in Walk and the host
immediately claimed52 degrees; readback reached107.6 at30274375 (1125ms later).
That native exit blend explains the reported rapid expansion.

The FOV exit bridge now keeps the validated same owner's override during Walk,
Falling or Jump until the sensor is within0.5 degree of the VR target. It cannot
start from ordinary gameplay zoom. Menus/loads, invalid state/identity and an
invalid sensor cancel it. A3s bound prevents indefinite suppression if readback
stalls; the observed1125ms recovery is covered. No runtime policy change.

VR-105 clarification: suppress forced up/down TILT ONLY. Height changes,
physical HMD movement and actual climbing remain. Default-off Cine.LockPitch
uses physical absolute HMD pitch with the same clamp as gameplay. Fully authored
head-look composition replaces only its resulting pitch. Other live animation
states use a validated draw scope that retains the gameplay translation request.
Yaw/roll and position are preserved; both draws share the scope and exact incoming
fields are restored afterward. F10 View Suppress animation up/down tilt; seam
cinepitch on|off; Save As Defaults persists it.

VR-104 extension: default-off Anim.MantleHandBack uses the exact live
StatePlayerMasterMantle state and the existing native hand/weapon/arm consumers.
It does not add every locomotion state. The older explicit preference to keep
controller hands while mantling is superseded for this installed candidate by
the new request. F10 View Native hands while mantling; seam mantlehands on|off.

All three changes are combined in one build as explicitly requested. 38 FOV/
handback checks,28 head math/policy checks and13 extracted production-scope
checks pass. New checks cover convergence, timeout, stale owner, physical tilt,
unchanged yaw/roll and preserving the gameplay position request. Runtime smoke,
exports and final install identity are recorded below. No merge approval.

Next combined test: replay the affected cutscene through gameplay return, then
mantle a nearby ledge. Expected: no exit shrink; the scene cannot tilt the view
up/down but physical head tilt works; mantle hands follow the ledge animation
and normal tracking returns afterward. One question: does that sequence behave
as expected, or which part still differs? Agent reads the log, never launches.

## Installed combined follow-up

Installed candidate: vr33-hands-working-232-g4ec4f457, compiled 22:10:59.
DLL SHA256: 71681379ff190a924d6f6002863eebe4f8ea1ce7246c4b14b92e8eee170c49c6.
Full installed INI comparison: only Anim.MantleHandBack=1 and Cine.LockPitch=1
added; CRLF verified. Prior DLL, INI and both logs archived under
build/cinematic-fov/install-20260913-221229. Release, lint, exports, golden INI,
38 FOV checks, 28 head-math checks, 13 camera-scope checks and standalone XR
60-frame self-test pass. Headset acceptance of this combined follow-up is pending.
The agent did not launch the game.

## 2026-09-13: upright cinematic tracking follow-up

Build232 (4ec4f457, compile22:10:59) log banner verified before interpretation.
Both logs archived in build/cinematic-fov/playtest-20260913-230307. The tester
reports the FOV exit improvement successful; mantle handback was not tested.
Forced pitch suppression exposes a remaining tilted-axis swivel while holding
the Empress. At tick32642531 authored P/Y/R=-57.78/55.56/32.61, composed
P/Y/R=-32.21/40.92/54.40. This is smooth camera-axis coupling, not eye flicker.
Replacing pitch AFTER full rotation composition leaves authored tilt in yaw/roll.

The comfort path now constructs upright yaw as authored yaw plus physical yaw
relative to the entry reference, then applies physical pitch and roll. Authored
yaw and camera location remain active. Independent default-off Cine.LockRoll
(F10 Suppress cinematic roll; cineroll on/off; Save As Defaults) removes authored
roll, retaining physical HMD roll. LockPitch keeps its independent toggle.

Decompiled DisConv_PlayerLookAtSpeaker declares maximum pitch/yaw constraints;
DishonoredCamera_PlayerControl resets controller rotation, and camera influences
form a non-additive group. Native bodies are unavailable. The log includes942
fully animation-owned,304 fully player-owned and66 blended InDialog samples.
The prior head scope skipped the latter two populations. Scripted Soiree,
InDialog and InScriptedChoice now keep a final head scope across those influences.
A100ms lease from a successful live scope suppresses controller HMD injection,
including direct fallback, so physical yaw is applied once. Native controller
and stick changes remain; script resume references stay current. Unknown weights,
menus, owner changes, stale poses and runtime loss retain refusal/reset guards.
No new engine offsets or persistent engine-field writes are introduced.

Reported restricted movement is provisionally interpreted as head rotation;
physical lean versus rotation clarification is pending. The fix is a candidate,
not a rendered acceptance claim.39 math/ownership checks and13 extracted camera
scope checks pass, including the steep authored-axis regression and preserving
ordinary gameplay blend ownership. New one-question test: holding the Empress,
look left/right and up/down; expect no orbit or forced roll and unrestricted
physical look. A remaining orbit rejects upright composition; a yaw lock points
to ownership/constraint handling. FOV and mantle settings stay enabled.

Installed build vr33-hands-working-234-gcd0ee5f9, compile 23:11:43.
DLL SHA256 b0f312b4e777bdac59d1e513fa29e704050c7bf007a1e9b22e32d2daf80180f4.
Full INI diff adds only Cine.LockRoll=1; CRLF verified. Prior DLL/INI/both logs
archived in build/cinematic-fov/install-20260913-231230. Release,39 head math/
ownership checks,13 scope checks, lint, exports, golden INI and standalone XR
60 frames FOCUSED/zero errors pass. No game launched. Headset test pending.

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

## 2026-09-14: build234 acceptance and VR-109 regressions

Verified DLL SHA and log banner: vr33-hands-working-234-gcd0ee5f9,
Sep13 23:11:43. Both logs and the actual INI archived at
build/cinematic-regression/20260914-071943. Reported acceptance covers the
roll/pitch comfort, FOV and cinematic free look. Mantling was not specifically
confirmed. Reported regressions: about1s mono at cinematic exit and a persistent
diagonal movement direction despite right-stick view turning.

Measured stereo transitions: at752187ms Walk has view=0, fresh scene/c5age0,
live pawn, valid state and no menu. Runtime quad begins752203 and ends753265,
a1062ms interruption. Similar sequences occur at786734 and804500. The cause is
that DvrScriptViewLive measured head WRITES, which cinematic ownership now
suppresses intentionally despite live PVR dispatches. A later1048765 transition
also overlaps ClientPlayMovie/OnToggleJournal and must not be called an identical
no-menu case. No eye-tag/capture hypothesis is reopened.

Body-facing code retained its previous published target while cinematic PVR
returned early. Afterward, an old head-contribution accumulator survived the
native camera reset. Log at1063750 asks-179.5deg and faces-228.5deg, approximately
49deg apart. This supports the reported movement mismatch, but does not prove
when it first became visible.

VR-109 counts only PVR dispatches actually yielded to the live cinematic owner
as activity, with the existing750ms silence limit. It does not fabricate head
writes or loosen the menu/pawn/state stereo gates. Body-facing yields while the
cinematic owns look, refuses targets older than150ms, and revalidates current
GObjects slots, IsLiveObject, possession and full FName/class identity. The first
fresh gameplay yaw publication discards the outgoing head contribution and
seeds from the native incoming view. Later stick turns and head/body separation
continue through the same arithmetic. Existing HeadLook toggle controls this
ownership path; no new defaults or INI changes.

Validation:17 production yaw/activity regression checks,39 cinematic math and
ownership checks,13 scoped-write checks,38 FOV/handback checks, x86 build,
9 exports, lint and INI golden pass. First build exposed missing unity forward
declarations; corrected before candidate installation. Perceptual fix untested.

Next single question: after a cinematic returns control, with the headset held
forward, turn roughly90deg using the right stick and strafe left/right; does
movement now remain straight sideways relative to that facing direction?
Pass supports correct heading handoff; persistent diagonal motion falsifies the
reference reset and requires view/body/native controller evidence from this run.
Agent reads stereo transitions from the same log; no second question this launch.

## Installed VR-109 correction, 2026-09-14

Current candidate: vr33-hands-working-237-g563e14d6, compiled07:27:03 Sep14.
This is NOT standing-arc237-gfd7a830d (Sep13 23:34:10). Full hash/compile time,
not the numerical build count alone, identifies a candidate across branches.
Bundle: build/playtest-candidates/cinematic-handoff-237. DLL SHA256:
6f6aaf5d4f10adaaff67f7d774bf5392b6f0ac95ad7f1a0e3e280df4da5e39f7.
Installed INI unchanged byte-for-byte (full diff empty, CRLF checked), SHA256:
7819d090054a57518267ce01e2dc15e6c4734e1427294e9cd12463edfac94d1c.
Install archive: build/playtest-candidates/installs/20260914-072808-025185.
Standalone XR smoke also passed60 frames, FOCUSED,0 errors; no game launch.
The next launch tests movement-heading handoff. Stereo is checked in its log.
PR56 remains draft; VR-109 In Progress. PR57/58 are unchanged and unmerged.

## 2026-09-14: mono accepted, character drift remains, head-based option

Verified cinematic237-g563e14d6 (Sep14 07:27:03) and installed DLL hash.
Both logs/INI archived at build/cinematic-regression/20260914-073936.
Reported: mono handoff correction works; character-oriented movement is still
5-10deg off after the first scene and around45deg after dialogue/FOV framing.
The reference-reset fix is insufficient. Logs show resets did occur and later
native requested/body headings diverged again. VR-109 remains open for that
character-mode issue; its mono portion is headset-confirmed.

VR-110 adds Camera.HeadBasedMovement (default0, candidate1), F10 Head-based
movement and command movement head|character. Config Save persists the choice.
Head mode leaves the native FaceRotation request untouched, so the game follows
the full view heading rather than our separated body target. No input-vector
rotation, cached heading read, new offset or engine write is needed. Physical
head yaw can turn the character in this mode, by design. Existing ArmBodyFacing
and character bookkeeping remain available; switching back does not claim to
fix their outstanding drift. Cutscene-facing requests remain native as well.
The camera comfort/FOV and confirmed stereo activity changes are unchanged.

15 host checks compile the production handler and verify head passthrough across
yaw/wrap values, no stale body-state read in head mode, character-mode restoration,
cinematic/stale/dead-owner guards and other-pawn passthrough. Build, exports,
lint and generated INI comparison pass. Headset behavior still awaits testing.

Next one-question test: after the dialogue/FOV scene returns control, turn your
head left/right and use forward/sideways movement, then turn with the right
stick. Does movement consistently follow where you are looking without the
diagonal offset? Pass supports native head-based movement; a persistent offset
means the final camera and native movement direction still disagree. The agent
reads the archived run and never launches the game.

## Installed head-based candidate

vr33-hands-working-239-gadebc947, Sep14 07:44:40, source adebc94720a92076849b5e09c7846355b12ed28a.
Bundle: build/playtest-candidates/head-movement-239.
DLL SHA256: 552239ccb091d63df676f93ce0b708bf6e403267a60b014a03764c8ce57e4611.
INI SHA256: a5c8468b60b16ef3f3906dcc082d61f1d1a556ee2227ac93334a8d3b74a2f05f.
Installed/hash-verified; full settings diff adds only Camera.HeadBasedMovement=1.
CRLF checked. Previous DLL/INI and both logs archived under
build/playtest-candidates/installs/20260914-074519-801836. No game launch.
PR56 remains draft/unmerged. VR-109 character drift and VR-110 acceptance open.

## 2026-09-14: head-based movement accepted, PR56 finalized

Verified239-gadebc947, Sep14 07:44:40, against installed DLL hash and log banner.
Logs/INI archived at build/cinematic-regression/20260914-075605. Head-based
movement is headset-confirmed; log shows native facing passed unchanged.
Cinematic comfort/FOV/free look and mono handoff fixes retain their acceptance.
Character-mode drift remains open under VR-109; the selectable fallback does
not close it. VR-110 is accepted pending merge. PR56 is ready for review.
The user chose completing the remaining PR57/58 tests before merging all three;
keep them unmerged while those tests remain. Next: rebuild PR57 with this parent.
