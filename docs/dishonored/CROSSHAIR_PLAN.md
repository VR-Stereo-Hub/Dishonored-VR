# The crosshair on the weapon ray - plan (VR-57, 2026-09-09)

**Status: plan, nothing built.** One feature in two halves that must land
together: take the game's reticle off the head, and put ours on the ray the
weapon actually fires along.

---

## 0. The single rule this whole ticket hangs on

> **ONE RAY.** Anything that claims to point where shots go derives from the
> IDENTICAL ray. The crosshair, the projectile and any assist read the same
> source.

Paid for in the BioShock trilogy mod. A crosshair computed from a second,
parallel derivation is how two things silently disagree, and the disagreement is
invisible until a player misses a shot they were told they would hit.

So this is not "draw the reticle somewhere else". It is: **establish the weapon
aim ray as the single source, then have the crosshair consume it** - and make
that structurally true, not true by inspection.

---

## 1. The good news: most of the rendering already exists and is unused

The runtime layer adopted from BioShock already carries the whole dot pipeline,
and **nothing in Dishonored calls it**. `grep set_aim_dot` returns only the
header.

| Piece | Where | State |
|---|---|---|
| `AimDotConfig { enabled, valid, posXr[3], sizeDeg }` | `openxr_runtime.h:608` | ready |
| `set_aim_dot` / `set_aim_dot_slot` | `openxr_runtime.h:615,626` | ready, **no callers** |
| `build_aim_dot_slot` - billboards a quad at the point, sizes it by angular diameter | `openxr_runtime.cpp:3512` | ready |
| The dot texture + swapchain (shared with the laser) | `openxr_runtime.cpp` | ready |
| Staleness guard (`kDotStaleMs`) - a publish that stops removes the dot | `build_aim_dot_slot` | ready |
| Two independent slots | - | ready |

And its design comment already states the rule this ticket needs:

> Unlike the laser this computes NO ray: the point arrived from the game thread
> already in XR space, converted from the exact fire-seam ray. All that happens
> here is billboarding and sizing, so **there is no second algebra that can drift
> from the first**.

**That is the architecture, and it is better than what BRVR shipped.** BRVR's
crosshair is a head-locked VIEW-space quad moved by an angular offset
(`CameraHook_GetAimOffset` -> dYaw/dPitch), which was honest there because
BioShock's aim was head-driven. Here the weapon is on a tracked controller, and
a world-anchored quad at the ray's actual endpoint is both correct and simpler
than reproducing an angular offset.

**So the render half is close to free. The work is the ray and the suppression.**

---

## 2. The four pieces, in dependency order

### Piece A - the ray, and making it the only one

`motion_aim.cpp` already steers projectiles onto a "hand ray"
(`aim: velocity steered onto the hand ray`). **That ray is the source.** It is
not currently exposed; the first change is to give it an accessor and route the
existing steering through the same accessor, so there is exactly one derivation
by construction rather than by review.

Shape:

```
bool MaGetAimRay(int hand, float originGame[3], float dirGame[3]);   // game space
```

Every consumer - the projectile steering that exists today, the crosshair that
does not - calls this. If the steering keeps its own copy of the math, the rule
is already broken and the ticket has failed.

### Piece B - the hit point

The dot needs a POINT, not a direction. Three sources, in order of preference,
because **reading beats reproducing**:

1. **The engine's own trace.** Blink already hooks it - `blinkdir` redirects the
   trace SOURCE and the engine traces the vector we hand it, with the result
   landing at a known offset (`ENGINE_NOTES`, the blink section; the raw trace
   hit reads at `+0x0d0`). If that trace can be asked for an arbitrary ray
   without disturbing Blink, we get the real hit point the game itself would
   compute, and there is no second algebra anywhere.
2. **The projectile's own resolved target**, if the fire path already computes
   one.
3. **A fixed distance along the ray** - the fallback. Honest but wrong at range,
   and it must be labelled as a fallback in the log, not silently substituted.

**Open question to answer before writing code**: can the Blink trace be invoked
off its own path, or is it only reachable while Blink is aiming? If it cannot be
borrowed, option 3 ships first with the ini key that says so, and option 1
becomes its own ticket.

### Piece C - game space to XR space

`build_aim_dot_slot` wants `posXr[3]`. The BioShock side had
`game_point_to_xr`; Dishonored has the pieces but not that function:

* `camera.cpp` owns the world scale (`set_world_scale`, `uuPerM`).
* VR-33 built the coordinate bridge for the hands, which already maps a game
  placement into the frame the draw uses.

This is one new function, and it must be **derived from the same bridge the
hands use**, not written fresh - two conversions between the same two spaces is
the same failure mode as two rays.

### Piece D - suppress the game's reticle: THE GAME ALREADY DOES IT

The game's own settings can turn the crosshair off. That removes almost all of
this piece: no hook, no Scaleform work, no render change. What remains is one
documentation line telling the player to switch it off, and an ini key only if
we ever want to do it for them.

**Do not build a suppression hook.** The two open questions this piece carried -
Scaleform or engine-composited - no longer need answering to ship the feature.

#### (superseded) the original piece D

The game's crosshair is a Scaleform element drawn at screen centre, so it is
baked into the eye image and inherits everything wrong with being head-locked.

**Open question**: is it Scaleform, or composited by the engine? That decides
whether this is a HUD change or a render change, and it is the first thing to
measure. The `hud_stub` seam and the Scaleform notes are the starting point.

**It must be a lever, default OFF like every render lever here**, because
hiding the game's reticle while ours is not yet trustworthy leaves the player
with nothing.

---

## STEP 1 DONE (2026-09-09): the ray has one home

`src/game/dishonored/aim_ray.cpp`, `aimray` on the seam, and a line every 5 s on
the motion-aim tick so a headset run shows whether it resolves without anyone
typing a command.

**What is shared, and what honestly is not.** The composition is
`MaimDirFromView`, and both consumers call it verbatim - one algebra, one place,
and its own comment now says so. They differ only in the VIEW BASIS they pass,
and that difference is real:

* the projectile steering passes the **shot's own spawn forward** - the ground
  truth of where the game aimed, which cannot exist before the shot;
* the crosshair passes the view **the camera is currently on**
  (`g_viewYawRad`/`g_viewPitchRad`, what head_track just wrote), because there is
  no projectile yet.

**So the crosshair is a PREDICTION**, and that is stated on the line rather than
hidden. Its test is step 6: fire, and the bolt lands under the dot. If it does
not, the engine builds the shot from a view we are not tracking - and the fix is
to track that view, **never** to give the crosshair a second derivation.

The origin is the game camera's own world position (`g_camObj + 0x80`, the read
blink.cpp already uses), because that is the point the direction is relative to.
The hand's position would be a second, unverified claim about where shots start.

Every refusal names itself: no controller pose, degenerate direction, no
readable camera object. A ray that is unavailable in a menu is normal and must
not read as a fault.

---

## 3. Order of work, with what each step proves

| # | Step | Proves | Kills it |
|---|---|---|---|
| 1 | Expose `MaGetAimRay` and route the EXISTING steering through it | one derivation, structurally | - |
| 2 | Publish a dot at a FIXED distance along that ray; `set_aim_dot` | the whole render path lights up, and the dot tracks the hand | no dot appears -> the runtime path or the space conversion is wrong, and nothing else is worth trying |
| 3 | Headset: does the dot sit on the weapon's barrel line? | the ray and the conversion agree with what the player sees | - |
| 4 | Replace the fixed distance with the engine trace's hit point | the dot sits ON surfaces | the trace cannot be borrowed -> ship the fallback, open a ticket |
| 5 | ~~Suppress the game's reticle~~ **turn it off in the game's own settings** | the two are not fighting | - |
| 6 | Headset: point away from where the old crosshair sat, fire, and the shot lands under OUR dot | the ONE RAY rule holds end to end | it does not -> there are two rays and step 1 was not done properly |

**Step 2 is the cheap one that de-risks everything.** It needs no trace, no
suppression, and no engine knowledge beyond the bridge - and if the dot does not
appear, every later step was going to fail anyway.

---

## 4. What ships as a lever

Per project rule, every new render lever is default OFF with a live A/B:

| Key | Default | What |
|---|---|---|
| `[Crosshair] Dot` | 0 | our dot on the weapon ray |
| `[Crosshair] Hand` | 0 (left) | which controller carries the aiming weapon - configuration, not a constant |
| `[Crosshair] SizeDeg` | 0.5 | angular diameter, the units `AimDotConfig` already uses |
| `[Crosshair] Distance` | 8.0 m | the FALLBACK distance, used only until the trace lands |
| `[Crosshair] HideGame` | 0 | suppress the engine's own reticle |

`crosshair status` on the seam prints which ray fed it, whether the point came
from a trace or the fallback, and the last published position - so a wrong dot
is arithmetic instead of opinion.

---

## 5. The sword, deliberately deferred

Melee has no meaningful ray and a dot on it would be noise. When the equipped
item is the sword, the dot publishes `enabled = false` and the game's own
reticle stays. That needs the equipped-item state, which is
`docs/dishonored/GAMEPLAY_STATE.md` and VR-61's property resolver - already
landed, so this should be readable rather than guessed.

---

## 6. What could go wrong, from this project's own record

* **Two rays.** The failure this ticket exists to prevent. Step 1 first, always.
* **Two space conversions.** Same shape. Reuse the hands' bridge.
* **A fallback that does not announce itself.** A fixed-distance dot that silently
  stands in for a trace will be read as "the trace works and is wrong".
* **Hiding the game's reticle before ours is trusted.** Default OFF, separate key.
* **Believing a verified write is an honoured one.** The dot appearing in the log
  is not the dot appearing in the headset; acceptance is the step-6 shot test.

---

## 7. Done when

- [ ] The crosshair sits where the weapon points, not where the head looks.
- [ ] The crosshair, the projectile and any assist derive from ONE ray, and the
      code makes that structurally true rather than true by inspection.
- [ ] Headset: point away from the old crosshair's position and the shot lands
      under the dot.
- [ ] The ray's derivation and the space conversion are in `ENGINE_NOTES.md` in
      the same commit as the code.
- [ ] The sword case is handled by not drawing, and says so in the log.
