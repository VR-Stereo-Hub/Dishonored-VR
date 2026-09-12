# Plan: make the bolt follow the controller dot (VR-57)

2026-09-12. For review before implementation. Scope is deliberately narrow: get the
fired bolt to go where the controller's dot is drawn. The aim assist's pull toward
the game's own solution is a SEPARATE effect and is explicitly **not** in this
plan; it gets its own ticket.

Branch `claude/vr-57-crosshair-on-hand-ray`, builds 100-112. Nothing merged.
Background, and every number quoted here, is in `VR-57-AIM-PIPELINE.md`.

---

## 1. What is now settled, and must not be re-derived

These were open questions a day ago. Each is closed by a measurement, and the plan
below depends on all of them.

| Fact | Evidence |
|---|---|
| The game computes the fire direction itself; post-spawn steering is not the route | `[MotionAim] Enabled=0` and the bolt still goes to the game's crosshair |
| The equipped weapon caches the assist result every tick at `m_CachedAimAssistPos`, +0x00d0 | build 102-103, tick tag advancing on `DisItemContext_FireCrossbow` |
| That cache is writable and the write is not refused | build 104: 485,083 writes, 0 refused |
| The ray maps into game axes exactly | angle off the head in XR equals angle off the view in game (30.0/30.0, 25.3/25.3, 11.0/11.0) |
| The projection layer is aligned with the world it carries | build 111: head-anchored dot sits on the game's crosshair; claim 108.07 vs rendered 108.07 deg; pose tag 0.00 deg off the located pose with the head still |
| **The AIM pose is correct** | build 112, headset: the controller dot tracks the controller. The earlier "45 deg off" was a comparison against the separately rotated weapon MODEL, not against the controller |

The last row is the one that makes this plan worth writing. Until it landed, every
bolt result was confounded by a ray nobody trusted.

## 2. The one thing this plan changes

`[Aim] DriveFromHand` already writes the controller ray into the aim cache. It has
been off for the last two runs because it moves the game's crosshair, which was
the reference the layer tests were measured against. Those tests are done. The
drive goes back on, and the plan is about **the assumptions inside that write that
have never been tested separately.**

There are three, and they are listed in the order of how much they can cost.

### Assumption A: the engine uses our POINT, and our point is close

The drive writes both `m_AimDir` (a direction) and `m_AimPos` (a point), where

```
origin = cameraRenderPos + handOffsetFromHeadInGameUnits
pos    = origin + dir * distUU
```

`[Aim] DriveDistanceUU=0` means "follow the beam", which resolves to the crosshair
lever's 8 m. That is the setting that agrees with what the player SEES, and it was
chosen for that reason.

**It is also the worst case if the engine derives the direction itself.** If the
fire path computes `normalize(m_AimPos - muzzleLocation)` rather than reading
`m_AimDir`, then any offset between our `origin` and the engine's muzzle becomes an
angular error that scales as `offset / distance`:

| Aim point distance | Error from a 0.4 m origin mismatch |
|---|---|
| 8 m (`DriveDistanceUU=0`) | 2.9 deg |
| 30 m | 0.8 deg |
| 100 m | 0.23 deg |

So a far aim point makes the result **insensitive to which origin the engine
uses**, and that is the single cheapest way to find out whether the origin matters
at all. This is the first thing to sweep, before anything is rewritten.

### Assumption B: the write is fresh enough on the tick that fires

The drive deliberately does not write `m_TickTag`, on the reasoning that inventing
a freshness stamp could read as stale to code that has not been read, and that
leaving it alone lets the write ride whatever the game already considers current.
That reasoning is sound but untested: if the consumer compares the tag against the
current tick and our write lands before the game's own refill, the shot uses the
game's value and ours is never seen. The drive runs on every script dispatch, so
the window is small, but "small" is not "measured".

### Assumption C: there is no left/right mirror

`AsHandDirGame` checks that the angle off the head in XR equals the angle off the
view in the game, and the code says plainly what that check cannot catch: **a
left/right mirror has the same angle and passes.** A mirrored ray sends the shot to
the wrong side of the view while every number in the log agrees with itself. This
has never been tested, and it is the failure mode most likely to be mistaken for
the aim assist pulling the shot across the crosshair.

## 3. The plan

### Phase 1: read the bolt, so nothing after this is a judgement call

No behaviour change. Resolve the spawned projectile by NAME through the existing
property resolver and log, once per shot:

* the bolt's spawn location and its direction (`DisProjectile extends Actor`, so
  `Location`, `Rotation` and `Velocity` are there; `DisProjectile` adds
  `m_InitialLocation` and `m_bJustFired`, which marks the spawn tick),
* the ray we wrote on the tick it fired, and the aim point we wrote,
* **the angle between the bolt's actual direction and our written direction**, in
  degrees, and the distance between the bolt's spawn point and our `origin`.

That last pair is the whole measurement. It answers, per shot and without the
tester judging anything:

* angle near 0 -> the write is honoured and the bolt is on our ray. Done.
* angle nonzero but the bolt's spawn point is far from our `origin` -> Assumption A
  is the fault, and the number says how much of it.
* angle mirrored in sign about the view axis -> Assumption C.
* our written direction not present in the cache at fire time -> Assumption B.

The instrument must be able to print the unwelcome answer, so it logs the shot
even when the angle is zero, and it names the population: shots seen, shots where
the context was ours, shots where the write was live. A shot counter that only
increments when something is wrong cannot tell us the write works.

**This phase is where the risk is.** Walking to a just-spawned actor means a
pointer into an object the engine owns and may destroy. The rules this repo already
paid for apply: read `ActorComponent.Owner` style ownership rather than trusting a
name, never hold the pointer across a tick, drop it the instant gameplay ends
(the 2026-09-11 crash seven seconds after a pause menu is the precedent), and
byte-verify before every read. If a safe route to the spawned actor cannot be
found, fall back to hooking the fire seam's own dispatch and logging the cache as
it stood on that tick - less informative, but it needs no new pointer.

### Phase 2: sweep the aim point's distance

One variable, three values, `DriveFromHand=1` throughout:
`DriveDistanceUU` = 0 (the beam, 8 m), 3000 (30 m), 10000 (100 m).

Phase 1's angle is the readout, so this does not need a tester verdict - but it
should have one anyway, because "the bolt hits the dot" is the actual goal and a
number can agree while the experience does not.

Prediction, written before the run: if the engine reads `m_AimDir`, all three read
the same angle and the distance is irrelevant. If it derives from `m_AimPos`, the
angle shrinks as 1/distance in the ratio in the table above. Those two outcomes are
distinguishable on the first sweep, and a third outcome - the angle changing in
some other pattern - falsifies both.

### Phase 3: the mirror check

Hold the controller clearly to the LEFT of the view axis and fire. The bolt must go
left. This is one launch, one question, and it closes the one hole the mapping's
own guard is documented as unable to see. Worth doing even if Phase 2 comes back
clean, because a mirror plus an assist pull can look like an assist pull alone.

### Phase 4: keep both dots as the shipped reference

Requested directly: a permanent reference for where the head points and where the
controller points. The control dot stops being a diagnostic and becomes a feature.

* big dot = head, small dot = controller. Already true.
* both default OFF in the tree with a live A/B, per the project rule; the installed
  ini arms them.
* the head dot is drawn from the located views inside the runtime, which is why it
  is exact; that stays.
* rename the ini key from `ControlDot` to something that reads as a feature rather
  than an experiment, and document both dots in the user-facing docs rather than
  only in the investigation notes. `KNOWN_ISSUES.md` should say the game's own
  crosshair lags the head by design (it is painted into an image submitted with a
  pose two generations old and a flat HUD element cannot be reprojected), so the
  head dot is the accurate one.

## 4. Deliberately not in this plan

* **The aim assist's pull toward the game's own solution.** Shots are pulled back
  toward the crosshair from either side; that is `m_fMaxAimAssistAngle` and the
  choice weights doing their job. The per-weapon switches exist and are reachable
  (`DisTweaks_ProjectileAttack` carries `m_bUseAimAssist`, `m_bUseAimAssist_Mouse`,
  `m_bUseAimAssist_Gamepad`; the mod serves the controllers as a gamepad, so the
  gamepad path is the live one; `m_AimAssistChoiceInfo` carries the max angle and
  the weights; the context reads auto-aim strength from the profile settings; and
  `PlayerToggleAimAssist` is a native cheat that names the system). **None of it is
  touched here.** It gets its own ticket after the bolt is on the ray, because
  turning the assist off while the ray is unverified would remove the only
  reference the bolt currently has.
* **The weapon model's rotation.** Separate from the ray, tuned by the numpad
  adjust, and now known to be the source of the "45 deg" report. Its own work.
* **Blink and powers.** They share the legacy grip-pose path. Out of scope.

## 5. Questions for review

1. Is Phase 1 worth the pointer risk, or should it go straight to the fire-seam
   hook that needs no new pointer? The first gives the bolt's real direction; the
   second only proves what the cache held.
2. Phase 2 assumes the engine either reads `m_AimDir` or derives from `m_AimPos`.
   Is there a third shape worth designing the sweep to separate - for instance a
   blend between the assist's answer and the player's over `m_fAimAssistBlendTime`
   (0.1 s by default), which would make the angle depend on how long the aim has
   been held rather than on distance?
3. Should `m_TickTag` get a lever (write it / leave it) in Phase 2 rather than
   waiting for Phase 1 to implicate it? It is one line and one A/B.
4. The drive writes `m_ProjectedAimPos` so the crosshair follows. With the head dot
   now shipping as the reference, is driving the game's crosshair still wanted, or
   should the crosshair stay the game's own and the dots carry the VR aim?
