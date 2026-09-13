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
