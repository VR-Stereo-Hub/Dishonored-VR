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
