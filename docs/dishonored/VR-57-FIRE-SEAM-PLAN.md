# Plan: find the rotation the shot is fired from (VR-57)

2026-09-12, for review before implementation. Follows
[VR-57-BOLT-PLAN.md](VR-57-BOLT-PLAN.md), whose Phase 1 is complete and whose
Phase 2 this selects from. Scope: name the field the fire direction comes from,
then aim the bolt from the controller without moving the view. The aim assist and
the weapon model stay out.

---

## 1. What Phase 1 established

The bolt probe works and its answers are measured, not inferred. Populations
matched the tester's own count on three consecutive runs (5/5, 5/5, 6/6).

| Finding | Evidence |
|---|---|
| The camera position arrives NEGATED in c5 | controller origin was the exact mirror of the launch point through the world origin on 5 of 5 shots, while the engine's own `camZ` read positive. Fixed; the gap is now 0.5 to 0.8 m. |
| **The aim cache is the HUD's input, not the shot's** | with the drive ON the game's crosshair follows the controller dot (tester watched it), while the bolt leaves along the head. At ~20 deg off-gaze the bolt came out 21 to 23 deg from our written direction and missed the dot by 3.0 to 3.3 m; along the gaze the same shots landed 0.06 to 0.10 m out. |
| The acceptance metric is sound | 0.06 m when the ray and the shot agree, refused (not faked) when the bolt sits near-perpendicular to the ray. |
| The write counter was never evidence | 485,083 unrefused writes sat beside a bolt that ignored every one. **Withdrawn as evidence about the firing consumer**, as the review said it should be. |

So `m_CachedAimAssistPos` feeds the reticle and `m_ProjectedAimPos` is honoured,
while `m_AimDir` and `m_AimPos` are not read by the fire path at all. Writing that
cache can never aim the bolt.

Unexplained and recorded: the crosshair drops below the dot once or twice a second
and climbs back. The shape fits the game refilling the cache with its own
head-derived answer between our writes, but the cadence has not been measured and
no mechanism is claimed.

## 2. The question this plan answers

**Which rotation does the fire direction come from?** The probe measures the
bolt's launch direction `b` directly, so the test is a comparison, not a search.

Three candidates, and all three are already reachable - nothing needs deriving:

| Candidate | Where it lives | Already in the mod? |
|---|---|---|
| The camera's POV rotation | `kCamRotBase` = `{0x9c, 0xd0}` on the camera object | YES, the mod WRITES it from the head; `g_viewYawRad` is published from that same write |
| The PlayerController's `Actor.Rotation` | resolved by name | YES, `FindPropOffset("Actor","Rotation")` is already used (`g_yawRotOff`, `g_afActorRotOff`) |
| The Pawn's `Actor.Rotation` (the body) | same property on the pawn | YES, same resolver; VR-30's body-yaw hold already touches it |

This distinction is the whole point. In UE3 a weapon's fire direction usually
comes from the controller's rotation by way of the base aim rotation, **not** from
the camera's POV. The mod drives the view by writing the CAMERA's POV. If those
are different fields here, then the shot can be aimed from the controller without
the view moving at all - which is exactly what VR needs, and it is an engine-side
write, so attachments follow for free rather than being patched afterwards.

There is already evidence the candidates are NOT interchangeable, and it cost a
session: `patterns.h` records that `0x9c` on a PlayerController is a float, not a
rotator, that it was copied from the camera's entry, and that reading it pinned the
pawn's yaw to a constant so the arms froze and the stick could not turn. The
retired `kPcRotBase` is still in the file as the warning.

### What the tester's own reports already constrain

The bolt goes to the HEAD crosshair. The tester also reports the body facing 20 to
45 deg left of the headset. So if the shot were taken from the PAWN's rotation the
bolt would land 20 to 45 deg left of the head crosshair, and it does not.

That makes a prediction worth writing down before the run: **the shot is NOT
pawn-rotation-derived.** It should agree with either the camera POV or the
controller's rotation, and if the controller's rotation tracks the head rather than
the body, those two are indistinguishable on this evidence and the run must
separate them by value, not by eye.

## 3. Phase A: name the source, read-only

One candidate, no writes, `ShotProbe=1`, `DriveFromHand=0` (the cache write is
now known not to reach the bolt, so leaving it on only adds a moving part).

Resolve `Actor.Rotation` on the player controller and on the pawn by NAME, read
the camera POV rotator from the fields the mod already writes, and add to every
shot record the angle between the measured bolt direction `b` and the forward of
each. Also log the three rotators themselves, as yaw and pitch in degrees, and
their pairwise differences, so "two candidates agree" is visible rather than
inferred from two similar angles.

Acceptance for this phase is not a small number, it is a DISTINGUISHING one: the
run must show at least one candidate near zero and say whether any two of them are
within noise of each other. If all three agree to within a degree the phase has
failed to separate them and the next step is to make them disagree deliberately -
turn the body away from the head with the stick and fire again, which is free and
which the tester can do in the same session.

Guards, because two of these fields have already caused a freeze when misread:
verify each resolved offset is a rotator by range (three int32 whose values behave
as `kUEPerRad` angles) before trusting it, log the raw values on first read, and
refuse rather than guess when a resolve fails. Nothing is written in this phase, so
a wrong offset costs a bad log line and not a pinned pawn.

## 4. Phase B: aim from the controller, branched on Phase A

Only one of these gets built, and which one is decided by Phase A's numbers.

### B1, if the shot reads a rotation the VIEW does not use

Write that field from the controller ray. Engine-side, so the muzzle, the
attachments and any trace the engine does all follow one value, which is the rule
this project already paid for. Ships default OFF with a live A/B, and the probe's
miss at the dot is the acceptance number.

Risks to design for rather than discover: the field is shared with body facing and
`FaceRotation`, so a write must be scoped to what the shot reads and must not pin
the pawn's yaw (the recorded freeze). The body-yaw hold and the arm-follow code
already read `Actor.Rotation`; a write has to be reconciled with both, not layered
on top. And a rotation written every tick is not the same as one written for the
tick that fires - the cheaper and safer shape may be the latter, which needs a fire
event to hang on.

### B2, if the shot reads the same POV rotation as the view

Then there is no field to write without moving the picture, and the options are
worse. A transient swap around the fire tick needs a verified fire seam, a
guaranteed restore on every path including an early return, and proof the view did
not flick. Before building that, re-check whether the engine offers a separate aim
rotation at all (`GetAdjustedAimFor`, `GetBaseAimRotation` and the weapon's own
fire are the names to resolve), because a one-tick rotation swap on the field the
camera uses is the most dangerous change proposed in this whole investigation and
should be the last resort, not the first attempt.

### Not the route, with reasons

* **Post-spawn steering of the projectile's velocity.** In the graveyard, and the
  probe now explains why it looked arbitrary: the engine fills velocity AFTER
  announcing the projectile, so a write at first sight is overwritten and a later
  write fights whatever else is integrating. It also cannot fix the start point.
* **Writing the aim cache harder, or writing `m_TickTag`.** The cache is the HUD's.
  No cadence or freshness change to a field the fire path does not read can move
  the bolt, and the review's refusal of a blind tag A/B stands.

## 5. What "done" means here

Unchanged from the bolt plan: the miss at the plane of the visible dot, inside a
tolerance fixed in advance, with the populations and the origin audit clean, the
tester confirming the bolt goes where the small dot is, and no new flicker or
pause/load regression. The assist may still pull shots; that is reported as a
remaining failure and not folded into this result.

Tolerance proposed now, before any number is seen: the launch line should pass
within **0.25 m** of the dot at 8 m, which is about 1.8 deg and is inside the
transverse muzzle/controller gap already measured (0.27 to 0.62 m) - so meeting it
implies the shot converges on the dot rather than merely running parallel to the
ray. A direction-only fix cannot meet it, which is deliberate.

## 6. Questions for review

1. Is Phase A's three-way comparison the right separation, or should the pawn
   candidate be dropped on the tester's own evidence and the run spend its
   attention on distinguishing the camera POV from the controller rotation by
   deliberately turning the body away from the head?
2. For B1, is a write scoped to the firing tick preferable to a continuous one,
   given the body-yaw hold and arm-follow already read `Actor.Rotation`? A
   per-tick write is simpler to reason about but collides with more code.
3. Is the 0.25 m tolerance at 8 m the right bar, and should it be stated as an
   angle instead so it does not quietly change when the dot distance does?
4. The crosshair dip: worth its own measurement now (it is the only direct
   evidence of the refill cadence, which B1 may also have to race), or left until
   the bolt is fixed?
