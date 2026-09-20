# The motion sword (VR-37)

Swing the right controller and Corvo swings the sword. This file is the design,
every lever and word, how to read the log, what was measured, and the guide for
tuning it in the headset. Section 7 is the sneak-kill thrust (VR-155). The search
for a readable kill-available signal is VR-156.

## 1. What the player gets

- A swing of the right hand presses the game's attack for a moment. The trigger
  still attacks exactly as before: the swing ADDS a press, it never replaces one.
- It only happens while a swing makes sense: a gameplay view, the sword in the
  right hand, no grip held, the F10 overlay closed, and the game not owning the
  body (a takedown, a climb, a mantle).
- One physical swing is one attack. A haptic tick in the sword hand confirms it.
- **The swing decides WHEN the attack happens. The game decides WHERE it lands**,
  exactly as it does for the trigger: the sword's hit follows the game's own view
  and its melee camera assist, not the arc the hand drew.

## 2. How a swing is recognised

The decision is `src/game/dishonored/swing_core.h`: pure, no engine objects, hand
and head positions and a timestamp in, a verdict out. `tools\swing-core-host.ps1`
drives exactly that code (52 checks, 16 of them the thrust). The game side is `melee.cpp`
(`namespace dvr::swing`, declared in `swing.h`), present lane throughout.

Two detectors, chosen by `[Melee] Detector` and live by `swing mode`:

| Detector | How it decides | Why it exists |
|---|---|---|
| `edge` | Hand speed from two successive poses, the head's own movement subtracted, the median of the last three readings. The attack fires **the instant that crosses `EdgeSpeed`**, so the game's wind-up lands the hit where the arm is going. A fire clears an arm latch that only two slow readings in a row (below `RearmSpeed`) set again, and `CooldownMs` separately bounds a shake | The feel of the sibling BioShock mod's wrench swing, whose 3.6 m/s was tuned in a headset |
| `sustain` | EMA-smoothed room-space speed; a run above `SwingSpeed` must last `SwingMs` AND cover `SwingDistM`, then the attack is held for `HoldMs`. Keeps exactly the gates it always had (hand mesh, wheel, a 3 s mute after any UI event) | The detector this mod shipped until VR-37, moved verbatim and kept as the live A/B |

`Detector=sustain` is the shipped default until a headset verdict picks one; the
flip to `edge` is its own commit.

### The sample feed, and why it looks the way it does

All measured on the simulator, 2026-09-20:

- **The render presents twice per game tick** (`stereo: beat out/s=171 L/s=85`).
  A detector fed once per present sees every hand pose twice. The old detector read
  three scripted 0.68 m swings as ten 4-39 ms "flicks" with peaks of 8-14 m/s and
  fired nothing. The core is fed once per HAND SAMPLE GENERATION
  (`input_hand_aim_sample(1).generation`); the beat's `dup` counts the rest and
  reads about half of all presents by design.
- dt is the runtime's predicted display time, not the wall clock. Under 4 ms keeps
  the older seed (so the travel is timed over the full gap); over 100 ms re-seeds
  (an alt-tab, a load); a lost or non-finite pose re-seeds.
- **Over 20 m/s is a tracking jump**, not an arm: a controller that comes back from
  behind the body is re-acquired somewhere else in one sample. It re-seeds and logs
  `swing: tracking jump discarded`.
- **One sample can lie in either direction.** A smooth 1.6 m/s reach showed single
  readings of 2.9 and 3.2: a repeated pose reads 0 and the next sample carries two
  frames of travel in one frame's time. A lone tracking-noise spike has the same
  shape. `Median=1` decides on the median of the last three readings (one sample
  of latency, 11 ms at 90 Hz); a re-seed zeroes the history so the first reading
  after one cannot fire alone. The same reach reads 1.61 with it.
- Re-arming takes TWO slow readings in a row: a lone zero in the middle of a swing
  re-armed the latch and logged one BLOCKED line per repeat.

### The gates (edge), fail closed, first closed gate named

| Gate | Source | Note |
|---|---|---|
| the gesture is on | `g_meleeOn` | `[Mode] GamepadOnly=1` and `DISHONORED_VR_XR_SAFE=1` veto it and stay the owner: `swing on` refuses and says which |
| a gameplay view | `DvrGameplayVerdict()` | covers the pause menu, loads, cutscenes AND the power wheel, which the mod counts as a UI surface |
| the sword is in the right hand | `g_rflPrimaryKind` + tick | published from the script lane's 4 Hz equipment read as two atomics; older than 1000 ms is unknown, and unknown is not a sword. The attack input with an empty hand would DRAW the sword, which is not what swinging an empty hand should do |
| no grip held | `g_padBtnsPub` LB/RB | the right grip is block; a blocking arm is not attacking |
| the overlay is down | `g_ovlVisible` | |
| the game does not own the body | `dvr::anim::snapshot()` | master in the hand-back list, or Mantle. An invalid snapshot leaves this gate OPEN: it is a refinement, the verdict and the cinematic pad park are the safety |

### The attack press

`Output=rt` presses the right trigger (OR `rb`, the right shoulder) for `PulseMs`,
and at least until the game has polled the pad `PulseMinPolls` times, capped at
500 ms: a hitch can swallow a short press whole. 120 ms against a 300 ms cooldown
leaves 180 ms released between combo presses; the old 220 ms hold left 80. The
cinematic pad park zeroes it after the fact, so a pulse cannot leak into a cutscene.

### A verified write is not an honoured one

After every fire the mod watches the game's own animation state for `HonourMs`:

- `swing: HONOURED slash after N ms (upper=StatePlayerMeleeAttack seq=...)` - the
  game started an attack because of the press. `HONOURED kill` is master
  `StatePlayerMasterAssassinate` (the stealth kill is the same input in context).
- `swing: NOT HONOURED ... the game BLOCKED instead` - the upper lane passed
  through a block state: this install's pad binding puts block on that input. The
  line names the other `swing output`.
- `swing: NOT HONOURED ... no block state was seen` - with the pad poll counts:
  flat means the game never read the pad while the press was open.
- `swing: INCONCLUSIVE` - it cannot tell, and says why: the player's own trigger was
  pulled in the window, the sword was already mid-attack, the press drew the sword,
  or there is no animation snapshot.

Measured: `Output=rt` HONOURED in 15-16 ms on every simulator swing. `Output=rb`
on the same machine: upper `StatePlayerBlock`, NOT HONOURED - so the instrument
can print the unwelcome answer. Which pad binding set is active is a property of
the install; this line is how a tester's log answers it.

## 3. Levers (`[Melee]`)

The pre-VR-37 keys keep their names and values: they are materialised in every
installed ini, so re-defaulting them would do nothing (TRAPS section 1). The new
keys have new names and resolve from compiled defaults when absent, so there is no
`kConfigVersion` bump.

| Key | Default | Range | Meaning |
|---|---|---|---|
| `Enabled` | 1 | | the gesture |
| `Detector` | `sustain` | edge, sustain | see section 2 |
| `EdgeSpeed` | 3.6 | 0.3-10 | edge: the hand speed (m/s) that is a swing |
| `RearmSpeed` | 1.0 | 0.05-9, effective <= 0.9 x EdgeSpeed | edge: how slow the hand must get to re-arm |
| `CooldownMs` | 300 | 0-2000 | both: between attacks |
| `PulseMs` / `PulseMinPolls` | 120 / 2 | 20-500 / 0-10 | edge: the press |
| `HeadRel` | 1 | | edge: subtract the head's movement |
| `Median` | 1 | | edge: median of three readings; 0 = raw |
| `RequireSword` | 1 | | edge: the sword gate |
| `Output` | `rt` | rt, rb | the pad input pressed |
| `HonourMs` / `HonourHaptic` | 600 / 0 | 100-2000 | the watch window; a second soft tick on HONOURED |
| `SwingSpeed` / `SwingMs` / `SwingDistM` / `HoldMs` | 1.8 / 120 / 0.25 / 220 | | sustain only |
| `Haptic` | 1 | | the tick on a fire |

**3.6, 1.0, 300 and 120 are the sibling mod's headset-tuned numbers, used here as
STARTING values.** They describe a player's arm, not an engine, which is why they
may cross games at all; they are not yet a Dishonored verdict.

## 4. Words, status, F10

`swing status | on | off | mode edge|sustain | threshold <m/s> | rearm <m/s> |
cooldown <ms> | pulse <ms> | polls <n> | rel on|off | filter raw|median |
sword on|off | output rt|rb | honour <ms> | log on|off | force on|off |
sim <peak m/s> [humpMs] [reps] | save`

- `status` prints the gate, the counters and **PEAK SINCE LAST STATUS**, then
  resets the peak: that number is what replaces a guessed threshold.
- `log on` adds a 10 Hz speed line and one line per moving sample (generation, dt
  source, speed, raw, room, position): the cadence itself is the thing under test.
- `sim` drives half-sine speed humps, as POSITIONS, through the real core and the
  real gates; `reps` is what makes the cooldown testable. `force on` skips the
  sword gate, is never saved, and warns.
- `status.json` `features.swing`: on, vetoedBy, detector, output, threshold,
  gateOpen, closedBy, armed, samples, dupSamples, stillSamples, fires, blocked,
  lastBlock, honoured, kills, notHonoured, inconclusive, lastSpeed, peakSpeed10s.
- F10 > Controls > Motion sword: enable, detector, speed, re-arm, press length,
  cooldown, and live last / PEAK (10 s) / gate / counters. Saves on release. The
  overlay closes the gate while it is up, so swing, then open it and read PEAK.
- Every 5 s: `swing: beat ...` names the closed gate. Fires stay 0 while a gate is
  closed, and the gate named is the owner of that zero.

## 5. Verification

| Intent | Command | Read |
|---|---|---|
| the decision core | `tools\swing-core-host.ps1` | `swing-core: 52 checks passed` |
| a swing fires, a reach and a body turn do not | `tools\xrsim-run.ps1 -Path tools\xrsim\swing-edge.xrs` | FIRE then HONOURED; `peakSpeed10s lt 2.2` on the reach; `sim window finished: 3 fire(s)` |
| every gate blocks once and says why | `tools\xrsim-run.ps1 -Path tools\xrsim\swing-gates.xrs` | four BLOCKED reasons, then the same swing fires |

Both need a loaded level in GAMEPLAY and `GamepadOnly=0`. The sequences only ever
MOVE the hand (`hand r to ...`): `hand r grip pose` teleports it, and a teleport is
a tracking jump the detector discards on purpose.

Counter-checks run 2026-09-20: the body-turn leg with `swing rel off` FIRES (so
head-relative is what rejected it); `swing mode sustain` on the clean feed fires
(`run 122 ms 0.50 m`), so the A/B is a fair one; `swing off` leaves a swing silent.

## 6. Tuning it in the headset

Loop: play 30 seconds, `swing status` (or F10 PEAK), change ONE value, `swing save`.

| What you notice | Read | Change |
|---|---|---|
| Swings do not register | `PEAK` against the threshold; `lastBlock` | PEAK under the threshold: lower `EdgeSpeed` to about 0.8 x your usual PEAK. A gate named: fix the gate (sword out, grip released) |
| It attacks while you walk, turn or reach | PEAK during that movement | raise `EdgeSpeed` above it; confirm `HeadRel=1` and `Median=1` |
| One swing, two attacks | two FIRE lines under 400 ms apart | raise `CooldownMs`; lower `RearmSpeed` |
| A fast combo drops swings | `BLOCKED: not re-armed` or `cooldown` | raise `RearmSpeed` (up to 0.9 x the threshold); lower `CooldownMs` |
| FIRE but no attack | the NOT HONOURED line | "BLOCKED instead": `swing output rb`. Polls flat: raise `PulseMs` or `PulseMinPolls` |
| The attack feels late | the HONOURED `after N ms` | edge already fires on the crossing, so lower `EdgeSpeed`; try `swing filter raw` to give back the one frame the median costs |
| You want the old feel to compare | | `swing mode sustain` |

## 7. The sneak-kill thrust (VR-155)

**What the player gets.** Creeping up behind a guard you stab, you do not slash.
With `Stab=1`, a THRUST of the sword hand while crouched presses the attack. The
game has no separate input for the stealth kill: the input ini binds no
assassinate alias, the kill is the attack in context. So the thrust presses what a
slash presses and the game decides what it becomes.

**Why it is its own shape.** A stab is 1.5 to 2.5 m/s over 20 to 35 cm. It never
crosses a slash threshold, and lowering that threshold while crouched would turn
every reach, lean and point into an attack in the middle of a stealth approach. A
false positive here costs a whole run, so the thrust ships OFF and is judged on
four things at once:

| Bar | Key | Default | What it rejects |
|---|---|---|---|
| the hand EXTENDS fast enough to start | `StabSpeed` | 1.5 m/s | a slow drift forward |
| it extends far enough, soon enough | `StabTravelM` in `StabWindowMs` | 0.20 m in 400 ms | a jab, a twitch |
| in a straight line | `StabRatio` (extension gained / path travelled) | 0.75 | a sweep, an arc |
| the way the head faces | `StabForward` (dot, flattened) | 0.5 | reaching to the floor or the side |

"Extension" is the hand's own displacement (head movement subtracted) projected on
the direction away from the right shoulder. The shoulder is modelled from the
head, yaw only, at `ShoulderRightM/DownM/BackM` 0.17/0.22/0.04 (the mirror of the
choke model in `CHOKE_GESTURE.md`) and is used for that DIRECTION and nothing
else. Measuring the change in hand-to-shoulder distance instead would make a head
that turns with the hand held still read as extension, because the modelled
shoulder swings; a host test pins that it does not. The same median of three and
the same shared arm latch and cooldown apply, and a slash that fires takes the
gesture: one movement is one attack.

**Armed only while sneaking** (`StabArm=sneak`): the game's own crouch, read from
the collision capsule the crouch module already samples every 50 ms (87.5 standing,
65 crouched). It is the one reading that covers the crouch button AND a physical
crouch, which presses that same button. Under 50 is a crawlspace and does not arm;
a capsule not read for a second is unknown and does not arm. Standing, a thrust is
silent: no verdict, no log line. `StabArm=always` exists to separate the arming
from the detector while testing. `StabArm=kill` (arm when the game would accept a
kill, with a haptic ready cue) waits on VR-156.

**Words.** `swing stab status | on|off | speed | travel | ratio | forward | window |
arm sneak|always | shoulder <right> <down> [back] | sim <peak> [humpMs] [reps]`.
`status.json` `features.swing.stab`: on, arm, armed, armedBy, fires, rejects,
lastReject, peakExtension10s, bestTravel10s, bestRatio10s. F10 rows under Motion
sword (edge detector only).

**Log.** `stab: ARMED - crouch (capsule 65.0)` / `stab: DISARMED - none (standing,
capsule 87.5)` on the edge only. `stab: FIRE extension 2.82 m/s travel 0.20 m ratio
0.96 forward 1.00 in 111 ms`, then the shared `swing: FIRE #n stab` and the
honoured-check. `stab: REJECTED travel: travel 0.07 m (needs 0.20) ratio 0.94
(needs 0.75) forward 1.00 (needs 0.50)` names the bar that was missed and every
value, which is the tuning feedback.

**Measured, simulator, 2026-09-20** (`tools\xrsim\swing-stab.xrs`, 7 legs): a
0.35 m thrust in 250 ms standing does nothing and logs nothing; after `btn b` the
capsule reads 65.0, `stab: ARMED`, and the same thrust fires at 0.20 m of extension
111 ms into the movement, HONOURED 16 ms later; a 0.12 m jab is REJECTED on travel;
a 0.45 m reach to the floor in 450 ms does not fire; a slash while crouched is a
slash, once; standing up logs DISARMED; `swing stab sim 2.2 200 3` fires 0 standing
and 3 with `arm always`. Host: 16 thrust checks inside the 52.

**Not measured, and what would measure it.** Whether the attack becomes the KILL.
The dev PC's newest save is the Hound Pits pub, which has nobody to kill, so every
thrust above was honoured as `StatePlayerMeleeAttack`. Behind an unaware guard the
line to look for is `swing: HONOURED kill (master=StatePlayerMasterAssassinate)`.

**Tuning the thrust in the headset.**

| What you notice | Read | Change |
|---|---|---|
| The stab never fires | `stab: REJECTED <bar>` and `swing stab status` (PEAK extension, best travel, best ratio) | lower the bar the line names: `StabTravelM` first, then `StabSpeed`, `StabRatio`, `StabForward` |
| No REJECTED line either | `stab: ARMED` in the log, F10 "thrust:" row | not armed: crouch, or `swing stab arm always` to isolate. No line at all with it armed means the extension never reached `StabSpeed` |
| It stabs when you reach or gesture while sneaking | the FIRE line's ratio and forward | raise `StabRatio` or `StabForward`, then `StabTravelM` |
| Travel reads short for a full arm's stab | `travel` against what your arm really did | the shoulder is off for your build: `swing stab shoulder <right> <down> [back]` |
| A hard stab comes out as a slash | `swing: FIRE #n slash` | expected above `EdgeSpeed`; it is the same input, so the game still makes it the kill |

## 8. Graveyard

- **Deduplicating by pose.** Dropping a sample whose position equals the last one
  looked like the clean fix for the double present. A resting simulated hand is
  bit-identical every frame, so every sample was dropped and the latch could never
  re-arm. The identity is the hand sample's generation; a pose repeated at a NEW
  generation is a real reading of a hand at rest and must go through.
- **Deduplicating by `locate_gen()`.** The head locate and the hand sync are not the
  same cadence: some new locate generations carry the previous hand pose. One swing
  logged four `BLOCKED: cooldown` lines with speeds jumping between 4 and 13.9 m/s.
- **Blaming the simulator's clock for the 3.1 m/s reach.** Moving the simulator's
  motion onto the frame's display time was right for its own reasons and changed
  the reading from 3.11 to 3.20, that is, not at all. The per-sample log found the
  real shape (a repeat, then a doubled step) in one run. Measure first.
- **Asserting the pulse from `pad: xbtn=`.** That line prints the button mask before
  the swing's RB is ORed in, so it cannot see `Output=rb`. The honoured-check is the
  instrument for whether a press arrived.
- **A thrust sim that counted humps.** The simulated hand alternates direction to
  stay in reach, so of three humps the middle one is the hand coming BACK, and
  `swing stab sim .. 3` fired two. The detector was right and the sim miscounted:
  a thrust rep is an out-and-back pair.
- **A floor reach in 250 ms as the "not a thrust" case.** 0.45 m in 250 ms peaks at
  3.65 m/s, which IS a slash, and the slash detector took it. The realistic reach
  (450 ms, 1.9 m/s) starts a thrust run and is rejected for not going forward.
