# The crosshair on the weapon's aim ray - plan for review (VR-57)

2026-09-11. **Status: plan only, no code written.** Branch
`claude/vr-57-crosshair-on-hand-ray` off `VR-Main` at `c3d6972b`. Ticket VR-57
(the crosshair), with VR-34 (one ray) as the rule it has to satisfy and VR-35,
VR-36, VR-44 as the consumers that come later.

This is the first step of bringing motion controls back. It is deliberately the
smallest one that can be judged in a headset, and it changes nothing about how
the game aims or fires.

---

## 1. Where this starts

The weapons follow the tracked controllers (VR-33), but the game's crosshair is
still drawn where the HEAD looks. The player aims with one thing and is told
where they are aiming by another.

The previous attempt at hand aiming, `[MotionAim] Enabled=1`, is worse than
nothing today: with it on, crossbow shots leave permanently up and behind the
player. That is a headset observation from 2026-09-10, and the log of that run
backs it up - the arrow was redirected from the view direction
`(-0.81,-0.59,-0.09)` to a hand direction `(-0.51,-0.05,0.86)`, then again to
`(0.55,0.32,0.77)`, before velocity steering. Those are not small errors around
a correct answer; they are a different direction. The lever is `0` in the tested
configuration and stays `0` throughout this plan.

## 2. Why that ray was wrong, and why this is not a tuning problem

`motion_aim.cpp` builds its own ray: `HandRelSnap` takes a controller pose,
tilts it by `[MotionAim] PitchOffsetDeg` (default **40 degrees**), optionally
mirrors it (`FlipRight`, `FlipUp`), expresses it as head-relative components,
and `MaimDirFromView` rebuilds a world direction from a projectile's own spawn
yaw and pitch.

The input layer already says what that 40 degrees is compensating for. Its
comment on the pose actions (`openxr_input.cpp`, around line 82) states that the
GRIP pose reads tens of degrees low as an aim vector, which is why the runtime
exposes the AIM pose separately. **A forty-degree constant and two mirror flags
are the signature of a derivation that was fighting its own coordinate
convention.** Tuning it further is not the fix.

> Do not re-walk: adjusting `PitchOffsetDeg`, `FlipRight` or `FlipUp` to make the
> crossbow land near the reticle. That is fitting a constant to a wrong basis,
> and it was already paid for.

## 3. What already exists (more than expected)

| Piece | Where | State |
|---|---|---|
| OpenXR **aim pose** actions and spaces, bound for Touch/Index/Vive | `core/vr/openxr_input.cpp` (`g_aimL`, `g_aimR`, `/input/aim/pose`) | live |
| `input_get_hand_pose(hand, aimPose, pos3, quat4)` | `core/vr/openxr_input.h:75` | live; `aimPose=true` is "where this controller points" |
| Laser beam builder, already reading the AIM pose | `openxr_runtime.cpp` `build_laser_from` (line ~3336: `input_get_hand_pose(hand, true, ...)`) | live, **no game-side caller** |
| `set_laser` / `set_laser_slot`, `LaserConfig` (hand, trims, dots, near/far, size) | `openxr_runtime.h:565` | ready, no callers |
| Aim dot builder: billboards a quad at a point, sized by angular diameter | `openxr_runtime.cpp` `build_aim_dot_slot` (line ~3512) | ready, no callers |
| `set_aim_dot` / `set_aim_dot_slot`, `AimDotConfig { enabled, valid, posXr[3], sizeDeg }` - `posXr` is **XR LOCAL space, metres** | `openxr_runtime.h:608` | ready, no callers |
| Blink's trace redirect (the engine's own trace, hit read at `+0x0d0`) | `game/dishonored/blink.cpp` | live for Blink |
| Controller-to-game-world mapping, measured | `mesh_split.cpp` (`B * F * transpose(R_head)`), `camera::world_scale()` | live, used by the hands |
| Projectile steering after spawn | `motion_aim.cpp` | live, **off**, and wrong (section 2) |

So the render path for both a beam and a dot is finished and untested in this
game, and the pose it wants is already published. **Step 1 needs no engine
knowledge, no trace and no game-space conversion at all.**

## 4. The rule this has to satisfy: ONE RAY

The project's rule, paid for in the BioShock trilogy mod: anything that claims
to point where shots go derives from the identical ray. The crosshair, the
projectile, Blink, any assist and any debug draw read one source. Two
derivations disagree by a small amount that is invisible until a player misses a
shot they were told they would hit.

**Design:** one function owns the ray and every consumer is listed in a comment
at its definition (VR-34's first pass criterion). Proposed shape, in a new
`game/dishonored/aim_ray.{h,cpp}`:

```
struct AimRay {
    bool     ok;            // false = no pose this frame; why says which gate
    int      hand;          // 0 left, 1 right - which controller it came from
    float    originXr[3];   // XR LOCAL space, metres
    float    dirXr[3];      // unit, XR LOCAL space
    uint32_t gen;           // pose generation it was built from
    const char* why;        // the refusal reason, always set
};
AimRay ray();               // consumers: the dot, the laser, (later) the
                            // projectile steering, Blink, the powers
```

The game-space form (origin and direction in unreal units) is a SECOND function
in the same file, built from the same `AimRay` through the hands' existing
mapping and `camera::world_scale()`. Nothing else may build either one.

## 5. The ladder

One behavioural change per build. Each step names what it proves and what kills
it. Steps 1 and 2 are this branch; the rest are their own tickets and branches.

| # | Step | Proves | What kills it |
|---|---|---|---|
| 1 | Publish a dot (and optionally the laser) at a FIXED distance along the aim ray, XR space only | the ray, the space conversion and the whole render path | no dot appears -> the layer gating or the publish is wrong, and nothing later is worth trying |
| 2 | Headset: does the dot sit where the controller points, and on the crossbow's barrel line? | the aim pose is the right source | it points elsewhere -> compare against the grip pose in the same log line, and against the drawn weapon's own direction |
| 3 | Replace the fixed distance with a real trace hit (borrow Blink's trace, or ship the fixed distance as a labelled fallback) | the dot sits ON surfaces | the trace cannot be called off Blink's path -> keep the fallback, open a ticket |
| 4 | Route the projectile steering through the same ray; delete `PitchOffsetDeg` and the flips | shots and crosshair agree by construction | shots still diverge -> there are two rays and step 1 was not done properly |
| 5 | Suppress the game's head-locked reticle, behind its own lever | the two stop fighting | - |
| 6 | Blink (VR-36) and the powers (VR-44) consume the same ray | one ray end to end | - |

**Step 1 is the whole ask of this branch.** It cannot move a shot, because it
writes nothing the game reads.

## 6. Step 1 in detail

* `aim_ray.cpp` calls `input_get_hand_pose(hand, /*aimPose=*/true, pos, quat)`
  and rotates the OpenXR forward axis (`0,0,-1`) by the quaternion. Use
  `core/util/xr_math.h`, the same helpers the laser's trim uses; do not write a
  new quaternion path.
* Publish once per present, from the present thread (the runtime layer's lane
  rule), via `set_aim_dot` with `posXr = origin + Distance * dir`, and
  `set_laser` when the beam lever is on.
* `[Crosshair] Hand` selects the controller. Default **left**: in this game's
  mapping the left controller is the weapon hand, and that is the hand this
  first test should use.
* Every refusal logs its reason once per spell: no session, no pose this frame,
  the method is not in projection mode (both builders require it and valid
  views), the publish went stale (the dot builder drops a publish older than its
  staleness bound).

### Levers, all default OFF in the tree

| Lever | Default | Meaning |
|---|---|---|
| `[Crosshair] Dot` | `0` | publish the dot |
| `[Crosshair] Laser` | `0` | publish the beam as well (a beam makes a wrong ray obvious; a dot alone can look plausible) |
| `[Crosshair] Hand` | `left` | which controller |
| `[Crosshair] DistanceM` | `8.0` | fixed distance, step 1 only, and labelled a fallback in the log |
| `[Crosshair] SizeDeg` | `0.5` | angular diameter |
| `[Crosshair] HideGame` | `0` | not implemented in step 1; the key is reserved so the ini does not change shape later |

Seam word `crosshair status|dot on|off|laser on|off|hand left|right` and an F10
control (the repo's rule: a seam word without an F10 control is not shipped -
VR-81 exists because that rule was missed once already). The tester's installed
ini gets `Dot=1` and `Laser=1` armed before handover; the tree default stays off.

### Instruments that can fail their own hypothesis

* **Aim versus grip, on the same line.** Log both poses' forward vectors and the
  angle between them, once per second while armed. If a runtime returns the grip
  pose for the aim action, that angle is near zero and the line says so - which
  is the one reading that would make the old 40-degree offset reasonable.
* **The ray against the rendered barrel.** The weapon's drawn direction is
  available from the hands' own draw (`mesh_split.cpp` publishes the hand's
  transform each frame). Print the angle between it and the aim ray. Healthy is
  a small, steady angle (the grip-to-barrel difference); a swinging angle means
  one of the two is built in the wrong frame.
* **The publish, end to end.** Count publishes, and count the presents where the
  builder refused, with the reason. Equal-and-zero is the honest reading for a
  menu; publishes with no dot on screen names the gating.
* A zero that is expected says so on its own line.

## 7. Test protocol (one question per launch)

1. **Launch A - does the dot appear and follow the controller?** Stand still,
   point the left controller at a wall, sweep it. Pass: a dot (and beam) that
   move with the controller, no game behaviour change, no new hitches. Fail with
   no dot: read the refusal reason.
2. **Launch B - is it the right ray?** Point the crossbow along the dot and
   compare with the barrel; look away from the dot and back. Pass: the dot stays
   on the barrel line, does not follow the head, and the aim-versus-grip line
   shows a real aim pose.

Perceptual verdicts come from the headset; the log must be able to explain a
failure without another run.

## 8. Questions for the reviewer, before code

1. Is `AimRay` best placed in `game/dishonored/`, or does it belong in `core/`
   alongside the input layer, given the later consumers (projectile, Blink,
   powers) are all game-side?
2. `build_aim_dot_slot` and `build_laser_from` both require projection mode and
   valid views. Confirm that is true in this host with `reentry` (the mono quad
   path would silently show nothing), and whether the dot layer's budget
   interacts with the existing layers this game submits.
3. Is there any reason the fixed-distance step should use the laser's dots
   instead of the single aim dot? The laser draws several quads along the ray
   and would answer "is the ray straight and forward" more directly.
4. The trace (step 3): can Blink's trace be invoked outside Blink's aiming
   state, or is it only reachable while Blink is charging? If it cannot be
   borrowed, is a fixed distance acceptable to ship behind a lever while the
   real trace becomes its own ticket?
5. Step 4 deletes `PitchOffsetDeg` and the flips. Any reason to keep them as
   compatibility keys rather than removing them with a `RELEASE_NOTES` line?
6. Anything in section 2's diagnosis you consider unproven. It rests on a code
   reading plus one headset report, not on a measurement of the old ray.

## 9. Non-goals and blast radius

Step 1 writes nothing the game reads: no projectile, no camera, no input. The
risk is the layer budget and one more publish per present.

Not in this branch: the trace, the game reticle, motion melee and crouch
(VR-37), the powers (VR-44), Blink (VR-36), the custom dot's appearance beyond
size (VR-35), and `[Mode] GamepadOnly=0` as a default (VR-40).

## 10. What this plan is built on

* VR-57's own order of work and its lever list (this document is the plan it
  references).
* VR-34: one ray, with named consumers, provable rather than asserted.
* `docs/dishonored/VR-33-HANDS-AND-WEAPONS.md` for the measured
  controller-to-game mapping and its graveyard - in particular that a frame
  question cannot be settled by asking whether a hand looks right, and that the
  neutral must not be stored in head space.
* `docs/TRAPS.md`: an instrument that cannot fail its own hypothesis is not
  evidence; a counter is not evidence until you know its population.
* `docs/dishonored/HANDOFF-GINGASVR.md`: one behavioural change per build, and
  never ship a guessed constant as a measured one.

## Implementation review follow-up (2026-09-11)

Step 1 is implemented and installed for user-run testing. See
[VR-57-CODEX-HANDOFF.md](VR-57-CODEX-HANDOFF.md) for the reviewed decisions,
implementation, hashes and validation. The original plan above is preserved.
Key corrections: the legacy weapon uses HandRelFull, not HandRelSnap; grip/trim
is a hypothesis rather than an isolated cause; no measured barrel axis is
published; the old laser re-derives a second ray; and the original visual stage
omits held-frame submissions. The new endpoint and beam share explicit points
from one ray and are appended after the held projection fallback. The game was
not launched by the implementation agent. MotionAim remains disabled.
