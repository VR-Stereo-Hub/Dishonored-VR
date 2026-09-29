# The contact-timed sword: research and a next-session brief (VR-173)

Status: **RESEARCH, in progress since 2026-09-29.** Written 2026-09-21 alongside VR-170.
Sections 1 to 5 are the plan as written and are left as they were. Section 6 holds what
turned out to be different by the time the work started, section 7 the verdicts, one per
prerequisite, in the order they were measured. Prerequisites 1 to 4 have their verdicts
and 7.5 is the go or no-go: GO, as a default-off detector.

## 1. What the player would get

Today a fast swing of the right hand presses the game's attack wherever the hand is
(`PHYSICAL_SWING.md` section 1: the swing decides WHEN, the game decides WHERE). A
swing in empty air therefore plays the whole canned attack, swoosh and all.

The idea: the mod runs the engine's own trace along the PHYSICAL blade, and presses
the attack at the moment a fast blade actually reaches an enemy or a breakable. The
game still owns damage, parry, combos, stealth kills and every AI reaction, because
the thing delivered is still the attack input. An air swing does nothing. Two things
follow for free: the speed threshold can come down a long way (an accidental swing in
the air costs nothing), and the sword trail stops playing in empty air.

Two things it is explicitly NOT: mod-side damage, and suppression of the game's
attack animation. Parry, the combo counter and the stealth kill all hang off the
game's attack state, and the kill has no input of its own (it is the attack in
context, `PHYSICAL_SWING.md` section 7).

## 2. What the sibling BioShock mod's "servo" actually is

It was the prior art asked about, and it is a different thing. It is a **proportional
controller on the right stick's Y axis** that steers the ENGINE's own view pitch
toward the headset's pitch (`bioshock-1-vr-mod`: `src/core/input/xinput_bridge.cpp`
`pitch_servo_stick`, fed by `publish_pitch_error` from `src/game/bioshock1r/camera.cpp`
just before the camera write overwrites the pitch).

Why it existed: that mod zeroed stick pitch (the head owns pitch) and wrote the
rendered pitch absolutely from the head, discarding the engine's value. The engine's
internal pitch therefore froze wherever it last was - measured at -88.9 degrees,
straight down - and the wrench's hit volume, which the engine aims from its own view,
hit the floor. Nothing on screen showed it, because the rendered view was the head's
either way. The servo walks the engine's belief back to the head through the game's
own input: no engine memory written, the game's own pitch clamps inherited, invisible,
and it fails open to the plain zero when its publisher goes stale. Deadzone 1.5
degrees, gain 900 stick units per degree, ceiling 8000 (about 24 % deflection) so a
wrong sign saturates somewhere recoverable. Residual 4 to 8 degrees, because near
convergence the stick value falls under the game's own deadzone.

Its swing gesture is the one Dishonored's `edge` detector was copied from, and it has
the same contract: WHEN, never WHERE. **There is no blade-contact hit test in that
mod.** Its own header says melee reaches neither of its fire-start seams.

Does Dishonored have the frozen-pitch fault? Very likely not: `ApplyHeadToViewRotation`
writes pitch absolutely into the PlayerController's OWN `ProcessViewRotation`
out-rotator (`head_track.cpp`, the fresh branch), which the controller keeps, and the
2026-09-20 headset run scored 46 of 47 swings honoured with no hit landing low. It is
still prerequisite 1 below, because the whole lesson of that story is that the fault
was invisible for months.

Three lessons that port directly:

1. **Measure which seam melee reaches before hooking anything.** That mod spent a
   session improving a fire-origin seam the wrench never called (0 melee calls at
   either seam across a whole wrench session).
2. **Test a suppression by setting it absurdly HIGH, not to zero.** A lock-on radius
   of 5000 looked identical to 0, which proved the whole lever was a no-op; a zero
   alone could never have shown that.
3. **Publish with an expiry and fail open.** A stale publisher must revert to the
   safe behaviour without a special case.

## 3. Prior art outside these two mods

Numbers, for calibrating expectations (sources in the VR-173 ticket):

| Project | Swing gate | Notes |
|---|---|---|
| Skyrim VR (native) | hand speed 2.0 (community raises it to about 4 because walking triggers it) | collision volume plus a velocity gate; no head-relative term, no median |
| PLANCK (Skyrim VR physics melee) | arms at 4.5 m/s, fires at the speed PEAK; contact classified at 5.0 slash / 2.0 stab with a direction dot of 0.8; hand must move 1.0 m/s in room space | damage from the PHYSICAL contact, using relative point velocity at the blade (tip speed, not hand speed); per hand, per target cooldown 0.25 s, 1.5 s fallback; swing cooldown 0.55 s; velocity smoothed over about 55 ms |
| Jedi Knight VR ports | 2.0 m/s, half that for the off hand and fists | the saber itself is collision-driven, not velocity-gated |
| Half-Life 2 VR crowbar | minimum speed AND minimum distance, a cooldown | damage scales with both; waggling discouraged by design |
| UEVR per-game plugins | gesture presses the button (per-tick position deltas) | frame-rate dependent as written: a pattern to avoid |

Three families: (a) gesture presses attack - what ships today; (b) sweep the blade and
apply damage on contact - most physical, needs a damage entry point and loses the
game's attack state; (c) keep the game's attack and time or aim it from the physical
blade. The contact-timed sword is (c).

## 4. Prerequisites, in order. Each is a measurement with a written verdict here.

1. **Engine pitch against head pitch during an attack.** One log line in the fresh
   branch of `ApplyHeadToViewRotation`: the incoming `rot[0]` before the write against
   the head's pitch, sampled through a sword attack. Tracks within a degree: the
   BioShock fault is absent and section 2's servo has no work here. Diverges: port the
   servo FIRST, it is 40 lines and everything after it aims from that pitch.
2. **The blade's axis and tip.** Nothing measures the sword today: the detector uses
   the hand POSITION only. The VR-57 machinery (`hands/bolt_axis.h`,
   `hands/bolt_model_ray.cpp`) derives a drawn mesh's principal axis and tip from its
   vertex buffer, converts it to a palm-frame constant, persists it and keys it on the
   engine's equipped item. It has only ever run on crossbow bolts. Read
   `VR-57-MODEL-RAY.md` first: one asset name was two meshes with different tips, and
   an origin bound of 2 m per axis once accepted a tip 3.4 m from the palm.
3. **A bridge from XR-local metres to world units.** `aim_ray.h` says it plainly: the
   game-space conversion does not exist yet. Pieces: `dvr::hands::TrimSnapshot`
   (grip pose, calibration, `handToWorldScale`), `dvr::camera::render_pos_world`, the
   published view yaw and pitch, `shared/ue_math.cpp`. Acceptance is a known answer:
   the blade tip's world position must sit where the drawn blade is, judged by placing
   a debug marker at it and looking.
4. **The engine's own trace, called from the mod.** ENGINE_NOTES (the interaction
   section) records that the native registration table has exec thunks for
   `AActor::Trace`, `FastTrace` and `TraceActors`, and recommends exactly this: run the
   engine's own trace along a different ray, so the mod agrees with the game's
   collision rules by construction. Route: the outbound ProcessEvent path
   (`PFN_ProcessEventCall`, `g_peReentry`), **script lane only** - ProcessEvent from the
   present thread is the lane error this project has a rule about. Self-test before
   trusting it: trace straight down from the pawn and require the floor at about the
   capsule's half height; trace along the view and compare with what the crosshair
   reports.
5. **Only then the detector mode.** `[Melee] Detector=contact`, default OFF, live A/B
   against `edge` (`swing mode`). The blade segment is traced each hand generation
   (the sample identity rule, TRAPS); tip speed is `|v_hand + w x r_tip|`, not hand
   speed; the attack is pressed when tip speed is over a (much lower) threshold AND
   the trace reports a pawn or a breakable. Reuse everything after the decision: the
   gates, the pulse, the honoured-check, the VR-170 hump census (which will then say
   what contact speeds look like before any threshold is chosen).

Open design questions to settle by measurement, not in advance: the lane hop (the
decision is on the present lane, the trace on the script lane, so a contact arrives a
tick late - measure that latency against the honoured-check's 15 to 31 ms); what
counts as a target (pawns, breakables, ropes and planks the sword can cut); whether
an air swing should still attack above some high speed, so a player can swing at
nothing on purpose; and what the engine's melee assist does when the attack is
pressed with the target already at blade range.

## 5. The brief to paste into the next session

> Work on VR-173, the contact-timed sword, in the Dishonored VR mod. Read
> `docs/dishonored/PLAN-contact-sword.md` first, then `PHYSICAL_SWING.md`,
> `VR-57-MODEL-RAY.md` and the interaction section of `ENGINE_NOTES.md`. This is a
> Research ticket: the deliverable is a measurement and a written verdict per
> prerequisite in section 4 of the plan, in order, each committed with its finding
> before the next begins. Do not write the detector mode until prerequisites 1 to 4
> each have a verdict. Start with prerequisite 1 (engine pitch against head pitch
> during a sword attack, one log line, simulator run with `swing sim 5 200 3`), because
> it decides whether the BioShock pitch servo is needed at all. Everything on the
> script lane that calls the engine goes through the existing outbound ProcessEvent
> path; nothing calls the engine from the present thread. Every new lever ships
> default OFF with a live toggle. Validate on the simulator (`docs/VERIFICATION.md`,
> and the TRAPS entry on the simulator's display clock leaping after a hitch: use
> `swing sim` for anything with no speed margin). End with a go or no-go and the
> feature ticket it implies.

## 6. What was different when the work started (2026-09-29)

Read from the code on `staging` at `5997c5952`, before anything was measured. Each line
corrects a statement in sections 2 to 4 and says where the fact lives.

| Section says | What the code says |
|---|---|
| 4.3: the bridge from XR-local metres to world units does not exist | It exists for RAYS: `HandRayWorld` (`interact_aim.cpp`) over `dvr::fireaim::solve` (`fire_aim_math.h`), anchored on the game camera, used by interaction, throws, powers, Blink and the crossbow, headset-confirmed (VR-166, VR-181). The comment in `aim_ray.h` is stale. What is missing is the same transform for a POINT, and for the RIGHT hand: the published aim ray is the left hand's (`aim_ray.cpp` forces hand 0) |
| 4.3: pieces include `handToWorldScale` | Confirmed, and it matters: the hands and the held weapon are drawn at hand travel `[Hands] WorldScaleUU` x `PaletteDriveGain` = 100 uu per metre, the camera at `[PosTrack] Scale` = 108. A blade point goes through the same two steps the aim ray's origin does, or it lands 8 % off the drawn blade |
| 4.2: the VR-57 machinery is keyed on the equipped item | Its comments say so; the code latches ONE bolt axis for the session and shares it across every weapon. The equipped-item field in it is unused |
| 4.2: run the sword through that machinery | Three of its gates refuse the sword before any axis is fitted: 2481 vertices against a cap of 1024, more than one bone against a rigid single-bone rule, and a candidate test that accepts the regular bolt only. `tools/bolt-axis-tests.cpp` asserts the sword stays refused as an AIM-RAY candidate, and it must. The blade therefore gets its own measurement and its own publication slot |
| 4.5 and the open questions: the honoured-check's 15 to 31 ms | That is the measured LATENCY of an honoured attack (simulator 15 to 16 ms; headset 15 to 109 ms, median 31). The window in code is `[Melee] HonourMs`, 600 ms |
| 4.4: `g_peReentry` guards the outbound call | It guards two callers only. A script-lane module that calls the engine needs its own `inside` guard and a once-per-frame gate (TRAPS, VR-182: build 668 overflowed the stack without one). `FxFollowTick` is the pattern |
| 4.4: find the function by name | `Actor.Trace` is ambiguous by name: a name match on the outer collides with other classes' `Trace`. The function object is found once by class and name, and its parameters by their outer POINTER |

Sword work merged after the plan was written, and what it means for prerequisite 5:

* **VR-220, the attack source.** An attack is classified as a physical swing when the
  game enters the attack while the pulse is open or within 80 ms of its close. The rule
  is relative to the PULSE, so a contact press classifies exactly as an edge press does.
* **VR-203 and VR-111, the drop takedown.** An airborne attack press is held up to
  420 ms until the game reports a target under the player. A drop kill needs its press
  BEFORE the landing; a blade cannot reach the target in time. In the air the game's
  own drop decision has to stand in for blade contact.
* **The motion sword's gate for a body the game owns** reads the camera-action
  classifier, not the hand-back as a whole. Unchanged by this work.
* **The stab and the plunge** run only under `edge` (`melee.cpp`, `stab_armed` is asked
  only when the detector is `edge`). A third detector has to enable them on purpose.

One design risk the plan did not name, recorded here so the go or no-go weighs it. `edge`
presses attack at the START of a swing, so the game's wind-up runs while the arm is still
travelling. Contact presses when the blade ARRIVES, so the wind-up runs after it and the
hit lands once the arm has passed. The press-to-hit latency has to be measured before a
threshold is chosen.

## 7. Verdicts

### 7.1 Prerequisite 1, engine pitch against head pitch: THE FAULT IS ABSENT (2026-09-29)

**Verdict: the engine's view pitch follows the head through a sword attack, to the unit.
The sibling mod's pitch servo has no work here and is not ported.**

Build `v1.0.1-158-g5997c5952`, RelWithDebInfo, legacy off, the simulator (`dvr-xrsim`,
2064x2208 per eye, `stereo reentry`, 90 pairs a second), RTX 4060, the dev PC's newest
save. No new code ran: the instrument is VR-172's `camshake capture`, which records one
row per game tick from the fresh branch of the head write, before the write.
`tools/xrsim/pitch-probe.xrs` is the sequence, `tools/pitch-probe-read.py` reads the rows.

| Leg | Head pitch | Game ticks (in an attack) | Engine handed back minus last written | Controller's own rotation minus last written | Camera minus last written |
|---|---|---|---|---|---|
| level | 0 | 450 (141) | 0.000 | 0.000 | 0.000 |
| up | +25.0 | 450 (143) | 0.000 | 0.000 | 0.000 |
| down | -25.0 | 450 (75) | 0.000 | 0.000 | p90 0.319, max 0.483 |
| swept -25 to +25 in 1.2 s | moving | 450 (74) | 0.000 | 0.000 | p90 0.308, max 0.472 |
| down, the game's own camera motion allowed | -25.0 | 450 (74) | 0.000 | 0.000 | p90 0.313, max 0.489 |

Degrees, the attack rows' worst value unless marked. Every leg drove three attacks with
`swing sim 5 200 3`, and every one fired three times.

What each column can and cannot say:

* **The level leg is not evidence.** A pitch frozen at zero agrees with a level head. It
  is the reference only, and the reader says so on its own line.
* **The controller column is the one that settles it.** It is read from the player
  controller's own `Actor.Rotation`, engine memory, not from the event's parameters. In
  the sibling mod that value sat at -88.9 degrees while the picture followed the head.
  Here it reads the head's +25 and -25 and follows the sweep tick by tick.
* **The camera column moves only in attack rows**, by under half a degree: that is the
  attack animation's own camera motion, and it is what shows the rows are live readings.
* The mod zeroes the right stick's pitch axis in gameplay (`pad_bridge.cpp`, "pitch
  belongs to the head"), which is the same arrangement the sibling mod had. The
  difference is where the head's pitch is written: into the rotator the engine keeps.

**The control leg, and what it does and does not prove.** It is a pistol shot with the
game's kick allowed. On the first run it could not happen: the newest save holds no
pistol in the left hand, the leg read flat, and a flat control is indistinguishable from
a blind instrument. The whole sequence was run again on the sewer save (2026-09-29, same
build), where the pistol is: legs 1 to 5 read 0.000 in all three columns on 2180 game
ticks, 703 of them in an attack, and the shot put **2.928 degrees** into the camera
against the controller. So the capture sees pitch the game adds, where the game adds it.
What the shot did NOT do is move the controller's rotation or the value handed back:
the kick is a camera influence and lives in the camera alone. The two columns that carry
the verdict were therefore never seen to diverge by a control, and their evidence is the
second bullet above: they are raw engine values, they read the head's pitch at +25 and
-25, and they follow a sweep tick by tick.

On the first save the camera moved up to 0.49 degrees inside attack rows and on the sewer
save not at all. The player was crouched on the first and standing on the second; which
of the two differences owns it was not measured.

**The headset agrees.** The logs of the 2026-09-25 headset session (build
`v1.0.1-9-g1e948fac4`) carry the 3 s `headtrack:` heartbeat, which prints the incoming
pitch beside the written one: 1012 samples, 95 swings. Incoming against written, median
0.06 degrees, p90 0.39; within 1.5 s of a swing, 66 samples, median 0.11, max 1.65. 18
samples were over 5 degrees and NONE was near a swing: 14 follow a menu, a load, a
cutscene, a keyhole or the power wheel by under 3 s, and the other 4 sit on the tick the
master state returns to walking. They are the first write after the game owned the view,
not an engine that stopped following.

Not measured: a takedown or a drop kill (the game owns the camera there, and the mod
stands down), and a real opponent (the simulator's attacks hit nothing).

### 7.2 Prerequisite 2, the blade's axis and tip: MEASURED, and it sits on the drawn blade (2026-09-29)

**Verdict: the held sword's blade is known in the palm frame, from its own drawn mesh,
and the measured base and tip lie on the drawn blade in both eyes at three poses. While
the arm is tracked it is a constant of the hand. While the game plays an attack clip on
the hand it is not, and the detector must not use it then.**

Build `v1.0.1-160-g723bb2d91` plus this commit's code, RelWithDebInfo, legacy off, the
simulator, the sewer save. `[Blade] Measure` and `[Blade] Marker`, both default 0, live
through the `blade` word. Code: `blade_math.h` (pure, `tools\blade-host.ps1`, 120
checks), `hands/blade_axis.cpp` (the reader, beside the bolt's).

**The route.** Not the bolt reader: section 6 says why. A separate measurement on the
same draw, which keeps every vertex's four bone weights and skins the mesh through the
palette OF THE DRAW BEING MEASURED, so the sword is read as it is drawn. The long axis
comes from an even subsample of 827 vertices, the two ENDS from all 2481, because a tip
is one vertex and a subsample can step over it (the host test puts the tip at such an
index). The tip is the end farther from the palm; the base is the point of the blade
line nearest the palm, held inside the sword, so a trace never starts at the pommel
behind the hand.

**What the sword is**, as the draw had it at rest:

| | |
|---|---|
| asset | `Wpn_PlySword01`, 2481 vertices, 2074 triangles, one draw |
| bones | 12. Two carry the grip and pommel (876 and 287 vertices), six the folding mechanism within 8 uu of the guard, three a ring of the blade two thirds along, one the blade's last 18 uu (84). ENGINE_NOTES has the line per bone |
| long axis | 84.1 uu pommel to tip in the mesh's own units, variance ratio 25.2 to 1 |
| in the hand | base (0.003, 0.003, -0.003), tip (-0.275, -0.233, -0.504) m, palm frame |
| blade ahead of the palm | 0.620 m of hand travel, which is 62.0 uu in the world and 0.574 m as the headset shows it (hand travel 100 uu per metre, camera 108) |
| blade line to the palm | 0.005 m: the line through the blade passes through the hand that holds it |
| model scale | 0.85 (`[Hands] ModelScale`), part of the latch's key |

It latched on the first run, after 55 agreeing draws over 312 ms with the body at rest.
The 16 to 1 the bolt reader demands would have passed this mesh (25.2), and refuses the
host test's synthetic sword (10.7): the ratio is logged and is not what decides.

**Acceptance: `tools\xrsim\blade-marker.xrs`, read by `tools\blade-marker-check.py`.**
The tolerance was written before the run: 3.0 cm along the blade, 2.0 cm across, at the
tip's own range. The check projects the marker into each eye's pixels from the capture's
JSON, confirms the projection against the image (the dot must land where it was
predicted), and compares the measured tip with where the drawn blade's own silhouette
ends (sword drawn minus sword sheathed).

| Pose | Eye | Drawn blade's end from the measured tip | Dot from its predicted pixel | The same pose, marker moved 10 cm on purpose |
|---|---|---|---|---|
| A, blade across the view, head level | left | -1.4 cm along, +0.1 across | 0.0 px | 8.7 cm across: FAIL, as it must |
| | right | -1.6, +0.0 | 1.4 px | 8.7: FAIL |
| B, blade low, head 20 down | left | -0.8, +0.3 | 0.9 px | 10.3: FAIL |
| | right | +0.1, -0.1 | 0.1 px | 8.9: FAIL |
| C, head turned 25, up 10, ROLLED 12 | left | -0.9, +0.1 | 0.2 px | 4.4: FAIL |
| | right | -0.4, +0.2 | 0.1 px | 4.3: FAIL |

One pixel is 0.13 to 0.18 cm at the tip in these poses. Pose C's offset reads 4.4 cm
and not 10 because most of that offset lay along the line of sight there; it is still
twice the tolerance.

**The check itself was wrong twice before it was right, and both are kept in the tool's
comments.** Its first version took the furthest difference in a corridor for the blade's
end and reported the blade 33 cm too long: something had moved in the scene between the
two captures. Its second followed the run outward with no floor and walked 16 cm past the
tip on image noise in one eye. Its third measured the floor against the run's last few
bins and ended 19 cm short where a guard walked behind the blade. In all three the
overlay picture showed the measured tip on the drawn tip. The measurement did not change
between versions; the instrument did.

**Constant, or live?** Both are published. Measured on the same run:

| While | Latched constant against the live tip | Samples |
|---|---|---|
| the body is at rest | 0.0000 m | 799 |
| three physical swings (`swing sim`, the arm stays on the tracked hand) | 0.03 degrees, 0.0000 m | 278 |
| two trigger attacks (the game's clip plays on the hand) | 178.9 degrees, 1.449 m | 229 |
| the hand-back's release after them, upper state already idle | 0.959 m | 909 |

So for the attacks the contact sword is about, the physical ones, the blade is rigid in
the palm and the constant is the blade. During a trigger attack the game owns the hand
and the sword goes with it; the palm-frame constant then describes a sword that is not
there. That gives prerequisite 5 a gate it can measure instead of assume: **trace only
while the live tip agrees with the constant**, and the animation state's word for "at
rest" is not enough (the release reads idle for 0.96 m of disagreement).

**The fallback route (sockets and bones on the script lane) was not built.** It existed
in the plan in case the draw-time fit could not take a folding, multi-bone mesh. It can.

Not measured: the sword mid-fold (the latch waits for rest and a two-sided reach bound
refuses a short sword, both host-tested, neither seen in the game); any sword but this
one; and the headset, where the marker is judged by eye.

### 7.3 Prerequisite 3, XR-local metres to world units: THE BRIDGE HOLDS, to a quarter of a centimetre (2026-09-29)

**Verdict: a point of the held blade carried from the headset's space into the game's
world lands where the renderer drew it, within 0.27 uu (2.7 mm) at seven poses. The
bridge is the one the hand rays already use; nothing new was invented.**

Build `v1.0.1-162-g64540b6a1` plus this commit's code, RelWithDebInfo, legacy off, the
simulator, the sewer save. `[Blade] World`, default 0, live through `blade world`.

**What was built.** `dvr::fireaim::point_to_world` (`fire_aim_math.h`), factored OUT of
the ray solver, which now calls it: a blade point takes exactly the arithmetic every
hand-aimed ray's origin takes. The host test pins the two bit for bit at 60 head poses,
rolled ones included, and pins the way back (`point_to_xr`). The present lane publishes
the blade as the headset shows it once a present (`dvr::hands::blade_frame`); the script
lane carries it (`blade_contact.cpp`).

**The known answer.** Two routes to one world point, sharing the palm-frame blade and
nothing else:

* the XR route: grip pose, palm-frame blade, XR local metres, `point_to_world` with the
  game camera as the anchor and the view's yaw and pitch as the frame;
* the draw route: the same palm-frame blade carried the way the DRAW went (the hand
  draw's own palm target) and back across the coordinate bridge the weapon path
  identifies its draws with, whose anchor is the arm mesh's own component transform.
  No headset pose is in it.

The tolerance was written before the run: 3.0 uu mean, 5.0 uu worst.

| Pose | Left eye's draw, tip / base | Right eye's draw, tip / base | Samples |
|---|---|---|---|
| 1 head level, blade across the view | 0.25 / 0.25 | 0.25 / 0.25 | 197 |
| 2 head 20 down, blade low | 0.25 / 0.25 | 0.25 / 0.25 | 357 |
| 3 head turned 25, up 10, ROLLED 20 | 0.26 / 0.26 | 0.25 / 0.25 | 311 |
| 4 head turned -40, down 30, ROLLED -20 | 0.27 / 0.26 | 0.24 / 0.25 | 318 |
| 5 hand near, 0.35 m ahead | 0.25 / 0.25 | 0.25 / 0.25 | 361 |
| 6 hand far, 0.95 m ahead | 0.26 / 0.26 | 0.24 / 0.24 | 296 |
| 7 head MOVED 0.25 m right and 0.15 m back, turned and pitched | 0.25 / 0.27 | 0.25 / 0.24 | 294 |

Mean disagreement in uu (1 uu = 1 cm); the worst sample equals the mean to two places
in every row. The blade is 62.0 uu long in the world at every pose.

**Where the quarter centimetre comes from.** The two eyes' draws put the blade 0.5 uu
apart, in opposite directions from the XR route, which sits between them. The eyes are
6.8 uu apart at the camera's 108 uu per metre, the hand is placed at hand travel's 100,
and 6.8 x (1 - 100/108) is 0.50. It is a property of how the hands are drawn, it is
already in what the player sees, and it is a twelfth of the tolerance.

**The check can fail, and says by how much.** Same pose as row 2, one lever at a time,
each a deliberate error that is never saved:

| Leg | Tip, left / right | Predicted |
|---|---|---|
| as shipped | 0.25 / 0.25 uu | |
| the XR route's scale off by +5 % | 5.17 / 5.12 | 5.14 (the tip is 102.8 uu from the head) |
| off by -10 % | 10.26 / 10.31 | 10.28 |
| anchored on the last render sample | mean 28.4, worst 1491 | not a number: that sample is whichever scene draw uploaded last, a shadow pass included. It is why the hand rays left it (VR-181) |
| as shipped again | 0.25 / 0.25 | |

**What agreement does NOT prove.** Both routes start from one grip pose, one hand
calibration and one hand-travel scale, so a fault in any of those moves both and shows
in neither. That is prerequisite 2's acceptance (the marker against the drawn blade, in
the image) and not this one's. And the view's yaw and pitch are the last head WRITE
while the head pose is this present's: every pose above is a held pose, so the skew a
turning head adds between the two is not in these numbers. It is bounded by head rate
times one game tick, and belongs to the detector's latency measurement.

**World scale.** Changing `[PosTrack] Scale` cannot break the bridge: the hand-frame
point is scaled about the head by hand travel over the camera's scale and then carried
at the camera's scale, so the camera's scale cancels and the blade is carried at hand
travel, as it is drawn.

### 7.4 Prerequisite 4, the engine's own trace: IT ANSWERS, but not by the route the plan named (2026-09-29)

**Verdict: the mod can ask the engine what lies along the held blade, on the script lane,
read-only, for about 8 microseconds a question, and the answer names characters,
breakables and the world. The route is the world's line check, called the way the
script-callable `Actor.Trace` calls it. `Actor.Trace` itself cannot be reached through
ProcessEvent, and that was measured, not assumed.**

Build `v1.0.1-163-g835160123` plus this commit's code, RelWithDebInfo, legacy off, the
simulator, the sewer save. `[Blade] Trace`, default 0, live through `blade trace`.

**What the plan asked for, and what happened.** Section 4.4 routes the call through the
outbound ProcessEvent path. It was built exactly so: the function object named `Trace`,
declared on `Actor`, found once by class and name; its nine parameters by their outer
pointer (the block is 100 bytes and is stock: vectors at +0, +12, +24, +36, the bool at
+48, the extent at +52, a 28 byte hit record at +64, the flags at +92, the returned
actor at +96); a zeroed block, every optional set, a sentinel in the return value. The
self-test then FAILED four of four with `the call did not write its return value`, in
both stances. It could print the unwelcome answer, and did.

**Why.** Read from the game's own ProcessEvent (`tools/disasm-rva.py`): after the
function-flags test and the pending-kill test it compares the word at function +0x84
with zero and leaves when it is not. That word is the native index. `Trace` carries one:
the runtime read of the function object says **277**. The natives the mod has always
called this way (`SetHidden`, `TransformFromBoneSpace`, `GetProfileSettings`) carry
none. So every numbered native is closed to this path, `FastTrace` and `TraceActors`
with it.

**The route that works.** The thunk the native registration table gives for
`AActor::execTrace` reads its parameters and then calls one function, on the world
object, with seven stack arguments: a hit record, the source actor, the end, the start,
the flags, the extent and a light. That function returns with `ret 0x1C`, which is
seven. It is the world's single line check, the same one 196 call sites in the game use.
The mod calls it directly, on the script lane, with the flags the thunk composes for
"trace actors too" (0x20BF) and the player's pawn as the source actor, and reads the
same three fields the thunk reads back.

Before the first call, and refused on any mismatch:

* the line check begins with the 24 bytes it had when it was read;
* the thunk loads the world pointer and calls the line check at the two places the
  disassembly says, byte for byte;
* **the function object NAMED `Actor.Trace` holds the thunk's address** (found at +0x9C
  of it). The address is tied to a name at runtime, not only to an offline table.

Five addresses and two offsets, all in `patterns.h`, derivation in ENGINE_NOTES.

**Known answers** (`tools/xrsim/trace-selftest.xrs`, `blade trace selftest`):

| Test | Crouched (half height 65.0) | Standing (87.5) |
|---|---|---|
| floor under the feet: a hit, facing up, on the line | 76.0 uu, normal z 0.98, 0.00 off the line | 99.2 uu, 0.98, 0.00 |
| the same line stopping 10 uu short of that floor | no hit | no hit |
| upward inside the player's own capsule | no hit: the player is not a target | no hit |
| the same floor from 50 uu higher | 126.0 (want 126.0) | 149.2 (want 149.2) |
| the same floor from the other stance | z 2829.1 | z 2829.1 |

The first version of the floor test demanded the capsule's half height within 4 uu and
failed by 11 in both stances. The trace was right and the expectation was wrong: the
save stands on a ledge whose surface leans (normal z 0.98), the capsule rests on its
rim, and the floor under its centre is lower. What a slope cannot move is that the same
floor is 50 uu further from 50 uu higher, and at the same height in the world from
either stance while the pawn's own location moved 23.2 uu. Those are the tests now; the
half height is a band (never less, up to 25 more).

**The held blade, traced** (`blade trace on`, one trace per hand sample, base to tip):

| Hand | Blade | Answer |
|---|---|---|
| free air, five poses | 62.0 uu | nothing, every trace (230 to 361 per 3 s) |
| lowered, blade down, tip 2.3 uu above the floor | 62.0 | nothing |
| 10 cm lower | 62.0 | the world, 54.31 uu from the base |
| 10 cm lower again | 62.0 | the world, 44.10: 10.21 nearer, 10.40 predicted from the floor's own normal |
| and again | 62.0 | the world, 34.16: 9.94 nearer, 9.89 predicted |

The hit lies 0.00 uu off the blade's line every time.

**What it names** (`blade trace scan`, a one-shot fan of 1681 traces about the view, an
instrument only):

| Class | Taken for | Pawn ancestry |
|---|---|---|
| `DishonoredNPCPawn` | a character (guards, at 911 to 2089 uu) | yes |
| `StaticMeshCollectionActor` | the world | no |
| `DishonoredBreakableNavBlock` | another actor: a breakable | no |
| `DishonoredMovable` | another actor: something that can be carried | no |

That answers one open question as far as the simulator can: a character is reported as
a character, and breakables are told apart from the world by class. Doors, ropes and
planks the sword cuts have not been touched yet; the vocabulary line prints each new
class once.

**Cost and delay.**

| | |
|---|---|
| one trace | median 7 to 8 us, p95 10 to 11, worst 22 (256 samples each) |
| in bulk | 6.2 us each (1681 in 10.4 ms) |
| traces a second | the hand sample rate, about 85 |
| finding the function | one pass over 101,130 objects, 113 ms, ONCE per launch, at the first use |
| blade published (present lane) to answer held (script lane) | median 0.1 ms, p95 10.3 to 11.3, worst 13.9 |
| pairs a second, cap lifted, same pose, 40 s legs | levers off 107 and 108; measure, world and trace on 106 and 112. Each leg swings 67 to 132 by itself (the scene's own cycle) |

The lane hop is at most one game tick. The honoured-check sees the game start an attack
15 to 16 ms after a press on the simulator and 31 ms (median) in the headset, so the hop
adds up to two thirds of a simulator tick to that and is not the delay that matters. The
delay that matters is the game's own wind-up after the press, and it belongs to the
detector (section 6).

**What this does NOT show.** A blade touching a character (the guards are 9 m below the
ledge, and the simulator cannot walk to them); a moving target; and what the game's own
melee does with a press that arrives while the target is already at blade range.

### 7.5 Go or no-go: GO, as a default-off detector beside `edge` (2026-09-29)

Every prerequisite that could have stopped the work came back clear:

| Prerequisite | Could have stopped it by | Came back |
|---|---|---|
| 1 | the engine dropping the head's pitch in an attack, as the sibling mod's engine does | retained to 0.000 deg; no servo is needed and none is ported |
| 2 | the blade having no stable line in the hand | one palm-frame constant, 0.0000 m from the live draw at rest, 0.03 deg in physical swings |
| 3 | the blade landing somewhere else in the world than where it is drawn | 0.24 to 0.27 uu apart over 7 poses |
| 4 | the engine having no trace the mod may call, or one too dear to call per sample | one verified call, 7 to 8 us, at most one game tick late |

What is NOT settled, and why it does not block building the detector:

* **The wind-up** (section 6). `edge` presses within the first 100 ms of a swing; contact
  presses when the blade arrives, which for a 200 ms swing is 50 to 150 ms later. The game
  then needs its own time from the press to the hit (the swing trail appears 260 to 290 ms
  into an attack, VR-171), so under either detector the hit lands after the arm has
  passed, and contact lands it later still. That is a question of FEEL and only a headset
  answers it. The detector therefore carries `ContactLeadMs`: the blade is also traced
  ahead along the tip's own velocity, so the press can be moved earlier by a measured
  number of milliseconds. It ships 0.
* **The blade against a character.** The simulator's save cannot reach one. The trace
  reports a character as a character at range (7.4); the blade's own segment reporting one
  is a headset item.
* **What the game does with a press at blade range.** The attack is still aimed by the
  game along the view. A blade that touches a guard well off to the side presses an attack
  the game may swing at nothing. Headset item; the FIRE line carries the contact's class
  and distance so the log can be read against what was seen.

So: the detector is built, `[Melee] Detector=contact`, **default off, `edge` stays the
default**, switched live by `swing mode contact` and in F10. Whether it ever becomes the
default is a headset verdict and is not claimed here.
