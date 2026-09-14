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

20 x86 policy/scope checks cover enable/state/menu/projection/target guards,
write and restore, nested refusal, missing identity, changed field and invalid
values. Compile/lint/golden checks pass. Headset validation pending.

## Native hands implementation plan, VR-104

The existing VR-88 classifier already drives hand/weapon blend back to native,
mesh split bypass, draw suppression bypass, arm/bone visibility restoration,
material restoration and persistent SkelControl release. Extend its cinematic
classification rather than adding another placement path. Candidate state list:
Soiree, InDialog, InScriptedChoice; use fresh FSM identity and preserve existing
250ms release/150ms blend behavior. Store remains excluded. Add an independent
default-off live lever so FOV and hands are tested separately.

Before enabling, audit AnimReleaseControls saved identity across menus/loads:
it currently retains a pointer, bits and scale, while SkcAlive checks slot/class.
Use full retained identity and a refreshed live table, and refuse stale restoration.
Existing ArmsHideTick, BoneVisOff, MatRestoreAll, mesh_split native_draw and weapon
attachment consumers must agree. Preserve normal gameplay and intentional mantle
exception. No hands behavior change is installed with the first FOV test.

## Next test

Installed candidate identity: build/cinematic-fov/latest-install.json. Verify its
banner before reading the next run; archive both logs before relaunch. Never
launch the game. One question: during the conversation that previously shrank
the view, does the image stay full-size throughout the speaker/choice sequence?
Full-size supports final camera FOV suppression. A shrinking image despite a
successful scope means another render/viewport control is involved; inspect
cache request, sensor and fovaudit without assuming the write was consumed.
Native hands/arms test follows only after this FOV result. VR-50 broader kill-cam
and manual-FOV cases remain unverified and the ticket must not be closed merely
because the conversation candidate passes.
