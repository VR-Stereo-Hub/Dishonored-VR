# The hand swap: the sword in the left hand (`[Hands] SwapHands`)

Written 2026-10-20. Left-handed play: the sword is held in the player's LEFT hand and the pistol,
crossbow, gadgets and powers in the RIGHT, and the hands, the arms and what they hold are drawn
mirrored so each looks like the hand holding it.

**Status: built and SIMULATOR-CONFIRMED in the game (2026-10-20), not yet run in a headset**
(section 6). Default OFF (a render lever), live toggle.

Read VR-33-HANDS-AND-WEAPONS.md first: this rides on its one idea, a single transform composed
onto every bone of a draw.

---

## 1. What the player gets

| | Off (the game's hands) | On |
|---|---|---|
| Sword | right controller | left controller, drawn as a left hand |
| Pistol, crossbow, grenades, Heart, powers | left controller | right controller, drawn as a right hand |
| Full-arm IK arms | as before | mirrored: the sword arm hangs from the left shoulder |
| Attack (RT) / Power (LT) | right / left trigger | left / right trigger |
| Choke (RB) / Power wheel (LB) | right / left grip | left / right grip |
| Motion sword, physical pickup, grab animation, haptics | per game hand | follow the game hand to its new controller |
| Aim ray, Blink, throws | from the left controller | from the right controller, on the mirrored barrel |
| Zoom gesture | right controller at the right eye | left controller at the left eye |
| Sticks, face buttons, stick clicks, thumbrests, both-sticks chord | | unchanged |
| HUD hand anchors, F10 pointer | | unchanged: they name a controller, not a game hand |

`[Hands] SwapHands` (default 0), F10 Hands > "Left-handed: sword in the left hand", seam
`swaphands on|off` (saved) and `swaphands` (status).

The gesture (2026-10-21): a double click of the physical right stick (two presses within 350 ms)
toggles the swap live and writes `SwapHands` back to the ini. `[Controllers] SwapHandsGesture`
(`right`, the default; `left`; `off`), F10 Controls > "Swap hands shortcut", seam
`swaphands gesture right|left|off`. On the right stick the single tap is still the zoom: the zoom
pulse waits the 350 ms out and is dropped when the second press arrives, so a double click never
zooms. The stick click is physical, so the gesture does not move with the swap itself.

## 2. Two layers, one switch

### 2.1 The action layer: the game's hands, not the controllers (`core/vr/openxr_input.cpp`)

The switch lives in the action layer (`input_set_hand_swap`), and the render asks it
(`HsOn()`), so the poses, the buttons and the drawing cannot disagree. Every per-hand read
there now takes the GAME's hand (0 = gadget/pistol/powers, 1 = sword) and maps it to a controller:

- `input_get_hand_pose` (grip and aim), `input_locate_grip` (the spacewarp's synthesized slots),
  `input_hand_aim_sample`, `input_haptic`;
- the published snapshot's `trigL/trigR` and `gripL/gripR` are swapped at publish.

So `g_devPose[3]` is the game's left hand wherever it is held, and the whole mod - motion aim,
Blink, the motion sword, physical pickup, the grab, the pad bridge, the binds layer - follows
without knowing. `input_get_physical_hand_pose` / `input_physical_hand` exist for what names a
controller: the HUD's hand anchors (`openxr_runtime.cpp`, `hud_layout.cpp`) and the F10 pointer
(`overlay.cpp`, `pad_bridge.cpp`). The swap is its own inverse, so `input_physical_hand` also
turns a controller into the game hand it holds.

### 2.2 The render: the mirrored world (`hands/hand_frame.h`, "the hand swap")

Drawn on the left controller as it is, the game's right hand would be a right hand on a left
arm. Each hand is therefore placed in a MIRRORED WORLD and the finished palette reflected back:

```
d' = S d,  O' = S_r * O_C * X        the controller target, reflected (X = diag(-1,1,1))
D' = the usual placement on (d', O')   calibration, trims, IK, all unchanged
rows' = S * (D' * rows)               the palette, reflected back; cull flipped
```

- **X** is the controller's own x axis. OpenXR's grip +X points the same way in the world on
  both hands (out of the left palm, into the right), so the mirror image of a left controller is
  a right controller only after X undoes the one axis the plane reverses. `aim_ray.cpp`'s carry
  mirror flips the same local x for the same reason. Without X the result is not mirrored at all
  (`swap_mirror_needs_x`).
- **S S = I**, so the palm lands exactly on the controller; only the SHAPE is mirrored. The drawn
  palm frame works out to `O_C * X * G * Trim` at the controller's own position, and for a hand
  alone the plane cancels (`swap_mirror_plane_cancels`).
- **The plane.** The per-class path uses the view's own vertical plane through the head's centre
  (`ctx.r`; the eye offset undone so both eyes use one physical plane). Full-arm IK uses the BODY's
  plane (`body.right`) through the shoulder pair's centre (the head moved by `ArmShoulderRightCm`),
  because the shoulders are placed symmetrically about exactly that plane: the mirror image of the
  right shoulder is then the left one (`ik_mirror_maps_shoulder_to_shoulder`; a plane through the
  head alone misses by 2 x `ArmShoulderRightCm`).
- **Order.** The reflection goes on AFTER the finger work (the open right hand, the ready hand,
  the grab), which composes relative to the wrist and must see the unreflected rows. det(S) = -1
  reverses every triangle, so the draw flips D3DRS_CULLMODE (CW <-> CCW; NONE untouched) and puts
  it back (`HsCull`).
- **Calibration and trims transfer by symmetry.** The game's right hand keeps its own grip G and
  trims and is placed on the MIRROR IMAGE of the left controller, which is where a right
  controller in a right hand would be. A SHIFT+F7 taken while swapped is solved against the same
  mirrored frame it is used in, and det(O') = det(O_C), so the parity bookkeeping is untouched.

`MpWorldTarget(..., mirrorN, mirrorOut, mirrorShift)` does step 1 and hands back the plane;
`HsReflectPalette` does step 3 in both draw paths (`MsDraw` per class, `IkTryDraw`).

### 2.3 What rides the hand's correction

- **The weapons** (`weapon_attach.cpp`). `WaCommon` carries the mirrored-world `D` and the
  reflection `W`, both camera-relative. Each candidate takes `W` through the same conjugation as
  its correction (the native bridge too); the contract keeps `dmW` (the reflection in its own local
  space) and `dmMirror` for its sibling passes. A pass is drawn with `dmW * dm`, culling flipped.
  `BrMeasure` gets `dm` alone: the bolt axis is measured against `palm`, which lives in the same
  mirrored world, so the palm-frame axis comes out exactly as unswapped and the shipped
  `ModelAxisL*` record stays valid. The VR-138 back-fill copy is reflected twice and flips the
  cull back on its own; the Heart's back draws culling off.
- **The hand effects** (`fx_follow.cpp`): the reflection is applied after the correction in the
  native world. The position is exact; a rotator holds only a proper frame, so Y (UE3's right) is
  turned back, the emitter's mirror image as near as a rotation gets.
- **The aim ray** (`trim_snapshot`) and **the view-aligned trim steps** (`MpTrimViewStep`) build
  the palm from the controller on the present lane. They need `R_C * X` and no plane
  (`aim_ray_on_the_drawn_barrel`, `trim_step_moves_the_drawn_palm`).

## 3. Deliberately not here

- **The game's own arm draws are not mirrored.** Where the mod hands the arms back entirely
  (`native_full_arms`, cutscene arms, `dvr::anim::native_draw`), the game draws its own arms with
  the sword in the right hand. Game animations that stay on the mod's draw (takedowns and chokes
  under `ArmIKGameArmInAnim`, the hand-back blend) ARE mirrored with the hand, so the sword stays
  left; a stab from the left reads as a stab from the left.
- **A refused placement draws the engine's own, unmirrored hand** (the existing fail-soft), which
  for that frame puts the sword back on the right.
- **Labels stay the game's.** F10's Left/Right hand adjust, the `GripL/TrimL...` keys, the numpad
  modes, `[ControllerBinds]` sources (LeftTrigger = the trigger of the controller holding the
  game's left hand) and the log's LEFT/RIGHT name the game's hands. The F10 tooltip says so.
- **HUD hand panels are not mirrored** onto the other wrist: they are placed by controller and stay
  where they were configured. A note opened by a grip opens on the controller that took it.
- **No installer key.** The installer writes five keys and this is an in-game choice.
- **The other hand lanes read the same game-hand poses and are not mirrored.** The SkelControl
  drive (`[Hands] Enabled`) moves the engine's own bones toward the game hand's controller; the
  palette correction measures the animated palm every draw and re-places it, so that move never
  reaches the picture. The old `[VRHands]` `.obj` models (default off) would draw the game's left
  model on the right controller unmirrored.

## 4. Known costs

- **Normal-mapped lighting on the mirrored models.** The shaders rebuild the bitangent from a
  cross product and a stored handedness, and a reflection flips the cross product's sign, so the
  normal map's detail is lit from the mirrored side along one axis - the same cosmetic cost the
  VR-138 copy already carries and accepted.
- **Effect orientation is approximate** (above). An effect whose correction exceeds
  `kFxMaxMoveUU` (120 uu) is refused as before; with the hands swapped the game's own hand and the
  controller can be on opposite sides of the body, so that guard is nearer.

## 5. Reading the log

- `input: hand swap ON - the game's sword hand now reads the LEFT controller ...` (action layer,
  on change).
- `hands/swap: ON (ini [Hands] SwapHands) - ...` at load and on every toggle, naming who.
- `hands/swap: ON | mirrored so far: hand draws N, full-arm IK draws N, weapon passes N, effect
  writes N, cull flips N` every 5 s while on. All read 0 while off, by design. With it ON and the
  hands drawn, the hand count (per-class) or the IK count must climb or the mirror is not running.
  `swaphands` with no word prints the same line on demand.
- `pad/overlay: ... the right controller's trigger ... it drives the game's left hand` while F10
  is open and swapped.

## 6. Verification

Run on the writing machine (VS 2022 Build Tools; the imgui v1.92.8-docking and OpenXR-SDK
release-1.1.63 sources unpacked into `third_party`, the checkout being a zip without submodules):

| What | Result |
|---|---|
| `tools\build.ps1 -Release` | d3d9.dll, dvr_steamvr32.dll, dvr_xrsim32.dll, frame_test, xr_hello32 and the installer all built; the six compiler warnings are pre-existing (`WIN32_LEAN_AND_MEAN` and `DVR_CAT` redefinitions), none in this change; `tools\exports-check.ps1` OK (11 undecorated names); the CMake-built frame_test passes the `swap_mirror_*` cases |
| `tools\hand-swap-host.ps1` -> frame_test (the suite the proxy runs at every start) | all cases pass, including `swap_mirror_lands_on_the_controller`, `swap_mirror_plane_cancels`, `swap_mirror_needs_x`, `swap_mirror_palette` |
| `tools\hand-swap-host.ps1` -> hand-swap-tests | 9 / 9: aim ray on the drawn barrel (and its control), trim step, weapon rigid on the hand by both routes (and its control), IK shoulder to shoulder (and its control), effect frame |
| `tools\fx-follow-host.ps1` (compiles the real fx_follow.cpp) | 739 checks, 0 failed, with a new mirrored-hand case: the effect lands 0.00006 uu from the reflection's target |
| every `tools\*-host.ps1` (2026-10-20 sweep, 90 suites; the two DLSS ones skipped) | 75 pass, among them every one that builds the changed `hand_frame.h` (arm-ik, bolt-axis, follow-trim, grab-pose, palette-eye, pose-view, frame_test) and controller-emulation, which compiles `pad_bridge.cpp`'s face-button block. 15 could not run on that machine and include no changed file: no OpenXR/imgui submodules (aim-ray, installer-ui), a hard-coded full-Visual-Studio path (5), no Python (head-movement, yawtest), no ReShade, local compile/GPU failures (crash-capture, launcher-update, heart-material, desktop-eye), and default-profile, which fails on the release ini's pre-existing missing `KeyholeForwardCm` block (the `SwapHands` lines match the writer in all three copies) |

**In the game, on the simulator (2026-10-20).** GOG build (exe bytes match `patterns.h` at four
hook sites), the player's save in the Flooded District, full-arm IK on, the Heart in the left
hand and the sword drawn, `xrsim-shot` per eye:

| Step | Seen |
|---|---|
| start | the live proxy's own self-test passes the four `swap_mirror_*` cases; `hands/swap: off (ini [Hands] SwapHands)` |
| swap off (baseline) | sword in the right hand on the right controller, Heart in the left |
| `swaphands on` | both eyes: the sword in a LEFT hand on the LEFT controller (thumb on the inside, left sleeve), the Heart in a RIGHT hand on the right; no hollow or inside-out surface on either hand, arm or weapon |
| left controller raised 15 cm, moved right, rolled 35 deg | the left IK arm reaches up from the LEFT shoulder with the sword in its hand; the right hand does not move |
| counters | `hands/swap: ON \| ... full-arm IK draws 20336, weapon passes 50868, effect writes 2336, cull flips 71204` (hand draws 0: IK draws the hands) |
| simulated LEFT trigger pull | `pad/triggers: LT up RT DOWN`, then `anim/melee: attack source=TRIGGER` and `seq=Gadgets_SwordSlash_Right`: the left trigger attacks with the sword, the slash plays mirrored on the tracked left hand |
| `swaphands off`, live | the frame returns to the baseline |

**Not established:**

1. Headset: the hands look like your hands (thumbs inward), the sword follows the left controller
   and swings with it, the crossbow's laser sits on the mirrored barrel, Blink aims from the right
   hand, no hole or inside-out surface on either hand or weapon (that would be a cull path not
   flipped), takedowns, and toggling live in F10.

## 7. Traps

- **Do not mirror the controller without X.** It compiles, it places the palm on the controller,
  and the hand comes out unmirrored and turned (`swap_mirror_needs_x` exists for that).
- **Do not give the bolt measurement the mirrored delta.** It would express a mirrored bolt against
  an unmirrored palm and corrupt the `ModelAxisL*` record for both modes.
- **Do not reflect before the finger work.** The open hand and the grab compose relative to the
  wrist from rows that must be in one world.
- **A HUD anchor's `d.hand` is a controller**; every other hand index in the mod is the game's.
  Mixing them puts a wrist panel on the wrong arm.
