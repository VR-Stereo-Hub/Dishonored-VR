# Engine notes - Dishonored (Dishonored.exe, Steam, patch 1.4)

The reverse-engineering knowledge base. Everything below was established by the original
author (GingasVR) across proxy builds 30.0 to 38.92 and recorded in the code's comments; this
file distills those comments so an agent does not have to read 23k lines to find a number.
"Verified" means the shipped build depends on it. Nothing has been re-measured by the
continuation yet (the game is not installed on the dev PC as of 2026-09-02).

Rules: every number lives in `src/game/dishonored/patterns.h` and here, with how it was
derived; a hook byte-verifies its target and refuses on mismatch; never copy a number from
another game; findings go here in the same commit as the code that uses them.

## Identity

- UE3 licensee build 9099, x86, D3D9, Scaleform. No ASLR: image base 0x400000, `.reloc` ends
  at +0x1206A0C (`kModEnd`), `.data` at +0xE69000 for 0x21B3BC bytes (`kDataStart/End`).
  Derived by static analysis of the exe (the original "UE3 introspection probe", stage 3).
- The mod is a `d3d9.dll` proxy: the game's static d3d9 import loads it before any engine
  code runs, which is why the XInput IAT hook can be installed in DllMain (the input system
  decides "gamepad or not" once, at startup).

## Fixed addresses and hooks

| Symbol | Value | What | How derived / verified |
|---|---|---|---|
| `kGObjHdr` | 0x1423630 | `TArray<UObject*>` GObjects {Data, Num, Max} | static analysis; walked by `IsLiveObject`/`BuildLiveSet` |
| `kGNamesData` / `kGNamesNum` | 0x1435674 / 0x1435678 | GNames table | static analysis; `NameFromIndex` |
| `kNameOff` / `kClassOff` / `kOuterOff` | 0x28 / 0x30 / 0x24 | UObject FName, Class, Outer | probe dumps |
| `kProcessEvent` | 0x00470640 | `UObject::ProcessEvent`, prologue `55 8B EC 6A FF` | the script dispatch spine; hooked with a hand-built 96-byte trampoline (`InstallProcessEventHook`) |
| `kBlkDirHook` | 0x00bf55a3 (back 0x00bf55a8, bytes `8b 08 89 4d b4`) | Blink: the engine's aim vector at its source | THE shipped Blink redirect: the engine traces, validates and draws along the controller |
| `kBlkAimHook` | 0x00bf595f (back 0x00bf5964) | Blink: the first `movss` of the aim | observe/redirect stage |
| `kBlkDstHook` | 0x00bf5e4f (back 0x00bf5e55) | Blink: destination write | |
| `kBlkTrcHook` | 0x00bf5d1a (back 0x00bf5d1f) | Blink: the trace | |
| `kXIGetSlot` / `kXISetSlot` | 0x00f946c4 / 0x00f946c0 | exe IAT slots for `XInputGetState` (ord 2) / `XInputSetState` (ord 3) | verified at install: the slots must still point at the real functions |
| `kCamHookAt` | 0x56dd36 (`5E 8B E5 5D C3`) | camera-matrix builder epilogue | LEGACY: the spin test proved the renderer does not use this matrix |
| D3D9 vtable slots | IDirect3D9: 16 CreateDevice, 8 GetAdapterDisplayMode, 6 GetAdapterModeCount, 7 EnumAdapterModes; IDirect3DDevice9: 17 Present, 16 Reset, 94 SetVertexShaderConstantF, 37 SetRenderTarget | COM layout | |
| user32 IAT hooks | GetSystemMetrics, SystemParametersInfoA/W, GetMonitorInfoA/W, SetWindowPos, MoveWindow, SetWindowPlacement, GetClientRect (exe AND the fork's own import table) | holding the 4032x2268 window against the window manager | the fork shrank the window from inside Reset (32.76) |

## UE3 layouts used

- Camera object: `kCamRight` 0x60 (basis Y row), `kCamLoc0/1/2` 0x80/0x90/0xa0 (matrix
  translation row, cached POV location, ...); POV rotator candidates `kPovOffs` {0x330,
  0x350, 0x374}; FOV candidates `kFovCands` {0x53c, 0x540, 0x564, 0x254}; controller/camera
  rotator bases `kPcRotBase`/`kCamRotBase` {0x9c, 0xd0}. The FOV lever writes
  `kLevCtrl` {0x3ac FOVAngle, 0x3b0 Desired, 0x3b4 Default} on the controller and `kLevCam`
  {0x254, 0x348, 0x368, 0x38c, 0x53c, 0x540, 0x564} on the camera every dispatch.
- SkeletalMeshComponent: `kMeshTrans` 0x190, `kMeshRot` 0x19c, `kMeshScale` 0x1a8,
  `kMeshScl3D` 0x1ac; pawn -> first-person mesh at `g_fpMeshOff` 0x3dc (re-derived by
  reflection if wrong).
- SkelControl (AnimTree bone-override node): `kSkcName` 0x5c FName ControlName, `kSkcStr` 0x64
  ControlStrength, `kSkcScaleProp` 0xa0, NextControl 0xac, `kSkcBools` 0xb8 (bApply/bAdd/
  bRemove bits 0x01/0x02/0x04/0x08), `kSkcTrans` 0xbc FVector BoneTranslation, `kSkcTSpace`
  0xc8, `kSkcRSpace` 0xc9, `kSkcRot` 0xd4 FRotator BoneRotation. Resolved by reflection
  (`FindPropOffset`) and audited (`SkcOffsetAudit`).
- Reflection: `FindPropOffset(class, name)` walks the UProperty chain; `FindFunctionObj`,
  `FindNameIdx`, `AsUFunction`. `ProcessEvent` parms for `ProcessViewRotation` are sniffed
  by locating `DeltaTime` in the parm block (guessing `+4` crashed).

## The head-tracking seam (verified, shipped)

`ApplyHeadToViewRotation` inside the ProcessEvent hook writes HMD pitch/yaw(/roll) into the
`ProcessViewRotation` parms; `[HeadTrack] ChainStamp=1` stamps every camera-modifier dispatch
in the same tick because which modifier survives the chain is re-decided at every level load.
Watchdog: `g_scriptHeadOK`, with a fallback re-arm (`[HeadTrack]` keys, F9). Retired
generations still in the tree as legacy: mouse injection (swims, lags, no roll), the
camera-object matrix detour (renderer ignores it), the view-projection shear at c0
(`ShearVP`/`LeanVP`, kept for the AER path).

Positional tracking (`TrackHead`): body anchor EMA, physical crouch with a self-healing
standing reference, lean/peek with a safety clamp, roomscale with deadzone bleed and
auto-recenter, deep-crouch collision-cylinder shrink (`PawnCollisionHeight`, 87.5/65/33).

## Head coupling of the arms (the open problem; roadmap D5)

Root cause as established: Arkane draws the first-person view model in camera space; there
is no world matrix for the arms anywhere in the constant map, so the rig's placement is
defined by the camera. Six attempts, in order:

1. Bone-bank writes in memory (`SpaceBases` +0x208 / `LocalAtoms` +0x214): no visible effect
   at any rate; the renderer keeps its own copy (`SbApply/SbTick`, legacy).
2. Render-time bone-constant drive: bone palettes arrive as VS constants at c6, 3
   registers/bone; sizes x36 = sword, x144 = arms, x204 = NPC. Moved the sword, never
   decoupled it (`rtd_drive`, legacy).
3. The deciding test (`g_rtdLockTest`): apply only `gain * (head yaw since neutral)` and sweep
   the gain: no gain held the weapon still. Conclusion: no seam between the arms and the
   camera downstream.
4. VR hands (shipped for the weapons): hide the engine's arms and draw our own OBJ meshes at
   the controller pose (`core/gfx/hand_mesh`, `vrhands\*.obj`); tracking is 1:1 by
   construction.
5. SkelControl route: world-space translation works and is absolute (the pin test: you can
   walk away from your hands) but throws away the authored resting pose; world-space rotation
   does not work (9,000 writes/s into BoneRotation outrun the recompute and the hands still
   rotate with the head). The real culprit: three controls exist, `LookAtControl_LeftHand`,
   `_RightHand` and `_Camera`; the camera one aims the arms at the view and was untouched for a
   dozen builds. Dial: `g_skcCamStrength` (ControlStrength 0 = off).
6. The donor graft (newest lane): a pristine `SkelControlSingleBone` from the player rig's
   archetype template (`Ply_Player_at`) spliced onto a live hand control's empty NextControl
   (+0xac). Residual coupling is upstream of the grafted bone; the coefficient could not be
   pinned, so `[Hands] GraftHeadFollowYaw/Pitch` (default 1.5, a guess) compensate:
   `cmd -= HC * (hmdNow - hmdAtZero)`. Safety: `SkcAlive`/`GraftDonorAlive`/
   `GraftEmergencyRestore` because writes into a freed AnimTree after `CollectGarbage` on a
   save load corrupted the heap.

Motion sickness note: position following the controller while orientation follows the head
is two contradictory cues on one object; F9 kills the whole drive for that reason.

## The OpenXR presentation (the open problem; roadmap D3, see XR_HANDOFF)

Both backends share the pipeline; only the pose source and delivery differ. The XR path
(builds 37.3-38.92) accumulated mutually exclusive theories, all still selectable:
`[Screen] RigidScreen`, `EyeCant` (measured identity, not the warp), `WorldScreen`
(geometrically correct, experientially wrong), `OverlayScene` (rejected 38.0), `XrCylinder`
/ `[VR] XrLayer` (cylinder "weird wrapped around"), `[VR] XrPoseDelay` (0-3), `StampFix`
(needs `dxvk_vr_view`, absent), `StampLive` (default 1: stamp the head pose located THIS
submit; the content already carries the head rotation, so a stamp that disagrees makes the
compositor cancel the motion: "I can look up and down for a few seconds after load, then it
locks" = the seconds before head injection starts). `[VR] FpsCap` pins the game to the
display rate because fps wandering 66-80 against 72/90 Hz was the measured stutter cause.
XR-3 architecture: a detached pace thread owns every runtime call (VDXR raced a haptic call
from the render thread and trashed the heap); the game thread only publishes eye textures.

## Other seams (verified)

- Blink: the source-vector detour (above) plus `BlinkControllerDir` and `BlinkReach`
  (distance by hand pitch); a landing marker drawn through the fork (`dxvk_vr_mark`).
- Motion aim: freshly spawned projectile objects near the camera get their aim vector
  rewritten (`MotionAimTick`, `MaimWriteAim`, `SteerTick`); haptics on catch.
- Melee: swing detection on controller velocity (`MeleeTick`, `[Melee]` speed/sustain/
  distance gates); block/choke state (`BlockStateTick`).
- Menu/cine: `g_menuOpen`/`g_inMenu` from script events and the cursor; `CineActive` latch
  cleared when no live pawn (the main menu fires the same toggle); dialog holds.
- Console: `RunConsole` drives the engine console from the script lane (the intro skip:
  `ce ChangeLvl_fromTower_toPrison`; `SetResApply` asks for the render size from inside the
  engine because every window-route attempt failed).
- Resolution: `hkGetAdapterDisplayMode/ModeCount/EnumAdapterModes` hand the game a
  4032x2268 mode; the window hooks hold it; `[Screen] SpoofDesktopW/H` and `ResX/ResY` must
  match.

## Dead ends (do not re-hunt)

- The camera-object matrix at `kCamHookAt` is not what the renderer draws with.
- Mouse-count head injection: swims, lags, no roll.
- "Any constant upload of 9+ registers is a bone palette" is false; writing into them corrupts
  world geometry.
- `HideBoneByName` on the arms does nothing (no visuals, no allocation); hiding is by
  render-size masks (`WeaponHideBones`, `ArmsHideTick`).
- Window-route resolution changes (work area, max tracking size, self-resize, fullscreen
  escape, client-rect spoof, mode list): six builds, the game still chose its own size; the
  engine's own setres is the way.
- The overlay-scene XR architecture (compositor overlay, reprojection-exempt): rejected
  at 38.0 (cannot be motion-smoothed).

## Build history (the original author's numbering)

30.x VR 2.0 (DXVK chain, hands, overlay, SkelControl, blink), 32.x resolution wars and
Blink detours, 33-36 graft and calibration, 37.x OpenXR bring-up (XR-1 bench, XR-2 sync,
XR-3 pace thread), 38.x the Quest convergence attempts, 38.92 shipped. The fork: M2 frame
map, M3 splice, M4 twins, M5 sequential + shafts, M6 wrist HUD + shadows, M7 pixel-shader
shear, M8 quarter-res light passes (M8.2 shipped).
