# The aiming pipeline, end to end (VR-57, VR-34)

2026-09-12. Everything known about hand aiming in this mod: what the chain is,
what was built, what every number measured, what is ruled out, and the leads
worth the next session. Written so a reader who has never seen this feature can
start from the top and be current by the end.

Branch `claude/vr-57-crosshair-on-hand-ray` (off `VR-Main` at `c3d6972b`).
Builds 100 through 110. Nothing here is merged.

---

## 1. What the player wants

Point the controller, have the shot go there, and see where it will go before
firing. Today the mod places the weapons on the tracked controllers (VR-33) but
the game still aims from the HEAD, so the player aims with one thing and is told
where they aim by another.

The tester's own framing, which shapes the design: the laser should follow the
game's own crosshair, and the crosshair should be driven, because the crosshair
carries the game's aim assist and that assist is worth keeping (optional later).

## 2. The chain, from controller to bolt

```
OpenXR runtime
  aim pose   (/user/hand/*/input/aim/pose)  -> where the controller POINTS
  grip pose  (/user/hand/*/input/grip/pose) -> the handle
        |
        |  openxr_input.cpp: both located against the app space (LOCAL),
        |  published through atomics; input_get_hand_pose(hand, aimPose, ...)
        v
game/dishonored/aim_ray.{h,cpp}         the ONE ray (XR LOCAL metres)
        |                                dvr::aim::ray(), consumers listed there
        |
        +--> core/vr/aim_visual.h -> openxr_runtime.cpp build_aim_visual()
        |       the dot and the beam, as compositor quads in XR LOCAL space
        |
        +--> game/dishonored/aim_seam.cpp  AsHandDirGame()
                 the ray mapped into the GAME's world axes, through a
                 roll-free head frame and MaimDirFromView's view basis
                        |
                        v
                 the game's own aim-assist cache, written per script tick
                 (DisItemContext_FireCrossbow / _FirePistol, +0x00d0)
                        |
                        v
                 the engine's fire path  ->  the bolt
```

Everything the game does downstream of that cache is native code. The scripts
declare the data; the behaviour is in the exe.

## 3. What was built (and the ini keys)

| Piece | Where | Key |
|---|---|---|
| The one ray, from the aim pose | `game/dishonored/aim_ray.{h,cpp}` | - |
| Dot and beam as compositor quads | `core/vr/aim_visual.h`, `openxr_runtime.cpp` | `[Crosshair] Dot`, `Laser`, `Hand`, `DistanceM`, `SizeDeg` |
| Both poses drawn for comparison | `aim_ray.cpp` | `[Crosshair] BothPoses` |
| Read-only probe of the game's aim cache | `game/dishonored/aim_seam.cpp` | `[Aim] SeamProbe`, `SeamVerbose` |
| Writing that cache from the ray | same | `[Aim] DriveFromHand`, `DriveDistanceUU` (0 = follow the beam) |
| F10 Aim tab, `crosshair ...` seam word | `core/ui/overlay.cpp`, `commands.cpp` | - |

All default OFF in the tree. The tester's installed ini has `Dot=1 Laser=1
Hand=left BothPoses=1 SeamProbe=1 DriveFromHand=1 DriveDistanceUU=0`.

## 4. What the game does, from the decompiled scripts

Read from `tools/uscript/dishonored` (gitignored, never copied into this tree).

* The player's ranged weapons each own an item context:
  `DisItemContext_FireCrossbow` and `DisItemContext_FirePistol`, both extending
  `DisItemContext_ProjectileAttack`, which extends `DisItemContext_AimAssistAttack`.
  The parent's cache is `m_CachedAimAssistPos`, resolved live at **+0x00d0**:
  `{ int m_TickTag; bool m_bFound; Vector m_AimPos; Vector m_AimDir;
  Vector2D m_ProjectedAimPos; bool m_bWillTrack; }`.
* The contexts hang off the equipped item:
  `DishonoredInventoryItem.m_ContextSlots_Primary_Player`, reached from the pawn
  through `m_pInventory` and `DishonoredInventory.m_Slots`.
* Aim assist is a full system with per-hit-region windows and weights
  (`DisAimAssistInfo`, `DisAimAssistChoiceInfo`: `m_fMaxAimAssistAngle`,
  `m_fMaxAimAssistDistance`, safe percents, per-weapon tweaks in
  `DisTweaks_ProjectileAttack`). Settings exist for gamepad and mouse strength.
* The HUD declares crosshair states, per-item crosshair settings and a
  crosshair style; the player controller carries `m_CrosshairStatus`,
  `m_pCrosshairActor`, `m_pCrosshairHighlightActor`.
* Two cheat functions name the systems directly: `PlayerToggleAimAssist` and
  `IgnorePlayerAimOffset`.

## 5. What every run measured

| Question | Answer | Run |
|---|---|---|
| Does the game compute the fire direction itself? | Yes. With `[MotionAim] Enabled=0` the bolt goes where the game's crosshair is. | tester, 2026-09-11 |
| Is the aim cache live on the equipped weapon? | Yes. `DisItemContext_FireCrossbow`, tick tag advancing. The first probe found only the class default object because it matched the parent class name exactly. | build 102, 103 |
| Does that cache follow the head? | Yes. When the assist found a target: `found=1 willTrack=1 aimPos (8142 5484 2400) aimDir (+0.506 -0.103 -0.856) projected (0.046 0.003)`, `dot(view)=+0.998`. | build 103 |
| Can we write it? | Yes. 485,083 writes, zero refused; the game recomputes each tick and our write rides on top. | build 104 |
| Was the first driven ray right? | **No.** It pointed up: `dir (+0.491 -0.119 +0.863)`, 60 deg off the view. Cause: the legacy path reads the GRIP pose (`g_devPose`, filled by `present_tick` from `input_get_hand_pose(h, false, ...)`) and tilts it by `[MotionAim] PitchOffsetDeg` (40). | build 104 |
| Is the aim-pose ray right? | The mapping is: the angle off the head in XR equals the angle off the view in game, exactly (30.0/30.0, 25.3/25.3, 11.0/11.0). | build 105 |
| Where does the aim pose point when the controller is held straight? | With the arm extended and steady (36 samples): **az -12 deg, el +5 deg** off the head. Near straight ahead. The hand sits 0.56 m in front of and 0.12 m below the head, which is a real extended arm. | build 110 |
| Where does the grip pose point? | Nearly straight up: el **+74 to +84 deg**. Its beam is out of view above the player, which is why the both-poses comparison looked like one beam. | build 110 |
| Aim versus grip angle | A constant **60.00 deg** - the fixed Touch grip-to-aim offset, which is also what the legacy 40 deg tilt was approximating. | build 100+ |
| Layer budget | Not a limit: 10 points wanted, 10 drawn, 1 layer used of `maxLayerCount` 16. An earlier reading of "budget 2" was wrong - `dotFrames`/`beamFrames` count FRAMES, not points. | build 110 |

## 6. What the tester sees, and what it means

1. **The beam does not lie along the controller** - reported as roughly 45 deg
   left and 10 to 20 deg up. The ray it is drawn from measures near straight
   ahead (section 5), and it is drawn from the controller's own position, so the
   remaining suspect is the alignment between compositor-drawn quads and the
   game's rendered world, NOT the ray.
2. **Shots are pulled toward the game's crosshair** - firing left of it lands
   right, firing right of it lands left, and the tester confirmed this repeats.
   That is the aim assist clamping how far it will move a shot from its own
   solution (`m_fMaxAimAssistAngle` and the choice weights).
3. **The weapon model is rotated from the controller.** `SHIFT+F7` (the VR-33
   grip capture) improved it; the stored grip is `GripL 24.2, 62.2, -53.7` with
   a further `TrimLRX 23`. Model alignment is separate from the ray and is
   tuned by the numpad adjust.
4. **The game's crosshair settles back to one spot** regardless of head
   movement. That is what a ray-anchored crosshair should do; the lag is the
   HUD's own smoothing.

## 7. The leading hypothesis for the beam, and how to kill it

**The compositor quads and the rendered world do not share a frame.** The world
arrives as a projection layer built by the stereo method from the game's own
camera, with a claimed FOV; the dot is a quad placed in XR LOCAL space. If the
claimed FOV, the submitted view poses or the eye tagging differ from what the
game actually rendered, then a quad at a true XR direction will not sit on the
world feature in that direction, and the error GROWS with angle from the centre
- which matches a beam that looks roughly right ahead and badly wrong to the
side.

What would settle it, in order:

1. **The world-anchored control.** Place a dot at a fixed XR point straight
   ahead of the HEAD (not the controller) at 8 m and look straight at a wall
   feature. If a head-anchored dot also sits wrong, the ray is exonerated
   entirely and the fault is the layer alignment.
2. **The FOV audit.** `xr: fovaudit` already reports the submitted tangents
   (`tanH 1.378 tanV 1.428`, hfov 108.07, from readback). Compare against the
   game's own rendered FOV (`dvr::camera::rendered_fov_deg()`, the 0x53c
   sensor) in the same window. A mismatch is the mechanism.
3. **The eye tag and pose.** The projection views are submitted with
   `parallel_eye_tag` poses under the eye-tag path; confirm the submitted view
   pose equals the pose the game rendered with (VR-65's pose record already
   measures this class of fault).

Only if all three come back clean should the aim pose itself be doubted again.

## 8. Ruled out, with evidence - do not re-walk

* The quaternion convention: `(x,y,z,w)` at every call site, one shared
  `quat_rotate`.
* The reference space: hand poses locate against the app space, which IS
  `g_space` (LOCAL), and the quads are submitted in `g_space`.
* The layer budget: 10 of 10 points drawn (section 5).
* The grip pose as the beam's source: it points 74-84 deg up.
* Tuning `[MotionAim] PitchOffsetDeg` or the flip flags: that path is the grip
  pose plus a constant, and Blink uses the same path, so changing it moves
  Blink too.
* Post-spawn projectile steering as the mechanism for hand aim: the engine
  computes the fire direction; steering after the fact is what shoots up and
  behind.

## 9. Leads in the decompiled scripts, for the next session

The scripts are declarations; the behaviour is native. They are still the
cheapest map of WHICH names to resolve at runtime. Worth mining:

* **The fire seam.** How a shot gets its direction: `DisItemContext_FireCrossbow`,
  `DisItemContext_FirePistol`, `DisItemContext_ProjectileAttack` (native
  `WeaponRanged`), `DishonoredWeapon_Ranged`, `DisProjectile_Arrow`,
  `DisSeqEvent_PlayerFiredWeapon`, `DisDLC07SeqEvent_PlayerFiredWeapon`. Look
  for any property naming a start location, a direction, a socket or a spread,
  then resolve it live with the property resolver and watch it across a shot.
* **The assist clamp** (what pulls shots back to the crosshair):
  `DisItemContext_AimAssistAttack`, `DisTweaks_ProjectileAttack`,
  `DisAimAssistInfo`, `DisAimAssistChoiceInfo`, `DisAimAssistPawn`,
  `DisAimAssistGroup`, `DisAimAssistPlatform`. Find the per-weapon max angle and
  whether a tweaks object can be read or written at runtime, and what
  `PlayerToggleAimAssist` toggles.
* **The crosshair.** `DisGFxMoviePlayerHUD` (crosshair states, per-item settings,
  `m_CrosshairName`), `DishonoredPlayerController` (`m_CrosshairStatus`,
  `m_pCrosshairActor`), `ArkProfileSettings.ECrosshairStyle`. The question worth
  answering: does the HUD place the crosshair from `m_ProjectedAimPos`, or from
  its own projection of `m_AimPos`?
* **The barrel axis** for the tester's bolt-tip idea: the weapon meshes and
  their sockets (`DishonoredItemSkeletalComponent`, `DisWepCrossbow`'s arrow
  mesh), and whether a muzzle socket exists by name. The runtime's laser already
  has a muzzle mode that takes a barrel axis.
* **The cheats**, because they name systems: `IgnorePlayerAimOffset`,
  `PlayerToggleAimAssist`.

## 10. The rules this work runs under

* One behavioural change per build; one question per launch.
* The tester runs the game. Never launch it. Arm everything in the INSTALLED
  ini, then hand over with one instruction.
* Build Release and install every change without asking.
* Every lever ships default OFF in the tree, with a live A/B.
* An instrument that cannot fail its own hypothesis is not evidence. Say what
  would make a counter move, and let it print the unwelcome answer.
* A counter is not evidence until you know its population. (`dotFrames` counted
  frames while being read as points, in this very feature.)
* Never merge to `VR-Main` without explicit permission.
* Findings go in `docs/`; decompiled script text never does.
