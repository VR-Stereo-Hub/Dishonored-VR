# Traps and the graveyard

**Read this before spending a session on a setting that "does not work".**

This file collects the traps this project has actually fallen into and the plans
that were tried and failed. It is not a style guide and not a list of good
ideas - everything here cost a session, a headset run or both. Entries are added
when something is learned the hard way, and they are never deleted: a plan that
failed is a result, and the record is what stops it being tried a third time.

The per-topic graveyards live with their topics and are indexed at the bottom.

---

## 1. THE STALE-SETTING CLASS - check this FIRST, every time

**This class has bitten twice, and both times a whole session went to it.**

The shape is always the same: a setting has more than one place it can live, the
place you edited is not the place that decides, and **nothing says so**. The
symptom is "I changed the value and nothing happened", or worse, "I changed the
value and something unrelated broke".

### The two that happened

**VR-65, the pose lag.** Two builds changed the compiled default from 1 to 2,
then to 0. The installed ini named `Lag=1`, and the loader reads a compiled
default **only when the key is ABSENT**. Both builds ran at lag 1. The results
were read as "the new value failed", and pose selection was wrongly declared
eliminated as the cause of the judder. It was the cause.

> **An ini key that exists beats every compiled default.**

**VR-66, the render size.** The size lived in two files. The launch file drove
the `-ResX/-ResY` the engine obeys; the mod ini drove the mode `VirtualMode`
advertises. Only the `res` seam word wrote both, and a text editor writes one.
Editing the ini alone left the engine asking for a five-day-old size that was no
longer being advertised, so UE3 fell back to a real display mode and the picture
came up fullscreen and soft. Two attempts were spent editing the key again, and
the ticket was filed blaming "something outside both files".

> **A file that exists beats the ini you edited.** A setting with two persistent
> homes has no owner.

### What to do before touching a key

1. **Find every place the value can live.** Grep for the key name across `src/`,
   `tools/` and the game's own config. If more than one file or more than one
   code path can supply it, that is the bug until proved otherwise.
2. **Read what the run actually resolved it to**, not what you wrote. Every lever
   worth arguing about now logs its effective value and where it came from
   (`config: [Pace] Lag=%d - %s`, `launch: the render ask is ... from %s`). If a
   lever does not log that, **add the line before running the experiment** - it
   is cheaper than the run.
3. **Confirm the change reached the consumer**, not just the file. For the render
   size that is three lines that must all carry the same number
   (`docs/VERIFICATION.md`); for a pose lever it is the resolved-value line.
4. **Do not wipe and reinstall to diagnose this.** It has never once been the
   answer and it destroys the evidence.

### Where the settings actually live

| Setting | The places it can live | Which one decides |
|---|---|---|
| Render size | `dishonored_vr.ini` `[Screen]`, `dishonored_vr_launch.txt`, `DishonoredEngine.ini` `[SystemSettings]`, `DishonoredCompat.ini` four `[AppCompatBucketN]` | **The mod ini** since VR-66. The launch file is a mirror, rewritten on a disagreement. The game's own ini is INERT on this build (measured) and is written only for tidiness. `tools\arm-res.ps1` writes all four and is the safe route with the game closed. |
| Everything else in `dishonored_vr.ini` | the installed ini next to the exe; the compiled defaults in `WriteDefaultIni` | **The installed ini, whenever the key is present.** A compiled default only applies to an ABSENT key, so bumping a default does nothing for an existing install. |
| Log verbosity | `[Log]` in the ini, `DVR_LOG`, `DVR_LOG_CATS` | The environment variables, and they apply from the first line - before the ini is read at all. |
| Data directory | `[Paths] DataDir=`, `DVR_DATA_DIR` | The environment variable. On the dev PC both point at `D:\dvr-data` and a tool reading the wrong one finds an empty directory (VERIFICATION gotcha 14). |

---

### VR-76: delivered-pixel tags do not identify the live backbuffer

A tag can be correct for its consumer and wrong for another image. Shared
capture at SharedWait=0 and deferred capture return previous-present pixels
with their own tag. The desktop pin was applying that tag to the current D3D9
backbuffer: it held the right eye and leaked raw left frames after single draws.
Keep current-draw and delivered-texture identities separate. SharedWait=1 is
current delivery, so the mode name alone does not establish latency.

Moving an action to a common-looking tail is insufficient: pairHold returns
early after the first-eye capture, and mirror_present had its own zero-tag
guard. Follow every return and every nested guard. A lifetime snapshot counter
also does not prove a newly recreated surface contains valid pixels. The host
suite exercises these surface lifetime boundaries and reproduces the old leak.

The marker windows overlap and flickers cluster. Raising the 42% baseline rate
to the 37th power assumes independence the sample does not have. Keep the timing
correlation, discard the p-value. See `dishonored/VR-76-CODEX-HANDOFF.md`.

### A save that persists a whole block of defaults, with one wrong literal in it

**2026-09-12. It stopped the weapons tracking the hands, which is the one thing
the original author's rules say must never break.**

`AttachRigRadius` had never been in the installed ini, so it took its compiled
default of 200. An ini save then wrote out the entire `Attach*` block - every key
that had been absent - and all of them landed on their correct defaults except
that one, which was written as **2**. The value 2 belongs to
`AttachHeldMaxPresents` and `AttachRefMaxPresents`, which sit immediately beside
it in the same block.

At 2 the gate asks whether a weapon is within 2 units of the body mesh before it
counts as part of the view model. The crossbow measured 157. So every weapon was
refused as "not a member" and none was moved to the hand, while the HANDS kept
placing normally - `placed` climbed past 34,000 in the same run. A subsystem was
dead and the nearest counter said everything was fine.

Three things made it hard to see, and each is the lesson:

* **The loader clamps the value to a minimum of 10, and the log prints the
  CLAMPED number.** The run said "past the 10 uu rig radius" while the file said
  2. Neither number was the default, and the one in the log was not the one
  written. *Log the requested value beside the effective one, or a reader cannot
  tell a clamp from a setting.*
* **An absent key and a key at its default are not the same thing.** Absent means
  the compiled default applies and a save will materialise it. Once materialised
  it is a value somebody can get wrong, and it beats every compiled default
  afterwards. This is the stale-setting class from section 1 arriving by a new
  route: not an edit in the wrong place, but a SAVE of a place nobody had edited.
* **Offline tests cannot catch it.** 73,822 host checks passed on that build. The
  fault was entirely in a config value, and the geometry maths they exercise was
  correct the whole time.

> **Diff the installed ini against the previous one on every install**, not just
> the keys you meant to change. A save can write keys you never touched, and a
> wrong literal in one of them reads as a broken subsystem rather than as a
> setting.

### VR-57: a constant is not a sample, and other ways a correct value was thrown away

2026-09-12. The model-ray work produced a run of faults that were all the same shape:
the MEASUREMENT was right and the machinery around it discarded, expired or corrupted
it. Each one cost a headset run, and each was found from the tester's own description
rather than from the code.

* **A constant was published as a sample.** The latched palm-frame axis does not depend
  on the pose, so it cannot go stale - but it was stamped with a time and the consumer
  demanded a republish within 250 ms. That republish only happens when a weapon draw
  reaches the measurement code, which is not continuous, so a perfectly valid value
  EXPIRED and the guide vanished until the next shot drew a bolt. *Ask whether a value
  is a reading or a fact. A fact does not need a freshness window, and giving it one
  invents a failure.*
* **A one-shot latch was taken during an animation.** The measurement could land while
  the bolt was being reloaded, when its pose relative to the palm is not the firing
  pose, and that one frame became the session's ray. A latch is only as good as the
  instant it captured: candidates must agree with each other first.
* **Frames are not time.** Five agreeing frames can pass in 50 ms, which a transient
  holds through easily. A stability window needs both a count and a duration.
* **A per-axis bound admits a corner.** "Within 2 metres on each axis" is 3.4 m away in
  the diagonal, for a bolt tip that sits 0.41 m from the palm. Bound a distance as a
  distance.
* **A name arrives empty on the draw that needs it.** The equipped weapon's name is
  resolved from the component table and is EMPTY on the projectile's own draw - the
  draw that measures. Treating empty as "different" discarded the axis every frame:
  408 adoptions in one run, all under weapon `'?'`.
* **Nothing asked whether the projectile belonged to the equipped weapon.** A bolt is
  drawn while the pistol is out, so the crossbow's bolt was measured and stored as the
  pistol's axis and signed against the pistol's forward, mirroring both weapons. The
  log said it plainly: `'bolt_01' axis adopted for weapon 'EliteGun'`.
* **A guard asserted the opposite of the intent it was added for.** A publication-order
  check was written to insist that a model-ray miss must NOT invalidate the ray. When
  the right behaviour turned out to be the reverse - suppress the guide while the axis
  is pending, rather than show a ray that will jump - the guard passed while the
  behaviour was wrong. *A guard that encodes a decision rather than a contract will
  defend the decision after it stops being right.*
* **A refusal named a gate it never reached.** Weapon bodies were reported as failing a
  16:1 variance test when the 1024-vertex size check had rejected them first, which
  sent the investigation after the wrong property. *Say which gate refused, not which
  gate exists.*

> The through-line: **when a measured value behaves intermittently, suspect the
> bookkeeping around it before the measurement.** Three successive fixes here went into
> keying, caching and freshness, and the measurement had been correct since the first
> one.

## 2. Instruments that could not fail their own hypothesis

Every one of these produced a confident number that meant nothing. They are
listed because the failure is always the same: **the instrument was not able to
print the unwelcome answer.**

* **A counter read across two threads.** A 39% "disagreement" figure came from
  reading a bare global written on the game thread from the render thread. It is
  retracted and must not be cited.
* **An angle differenced between two coordinate systems.** A 51-degree error came
  from differencing a UE world yaw against an XR tracking yaw, then doubling it
  with a sign convention. Retracted.
* **A comparison that was circular by construction.** A near-zero error came from
  comparing a camera against the head values that camera was built from. It can
  only ever print zero.
* **A control that never ran.** Three builds reported `passed 0 FAILED 0` because
  the phase counted records opened rather than checks performed, and then sat
  behind a leg that refused.
* **A threshold picked before anything was measured.** The `EVEN CADENCE` test
  forgave `|off| > 0.06`, so a beat every twenty frames printed as a clean bill
  of health - and that clean bill was read as falsification of the cadence
  hypothesis, which turned out to be correct.
* **An average taken across a deliberate switch.** The VR-65 orientation
  difference tracked the switched variable exactly. It was averaged over the
  transitions and reported as "nothing in the log distinguishes the phases".
  **Averaging over a variable you are deliberately switching destroys the signal
  the switch exists to produce.**
* **Two counters that read 0 by design** were read as "the hands are dead" by
  three separate readers, including the original author. A zero that is expected
  must say so on its own line.

### The VR-67 A/B, added the same day it was written (2026-09-09)

The newest instrument joined this list within hours of shipping, which is the
point of keeping the list.

* **The verdict tested p50 and was quoted as covering the tail.** `NO CHANGE`
  compared medians only; the write-up extended it to p99 and the hitch count,
  which were never compared. **The baseline's own hitch share ran 1.68 -> 3.27
  -> 4.99 % inside one run: the tail noise floor is threefold, not the 3 % the
  median's spread suggested.** Neither lever was eliminated on tails.
* **A tail was differenced against a median.** The printed `dP99` compared p99
  against the baseline's *p50*, so its +100..140 % figures measured the shape of
  the distribution, not a change.
* **A third of the plan measured the baseline.** "max frame latency 3" was swept
  against a baseline that already was 3, and nothing checked.
* **The stalls being hunted were excluded from the sample.** Intervals of 5 s or
  more were silently dropped.
* **The sample had no deadline.** Consecutive PRESENT intervals alternate short
  and long by construction under a two-present method, so twice their moving
  median is not a frame deadline.

Corrected: pair intervals, a separate tail floor and hitch floor that must BOTH
be cleared, fixed 40/50/75/100 ms thresholds beside the relative one, severe
stalls counted, baseline-equal segments skipped, and the sweep now ships
**default OFF** so it cannot vary levers underneath another experiment.

> **A "no change" verdict is a claim about the column it tested, and no other.**
> The median and the tail have different noise floors, and the tail's is wider.

### The VR-68 weapon analysis, the next day's write-up (2026-09-09)

Three more, and the first is the most embarrassing because it needed no theory
at all - only the timestamps that were already on the lines being counted.

* **A hitch tally that spanned the wrong population.** "The hitch distribution
  moved, `out` gaps doubled" was computed over the whole log. **Every one of the
  18 `out` gaps happened before gameplay started.** Inside gameplay the mixture
  was identical to the previous run. *Filter to the population before counting,
  and print the population on the line.*
* **Summary windows joined by eye rather than by timestamp.** A GPU span from one
  window was quoted against a frame rate from another, and a third window was
  used as evidence while carrying 40 untagged presents. The window that actually
  mattered - 57/s, 17.5 ms tick, 16.4 ms of elapsed render-thread and Present
  time against a 0.1 ms pacing wait - was never mentioned.
* **A mechanism that named the wrong variable.** The weapon judder was blamed on
  the controller sample being NEWER than the tagged view. It is not: a controller
  drawn into the rendered view and reprojected by that view's own delta comes out
  correct for any sample age. The residual needs the hand to be made head-relative
  to a DIFFERENT head than the view was rendered with. Same family of fault,
  completely different quantity to measure - and the proposed falsification test
  (sweep the global lag) could not have moved the residual at all, because it
  moves the world and the weapon together.

> **A wait, a gap or an error observed somewhere does not name what caused it**,
> and a mechanism that predicts the symptom is not thereby the mechanism. Write
> the algebra before writing the arithmetic.

### The write-up over-claimed too, in three ways worth naming

* **The wrong budget.** The GPU's 14-17 ms per pair was compared against a
  12.5 ms native-80 budget while the target was 40 fps with spacewarp, where the
  budget is 25 ms and it fits. That answered a question nobody asked and made a
  comfortable workload look like the cause of the hitches.
* **"The mean did not rise"** was written directly beneath a quote showing it
  rose from 0.07 to 2.58 ms.
* **"Nothing of ours runs inside `xrEndFrame`"** is false. That call takes a
  mutex, can wait on the previous submission, and can wait on D3D11
  synchronisation - so our own GPU work and resource dependencies can be charged
  to it. `acq 0.0` and `xrCopy 0.0` are CPU submission times and clear none of
  that, because `CopyResource` is asynchronous. **Where a wait is observed does
  not identify who caused it.**

Also corrected the same day: the capture mode was called `deferred` in the
write-up and is `shared`, and `hmd 25.00 ms` is `predictedDisplayPeriod`, which
OpenXR does not require to equal the panel's refresh - so it is not evidence the
panel runs at 40 Hz, and slot-occupancy ratios built on it are not physical.

### And one instrument whose FAILURE was the useful result (VR-68, 2026-09-09)

Worth recording because it is the opposite of everything above. The first
head/view instrument returned a clean zero - generation gap 0 on every frame,
under 0.1 deg in 87 of 100 windows - and it was **near-circular**: both values it
compared derived from `g_hmdYaw` around the same pose consume, so it could only
ever have caught a lane split that does not exist.

That zero was not a dead end. It named the pair that HAD to be compared instead:
fresh against **RENDERED**, not fresh against fresh. The replacement found the
answer at ten times the confidence.

> **A negative from an instrument you understand is worth more than a positive
> from one you do not.** But check for circularity BEFORE the headset run, not
> after - this one cost a run to discover.

The rules that came out of it, all of which are enforced in `CLAUDE.md`:

> A counter is not evidence until you know its population. A measurement carries
> the identity of what it measured. An instrument that cannot fail its own
> hypothesis is not evidence. Name the owner before the result.

### A refusal line whose number can only ever read zero (VR-82, caught in review)

The pistol's fire hook refuses when the bullet's standoff distance would reach
the aim point, and the refusal was written to print the reach that caused it. But
the reach lived in the solution struct, which by contract is only written on
SUCCESS - so the line would have printed `reach=0.00` on every refusal it ever
made, including the one whose whole subject is that number.

It would have looked like evidence. It would have been a constant. The fix is an
explicit out-parameter filled the moment the reach is known, refusal included,
initialised to `-1` for the paths that never compute one, with the log naming what
`-1` means on the line. Caught before the build shipped, but only because the
value was checked against the case it was supposed to explain.

> **If a refusal prints a number, ask what that number reads when the refusal
> fires.** A field that is only populated on the success path is zero on every
> line you will actually read.

### A hook placed before a call that takes the local by address (VR-82)

The pistol's firing routine reads its aim direction from the native cache, and the
obvious hook site is right after that call returns. It is wrong: the direction
local is passed BY ADDRESS to a later call, which can still write it. A hook there
installs, fires, increments its counter, logs a successful write - and changes
nothing, because the engine overwrites the value afterwards.

This is the same shape as the graveyard's other worst entries: the write is real,
the acceptance is not. It was found by listing every write to each local between
the function's entry and the candidate join before choosing the join, which took
one pass over a disassembly the session already had open.

> **Before hooking a stack local, list every write to it up to the join, and
> count `lea`-then-push as a write.** A verified write is not an honoured one.

---

## 3. Plans that were tried and failed

| Plan | Why it failed | Where the detail is |
|---|---|---|
| `setres` on the game console to change the render size | Reaches the engine, returns empty, changes nothing. Inert on this build. | ENGINE_NOTES, "The console seam was dead since 41.0" |
| Writing the size into the game's own ini (both files, all four AppCompat buckets) | The game created a 1920x1080 windowed device on every run and never Reset out of it. The command line is the only route. | ENGINE_NOTES, "The render size" |
| A windowed device at the eye's size | Clamped to the desktop's rows. Every 41.0-era dead end ran windowed; the fullscreen path is the one with no clamp. | ENGINE_NOTES, "The render size" |
| Blaming the submit cadence for the head-turn judder | Falsified directly in a headset: buttery smooth at 61-72 submits/s with an inconsistent presentation rate - the same cadence that had juddered. | ENGINE_NOTES, VR-65 |
| Taking the weapon eye from the drawing pass | Executed zero times in 83,400 draws. The stereo passes run on the game thread and the palette draws on the render thread, so there is no stack to look up. | STATUS, 2026-09-09 |
| The `+0x288` per-bone visibility poke to hide arms | That array is a per-bone animation control; the arms froze to the view and rode the head. Recorded by the original author in a code comment, and missed by three passes over the corpus. | ENGINE_NOTES, VR-31 |
| Gating the bbox readback to cut frame gaps | Cut samples from one per 3 s to 2-3 per run and changed the gap rate not at all. The prediction failed and is recorded as failed. | STATUS, session 15b |
| Blaming the re-entry second draw (or the tick rate it costs) for the intro boat fall (VR-73) | `[Stereo] Armed=0` still fell, at 74-90 ticks/s. The cause was the hand collector clearing the boat's collision. | ENGINE_NOTES, VR-73 |
| Serving a neutral virtual pad when its sample is older than 150 ms, for the boat fall (VR-73) | Still fell. The first fresh sample after the hitch still held A, so the guard itself produced a new jump press; the runs without it show no jump at the seat-in at all. Patch kept outside the tree. | ENGINE_NOTES, VR-73 |
| A name test (`pPlayerMesh`, asset names) as the licence to write a component | Names do not establish ownership; the pointer walk reaches world meshes (the intro boat, doors, props). Read `ActorComponent.Owner`. | ENGINE_NOTES, VR-73 |
| A Vulkan translation layer (the DXVK fork) | Removed in 41.0. The game renders natively through D3D9; do not bring it back. Git history keeps it under the `dxvk-*` tags. | CLAUDE.md |

---

## 4. The per-topic graveyards

| File | What is buried there |
|---|---|
| `docs/dishonored/BRIEF-eye-flicker.md` | Four hypotheses for the eye flicker, argued and killed. ANSWERED; kept as the record. |
| `docs/dishonored/VR-33-HANDS-AND-WEAPONS.md` section 8 | Every approach to the hands and held weapons that cost a headset run. |
| `docs/dishonored/HANDOFF-GINGASVR.md` "Traps and Dead ends" | The original author's own list, each item paid for in their 39.x builds. Read it before writing code that touches what they touched. |
| `docs/dishonored/DESKTOP_MIRROR.md` | The counter reading that was retracted, and why the eye pin is not in the runtime layer. |
| `docs/ARCHITECTURE.md` decision log | Why each non-obvious choice was made, dated. |
| `docs/CODE_REVIEW.md` | Every finding from the review of the original single file, with its disposition. |

### VR-57: an existing visual API can still violate the one-ray contract

The legacy laser fetches its own pose and trims; the aim-dot API accepts a final
point. Calling both does not make them consumers of identical ray data. The new
guide publishes explicit endpoint/beam points from one immutable ray. Original
sample age must also travel with that publication, or repeated publishes keep a
stale pose falsely fresh.

The existing visual block precedes held-layer recovery. A dot wired only there
would disappear on single-draw holds. Submit opportunity, pair-open deferral,
actual projection layer, built quad and successful xrEndFrame are distinct
populations. Log each, and never infer visibility from a publish counter alone.
A grip/aim angle near zero is not proof of a runtime bug; a hand correction
matrix is not a calibrated barrel direction. See the VR-57 implementation review.

### VR-57: do the geometry before the headset run, not after it

Build 111 shipped a head-anchored control dot at TWO distances on one ray from
the view midpoint, with the written prediction that they would appear concentric
and that a separation would mean the dots and the compositor disagreed about
where the head IS. **That prediction was geometrically impossible.** Two points
at different depths on a CYCLOPEAN ray cannot project to the same point in either
eye: each eye is offset laterally, so the nearer point is displaced outward by
`atan(ipd/2 / d)`. At the measured 63.2 mm IPD that is 1.21 deg at 1.5 m against
0.23 deg at 8 m - a 1.0 deg split, right of the far dot in the left eye and left
of it in the right, which is precisely what the headset showed and what was
briefly read as a finding.

It cost nothing only because the FAR dot answered the question on its own. The
rule it belongs to is already in this file, in a different costume: an instrument
whose predicted outcomes have not been worked through cannot distinguish the
answers it claims to. **Write the arithmetic for every branch of the prediction
table before the run, including the branches you expect not to take.**

The same run also printed `TAG vs LOCATED: worst 180.00 deg` on every present
where the layer was a quad rather than a projection. That is an uninitialised
quaternion, not a measurement - the projection views are only filled on the
projection path. A number printed outside the population it describes is still a
number, and it reads as a catastrophic finding. The line now refuses instead.
