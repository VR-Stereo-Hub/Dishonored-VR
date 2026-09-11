# VR-76: the one-frame rightward hand/weapon jump - findings and fix plan for review

2026-09-11. **Status: diagnosis proposed, NOTHING CHANGED YET.** This document is
written for an outside review before any behaviour change. It carries the
evidence, the mechanism it points to, what that mechanism does not explain, and a
plan whose every step can fail on its own terms. Review the reasoning, not just
the conclusion: this project has a graveyard of confident flicker mechanisms
(section 7), and three of them were argued from counters that could not see the
fault.

Branch `claude/vr-76-hand-flicker-right` (off `claude/vr-73-boat-dock-fall`, PR #35
open and unmerged). Ticket VR-76. Separate root-cause ticket VR-77.

---

## 1. The symptom, as observed

- The hand and weapon models shift to the RIGHT for a single frame, then return.
- Much clearer on the desktop mirror (the game window) than in the headset, where
  it is subtle.
- Rotation of the hands does not matter: the shift stayed rightward with the
  hands held upside down. The shift is in screen space, not the model's own axes.
- It often comes in clusters, several times within one second.
- The world is not reported to move with it.

Tested configuration, unchanged throughout (do not vary it during this work):
`[Pace] Lag=2`, `[Stereo] LagAB=0`, `[Hands] PoseLag=2 PaletteEyeOffset=1`,
`[MotionAim] Enabled=0`, `GamepadOnly=0`, render 2750x2850, `VirtualMode=1`,
90 Hz, Quest 3 over VDXR, `[Stereo] Method=reentry`, `HoldUntagged=3`. The run
switched capture to **shared** at startup (`capture: mode sync -> shared`) and
every frame-identity window during play reports `mode=shared` (37 of 37).

---

## 2. The instrument (commit `37aab49f`, build `vr33-hands-working-94-g37aab49f`)

`V` in the game window writes `MARKER #N (V)` plus the placement history of the
2.5 s before it (`MfMarker`, `mesh_split.cpp`; key read in `hotkeys.cpp`). No
draw changes. Per present that drew the hands it records:

- the palette placement's eye decision (`MpEyeForPresent`) and why:
  `T` toggled, `S` same eye kept, `A` ambiguous (no offset), `F` first sample;
- the jump `d` it decided from, the draw's `projRight`, each hand's placed target
  on the draw's right axis `tR`, the pose generation, refusals, weapon misses;
- the runtime's eye tag, from a ring filled once per present in `DvrGameTick`
  (`MfNoteTag`, reads `dvr::stereo::last_output().eyeSign`).

The alignment between that tag ring and the draws is **measured, not assumed**:
the marker reports agreement at offsets +1, +2 and +3 and uses the best one.

Run 1: one baseline marker pressed before any flicker, then 37 markers pressed
just after a seen flicker. Log kept locally (not committed, it is a run
artifact): `D:\dvr-data\flicker\run1-markers.log`. Parsers used for every number
below: `D:\dvr-data\flicker\mf.py` (marker histories) and `corr.py` (timing
correlation). Earlier log: `before-marker-build.log` in the same folder.

---

## 3. Findings

### F1. The hand placement is clean on every flicker window

Across all 37 flicker markers (about 15,800 presents with hand draws):

| Check | Result |
|---|---|
| placement eye vs runtime tag, at the measured offset | agree on every tagged present (e.g. `+3 agrees 442/442`, `389/389`) |
| target outliers (a hand's `tR` more than 0.35 IPD off its same-eye neighbours) | **0** |
| refused hand placements | **0** |
| ambiguous (`A`) decisions | **0** |
| flags raised at all | 346 `decision S` (explained in F2), 7 weapon misses |

The `tR` rows alternate by one IPD exactly as designed (for example
`tR -10.44/+8.47` L, `-16.76/+2.15` R, `-10.46/+8.45` L). **No logged placement
quantity moves on or near the flickered frames.** This clears the palette eye
decision, the half-IPD correction and the pose sample for this symptom.

It is consistent with a direction argument made before the run: the mirror was
documented as pinned to the LEFT eye, and in the left eye an unknown or wrong eye
decision places the hands LEFT, not right. A rightward jump never fitted the
known eye-decision fault.

### F2. Every flicker marker follows a burst of single-draw ticks

The stereo method (`reentry`) normally doubles every tick: pass 1 draws the left
eye, pass 2 the right. When the present-stall gate refuses
(`scene_draw.cpp` `SceneDrawDecide`, `g_frame == g_sdLastDrawPresent` ->
`no present since the previous draw`), the tick draws once, from the -1 camera
(BRIEF-eye-flicker.md 3d), and that present is **untagged** (0).

In run 1 these come in bursts where single and double ticks alternate, logged as
`reentry: gates -> DOUBLE draw after 1 single tick(s)` every 15-30 ms. In the
marker rows a burst reads:

```
tag  L R 0 L R 0 L R 0 ...
eye  L R L L R L L R L ...
why  T T T S T T S T T ...
```

The `S` flags are the present after each single: the placement correctly keeps
the left eye because the single tick really was a left-camera draw.

| | Result |
|---|---|
| flicker markers with a single-tick line in the 600 ms before them | **37 of 37** |
| time from the last single tick to the marker | min 15 ms, **median 328 ms**, max 578 ms (a reaction time) |
| random instants in play with a single-tick line in the previous 600 ms | 42 % (458/1095, 50 ms steps) |
| baseline marker (pressed before any flicker), stereo part of its window | no single ticks in its last 2.3 s |
| single-tick transition lines in 55 s of play | 172; plus 24 `present handed in NO frame` holds |
| `pushed eye TWICE`, `STALE L/R EYE`, `SWAPPED` lines during play | **0** (`stereo: eyes ageL=1 ageR=0`, healthy) |

At a 42 % base rate, 37 of 37 by chance is about 1e-14. The correlation also
explains the clusters: a burst leaks one bad frame every three presents (F4).
Frame gaps (40-103 ms in `xrEndFrame`) are not required: 22 of 37 markers had
none in the 1.5 s before them.

### F3. The runtime's eye tag names the PREVIOUS present's pixels (shared capture)

The marker's measured alignment was **+3, not the +2 the draw/present order
predicts**, and +2 read **0 %** (perfectly opposite) on every marker:

```
runtime tag offset +1 agrees 443/444, +2 0/443, +3 442/442 -> using +3
```

The arithmetic, from `frame_hooks.cpp` `hkPresent`:

1. Draws made while `dvr::frame::count() == k` are presented by the next
   `hkPresent`, which increments the counter to `k+1`.
2. In that present, `game_tick` (and so `MfNoteTag`) runs BEFORE `end_frame`, so
   the ring entry stamped `k+1` holds the output of present `k`.
3. If the output of present `P` described the pixels drawn for `P`, draws `k`
   would pair with the entry stamped `k+2`. The measured `k+3` means the output
   of present `P` describes the pixels of present `P-1`.

That is the shared capture path, confirmed in code: `capture.cpp` writes the
current backbuffer into one slot with this present's tag and delivers the other
slot, the previous present's, with ITS tag (`g_sharedCur = prev`,
`g_sharedDelivered = slot`; the file header says the same of `deferred`: "the
frame reaches the headset one present late"). `reentry.cpp` pushes
`delivered_tag()` to the runtime (`delivered`, around line 404), and the runtime
pops it as `srSign` (`openxr_runtime.cpp` `sr_pop_eye`, around line 3795). For the
headset this is correct: tag and pixels travel together. In `sync` mode (the tree
default) the delivered frame is the current one and there is no lag.

### F4. The desktop pin uses that delayed tag on the CURRENT backbuffer

`desktop_eye::on_present(eyeSign)` (`core/gfx/desktop_eye.cpp`) reads
`GetBackBuffer` at the moment it is called: the frame about to be presented. It is
called from the runtime with `srSign`: `if (srSign < 0) mirror_present(srSign)`
(around line 3804, snapshot) and `if (srSign > 0) mirror_present(srSign)` (around
line 4423, re-blit), plus `mirror_present(eatenEye)` when no XR frame is open
(around line 3641). With shared capture `srSign` is the eye of the PREVIOUS
present, so the pin acts one present out of step with the pixels it touches.

Steady stereo (drawn eyes L R L R), shared capture:

| present | drawn eye (backbuffer) | srSign (previous pixels) | pin action | window shows |
|---|---|---|---|---|
| p1 | L | +1 | re-blit held | held (R) |
| p2 | R | -1 | snapshot backbuffer = **R** | R |
| p3 | L | +1 | re-blit R(p2) | R |
| p4 | R | -1 | snapshot R | R |

**The window is pinned to the RIGHT eye**, not the left as DESKTOP_MIRROR.md
states. It looks correct because it is still one eye, and it stays one eye as
long as the tags alternate.

A single-draw burst (drawn L R Lu L R Lu), shared capture:

| present | drawn eye | srSign | pin action | window shows |
|---|---|---|---|---|
| p1 | L | +1 | re-blit R(p0) | R |
| p2 | R | -1 | snapshot R(p2) | R |
| p3 | L untagged | +1 | re-blit R(p2) | R |
| **p4** | **L** | **0** (p3 was untagged) | **no call at all** | **raw L - the leak** |
| p5 | R | -1 | snapshot R(p5) | R |
| p6 | L untagged | +1 | re-blit R(p5) | R |
| **p7** | **L** | **0** | **no call** | **raw L - the leak** |

A left-eye image shown in a right-pinned stream moves near objects RIGHT by their
binocular disparity and leaves distant ones in place. At about 1000 px focal
length (108 deg horizontal FOV over 2750 px), a hand at 0.5 m shifts about 125 px
and a wall at 5 m about 13 px. That predicts every reported property: rightward,
hands and weapon (nearest) only, independent of their orientation, one frame,
clustered at up to one leak per three presents during a burst, clearer on the
window than in the headset.

Under the no-lag reading (sync capture) the pin would be left-pinned and the
single tick's raw frame would itself be a left-camera image: **no jump would be
predicted.** So the reported rightward jump is itself evidence for F3.

### F5. What this does NOT explain

- **The headset version.** In the headset, the pairs are assembled from
  delivered tags that match their pixels, and the untagged present is held (the
  previous layer re-submitted, `xr: present handed in NO frame`). No leak of a
  wrong-eye image is predicted there. The subtle in-headset percept is therefore
  unexplained. Candidates, none measured: the held slot itself during a burst (a
  re-shown pair while the hands move), or a percept of the same event through a
  different path. It must be re-checked in the headset after the mirror fix, as
  its own question.
- **Why the pin was accepted when it shipped** (92b04896, 2026-09-06). Its
  verification may have run in `sync` capture, or its counts (snapshots equal to
  re-blits) could not distinguish a left pin from a right pin. Worth confirming,
  not required for the fix.
- **Run 1 has no per-present record of the pin's own actions.** F4 is derived
  from code and from F2/F3, not observed directly. The first build below closes
  that gap before the fix is credited.

---

## 4. The fix plan

One behavioural change per build, each with its own question and its own
failure condition.

### Step 1 - desk: prove the policy on the host, both ways

Extract the pin's decision into a pure function (sequence of drawn eyes, capture
lag 0 or 1, pin policy) -> sequence of eyes the window shows. Cases: steady
`L R`, bursts `L R Lu`, repeated eyes `L L R` and `R R L`, starting mid-stream,
a reset. Assertions:

- **old policy, lag 1, `L R Lu`: the window shows a non-pinned eye** (the test
  must fail the current code, or it cannot see the fault);
- old policy, lag 0: no leak (explains the shipped verification);
- new policy, lag 0 and lag 1, every pattern: the window never shows the
  non-pinned eye; at worst it repeats a frame.

Follow the existing host-test pattern (`tools\palette-eye-host.ps1`,
`tools\camera-clamp-host.ps1`, `frame_test.exe`).

### Step 2 - the change: pin by the eye of the pixels, not the delivered tag

a. **The eye of the current backbuffer** is already known to the method:
   `reentry.cpp` `end_frame` computes `eye` (the tag popped for "the frame the
   game just drew", after the c5 override) and passes it to
   `capture::set_pending_tag(eye)` before the grab. Publish it to the pin at that
   point (for example `dvr::desktop_eye::note_drawn_eye(eye)`). The mono screen
   method publishes 0.

b. **Policy in `desktop_eye`:** snapshot when the drawn eye is -1, re-blit the
   held image when it is +1. For a drawn eye of 0 inside a live stereo stream
   (a snapshot taken within the last few presents), re-blit the held image too,
   so an untagged frame can never reach the window. Outside a stereo stream (menus,
   loads), 0 shows the frame as today. Reviewer: the alternative for 0 is to show
   the raw single, which is a left-camera image today but would become a half-IPD
   shift if VR-77 or BRIEF fix A ever centres single ticks; the re-blit is
   proposed because it does not depend on that.

c. **Where it is called.** The pin must run on EVERY present, including those
   whose `srSign` is 0, which the current call sites skip. Two options:
   - (preferred) in `openxr_runtime.cpp`, drop the left-half call and make the
     right-half call unconditional, marked `41.x (Dishonored)`; the pin ignores
     the sign it is handed and uses the drawn eye. The snapshot then happens after
     the XR capture block and before `composite_hud`, which is safe only if
     nothing between those points writes the D3D9 backbuffer - **verify that**. The
     `eatenEye` site (no XR frame open) already calls unconditionally.
   - (no runtime-layer change) call the pin from `frame_hooks.cpp` after
     `on_present_end` and before the game's `Present`. Cost: the re-blit would
     then overwrite the window HUD composite that `composite_hud` draws, which is
     why the pin sits before it today.
   The runtime layer is kept close to the BioShock copy (CLAUDE.md), so the
   reviewer should weigh a two-line seam change against losing the window HUD.

d. **The lever.** `[VR] DesktopEyeSource=draw|tag`, live toggle through the
   command seam, logged on change and at startup with where the value came from.
   Repo rule: a new render lever ships default OFF, so the tree default stays
   `tag`; the tester's installed ini gets `draw` for the headset run. Reviewer:
   the desktop window is not the headset render and the old behaviour is a
   proven defect in shared/deferred capture, so arguing for `draw` as the tree
   default is reasonable, but it is a decision for the maintainers, not this plan.

e. **The instrument (the part that can fail).** Extend the 15 s `desktopeye:` line:
   - presents by drawn eye (L/R/0) and how `srSign` compared with the drawn eye
     (same / opposite / zero). **Prediction under shared capture: opposite on
     nearly every tagged present in steady stereo. Under sync: same.** If shared
     capture reads "same", F3 is wrong and the fix is aimed at nothing;
   - a shadow of the OLD policy: how many presents it would have shown a
     non-pinned eye. **Prediction: above 0 only in windows with single-draw
     bursts.** If single ticks occur and the shadow reads 0, F4 is wrong;
   - the NEW policy's own count of non-pinned frames shown: must be 0;
   - single-draw ticks in the window, so a run with no bursts reads as
     inconclusive rather than as a pass.
   Add the pin's action per present (snapshot / re-blit / none) and the drawn eye
   to the `V` marker rows, so any residual marker can be read against it.

### Step 3 - headset run A: does the mirror still jump?

Installed ini: only `[VR] DesktopEyeSource=draw` added; every tested value above
untouched. Play as in run 1, pressing `V` after any jump seen on the mirror.

- **Pass:** no rightward jump on the mirror; the log shows single-draw bursts
  occurred, the old-policy shadow counted leaks, the new policy counted 0.
- **Fail:** jumps remain on the mirror with the new policy at 0 leaks - the
  mechanism is wrong, and the marker rows plus the pin actions say what differs.
- **Inconclusive:** no single-draw bursts in the log.

### Step 4 - headset run B: is the in-headset version gone? (separate launch)

- **Gone:** close VR-76 after review and merge approval.
- **Still there:** it is not the window pin. Next candidates, in order: the held
  slot during bursts (A/B `[Stereo] HoldUntagged` or `vrpace strict`, both
  existing levers), then the bursts themselves (VR-77).

### Step 5 - documentation, in the same commit as the change

- DESKTOP_MIRROR.md: the pin was right-eye under shared/deferred capture and
  leaked the left eye on untagged presents; what it keys on now.
- TRAPS.md section 2 or 3: "a tag that travels with delivered pixels does not name
  the backbuffer being presented" - the tag is correct for the consumer it was
  built for and one present out of step for any consumer that reads the live
  backbuffer.
- ARCHITECTURE.md decision log: why the pin reads the drawn eye, and which call
  site was chosen.
- VR-33-HANDS-AND-WEAPONS.md "Reading the log": the marker's new fields.

---

## 5. Out of scope for this change

- **VR-77**, the single-draw bursts. They are the trigger, not the defect in the
  window: a correct pin is correct whether or not bursts happen. Fixing the bursts
  changes stereo liveness and render cost and needs its own A/B.
- Any change to the palette eye decision, pose lag, weapon contracts, capture
  mode, pacing or the stereo tag ring. F1 clears the placement for this symptom.
- VR-75 (cutscenes in stereo) rides the same branch as its own commit and its
  own headset check; it is unrelated to this review.

---

## 6. Questions for the reviewer

1. Check F3's arithmetic against `hkPresent` and `capture.cpp`'s shared grab. Is
   `last_output().eyeSign` at present P really the tag of P-1's pixels in shared
   mode, and of P's pixels in sync mode?
2. Is `eye` in `reentry.cpp` `end_frame` the eye of the pixels in the D3D9
   backbuffer at that present on every path, including the c5 override and the
   realign drain? Is there any path where the grab copies a different frame?
3. Between the left-half and right-half mirror call sites in `on_present_end`,
   does anything write the D3D9 backbuffer (HUD redirect, overlay, frame-id
   stages)? This decides whether step 2c's preferred option is safe.
4. Do other consumers read the live D3D9 backbuffer while keying on `srSign` or
   `last_output().eyeSign`? The same one-present error would apply to them.
   (`hand_draw` already uses `delivered_tag()` against the delivered texture,
   which is correct.)
5. Is the 0-eye policy in step 2b right, given BRIEF fix A's history?
6. Anything in F1-F4 you consider measured wrongly. Name the line.

---

## 7. The graveyard this sits on - read before proposing anything else

Each of these cost a session or a headset run. None is re-opened by this plan.

| Fault or idea | Outcome | Where |
|---|---|---|
| The game window alternating L,R,L,R (no desktop eye policy at all) | FIXED by the pin, 2026-09-06; this plan corrects which eye the pin holds | DESKTOP_MIRROR.md, BRIEF-eye-flicker.md |
| Four eye-flicker hypotheses (cadence beat, headset mirror, missing second present, stuck latch, runtime never pairing) | falsified | BRIEF-eye-flicker.md section 4 |
| Intermittent doubling with the camera still at -1 on single ticks | the context for VR-77, not this fix | BRIEF-eye-flicker.md section 5 |
| Ghosting on head turns | the cadence beat; 90 Hz | ENGINE_NOTES, session 15 |
| Startup eye starvation (tick slower than the slot) | measured, self-heals | ENGINE_NOTES, session 15c |
| One-sided tag stream after pause/resume, stale right eye | gates decided once per tick | ENGINE_NOTES, session 7 |
| c5 pairing arms trusted equally | fragile arm defers to the ring | ARCHITECTURE decision log, session 10 |
| HoldUntagged installing a black frame | holds re-submit the previous layer | ARCHITECTURE decision log |
| Live script mono flag resetting an older render eye | restored the pre-#26 eye decision | ENGINE_NOTES VR-69, LOCKON_REVIEW.md |
| Z clamp breaking camera offset ownership (downward motion) | ownership kept through the clamp | ENGINE_NOTES VR-69, DOWNWARD_CLAMP_REVIEW.md |
| `pushed eye +1 TWICE` read as proof of a stale left eye | over-claimed once; 0 occurrences in run 1 | STATUS, VR-69 ticket |
| Eye from the drawing pass; writer position vs c5; draw's own matrices; same-eye hold | closed by measurement | VR-69 ticket, TRAPS section 3 |
| Palette size as identity, the five hiding filters | dead | VR-33-HANDS-AND-WEAPONS.md section 8 |

The rules this plan tries to follow, from CLAUDE.md and TRAPS: an instrument that
cannot fail its own hypothesis is not evidence (step 1 must fail the old code;
step 2e must be able to read "same" and "0"); a counter is not evidence until you
know its population (step 3 requires bursts to have occurred); a verified write is
not an honoured one (the pass is the picture on the window, not the counters).

## Implementation review follow-up (2026-09-11)

The candidate implementation and review answers are in
[VR-76-CODEX-HANDOFF.md](VR-76-CODEX-HANDOFF.md). This original proposal is kept
as the record of the pre-change reasoning. Corrections: shared latency depends
on SharedWait; mirror_present has an internal zero guard; pairHold returns before
the normal tail; composite_hud is currently a no-op; startup/resource lifetime
needs an explicit warmup exception; clustered marker windows do not justify the
quoted independent-trial p-value. The installed draw policy awaits user testing.
