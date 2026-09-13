# VR-80: sustained eye flicker after note close while crouched - revised analysis plan

Status: **OPEN; analysis and measurement plan, no renderer fix implemented.**
Revised 2026-09-13 against `c4084478` on `claude/vr-80-note-exit-eye-trace`.
Read the latest [flicker master reference](FLICKER_REFERENCE.md), especially sections
2, 3.1-3.4, 3.8-3.9, 3.11, 3.13-3.15, 4.2-4.3 and 6, before implementing this plan.
The master reference's opening baseline hash predates its appended investigations;
this review includes the updates through `c4084478`.

## 1. Objective and evidence boundaries

Identify the first loss of association between a queued draw, its tag, captured pixels,
and submitted eye after note close. Explain separately what starts the skew, what
sustains it, and what permits recovery. Do not choose a repair from aggregate counters.

The tester reports both-eye world/hand/weapon flicker after note close with a weapon
drawn: sustained while crouched until pause/resume, brief while standing. This is
separate from VR-93 weapon relearning, VR-95's one-eye roll classifier problem,
VR-97's unavailable-c5 case, and VR-69's already-corrected downward camera ownership.
VR-80 predates note retention; retention is not necessary for the recorded signature.

**Leading investigation:** a feedback loop involving tag consumption and realignment.
The observed correlation justifies tracing it first; it does not prove the drain
removes a future draw's tag. Queue serials alone will not establish rendered identity.

This revision reads source and saved run-4 log excerpts directly. Four build headers
were checked. Earlier census totals below are attributed to the original investigation
in master section 3.15, not presented as a fresh full-log recomputation.

## 2. Preserved reproduction and artifacts

Keep the tested Quest/VirtualDesktopXR 1.0.10 profile: 90 Hz, 2750x2850,
`Method=reentry`, shared capture with `SharedWait=0`, `[Pace] Lag=2`,
`[Stereo] LagAB=0`, `[Hands] PoseLag=2 PaletteEyeOffset=1 ModelScale=0.85`,
`[Neck] Mode=cancel CrouchPivotBelowM=0 CrouchPivotBehindM=0 RollArc=0`,
`[PosTrack] ZAccount=0`. Preserve installed menu/note retention, mirror, hold,
and palette settings; record resolved values instead of replacing the ini.

Local ignored artifacts, each containing `dishonored_vr.log`, previous log and ini:

| Run | Directory under `build/vr93-logs/` | Build header | Contribution |
|---|---|---|---|
| 1 | `vr80-run1-142351` | `vr33-hands-working-196-ga9e44e60-dirty`, 14:11:40 | Writer/tag/c5 trace |
| 2 | `vr80-run2-143606` | `vr33-hands-working-199-ga7e7dde8`, 14:34:20 | Three other static draw callers inactive |
| 3 | `vr80-run3-144403` | `vr33-hands-working-201-g5cb3e715`, 14:39:18 | One observed Present return address |
| 4 | `vr80-run4-145350` | `vr33-hands-working-202-g2d8e9374`, 14:47:03 | Device work, Present arguments, crouch episode |

Related controls: `launch3-133430` (build 192, old note-drop path and zero-c5
periods) and `launch4-135857` (build 193, retained notes and skew with c5 present).
For new measurements preserve DLL/config/log hashes and dirty patch identity before
rotation. A source hash in a dirty build header is not complete build provenance.

## 3. Evidence and corrections to the original plan

The original investigation reports healthy writer-to-c5 distances of 0.00-0.03 uu,
versus approximately 6.82 uu during skew. Tagged records carry the expected writer
eye and no repeated-write signature. This weakens the stale-camera-write hypothesis
in the sampled population; it does not independently identify every rendered draw.
It reports 17 episode starts preceded by an untagged record (14) or a small-step
record labelled repeat (3). A small camera step alone does not prove repeated pixels.

Build 202's sampled untagged intervals contain scene-scale work: 450-910 draw calls,
2 BeginScene, 51-65 SetRenderTarget and 23-34 c5 uploads. This contradicts a simple
no-render buffer re-show. Similar workload does not establish identical pixels,
one viewport invocation per Present, or the identity of the owning queued draw.
The same Present caller (`009c01a4`) and null rect/window/dirty arguments were
observed. These constrain the path; they do not prove one logical view or target
resource. Zero calls at the three instrumented static sites excludes those sites
in that run, not every possible nested or queued render path.

### Run 4: timestamps checked in the saved log

Times below are log milliseconds converted to seconds, not wall-clock times.

| Time | Observation / consequence |
|---|---|
| 5083.093 | `neck: stance -> CROUCHED` |
| 5085.375 | Beat `draws/s=85 2nd/s=83 presents/s=169`; this window overlaps the stance change and note opening. It is **not a standing baseline** |
| 5086.765 | Note-visible bit falls; do not equate this with GAMEPLAY returning |
| 5087.750 / 5087.765 | DOUBLE resumes after 141 single ticks / state becomes GAMEPLAY |
| 5087.796 | Counters: same-eye 21, took 13, held 7, realigned 6, untagged 60 |
| 5091.406 / 5094.406 | Beats 76/76/152 and 74/74/148; stall counter 51 |
| 5096.265 onward | SINGLE due to present-progress guard, followed by single-to-double transitions |
| 5097.406 | Beat 73/71/144; stall counter now 55 |
| 5098.968 | Counters: same-eye 224, took 145, held 72, realigned 71, untagged 129 |
| 5099.453 | Pause opens; tester reports pause/resume clears the episode |

Thus the sampled counter interval contains **65 additional realign attempts**, not
71 new attempts, and **69 additional c5-untagged-branch entries**. The latter are
not 69 proven empty-ring Presents. That branch includes zero tags and failed pops;
a failed pop can mean a depth clear as well as an empty ring. Single ticks did occur.
The original claim of no single ticks across the entire episode is retracted.

The printed realign samples have `along=-6.82 other=0.00`, but the message is
rate-limited to 3000 ms. It does not report every realign's geometry or removals.
`realigned` counts entry to the repair, even when nothing is removed. The message
"by one pop" can describe multiple removals, and "the ring was empty" can also
mean the peek already found the desired next eye. Count operations directly.
No depth-clear warning was found in the run-4 log; this is not a full mutation ledger.

The source's `draws/s` means first/root draws (roughly ticks), with `2nd/s` counted
separately. Compare Presents with their sum using aligned windows and queue boundary
occupancy. Neither reduced tick rate nor these mixed-window means proves that
crouching changes GPU cost or producer lead. Re-select clean standing and crouched
windows before making that comparison. The standing episodes of one/two realigns
remain prior reported observations, not a newly recomputed control here.

## 4. Actual pairing path and audit targets

| Stage | Source / behavior relevant to this investigation |
|---|---|
| Producer | `src/game/dishonored/scene_draw.cpp`: `SceneDrawDecide`, `DvrViewportDrawStub`, `SceneDrawMaybeSecond`; decision once, push before each double draw; forced skip2 and faults are exceptions |
| Single/transition coverage | A zero tag requires `g_sdTick.gameplay && armed && !poisoned`. Earlier gate returns do not push it. Forced `reentry rearm` returns before `gameplay=true`; it is not equivalent to an ordinary zero-tag gameplay single |
| Queue | `src/core/gfx/reentry.cpp`: capacity 8; producer rejects a push at depth >=8; `pop_tag` clears the entire observed backlog at depth >6, including when called by the realign loop |
| Arbitration | Normal pop, then c5 classification. Robust +1 may override or invent an eye; fragile -1 defers unless the streak reaches three. Drain peeks until next eye is `-inv` or queue is empty |
| Streak/history | Agreement resets the streak. Unknown c5, zero tags and empty results do not necessarily reset it; these are not strictly three consecutive Presents. Missing c5 leaves previous valid c5 history in place |
| Lifecycle | `shutdown()` clears the ring and local history; `on_reset()` calls capture reset. Ordinary pause/resume is not established as a shutdown or full pairing reset |
| Present | `src/core/framework/frame_hooks.cpp`: disabled/exiting paths can bypass stereo; method also has pre-pop returns for poison/device/blit and post-pop returns for capture/target/hold |
| Output | Capture's current pending eye/record differs from the delayed delivered eye/record under SharedWait=0; then XR pairing and release in `src/core/vr/openxr_runtime.cpp` |
| Hands | `src/game/dishonored/hands/mesh_split.cpp`: render-time LocalToWorld classifier runs before Present arbitration; changing the final eye cannot retroactively repair those pixels |
| Instruments | `src/game/dishonored/z_account.cpp`, `scene_draw.cpp`, `frame_hooks.cpp`; reuse PairTrace/DrawCallerTrace |

Audit `camera.cpp`, `fov_lever.cpp` and `head_track.cpp` for writer ownership and
stance transitions while preserving the confirmed clamp/neck fixes.

**Queued work matters:** no new stub tick between two Presents is compatible with
older enqueued draws executing. Push-before-enqueue guarantees availability only
if that draw actually published a tag and no pop, clear, rejection or lifecycle
change broke ownership. It does not prove that an untagged image is an extra render.

**Metadata matters:** arbitration changes `t.eye` but retains the popped tag's
`rec` and `acct`; robust invention from an empty result starts with zero records.
A visually plausible eye label may therefore carry the wrong pose/write record.
Track eye, pair, pose and capture identity together, rather than accepting eye balance.

## 5. Competing hypotheses and discriminating evidence

| Hypothesis | Supporting result required | Result against it |
|---|---|---|
| H1: repair consumes a future draw's tag and sustains skew | Actual removal ledger plus independently joined render identity shows a later presented draw lost its tag to repair; faithful replay reproduces recurrence | All repair removals belong to completed/cancelled work, and later faults arise independently |
| H2: push/pop coverage or another queue mutation creates skew | A rejected/missing push, depth clear, non-presenting draw, extra consumer, bypass or lifecycle boundary explains the first mismatch | Complete accounting and valid rendered identity exclude these in the event |
| H3: c5 classifier/history misidentifies the view | Disagreement against independent render identity, including across invalid-c5 gaps, motion and transitions | All relevant classifications match that identity |
| H4: engine render/Present mapping differs from assumed one-to-one | Render-boundary evidence establishes extra, omitted, nested or replayed work | Independently joined one-to-one execution through the entire event |

H1 may explain maintenance while H2/H4 explains onset. An initial empty result
without a drain does not alone falsify H1's maintenance claim. Crouch is a possible
modifier of any hypothesis, not yet its cause. Geometry samples weaken a large
crouch-induced off-axis distortion, but cannot establish correct pair adjacency.

## 6. Measurement sequence before selecting a fix

1. **Build an exact source/accounting map.** Enumerate all head/tail writes,
   push rejections, pop callers, bypasses, lifecycle paths, gate reasons and
   counter reset sites. Record thread/device identities and snapshot semantics.
2. **Extend existing bounded diagnostics, default OFF.** Assign a monotonic draw
   attempt ID even when publication is rejected; record tick/pair/pass, gate,
   bShouldPresent, push acceptance, queue head/tail, pose record and accounting ID.
   At each Present record a separate Present ID, raw pop outcome (empty, zero,
   tagged, depth clear), every removed ID with reason, repair attempt/removal count,
   c5 serial/validity/history/geometry, streak before/after, and return reason.
   Preserve pre-event history and report dropped trace records. Avoid synchronous
   per-frame file I/O; measure tracing overhead and do not add rendering delays.
3. **Reconcile counts at consistent snapshots.** Attempted pushes = accepted +
   rejected. Occupancy change = accepted - normal removals - repair removals -
   depth-clear removals - lifecycle removals. Empty attempts remove nothing.
   Reconcile Present calls with method bypasses, pre-pop exits, normal attempts,
   downstream capture results and XR consumption separately. Pop calls are not
   expected to equal Presents because repair itself calls pop.
4. **Establish rendered identity where ambiguity remains.** Queue serials measure
   queue order, not which draw generated a backbuffer. Audit an engine render-view
   boundary and the ordering of any marker through it. Two BeginScene calls in
   build 202 invalidate assuming the old once-per-Present boundary. Treat c5
   position matching as corroboration with explicit ambiguity, not ground truth.
5. **Measure producer lead and recovery.** Queue depth/ID distance and push-to-pop
   age describe queued tags; they become render lead only with a valid draw join.
   Record stance, transition, frame timing and trace state in the same timeline.
   Trace pause entry/resume, queue, c5 history/streak, capture slots and XR pair
   state to identify what actually recovers; do not assume pause resets everything.
6. **Follow corrected identity downstream.** Join current backbuffer ID to capture
   serial/slot/delivered pose, XR eye releases/submission and hands' render identity.
   Do not use current c5 with delayed delivered tags or fit join offsets to maximize
   classifier agreement. Existing `pair geom` and TWICE diagnostics are insufficient.

Deliver an event table from before note close through onset, repeated repairs and
pause recovery. Mark uncertain ownership explicitly. If tracing cannot distinguish
H1 from H2, improve the join rather than treating serial adjacency as proof.

## 7. Host model, simulator and controlled reproduction

First create a faithful host model of the current queue/arbitration behavior with
externally assigned render IDs as the oracle. Include concurrent pushes between
peek/pop, lead 0/1/2 and larger backlogs, depths 6/7/8, zero tags, missing/rejected
pushes, missing/extra Presents, rearm, shutdown, capture failure, invalid c5 gaps,
still/moving cameras and unknown verdicts between disagreements.

With ordered one-to-one draws and no losses, changing lead alone should not create
skew. Seed an explicit fault when testing recovery and identify it. Reproduce the
measured sequence if available; failure to reproduce narrows H1 but does not falsify
it unless the model covers the observed conditions. Synthetic failures establish
possibility, not what happened on the headset. Keep shortest failing schedules.

Simulator controls `skip2`, `rearm` and `pulse` inject different conditions; validate
their actual producer behavior first. Add a precise delay or extra-Present injection
only when it tests a named hypothesis. A standalone runtime self-test does not
establish an in-game reproduction. Never launch the game automatically.

The tester runs one question per launch. First compare repeated note closes while
standing still and crouched still with the same view/equipment. Separately test
standing during an active episode without pausing, then pause recovery; this avoids
confounding stance with the pause. Only then compare matched timing/load windows,
walking/crouch-walking and power-related c5 loss. Record both-eye visual verdicts
and desktop separately, including unsuccessful reproductions.

## 8. Candidate fixes and decision gates

No candidate is selected. New behavior ships OFF with live A/B, one cause per build.

| Candidate | Prerequisite and risk |
|---|---|
| F1: position-based matching | Historical walking failure (master 3.2/4.2) makes this a research fallback. Require unique motion-aware matches, ambiguity handling, and c5-loss behavior. A wider tolerance or first-two/three search is not identity |
| F2: correct consumption/repair policy | Prefer if a concrete loss owner is proven. A serial-aware drain needs the actual Present's draw serial; the popped queue serial cannot establish which tags are stale. Repair eye and associated metadata consistently |
| F3: transport explicit draw identity | Stronger association if an ordered per-view render-command path is demonstrated. An unused shader register is not assumed safe or Present-persistent; prove ownership, byte-verify any hook, handle multiple views, overwrites and lost/reset devices |
| F4: repair a specific missing/duplicate publication or lifecycle boundary | Choose when H2/H4 identifies it. Preserve legitimate mono/menu behavior and present-stall liveness. Do not merely enlarge the ring or disable all c5 correction |

A crouch-only workaround or increasing HoldUntagged may hide symptoms without
repairing identity; neither establishes closure. Turning C5Pair off changes both
classification and draining, so it is not an isolated drain experiment.

## 9. Acceptance, regression coverage and handoff

Require an exercised old-code failure and candidate success on the same deterministic
schedule, with all accepted/rejected/removed tags accounted for. Then require tester
confirmation through repeated standing/crouched note closes and recovery. Check
correct depth, world, hands and weapons in both eyes; balanced eye counts alone do
not pass. Missing pose IDs, wrong capture association or held stale output are failures.

| Preserved behavior | Relevant validation |
|---|---|
| VR-76 mirror and delayed capture | Current/delivered identity, single bursts, desktop-eye host checks; no stale re-push |
| VR-54 / hold / black-frame fixes | First-eye hold, genuine mono transition, capture failure, reset and saved-layer lifetime |
| VR-69 camera and hands | Palette-eye and camera-clamp host checks, motion/descent, no live script flag identifying queued draws |
| VR-93 retention | Note/pause lifecycle and save-load invalidation; run retention tests if lifecycle changes |
| VR-95 / VR-97 | Keep palette prediction setting fixed; genuine repeats, head roll, missing-c5 intervals and restored history |
| Pose/capture ownership | Eye plus pose/pair record correspondence through delayed delivery and XR release |

Run relevant existing tests after implementation and report actual coverage; historical
passing counts are not new results. Do not install a candidate or alter tracing settings
as part of this documentation revision. PairTrace/DrawCallerTrace are reported installed;
verify their resolved values when preparing the next diagnostic run.

Update master section 8's six-field record with evidence, counterprediction, exact
change/build identity, negative control, host/simulator/desktop/headset verdicts and
remaining scope in the same eventual commit. Preserve failed predictions and never
quote private chat in published records. Current result: source/log review complete;
new instrument, model, simulator reproduction and headset fix validation **not run**.

## 10. Execution log

### 2026-09-13, checkpoint 1 (source map, before any code)

Work order being executed: step 1 (source/accounting map, below), step 7 (host model of the
current pairing, extracted so the model is the shipped code), step 2 (the ledger instrument,
default OFF), then one headset question. No fix is being written.

**Queue mutation sites (`src/core/gfx/reentry.cpp`).**

| Site | Thread | Effect |
|---|---|---|
| `reentry_push_tag_acct` | game (stub) | head+1; REJECTED without trace at depth >= 8 (`g_ringDropped`) |
| `pop_tag` from `end_frame` | present | tail+1, or returns false on empty, or CLEARS the whole backlog at depth > 6 (`g_ringCleared`, rate-limited warn) |
| `pop_tag` from the realign loop | present | as above, repeatedly, until `peek` shows `-inv` or empty; a clear inside the loop removes everything |
| `shutdown()` | present | tail = head (lifecycle clear); also resets c5 history, streak, last pushed eye, hold state |
| `on_reset()` | present | capture reset only; the ring and the arbiter state are NOT reset |

**Present-side exits.** Before the pop: poisoned, missing devices, blit init. After the pop:
no capture source, target creation failure, the untagged hold (returns false with no
submission), and the normal return. The pop and the arbitration therefore run on presents that
submit nothing.

**Reading from source, to be tested by the host model (not a result).** The realign can only
REMOVE tags. If the ring is one tag ahead of the presented images, the three-disagreement drain
pops until the next tag is `-inv`, which restores consistent labels by moving the ring a whole
tick ahead: the eye labels are right and the records (`rec`, `acct`) belong to the next tick.
With a producer lead of about one tick, a tick-ahead ring runs dry: a present finds it empty and
consumes nothing, which puts the ring one tag ahead again, and the disagreements and the drain
repeat. With a larger producer lead the tick-ahead state never runs dry and the episode ends
after one realign, with the records a tick early until something resets. Predictions for the
model: lead 1 with one seeded extra consumption sustains a loop of one empty pop per realign;
lead 2 ends after one realign; no seeded fault never starts either.

### 2026-09-13, checkpoint 2 (host model result, ledger built, one headset question)

**Host model (`tools/reentry-pair-host.ps1`, 79 checks).** The ring, pop, peek and the c5
arbitration moved verbatim into `src/core/gfx/reentry_pair.inc`; the proxy and the model both
compile it. The model is a producer pushing a -1/+1 pair per tick with a configurable lead
(steady, or jittered by per-tick dips and rises) and a render thread presenting each draw in
order, with every draw's identity as the oracle. Faults seeded once: a repeated present, a draw
that never presents (pass 1 or pass 2), a tag that never publishes, a single-draw tick with a 0
tag. Still and walking cameras, leads 0 to 3.

- No fault, steady lead 0 to 2: every present carries its own eye and record (asserted).
- Every single seeded fault, steady or jittered lead: eye labels are correct again within a few
  presents (0 or 1 wrong eyes more than 200 presents after the fault).
- At lead 1 or more (2 or more for a never-presenting draw), a fault leaves the ring a tick
  ahead for the rest of the run: the eye is right and the record is the next draw's. This is a
  sustained record skew with correct labels, which the eye counters cannot see.
- Lead 3 with no fault is cleared by the depth-6 rule by design.
- **The prediction in checkpoint 1 failed**: lead 1 with one extra consumption does NOT sustain
  a drain loop; it recovers. A sustained wrong-eye episode needs something the model does not
  have: recurring onset events, concurrency inside the drain, the capture delay, the hold, or
  the present-progress guard. Those are the next model scenarios, not yet written.
- Every schedule's accounting reconciles: tail moved equals normal plus repair plus clear
  removals, head moved equals accepted pushes (asserted for all 72 schedules).

**The ledger ([Stereo] RingLedger, default off, never saved).** Every tag push attempt from
`scene_draw.cpp` carries a monotonic draw id (-1, +1 and 0 pushes alike; a rejected push is
counted and its id kept). One record per present that reaches the pop: frame, stance (from the
neck's capsule stance), ring tail/head/depth and newest draw id before the pop, the raw pop
(draw id and eye, EMPTY, or CLEAR with the count), the popped tag's age, the c5 arms (along,
other, invariant eye), streak before and after, the action bits (agree, TOOK, HELD, REALIGN,
INVENT, REFUSE, unknown), every draw id the drain removed and why it stopped, the eye and draw
that went out, the c5 distance to the popped tag's written position and to the next tag's
(corroboration only), the capture's delivered eye and serial, and the end_frame return reason
(stereo, mono, HOLD, NOSRC, TARGET). Pre-pop exits are counted. Lines print only in windows:
12 back and 24 ahead after a return to gameplay or a re-arm, 12 back and 16 ahead around an
override, a drain, an invented or refused eye, or an empty pop in a tagged stream (2 s apart,
40 windows at most). A 10 s `ledger/reconcile` line checks tail movement against removals
exactly (the present thread owns the tail) and head movement against accepted pushes (one push
in flight allowed and named).

**What it can fail on.** A drain whose removed ids include a draw that later presents (the drain
over-consumed). An empty pop with no missing present (the producer ran dry). A skipped draw id
between consecutive pops with no drain, clear or rejection to account for it (an unaccounted
removal). A reconcile line that does not reconcile (a mutation path the map missed).

**Headset question (one).** Close a note standing still, several times, then crouched still,
same spot and equipment. Does the crouched close produce ledger windows with drains or empty
pops that the standing closes do not, and where is the first present whose draw id breaks the
+1 sequence?

### 2026-09-13, checkpoint 3 (headset run 5: the onset is a late tag, measured per present)

**Run.** Build `vr33-hands-working-205-g6d858c8f-dirty` (the ledger code of `7226a613`, built
before that commit), `[Stereo] RingLedger=1`, profile unchanged. Several note closes standing and
crouched; the last close was crouched (`menukeep/resume` at 8982.375 s), the flicker was reported
sustained until quitting through the pause menu at 9006 s. Log saved locally under
`build/vr93-logs/vr80-run5-155743/`. Both reconcile sides held in every 10 s window: no
uncounted mutation path.

**The episode's repeating cycle** (draw ids from the ledger; the same four presents every time):

| Present | Ring before | Pop | c5 names | Action | Out |
|---|---|---|---|---|---|
| P | empty | EMPTY | left (+6.82 along) | REFUSE | untagged |
| P+1 | D(n)-1, D(n+1)+1 | D(n)-1, age 4-8 ms | right (-6.82) | TOOK | +1, HOLD |
| P+2 | D(n+1)+1 ... | D(n+1)+1 | left | none, streak 2 | **+1: the left image to the right eye** |
| P+3 | D(n+2)-1, D(n+3)+1 | D(n+2)-1 | right | TOOK, drain removes D(n+3) | +1 (the right eye pushed twice) |
| P+4 | D(n+4)-1 ... | D(n+4)-1 | left, self 0.00 | agree | aligned again |

Reading: at P the image of D(n) was presented before D(n)'s tag was pushed. The tag arrived a
few ms later, so every later pop is one tag behind the images until the three-disagreement drain
realigns at P+3. **The drain removed the right tag; the checkpoint 1 over-drain prediction is
not what happens.** Each cycle costs one refused present, one held present and one present with
the left image in the right eye (the tester's report of the scene jumping right in the right eye
and left in the left eye is the eye swap). In the episode the empty pops, repairs and cycles ran
at about 3.3 per second (33 and 35 per 10 s).

**Why crouched and after a note close.** Tag age at pop (push to present) and ring depth before
the pop, ledger records split by state:

| State | presents | -1 tag age, median / p10 | +1 tag age, median | depth before pop |
|---|---|---|---|---|
| standing, before | 367 | 7.6 / 0.6 ms | 17.0 ms | mostly 2-3 |
| crouched, before the last close | 174 | 10.7 / 6.3 ms | 18.2 ms | mostly 2-3 |
| crouched, episode | 318 | **0.8 / 0.3 ms** | 7.6 ms | mostly 1 |

In the episode the tag reaches the ring under a millisecond before the present that shows its
image, so ordinary jitter loses the race several times a second. The windows are biased toward
events, so these are indicative distributions, not rates. Standing has a low p10 too, which
fits short standing episodes. Presents and accepted pushes were equal in the episode (157/s)
while healthy windows had more presents than pushes. **Open:** why the push-to-present margin
collapses by about 10 ms after a crouched note close and stays collapsed until a pause. Candidates:
a changed phase between the game thread and the presenting thread, or a one-tick record skew with
correct labels (the host model's sustained state; indistinguishable while still, since c5 only
tells a tick apart when the camera moves).

**Draft plan (execution order).**
1. Host model: add the measured fault, a recurring late pass-1 push at zero lead. Accept only if
   it reproduces the ledger's four-present cycle exactly (EMPTY/REFUSE, TOOK, a wrong eye, TOOK
   plus a one-tag drain).
2. Candidate F-late (default OFF, `[Stereo] LateTagRepair`, `reentry latetag on|off`, F10
   checkbox): an empty pop in a tagged stream whose c5 names an eye owes that eye one tag. At the
   next present, only if its own c5 names the opposite eye and the front tag is the owed eye, that
   tag is removed as a repair (the image it belonged to was already shown). The present then pops
   its own tag. Moving-player safety: the next present's c5 must confirm, so the fragile cross-tick
   arm alone can never trigger it.
3. Host gates: the late-push schedule with the lever on shows no wrong eye and no drain; every
   existing schedule with the lever on is no worse than off; accounting still reconciles.
4. Headset, one question: with the lever on and the ledger on, does a crouched note close still
   flicker? The ledger shows whether the late-tag repair fired and whether any TOOK cycles remain.
5. Separately and later: why the margin collapses (a phase measurement of push time against the
   presenting thread's frame period, and a moving-camera check for a tick skew).

**Steps 1-3 done (same day).** The host model's run 5 schedule (tags pushed just before their own
presents, pass 1's tag late every 40 or 17 ticks, still and walking) reproduces the ledger's
cycle present for present with the lever off: `EMPTY REFUSE`, `TOOK`, the left image to the right
eye, `TOOK` plus a one-tag drain, then agreement. Every late event costs one wrong eye, one drain,
two TOOKs and three wrong records. With F-late on: zero wrong eyes, zero drains, zero wrong
records; the refused present itself remains (an untagged present, the one `[Stereo] HoldUntagged`
covers). All 72 earlier schedules are no worse with the lever on, and all reconcile (240 checks).
The lever is `[Stereo] LateTagRepair` (default 0), `reentry latetag on|off`, and an F10 Display
checkbox; the ledger prints `OWE` and `LATE-REPAIR` and the 10 s reconcile line counts owed,
repaired and expired. Step 4 (the headset question) is next.
