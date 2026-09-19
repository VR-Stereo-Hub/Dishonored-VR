## Watch the pair rate, not the tick mean (2026-09-19)

A judder report was nearly dismissed because the obvious number barely moved.
Mean `perf: tick` went 9.41 -> 10.70 ms between the smooth build and the one the
tester called laggy: 13%, easy to wave off as scene difference. The stereo pair
rate over the same two runs went from a median of 109/s to 45/s - a 59% loss,
and the thing the headset actually experiences.

The tick mean is averaged over 3 s windows that include menus, loads and
cutscenes, so it dilutes a gameplay collapse. `stereo: beat ... L/s` counts the
unique pairs submitted and does not.

**When someone reports judder at a high reported framerate, read
`stereo: beat` L/s first**, and read its DISTRIBUTION rather than its mean - p25
and median separate a steady low rate from an occasional dip, and those are
different faults.

## A cap that stops remembering must also stop logging (2026-09-19)

`pcap/layout` kept a sixteen-entry table of shaders it had already named:

    if (saidN < 16) said[saidN++] = key;
    Log("pcap/layout: shader %p declares ...");

Past sixteen it stopped recording but kept printing, so every later shader
logged on EVERY DRAW - 77992 lines in one run, five-argument formats plus file
I/O on the render thread. The comment immediately above it explains that the
table exists because an earlier version "produced a 25 MB log in a single short
run". The lesson had been learned and the guard still leaked.

**When a bounded table fills, the branch that writes to it and the branch that
acts on it must end together.** Say once that the bound was reached, then go
quiet. Check every `if (n < CAP)` for what happens on the else.

## Hoisting a block above the early returns inside a function does not help if the function itself is below one (2026-09-19)

`ApplyHandToMeshInner` opens with a comment from 30.95: the SkelControl probe and
the Blink latch "run FIRST, above every early return", because burying them meant
"five separate ways for them to silently never execute". That hoist was correct
and it was not enough. `ApplyHandToMeshInner` is the LAST call in its caller
`ApplyHandToMesh`, under three early returns - including the crawl tuck's
`if (t) return;`. The block was at the top of a function that was at the bottom.

Cost: two separate user-visible faults, reported weeks apart and investigated as
if unrelated.

- Blink aiming with the engine's head vector after loading a crouched save, which
  "fixed itself" if you switched power and back (anything that released the tuck
  let the latch run).
- A two-to-three second freeze on the first stand-up after a load, measured at
  3975 ms of script-lane time in a 4000 ms window - every deferred discovery step
  firing at once the moment the tuck released.

**When a block must always run, check the whole call chain, not the function it
lives in.** The guard that skips it may be one frame up. The lesson generalises
to `PawnCollisionTick`, which already carries the right instinct in its own
comment - "load liveness must not wait for a pawn event or head/hand drive" - and
is called straight from `PeHandler` for exactly that reason.

## A latch that logs only its successes cannot report being dead (2026-09-19)

Blink went back to head aim and the log had **no `[blink]` lines at all** - not
one, in a whole run. That is not a quiet fault, it is an invisible one, and it
sent a session looking at the aim maths and at settings before anyone noticed
the category was empty.

The chain: `BlinkLatch` logs when it FINDS the player `PowerBlink` and logs
nothing when a full sweep of GObjects finds none. `blinkdst` and `blinkdir` only
install once the latch exists, so they cannot speak either. A run whose sweep
never succeeds therefore produces exactly as much `[blink]` text as a run where
the feature was never compiled in: zero.

Two different states were behind that same silence, and they need opposite
responses: **the power does not exist in this level yet** (Blink is granted by
the mark, so an early save legitimately has none, and there is nothing to fix),
versus **the object is there and our liveness walk is rejecting it** (a real
fault). The log could not tell them apart because it counted only successes.

The general rule this project already has - an instrument that cannot fail its
own hypothesis is not evidence - has a corollary: **a latch must report the
sweep that found nothing, with the population it examined.** The fruitless sweep
now warns once and then every 30 s with how many slots it scanned, how many were
the right class at all, and how many of those the liveness walk refused, and the
line says in words which reading is a fault and which is not.

Cost of not having it: two runs and a headset session spent diffing settings
that turned out byte-identical between the working and broken runs.

## A GObjects slot that still holds the pointer is not a live object (2026-09-19)

Blink aimed with the engine's own HEAD vector for 23.8 seconds after a save load
in run 512, and the tester saw it persist across the reload rather than being
cleared by it. The cause was not the aim code: `BlinkDirHook` refuses any call
whose `self` is not the latched player `PowerBlink`, and the latch was stale.

`BlkAlive` tested three things - the GObjects slot still holds the same pointer,
the index is in range, and the class pointer is unchanged - and all three survive
a save load, because UE3 leaves a destroyed UObject in its slot until the
collector sweeps. So `BlinkLatch` saw a live latch and returned at once while the
game's real Blink was a NEW object. It healed on its own when the collector
finally ran, which is exactly the shape that reads as "it came back eventually"
and sends the next session looking at the aim maths.

The class of trap: **identity is not liveness.** CLAUDE.md already says
`IsLiveObject` is the only valid liveness test and that a class-name comparison
catches reuse but not freeing; a class POINTER comparison does not catch it
either. Any latch onto an engine object needs something that changes when the
level does. The fix ties the Blink latch to the player pawn it was taken under -
`PeLatch` already notices a new pawn, and already clears the cinematic latch for
the same reason - and logs the drop with both pawn pointers.

What made it expensive to see: `blinkdir: 943 calls (0 ours) | ray ready 0,
refused 0`. Neither the ready nor the refused counter moved, because the return
happens before the ray is ever asked for, so the only evidence that anything was
wrong was the parenthesised `(0 ours)` against a healthy-looking call count. The
counters could not fail their own hypothesis. `blink: dropping the PowerBlink
latch` now names the cause on the transition.

## Cancellable action does not imply native pose ownership (2026-09-17)

Build435 enabled native poses for every cancellable state, including generic upper/
left Action states used by movement transitions. This caused repeated hand-control
handoffs around jumps/landings despite movement Arms.* choices being off. The build
was rejected and its complete pose/split changes reverted. Keep input/action
eligibility separate from pose policy; test movement integration, not only helpers.

## Arm visibility and animation ownership are different (2026-09-17)

Build433 Arms.* checkboxes selected native pose ownership as well as full-arm drawing.
Unchecked mantle therefore kept the native FSM action but overrode its hand animation
with controller placement. Separate pose classification from mesh visibility. Hidden
arms must use the clipped hand mesh with native bone constants, not controller poses.
See ANIM-HANDOFF-PLAN.md for verified log evidence and the corrective candidate.

# Traps and the graveyard

## Reading attachment is a relative pose, not an opening pitch fit (2026-09-17)

425 logged successful entry pitch fits but still rebuilt the grip reference on
every opening. A height-based pitch does not preserve a preferred hand-relative
attachment. The final full reading pose now supplies that reference; see
HUD_ANCHORS.md. Do not recapture it from each opening head/hand orientation.

## Native HUD comparison is deliberately stock, not a reset (2026-09-16)

Build397's NativeGameplayReference=1 bypassed the tuned gameplay panels for the
objective comparison. The tester could not locate the toggle because the launch
instructions omitted F10's HUD tab, and the full source HUD was at the FOV edges.
345 native-reference samples with no toggle establish that reference ran, not an
ON/OFF comparison. Restore0 in the next installed INI; expose a prominent restore
button in the HUD tab. Do not discard the saved placement values or interpret
this report as a panel configuration reset. Rotation reported in the native path
weakens capture-only attribution but does not identify the camera error.


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

### VR-117: two setup traps found while making a dev PC match the tested profile (2026-09-14)

- **The host test suites could not run on a Build Tools machine.** Every `tools/*-host.ps1`
  globbed `C:\Program Files\Microsoft Visual Studio`, where a full Visual Studio installs;
  the VS 2022 Build Tools live under Program Files (x86) and every suite threw before
  compiling. `tools/lib/msvc.ps1` asks vswhere first (what `build.ps1` always did). A suite
  that has never run on your machine has never passed on it: run it once before trusting
  the green in STATUS.
- **`setup-game-ini.ps1 -VRBaseline` stopped at a key this machine's `DishonoredInput.ini`
  did not have** (`bEnableMouseSmoothing` under `[Engine.PlayerInput]`), after it had already
  written the three engine values. The script now appends a missing key at the end of its
  section instead of throwing, and says so. A game ini is not the shape the script expects
  on every machine; read the "already set" / "->" lines, not the exit code.
- **The mod ini is never refreshed by the mod** (`kConfigVersion` stayed at 11 since VR-11), so
  an ini next to the exe from an older build keeps its Version-10 values forever, including
  a `[VR] XrRuntimeJson` pointed at the simulator. "Identical to the tested profile" means
  a byte copy of `release/dishonored_vr.ini`, checked by SHA256, then `arm-res.ps1 -Status`
  for the four resolution places.

### VR-118/VR-120: three traps in the HUD element work (2026-09-15)

- **A shadow reader that wrapped its index.** VR-117's vertex-constant shadow held c0..c3
  and its reader did `row & 3`, so a caller asking for c6 read c2 and could not know it. The
  transform it looked for lived in c6..c9; every HUD draw read the same stale values and
  the rectangles were nonsense. A reader that cannot say "outside my range" returns a wrong
  answer with a straight face: it now returns null, and the probe refuses with a reason.
  The register numbers themselves are never hard-coded: they are parsed from each shader's
  own disassembly (`draws vsdump`).
- **The census recorded the pixel shader and not the vertex shader**, which hid the very
  thing that decided the transform (whether one was bound at all, and which). The fixed-
  function hypothesis had to be built as an instrument (the SetTransform hook and a
  per-bucket `vs=`) before it could be killed in one window: `HUD draws with a vertex
  shader 8862, without 0; SetTransform calls 0`. Record what the hypothesis space needs,
  not what the first hypothesis needed.
- **Lazy sinks and an armed-only router deadlock.** The element table acquires a sink on
  the first draw routed to it; a draw is routed only while the redirect is armed; the
  redirect armed only when a sink's hand-off was ready. The first table build never armed
  and the log said so in one line (`hud/beat: ... (no sink in use) ... handoff=0`). The
  hand-off now counts as ready on the blit alone while nothing is in use. When two lazy
  things wait on each other, one of them has to be allowed to go first.
- **The screen offset is not the hand's.** Placing a cropped element on the hand by its
  screen-relative offset (as the window does) put the vitals 0.16 m up-left of the grip at
  0.044 m wide: a fraction of a 0.22 m panel scaled by a fraction of the screen. A wrist HUD
  fills the panel at the hand; the sim's pose numbers said so before a headset run did.

### VR-122: a lever with one purpose switched off a control with another (2026-09-16)

The crawl tuck (38.19) exists to hand the ARMS back to the game's crouch animation: on a
crouch it wrote `ControlStrength=0` to "our hand controls", and the list it walked held
three player look-at controls, of which slot 0 is `LookAtControl_Camera`, the camera's own
bone control (ENGINE_NOTES VR-30: "do not zero it"). Crouched, the rendered view stopped
pitching with the head, and under a projection layer that reads as the world moving with
the head. It had done this on every crouch since the list gained the camera slot, and the
log line said `wrote 3 validated controls` without naming them. Two rules from it:

- **A writer that walks a list must say what is on the list.** The line now names what it
  did to the camera control. A count of writes is not evidence of which object was written.
- **A measurement made under a mod lever describes the game WITH that lever.** VR-78
  measured "no crouched neck arc" while this write had the camera control at 0, so its
  crouched pivot (0/0) is right for that camera and wrong for the stock one (0.291/0.052 m,
  the standing neck). Before fitting an engine constant, list the mod's own writes that were
  live on the object being measured.

Eliminated on the way, each with the counterprediction that killed it (ENGINE_NOTES
"Crouched pitch on two machines"): the crouched neck term (`neck off` changed the crouched
capture by 2.6 mean-abs, noise), the eye ceiling (the cap trims about 5 uu in both stances
on both machines and the crouched episodes clipped 0 presents), the cinematic pitch and
head-look scopes (`cinepitch off`, `cinehead off`: no change). The hypothesis that survived
was the fourth, found by A/B and not by argument: `hands off` before the crouch, 63.2.

### The console-opened level's "press any key" board never took a key on this build (2026-09-16)

`console open L_PrsnSewer_P` from the MAIN menu, on the simulator, with the RelWithDebInfo
build 287 and later 294, left the Dunwall Sewers loading board up for over 15 minutes on
four launches: Return, Space, letters, virtual-key Return, mouse clicks, pad A/B/X/Start,
stick and trigger, with the game window in the foreground, `pad: xbtn=` confirming the pad
composed the buttons, and the level's pawn live behind it. The mod's movie-completion
observer read `finished=0` throughout; on the Debug run that had worked (vr120-run8) it
flipped to `finished=1` 37 s after the open with no key logged. Not diagnosed: it is not
this ticket's fault, and the loading board is a separate question. **What worked**: the
saved-game route (main menu, Return on Continue, Return again on the confirmation, ~30 s,
one Return on the board), which reached GAMEPLAY in one try, twice. The newest save on this
PC is at the Hound Pits pub, not the intro boat.

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

### A liveness guard that could not detect the thing it existed to catch (VR-85)

`InteractFocus` held the last actor the engine had focused and wrote it back
while the engine had none. It crashed the game twice - once loading a save, once
on pause immediately after a pickup.

The guard was a class-name comparison: remember the actor's class, re-read it
every tick, and drop the pointer if it changed. That catches an allocation being
REUSED. It cannot catch an allocation being FREED, because freed memory still
holds a plausible class pointer until something else claims it. So the guard
passed on a dangling pointer and handed it back to the engine about ninety times
a second.

What makes it worse than an ordinary oversight: the actor being held was a
crossbow bolt, and **the action the experiment existed to test - picking the bolt
up - is the action that destroys it.** The success path of the experiment was
also the path that guaranteed a dangling pointer. That should have been visible
before the build shipped, from the description of the test alone.

The project already had the right tool and it was passed over as too expensive:
`IsLiveObject`, a binary search against the GObjects set. Cost is a reason to
sample less often, not a reason to substitute a weaker check.

> **A guard has to be able to detect the failure it is named for.** "Is this
> pointer still valid" is not answered by reading through it. And when the
> experiment's own success path destroys the thing it holds, the experiment is
> the bug.

The run was still worth it: the beat line read `wrote 1, survived to the next
tick 0`, which settled the design question outright. The engine recomputes that
field after us every tick, so writing it can never work, and the fix has to go at
its writer. A negative result that arrives with its own evidence is a result.

### A read-only probe that cost the frame budget (VR-85)

`PropWatch` writes nothing and reads 46 dwords. It still produced visible world
jitter on its first headset run, because it validated its OWNER once per
PROPERTY rather than once per tick: the same two objects went through
`LooksLikeObj` - a readable check, a class-name fetch and a string scan - forty
odd times per script tick, and it ran every tick for a value that changes on
gameplay timescales.

The log named it exactly, which is the only good part of the story. The perf line
read `RENDER THREAD STARVED: idle > 30 % of OUT, the game thread is the limiter`,
and the game thread is the script lane, which is where the probe ticks. The
tester's report and that line agree.

Fixed by validating each owner once per tick, resolving the owner as an index at
build time instead of a `strcmp` per slot, and throttling the sample to 50 ms.

> **"Read-only" is not "free".** A probe that changes the thing it measures is
> not a probe. Cost a diagnostic per TICK, not per item, and throttle it to the
> timescale of the thing being watched.

### The live-object table is a snapshot from the main menu (VR-88)

`IsLiveObject` binary-searches a sorted copy of GObjects that only the player controller
scan rebuilds, and in a normal run that scan happens once, at the main menu. The level's
pawn, its state machines and their states are created after it and are absent from the
table for the whole level. The VR-88 reader validated every pointer against it, so on a
normal run it would have read UNKNOWN all session and handed nothing back, with nothing
visibly wrong. The first playtest could not show it, because that playtest ran the
previous installed build: the log banner, not the report, said so.

The table was also rebuilt on the present thread while the script lane searched it, with
no lock. It is now SRW-locked, and the animation reader rebuilds it when a plausible
object is missing, at most once a second.

> **`IsLiveObject` answers "was this alive when the table was built".** Code that checks
> objects created by a level load must rebuild or refresh the table first. And before
> reading a playtest, check the log banner names the build under test.

### A "fresh sample" test that no real sample could pass (VR-78)

The accounting probe fitted the engine's neck only from bases the writer read FRESH, and
excluded any base recovered from its own previous write as contaminated. In game every
base was recovered, in both stances, because the writer runs on every script dispatch
and the tick's last call always finds the earlier call's write. The fit printed
`NO_FRESH_SAMPLES ... the clamp or the writer owns it` for the whole run - a line that
blamed an owner for what was really the filter's own population. The synthetic tests
had passed because they wrote one fresh value per tick, which the game never does.

> **Before trusting a filter's empty result, count what it rejected on real data.** A
> test population built to match the model is not the population the game produces.

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

### The golden ini check compared the golden against itself (VR-84)

`tools/ini-golden.py --check FILE` is the gate that catches a `WriteDefaultIni`
literal edited without a regenerated golden. Its own docstring says so. It could
not do it: the check read `want` from the golden FILE rather than from
`extract()`, so pointing it at the golden compared that file to itself and
printed `MATCH` unconditionally. Every "golden ini: MATCH" in this project's
history up to 2026-09-12 carried no information.

It was found only because a second bug in the same script - `%%` never being
unescaped, so the golden said `%%LOCALAPPDATA%%` where the runtime writes
`%LOCALAPPDATA%` - produced a visible diff when the file was regenerated. A
passing check had been hiding a golden that did not match the source.

The fix compares against the source literal. Verified the way it should have been
from the start: corrupt the golden, confirm the check FAILS and prints the diff,
restore, confirm it passes.

> **A checker that reads its expected value from the artifact it is checking is
> not a checker.** Prove a gate can fail before trusting that it passed.

### An ini save wrote one key's value into another key (VR-84)

Weapons silently stopped tracking the hands after an ini save, once before and
again on 2026-09-12. The cause is not subtle once seen, and it is in the save:

```c
_snprintf(v, 64, "%.0f", g_waRigRadiusUU);   // formats 200 into v
WritePrivateProfileStringA(..., "AttachEquippedMembers", "1", ini);   // does not use v
...  17 lines ...
_snprintf(v, 64, "%d", g_waRefPresents);     // OVERWRITES v with 2
WritePrivateProfileStringA(..., "AttachRefMaxPresents", v, ini);
WritePrivateProfileStringA(..., "AttachRigRadius",      v, ini);      // writes 2
```

The format call for `AttachRigRadius` had drifted seventeen lines from its write
and sat beside a write that does not take `v` at all, so the value that reached
the file was whatever the shared buffer last held. Every save wrote
`AttachRefMaxPresents` into `AttachRigRadius`.

What made it expensive is the symptom. 2 clamps up to the minimum of 10 on the
next load, and at a 10 uu rig radius every weapon is refused as not being on the
view model while the hands keep placing normally - so "weapons stop tracking"
arrives with every nearby counter healthy. The log prints the CLAMPED 10 while
the file says 2 and the default is 200, so all three readings disagree and none
of them names the key that was actually wrong.

The whole save function was then scanned mechanically for the same shape - a
`WritePrivateProfileStringA(..., v, ini)` with no `_snprintf` into `v`
immediately before it. This was the only one.

> **A shared format buffer is a shared variable.** Format immediately before the
> write, and when a value is wrong in a file nobody edited, suspect the writer
> before the reader.

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

### An input learned from our own output, which could only ever ratchet up (VR-36)

`g_blkReachSeen` held "the furthest a blink has been seen to go", learned from the Blink
destination seam. That was sound while the mod only watched. The moment the source seam
started driving the aim, the destination it observed was the result of the mod's own
vector - so the input became a function of the previous output, with no term anywhere
that could lower it.

Measured in one run: `reach 1100 -> 1839 -> 2007 -> 2062 -> 2610 -> 4698 -> 5606 uu`,
monotonic, the blink getting longer every time it was used. The tester reported it as
"the distance is unlimited now"; the ratchet is visible in the log line as plain
arithmetic, which is the only reason it took one run rather than a session.

> **A quantity learned from a seam the mod also writes is not a measurement of the game.**
> When a read-only observer becomes a writer, every statistic it was feeding has to be
> re-asked: is this still an input, or is it now my own output coming back?

The second half of the same fault is worth its own line. The mod had replaced a magnitude
the engine authored - and that magnitude was carrying a rule nobody had noticed, the
vertical cap on Blink (ENGINE_NOTES). **Substituting a value you did not derive discards
whatever it encoded**, silently, and the symptom appears somewhere else entirely. The fix
is structural: the reach curve is now clamped so it can only ever shorten what the engine
offered, which makes the class of fault unreachable rather than fixing this instance.

### An identity check whose "unique" field is the same after a respawn (VR-93)

The menu retention validated each retained object as live, same class, same FName, on the
theory that a respawned actor gets a new FName number and so a reused address would fail.
The save-load run measured the opposite: the old pawn and the new one both read FName
`10783_0`. The load was caught only because the new pawn landed at a different address
and the pawn-changed tripwire fired. A load that reused the address would have passed.
The host test had passed too, because it asserted the theory with a fake table instead of
checking it against the game.

> **A field is only an identity discriminator once a real replacement has been seen to
> change it.** Until then it is a guess with a test around it. Prefer the game's own
> event for the transition you fear (here `LoadGameClicked`) over any property of an
> object that the transition recreates.

### A crash dump that holds none of the memory the crash was about (VR-96)

The unhandled-exception filter wrote a 44 MB minidump with indirectly referenced memory,
but it wrote it later, on a different thread, not in the faulting thread's context. It
holds no GObjects array and not the region the garbage collector faulted on, so the
object holding the bad value could not be named offline. Read the crash registers from
`dishonored_vr_crash.txt`, and do not expect the dump to answer an object question.

The September 13 pause recurrence confirmed the same limit: its main log retained
first-fault registers but the dump, written 125.672 s later, contained a different
exception and omitted the bad reference's memory. `[Diagnostics] GcFaultDump=1`
now captures full memory at the byte-verified first AV/read site, before normal
fingerprinting. The standalone production-handler test verifies context and heap
retention. This is diagnostic readiness, not a game-crash fix; see ENGINE_NOTES.

### Aggregate counters that read as a self-sustaining loop (VR-80)

Across three instrumented runs the after-note flicker's counters (realigns, untagged-branch
entries, TOOKs) rose together, and they read naturally as a drain that over-consumes the ring
and keeps re-triggering itself. The plan predicted exactly that. The host model, compiling the
shipped pairing, could not reproduce any self-sustaining loop from a single fault, and the first
per-present ledger with draw ids showed the drain removing the RIGHT tag every time. The fault was
a recurring onset: a present showing a draw's image under a millisecond before that draw's tag
reached the ring. The counters could not tell onset from repair because both move them.

> **When counters rise together, measure one event end to end before naming a loop.** Give
> every item an identity (a draw id) and record what each step removed. And under a pipelined
> capture (`SharedWait=0`) a corrected label is not a corrected image: the present delivers the
> previous slot, so check the delivered column too (the first repair moved the flicker to the
> left eye for exactly this reason).

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
| Blaming the three-disagreement drain for the sustained after-note flicker (VR-80) | The host model found no self-sustaining drain loop, and the headset ledger showed the drain removing the correct tag. The onset was a late tag. | FLICKER_REFERENCE 3.15 |
| Explaining the crouched pitch report by the neck term, the eye ceiling or a cinematic scope (VR-122) | Each was A/B'd live on the simulator and changed the crouched capture by noise (1.8 to 2.6 mean-abs); `hands off` changed it by 63. The owner was the crawl tuck zeroing the camera's look-at control, a hands lever. | ENGINE_NOTES, "Crouched pitch on two machines" |
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


### Readable recycled hand controls were live upgrade objects (2026-09-13)

A crawl-release wrapper wrote floats through old rig pointers before its inner
function checked their lifetime. Reload had reused all three addresses for
upgrade objects, so readable memory did not mean a valid write destination.
Fresh IsLiveObject membership alone would still accept the new live owners.
Require current liveness AND the retained index/class identity at the writer,
including release/restore paths outside the main drive. The bad stores happened
after reload; GC exposed them later on pause. See ENGINE_NOTES's pause GC record.

The 32-bit full dump also sign-extends virtual addresses above 0x80000000 into
64-bit descriptor fields. Normalize to 32 bits when looking up captured memory;
otherwise live controls above 2 GB appear missing and lead to false stale-object
conclusions. Do not treat a parser lookup failure as proof of absent dump data.

### VR-70: a single scene draw is not a cinematic exit (2026-09-13)

Candidate219 cleared the HMD reference whenever stereo decided not to double a
draw.32 pacing interruptions became32 gaze reanchors,despite every camera store
restoring correctly. Separate draw eligibility from reference lifetime. Hold
through pacing/runtime/pose gaps; reset for actual ownership/menu transitions.
The regression changes both yaw and pitch across32 holds and must retain one
reference. Full evidence:CINEMATIC_HEAD_TRACKING.md under docs/dishonored.

## 2026-09-14: an old completed query does not submit new D3D9 work

VR-115's first mirror-off candidate polled a pending event on later frames. Once
the older event completed, GetData(FLUSH) could return success without submitting
new commands. A real D3D9Ex host test caught frame2's independent marker remaining
pending for two seconds. Issue END covering the current stream before its FLUSH
request; the submission-only event may abandon its old result. Never do that to
a fence whose completion authorizes texture reuse. A device double alone missed
this driver behavior. Corrected native test passes120 frames without Present;
details: [desktop candidate](dishonored/DESKTOP_PRESENT_PERFORMANCE.md).

## HUD grouping ownership must be retained (2026-09-16)

The382 proximity group changed the current route but did not update the draw's
cached owner. A later larger move could restore its initial unrelated row and
lose the group's only seed. Adopt only unambiguous matched content, then let it
reseed the neighborhood. Small size plus central position also does not identify
the reticle: moving10-primitive button draws were eligible for that exclusion.
Measured centered two-primitive reticle protection is narrower. These corrections
do not establish semantic identity for animated/rebatched UI. See HUD_ANCHORS.

## Native objective controls and incomplete Flash previews (2026-09-16)

NativeObjectiveIcons=1 returns recognized objectives to the game frame, bypassing
Element.objective anchor/WinX/WinY/WinScale. Editing those controls cannot affect
that path. Use NativeObjectiveScale; F10 now shows the active controls and explains
the inactive panel settings. Do not interpret unchanged panel sliders as proof of
wrong icon identity without following the native consumer.

An exported GFX authoring frame is not the runtime HUD. Wheel assets span its own
package, Startup's imported lib movie, and engine-loaded equipment icon textures.
Resolving only the wheel's adjacent TGA files misses imports; resolving the library
still misses native req_EquipmentIconImage execution. Use the full dependency
export and inspect individual runtime textures; do not present a static frame as a
complete populated wheel or assume more geometry is inside the wheel movie.
