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

- Camera object: `kCamRight` 0x60 (basis Y row), `kCamLoc0/1/2` 0x80/0x90/0xC4 (matrix
  translation row, cached POV location, cached POV location 2); POV rotator candidates `kPovOffs` {0x330,
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

## From the author's handoff (HANDOFF-GINGASVR.md, their build 39.4)

Measured field offsets the table above lacks: PlayerController `+0x248` current Pawn;
PlayerCamera `+0x53c` is the **sensor** (the FOV actually rendered) and `+0xC4` the cached
POV origin; SkeletalMeshComponent `+0x60/0x70/0x80` world matrix rows, `+0x90` translation,
`+0xcc` bounds origin; `kUEPerRad` 10430.378 (65536 / 2pi). The addresses are the same
binary for everyone (Steam build): never chase them for a per-machine difference.

**Three head-motion paths are on at once** and most head-tracking bugs were in the gates
between them: (1) head-mouse (`[Tracking] Enabled`, synthetic mouse; silently needs the
game window foreground and the menu gate clear); (2) the native script write
(`[HeadTrack] Native`, `ApplyHeadToViewRotation`; `ProcessViewRotation` reaches
`ProcessEvent` ONLY through the camera modifier chain, never the PlayerController, and the
rotator is at `Parms+4` or `+8` depending on the declaring class, found by locating a
plausible `DeltaTime`; yaw is a delta, pitch is absolute); (3) the direct fallback
("viewinject") into the PlayerController when path 2 goes quiet, held off during cinematics,
for 15 s after a pawn latch and while script writes are fresh. Collapse to one path only
with the game in front of you.

The VR hands ARE the weapon view models (`pPlayerMesh`); `Skm_Player` is the body and
nothing drives it. `FpCollect` walks the object graph from the pawn (depth < 3, offsets
0x20..0x600, 24-candidate cap) and world props (`wash_rag`, `FeatherDuster`) get in and eat
slots. Calibration (`FpCalibrateTick`, 8 phases) is only valid while the player holds still;
records must be banked by asset name (their 39.0), not component pointer.

Resolution: `SpoofDesktopW/H` 4096x2304 is the lie to `GetSystemMetrics`/`GetMonitorInfo`,
`RenderWidth/Height` 4032x2268 what the game is told its client rect is, the real window
1600x900 for the spectator; `hkGetClientRect` returning the render size is load-bearing
(without it the game falls back as far as 800x600 and writes that to its own ini).
Windowed mode keeps the desktop cursor visible, so script events are the only menu signal.

Traps (their section 10): every `POOL_DEFAULT` D3D9 resource must be released in `hkReset`;
never log from a static initialiser (the fork stopped loading with no log at all); check
UObject liveness by GObjects index before writing (`CamAlive`); the log is overwritten on
launch (ours now rotates to `.prev.log`); inis are CRLF; **a direct exe launch crashes at
the menu, launch through Steam**; an explicit adapter needs `D3D_DRIVER_TYPE_UNKNOWN`;
diagnostics must pay out as discovered, not after a timer.

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

## The per-eye camera seam: write points (2026-09-02, 41.0)

The stereo methods drive the camera through `game/dishonored/camera` (a real module) on
the script lane, inside the ProcessEvent hook's camera pass right after `FovLeverApply`.
A per-eye render needs three writes; two are measured, one is not:

| Need | Write | Status |
|---|---|---|
| Rotation | `ApplyHeadToViewRotation`: HMD pitch/yaw(/roll) into the `ProcessViewRotation` parms, every dispatch of the modifier chain (`[HeadTrack] ChainStamp`) | MEASURED, shipped since 30.57 |
| FOV | the lever: `kLevCtrl` {0x3ac, 0x3b0, 0x3b4} on the controller and `kLevCam` {0x254, 0x348, 0x368, 0x38c, 0x540, 0x564} on the camera every dispatch; 0x53c is the read-only sensor (what the engine rendered) | MEASURED, shipped since 30.50; `camera::set_fov_deg` is the lever's target, `rendered_fov_deg` the sensor |
| Eye Z (the crouch clamp) | Z of {0x80, 0x330, 0x350, 0x374} on the camera, dispatch cadence | MEASURED (38.24): the clamp sticks, so at least one of those four reaches the renderer after the script pass |
| Lateral eye offset (+/- IPD/2 along `kCamRight` 0x60) | `camera::apply_eye_offset` into `[Camera] EyeField` (camera+0x330, which holds -position) | MEASURED 2026-09-02: 0x330 HONOURED 119/120, the five others DISCARDED (below) |

Why the lateral write is unproven: `head_track.cpp` records that the POV location the matrix
carries is a cache the engine recomputes each tick, yet the 38.24 eye clamp writes Z into
0x80/0x330/0x350/0x374 on the same cadence and is honoured. Either the clamp lands because
the same write repeats every dispatch (the last write before the draw wins), or because one
of the four fields is the renderer's actual source. For Z both explanations give the same
picture; for a lateral offset they do not (a field recomputed between the last dispatch and
the draw discards the offset, and a persistent field accumulates it).

The instrument: `camera eyetest <uu> [0x80|0x90|0xc4|0x330|0x350|0x374|all]` (seam word;
`camera eyetest stop` cancels). Stand still in gameplay. For 120 presents per candidate the
script lane adds `<uu>` along the camera's right row into the candidate (re-based every
tick, so a persistent field does not accumulate; restored afterwards), and the present
thread reads the render-side camera position back from vertex constant c5 (the frame-map
ABI: c5 = camera world position, captured in `core/framework/vs_const_hook.cpp`) and
projects `c5 - base` on the right vector. One line per candidate, then a summary:

    camera/eyetest: 0x80 asked +100.0 uu along right -> c5 moved +99.8 uu (mean of 118): HONOURED 118/120 frames ...
    camera/eyetest: 0x330 asked +100.0 uu along right -> c5 moved +0.3 uu (mean of 120): DISCARDED 120/120 frames - recomputed before the draw
    camera/eyetest: DONE (+100.0 uu): 0x80=HONOURED 0x90=DISCARDED 0xc4=DISCARDED 0x330=... 

HONOURED means the renderer drew from the offset position: that field is the seam's write
point (`[Camera] EyeField=`, or `camera eyefield <name>` live). DISCARDED means the engine
recomputed the field before the draw. INCONCLUSIVE means c5 moved by neither amount (the
player moved, or the draw sampled c5 from a different tick than the write). A DISCARDED-
everywhere result is a finding, not a failure: the lateral offset then needs a later write
point - a position-only patch at the camera-matrix builder epilogue (`kCamHookAt` was
disproved for ROTATION; position was never tried) or the c0 view-projection translation
(`LeanVP`, which positional tracking already uses and which AER can drive per eye).

**MEASURED (2026-09-02, dev PC, simulator lane, the auto-continued save, builds `dd10da09`
and `a3ede882`; runs 10 and 11; +100 uu, 45 presents of c5 baseline then 120 writing per
candidate):**

| Field | Reads (with c5 = (-53.6, 4835.0, -3036.1)) | Verdict |
|---|---|---|
| 0x80 `kCamLoc0` | (5.2, 500.0, -130.6) - a fixed offset vector, not the position | DISCARDED 120/120 (c5 moved 0.0) |
| 0x90 `kCamLoc1` | the same (5.2, 500.0, -130.6) | DISCARDED 120/120 |
| 0xC4 `kCamLoc2` | the same (5.2, 500.0, -130.6) | DISCARDED 120/120 |
| 0x330 `kPovOffs[0]` | (53.6, -4835.0, 3036.1) = exactly **-c5** (a view-matrix translation) | **HONOURED 119/120**: writing +100 uu into the negated field moved c5 by +99.2 uu along right (run 10, before the sign was known: -98.7 uu on 75/76 frames) |
| 0x350 `kPovOffs[1]` | (53.6, -4735.0, 3036.1), also -c5 in form | DISCARDED 120/120 (c5 moved -2.2 uu, mean) |
| 0x374 `kPovOffs[2]` | (53.6, -4835.0, 3036.1), also -c5 in form | DISCARDED 120/120 (0.0) |

So: c5 IS the camera world position (it equals minus 0x330 to the decimal), and **camera+0x330
is the eye-offset write point**: a value written there on the script lane at dispatch cadence
is what the renderer draws from, in NEGATED form. The seam's field table carries the sign
(`kFields[].sign`), `[Camera] EyeField=0x330` is the default, and `apply_eye_offset` writes
base - offset there. The camera's right row at +0x60 read (0, 1, 0) at yaw 0 (UE3: X forward,
Y right). 0x350 and 0x374 are copies the engine recomputes before the draw; 0x80/0x90/0xC4 are
not positions at all (the earlier reading of them as "matrix translation row" and "cached POV
location" was wrong - retire it). Whether 0x330 persists between dispatches (the writer's
re-base logic) was not separated out; the eyetest's own 120-present window shows the value
must be rewritten every dispatch, which the seam does.

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

## FovLever and the render size are ONE setting (2026-09-01)

Restoring GingasVR's tuned values after a detour explained the "uncanny" report, and the
refactor is not implicated: the frustum-fill block in `eye_quads.cpp` is byte-identical to
the original single file at `48766c07` - same clamp, same tan-linear UV mapping, same guard.
Everything that regressed was a **config value changed in session 2**.

Her tuned values, from the pre-session-2 ini backup: `RenderWidth/Height = 4032x2268`
(16:9), `SpoofDesktopW/H = 4096x2304`, `FovLever = 130`, `[PosTrack] Scale = 50`. Session 2
went to 2750x2850 with `FovLever = 100`.

**The lever and the render size are a pair.** With `XrFrustumFill = 1` (default since
38.13) the quad corners are clamped to `+/- tan(FovLever/2) * ScreenDist`:

| lever | clamp limit at D=1.6 | vs a Quest 3 frustum edge (~2.05 m) | result |
|---|---|---|---|
| **130** | 3.43 m | outside | **no clamp** - the quad fills the eye and samples the middle ~60% of a wider render. Edge to edge, nothing for reprojection to drag. |
| 100 | 1.91 m | inside horizontally, outside vertically | the clamp fires on one axis only: an **asymmetric border** returns, and with it the residual warping 38.13 exists to remove |

**Confirmed in an image, 2026-09-02.** `dump eyes` at `FovLever=100` shows the world inset in
the eye render target with a black border on all four sides - roughly 9% left, 10% top, 10%
bottom. Against the simulated Quest 3 frustum (l -54, r +44, u/d +/-55 deg) the clamp limit
at lever 100 is 1.91 m while the frustum reaches 2.20 m horizontally and 2.29 m vertically,
so it fires on every side. That border is the artifact, and it is what the tester described
as "the render square is at the top left of my vision and I can only see part of it".

So the design is: **render WIDER than the headset shows and let the fill crop**. Session 2
lowered the lever using the *authored quad* subtense formula (`W = 2 D tan(fov/2) * fill`,
`H = W / aspect`), which the frustum-fill branch overrides entirely - correct arithmetic
applied to the code path that was not running. Her 16:9 render also gives a per-eye half of
2016x2268 (aspect 0.889), close to a Quest 3 eye's 0.928; the square renders gave 0.518.

**Baseline restored** (her process rule 2: build on a snapshot confirmed good). The game
ini and all four AppCompat buckets are set to 4032x2268 to match, `PinBackbuffer=1` is kept
because it is the mechanism that makes that size hold on modern hardware, and the tester's
own hand-calibration neutrals are kept. `[PosTrack] Scale` is back to her 50 - the measured
100 below stands as a derivation and should be re-applied as a SINGLE change once the
baseline is confirmed, not bundled with the restore.

## The three rendering symptoms, and the one geometry that ties them (2026-09-02)

Measured from the 03:03 headset run at 4032x2268 requested / `capture: 3840x2160` actual,
`eye render targets: 2496x2688`, and the tester's report at those settings.

### 1. "Super pixelated, but the pause menu is huge like it's at full resolution"

**Both halves of that sentence are the same fact.** The frame is a side-by-side pair, so the
WORLD gets half the frame width per eye - `capture: 3840x2160 (per eye 1920x2160)` - while a
menu frame is MONO and each eye samples the **whole** 3840 across the same quad. The menu is
therefore drawn at exactly **twice** the horizontal sampling density of the world. The
tester's A/B is the cleanest possible confirmation of the SBS packing, and it is not a bug.

**What it costs**: 1920 per-eye columns are stretched across an eye render target 2496 wide,
a 1.30x upscale before the compositor's own resampling. To reach 1:1 the FRAME must be at
least twice the eye width: **2 x 2496 = 4992 columns**. At 3840 the world is at 77% of the
panel, which is what "really low resolution" is.

### 2. The fisheye is `FovLever`, and the code says so

`FovLever` does not only size the quad - **it writes the game camera's FOV**
(`fov_lever.cpp`, `LevWrite` into the camera's FOV field). At `FovLever=130` the game renders
a **130 degree horizontal** rectilinear frame, and the log confirms it:
`quad/fill: world scale is set by the MEASURED render FOV (fork dxvk_vr_proj) = 130.0 deg`
(129/130/137 across the run).

A 130 degree rectilinear render shown across a ~94 degree headset frustum stretches the edges
hard. That is the fisheye. The original author anticipated exactly this - `frame_hooks.cpp`
carries a safety net that disarms the lever if the rendered FOV overshoots the target, whose
comment is *"disarm rather than leave the user in a fisheye"*.

### 3. Why the black bottom border cannot be tuned away at 16:9

The quad's vertical subtense is `2*atan(tan(fovDeg/2)/frameAspect)`, so filling this rig's
99 degree vertical frustum requires:

| frame aspect | lever needed to fill 99 deg | horizontal render FOV | edge distortion |
|---|---|---|---|
| 1.778 (16:9) | **128.6** | 128.6 deg | severe - the current fisheye |
| 1.333 (4:3) | 114.6 | 114.6 deg | moderate |
| 1.25 (5:4) | 111.3 | 111.3 deg | moderate |
| 1.036 (near-square) | **100.5** | 100.5 deg | mild - the right answer |

**So the fisheye and the black border are the same setting pulling in opposite directions,
and at 16:9 there is no value that satisfies both.** The tester found this empirically:
"I couldn't find a balance where the scaling felt right where I couldn't see the bottom black
border". A taller frame is not a preference, it is the only way out.

**But taller costs sharpness**, because per-eye width is `frameWidth/2` (symptom 1). The two
constraints together want a frame that is both wide and tall:

| candidate | aspect | lever | per-eye | vs the 4992 needed for 1:1 | MP |
|---|---|---|---|---|---|
| 3840x2160 (now) | 1.778 | 129 | 1920x2160 | 77% | 8.3 |
| **3840x2880** | 1.333 | 115 | 1920x2880 | 77% | 11.1 |
| **4096x3072** | 1.333 | 115 | 2048x3072 | 82% | 12.6 |
| 4992x4800 (ideal) | 1.040 | 100 | 2496x4800 | 100% | 24.0 |

`3840x2880` is the cheapest step that buys the taller ratio at no sharpness cost - same
per-eye width, +33% pixels, and it drops the lever from 129 to 115, which is where the
fisheye should visibly ease. That is the next single change.

### Two corrections to session 4c

- **The mode-list claim was too strong.** 3840x2160 WAS honoured (`capture: 3840x2160`), so
  "must be a real display mode" is not the rule. The rule is narrower: **`PinBackbuffer=1`
  causes the crop**, because it forces the device to a size the game is not rendering at. A
  size the game will not accept merely falls back to one it will, which is harmless as long
  as the pin is off. Both effects were present at once at 2850x2750, which is what made them
  look like one.
- **There is no 2560x1440 cap.** That was read from the previous run's log, before the pin
  was turned off. 4032x2268 is still not honoured (its `setres` replies come back empty), so
  the requested number and the achieved number are different things: **trust `capture:`.**

## FovLever IS the vertical fill lever for a 16:9 render (2026-09-02)

Correcting session 4's own mistake, and completing "FovLever and the render size are ONE
setting" with the arithmetic that section was missing.

**The frustum-fill branch derives its VERTICAL extent from the frame aspect**
(`eye_quads.cpp:233-242`): `tanC = tan(fovDeg/2)`, `tanCv = tanC / aspect`, and the quad's
top/bottom are clamped to `+/- tanCv * D`. `fovDeg` is the measured render FOV, but the
lever raises it through the zoom floor: `if (fovLever >= 40) fovDeg = max(fovDeg, fovLever *
ZoomFillFloor)`. With `ZoomFillFloor=1.00`, **FovLever sets `fovDeg` outright** whenever it
exceeds the render FOV.

At `D=1.6` against this rig's frustum (x -2.202..+1.342, y -2.285..+1.546):

| render | aspect | lever | tanCv | vertical clamp | result |
|---|---|---|---|---|---|
| 2850x2750 | 1.036 | 100 | 1.150 | +/-1.84 | fills; the near-square frame carries the height |
| 3840x2160 | 1.778 | 100 | 0.670 | +/-1.07 | **67.7 deg of a 99 deg frustum - letterboxed** |
| 4032x2268 | 1.778 | **130** | 1.206 | +/-1.93 | edge to edge top and sides, ~9% black at the bottom |

**So a 16:9 render needs lever 130 and a near-square render needs lever 100.** They are not
independent, which is what the section title always said - this table is the missing half.
GingasVR ships 4032x2268 WITH FovLever=130 for exactly this reason.

**Session 4's error, recorded so it is not repeated.** The known-good restore set
`FovLever=100` (correct: it was paired with the near-square 2850x2750), and session 4c then
moved the render to 16:9 **without moving the lever**. That is the worst pairing available
and it produced the tester's "rectangular again so it didn't fill my view". Changing the
render aspect without changing the lever is not a one-variable change; the pair is the
variable.

**Second finding from the same run: the requested resolution was not honoured.** The mod
asked for 3840x2160 and the log records `capture: 2560x1440`. With `PinBackbuffer=0` this is
harmless - the device is created at what the game actually asked for, so buffer and content
agree and there is no crop - but it means **this rig will not render above 2560x1440**
(desktop 5120x1440, i.e. the monitor height caps it). Every "4032x2268" or "3840x2160" run on
this machine is really a 2560x1440 run. Anything derived from the requested number rather
than the logged `capture:` number is wrong.

## THE RENDER MUST BE A REAL DISPLAY MODE (2026-09-02) - the injected-mode crop

**This is the root cause of "tiny and in the top left corner", and probably of the project's
central open bug.** Six capture dumps on this rig, measured by non-black bounding box:

| requested buffer | actual content | is it a real display mode? | verdict |
|---|---|---|---|
| 1600x900 | 1600x900 | yes | **FULL** |
| 2560x1440 | 2560x1440 | yes | **FULL** |
| 3840x2160 | 3840x2160 | yes | **FULL** |
| 4032x2268 (GingasVR's own) | 3024x1440 | no - injected | CROPPED |
| 2750x2850 | 2750x2200 | no - injected | CROPPED |
| 2850x2750 (this rig's "known good") | **2560x1440** | no - injected | CROPPED |

**The game renders into the TOP-LEFT of the buffer and leaves the rest black.** At
2850x2750 the content is exactly 2560x1440 - 89.8% of the width, **52.3% of the height**.
Everything downstream then works on a frame that is half empty:

- **"tiny"**: the eye quad maps the WHOLE 2850x2750 frame onto itself, so the 2560x1440 of
  real content covers only ~90% x 52% of it.
- **"top left"**: the content is literally in the top-left of the frame.
- **"the eyes will not fuse"**: the SBS split assumes the halves meet at x=1425. The game
  drew its stereo pair inside 0..2560, so the halves actually meet at **x=1280**. The left
  eye is handed 0..1425 (its own view plus 145 px of the right eye's) and the right eye
  1425..2850 (the tail of the right view plus 290 px of black). Those cannot fuse, and no
  amount of separation, convergence or FovLever tuning can make them.

**Why the mod does not notice.** `PinBackbuffer=1` forces the DEVICE to 2850x2750 at
CreateDevice while the game keeps rendering at the size it asked for (the log records the
request: `CreateDevice the game asked for 2560x1440 windowed=0`). The mod then spoofs
`GetClientRect` to report 2850x2750, and the setres path READS THAT BACK and concludes
`setres: the game is already at 2850x2750 - skipping the resolution script entirely`. That
check cannot fail its own hypothesis: it is reading our own spoof. So the engine-side setres
that would genuinely resize the render never runs, and `capture: 2850x2750` is logged for a
frame that only holds 2560x1440 of picture.

**`PinBackbuffer` is not GingasVR's.** Her own tuned ini (`dishonored_vr.ini.pre-2750`) has
no `PinBackbuffer` line at all - it defaults to 0. The key was added by this project. Every
ini since carries `PinBackbuffer=1`, which is when the crop appears.

**Why this is probably "works only on her PC".** 4032x2268 is not a standard display mode
either, and it cropped to 3024x1440 here. Whether an injected mode is honoured depends on the
machine's GPU, driver and desktop mode - this rig's desktop is 5120x1440, and two of the
three cropped captures came back exactly 1440 tall. A user whose display happens to accept
the injected mode sees a correct image; everyone else gets a frame with content in one
corner and eyes that will not fuse. That is the reported shape of the central bug, and it
predicts that the affected users' desktop height is smaller than the requested render height.

**The fix is to request a resolution the display actually offers.** Applied for testing:
`RenderWidth/Height = 3840x2160`, `SpoofDesktopW/H = 3840x2160`, `PinBackbuffer=0`, with
`DishonoredEngine.ini` and all four `[AppCompatBucket1..4]` moved to 3840x2160 as well.
3840x2160 is 16:9 landscape (so the fork splices), measured FULL on this rig, and gives a
per-eye half of 1920x2160 = **aspect 0.889** - the same per-eye aspect as GingasVR's
4032x2268, which ENGINE_NOTES "FovLever and the render size are ONE setting" identifies as
the number that matters. 2560x1440 is the guaranteed-safe fallback: identical per-eye aspect,
a quarter fewer pixels, and the game asked for it itself.

**Instrument that should exist and does not.** Nothing compares the captured frame's real
content extent against the buffer size. A cheap non-black bounding-box check on the capture,
logged once per resolution change, would have caught this on the first run instead of the
fourth session. The setres check must also stop reading the mod's own `GetClientRect` spoof.

## MenuFillScale pumps the world size during GAMEPLAY (2026-09-02)

**Measured, not theorised**: the headset log of 2026-09-02 02:23 (VirtualDesktopXR, Quest 3,
the restored known-good ini). The tester reported "still rendering tiny and in the top left
corner". The log says exactly why, on one line, twice over:

```
quad: fov=100.0 fill=0.60 frameAspect=1.036 W=2.288 H=2.208 D=1.60 -> subtends 71.1 x 69.2 deg
quad: fov=100.0 fill=1.00 frameAspect=1.036 W=3.814 H=3.680 D=1.60 -> subtends 100.0 x 98.0 deg
```

40 rebuilds at `fill=0.60`, 6 at `fill=1.00`. The run ENDED at 0.60.

**The frustum it has to fill** is `L[-1.376 0.839 -1.428 0.966]` (tangents), i.e.
**94.0 deg horizontal x 99.0 deg vertical** (left 54.0, right 40.0, down 55.0, up 44.0). A
71.1 x 69.2 deg quad inside a 94 x 99 deg frustum covers about **half its solid angle**.
That is the whole of "tiny" - it is not a resolution, adapter, scale or convergence fault.

**The mechanism.** `eye_quads.cpp:134` applies `[Screen] MenuFillScale` (0.60 in the shipped
and known-good ini) whenever `g_menuOpen || g_inMenu || g_sbsMonoNow`, and `:204` skips the
`XrFrustumFill` branch on exactly the same condition. So one flag decides BOTH the size and
which geometry path runs. `present.cpp:19-41` then forces a quad rebuild whenever either
flag changes. The result is that the world size **pumps** as the flag flaps:

| t (ms) | fill | subtends |
|---|---|---|
| 52270750 | 1.00 | 100.0 x 98.0 |
| 52279406 | 0.60 | 71.1 x 69.2 |
| 52280593 | 1.00 | 100.0 x 98.0 |
| 52281328 | 0.60 | 71.1 x 69.2 |
| 52283375 | 1.00 | 100.0 x 98.0 |
| 52285125 | 0.60 | 71.1 x 69.2 |

Those are all AFTER the game reached gameplay (`crouch/raw` from 52273875 reports
`menu=0/0 ... mono=0`, the pawn moving, `pos=(6402,5110)`).

**It is the MENU flag doing it, not the splice counter.** `sbs:` logs on every change of
`g_sbsMonoNow` and its last transition is at 52263406 - before any of the pumping above. So
`g_sbsMonoNow` was steady while the fill alternated, which leaves `g_menuOpen || g_inMenu`
as the only remaining term. `present.cpp:613-616` already records why that flag is
untrustworthy in gameplay: the game's `Req_SaveSlotInfos` save-slot polls re-open it. The
38.x fix taught the MONO decision to prefer the splice counter over the menu flag for that
exact reason - but the **fill** decision and the **frustum-fill gate** were never given the
same treatment, so both still trust the flag the fork's counter was brought in to replace.

**Why it never showed on the Index.** Under OpenVR there is only ONE geometry path: the
authored quad. `MenuFillScale` shrinks a menu, which is what it is for, and there is no
second path to jump to. `XrFrustumFill` (38.13) added a second path for the OpenXR port
without making the transition between the two continuous, so on Quest the same flag flap
that merely dimmed a menu on the Index now swaps the entire quad construction mid-gameplay.
This is a good example of the class the mod's author flagged: SteamVR/Index was the tuned
target and OpenXR/Quest was a later port.

**The placement, worked from the logged frustum.** At `fill=0.60`, `D=1.60`:
`ccy = D*0.25*sum(vertical tangents) = -0.3696 m`, `ccx = 0`, `W=2.288`, `H=2.208`. Against
the left eye's frustum cross-section at D (x -2.2016..+1.3424, y -2.2848..+1.5456):

- **vertically: 21.2% black top, 57.6% world, 21.2% black bottom - symmetric.**
- horizontally, LEFT eye: 29.8% black on the temple side, 64.6% world, 5.6% on the nasal
  side; the right eye is the mirror. That per-eye asymmetry is the rigid-screen design
  (`:167-176`), not a fault, but it is why a small image reads as displaced sideways.

**This is a falsifiable prediction, and it contradicts the standing session 3c reading.**
Session 3c recorded a `dump eyes` showing the world in the "top ~54%, bottom half black".
The geometry above says the vertical border must be SYMMETRIC and about 21% on each side.
The next `dump eyes` decides it: symmetric borders confirm this model and retire the "sits
high" contradiction as a misread dump; a genuinely black bottom half falsifies it and means
something flips or crops vertically between the quad and the eye texture.

**Fix applied for testing (config only, no build)**: `[Screen] MenuFillScale=0.60 -> 1.00`,
which makes the menu-flag branch produce the same 100.0 x 98.0 deg quad as gameplay, so a
flap can no longer change the world size. Cost: menu edges crop, which is what 32.4 added
MenuFillScale to avoid. The proper fix is to stop letting the menu flag gate world geometry
at all - give the fill and the frustum-fill gate the same splice-counter-first rule the mono
decision already has - and that needs a build.

## World scale: 50 UU/m is a default, not a measurement (2026-09-01)

`[PosTrack] Scale` is 50, UE3's canonical 1 uu = 2 cm. One knob drives both the fork's
stereo separation and positional parallax, live on the F10 View tab ("world scale (uu/m)")
and on PgUp/PgDn (**PgDn = world smaller**, 5% a press).

**Prior art from another UE3 game.** BioShock Infinite's VR mod ships the same canonical 50
as its code default and its own notes say plainly that this is *not* the answer - "the true
value is the user's headset calibration". The value that came out GREEN in the headset there
was **150 UU/m**, three times canonical (source: the bioshock-trilogy-vr tree,
`docs/bioshockinfinite/ENGINE_NOTES.md` and `ROADMAP.md`, session s41 "world scale tuned to
150"). Its notes also carry the warning that applies here: BS1/BS2's calibrated 100 must
never be copied across, because it is a different engine. The same caution applies to
Infinite's 150 - it is a starting bracket for Dishonored, not a value to adopt.

So the first headset attempt at Dishonored's scale should sweep **50 -> 150**, not the
narrow 50 -> 74 that inverting GingasVR's static `dxvk_stereo.txt` marker (`sep=0.014
conv=140`) suggests. That inversion assumes the FOV he tuned at, which is unrecorded.

**Measured 2026-09-01: the separation write is honoured.** `depth:` logged
`sep asked 0.01397, fork reads back 0.01397` across a live sweep of 50 -> 73.9 UU/m
(eyes 3.16 -> 4.66 uu apart, sep +48%), and the tester reported no change in apparent world
size at all. So the proxy -> fork chain is healthy and **separation is not what makes the
world feel huge**. The likely reason it changes nothing perceptually: at convergence 140 UU
(~2.8 m at 50 UU/m) nearly everything in a room is past the distance where disparity still
carries size information, so angular size dominates - and angular size is pinned to 1:1.

### MEASURED: Dishonored is 100 UU/m, 1 uu = 1 cm (2026-09-01)

Derived by the movement-constant method below, from a walk-then-sprint run read off the
crouch diagnostic's `spd=` plateaus. Four clusters, all uncrouched: 124, 246-250,
**355-363 (14 samples)** and **537-545 (21 samples)**. The two strong plateaus are the
engine constants - **360 uu/s default, 540 uu/s sprint, ratio exactly 1.5** - and the
246-250 cluster is partial analog-stick deflection.

| scale | default | sprint | verdict |
|---|---|---|---|
| 50 (UE3 canonical) | 7.2 m/s | 10.8 m/s | **untenable** - Corvo would sprint at world-record pace and stroll faster than most people can run |
| 78 | 4.6 m/s | 6.9 m/s | fast |
| **100** | **3.6 m/s** | **5.4 m/s** | **a jog and a run - how Corvo actually moves** |
| 150 | 2.4 m/s | 3.6 m/s | a sprint slower than a jog |

Corroborated independently by eye height: 78.1 uu above the pawn origin plus a typical
~88 uu human collision half-height (1 uu = 1 cm) puts the eye at 1.66 m. `[PosTrack] Scale`
default changed 50 -> 100 in `config.cpp`, with the derivation in the comment and in the
generated ini's own help text.

**What this does and does not fix.** It corrects positional parallax amplitude, stereo
depth and hand reach - everything that maps a real metre onto game units. It does **not**
change apparent angular size, which is pinned at 1:1 by the frustum-fill path and is a
different lever entirely (`FillScale`). Do not expect it to make a too-big world smaller.

**How to DERIVE UU/m instead of tuning it by feel.** The Mirror's Edge VR mod (also UE3,
MIT) derives it from the game's own movement constants against known human speeds, and gets
three independent constants agreeing on **100 UU/m, 1 UU = 1 cm** - walk 200 UU/s = 2.0 m/s,
run 380 = 3.8 m/s, sprint 700 = 7.0 m/s (source: its `ENGINE_NOTES.md`, "World scale").
That is a falsifiable method rather than a preference, and it transfers directly: the crouch
diagnostic already prints `spd=NNuu/s`, so one run walking and sprinting in a straight line
gives Dishonored's own number. **UE3 is not uniformly 50 UU/m** - Mirror's Edge measured
100, BS1/BS2 calibrated 100, Infinite tuned to 150. 50 is only the engine's canonical
default, and a scale that is too SMALL makes the world feel too BIG.

Eye height is a weaker second estimate and is currently ambiguous: the crouch log gives
`camZ - pawnZ = 78.1 UU`, which is ~1.56 m at 50 UU/m if that origin is at the feet, but
~2.4 m at 50 (and ~1.6 m at 74) if it is the collision-cylinder centre. Resolve the origin
before trusting it.

**Separation changes DEPTH, not angular size.** With the frustum-fill path presenting the
measured render FOV at 1:1, apparent angular size is already correct by construction and
will not move with this knob. What moves is how far away things read, which is what makes a
correctly-scaled view still feel enormous. Note also that `[Screen] FillScale` (the "screen
fill" slider) is **inert while `XrFrustumFill=1`**, because the frustum-fill branch
recomputes the quad corners and discards the authored W/H.

40.2 logs the whole chain every 5 s - IPD, world scale, the resulting eye separation in game
units, xs, convergence, the separation asked for AND the value read back out of the fork -
so a knob that moves the hands but not the world names which half is dead.

## The "freeze then rescale" was VR injection, not a fault (2026-09-01)

Reported by the tester after the landscape fix: the square flat render appears, the image
freezes for about a second, and then the game is running in VR. That is the bring-up
sequence - the capture, the eye quads and the projection layer all coming up - and it was
only ever read as a glitch because what came out the other side was scaled so wrongly that
it did not look like a successful injection. The freeze itself has no fault behind it. What
remains open is world scale, above.

## THE RENDER MUST BE LANDSCAPE (2026-09-01) - the portrait splice refusal

The cause of "the eyes are super far off and both are zoomed in", and a self-inflicted
regression: session 2's resolution fix set the render to **2750x2850, which is portrait**.

`dxvk/src/d3d9/d3d9_device.cpp`:

- **Line 4381** - the main scene's splice gate ends with
  `else if (!(stVpS.Width > stVpS.Height)) stWhy = "rt-portrait";`
- **Line 4578** - the per-eye splice runs only `if (stWhy == kSplice)`.

So on a portrait viewport the fork **does not splice the main scene at all**. The world is
drawn **mono** across the full frame. The proxy, which has no idea, still hands eye 0 the
left half and eye 1 the right half (`eye_quads.cpp`, `u0 = eye ? 0.5 : 0.0`). Two unrelated
views that cannot fuse, each stretched across the whole quad and therefore magnified 2x.
That is precisely the reported symptom, and it is worse than a black screen because
everything downstream keeps reporting success.

**The splice counter lies about this.** Light shafts, shadows and the M8.1 quarter light
pass splice under *different* conditions (lines 4526-4527, `"mirrored-vp"` or
`"vp!=rt" && stQuarter`), so they keep working. The measured run showed `splices=85` with
the main scene never spliced, which kept `g_sbsMonoNow` false and the half-frame UVs on.
The fork's own log confirms it: only `shaftfix`, `shadowfix` and `M8.1 quarter light pass`
lines, no main-scene splice.

**Line 5996 carries the same landscape gate** on `dxvk_vr_proj`, the projection export:
`pjVp.Width >= 1024 && pjVp.Width > pjVp.Height`. So a portrait render also kills the one
MEASURED source of the rendered FOV. `g_liveFovX` stayed 0 for the entire session, the
frustum-fill path fell back to the ini constant `GameFOVDeg=100` with no log line, and an
assumed number set world scale from start to finish.

**Fix: 2850x2750** - the same two numbers swapped. Identical pixel cost, landscape by
100 px so the gate passes, full-frame aspect 1.036 so the eye quad subtends 100 x 98 deg at
`FovLever=100`, which is right for a Quest 3. `tools/setup-game-ini.ps1` now defaults to it
and its header explains why; keep `Width > Height` for any other value.

**This falsifies session 2's third "corrected belief".** "The eyes are not a stereo pair" was
recorded as disproved on a measurement of 32.7 mean-abs-diff static, 11.5 after a head turn,
read as ordinary parallax. Two different halves of one mono frame produce exactly that, and
the head-turn change is the image scrolling, not parallax. The eyes were genuinely not a
stereo pair. **A large diff between two crops is not evidence of stereo** - the test cannot
tell a stereo pair from two unrelated crops, so it never could have failed its hypothesis.

40.2 adds a proxy-side detector: a portrait capture now logs an Error naming the fork's
refusal reason and the fix, so this cannot be silent on both sides of the boundary again.

## The exit crash: reading the fingerprint correctly (2026-09-01)

Three records in `dishonored_vr_crash.txt`, register-identical, all after `PreExit`. The
session-2 reading of them ("0xDEDEDEDE freed-memory *writes* in d3d11.dll; the detached
pace thread is touching released D3D11 objects") is **wrong in both halves**, and both
errors came from the instrument rather than the engine.

**It is an EXECUTE fault, not a write.** `ExceptionInformation[0]` for an access violation
is three-valued: 0 read, 1 write, **8 execute (DEP)**. The fingerprinter tested it for
truth, and 8 is truthy, so every execute fault in this project's history printed as
"writing". The proof it is 8 is in the record itself: `ExceptionAddress ==
ExceptionInformation[1] == 0xDEDEDEDE`, and the module resolved to `?`. A data write would
have left `ExceptionAddress` at the faulting instruction *inside* `d3d11.dll` and printed
`[d3d11.dll+0x...]`. So EIP itself landed in freed memory - a **call through a poisoned
code pointer** (a freed vtable or callback), which is the opposite failure from a stray
store, and points at a destroyed COM object rather than at unsynchronised context use.

**The faulting thread is not the pace thread.** All three say `tid=... (other)`, and
`thread_name()` returns `"other"` only when the tid matches nothing in the registered
table. Both `present` (registered at `RenderEyesAndSubmit` entry) and `xr-pace` (registered
as the pace thread's first statement) are in that table before any crash can happen. The
direct faulter is a third-party worker - d3d11, the display driver, or the VR runtime. The
pace thread may still be the *cause* (it can outlive an object another thread then calls
through); it is not the victim, and instrumenting it as the victim will find nothing.

**Corollary for the register dump.** `ecx = esi = 0xDEDEDEDE` with `ebx = 0x24` and
`edi = eax - 0x20` is then not "a poisoned `this` being written through" but the poisoned
values still in the argument registers at the moment control was transferred - consistent
with a `thiscall` through a freed object's function pointer, made from
`d3d11.dll+0x4dfcd`'s call site (the return address in `esp[0]`).

Fixed in 40.2: `crash.cpp` decodes all three operations and names the
address-equals-EIP case explicitly.

## Why `dumps\` was always empty (2026-09-01)

Not a permissions or path problem. The crash file holds 3 `EXCEPTION` lines and **0
`minidump` lines**, which is direct proof that `unhandled()` never executed: UE3 wraps
`WinMain` in its own `__try` and installs its own filter, so the fault is consumed before
`SetUnhandledExceptionFilter`'s handler can run. The dump path was unreachable by
construction for the entire life of the project.

40.2 takes the dump from the **vectored** handler instead, which always runs, gated on the
instruction pointer resolving to **no loaded module** (`module_of` returns base 0). That
gate is fatal-only by construction - no `__except` frame can resume a thread whose EIP is
in unmapped or freed memory - so it cannot fire on the first-chance exceptions UE3 raises
and handles deliberately. It is also falsifiable: if the crash ever turns out to be an
ordinary in-module fault, no dump appears, and the wild-EIP reading is disproved by the
silence. `dbghelp.dll` is resolved once at `install()` time, never inside the handler, so
the VEH does not touch the loader lock.

**The original author read this fault correctly and session 2 inverted it.** The 38.79
comments say "EIP dededede after PreExit" and "a call through freed memory"
(`process_event.cpp`, `frame_hooks.cpp`). That is the execute-fault reading, arrived at
without the decode bug getting in the way. 38.79 acted on it by standing the **game**
thread down at `PreExit` - which is correct and necessary, and was never the whole path.

## The pace lane at shutdown (40.2)

38.79 set `g_xrRun = 0` and returned. Nothing waited. The pace thread can be up to 100 ms
inside `xrWaitFrame`, another 100 ms inside `xrWaitSwapchainImage`, or mid `CopyResource`
into `g_xriImg[eye][idx]` - swapchain textures **owned by the runtime**, never AddRef'd by
us, which stop existing when it tears its session down. So the exact fault 38.79 set out to
prevent still had an open path through the lane 38.79 did not close.

`XrPaceStop(why)` now joins with a 750 ms bound. On expiry the thread is **left running on
purpose**: `TerminateThread` would abandon `g_xrCs` held (deadlocking any later publish)
and abandon an acquired swapchain image the runtime is still tracking, which is worse than
the race. The error line is the instrument - a fault *after* it means the pace lane is
still the one to chase; a fault *without* it means the thread was already gone and the pace
lane is not the cause. The event pump's inner `while` also tests `g_xrRun` now, so a
runtime with an event backlog cannot hold the loop past a stop request.

Still not done, and deliberately not bundled in: `xrRequestExitSession` /
`xrEndSession` / `xrDestroySession` are never called. Doing that properly needs the pace
loop's cooperation and is a second behavioural change (rule 1 in the handoff's process
rules). Note also that `PreExit` is a UE3 script event: a kill or a hard crash never fires
it, and the join must NOT be moved into `DllMain(DLL_PROCESS_DETACH)`, where waiting on a
thread under the loader lock is a textbook deadlock.

## `XR_TIMEOUT_EXPIRED` is a success code (40.2)

`XrResult` is negative for failure only, so `XR_TIMEOUT_EXPIRED` (+1) makes `XR_FAILED()`
**false**. The pace loop's `if (!XR_FAILED(g_xrf.wait(...)))` therefore ran `CopyResource`
into an image the compositor had not finished reading - a data race with the runtime on the
one resource the headset displays, invisible because every call returns a success code. Now
tested as `== XR_SUCCESS`.

Same block: `g_xrpShown = seq` used to advance **before** the copies, so any frame lost to
a timeout was dropped permanently rather than retried. It now advances only once both eyes
have actually received the content. The image is still released after a timeout to keep
acquire/release paired - an unreleased image starves the swapchain within a few frames,
which is a hard stall rather than one stale frame.

## Evidence handling (both cost a session)

- **Log rotation is one deep.** `dishonored_vr.log` + `.prev.log` only. Two simulator runs
  after a headset session destroyed both headset logs; only the crash text survived, and
  the surviving pair contained no `EXCEPTION` and no `PreExit` at all. Copy the log out
  before the next launch, always.
- **The crash file had no run identity.** It is opened `FILE_APPEND_DATA` / `OPEN_ALWAYS`
  and accumulates forever with nothing separating runs. `dvr-xrsim` and VDXR fault into the
  same `d3d11.dll` and produce byte-identical fingerprint text, so three records could not
  be attributed to a backend, a runtime or a build. 40.2 writes one header per run (wall
  clock, `DVR_VERSION`, `DVR_BUILD_ID`, pid, backend + runtime name via
  `dvr::crash::set_context`, called from the OpenXR backend once the runtime names itself).

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

## The projection layer is gated on a gameplay verdict (2026-09-03, first AER run)

**Measured, headset, build `alpha-175-g13642778`.** AER tagged both eyes perfectly and the
player saw mono flickering side to side. The two log lines that name it:

```
xr: first projection-layer frame (claimed hfov 137.0 deg)
xr: cinematic quad ON (strict=0 stale=1 fovMismatch=0 screenOnly=0)
stereo: beat method=aer out/s=46 L/s=23 R/s=23 mono/s=0 none/s=0 3840x2160
```

`core/vr/openxr_runtime.cpp`'s cinematic fallback drops the projection layer to the M2 quad
while the published gameplay verdict is false **or stale** (`kCineStaleMs`). An adapter that
never calls `publish_gameplay_view` reads `stampMs == 0` -> permanently stale -> the quad
wins forever. Every per-eye frame was still rendered with the right camera offset and
captured into the right eye swapchain; it was thrown away one step later.

**The percept is diagnostic.** One image in both eyes whose camera alternates +/- IPD/2 IS a
picture flickering side to side. If a per-eye method ever looks like that again, the eye tags
are not the suspect - the layer is. `status.json` -> `aerProjection` / `aerCineQuad` answers
it without a headset, and `vrcine off` is the live A/B.

**The general rule this is an instance of.** "The eyes are tagged correctly" and "the eyes
reach the headset" are different claims. The beat line only ever measured the first, and it
read green through the whole failure.

## The game renders FULLSCREEN regardless of `[SystemSettings] Fullscreen=False`

Same run. `[SystemSettings] Fullscreen=False` and `ResX/ResY=2750x2850` were set, in the
engine ini AND all four `[AppCompatBucket]` sections, and nothing anywhere in
`My Games\Dishonored\...\Config\*.ini` contains 3840 or 2160. The device came up:

```
CreateDevice -> 0x00000000 (3840x2160 windowed=0)
capture: 3840x2160 fmt=21
```

**`windowed=0`.** In fullscreen, D3D9 can only take a real display mode, and 2750x2850 is not
one - so a near-square render is unreachable by ini alone on this rig. This is why the
pre-41.0 builds carried the desktop-size spoof (`SpoofDesktopW/H`): it existed to make an
oversized/odd render acceptable, and 41.0 removed it deliberately.

The route that does not bring that machinery back is UE3's own command line, which beats
every ini: `-windowed -ResX=2750 -ResY=2850` (Steam launch options). A windowed D3D9
backbuffer is not required to be a display mode. Unverified as of this writing; `capture:` is
the ground truth, never the requested number.

**Why the aspect is worth the trouble under AER.** Each game frame IS one whole eye, so the
render should carry the EYE's aspect. At 3840x2160 (1.778) with a 100 deg hfov claim,
tanV = tan(50)/1.778 = 0.670 against the Quest 3 eye's 1.428 - the render fills about 47% of
the eye vertically and the rest is black band. At 2750x2850 (0.965 vs the eye's 0.928) it
very nearly fits.

## CylTruthLive was parasitic on motion crouch (2026-09-03, second AER run)

**One dead signal, three unrelated-looking bugs.** `CylTruthLive()` answers "a gameplay pawn
exists" and reads `g_cylOkMs`, which was refreshed ONLY as a side effect of
`PawnCollisionHeight()` being called from the motion-crouch paths. `[Mode] GamepadOnly=1`
disables those, so on the shipped gamepad-only build the oracle never ticked. Measured over
35 s of gameplay at 116 fps, windowed, 1355x1405:

```
[game] state: NO_PAWN            <- logged ONCE, never transitioned
heartbeat: head hits=350 writes=350 | menu=1 (script=1) wheel=0
xr: cinematic quad ON (strict=0 stale=0 fovMismatch=0 screenOnly=0)
stereo: beat method=aer out/s=116 L/s=58 R/s=58 mono/s=0 none/s=0
```

Everything downstream agreed with the dead oracle:

1. **`[game] state:` stuck at NO_PAWN** - so `boot.ps1`'s wait line and `status.json` were
   both wrong all session.
2. **The stale-menu ghost clear never fired.** It is gated on `CylTruthLive()` (deliberately,
   38.17: the main menu's 3D background also dispatches view rotations, and a live pawn is
   the discriminator). `g_menuOpen` **defaults to true** and only clears on a script event
   (`MenuClosed` / `ResumeGameClicked` / ...) that the auto-continue-into-save path never
   sends. So the flag stayed true for the whole run: the head-mouse parks, and the pad chops
   the movement stick into discrete menu pulses and hard-zeroes the right stick.
3. **The runtime's cinematic fallback read "not gameplay" forever** and dropped the
   projection layer for the mono quad - discarding every correctly tagged per-eye frame while
   the beat line read a perfect `L/s == R/s`.

Fixed by calling `PawnCollisionHeight()` from `DvrPreTick` beside `GameStateTick`,
unconditionally - two guarded dereferences returning -1 with no pawn. It is not a motion
control and should never have lived behind one.

**The general shape, worth carrying to the other adapters:** a signal that several subsystems
depend on must be refreshed by something that always runs, not as a side effect of an
optional feature. The failure is silent and it does not look like one bug.

**Windowed mode removed the last safety net.** The mod logs it plainly:
`menu: game is WINDOWED - the desktop cursor is always showing, so the cursor half of the
menu test is disabled and script events are the only menu signal`. So the `-windowed` switch
that fixed the render size is also what let the stuck menu flag bite. Both are true at once;
neither is a reason to undo the other.

`vrcine on|off|status` is on the command seam now - the live A/B that separates "the per-eye
method is wrong" from "the verdict is wrong" without a rebuild.

## camera+0x60 IS the camera's right row - MEASURED (2026-09-02, session 7)

**The eye-separation direction is settled, and it is not the bug.** `camera::apply_eye_offset`
lays +/- IPD/2 along `cam + kCamRight` (0x60), an offset that had been ASSUMED since 41.0 and
never derived. One run logged `right=(-0.000 1.000 -0.000)` - the world Y axis - which is what
a fixed world axis would look like at yaw 0, and a fixed axis would make the two eyes drift
apart and back together as the player turns. That reads exactly like the reported flicker.

**The instrument that settled it.** A stream of `right=(x y z)` lines cannot answer this: it
needs a reader who remembers which way they were facing. So the seam now derives the yaw the
row implies (a roll-free UE3 basis gives `right = (-sin yaw, cos yaw, 0)`, so `yaw =
atan2(-r.x, r.y)`), tracks how far that has swept, and tracks **the HMD's own yaw as the
control** (`camera::note_head_yaw_deg`, fed from `head_track`). The control is what makes the
reading falsifiable: a row that never moves and a player who never turned are different
answers, and without the head sweep they print the same zero.

**Measured on the simulator lane** (build `alpha-186-gf8640b63-dirty`, RelWithDebInfo, the
game in gameplay through Steam on `dvr-xrsim`, `[Stereo] Method=aer`, head driven round a
full circle in 30 degree steps with `xrsim-cmd "head rot <yaw> 0 0"`):

```
camera/eyesep: eye -1 right=(-0.303 +0.953 -0.000) = yaw +17.6 deg | swept so far: row 0 deg vs head 0 deg
camera/eyesep: eye -1 right=(+0.191 +0.982 -0.000) = yaw -11.0 deg | swept so far: row 29 deg vs head 30 deg
camera/eyesep: eye -1 right=(+0.434 -0.901 -0.000) = yaw -154.3 deg | swept so far: row 172 deg vs head 180 deg
camera/eyesep: VERDICT the right row ROTATES WITH THE VIEW - it swept 258 deg while the head
  swept 300 deg. camera+0x60 is a real camera basis row ...
```

Final reading in `status.json`: `rowSweepDeg 375.2`, `headSweepDeg 390.0`. Every sample had
`r.z` exactly 0 and unit length, which is the shape of a roll-free right row and not of a
cached constant. **camera+0x60 is the camera's right row.** The offset MAGNITUDE was already
right (3.09 uu = half of 63.0 mm at 98 uu/m), so the whole write is correct and the flicker
has to come from somewhere else.

Two things the run did NOT settle, both cheap to look at next:

- **The row's yaw runs OPPOSITE to the HMD's** (head +30 gave row -28.6). That is either the
  mod's `g_hmdYaw` convention or the sign in `atan2(-r.x, r.y)`, and neither affects the
  offset (the vector is used directly, never the derived yaw). It WOULD matter if 0x60 turned
  out to be the camera's LEFT row - the eyes would be swapped and depth inverted, which is a
  different percept from flicker.
- The row sweeps ~96% of the head's sweep, not 100%. Expected: yaw is injected as a delta and
  the samples are one per second against a stepped head.

## The pawn oracle reads a source that never fills (2026-09-02, session 7)

**The 41.1 fix cured the plumbing and not the source, and the same three bugs are still live.**
`2d131622` moved the `PawnCollisionHeight()` call out of the motion-crouch paths and into
`DvrPreTick` so it runs every present under `[Mode] GamepadOnly=1`. It does. But the function
read `g_pePawn`, and **`g_pePawn` is never set in this game.**

`PeLatch` (`ue3/process_event.cpp`) names a pawn only when `ProcessEvent` dispatches ON an
object whose class name contains `"PlayerPawn"`. `DishonoredPlayerPawn` exists (the name
passes the filter) but it is native and dispatches no script events. Measured over a 5.5
minute gameplay run, same build and run as above:

```
handmesh: latched controller 'DishonoredPlayerController' @ 17BA6800 (from the event stream)
[game] state: NO_PAWN            <- logged ONCE at t+4.5 s, never transitioned
```

`latched pawn` never appears; the script-event census over the whole log carries
`DishonoredNPCPawn`, `DishonoredPlayerController`, `DishonoredPlayerInput`, `DishonoredHUD`
and no player pawn. `status.json` ended the run at `state NO_PAWN`, `menuOpen true`,
`inMenu true` - and the game was demonstrably IN a level, because `DisGFxMoviePlayerPauseMenu`
dispatched at the end of the run (a pause menu cannot exist outside gameplay).

Everything downstream agreed with the dead oracle, exactly as the 2026-09-03 entry above
predicted it would:

```
aer: tagging eyes but the CINEMATIC QUAD is up - the projection layer is being dropped for
     the flat screen  (every 5 s for the whole run)
status.json: aerProjection true, aerCineQuad true, aerLeft 13448, aerRight 13447, aerLRGap 1
```

**So AER tagged 26,895 eyes perfectly and every one of them was thrown away for the mono
quad** - which is the percept ENGINE_NOTES already named: one image in both eyes whose camera
alternates +/- IPD/2 IS a picture flickering side to side. That is a far better fit for the
tester's report than the separation direction ever was.

**The fix, and why it is the right one.** `PlayerController+0x248` is the possessed pawn: it
comes from the original author's measured field table and the 32.40 census, and `crouch.cpp`
and `hands/fp_mesh.cpp` have both preferred it over the event latch since then - but only
inside motion-control code, which `GamepadOnly=1` disables. It is now `kPcPawn` in
`patterns.h` and `PossessedPawn()` in `crouch.cpp`, tried first with the event latch as the
fallback, and `PawnCollisionHeight()` reads it. The oracle also names its own owner on every
change now (`pawn oracle: source now ctrl+0x248 ...` / `... none <-- NO GAMEPLAY PAWN ...`),
so a dead one can no longer look like a healthy one.

**The general shape, restated one level down:** the 2026-09-03 entry said a shared signal must
be refreshed by something that always runs. That was true and it was not enough. It must also
READ from a source that fills - and "the refresh is on a path that always runs" hides the
difference, because both failures print the same silent zero. BUILD-VERIFIED ONLY: nothing has
run since the change.

## Three things BRVR does that our AER does not (2026-09-02, session 7)

The pawn-oracle fix landed and was verified in a headset run - `pawn oracle: source now
ctrl+0x248`, `[game] state: GAMEPLAY`, `xr: cinematic quad off (strict=1 stale=0)`, so the
projection layer now genuinely reaches the headset. **The flicker survived, and its shape
changed: the tester reports each eye now flickering INDIVIDUALLY.** That is a different
fault from the mono-quad one and it points at the pair.

Read against BioShock Remastered VR (`docs/reference/bioshock-remastered-vr/`), which runs
the same one-eye-per-Present shape and paid for all three of these. Sources cited per item.

### 1. ONE WORLD ADVANCE PER EYE PAIR - we have no equivalent at all

`BioshockVR/Camera/CameraHook.cpp`, "PHASE 10a: ONE WORLD ADVANCE PER EYE PAIR":

> Option-B carry: pass 0 on the right-eye frame, (delta + carry) on the left. The world then
> advances ONCE per stereo pair instead of once per eye. **That is the entire cause of the
> bathysphere doubling - 4.2ms of world motion between the two eye renders, which near
> geometry turns into unfusable disparity.** Nothing is discarded, so full speed is preserved.

They hook the game-thread frame-delta function and zero the delta on the right-eye frame,
carrying it into the left. `docs/INVARIANTS.md` states it as a rule: "`DeltaClamp` advances
the world once per eye *pair*, so both eyes represent one instant. Reset carried state when a
new world identity appears."

**We do not do this.** Our two eyes are two consecutive game ticks, so at the measured 121
fps the eyes are **8.3 ms of world motion apart** - twice BRVR's 4.2 ms. Every moving thing
(the player's own translation, animation, particles, water, weapon bob) is at a different
instant in each eye. That is unfusable disparity by construction and it is the leading
suspect for per-eye flicker.

Note what this implies for the ladder: **rung 3 (SequentialReentry) does not have this
problem at all**, because it draws both eyes from ONE game tick. The delta clamp is the price
of rung 2, and it belongs in the S3 comparison.

### 2. THE PROJECTION LAYER MUST CARRY THE POSE THE IMAGE WAS RENDERED FROM

`docs/INVARIANTS.md` and `docs/modules/render.md`:

> The projection layer carries **the latched pose the image was rendered from**
> (`CameraHook_GetLatchedPose`), not the freshest pose at submit time. This was a major
> flicker fix - do not "improve" it by sampling a newer pose.

and in the code (`Render/XRSession.cpp`, `SubmitPair`), commented "flicker fix, section 2":

> stamp the pose the image was RENDERED from -- the eye-0 latched pose -- not the fresh one.
> Fresh-vs-latched mismatch = ~3 deg of baked-in yaw at 200 deg/s = the flicker.

BRVR gives BOTH eyes the latched orientation and puts each eye at `latchedPos +/- half-IPD
along the LATCHED right vector`, "same sign convention as the camera write".

**We stamp the runtime's own located per-eye poses.** The machinery to do it BRVR's way is
already in the runtime layer (`parallel_eye_tag`, `set_eye_tag_rendered`, `g_poseLag`) and it
is **default OFF with nothing in Dishonored arming it** - it was written for the Infinite
adapter. `vreyetag rendered|located` is the live A/B now.

Two caveats worth stating before anyone reads a result:

- Even armed, `parallel_eye_tag` reconstructs from the runtime's LOCATED pair, not from the
  game camera. In Dishonored the camera yaw is stick turn composed with a head DELTA, so the
  game camera's orientation is not the located head orientation at all. A true latched-pose
  channel (publish the rotation the script lane actually wrote, read it at submit) is the
  full fix; `vreyetag rendered` only removes the cant/IPD half of the mismatch.
- On a runtime whose located views are already parallel at the same IPD (the simulator) the
  rendered tag is bit-for-bit identity, so **this lever cannot be tested on the simulator** -
  it needs the headset.

### 3. ANYTHING FLAT MUST TAKE ONE EYE

`docs/INVARIANTS.md`, verified in a headset twice:

> **Anything flat must take ONE of them.** The menu quad submitted on both Presents, so the
> flat panel alternated between the left and right views - one IPD of horizontal shimmer on a
> surface being read. The desktop mirror had the same bug for the same reason, reported as
> *"on the monitor it flickers a lot which means people cant record it"*. Both now take
> `eye == 0`.

Worth auditing here for the F10 overlay and any quad that survives alongside the projection
layer: our overlay is drawn INTO the per-eye texture (`aer.cpp` calls the overlay draw on
every tagged present), so it is composited into whichever eye that present belongs to and
alternates exactly as BRVR describes.

### The instrument that was missing, and why the last run could not answer this

The heartbeat printed `headset(submits)=121fps` next to `GAME=121fps`, which reads as one
submit per present - but `dvr::frame::submit_count()` counts **presents that produced a
texture**, not `xrEndFrame` calls. The line was named for a quantity it does not measure, so
a pair that never closes and a pair that closes every time printed the same number. Fixed:
the heartbeat now prints `eyetex=N/s` and `xrEndFrame=N/s` separately, and `aer: pair` prints
the full `[pair]` probe every 3 s (pairs, aborts by kind, ring depth, per-eye captures,
staleL/R). BRVR's health signal for the same thing: "~2 Presents per XR submit. `EYEQ: depth
min=1 max=1`".

**Read it as: `xrEndFrame` must be HALF `eyetex`.** Equal means the pair never holds and every
XR frame carries one fresh eye beside one stale one - which is per-eye flicker on its own,
independent of 1 and 2 above.
