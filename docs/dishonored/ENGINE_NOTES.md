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

## Head roll: UE3 positive roll = right ear DOWN (2026-09-03, measured by picture)

The second headset run reported the head TILT reversed under the projection layer. The
roll telemetry on the `headtrack:` line (41.1) first proved the write LANDS - `incoming`
(what the engine hands ProcessViewRotation) equals `wrote` on the next dispatch, so the
engine keeps our roll and it reaches the render. The direction was then measured on the
simulator by picture (run 37): `head rot 0 0 20` rolls the simulated head right-ear-down
(the simulator's roll is a rotation about the forward axis, `quat_from_ypr`), and the
game's own frame showed the world's verticals leaning with their tops to the RIGHT. A
right-ear-down head must see them lean LEFT. Reversed, as reported.

Cause: `g_hmdRoll = atan2(right.y, up.y)` is positive for the right ear UP, and UE3's
rotator roll is positive for the right ear DOWN (the picture: writing a negative roll
rolled the camera left-ear-down). The value is now negated once at its derivation, so the
ProcessViewRotation write, the matrix injection and the lean counter-rotation all inherit
UE3's sense; `[HeadInject] FlipRoll` stays the A/B override with 1 = the measured
direction. Re-measured after the fix (run 38): +20 leans the verticals left, -20 right,
the exact mirror of the faulty frames; pitch and yaw untouched (hmd pitch -30 -> view
pitch -5461 units = -30 deg).

## The camera field holds the POSITION, c5 is its negation (2026-09-03, corrects session 5)

**This one sign put every eye offset and every lean backwards in the headset.** Session 5's
`camera eyetest` measured that camera+0x330 reads exactly -c5, assumed c5 was the camera's
world position, and recorded the field as holding the NEGATED position (`kFields[].sign =
-1`). Everything downstream then wrote its offsets negated.

**The measurement that settles it** (run 35, the simulator, the sewers, a DIFFERENTIAL
picture test against a known-good reference - the c0 `LeanVP` patch, shipped and
headset-tuned since 30.35): command the same head displacement on both lanes under a
projection layer and dump the game's own frame.

| displacement | vp lane (`LeanVP`, the reference) | camera lane, sign -1 (session 5) | camera lane, sign +1 |
|---|---|---|---|
| 2 m right | the corridor wall on the LEFT, the city outside on the right (the camera went right, through the wall) | MIRRORED: wall right, city left (the camera went LEFT) | matches the reference |
| 2 m up | up inside the ceiling pipe, the corridor floor below | the camera went DOWN, under the floor looking up | matches the reference |

So writing +X into camera+0x330 moves the rendered view by +X: **the field holds the
camera's world POSITION, and c5 (the vertex constant) is its negation.** Two independent
confirmations: the shipped 38.24 crouch clamp writes a world Z into these same fields and
compares it against the pawn's world Z (which only makes sense for a position), and it was
verified to fix crouch; and the picture above.

`kFields[]` now carries two numbers: `sign` (what a wanted WORLD displacement is multiplied
by to become the field delta: +1) and `c5Sign` (how c5 answers it: -1 on the POV fields).
The eyetest asks for VIEW travel and expects c5 to move by `uu * c5Sign`; it still reads
**HONOURED 119/120** (c5 -99.2 uu for +100 asked), the postest reads HONOURED on both axes
(+29.8 for +30 right, -24.4 for -25 up), and the doubling is unaffected (L/s 52 R/s 51,
draws = 2nd = 52).

**The trap, for the next reader.** The eyetest was an instrument that could not fail its own
hypothesis: it wrote into a field and measured a DIFFERENT engine quantity (c5) that moves
with it, so it proved "the renderer noticed" and was read as "the renderer drew from the
offset position". Only the picture can answer the direction. Any future write point needs a
differential picture test against a known-good path before its sign is believed.

## Positional tracking on the camera seam (2026-09-03, S1)

The camera object carries a row-major basis at +0x50 (forward), +0x60 (right, `kCamRight`),
+0x70 (up); the original author's `FindPovRotators` matched +0x50 against the POV
rotator's forward, and 41.1 reads all three (`kCamFwd`/`kCamRight`/`kCamUp` in patterns.h),
validating them orthonormal before the first write. Measured at yaw 0 in the auto-continued
save (run 20): `fwd=(1.000 0.000 -0.002) right=(-0.000 1.000 -0.000) up=(0.002 0.000 1.000)`,
pairwise dots 0.000 (UE3: X forward, Y right, Z up).

Lean/crouch/roomscale used to ride the c0 view-projection patch (`LeanVP`), a matrix patch
the renderer's attachments do not follow. `[PosTrack] Lane=vp|camera` (default vp, the
shipped path; `postrack lane <l>` live) moves the offset onto the camera seam's write: ONE
write per dispatch of `base - (eye + position)` into camera+0x330, the position offset
resolved along the basis rows. The seam is the single owner of the offset (TrackHead
publishes it there; both lanes read it there), and the `camera postest <R> [U] [F]`
instrument overrides it with a commanded triple, takes 45 presents of c5 baseline at zero,
then judges 120 presents:

| lane | asked (R, U, F uu) | measured (c5 travel on the basis rows) | verdict |
|---|---|---|---|
| camera | +30, 0, 0 | +29.7, +0.2, +0.0 (mean of 120; 2284 seam writes) | HONOURED |
| camera | 0, -25, 0 (a crouch drop) | +0.0, -24.4, -0.0 | HONOURED |
| camera | 0, 0, +40 | +0.0, +0.4, +39.7 | HONOURED |
| camera | +30, +25, -40 | +29.7, +25.2, -39.7 | HONOURED |
| vp | +30, 0, 0 | the c0 patch ran on 120/120 presents (2760 uploads) | APPLIED (c5 cannot see a matrix patch) |

**Where that table was measured.** Run 20's rows come from the TITLE SCREEN's attract
camera (`DishonoredPlayerCamera_0`), which the state line called GAMEPLAY (the next section).
Re-measured in real gameplay (run 21, the Dunwall Sewers level, `DishonoredPlayerCamera_1`,
ProcessViewRotation dispatching at 270/3 s): `camera eyetest 100 0x330` HONOURED 120/120
(+100.0 uu), `camera postest 30 0 0` on the camera lane measured R+30.0 U+0.2 F-0.0. The
same two instruments on the level's LOADING screen ("press any key to continue", no script
dispatches) read DISCARDED / NOT HONOURED with c5 frozen: a static camera the loader
re-sets every tick. So the write point holds on the attract camera and in gameplay, and
an instrument run must know which camera it measured: the `[game] state` line now says.

So the renderer draws from the offset position on the camera lane within 1-2 %, on all
three axes, the same write point the eye offset uses. What the instrument does NOT say:
the vp lane's effect is a matrix change c5 never sees, so "the same travel on both lanes"
is only half measurable; and a shot diff on the mono screen (a simulated 40 cm head step)
reads 1.5-2.1 mean-abs-diff on both lanes against a 1.3-1.7 return-to-zero control, i.e.
the scene's own animation swamps a 14 uu lean at the quad's 16 % coverage. The picture
verdict belongs to a stereo method's projection layer (`world-6dof.xrs` `w_trans_x`).

Two facts found on the way. (1) The 38.24 eye clamp lives inside `FovLeverApply` AFTER
its early return, so it is dead whenever the lever is off - which is the mono screen's
default (`[Screen] FovLever=0`). The camera lane's writer caps its Z at the same ceiling
while the clamp is live (`camera::set_eye_ceiling`), so the two never fight; with the lever
off neither runs. (2) The seam writes ~19 times per present (every ProcessEvent dispatch
of the camera pass), which is the cadence the 38.24 clamp and the FOV lever already used.

## The game state under GamepadOnly, and the projection gate (2026-09-03, S2)

The `[game] state` line read GAMEPLAY on the title screen, in the main menu and on a
loading screen (runs 21-22; the simulator captures showed "Press any key" while the
instruments ran). Cause: every script-event tracker (the pause-menu open/close names, the
`OnToggleCinematicMode` latch, the UI vocabulary) sat inside the motion-aim block behind
`g_maimEnabled`, which `[Mode] GamepadOnly=1` turns off, and windowed mode has no cursor
test (32.9). The same class of bug as the 41.0 PreExit handler. 41.1 hoists the tracking
out (only the fire-window arm stays gated) and adds what the runs measured:

| screen | signal (measured) | state |
|---|---|---|
| title screen / main menu | `Start`, `OnFocusGained`, `BackToStartScreen`, `Req_CanContinueGame` on an object whose class contains `MoviePlayerMainMenu`; leaves with `UnregisterControllerDelegates`. Its own flag (`g_mainMenu`): the stale-flag ghost test clears `g_menuOpen` while dispatches flow, and the attract camera behind the menu dispatches | MENU |
| the attract scene's cinematic toggle | `OnToggleCinematicMode` fires ON at the title screen with the attract pawn already latched, so a level load INHERITED the latch (CINEMATIC in the sewers). A new pawn latch now clears it: a fresh level starts clean; in-level cutscenes keep their parity | CINEMATIC only in a cutscene |
| loading screen ("press any key to continue") | no ProcessViewRotation dispatch for 750 ms with a live pawn (the head-write counter read 0/3 s); the loader dispatches a burst about once a second, so leaving LOADING needs one second of continuous dispatches | LOADING (new) |
| gameplay | pawn live, no menu, no cinematic, dispatches continuous | GAMEPLAY |

The runtime layer's projection path is gated on the same verdict: `frame_hooks` arms
camera mode when the active method (or `stereo projection on`) claims a projection layer
and publishes the verdict every present; the runtime's cinematic fallback (3-present
hysteresis) drops to the quad screen on a false verdict and returns on a true one. Walked
in run 24: title MENU -> quad, main menu MENU -> quad, load LOADING -> quad, level
GAMEPLAY -> `xr: cinematic quad off`, two projection views, both eyes 72 % non-black.
Before this, the runtime's projection machinery had no caller in this game at all (camera
mode was the overlay checkbox only, and a stale verdict would have pinned the quad).

Two FOV facts from the same runs. Under a projection layer the lever's target is the
runtime's circumscribed hfov (137.0 deg at 16:9 on the simulated Quest 3; vfov 110.0 for
the 54/55 deg half-angles) and the 0x53c sensor followed it to 137.0 within a second in
gameplay (the engine interpolates: 136.1, 136.2 ... per present), the claim reading
`src=readback`. The loading screen's camera ignores the lever (sensor 75.0, the level's
natural FOV). And the lever captures its "natural base" whenever it re-arms: after an
arm-disarm-arm it captured 137 (the FOV its own previous writes had left on the
controller), so the ratio law then scales from 137, which is harmless for the target but
means "natural" is not the game's 75 any more - a re-arm should reset the FOV first if the
number is ever used as a baseline.

The simulator's `claimRatioH` reads 2.17 under this lever by construction (the claim's
tan(68.5) against the eye's own mean half-tangent, tan(54)/tan(44)): BioShock rendered
the eye's own FOV, this lever renders the circumscribed one. Not a magnification error
while the claim equals the render; `fovaudit src=readback` is the check.

## The scene-draw root, derived live (2026-09-03, S2b)

The SequentialReentry seam needs the ONE function whose call tree draws the scene and
enqueues the present, called once per tick from the engine's tick. Derived live in runs
26-27 with the instruments in `game/dishonored/scene_probe.cpp` (`reentry census`, `reentry
stack event|caller|present`, `reentry probe`, `reentry findstart`), the method BioShock
Infinite's session 40 used (bioshock-1-vr-mod, ENGINE_NOTES "the render root"), then
confirmed statically with `tools\pe-xref.ps1`. Static walking alone was not attempted: on
Infinite it failed twice.

**1. The caller census at the camera write.** `ProcessViewRotation` (the head-tracking
write's dispatch) is dispatched from ONE call site, `call eax` returning to `0x005d0789`,
693 times in 693 presents (once per present); the dispatching object is the camera
modifier (`CameraModifier_CameraShake`). The script thread and the present thread are
DIFFERENT threads (script tid 16012, present tid 16848 that run): a render thread presents;
this game is Infinite's substrate (threaded, `OneFrameThreadLag`), not BioShock 1's.

**2. The one-shot stack scrapes** (`RtlCaptureStackBackTrace` is cut to 3 frames by
frame-pointer-omitted code; the raw call-preceded scrape walks the whole chain and, for an
`E8` site, names the function the frame ENTERED). Two chains on the game thread, walk-up
order, outermost frames last; they SHARE their outer half:

| ret | enters | role |
|---|---|---|
| `0x00ec44f3` | `0x009e3c60` | the main loop's per-frame body |
| `0x009e3d20` | `0x009e3b90` | |
| `0x009e3c4a` | `0x009e3980` | |
| `0x009e3b1f` | `0x009e03b0` | the frame function that calls the engine: at `0x9e0555` it does `mov edx,[ecx]; mov eax,[edx+0x124]; push ecx; fstp [esp]; call eax` = `GEngine->Tick(DeltaSeconds)` (a virtual with ONE float argument) |
| `0x009e055a` | (virtual) `0x00a17890` | `UDishonoredEngine::Tick` (0 direct callers, 1 `.rdata` vtable reference - a virtual, as it must be); it calls `0x00632860` at `0xa1799f` |
| `0x00a179a4` | `0x00632860` | **`UGameEngine::Tick`** (1 direct caller = the subclass above, 1 vtable reference); both chains below live inside it |
| tick chain: `0x00632a09` | `0x0065e0d0` | the world tick (1 caller) -> ... -> `0x005d0710` (1 caller) -> `call eax` at `0x5d0784` = the `ProcessViewRotation` dispatch. **The camera is computed in the TICK, before the draw** |
| draw chain: `0x006330e1` | **`0x005fc5b0`** | **the viewport draw root** (below) -> at `0x5fc92b` `mov ecx,[ebx+0x1c]; mov edx,[ecx]; mov edx,[edx+8]; ... call edx` = the viewport CLIENT's Draw through `[viewport+0x1c]` -> vtable slot 2 (Infinite's exact shape) -> a script event on the viewport client -> natives -> the generic event helper at `0x567a5e` (the ONLY direct `E8` caller of `ProcessEvent` in the exe; every other dispatch is virtual) -> `ProcessEvent(PostRender)` on `DishonoredHUD` |

**3. The bytes at the call site** (`reentry probe 633090 128`), inside `UGameEngine::Tick`:

    63309a  mov edi,[esi+0x48c]        ; this->GameViewport (UGameViewportClient*)
    6330a0  test edi,edi / jz
    ...     (a virtual on the client with one argument, slot 0xec)
    6330cd  mov eax,[esi+0x48c]
    6330d3  mov ecx,[eax+0x40]         ; GameViewport->Viewport (FViewport*)
    6330d6  test ecx,ecx / jz 6330e1
    6330da  push 1                     ; bShouldPresent = TRUE
    6330dc  call 0x5fc5b0              ; FViewport::Draw(TRUE)   <- kViewportDrawCallSite
    6330e1  ...                        ; kViewportDrawGameplayRet

and the root: `0x005fc5b0` begins `55 8b ec 6a ff 68 a3 97 f2 00 64 a1 00 00 00 00` (push
ebp; mov ebp,esp; push -1; push 0xf297a3; mov eax,fs:[0] - an SEH prologue, a function
entry), its first `ret imm16` is `ret 4` at +0x1fc (ONE stack argument: the `push 1`), its
body builds a canvas (the `lea ecx,[ebp-0x10c]; call` pair around the client-Draw dispatch)
and tears it down after. `pe-xref`: 3 direct callers (`0x4dba68`, `0x6330dc`, `0x641d87`),
0 vtable references; only `0x6330dc` is the per-tick gameplay dispatcher - the other two
are not reached in gameplay (the deny gate's foreign-caller counter reads 0 across the
runs). The control for the static tool: `ProcessEvent 0x470640` must report exactly 1
direct caller (`0x567a5e`) and ~2087 vtable references, and it does.

**The values in patterns.h**: `kViewportDraw 0x005fc5b0`, `kViewportDrawPrologue[16]`,
`kViewportDrawRetImm 4`, `kViewportDrawCallSite 0x006330da` (7 bytes `6a 01 e8 cf 94 fc
ff`, the `push 1; call`), `kViewportDrawGameplayRet 0x006330e1`, `kGameEngineTick
0x00632860` and `kViewportClientOff 0x1c` (derivation only). Every hook byte-verifies the
prologue AND the site (and that the site's rel32 targets the root) before patching, and
refuses with the bytes it found.

**4. Made to MOVE.** `reentry pulse 3` doubled three gameplay draws: `second draw ok,
call2=414/229/218 us`, presents advanced by one per pulse (the root presents in its own
tail, unlike Infinite's client draw), and under the method the pair line reads **the +1
present's c5 sits (0.02 6.17 0.00) uu from the -1 present's (|d| 6.17; ipd*scale = 6.17
expected along right)** - every pair, to the hundredth: the two presents of a tick are
drawn from two cameras half an IPD apart along the camera's right row. The capture pair
(`D:\dvr-data\xrsim\eyecheck\20260903_032840_reentry`) shows the parallax on the near pipe.

**5. The method, measured (run 28, the sewers, simulator at 90 Hz):** `reentry: beat
draws/s=53 2nd/s=53 presents/s=106`, `stereo: beat method=reentry out/s=107 L/s=54 R/s=53
mono/s=0`, `call2` 218-467 us, skips 0 on every gate, no fault, `stereo.xrs`
`projectionViews eq 2` PASS with both eyes 71/68 % non-black, eye-check leg 0 PASS (38/37)
and leg 1 PASS (projection, 0.063 m), `stereo mono` restores the call site (`reentry: hook
removed`) and the mono beat returns. The tick rate halves under the doubling on this rig
(90 -> 53 draws/s at 1080p on the simulator: the second draw is a full scene draw for the
GPU, and the game thread waits for the render thread); presents = 2x ticks holds.

**What the eye-check bands say here.** Legs 2/4/5 were calibrated on BioShock 1's
fairground (interocular mean 40-70). On this scene the MONO projection (identical images
composited at the two eye poses) reads 13-22 mean and the true stereo pair reads 6-7: a
per-eye render agrees with the compositor's per-eye poses better than one image shown
twice, so the diff FALLS. The instruments that carry the verdict here are leg 0 (the
pairing) and the pair line's c5 travel; the interocular band needs its own Dishonored
calibration once a headset run has judged fusion (KNOWN_ISSUES).

**The first headset run (2026-09-03, Quest 3 via VDXR, the user; `42-run30-quest3-reentry.log`)
failed on two counts, both explained by the log, both invisible on the simulator:**

1. *Both frames in both eyes, alternating.* The doubling ran (`draws/s=54 2nd/s=54
   presents/s=108`, `pair pacing live`, the pair c5 line 6.08 uu) but the beat read
   `L/s=36 R/s=54 mono/s=18`: a third of the LEFT tags were dropped by the method's own
   pairing check, which compared the camera position the tick's last write produced with
   the present's c5 and required them equal within 2 uu. While the player WALKS the engine
   moves the camera by a tick of travel AFTER that write (measured: `c5 5692.0 6376.0` vs
   `written 5689.5 6375.8`, ~2.5 uu along the heading, the eye offset intact), so the -1
   present failed the check (the +1 present is written in the stub right before its draw
   and always matched). Every dropped left tag broke a pair and the runtime submitted its
   latest single image to both eyes for that frame. On the simulator every run stood
   still. The position check is telemetry now (a 40 uu line for teleports); the ring's
   push/pop ORDER pairs the eyes.
2. *Head motion reversed on lean, a second motion on pitch.* Under a projection layer the
   compositor moves the image for the head's REAL displacement (the located pose,
   including the neck's travel on a pitch and the roll). The positional path was built
   for the head-locked quad: a screen-space matrix shift with a deadzone and a room-scale
   bleed that re-centres the reference within a second (the heartbeat shows the lean
   decaying to 1-4 uu while the user leaned), and `[HeadTrack] Roll=0` never rolled the
   camera. So the game rendered from a camera that had not moved while the layer said it
   had: reversed parallax on a lean, a swim on a pitch, a counter-rolling horizon. Under a
   projection layer the game camera now follows the head's RAW displacement (no deadzone,
   no bleed, no clamp, no synthetic crouch drop) through the camera lane in the yaw-only
   frame (`[PosTrack] Lane=auto`), and the head roll is written. The quad screen keeps the
   tuned lean and no roll. Not yet re-judged in the headset.

**A loose end, recorded.** The ring between the game thread's tag push (per draw) and the
present thread's pop (per present) can hold two pairs legitimately (the game thread runs a
frame ahead); the first build cleared it at depth 3 and re-paired mid-pair every few
seconds (the c5 check caught every one: "tag -1 dropped ... c5 6383.1 is not the position
the draw wrote 6376.9" - the two eye positions, 6.2 uu apart). Fixed by allowing two pairs
and letting the c5 match skip stale tags (`tagResynced` in status.json).

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

## The capture cost, measured (2026-09-03, S1)

The mono screen's per-present capture (`core/gfx/capture`) was the known structural cost
and nothing had measured it. Runs 16-19 on the dev PC (simulator lane, 1920x1080 windowed,
the auto-continued save in gameplay, ~85 presents/s), with the `capture: cost/present` line
(one 3 s window each, microseconds per present):

| mode | rtd | lock | copy | upload | blit | total | what it says |
|---|---|---|---|---|---|---|---|
| sync (shipped) | 2 | 2400-3150 | 700 | 1500-1700 | 0 | 4700-5500 | `GetRenderTargetData` returns at once; **`LockRect` is the wait** (the readback is queued behind the frame in flight); the row copy of 8 MB is 0.7 ms (cached), `UpdateSubresource` 1.5 ms |
| deferred, first form (read back the previous copy AND lock it in the same present) | 2 | 2900-3100 | 700 | 1500 | 1 | 5100-5400 | **no gain**: the readback queued this present sits behind this present's rendering too, so the lock waits just the same (run 18) |
| deferred, pipelined (queue the readback this present, lock the PREVIOUS present's) | 2 | **0** | 730 | 1500-1650 | 1 | **2250-2400** | the wait is gone; the picture is one present late (head-locked screen: tolerable; a stereo method's tag travels with the slot) |
| shared (a D3D9 surface opened on D3D11) | - | - | - | - | - | - | **REFUSED** by the device: `QueryInterface(IDirect3DDevice9Ex)` fails (the game calls `Direct3DCreate9`), `CreateRenderTarget` with a shared handle returns `D3DERR_INVALIDCALL`. D3D9 shares only under 9Ex, and a 9Ex device refuses `D3DPOOL_MANAGED`, which UE3's D3D9 RHI depends on, so upgrading the device is not an option either |
| user-memory readback surface (`CreateOffscreenPlainSurface` with the buffer pointer in `pSharedHandle`, to lose the row copy) | - | - | - | - | - | - | **REFUSED**: `D3DERR_INVALIDCALL` (run 17); the runtime does not take a caller's buffer here. Not kept as a mode |

So the cheapest capture this game's device allows is the pipelined `deferred` mode: half
the cost of the shipped path, at the price of one present of latency. It also resolves a
multisampled backbuffer through its `StretchRect`, which is what the run 6
`GetRenderTargetData` failure under the game's AA setting needed. `[Capture] Mode=` ships
`sync` (every new lever default OFF); `capture mode deferred` is the live A/B, and the
headset run decides whether it becomes the default (ROADMAP S1). The remaining 2.2 ms is
the row copy plus the D3D11 upload of 8 MB; a staging-texture map would fold the two into
one and is the next step only if the headset number asks for it.

The instrument that settled it: the phase split. The first cost line lumped lock and copy
together as "copy = 3.5 ms" and read as a slow write-combined memcpy; splitting the lock
out (run 18) showed the memcpy at 0.7 ms and the wait inside `LockRect`, which is what made
the pipelined form the obvious move instead of the user-memory trick.

## The tick budget, measured (2026-09-03, session 8)

The headset ticked at 26-28/s at the Quest 3 size and the capture's cost line could not say who
owned the tick: the `LockRect` wait it counts as "capture" is also the GPU finishing the frame,
so the same numbers fit "the render thread is bound by the readback" and "the GPU is bound by
two 2496x2688 draws". `core/framework/perf` measures the split per present (eight QPC stamps in
hkPresent, the first BeginScene after Present as the frame-start marker, a D3D9 timestamp ring
with a bracket around the readback copy, read five presents back and never waited on) and prints
`perf: tick` and `perf: gpu` every 3 s. Simulator lane, RTX 4060, 2496x2688 VirtualMode, the
sewers save, `stereo reentry` (runs 44-01, 44-03, 44-04; logs in `D:\dvr-data\logs`):

| capture | tick ms | ticks/s | presents/s | CPU capture / present | of which lock | GPU 3D / present | GPU readback DMA / present |
|---|---|---|---|---|---|---|---|
| sync (the 41.0 path) | 46-48 | 21-22 | 43 | 17-21 ms | 9-13 ms | 4.8 ms | 15.5-16.8 ms |
| deferred | 36-37 | 27-28 | 54 | 9.6-10.5 ms (copy 2.8, upload 7) | 0 | 4.8 ms | 10.4 ms |
| off (the control) | pace-bound at the sim's 90 Hz | - | 93 | 0 | 0 | 2.8 ms | 0 |
| shared on a 9Ex device, SharedWait=1 | 13.3 | 75 | 151 | 3.6-3.8 ms (the fence wait) | 3.6 ms | 4.4 ms | 0.2 ms |
| shared, SharedWait=0 (ships) | 11.1, PACE-BOUND (the sim's 11.11 ms) | 90 | 180 | 0.5-0.8 ms | 0.5 ms | 5.0 ms | 0.2 ms |

What the table says: the readback owned the tick on BOTH sides. `GetRenderTargetData` into a
system-memory surface moves 25.6 MB at about 1.6 GB/s, 16 ms of GPU time per present that
serialises with the next frame's draws, plus 7.5 ms of CPU copy and upload; the actual 3D work is
5 ms per draw. `deferred` hides the CPU wait but not the DMA (the GPU still spends 21 ms per tick
copying), which is why it gains only 6 ticks/s. The shared surface (a VRAM-to-VRAM StretchRect
fenced by an event query) removes the DMA and the crossing: the tick drops from 46 ms to the
simulator's pacing limit, with the GPU at 5 ms per present. The headset at 72 Hz should be
pace-bound at the Quest 3 size with about 4 ms of GPU headroom per tick (the render thread's own
work is 3.3 ms per present; Virtual Desktop's encoder shares the GPU and is not in this table).

The confounds the instrument named: the 1080p simulator runs of session 6 (sync 83-90 vs deferred
90-92 presents/s) were PACE-BOUND by the simulator's 90 Hz gate (`wait` 3-6 ms per present), so
"deferred gained nothing" there said nothing about the capture; the menu and the loading screen
are game-thread-limited (`RENDER THREAD STARVED`, idle 18 ms of 18); and the simulator lane shows
a 130-140 ms lock stall every 2-3 s under `sync` at the Quest size (`perf: frame gap ... sat in:
the method (capture)`, lock 125 ms) that the headset run 13 never showed: a simulator-lane
artifact until measured otherwise (VERIFICATION, known simulator defects).

## The creation census: what UE3 asks of D3D9 (2026-09-03, session 8)

`core/gfx/device_census` patches the device's eight creation calls and each resource class's
Lock, and counts what the game asks (run 44-02, the sewers level fully loaded, `device census`):

- 8120 creations, 859 MB asked; **8060 MANAGED (398 MB)**: textures 5217 (DXT1 2564 of them,
  244 MB at the first GAMEPLAY; DXT3/5 463; 8bpp 521; 16bpp 15; 32bpp 12), cube textures 23,
  vertex buffers 1874 (12 MB), index buffers 962 (2.6 MB). DEFAULT: the render targets (21
  float, 18 32bpp, 253 + 101 MB), 5 depth-stencils, one dynamic write-only vertex buffer.
  SYSTEMMEM: the mod's own readback surface. No AUTOGENMIPMAP anywhere.
- **Locks on MANAGED textures: READONLY 10598, write (NOSYSLOCK) 55393, partial 56, level > 0
  45415**; cube textures 774 plain writes; every static vertex and index buffer locked once,
  plain, at creation. The READONLY locks are UE3's mip streaming copying from the old texture.
- The device: `CreateDevice adapter=0 type=HAL flags=HWVP|PURE|FPU_PRESERVE`, the present
  parameters A8R8G8B8, one backbuffer, DISCARD, LOCKABLE_BACKBUFFER, interval IMMEDIATE; caps
  DYNAMICTEXTURES and CANAUTOGENMIPMAP; 4070 MB available.

So a 9Ex device (which refuses D3DPOOL_MANAGED) needs a translation for 99 % of the game's
creations, and the DEFAULT + DYNAMIC stand-in is the wrong one: a READONLY lock of a DYNAMIC
DEFAULT texture reads VRAM through an uncached map and can return garbage after streaming. The
translation that holds is the shadow (below). The census stays on (creation calls are rare, a
lock is one hash lookup); `device census|status` on the seam, `census{}` in status.json.

## The D3D9Ex device and the managed-pool shadow (2026-09-03, session 8)

`[Device] Ex=1` (launch-time; `device ex on|off`, the F10 Display tickbox) makes
`Direct3DCreate9` return an `IDirect3D9Ex` as the game's `IDirect3D9` and `hkCreateDevice` call
`CreateDeviceEx` (a `D3DDISPLAYMODEEX` from the present parameters when fullscreen, NULL when
windowed), falling soft to the plain `CreateDevice` on the Ex object, then to the plain
`IDirect3D9`. Measured on the simulator lane (runs 44-03, 44-04): `CreateDeviceEx -> 0x0`, the
device answers `QueryInterface(IDirect3DDevice9Ex)`, `GetAdapterLUID` 0-d03b, the capture probe
reads `shared surface AVAILABLE` (a 2496x2688 A8R8G8B8 render target opened as D3D11 fmt 87).
No Reset happened on the level load under VirtualMode (the windowed device keeps its size), so
the 9Ex Reset semantics are still unmeasured; a fullscreen Reset that returns INVALIDCALL has
`ResetEx` as its contingency.

`[Device] Managed=shadow` (the default while Ex=1): every MANAGED creation is passed to the
device as DEFAULT (buffers too; they are lockable there), and every translated texture gets a
SYSTEMMEM twin with the same levels and format. The class-wide Lock hooks the census installs
redirect `LockRect`/`LockBox`/`AddDirtyRect` on a translated texture to its twin, `UnlockRect`
pushes the twin's dirty regions to the real texture with `UpdateTexture`, and the last `Release`
drops the twin: what MANAGED did inside the runtime, done in the proxy, so a READONLY lock reads
the twin (every write went through it) and the game keeps its pointer to the real texture for
everything else. Measured: 5240 twins, 65552 unlock pushes, 0 failures, the sewers rendered
intact after minutes of play (`dump capture`, run 44-04). `none` (the refusals are the
measurement), `default` (textures lose their locks) and `dynamic` are the A/B, all behind Ex.
The session's finding for the belief recorded above under "The capture cost, measured": the 9Ex
route IS possible on this game, at the price of the shadow.

## The shared hand-off needs a fence in BOTH directions (2026-09-03, session 8)

A D3D9Ex share carries no keyed mutex, so the proxy fences it by hand. The first build fenced one
direction: a D3D9 event query after the blit, waited on before D3D11 samples the slot. The
headset run 15 then reported the eyes disagreeing "90 % of the time" with every tag instrument
clean, and the other direction was the hole: D3D11's read of a slot is queued, not executed, when
the consumer returns, and the runtime's `xrEndFrame` is what flushes it; two presents later D3D9
blits the NEXT frame - the other eye's - into the same slot, and if the read has not executed
yet it samples that frame. The consumer now ends a D3D11 event query after its draw and flushes,
and the blit into a slot waits (bounded) for that query; the count of blits that found the read
still pending (`readWaits`) is the number of frames that could have carried the wrong eye: 14 in
one simulator run at 90 presents/s, so the race is real, not theoretical. Two slots suffice only
because of this fence; without it, more slots would only have made the swap rarer.

## The one-view state: what the headset logs say, the frame-identity trace, and the pairing that swapped (2026-09-04, session 9)

**The fault as measured (runs 16-17, the user, Quest 3 via VDXR, `Ex=1` + `Mode=shared`)**: after a
level load, from the first arming of the second draw, both eyes show ONE picture (the run-17 dump
pair `eye_3342/3343` differs by 2.5 grey levels with no horizontal shift improving it; the later
pairs from the same run carry 64-96 px of parallax), with every tag instrument clean.

**What the archived logs say, read again before any code was written**:

- The engine's per-present population does not change between the bad and the good state:
  `BeginScene 1.0/present`, SRT 64-75/present, `perf: gpu 3d 6.0-6.4 ms/present`, `draws/s ==
  2nd/s == 72`, the P1/P2 split the same. The only per-window number that moves is the shared
  capture's `blit fence waits`: 41-95 of 432 grabs in the bad state, 172-220 in the good state,
  in BOTH runs. Unexplained; the trace below carries it.
- The dump pair is genuinely one tick's pass 1 and pass 2: `FrameDumpTick` runs in `game_tick`
  before `end_frame`, so `eye_N` holds present N-1's output, and under `shared` (delivery = the
  previous present's slot) those are the backbuffers of presents N-2 and N-1 with their own tags.
  Both were blitted before the dump's stall. 2.5 grey levels is two renders, not one texture
  copied twice (that reads 0.0).
- The `reentry: pair` line is `DVR_LOG_FIRST_N(6)`: its six lines in run 17 are the first six
  pairs of the BAD state, and they read c5 one IPD apart. So the two draws uploaded different
  camera positions while the pictures were the same. The line samples "the last c5 upload before
  this Present" with no per-draw association; the trace ties c5 to the pixels per present now.
- No capture-mode switch in run 17 tripped a gate (no `gates ->` line at any of the eight
  switches). Every switch to `shared` was followed within 1.5-9 s by a pause menu whose resume
  re-armed the doubling; no switch to `deferred` was. The user's "shared fixes it" is confounded
  with a pause/resume; PR #6's "a mode switch trips the no-present gate" is not in the log.
- **`dump eyes` re-armed the second draw by itself**: the PNG encode on the present thread
  stalled 620-660 ms per file, the script camera writes read stale (`viewinject: ... the direct
  fallback is taking the camera back`), the state dropped to LOADING, the gates went SINGLE for
  74 ticks and DOUBLE again. Both "good" pairs of run 17 (4464, 8499) were dumped after such a
  re-arm. The encode is on a worker thread now (the present thread copies 27 MB and returns).
- `drawTid == presentTid` in run 17 was a latch artifact: `g_presentTid` was set once, at the
  first present, which at boot is the game thread's. It follows the presenting thread now and
  logs a change. Every simulator run since reads two threads.

**The frame-identity trace (`core/gfx/frame_id`, `[Perf] FrameId=1`, `frameid on|off|status`)**:
one 64x64 luma thumbnail per present at four stages, keyed by the capture serial of the pixels
(so one frame lines up across the stages whatever the delivery lag), read three presents later
and never waited on, with the c5 of the draw and the camera's right row on the same record:

| stage | where | how |
|---|---|---|
| `bb` | `capture::grab`, right after `GetBackBuffer(0)` | D3D9 `StretchRect(bb -> 64x64 A8R8G8B8 RT, LINEAR)`, `GetRenderTargetData` into a SYSTEMMEM surface, `LockRect(READONLY|DONOTWAIT)` three grabs later |
| `slot` | `reentry::end_frame`, inside the read fence | the delivered slot's SRV drawn into a 64x64 RGBA target by the blit quad, `CopyResource` to staging, `Map(DO_NOT_WAIT)` three presents later |
| `out` | the same, from an SRV of the method's output texture | the same |
| `sc` | the runtime's frame-texture seam, after `CopyResource` into the acquired swapchain image, before its release | `CopySubresourceRegion` of the centre 64x64 into a staging texture of the swapchain format |

Per left/right pair the `stereo: frameid` line (at most once a second) prints the checksums, the
mean absolute luma difference per stage, the same-eye floor (this left against the previous
left), the c5 step of the +1 present from the -1's projected on the right row (the side check),
the picture's own best horizontal shift (the right eye's content must sit LEFT of the left
eye's: negative = a true pair, convention-free), and the first stage that reads as one
picture. A 3 s summary carries the counts; a state change at stage `bb` (ten pairs in a row
below the floor, or above it again) prints once at Warn. Evidence only: nothing here re-arms,
switches or kicks.

Simulator numbers (RTX 4060, 2496x2688, the sewers, runs 45-01..05): a true pair reads
L-R 4.1 at `bb`/`slot`/`out` and 4.7-5.0 at `sc` (a centre patch, not a downscale), the same-eye
floor 1.4-1.5, c5 |d| 6.17 = ipd*scale, picture shift -1 px at 64 wide, busy reads 0 at every
stage, slot repeats 0. Stages `bb`, `slot` and `out` come out byte-identical (one 2x2 bilinear tap
at the same 64x64 centres on both APIs), so a difference between them would itself be a finding.

**The diagnostic words, one per half of the user's remedy**: `reentry rearm [n]` (n gameplay
ticks decided SINGLE at the gate, no tag, no pass 2, then the doubling resumes; the capture
untouched), `capture reinit` (the shared slots, fences and D3D11 views, or the deferred ring,
released and re-created at the next grab, the mode unchanged; one present delivers nothing),
and the existing `stereo projection off` then `auto` for the runtime's quad -> projection
transition the pause path also makes. `capture status` prints the delivered slot and the reinit
count; the beat line counts pass-2 eye writes the camera seam refused (`p2write refused=`, the
camera pointer beside it): a refused write leaves pass 2 drawing from pass 1's camera.

**What the trace found on the simulator (not the headset fault, a second fault)**: the eye tags
SWAPPED against the draws. The method's tag ring pairs by ORDER (one push per draw on the game
thread, one pop per present on the render thread), and the order claim breaks in three
measured ways: (a) across a single -> double transition when the game thread runs a frame ahead
(a `reentry rearm 2` flipped the -1 tag's c5 from the left eye's position to the right's), (b)
within a second of the first arming, and (c) spontaneously in plain gameplay - with the check
off (`reentry c5pair off`) the side flipped twice in 25 s with no pause, no re-arm, no log
event, while the perf window read `untagged 16-19` per 3 s and P2 exceeding P1 by the same
count: a present found the ring EMPTY and the next one popped its tag. In the menu state the
ring overflowed every 3 s (draws outnumber presents there: `draws/s=67 presents/s=57`). Each
such skew showed the eyes swapped until the next one. The picture agreed with the c5 side every
time (shift +1 px when the side read SWAPPED, -1 px when ok), which also settles the sign
convention by picture: the field holds the position, c5 negates it, a right eye's c5 sits at
-ipd*scale along the camera's right row.

**The fix (`[Stereo] C5Pair=1`, `reentry c5pair on|off`, the A/B)**: the within-tick invariant is
the pairing. Between pass 1 and pass 2 the world does not tick, so the ONLY thing that moves the
camera is the writer's eye: `c5(pass 2) - c5(pass 1) = -ipd*scale` along right and 0.00 along
anything else (measured: `-6.17 along right, 0.00 other` on every pair). A present whose c5 sits
there from the previous present's is a pass-2 present whatever the ring says; one at +ipd*scale
is a pass 1 after a still pass 2; anything else (a pass 1 after a moving pass 2, an extra
present) takes the ring's tag. The ring's claim is checked against the measurement on every
present it can name; three disagreements in a row drain the ring to the tag the next present
must pop (`reentry: the tag ring skewed against the draws ... realigned`, Info, 3 s), and armed
single gameplay draws push a 0 tag so their presents cannot eat the next tick's -1 (not in
menus, where the ring would only fill with junk). On the fixed build: `side ok` from the first
pair after the arming through rearms of 1, 2 and 3 ticks, a `capture reinit`, a `stereo
projection off`/`auto` and a `stereo mono`/`stereo reentry` switch; P1 == P2 per window,
`untagged 0-1`; `reentry.xrs` 11/11. Counters: `c5Agree`, `c5Disagree`, `c5Realigned`,
`c5Unknown` (presents the invariant could not name), `c5Untagged` (the ring was empty, the
measurement named the eye) in status.json `stereo`.

**The headset run on this build (run 07, 2026-09-04, the user)**: the eyes right from the load
and after every word on the F10 EYES block; `side ok` and `SWAPPED=0` on every pair, c5 |d| 6.11
(the user's IPD), the picture shift negative, L-R 3-14 at 64x64 (the one-picture dump pair of
run 17 reads 1.49 at 64x64; the run-17 true pairs 9-10) - and the ring skewed against the
draws 131 times in about four minutes: on the user's rig the order claim goes wrong every 2 s,
which before this session swapped the eyes each time. Two costs of the first build, both fixed:
the verdict compared L-R against the same-eye floor, which on a live head holds a tick of head
motion that a within-tick pair does not (false `ONE PICTURE` warns; an absolute 2.0 at 64x64
now), and stage bb's `GetRenderTargetData` read every present is a pipeline sync on that GPU:
`perf: gpu idle(d3d9)` 1.5 ms per present against 0.3 in run 17, the tick 13.9 -> 16.7 ms
(60/s under a 72 Hz headset, `wait 0.0`, the time inside the game's own Present). The trace
samples one pair every 8 ticks now (`[Perf] FrameIdEvery`), which is all the judgement needs.

**THE A/B, IN THE HEADSET (run 08, 2026-09-04, the user): the pairing IS the fault.** With `c5
pairing` unticked on the F10 EYES block the eyes stayed right until a pause/resume, and then
went wrong at once and stayed wrong: the 3 s windows read `swapped=24 of 25` then `12 of 12`,
with the picture's own shift agreeing (`shiftPos=20`, the right eye's content on the wrong
side of the left's). Ticking it back on: one transitional pair, then `swapped=0` for the
remaining eighty seconds and the shift negative throughout. The user reported the same thing
by eye, three times, without seeing the log. So the eye fault this project has chased since
run 15 - "the eyes disagree", "90 % of the time, more at the beginning", "never correct
after a load" - is the order-based pairing breaking wherever the game thread runs ahead of
the render thread, and the within-tick camera step is the fix. The run-17 dump pair whose two
eyes differed by only 1.49 at 64x64 remains the one artifact not separately explained; it has
not recurred on the fixed build, and a swapped pair of a near-symmetric corridor view is the
simplest account of it.

**What this does and does not say about the headset's one-picture state.** A swap is two
pictures, not one; the run-17 dumps were one picture, so the swap is not that fault. It is,
though, exactly what "the eyes disagree" looks like, and it happened on every state transition
the ring order got wrong, so some of the run-15/16 reports were swaps. The one-picture state
still needs the headset run with the trace: if the line reads L-R below the floor at `bb` the
engine handed one picture twice (then the engine or the camera seam owns it); above at `bb` and
below at a later stage names that stage; `reentry rearm 2`, `capture reinit` and `stereo
projection off`/`auto` each alone say which half of the user's remedy repairs it.

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

## The pause/resume desync: a one-sided tag stream (2026-09-03, session 7)

The run-40 report ("the judder stays in the LEFT eye and stops in the RIGHT") is the
signature of a held right swapchain image, and the cause is on the game side, not in a
ring. Pass 1 pushed its `-1` tag on five gates and pass 2 re-evaluated them AFTER the draw
plus four more (exiting, a test running, the c5 serial, a present since the previous draw),
so the resume window - the game thread's catch-up burst tripping the stall gate, the verdict
flapping through LOADING for a second - produced `-1` tags with no `+1`. The runtime then
counted `abortLeft`, captured the left again and submitted a stereo layer that names each
eye's swapchain: the LEFT rewritten every other present (the judder), the RIGHT showing its
pre-pause image with its old pose (reprojected smoothly, so it looked frozen). STATUS had
attributed it to `xr: sr tag ring skewed`; that ring is pushed and popped consecutively on the
present thread in this port and cannot skew - the line that does appear is `reentry: tag ring
skewed` (the method's ring, a different threshold). The quad transition was never the fault:
`reset_aer()` clears both eyes on every quad present.

Fixed at the source (813807e3): the gates are decided ONCE at depth 0 before pass 1's tag, and
pass 2 takes the decision. Instrumented first (a9b2ef12): each eye capture stamps the present
counter, every stereo submit computes the per-eye image age in PRESENTS (a healthy pair reads
L=1 R=0), the beat prints an `eyes` line, and the method prints `STALE R EYE` with the OWNER
named from the game side's skip deltas or the runtime's counters. The fail-soft (d0650a38,
`vrpace strict`, default off): a stereo submit with an eye older than one present shows the
fresh eye to both eyes for that frame. `reentry skip2 <n>` reproduces the one-sided stream on
demand: strict off, the simulator counted held-eye stereo submits and the mod printed the
line; strict on, no new stale submit and 37 fallbacks to mono. The hammer
(`tools\arming-hammer.ps1`, 15 pause/resume cycles) and `stale-eye.xrs` read 0 stale submits
on the fixed build.

## The pair phase (2026-09-03, session 7)

`xrEndFrame` time minus the frame's `predictedDisplayTime`, through
`XR_KHR_win32_convert_performance_counter_time` (enabled when the runtime lists it; the
simulator now offers it with the same split QPC conversion its clock uses), sampled at every
pair close: negative = the pair closed before its slot, positive = it missed the slot and
displays a slot late with a pose predicted for the earlier one. On the TRACE pairs line, a
5 s log line, `vrpace status`, `stereo status`, status.json `stereo.pair.phase*`. On the
simulator the number is meaningless (its predicted time is `now + period` re-anchored, and it
read +58 to +75 ms, 100 % missed, at 40-64 pairs/s on a 90 Hz schedule); the headset's is the
number the judder question is decided on. `vrpace ahead 0|1|2` (7f569463) locates the head
pose the game renders with, and the views the layer is tagged with, for `predictedDisplayTime
+ ahead x period`; `xrEndFrame`'s displayTime and the tag generation are untouched, so at 0 the
paths are byte-identical. `vrpace lag` exposes the attribution generation for the measurement.

## THE GHOSTING WAS THE CADENCE BEAT, and the verdict's threshold hid it (2026-09-04, session 15)

**SOLVED, on the headset, by a one-setting A/B.** Same build, same scene, same 2750x2850 render;
only the headset's refresh changed.

| | 120 Hz | 90 Hz |
|---|---|---|
| display period | 8.33 ms | 11.11 ms |
| `perf: tick` p50 (p90, max) | 9.1 ms (10.8, 12.9) | 11.3 ms (12.0, 15.1) |
| **display slots per frame** | **1.05 - 1.11** | **1.00 - 1.02** |
| EVEN / UNEVEN windows | 9 / 20 | **33 / 16** |
| MATCHED / UNDER-SUBMITTING | 11 / 26 | **38 / 22** |
| tester's verdict | "still bad" | **"super smooth, pretty much zero ghosting or jittery frames"** |

The mechanism is arithmetic, and the `stereo: rate` line had been printing it all along:
at `off` slots of drift per frame, **one frame in `1/off` is held for an extra display slot**,
and consecutive frames shown for different durations is exactly a doubled edge under rotation.
At 1.11 that is every 9th frame. At 1.01 it is every 100th. The fault was never the resolution
and never the pose attribution - **it was the tick not dividing into the display period.**

### Session 14's falsification was wrong, and this is why

Session 14 measured 1.03-1.05 slots per frame at 2064x2208/120 Hz, read `EVEN CADENCE`, and
concluded the cadence hypothesis was dead. **The verdict was lying.** Its threshold was
`|off| > 0.06`, so it called 1.05 - a beat every twenty frames, plainly visible on a head turn -
a clean bill of health. The hypothesis was right; the instrument's *threshold* was wrong, which
is a failure mode worth naming: an instrument can be correctly built, correctly read, and still
mislead because the line between pass and fail was picked before anything was measured.

The threshold is now **0.02**, drawn at the measured edge (1.02 does not ghost, 1.05 does), and
both branches print the beat as a number - one frame in N, and the beat in Hz - so a future
"even" verdict shows the residual it is forgiving instead of hiding it.

### Why the frame rate drops FURTHER at 90 Hz than at 120 Hz

The tester's own observation, and it is not a contradiction:

- At **120 Hz** the tick (9.1 ms) never fit the 8.33 ms slot. The app was never trying to hit a
  slot - it free-ran and the compositor smeared over the mismatch continuously. There is no
  cliff to fall off when you are already permanently past the edge, so the rate reads a smooth
  100-120 and the ghosting is constant. **Smooth, and always wrong.**
- At **90 Hz** the tick (11.3 ms p50) sits *right at* the 11.11 ms period. Most frames make
  their slot, which is what removed the ghosting - but a frame that misses waits a whole period,
  so a single 11.3 ms overrun displays for 22.2 ms (45 fps instantaneous) and a run of them
  averages toward 60. **Correct, with a cliff directly underneath.**

**And the hitch RATE did not actually change.** Normalised by run length (29 vs 50 three-second
windows): 27.6 gaps/min at 120 Hz, 28.4 gaps/min at 90 Hz. Identical. They are simply visible
now, because they stand out against a locked cadence instead of disappearing into a permanently
smeared one. **54 of the 71 gaps sat in `present-tail (xrEndFrame)`, up to 101 ms** - on a Wi-Fi
streaming runtime a 101 ms block inside the submit call is the encoder or the link, not the
frame path. That is the next thing to attack, and it is not ours.

### The cost model, refit with the new point

`perf: tick` against per-eye megapixels, four sizes: **~0.63 ms/MP on a ~5.8 ms fixed floor**
(2750x2850 = 7.84 MP measured 11.3 ms against 10.8 predicted, so the floor is slightly higher
than the three-point fit said). Note the 90 Hz tick is PACE-BOUND (8 windows say so), so 11.3 ms
is partly the slot rather than the work - the render cost alone is lower and the headroom is
real but unquantified.

**The rule this leaves:** pick the refresh whose period the tick divides into, not the biggest
resolution. Read `stereo: rate` for `display slots per frame` and drive it to 1.00.

## The startup eye-starvation flicker, measured at last (2026-09-04, session 15c)

**Not new.** The tester reports it normally lasts a few seconds at the start of a session; on
this run it lasted much longer, which is what finally made it measurable. Percept: "flickering
a ton for like 30 seconds ... it looked like what AER looks like on a monitor but I could see
it in the headset", and "it took a long time at the start for the weapon to sync". Then it
stopped and stayed stopped: "once the weapon aligned properly it stayed aligned perfectly and
it was smooth like butter".

**What the log shows, `stereo: beat` L/s and R/s across one run** (2750x2850, 90 Hz, Quest 3
over VDXR, `alpha-272-g65ac9bd2`):

| t (s from proxy load) | out/s | L/s | R/s | none/s | draws/s | state |
|---|---|---|---|---|---|---|
| 8.5 - 17.5 | 21 - 85 | 0 | 0 | 0 | - | menu/loading, mono by design |
| **20.5** | 89 | **12** | **52** | 10 | 66 | GAMEPLAY starts; starved |
| **23.5 - 38.5** | 87 - 91 | **16 - 19** | **71 - 73** | 16 - 17 | 51 - 72 | starved |
| **44.5 onward** | 180 | **90** | **90** | **0** | **90** | **locked, and stays locked** |
| 77.5 - 83.5 | 155 - 234 | 0 | 0 | 0 | - | pause screen, mono by design |

**The cause is the tick, and the numbers say so directly.** During the starved window
`perf: tick` reads **17.5 ms against the 11.11 ms budget** and its per-class split is
`P1[-1] n=36` against `P2[+1] n=156` with `untagged 107` - the LEFT-tagged presents are a
quarter of the RIGHT ones. `reentry: beat` confirms pass 2 is running the whole time
(`2nd/s == draws/s`, skips all zero), so the second draw is NOT missing; the game is simply
producing 51-72 ticks/s against 90 display slots/s. With the tick below the display rate the
pair schedule cannot land one pair per slot, the tag stream goes lopsided, and 1016 same-eye
pushes accumulate (`reentry: pushed eye +1 TWICE in a row`, always +1, LEFT starving).

**One eye taking fresh frames at ~18 Hz while the other runs at ~73 Hz is not subtle - it is a
hard flicker, and it looks like alternate-eye rendering seen flat because that is structurally
what it has become.** It self-heals the instant `draws/s` reaches 90: L/s = R/s = 90, zero
untagged, zero stale, for the rest of the run.

The usual reason the tick is slow for the first seconds of gameplay is UE3 level streaming -
`call2` max spikes to 1836-2029 us in that window against 614-777 us once locked, and the
device census logs 14058 creations at first GAMEPLAY.

**Same root as the ghosting, at a different ratio.** When the tick is slightly longer than the
display period you get the beat (doubled edges); when it is far longer you get eye starvation
(flicker). Both are the tick not fitting the slot.

### Fix theory - NOT implemented, and the cheap test comes first

1. **`vrpace strict on` is the existing lever and has never been judged.** It already does the
   right thing in principle: a stereo submit with an eye older than one present shows the fresh
   eye to BOTH eyes instead. That converts the starved window from alternating eyes into a
   briefly flat picture, which is a far milder artifact, and it costs nothing to try - it ships
   off and toggles live. **Do this before writing any code.** The risk is that it also fires on
   the rare mid-gameplay stale eye and drops depth for a frame there, so it wants an A/B, not a
   blind default flip.
2. **If strict is not enough, the shape of a real fix** is to refuse to submit a pair at all
   while the tick cannot fill the slots, rather than submitting a lopsided one - i.e. extend
   the `HoldUntagged` idea from untagged presents to unbalanced pairs, holding the previous
   good pair until `draws/s` recovers. Bounded, because a permanent hold is a frozen image.
3. **The cheapest mitigation is not ours at all**: the window ends when streaming does, so it
   scales with load time. An SSD, and not turning the head for the first few seconds after a
   load, both shorten what the player sees.

Unresolved and worth measuring first: **why LEFT specifically.** The pushes are always `+1`
(RIGHT) doubled. A plausible mechanism is the shared-capture deferred delivery
(`SharedWait=0` delivers the PREVIOUS slot) repeating a tag when presents arrive irregularly,
but that is a hypothesis, not a measurement, and `capture sharedwait on` is the A/B that would
test it.

## The content-bbox readback prediction was FALSIFIED (2026-09-04, session 15)

Session 15 gated the 3-second full-frame CPU readback and predicted that if it were behind the
hitches, the `perf: frame gap` count would fall by roughly the number of 3-second windows in a
run. **It did not.** Samples fell from one per 3 s to 2-3 per run, and the gap rate was
unchanged (27.6 and 28.4 per minute across the two runs, against 62-82 per run before). The
counter-evidence recorded alongside the prediction - that the gaps mostly sat in
`present-tail (xrEndFrame)`, not the capture phase - was the correct read.

**The gate stays**: it removed a real, unlevered ~30 MB present-thread stall and cost nothing.
It just was not the hitch cause, and saying so is the point of having written the prediction
down.

## The two pose lanes, and why the tag can be a generation wrong (2026-09-04)

The mod samples the head TWICE per frame, on two different lanes, and the compositor only
ever sees one of them:

- **SCRIPT lane.** `on_present_begin` locates the head (`xrLocateSpace`, `openxr_runtime.cpp`),
  `DvrConsumePoses` -> `TrackHead` turns it into `g_hmdYaw` on the present thread, and the
  GAME thread's world tick reads that in `ApplyHeadToViewRotation` and writes the engine
  camera. `head_track.cpp` publishes the matched pair at that instant: `g_viewYawRad` (what
  was written) beside `g_injHmdYawSnap` (the HMD yaw it was computed from). **This is the pose
  the pixels are drawn with.**
- **PRESENT lane.** The same `on_present_begin` calls `xrLocateViews` for the same
  `locateTime`, and the projection layer's `XrCompositionLayerProjectionView.pose` is filled
  from one of three kept generations - `g_views` (N), `g_viewsContent` (N-1), `g_viewsPrev2`
  (N-2), selected by `g_poseLag`, shipping at 1. **This is the pose the compositor reprojects
  FROM.**

If those two are not the same sample, the reprojection is wrong by the difference on every
frame, the error tracks head speed, and it grows when a frame is slow. That is a doubled-edge
percept under rotation - and until 2026-09-04 nothing measured it.

**The tag is predicted to be one generation too fresh, and the game's own config says so.**
The attribution comment assumes "locate N feeds the tick that presents at N+1" - one
generation, hence `lag=1`. But `DishonoredEngine.ini [SystemSettings]` carries
**`OneFrameThreadLag=True`**: UE3's render thread runs a frame behind the game thread, so the
pixels in present N were drawn by a tick that read the head at locate **N-2**. The prediction
is therefore that the instrument reads a one-generation gap at `lag 1` and that
`vrpace lag 2` nulls it. `OneFrameThreadLag=False` is the independent second test - it removes
the skew at the source instead of compensating for it, at a throughput cost. Neither has been
run yet.

**Both eyes of a pair share ONE locate - do not go hunting a per-eye asymmetry.** Under
`reentry` the LEFT present holds the XR frame open (`pairHold`) and the RIGHT completes it;
`on_present_begin` returns at the top while a pair is open, so there is no second
`xrWaitFrame` and no re-locate between them. `g_viewsContent` is identical for both eyes. The
instrument prints per eye anyway, cheaply, so the invariant is checked rather than assumed.

**THE SIGN TRAP.** The two lanes read yaw out of the SAME rotation matrix with opposite
conventions:

| | reduces to |
|---|---|
| `xr_quat_yaw_deg` (`openxr_runtime.cpp`) | `atan2( m02, m22)` |
| `TrackHead` (`head_track.cpp`) | `atan2(-m02, m22)` |

so `g_hmdYaw == -xr_quat_yaw_deg / 57.29578` for any pose, and a naive subtraction reads about
TWICE the yaw. That would look like a catastrophic disagreement that is purely convention -
the most convincing possible way for this instrument to lie. `publish_script_head` negates
once, on the way in, and then PROVES it against live data: at the first publish it reads the
same `g_headPose` back through this file's own converter and logs
`xr: poseaudit SEAM CHECK ok|FAILED`. Do not read a delta until that line says ok.

Note also that `g_viewYawRad` is the absolute UE **rotator** yaw and composes stick turn with
head delta (`ue_math.cpp` differences the two deliberately). It is never the right thing to
compare against the tag; `g_injHmdYawSnap` is.

## The content-bbox readback: a full CPU round trip in the VRAM path (2026-09-04)

`capture.cpp`'s bbox instrument - the `100% x 100% (FULL)` / `(CROPPED)` line - needs CPU
pixels. In `shared` mode, whose entire purpose is that nothing goes to the CPU (the frame is a
VRAM-to-VRAM `StretchRect`), sampling it costs a full `GetRenderTargetData` + `LockRect` +
per-row `memcpy` of the whole frame, **on the present thread**. That is the same round trip
measured at 17-21 ms/present in `sync` mode at 2496x2688 ("The capture cost, measured"), and
it ran unconditionally every 3 seconds with no lever - about 31 MB per sample at 2750x2850.

`[Capture] BboxMs` (default 30000, `capture bbox off|<ms>` live) is the gate. A size change
still resamples immediately and unconditionally, because that is the sample that decides
CROPPED vs FULL and it must not wait for an interval.

**Falsifiable prediction, recorded before the run:** if this is behind the hitches, the
`perf: frame gap` count should fall by roughly the number of 3-second windows in a run (62-82
gaps over the last two runs is close to one per window). **The counter-evidence is already on
record**: those gaps mostly reported `sat in: present-tail (xrEndFrame)`, not the capture
phase. If the count does not move, this removed a real cost and was not the hitch cause.

## The pitch pivot: the engine's neck, measured (2026-09-03, session 7)

`camera pitchtest 30` (e374a6a2) takes three buckets of c5 (LEVEL, looking UP, looking DOWN,
60 presents each) and projects the travel from LEVEL on world up and the pitch-0 heading, so
the per-eye offset cancels and it runs under `stereo reentry`. Simulator, the sewers, 98 uu/m:

| run | head | c5 travel UP (U, F uu) | DOWN (U, F) | seam asked UP / DOWN | fit |
|---|---|---|---|---|---|
| 1 | `head rot 0 +/-30 0` at a FIXED position | -1.04, -16.58 | -7.07, +14.90 | 0 / 0 | below 0.321 m, behind 0.062 m, consistency 0.33 / -0.07 uu |
| 2 | `head pose` on an 11/9 cm arc | +1.91, -23.14 | -12.86, +19.13 | U+2.97 F-6.58 / U-5.85 F+4.20 | the same fit; the extra travel equals the ask |
| 3 | fixed position, `neck cancel 0.321 0.062` | -0.48, +0.11 | +0.14, +0.15 | the cancelling term | the render camera holds still |

So the ENGINE pitches its camera about a pivot 32 cm below and 6 cm behind the eyes: 17 cm of
backward travel at +30 deg, which the compositor (expecting only the tracked head translation)
cannot reproject - "looking up with the whole body". The tracked arc arrives on top of it
(run 2). `neck cancel` with the engine's own numbers cancels it (run 3), and the +30 frames
with and without the cancel differ by the predicted 17 cm (the lamp at the right edge cut
off, the pipe junction lower). The `[Neck]` lever (0db35c10) ships off with the measured pivot
as its defaults; the headset judges `cancel` against `off` from F10 Comfort. The 38.24 ceiling
now counts the presents it clips (0 in all runs).

## The console seam was dead since 41.0, and setres is inert (2026-09-03, session 7)

`RunConsole` returned -1 unless `g_fnConsoleCmd` was set, and the only latch lived inside the
resolution script 41.0 removed: every `console` word and IntroSkip returned -1 since 41.0
without reaching the engine, so "setres is a dead end" (session 2) was never re-measured on
this line. Latched again (c0bd3831); the first word that reached the engine overflowed the
game thread's stack, because `RunConsole` calls ProcessEvent, which is the mod's own hook,
which ran the still-pending request again - the request now leaves the seam before the engine
runs it and the nested call returns on the re-entry flag. Measured then: `setres 2560x1440f`
(a real mode) and `setres 1600x900w` both dispatch (`ConsoleCommand` on
`DishonoredPlayerController`, the console's `OutputText` fires), return an empty reply, and
change nothing - no Reset, no size change. `setres` is inert on this build.

## The render size: the ini route is inert, the command line is honoured (2026-09-03, session 7)

With 2560x1440 fullscreen in every place of the game's own ini (both files, all four AppCompat
buckets, the compat file rewritten by the game itself at launch) the game created a 1920x1080
WINDOWED device on every run, including the headset run, and never Reset out of it (runs
07-08); the install-side `DefaultEngine.ini` carries exactly 1920x1080. The command line is the
route: the proxy is loaded before the engine's entry point, so `ResRequest` writes the ask to
`dishonored_vr_launch.txt` and DllMain points the exe's and the CRT's `GetCommandLineA/W`
import slots (3 patched) at the line with `-ResX= -ResY= -FullScreen` appended (ce10a3f7).
Run 09: `CreateDevice -> (2560x1440 windowed=0)`, the capture and the eye swapchains followed.

A size the display does not list falls back to a real mode (2496x2688 -> 2560x1440, run 10).
`[Screen] VirtualMode=1` (3935f9f7): the proxy, which IS the game's IDirect3D9, advertises the
asked size in the mode list the game validates against (`GetAdapterModeCount` /
`EnumAdapterModes`, asked from 0x9b79b7 at startup and 0x9b9a20 in the frame loop, our mode
handed at slot 123) and, when the game asks for a FULLSCREEN device at that size, creates it
WINDOWED with the backbuffer kept. Run 12: `CreateDevice -> (2496x2688 windowed=1)`,
`capture: 2496x2688`, `res: HONOURED`, `xr: swapchain pair 2496x2688`, the runtime's
circumscribed hfov 108.0 deg at aspect 0.929 (137 at 16:9), the sim's claim ratio 1.18 (2.17
at 16:9), both eyes 77 % non-black in the sewers, the frame complete floor to ceiling by
picture. Every 41.0-era dead end ran the game WINDOWED, where the desktop clamp lives; this is
the fullscreen path. The cost: the sync readback reads 18-20 ms per present at that size
(25.6 MB each way; `[Capture] Mode=deferred` is the answer, ROADMAP S1).

## Dead ends (do not re-hunt)

- The camera-object matrix at `kCamHookAt` is not what the renderer draws with.
- Mouse-count head injection: swims, lags, no roll.
- "Any constant upload of 9+ registers is a bone palette" is false; writing into them corrupts
  world geometry.
- `HideBoneByName` on the arms does nothing (no visuals, no allocation); hiding is by
  render-size masks (`WeaponHideBones`, `ArmsHideTick`).
- Window-route resolution changes (work area, max tracking size, self-resize, fullscreen
  escape, client-rect spoof, mode list): six builds, the game still chose its own size; the
  engine's own setres is INERT on this build (measured 2026-09-03: it dispatches and does nothing); the command line is the way, and the fullscreen path takes a proxy-advertised mode ("The render size", session 7).
- The overlay-scene XR architecture (compositor overlay, reprojection-exempt): rejected
  at 38.0 (cannot be motion-smoothed).

## Build history (the original author's numbering)

30.x VR 2.0 (DXVK chain, hands, overlay, SkelControl, blink), 32.x resolution wars and
Blink detours, 33-36 graft and calibration, 37.x OpenXR bring-up (XR-1 bench, XR-2 sync,
XR-3 pace thread), 38.x the Quest convergence attempts, 38.92 shipped. The fork: M2 frame
map, M3 splice, M4 twins, M5 sequential + shafts, M6 wrist HUD + shadows, M7 pixel-shader
shear, M8 quarter-res light passes (M8.2 shipped).
