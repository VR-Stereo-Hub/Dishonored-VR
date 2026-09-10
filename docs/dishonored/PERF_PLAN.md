# Performance plan - DRAFT for review (VR-67, 2026-09-09)

**Review update:** [PERF_REVIEW.md](C:/dev/Dishonored-VR/docs/dishonored/PERF_REVIEW.md)
revises this draft using the original log and source. Its conclusions and test
order supersede the proposals below; this draft is retained as the review input.
The reduced-resolution test has not been run.

**Status: draft, for external review before any of it is built.** Everything in
section 1 is measured on one identified build; everything in section 4 is a
proposal with an explicit falsification test. Nothing in sections 3-5 has been
implemented.

---

## 0. The complaint, stated precisely

Playing at a headset refresh of **80 Hz with synchronous spacewarp active**, so
the application target is **40 fps**. The tester reports **lag spikes every 3-10
seconds**, consistent regardless of which direction they look. The spikes are
not correlated with head motion, which distinguishes them from the VR-65 judder
(fixed: the submitted pose was one render generation too new).

The measured rate is **one detected frame gap every 2.5 seconds** over a 221 s
run, which is the same phenomenon at the same order of magnitude.

---

## 1. What is measured, on this build

Build: the VR-67 branch (`claude/vr-67-perf-distribution-ab`), Release, installed.
Hardware: RTX 4070 Ti SUPER, single GPU. Runtime: Virtual Desktop (VDXR) over
Wi-Fi to a Quest 3. Render size **2750x2850 per eye** (7.84 MP), `VirtualMode=1`,
capture mode deferred, `[Pace] Lag=2`.

### 1.1 The frame time distribution, from the self-switching A/B

Seven segments of 20 s each, gameplay only, first 2 s of each discarded.
Interval between consecutive **presents** (two presents per stereo pair).

| Segment | n | p50 | p95 | p99 | max | over 2x median |
|---|---:|---:|---:|---:|---:|---:|
| baseline | 1427 | 11.42 | 21.33 | 23.76 | 67.35 | 1.68 % |
| frameid readback OFF | 1419 | 11.60 | 21.13 | 24.88 | 103.46 | 1.41 % |
| baseline again | 1408 | 11.33 | 21.90 | 25.19 | 101.79 | 3.27 % |
| max frame latency 1 | 1420 | 11.15 | 21.64 | 24.48 | 115.17 | 2.61 % |
| max frame latency 2 | 1432 | 11.26 | 21.30 | 22.76 | 86.61 | 1.54 % |
| max frame latency 3 | 1406 | 11.08 | 22.07 | 25.25 | 102.68 | 4.55 % |
| baseline last | 1403 | 11.08 | 22.15 | 26.98 | 104.97 | 4.99 % |

**Noise floor: the three baseline segments span p50 11.08-11.42 ms, so anything
inside 3.0 % is not a result.** Every alternative landed inside it.

### 1.2 The two cheapest candidates are FALSIFIED

- **The `FrameId` render-target readback** (`GetRenderTargetData`, 64x64, one
  pair in eight) - **no change**, +2.9 % on p50, inside the noise floor, and it
  did not move p99 or the over-2x-median count either.
- **`IDirect3DDevice9Ex::SetMaximumFrameLatency`**, never previously called in
  this codebase, swept 1 / 2 / 3 - **no change** at any value. The call
  succeeded and the device read the value back, so this is a real negative, not
  a refused lever.

Both were plausible from source inspection. Neither survived measurement.

### 1.3 The per-tick budget split

```
perf: tick 25.0 ms (40.0/s, 80 presents/s) [hmd 25.00 ms = 40.0 Hz, budget 25.00 ms/tick]
PACE-BOUND (wait 6.3 ms/present)
= P1[-1] n=120 in 14.6 (pre 0.0  begin 12.6 [wait 12.5]  tick 0.1  method 0.1
                        [cap 0.0: lock 0.0 copy 0.0 up 0.0 blit 0.0]
                        end 0.0 [acq 0.0 xrCopy 0.0 endFrame 0.0]  present 1.8)
        + out 4.3 (idle 0.4  R 3.9)
| P2[+1] n=120 in  3.2 (pre 0.0  begin 0.0 [wait 0.0]  tick 0.1  method 0.1
                        [cap 0.0: ...]
                        end 0.1 [acq 0.0 xrCopy 0.0 endFrame 0.1]  present 3.0)
        + out 2.9 (idle 0.0  R 2.9)
| untagged 0 | marker=BeginScene(1.0/present in 240 of 240)
```

Reading it:

| Item | Cost per tick | Note |
|---|---:|---|
| `xrWaitFrame` idle (P1 only) | **12.5 ms** | the app is deliberately parked; this is the pacing budget, not work |
| The two original D3D9 `Present` calls | **4.8 ms** (1.8 + 3.0) | 19 % of a 25 ms tick, **38 % of a 12.5 ms tick** |
| Render-thread `R` (executing commands) | **6.8 ms** (3.9 + 2.9) | |
| Capture (lock/copy/upload/blit) | **0.0 ms** | deferred capture is free at this size |
| `xrEndFrame`, typical | **0.1 ms** | see 1.5 for the tail |
| Real CPU per tick, excluding the wait | **~12.5 ms** | |

### 1.4 The GPU

```
perf: gpu/present span=7.0 ms (3d 6.9 + readback dma 0.1) idle(d3d9)=2.3 ms
    | per tick span=14.0 dma=0.3 idle=4.5 | 240 resolved, 0 late, 0 disjoint, 0 unmarked
```

**GPU: 14.0-17.3 ms per tick** (both eyes), against a 25 ms budget at the current
40 fps target. Zero late or disjoint query sets, so the population is clean.

**The number that matters for the next step: at a native 80 Hz target the budget
is 12.5 ms, and the GPU already needs 14.0-17.3 ms.** The CPU needs ~12.5 ms.
**Both budgets are at or over the 80 Hz line, independently.** That is why
spacewarp is on: the app cannot hold 80 native at this render size.

### 1.5 Where the spikes actually are

88 detected gaps over 221 s. Location tally:

| Where the gap sat | count | share |
|---|---:|---:|
| **`present-tail` (xrEndFrame)** | **71** | **81 %** |
| `out` (render thread after Present returned) | 13 | 15 % |
| other | 4 | 5 % |

Gap sizes: n=88, min 40 ms, **p50 68 ms**, p90 201 ms, max 2165 ms (one load).

A representative gap:

```
perf: frame gap 70ms (5.5x the mean present interval 12.7 ms)
  | sat in: present-tail (xrEndFrame) of #14592 tag +1
    (61.3 ms of in 64.6 / out 5.0; wait 0.0 lock 0.0 endFrame 61.3)
    = 2.8 display slots at 25.00 ms
```

And the same instrument across a good window versus a bad one:

```
good: endFrame mean=0.07 ms  max=0.2 ms  over 120 submits  MATCHED 1.00x
bad:  endFrame mean=2.58 ms  max=74.4 ms over 109 submits  UNDER-SUBMITTING 0.91x
```

**`xrEndFrame` normally costs 0.07 ms and occasionally costs 40-75 ms. That is a
thousand-fold discrete stall, not a load curve.** A rising encode cost would
raise the mean; this does not.

The instrument's own verdict on whether this is back-pressure:

> `UNDER-SUBMITTING 0.91x: display slots are going UNFILLED, so xrEndFrame is
> not throttling a surplus - the present-tail stalls are genuine hitches and
> their cause is upstream of the headset's cadence`

So the runtime is not merely pacing us. Something inside `xrEndFrame` blocks.

### 1.6 One more standing signal, unresolved

```
UNEVEN CADENCE: 1.10 display slots per frame (not a whole number) with sd 12.80 ms
- one frame in 10 is held an extra slot (a 3.9 Hz beat)
```

Measured previously: 1.05-1.11 ghosts, 1.00-1.02 does not. `vrpace sync <hz>`
exists to lock the pair schedule and **has never been A/B'd**.

---

## 2. What the measurements rule out

- **Not the capture.** 0.0 ms per tick in the deferred path.
- **Not the frame-identity readback.** Falsified by A/B.
- **Not the D3D9Ex present queue depth.** Falsified by A/B at 1, 2 and 3.
- **Not GPU query error.** 240 of 240 resolved, none late or disjoint.
- **Not eye pairing.** `untagged 0` across the run.
- **Not head motion.** The tester reports the spikes are direction-independent,
  and the VR-65 pose fix is in and confirmed.

## 3. What the measurements do NOT establish

Stated so the plan is not read as more certain than it is.

- **Whether the 81 % of stalls inside `xrEndFrame` are caused by anything we
  control.** The call body is entirely Virtual Desktop's: encode, then hand off
  to the Wi-Fi link. Our submitted image size is an input to it, and that IS
  ours. Nothing has been measured that separates "the link hitched" from "we
  gave the encoder more than it can absorb".
- **The GPU accounting is known to be partly wrong.** The reported span is
  `BeginScene` to Present-hook entry, and the capture copy is issued *after*
  that entry, yet the report subtracts capture DMA from the earlier span and
  labels the remainder "3d". `idle(d3d9)` is a gap between markers, not proof
  the GPU is idle. There are **no D3D11 GPU timings at all**, so the
  D3D9 -> shared texture -> D3D11 blit -> XR copy bridge is unmeasured.
- **How much of the second eye's render is duplicated engine work.** The stereo
  method re-enters the viewport renderer; the game-thread call is short but the
  render-thread and GPU consequences are not attributed per eye.
- **Whether the `R` time (6.8 ms/tick) contains our per-draw hook cost.** The
  indexed-draw hook routes through weapon attachment and mesh identification
  with device-state getters and COM refcounting on every draw. Never profiled.

## 4. Proposed order of work

Each step names the measurement that decides it, and what result would kill it.

### Step 1 (armed, not yet run): a resolution step-down to test the 80 Hz line

**Hypothesis.** The app is on spacewarp because both budgets exceed 12.5 ms at
7.84 MP/eye. Cutting to **5.80 MP/eye (2365x2451, the same 55:57 aspect)** should
put the GPU near 11.8 ms and let the app hold **native 80 Hz with no spacewarp**.

Fit used: GPU per-tick span 14.0 ms at 7.84 MP/eye on a ~5.6 ms fixed floor
implies ~1.07 ms per eye-megapixel; at 5.80 MP that is 5.6 + 6.2 = 11.8 ms.

**Decides:** whether the frame budget is pixel-bound at all.
**Kills it:** GPU per-tick span barely moves -> the cost is CPU, submission or
resolution-independent passes, and every pixel-reduction idea is dead.
**This is a diagnostic, not a proposed permanent downgrade.**

**It also tests the encoder theory for free**: if the `xrEndFrame` stall rate
falls with the submitted image size, the stalls are encode-bandwidth-bound and
therefore partly ours. If the stall rate is unchanged, they are the link and no
render-side change will touch them.

### Step 2: one desktop `Present` per completed stereo pair

**The largest single measured cost we control: 4.8 ms/tick, 38 % of a 12.5 ms
budget.** The game presents to the desktop after each eye; only one of those two
desktop frames is ever seen.

Constraints that make this non-trivial, and any of them broken makes the result
worthless:

- The capture may be driven by the same hook that performs the Present. Suppressing
  the engine's Present request must not suppress the capture.
- The D3D9/D3D11 bridge's flushes and synchronisation must be preserved.
- Reset, device-loss, menu and transition paths must be preserved.
- Which eye reaches the desktop must be chosen from the identity of the current
  backbuffer, not the delivered capture tag, which can describe the previous
  present.
- Desktop presents must be counted separately from rendered eyes and submitted
  pairs, or a "faster" build that drops an eye will read as a win.

**Kills it:** total tick time unchanged because the wait moved into `StretchRect`,
a fence, or the next draw. **A lower number in the Present column alone is not a
result.**

If this lands, the snapshot/restore pair of full-resolution desktop-eye copies
(`desktop_eye.cpp`) may become unnecessary - but only after confirming the
intended eye reaches the desktop and headset capture is still correct.

### Step 3: fix the GPU accounting, then add D3D11 timings

Not an optimisation. Steps 4-5 cannot be ranked without it.

- Label literal measured intervals; stop subtracting an interval from a span that
  does not contain it.
- Add D3D11 GPU timestamps around the bridge blit and the XR copy.
- Report distributions for complete stereo pairs, not window means. (The A/B
  harness from VR-67 already does this for present intervals and can be reused.)

### Step 4: `vrpace sync <hz>` against the 1.10 slots-per-frame beat

Already implemented, never A/B'd, and the cadence beat is a standing measured
signal with a known threshold (1.00-1.02 good, 1.05-1.11 bad). Cheapest
remaining lever. Add it as a segment to the VR-67 plan rather than as a separate
run.

### Step 5: profile the per-draw and per-`ProcessEvent` hook cost

Only after step 3 makes the CPU split trustworthy. Candidates named by source
inspection, none of them measured:

- The indexed-draw hook: weapon routing runs before the palette gate; buffer
  identification retrieves stream and index-buffer state; census work retrieves
  buffers, declaration and shader, then searches recorded draws. A dense scene
  puts every unrelated draw through this.
- `ProcessEvent`: camera validation and offset application per intercepted event,
  with readability checks that reach `VirtualQuery` rather than comparing pointers.
- The shader-constant hook copies small uploads into a diagnostic ring even when
  no capture is armed.

**Trap to avoid:** hiding the hands visually does not prove their draw-processing
machinery stopped running. Any toggle used for this A/B must be shown to bypass
the expensive work, not just the drawing.

### Step 6: share eye-independent engine work, or shorten the bridge

Largest intervention, lowest confidence, and it must follow step 3. Do not remove
synchronisation as a speed fix: incorrect resource ownership makes an apparently
faster build display overwritten or unfinished eye images.

---

## 5. Questions for the reviewer

1. **Is the encoder/link conclusion in 1.5 justified**, or is there a way the
   stall inside `xrEndFrame` could be caused by something we do on a previous
   frame - a fence, a resource still in use, a swapchain image not released?
   Note `acq 0.0` and `xrCopy 0.0`, so the acquire is not blocking.
2. **Does the step-1 fit hold?** 1.07 ms per eye-megapixel on a 5.6 ms floor is
   derived from a single operating point plus a floor measured in an earlier
   session. Two points would be better; is one run at 5.80 MP enough to trust
   the extrapolation, or should the sweep include a third size?
3. **Is step 2 ranked correctly ahead of step 3?** The 4.8 ms is real and
   measured, but the accounting that would tell us whether removing it helps is
   the thing step 3 fixes. There is a case for reversing them.
4. **Is there a cheaper way to separate "encoder" from "link"** than changing
   the submitted image size - something readable from the runtime, or from
   Virtual Desktop's own overlay, that would settle 1.5 without a code change?
5. **Anything in section 2 that should not be considered closed.** The two
   falsifications are single-run, 20 s each, on one scene.

---

## 6. Project constraints the plan must respect

- Every new render lever ships **default OFF with a live A/B toggle**; a method
  that refuses leaves the previous one running.
- `git diff main...HEAD -- src/core/` staying non-empty means the change is in
  scope for other games too; core changes must default to pre-existing behaviour.
- Retired experiments go to `src/legacy/`, never deleted silently.
- The present thread owns every runtime call. Never take a reference to an
  engine D3D object inside a detour.
- A verified write is not an honoured one; acceptance is a measured downstream
  effect.
- An instrument that cannot fail its own hypothesis is not evidence.
- The tester runs the game; diagnostics must ship default-on in the installed ini
  and be readable from `dishonored_vr.log` without a debugger.
