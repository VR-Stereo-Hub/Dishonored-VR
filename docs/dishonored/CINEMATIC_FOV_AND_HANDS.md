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
