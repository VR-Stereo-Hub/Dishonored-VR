# VR-165: the camera that leaves the body (research brief, 2026-09-22)

The single starting point for the session that finds the cause. It consolidates what is
measured, what is eliminated, the instruments already built, and the plan in order. The
detailed history is in `FLICKER_REFERENCE.md` (every `VR-165` entry, newest first) and
`PLAN-VR161-VR165-review.md` Part 2. Read those for provenance; act from this file.

## 1. The symptom

The whole view is displaced from the player's body: the player feels lifted and tilted,
looking up sends the view into the sky and looking down swings it wildly. The player can
still move. It persists for tens of seconds or until something resets it. It is the smooth
camera category (FLICKER_REFERENCE section 1), not an eye, tag or frame fault.

**Triggers seen:**
- releasing a climbing chain with X (jumping off does not do it), reproducible;
- the knockback and shake of an exploding oil container (2026-09-22, build 679), hard to
  reproduce. **The chain is therefore one trigger, not the cause.**

**What clears it:** Blink, mantling, the weapon wheel once, a save reload. Pause freezes it.
It continued at the same rate inside the slowed-time weapon wheel, so it runs on real time
or per present, not on game time.

## 2. Measured (the facts any theory must fit)

| Fact | Source |
|---|---|
| Our writers are flat during the swing: 0 reversals in our position request and the head pose | FLICKER_REFERENCE VR-165, 2026-09-20 |
| `Camera.ModifierList` holds one entry, `CameraModifier_CameraShake`, alpha 0, always | same, 120+ tables |
| The camera object and class never change (`DishonoredPlayerCamera`) | PLAN-VR161-VR165-review |
| Pawn EyeHeight stays 85 | Run 549 audit |
| **Bugged: camera POV minus pawn 125-281 uu; healthy 44-104 uu** | chain run and explosion run agree |
| **Explosion run: at the onset, PlayerControl's own source POV minus pawn stayed (-5, 7, 77), healthy, for the whole 49 s window** | run4 camera/source, 2026-09-22 |
| Every influence weight/target normal throughout (HitReact 0/0 because the mod holds it) | same |
| Standing still while bugged: z mean 139 uu (healthy 63), never under 124; x +35 uu; y spread 40 uu (healthy 14) | same |

**What that means:** the base first-person camera is correct; the displacement is ADDED
after it by the additive influence graph (`m_InfluenceGroups` group 2, the reaction group).
It is mostly a stuck offset, about 75 uu up and 35 sideways, which head rotation then
swings, which is the "lifted and swinging" feel. Influence WEIGHTS are eliminated; influence
STATE is not.

Onset timeline of the explosion (seconds into run4): 567.0 healthy, 567.5 displaced with
the pawn at rest, 569.5 the knockback fall (`Falling`, velocity -723 uu/s sideways).

## 3. Eliminated (do not re-walk)

The mod's own position and rotation writers; the neck model; the modifier stack; a camera
object swap; the game re-grabbing the chain; locomotion; influence weights. Two frequency
figures (~8 Hz, ~110 Hz) were instrument artefacts and are retracted: **quote no period.**

## 4. The suspects, from the game's own declarations

Group 2 of the influence graph (`DishonoredPlayerCamera.m_InfluenceGroups` +0x468):
`BumpSmoother, PhysicalReact, HitReact, Lean, Shake, Recoil, DisCamera_Rumble, DisCamera_Aim`.
The ones that keep STATE between frames:

| Influence | State | Config (the game's DishonoredCamera.ini) |
|---|---|---|
| `DishonoredCamera_PhysicalReact` | two `DisSpringPoint` springs, `m_StabilityPoint` and `m_StrengthPoint` (`m_Pos`, `m_Velocity`, previous pos/vel, `m_BoundPos`), rotated about `m_CameraPivotOffset` | springiness 85 / 75, damping 8, velocity cap 700, **fixed time step on**, **camera collision on** |
| `DishonoredCamera_HitReact` (extends PhysicalReact) | the same springs | springiness 200 / 300, damping 15 / 25, **pivot Z = 200 uu**; the mod holds its weight at 0 since VR-172 |
| `DishonoredCamera_Shake`, `_Recoil` (extend PhysicalReact) | the same springs | stiff (1000-2000), collision off |
| `DishonoredCamera_Lean` | `m_HeadPoint` spring, lean pivot and angle, camera-collided flag | spring 80 / 12, fixed time step |
| `DishonoredCamera_BumpSmoother` | `m_fLastInterpolatedHeight`, `m_bIsCompensating` | smooth distance 400 |
| every `DishonoredCameraInfluence` | `m_bActive`, `m_bIsSleeping`, `m_fAccumulatedDelta`, `m_fCurFixedTimeStep` | |

Also on the player camera: collision smoothing (`m_fCamCollisionLargestUnsmoothedPop` 50,
`m_fCamCollisionSmoothSpeed` 12). A camera pushed into geometry and smoothed back is another
path that keeps state.

**Leading hypothesis:** one PhysicalReact-family spring (or its sleeping/accumulated-delta
state) is left off rest by an impulse (a chain release, an explosion) and does not return,
so its pivoted offset stays applied. The chain bug predates the mod's HitReact hold (VR-165 on
2026-09-20, VR-172 on 2026-09-21), so the hold is not the cause, though it may matter to how
HitReact behaves now.

**Counterprediction:** in a bugged window every spring reads at rest (m_Pos near 0, no
velocity), as in a healthy one. Then springs are eliminated and the next place is the native
combine of the graph, or camera collision smoothing.

## 5. Instruments already on VR-Main

| Instrument | What it gives | Where |
|---|---|---|
| `camera/springs` | every 500 ms, no budget: the eye offset and each spring's `m_Pos` and speed for PhysicalReact, HitReact, Shake, Recoil, Lean's head point, BumpSmoother's height | `cam_modifiers.cpp` |
| `camera/displaced` | one WARN per episode after 1.5 s above 115 uu, and a line when it ends: the grep anchor | same |
| `camera/source` | the verbose per-influence table (weights, PlayerControl/AnimDriven debug POVs), first 1800 samples | same |
| `swing:` raw series | present-rate camera position samples on a large excursion, 6 dumps a run | `swing_trace.cpp` |
| `camshake capture <s> <tag>` | per-game-tick CSV of the game's camera motion minus the pawn, with the mod's own offsets removed; A/B by holding a handle at 0 | `cam_shake.cpp` |
| `camshake allow <category> on|off` | hold PhysicalReact (Landing), HitReact (Hits), Shake/Rumble (Generic), Recoil (Fire) at weight 0 live | same |
| `ue3-natives.py class <Name>`, `disasm-rva.py` | class to vtable and the native update code, offline | `tools/` |

Logs to compare: the explosion run is archived at
`build/playtest-candidates/vr185-186-hud-groups/run4` (local, gitignored); the earlier chain
runs are under `build/playtest-candidates/` (camera-source).

## 6. The plan, in order

1. **Widen the census before anyone plays.** Add, per stateful influence: `m_bActive`,
   `m_bIsSleeping` (bool bits: `FindBoolProp`), `m_fAccumulatedDelta`, the spring's
   `m_BoundPos` and previous position, and Lean's pivot/angle/collided flag. Add the player
   camera's collision smoothing state if it has one by name. Keep one line per sample.
2. **Make it reproducible on demand.** A seam word that kicks one influence's spring by
   writing its `m_Velocity` (`camspring kick <influence> <x y z>`), default nothing. If a kick
   leaves the camera displaced after the spring should have settled, the bug is reproduced
   without a chain or an explosion, on the simulator as well as the headset. If every kick
   settles, the springs alone are not it, and the kick is repeated during a chain release to
   look for the missing ingredient (collision, sleeping, a fixed-step accumulator).
3. **Read the native update, offline.** `ue3-natives.py <exe> class DishonoredCamera_PhysicalReact --verify`
   to its vtable, find the influence update slot, and disassemble how the spring target
   (`m_BoundPos`) is set, how the pivot is applied, when the influence sleeps, and what
   Blink/mantle/AnimDriven reset. Record findings in ENGINE_NOTES with the derivation.
4. **Name the owner on a real repro.** Chain X-release (reliable). Grep `camera/displaced`,
   read the `camera/springs` lines around it. One launch, one question.
5. **Only then, a fix.** It clears the stuck state at the moment it is known to be stuck
   (the way Blink does), not a clamp on the POV. The project rule stands: no guessed clamp,
   the fix ships with a live A/B and the log says when it acted and why.

Every result, including a failed prediction, goes into FLICKER_REFERENCE in the same commit
(its section 8 format).
