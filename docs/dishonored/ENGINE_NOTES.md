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
