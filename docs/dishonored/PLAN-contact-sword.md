# The contact-timed sword: research and a next-session brief (VR-173)

Status: **RESEARCH, not started.** Written 2026-09-21 alongside VR-170. Nothing here
is built. Section 5 is a brief to paste into a fresh session.

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
