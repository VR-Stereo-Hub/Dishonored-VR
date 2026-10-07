# CorvoBody (Nexus mod 453) and the VR mod: what it does, where it collides, how it attaches

Status: **research, 2026-10-07**. Nothing is built yet. This is the record of a full headless
IDA decompile of the shipped DLL, read against our own code, and the plan that follows from it.

| Item | Value |
|---|---|
| Mod | CorvoBody - First Person Body for Dishonored, v1.1, by TJD109901 (Nexus 453) |
| File | `dinput8.dll`, 2,505,728 bytes, md5 `dd05f9aadfaa2aef76707fd2f3a0dca9`, MinGW/GCC build (`.eh_fram`, `.gcc_exc`, `.buildid`, PDB name `dinput8.pdb` only), image base 0x10000000, 7017 functions |
| Ships with | `CorvoBody.ini` (every key below), `README.txt`, MinHook licence |
| Built against | the current Steam `Dishonored.exe`, SHA-256 `66443F3D...725E17E`: **the same build our staged exe is** (checked 2026-10-07) |
| Credits it lists | MinHook (hooks), **the CodeRed SDK for Dishonored from the dismod project** (class layouts and ProcessEvent wrappers; see `docs/SDK_WORKFLOW.md`) |
| Decompile | `tools/ida/cb1_fulldecomp.py` then `tools/ida/cb2_rename_sdk.py` against `CorvoBody.dll` staged in the IDA workspace (`-Module CorvoBody.dll`). Output stays in `<ida_workspace>\out\` and is **never committed**: it is someone else's copyrighted work as well as game-derived |

**Permissions.** The Nexus page forbids re-uploading the file anywhere and forbids modifying
it without the author's permission. Everything in this plan therefore **adapts our mod to
their DLL as shipped**: we never patch their file, never bundle it in our zip, and never
distribute a modified copy. The launcher may *detect* a copy the user downloaded themselves
and may write *their* ini (the README invites editing it). Anything beyond that - a
configurable-hotkey request, a "VR mode" in their code - is a conversation with the author,
and section 7 names what we would ask for.

## 1. What it is, in one paragraph

A `dinput8.dll` proxy (it forwards `DirectInput8Create` to the system DLL and chain-loads any
`dinput8_*.dll` next to the exe). On a worker thread it checks the image base is 0x400000,
reads `CorvoBody.ini`, pattern-scans for `LoadPackageAsync`, and installs four MinHook
detours (section 2). After every engine tick it builds, if needed, a second
`SkeletalMeshComponent` on the player pawn using Corvo's **third-person body mesh from the
Brigmore Witches DLC** (`DLC07_skm_Corvo_Body`, async-loaded from `DLC07_Endgame_Script`,
with `DLC06_Skm_Corvo_Body` / `DLC06_Tower_Script` and `DLC07_BaseIntro_Script` as fallbacks)
animated by an **AnimTree it constructs at runtime** from the Elite Guard's `empty` anim set
(`Npc_EliteGuard_empty_as` from `L_Distillery_Ext_Script`), then drives that body every tick:
walk/run blend by speed, turn-in-place steps, procedural crouch, slide, jump, climb, swim and
lean poses through SkelControls it also constructs, plus bone hiding (neck, torso, shoulders,
coat panels) so the body never shows the inside of the collar. Optionally it adds a hidden
"shadow body", a head-only shadow caster, a copy of the first-person arms as a shadow caster,
and item shadows. Its **RigidCamera** feature hooks the controller's `GetPlayerViewPoint`,
learns the game's look-up/down camera arc and removes it (section 5).

## 2. How it loads and hooks (all byte-verified by the DLL before patching)

| Target | VA | Our name | What the detour does | Our notes |
|---|---|---|---|---|
| `UGameEngine::Tick` | `0x00632860` (prologue `55 8B EC 6A FF 68`) | `hook_GameEngine_Tick` | calls the original FIRST, then runs the whole body update (`body_update`, 48 KB of code) once per tick, re-entrancy-guarded through TLS | `patterns.h kGameEngineTick`, same address, derivation only on our side (we never hooked it) |
| `UObject::CollectGarbage` | `0x0047D750` | `hook_CollectGarbage` | before a purge: detaches its own TArray buffers that instanced anim nodes share, clears the keep-alive flag it set on its mesh/anim set/components, logs `full GC: world %p ...` | not in `patterns.h` |
| `appExit` / `StaticExit` | `0x004645D0` (prologue `8B 0D 14 A9 40 01`) | `hook_StaticExit` | same detach on quit | not in `patterns.h` |
| `ADishonoredPlayerController::GetPlayerViewPoint` | `0x005E17A0` (10-byte prologue compare) | `hook_GetPlayerViewPoint` | the rigid camera (section 5); inert when `RigidCamera=0` | ENGINE_NOTES already derives this address (vtable `+0x3C4`, reads `PlayerCamera+0x330/+0x33C`); `power_aim.cpp` reads its result, we never hook it |
| `LoadPackageAsync` | pattern `55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 81 EC 8C 00 00 00 56 57`, first hit in `.text` | called, not hooked | async package loads with a completion callback | not in `patterns.h`; worth adding when we need async loads |

Globals it reads directly: **GWorld `0x01449888`** (world change = drop the body), the
**local pawn pointer `0x0145F628`** (checked `!bDeleteMe` at `+0x120 & 8`, `!bTickIsDisabled`
-ish bit at `+0x126 & 0x40`), **GObjects `0x01423630`** and **GNames `0x01435674`** (our
`kGNamesData`, same value; GObjects we resolve differently), `StaticConstructObject`
`0x004955A0`, `appMalloc` `0x00404280`. The two pawn/world globals are not in ENGINE_NOTES
yet; derive before relying on them.

Everything else goes through **UFunction calls by full name** (`Function Engine.SkeletalMeshComponent.FindSkelControl`
and 3,900 other SDK wrappers embedded as strings) over `ProcessEvent` (vtable index 59), and
through **hard offsets from the generated SDK**. Offsets it uses that we checked against the
SDK and against `patterns.h` (all agree):

| Field | Offset | Used for |
|---|---|---|
| `APawn::Mesh` | `0x3DC` | the first-person arms component ("player pawn has no Mesh yet") |
| `APawn::CylinderComponent` | `0x3E0` | collision height -> posture (crouch 65, slide 33, standing) |
| `APawn::Controller` | `0x26C` | controller `Rotation.Pitch` at `+0xD0` for lean / hide-upper-body |
| `APawn::EyeHeight` | `0x32C` | the eye point: `Location + (0,0,EyeHeight)` |
| `AActor::Location/Rotation/Velocity` | `0xCC` / `0xD4` / `0x1B4` | pawn yaw frame, speed, fall speed |
| `AActor::Physics` | `0x104` | `ClimbPhysics=9`, `SlidePhysics=14`, swimming |
| `ADishonoredPlayerPawn::m_pInventory` | `0x59C` | per-hand held item: an object name containing `Empty`, `Sword`, `Pistol`, `Crossbow` ("hands: right %s (%s), left %s (%s)") |
| `ADishonoredPlayerPawn::m_pPlayerMasterFSM` | `0xA2C` | state name -> "player state: %s" (mantle / takedown detection) |
| `APlayerController::PlayerCamera` | `0x384` | camera; `bCinematicMode` bit at `+0x38D` |
| `ACamera::CameraCache.POV.Location` | `0x330` | "camera %.0f uu from the eyes" |
| `ACamera::ViewTarget.Target` | `0x36C` | "camera view target is not the player" |
| `UPrimitiveComponent::Translation/Rotation/Scale` | `0x190/0x19C/0x1A8` | **ours: `kMeshTrans/kMeshRot/kMeshScale`** |
| `USkeletalMeshComponent::SkeletalMesh` | `0x1D4` |  |
| `USkelControlBase::ControlName/ControlStrength/StrengthTarget` | `0x5C/0x64/0x78` | every control is named after its bone |
| `USkelControlLimb::EffectorLocation/EffectorLocationSpace/JointTargetLocation` | `0xB8/0xC4/0xD0` | arm and foot IK |
| `USkelControlSingleBone` bool pack / `BoneRotationSpace` | `0xB8` / `0xC9` | leg swing, spine lean, hand rotation |

Hotkeys are **hard-coded** `GetAsyncKeyState(VK_F6..VK_F11) & 1` edge checks inside the tick:
F6 reload ini, F7 crouch-leg axis, F8 body on/off, F9 hide body arms, F10 attach mode, F11
debug dump. No ini key renames them.

## 3. What it builds

**Components on the pawn** (all `StaticConstructObject(SkeletalMeshComponent, outer=pawn)`):

| Component | Mesh | Visible | Purpose | Created when |
|---|---|---|---|---|
| body | `DLC07_skm_Corvo_Body` | yes | what you see | `Enabled=1` and the package loaded |
| shadow body | same | `SetHidden(1)`, casts shadow | `ShadowBody=1`: an unhidden-bone copy so the shadow keeps coat and arms | `PlayerShadows=1` |
| head shadow | `DLC07_skm_Corvo_Mask` | hidden, casts shadow | `HeadShadow=1` | `PlayerShadows=1` |
| 1P arm shadow | the pawn's own `Mesh` asset, `SetParentAnimComponent(Mesh)` | hidden, casts shadow | `FirstPersonArmShadow=1` | `PlayerShadows=1` and a hand holds gear |
| item shadows | the item attached to the 1P arms | hidden | `ItemShadows=1` | per hand |

Attachment (`AttachMode`): `1` (default) = `Actor.AttachComponent` to the pawn, and every tick
the body's **relative translation/rotation/scale are copied from the first-person arms
component** (`Mesh+0x190/0x19C/0x1A8`) plus the mod's own offsets; `0` = `SkeletalMeshComponent.AttachComponent`
to the 1P mesh's bone `root0_jnt`. The body is hidden by scaling it to 0.1% (`SetScale`), not
by `SetHidden`, so the shadow copy keeps working. Depth group and LOD are copied from the arms.

**The AnimTree it constructs** (`Class Engine.AnimTree` with `StaticConstructObject`, then
nodes by class name): a `BlendList` over `BlendBySpeed(Idle, WalkDirectional(N/S/E/W),
RunDirectional)` with the Elite Guard `Empty_*` sequences, a turn-in-place pair
(`Empty_TurnLeft45/Right45`), an optional `Assassin_Crouch_loop` branch (`UseCrouchPose`), and
`BlendPerBone` masks. The `BlendBySpeed` rates come from `WalkAnimSpeed/RunAnimSpeed` so the
feet stay planted. There is no body animation for the arms holding anything: the body's arms
play the `Empty` (holstered) cycles only, which is why `HideBodyArms=1` hides each shoulder
whose hand holds gear and lets the first-person arm show instead.

**SkelControls** (constructed and appended to the tree's `SkelControlLists`, every one
**named after the bone it drives**, `ControlStrength` 0 unless the feature is on):

| Control name | Class | Driven by | Config |
|---|---|---|---|
| `upper_leg_L/R_jnt`, `lower_leg_L/R_jnt` | `SkelControlSingleBone` (rotation, `BoneRotationSpace` from `CrouchLegSpace`) | crouch bend/spread, slide, jump tuck, climb, swim kicks | `SlideFront*`, `JumpHip/Knee`, `Climb*`, `Swim*` |
| `foot_L/R_jnt` | `SkelControlLimb` (effector world space, joint target from `KneeTarget*`) | foot IK: lifts the feet by the measured body drop | `FootIK`, `FootLiftExtra/Max/Sign` |
| `Back_pan_*_jnt` (4 coat panels) | `SkelControlSingleBone` | coat flare / hide | `CoatRearFlare`, `CoatMode` |
| `spine_1_jnt`, `spine_2_jnt` | `SkelControlSingleBone` | look-up/down lean and corner lean | `LeanUp/Down/Max/Sign`, `CornerLean*` |
| **`hand_L_jnt`, `hand_R_jnt`** | **`SkelControlLimb`**, `JointTargetLocation` (-30, -/+60, 0) actor space, `bMaintainEffectorRelRot` off | **only on the shadow body**, only while that hand holds gear, effector = the 1P hand bone's world location (`ShadowArmsFollowHands`). On the visible body they exist at strength 0 | `ShadowEye*`, `ShadowElbow*` |
| **`Offset_hand_L_jnt`, `Offset_hand_R_jnt`** | **`SkelControlSingleBone`**, `bApplyRotation` only, world space | same: copies the 1P hand rotation onto the shadow hand | |
| `shoulder_L/R_jnt`, `upper_arm`, `lower_arm` | no control; `HideBoneByName/UnHideBoneByName` | `HideBodyArms`, `ArmSpreadDegrees`, `ArmForwardDegrees`, `JumpArm*` write the arm pose procedurally through the existing bones' local rotation (via the SingleBone controls on `upper_arm_*`/`lower_arm_*`, created the same way) | |

The last two rows are the attach point for VR (section 6): **a two-bone IK and a hand-rotation
control already exist on every arm of the visible body, named, at strength 0, waiting for an
effector.**

## 4. The per-tick update, in order

`hook_GameEngine_Tick` -> original `Tick` -> (world changed? drop everything) -> read the pawn
global -> 3 s after a pawn change -> `body_update`:

1. hotkeys; F11 dumps pawn, 1P mesh (translation/rotation/scale/depth/LOD/hidden), anim
   sets, anim tree, body state to `CorvoBody.log`;
2. pawn change -> drop body and mesh; package state machine (async load -> `found body mesh`
   -> anim set -> `body created: comp=%p mesh=%p parent=%p armsDepthGroup=%d armsLOD=%d`);
3. **RigidCameraArms**: if `RigidCamera=1 && RigidCameraArms=1`, writes the 1P arms component's
   relative translation (`Mesh+0x190`) = its own tracked base + the camera shift. It re-bases
   when the field moved by more than its threshold since last tick (so an external writer is
   tolerated, but the shift is added on top). With `RigidCamera=0` the shift is zero and
   **nothing is written** (the write is gated on a delta > 0.01);
4. posture from the collision cylinder (`posture -> %s (cylinder %.1f of %.1f, eye %.1f, bIsCrouched %d)`),
   crouch smoothing (`CrouchSmoothSpeed`), slide, mantle, jump/land, climb, swim detection
   (physics mode, `m_pPlayerMasterFSM` state name, camera modifiers `step-up-mantle` /
   `crouch-mantle` read from the camera's modifier list);
5. body relative transform = 1P mesh translation + `Offset*` + crouch/slide/jump/clip-guard/
   stair/camera-follow pushes; rotation and scale = the 1P mesh's; shadow bodies the same
   without the pushes; `SetTranslation/SetRotation/SetScale` on each;
6. leg/foot/spine/coat/arm controls written (effectors, strengths, bone hides);
7. held-item scan (`m_pInventory`) -> `HideBodyArms` shoulder hide per hand, `hands: right %s (%s), left %s (%s)`;
8. shadow bodies: hand IK to the 1P hand bones, item shadows re-attached;
9. **hide decisions** (one reason logged as `body %s%s%s`): pawn `bHidden`, `bCinematicMode`,
   1P arms hidden, camera view target not the player, **camera further than
   `ThirdPersonDistance` (200 uu) from the eye point** (`camera %.0f uu from the eyes (fwd/right/up ...)`),
   mantle, take-off, **clip guard**: body hidden when hips/thighs/knees are closer than
   `ClipGuardDistance` (22 uu) to the camera after a `ClipGuardPush` (60 uu) retreat, upper
   body hidden on look-up (`HideUpperBodyLookUp` 20 deg, `LookUpHideDegrees` 70), on camera
   dip (`HideUpperBodyDrop`), on sprint (`HideUpperBodySpeed` 470), crouched above
   `CrouchHideUpperPitch`.

Everything it reads about "where the camera is" comes from `PlayerCamera.CameraCache.POV`
(`+0x330`), the eye point `pawn.Location + EyeHeight`, and `Controller.Rotation.Pitch`.

## 5. The neck pivot (RigidCamera) - the part to be careful with

**What the game does flat.** Looking up and down swings the camera on an arc around a pivot
below and behind the eyes (our measurement, 2026-09-03: 0.321 m below, 0.062 m behind,
standing; none crouched, VR-78). CorvoBody calls this "the camera swinging in an arc".

**What CorvoBody does.** Its `GetPlayerViewPoint` detour runs after the original fills the
out-location/rotation. Gated on `RigidCamera=1`, a valid camera whose view target is the pawn,
and the pawn being the one it built the body on. It then:

1. computes `sample = viewLocation - (pawn.Location + EyeHeight) - its own arms shift`,
   projected on the pawn yaw frame into (forward, up), at the current controller pitch;
2. **learns** that sample into a curve: 36 buckets of 5 deg from -90 to +90, per stance
   (standing / crouched, 148 floats each: forward, up, weight, fit), only while the pawn is
   still (speed^2 < 25, |vertical speed| < 5, position moved < 0.2 uu, not sliding), with
   the learning rate reduced when the bucket already has data. The curve is **persisted to
   `CorvoBodyCamera.bin`** (1184 bytes) next to the DLL and loaded at start;
3. **replaces** the out-location with `eye + curve(RigidCameraPivot = -85 deg) + RigidCameraForward`
   in the yaw frame - the camera stays where the game would put it looking fully down (in
   front of the chest), so looking down shows legs and not the collar. `RigidCameraVertical=0`
   keeps the arc's up/down part and removes only forward/back;
4. stores the shift it applied (`camera %s influence ...`) for the body follow
   (`RigidCameraBodyFollow`) and the 1P arms (`RigidCameraArms`, step 3 of section 4), and
   holds the camera briefly on quick mantles (`MantleCameraRelease`).

**Why this must be OFF in VR.** Two independent facts:

- The VR mod already owns the neck. `[Neck] Mode=cancel` subtracts the measured engine arc
  from the camera write so the HMD is the only pivot (`camera.cpp` pitchtest fits exactly the
  pivot model `eye = pivot + R(pitch) * (0, below, behind)`); crouched it subtracts nothing
  (VR-78). A second subtraction of the same arc by CorvoBody moves the view the *wrong* way
  by the full arc.
- CorvoBody's learner reads the POV cache **after our per-eye and head offsets have landed in
  it** (`kPovOffs[0] = 0x330` is a `[Camera]` eye-field option, and head tracking moves the
  cached location every present). It would learn room-scale head motion as "the game's arc",
  apply it as a camera correction, and **write it to `CorvoBodyCamera.bin` where it survives
  the session**.

Verdict for VR: `RigidCamera=0`, delete any `CorvoBodyCamera.bin` the user's flat sessions
wrote (or leave it: the file is only read when `RigidCamera=1`), keep `[Neck]` as the owner.
`RigidCameraArms` and `RigidCameraBodyFollow` are then inert. The benefit CorvoBody offers
flat (the view pivots in place) is what VR already has: the HMD is the pivot.

What VR loses by turning it off is only the *body-follow* term (the body eases forward with
the fixed camera so holstered arms stay in view). In VR the head moves on its own, so the
equivalent is CorvoBody's `CameraFollow` (0..1, default 0) which follows the POV sideways /
forward - that one reads the POV too, and in VR that is the tracked head, which is what we
want. Try `CameraFollow=0.5, CameraFollowMax=10` first.

## 6. Collisions with the VR mod, and what to do about each

| # | CorvoBody | VR mod | Effect if ignored | Resolution |
|---|---|---|---|---|
| 1 | hard-coded F6..F11 edge toggles | F6 (present_tick), F7, F8, F9, F10 (the overlay) all used | **every F10 overlay open flips CorvoBody's AttachMode**; F8 hides the body; F9 toggles its arm hide; F6 reloads their ini while we use F6 for something else | (a) ask the author for ini-configurable keys (section 7); (b) until then, when CorvoBody is detected the VR mod moves its own F6..F9 debug keys and documents F10/F11 as shared; (c) technically we could patch the **in-memory import** of `USER32!GetAsyncKeyState` in their module to filter VK_F6..F11 - that modifies nothing on disk but is still "altering their mod's behaviour", so it is **the user's call and not done without a yes** |
| 2 | `RigidCamera` detour on `GetPlayerViewPoint` | `[Neck]`, head tracking, per-eye POV writes | double neck cancel, corrupted learned curve persisted to disk | `RigidCamera=0` (section 5). The VR mod logs a `Warn` if it finds `CorvoBody.ini` with `RigidCamera=1` |
| 3 | body transform copied from the 1P arms component (`+0x190/0x19C/0x1A8`) | the SkelControl hand drive moves hand **bones**, not the component; the legacy `handmesh` lever (`fp_mesh.cpp FpCommandAll / the depth writer`) moves the **component** | with the legacy lever on, the whole body follows one hand; with the default drive, nothing | keep the default drive; the VR mod refuses the legacy handmesh writer while CorvoBody owns a body (one log line). Verify at runtime with F11: `1P mesh translation (...)` should read what the game set |
| 4 | `ThirdPersonDistance=200` from the eye point, clip guard 22 uu, `camera inside the body` | tracked head: at `WorldScaleUU=100`, 200 uu = 2 m from the pawn's eye point | walking 2 m off the pawn in room scale hides the body (fine); leaning over the body hides it within 22 cm (fine, intended); crouching *physically* (the pawn stays standing) reads as "camera dipped" -> upper body hidden by `HideUpperBodyDrop` | expected behaviour; tune `ClipGuardDistance`, `HideUpperBodyDrop` on the headset |
| 5 | look-up/down hides and spine lean read `Controller.Rotation.Pitch` | head tracking writes the control rotation pitch from the HMD | works: the body leans and hides with the head | none; note `LookUpHideDegrees=70` hides the upper body when looking at the ceiling |
| 6 | `HideBodyArms=1`: the body's shoulder is hidden while that hand holds gear; the body's arms otherwise play the holstered idle | VR hands are where the controllers are, the 1P arms are cut at the wrist (ARM_HAND_SPLIT) | holstered body arms hang at the sides while your real hands float in front: wrong; with gear, no body arm at all | **the shoulder attach, section 7** |
| 7 | keep-alive flag `ObjectFlags & 0x40 00 00 00` (byte `+9`) on its mesh, anim set, components; cleared in its GC hook | `IsLiveObject` liveness walks; our own GC-safety rules | none, but a VR-side pointer to their body component is only valid under `IsLiveObject` and must be re-found after `world changed` / `player pawn changed` log lines | find the body by class `SkeletalMeshComponent` + `SkeletalMesh` name `DLC07_skm_Corvo_Body` + outer = pawn, every time, never cache across a level |
| 8 | its own `dinput8_*.dll` chain-loader | we are `d3d9.dll`; no overlap | none | the launcher does not need to chain anything; the game loads `dinput8.dll` itself |
| 9 | `PlayerShadows=0` default; shadow bodies off | stereo cost | leave off; the shadow body duplicates skinning per eye | default |
| 10 | hides the body in `bCinematicMode` and when the view target is not the pawn | cinematic policies in `cinematic_*.cpp` | agrees | none |

## 7. The plan: attach the body to the VR hands at the shoulder

The body's arms already have a named two-bone IK (`hand_L_jnt`, `hand_R_jnt`,
`SkelControlLimb`) and a hand-rotation control (`Offset_hand_L/R_jnt`) on the **visible**
body, left at strength 0 (section 3). CorvoBody only drives them on the shadow body. The VR
mod can drive them on the visible body with the controller poses it already has, which is
exactly the "arms connect at the shoulder" result: the shoulder stays where the body's
animation puts it, the elbow solves, the wrist meets the VR hand.

In order, each step verifiable before the next:

1. **Detect** (`[CorvoBody]` section, `Enabled=1` default-ON only when the files are present):
   `dinput8.dll` next to the exe exporting `BlinkBootstrap_Register`, `CorvoBody.ini` present,
   and `CorvoBody.log`'s `body created:` line. Read their ini and `Warn` on `RigidCamera=1`
   and `Enabled=0`. Log one `Info` line naming the owner: `corvobody: detected v? body=%p`.
2. **Find the body component** every tick it is needed: walk the pawn's components (or
   GObjects) for class `SkeletalMeshComponent`, outer = pawn, `SkeletalMesh` named
   `DLC07_skm_Corvo_Body` (or the DLC06 fallback), not `HiddenGame`. Two will match when
   `PlayerShadows=1` (the visible one is the one not `SetHidden`). `IsLiveObject` on every use.
3. **Resolve the controls by name** with `FindSkelControl` (we already call it from the hand
   drive; `arm_follow.cpp` does the same for `LookAtControl_*`): `hand_L_jnt`, `hand_R_jnt`,
   `Offset_hand_L_jnt`, `Offset_hand_R_jnt`. Resolve the field offsets by name through
   `FindPropOffset` (`EffectorLocation`, `EffectorLocationSpace`, `ControlStrength`,
   `StrengthTarget`, `BoneRotationSpace`, the `bApplyRotation` bool) and cross-check against
   the SDK table above; refuse on a mismatch.
4. **Drive**: per tick, effector = the VR hand's world position (the same world point the
   SkelControl hand drive already computes for the 1P hand bone: one ray, one point),
   `EffectorLocationSpace = 0` (world), `ControlStrength = StrengthTarget = 1`; the
   `Offset_hand` control's rotation = the VR hand's world rotation, `bApplyRotation` on. Keep
   CorvoBody's `JointTargetLocation` (elbow back and out) initially; expose
   `[CorvoBody] ElbowBack/ElbowOut` later if the elbows look wrong. Write AFTER CorvoBody's
   tick (ours runs on the present thread / script lane, theirs at the end of `UGameEngine::Tick`;
   log the order once from the first tick's timestamps: theirs `body created` vs ours
   `corvobody: first effector write`).
5. **Unhide**: `HideBodyArms=0` in their ini so the shoulders stay visible with gear; the VR
   mod's own `arms_hide.cpp` keeps hiding the *1P* upper arm / forearm (the VR hands are the
   cut hands). Decide per hand which hand mesh shows at the wrist: the body's (`hand_L_jnt`,
   now at the controller) or ours (the cut 1P hand with the weapon). First build: hide the
   body's `hand_*_jnt` bones (`HideBoneByName`) and keep ours, so weapons and grab animations
   stay exactly as today; the body's forearm ends at our wrist.
6. **Stretch guard**: the body's arm length is fixed; a controller held further than the arm
   reaches leaves a gap at the wrist (`bAllowStretching` is a Limb option, off). Log the
   reach deficit per hand (`DVR_LOG_EVERY_MS`) and clamp our effector onto the sphere of reach
   from the shoulder bone (`GetBoneLocation(shoulder_X_jnt)`) at 98 %, with the 1P hand still
   at the controller. The gap is then a few millimetres at most.
7. **Hides that fight VR**: `HideUpperBodyLookUp`, `LookUpHideDegrees`, `HideUpperBodyDrop`
   now also hide the *shoulders*, which cuts the arm at the collar while the hand still
   shows. First build: `HideShouldersUp=0`, and when the body reports the upper body hidden
   (bone `spine_3_jnt` hidden - readable through `BoneVisibilityStates`), the VR mod drops
   its effector strength to 0 over `ReappearSpeed` so the arms do not float from nothing.
8. **Verification without a headset**: the simulator drives hands and head
   (`xrsim-cmd "hand right pos ..."`); `status.json` gains `corvobody: {found, body, effectorL,
   effectorR, reachL, reachR}`; a capture with the hand at 0.4 m forward must show the body's
   forearm reaching it (a non-black region between the shoulder and the hand in the lower
   half of the frame, compared against a capture with `Enabled=0`). Headset: the four
   headset-only questions are elbow direction, wrist seam, shoulder jitter against head
   motion, and whether the hidden-upper-body transitions read as intended.

**What to ask the author** (not blockers for the first build): ini-configurable hotkeys; an
ini switch that leaves the visible body's `hand_*_jnt` controls to an external driver
(today nothing writes them, so it works by accident and could change); a documented
`CorvoBody.log` line when `AttachMode` changes. All three keep their file unmodified.

**What is deliberately not here**: a VR-side reimplementation of CorvoBody (every
procedural pose above is theirs and stays theirs); bundling their files; patching their DLL;
a dinput8 chain of our own.

## 8. Provenance and the record

- Decompile: IDA 9.3 headless, `tools/ida/cb1_fulldecomp.py` (full decompile, string
  xrefs, game-range immediates, 173 s, 0 failures) and `tools/ida/cb2_rename_sdk.py` (3,936
  SDK wrappers named after the `Function Pkg.Class.Func` string each references, hook
  handlers named, mod core re-decompiled as `Class__Func()` calls). Both outputs in
  `<ida_workspace>\out\`, not committed.
- Static strings (`py -3` + `pefile`): exports `DirectInput8Create`, `DirectInput8Create@20`,
  `BlinkBootstrap_Register` (forwarded to the first chained DLL that exports it, logged
  `BlinkBootstrap_Register forwarded: %s`; it is the Dishonored Mod Loader's entry point,
  nothing to do with the Blink power).
- Offsets named from the dismod CodeRed SDK (`docs/SDK_WORKFLOW.md`), which agreed with
  `patterns.h` on every field both know (`0x190/0x19C/0x1A8`, `0x384`, `0x330`, `0x26C`).
- The VR side read: `camera.cpp` (pitchtest / `[Neck]`), `head_track.cpp` (control rotation
  pitch), `hands/fp_mesh.cpp` (the component-transform writers), `hands/skelcontrol.cpp`
  (the bone drive), `hands/arms_hide.cpp`, `core/input/hotkeys.cpp`, `present_tick.cpp`.
- Not read in detail (not needed for the plan): the slide/jump/climb/swim leg maths, the
  coat panels, the stair follow, the item-shadow attachment walk, the GC array detach.

## 9. Open questions

1. Who runs first on a tick, our hand writes or theirs? Decided by one log line (step 4).
2. Does the game load `dinput8.dll` before or after our `d3d9.dll`? Only matters if we ever
   hook `GetPlayerViewPoint` ourselves (MinHook and our detours chain either way, but the
   order of the two detours on the same function would be load order).
3. The pawn global `0x0145F628` and GWorld `0x01449888`: derive and record in ENGINE_NOTES
   with `ue3-natives.py` / `disasm-rva.py xref` before we read either.
4. The arm-reach clamp (step 6) versus `bAllowStretching=1` on their control: stretching
   keeps the wrist seam closed at any distance but deforms the forearm; a headset decides.
