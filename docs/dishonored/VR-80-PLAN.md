# VR-80: sustained eye flicker after a note closes while crouched - analysis plan for review

Status: **PLAN FOR REVIEW, no fix implemented.** Written 2026-09-13 on branch
`claude/vr-80-note-exit-eye-trace` (stacked on the VR-93 PR #50). Everything below the
"Evidence" heading is measured from local headset logs; everything under "Hypotheses" and
"Fix options" is proposal. The reviewer is asked for a deep, independent analysis of the
whole pairing path before any behaviour changes - see section 9.

Read first, in this order: `docs/dishonored/FLICKER_REFERENCE.md` sections 1, 2, 3.2, 3.14
and 3.15; this file; then the code map in section 3.

## 1. The symptom

- Close a book (the note screen) with a weapon drawn, **then be crouched**: both eyes flicker
  (hands, weapon and world alternate between eyes) and it continues until the game is paused.
  Pausing and resuming clears it every time it was tried.
- The same book close while **standing**: about half a second of flicker, then it clears by
  itself.
- Tester reports on four runs today, consistent with every log (section 4). Not seen without a
  book or pause transition first; not dependent on dark vision or any power (runs 3 and 4 had none).
- Not caused by the VR-93 menu/book retention: the same signature appears in a run where books
  still took the old drop path (FLICKER_REFERENCE 3.15, "Not caused by VR-93").

Headset, Quest via Virtual Desktop (VirtualDesktopXR 1.0.10), 90 Hz, render 2750x2850, stereo
method `reentry`, capture shared with SharedWait=0. Tested profile, which must be preserved:
`[Pace] Lag=2`, `[Stereo] LagAB=0`, `[Hands] PoseLag=2 PaletteEyeOffset=1 ModelScale=0.85`,
`[Neck] Mode=cancel CrouchPivotBelowM=0 CrouchPivotBehindM=0 RollArc=0`, `[PosTrack] ZAccount=0`.

## 2. The pairing path in one page

Two lanes. The **game thread** runs the world tick (the camera writer puts eye -1 into the camera
field), then reaches the viewport draw root through the one patched gameplay call site. The stub
(`scene_draw.cpp`, `DvrViewportDrawStub`) decides the tick's gates once, **pushes a -1 tag**
(with the written camera position and the pose record), calls the root (pass 1), writes eye +1
into the camera field, **pushes a +1 tag**, and calls the root again (pass 2). A single-draw
gameplay tick pushes a 0 tag instead ("ONE PUSH PER DRAW").

The **render thread** executes the enqueued scene and calls `IDirect3DDevice9::Present` once per
draw. The proxy's Present hook runs the stereo method's `end_frame` (`core/gfx/reentry.cpp`), which
**pops one tag per present, strictly in push order**, reads that present's `c5` (the camera
position constant the game uploaded, negated), and decides the eye:

- `inv` = what the `c5` step from the previous present says: step = -ipd along right means "pass 2
  after pass 1" (robust arm, no world tick between the two draws); step = +ipd means "pass 1 after a
  still pass 2" (fragile arm, crosses a world tick).
- If the ring's tag disagrees: the robust arm overrides the ring (`took`), the fragile arm defers
  (`held`). Three consecutive disagreements **drain the ring** until the next tag is the other eye
  of the measured one (`realigned`, lines 300-322).
- An empty ring (or a 0 tag): only the robust arm may name an eye; otherwise the present goes out
  untagged and `HoldUntagged` keeps the previous pair.

The eye then drives the capture slot, the XR swapchain for that eye, the desktop mirror, and -
separately - the hands and weapon placement, which read the eye from a LocalToWorld classifier
(`mesh_split.cpp`), not from the ring.

## 3. Code map

| What | Where |
|---|---|
| Tag push, gates, pass 1 / pass 2 | `src/game/dishonored/scene_draw.cpp` (`DvrViewportDrawStub`, `SceneDrawMaybeSecond`, header comment "ONE PUSH PER DRAW") |
| Ring, pop, peek, c5 arms, realign drain | `src/core/gfx/reentry.cpp` (`pop_tag`, `peek_tag`, `end_frame` lines ~223-400) |
| The writer's eye and the second-pass fork | `src/game/dishonored/camera.cpp` (`apply_offsets`, `eye_for_next_frame` is -1 under reentry) |
| Crouch-specific camera code | `src/game/dishonored/fov_lever.cpp` (the eye clamp, `eyeclamp:` lines), `camera.cpp` (`clamp_location_z`, `camera/clamp-rebase`), `head_track.cpp` (neck crouch pivot) |
| Untagged hold | `core/gfx/stereo.cpp` / reentry (`HoldUntagged`) |
| The instruments built for this | `src/game/dishonored/z_account.cpp` (pair trace, `[Stereo] PairTrace`), `scene_draw.cpp` (`DrawCallersNote`, `[Stereo] DrawCallerTrace`), `core/framework/frame_hooks.cpp` (Present return address, device activity) |

## 4. Evidence

Logs are local and ignored, under `build/vr93-logs/` (launch3, launch4, vr80-run1..4). Build
identity is in each log's first line.

### 4.1 What the pair trace established (build 196)

- **The camera writer is correct.** Every tagged present carried the write its tag names: ring -1 ->
  eye -1 write, ring +1 -> eye +1 second-pass write, no repeated write, and the distance from the
  write to the present's `c5` is 0.00-0.03 uu when healthy.
- **In an episode the tags run one present late.** The write-to-`c5` distance reads 6.82 uu (one
  eye separation): the present shows the other pass's image. 17 of 17 episode starts were
  immediately preceded by an untagged present (14) or a repeat present (3).
- Untagged presents: 0-3 per second before the book, 4-7 per second in the episode.

### 4.2 What was ruled out

| Candidate | Result | Run |
|---|---|---|
| A stale camera field (pass 1 drawing from pass 2's camera) | Never: no repeated write, eye -1 always | 196 |
| `c5` unavailable (VR-97) | `c5` present throughout runs 1-4 | 196-202 |
| The draw root's other three static callers (`0x4dba66`, `0x61236a`, `0x641d85`) | 0 calls in every census; no absolute reference or pointer to the root exists in the image | 199 |
| A second presenter | One Present return address, `009c01a4`, for every present including the untagged ones | 201 |
| A present to another target | Present arguments identical (no rects, no window override, no dirty region) on untagged and tagged presents | 202 |
| A buffer re-show | Untagged presents are full scene renders: 450-910 draw calls, 2 BeginScene, 51-65 SetRenderTarget, 23-34 `c5` uploads (healthy -1: median 746 / 2 / 63 / 32; healthy +1: 510 / 2 / 54 / 26) | 202 |

### 4.3 The crouched episode, build 202 (`vr33-hands-working-202-g2d8e9374`)

Timeline: `neck: stance -> CROUCHED` at 5083.09 s; book closed, GAMEPLAY at 5087.77 s; overrides from
5087.98 s until the pause at 5099.45 s; after the pause the stream is clean.

| Window | L/s | R/s | untagged/s | draws/s, 2nd/s | presents/s | Reading |
|---|---|---|---|---|---|---|
| Standing, healthy (5085 s) | 83 | 83 | 2 | 85, 83 | 169 | clean |
| Crouched episode (5091-5097 s) | 58-68 | 80-81 | 5-7 | 73-76, 71-76 | 144-152 | left eye starved |

Counters across the crouched episode (5087.8 -> 5099.0 s): `pushed eye +1 TWICE` 21 -> 224, c5 arm
`took` 13 -> 145, `held` 7 -> 72, **`realigned` 6 -> 71 (+65)**, **`untagged` (c5 arm found the ring
empty or a 0 tag) 60 -> 129 (+69)**. There were no single-draw ticks in that window (`draws/s ==
2nd/s`), so the `untagged` increments are empty-ring pops. **One empty-ring present per realign, to
within four, over 65 cycles.** Every realign reports `other=0.00`, so crouching does not disable the
measurement.

The standing episodes in the same run (5069.3 s and 5081.0 s) show 1 and 2 realigns and end.

### 4.4 The unexplained core

1. A full scene render is presented while the ring is empty, with no stub tick counted since the
   previous present. Pushes happen before the root enqueues its scene, so an empty ring at a pass-1
   present should be impossible unless something removed that tag first.
2. Realigns and empty-ring presents rise one for one while crouched. That is what a drain removing
   **one valid tag too many** would produce: the drain empties the ring past the next present's own
   tag, that present goes out untagged, the following presents pop one late, three disagreements
   trigger another drain - a self-sustaining loop.
3. Crouched frames are slower (73-76 draws per second against 85) and the untagged rate is higher.
   Standing, a single drain lands correctly and the loop never starts; crouched, the timing between
   the game thread's pushes and the render thread's pops changes what the drain finds.
4. A pause clears it: the resume re-arms (single ticks push 0 tags, then doubles), which resets the
   streak and refills the ring from a known state.

## 5. Hypotheses, ranked

**H1 (leading): the realign drain over-drains when the game thread's lead over the render thread is
not what the drain assumes, and the correction then sustains the fault.** The drain pops "until the
next tag is the other eye of the measured present". If the game thread is two draws ahead (its next
tick's -1 is already in the ring behind a genuine +1), or zero ahead (the -1 not yet pushed), that
rule removes a tag the next present needed. Crouching shifts the lead via frame cost.
- Predicts: in a per-present record with push and pop serials, every empty-ring present is
  immediately preceded by a drain whose last popped tag was that present's own tag; ring depth at the
  drain differs between standing and crouched episodes.
- Falsified by: empty-ring presents that follow no drain within the same pairing cycle, or drains that
  only ever pop tags that were genuinely stale (their serial older than the present being measured).

**H2: some draws do not present, or some presents pop twice, and the drain is a victim, not the
cause.** Census windows show presents at 0.76-0.99 of stub draws; a draw that never presents leaves an
orphan tag. Pops outside `end_frame` or an `end_frame` early return after `pop_tag` would also skew.
- Predicts: push serials skipped at the pop side without any drain; a present count below the pop
  count or above it.
- Falsified by: pops exactly equal presents and every skipped serial belonging to a drain.

**H3: a crouch-specific camera write changes the `c5` step between passes and makes the arms misread
genuine presents.** The crouched eye clamp (`eyeclamp: camZ ... -> ...`) and `clamp-rebase` write the
camera location on the script lane; if they land between pass 1 and pass 2 or move `c5` by a fraction
of an eye separation, `inv` can flip or vanish.
- Predicts: `other` or `along` distorted on crouched disagreements; overrides without any empty ring.
- Weakened already: every crouched realign logged `along=-6.82 other=0.00`.

**H4: an engine-side extra scene render per tick when frames are slow** (UE3 frame pacing or a
render-thread repeat). Ruled against by 4.2 (one presenter, no extra draw-root calls), but not
excluded for renders issued inside the root.

## 6. Measurements the analysis needs before any fix

All read-only, default OFF, bounded, and each able to print the unwelcome answer.

1. **Serials through the ring.** Assign a monotonic serial at each push (game thread), carry it in the
   tag, and record per present: popped serial (or none), ring depth before the pop, the drain's popped
   serials, and whether the present's `c5` matches the popped tag's written position (the existing
   write-to-`c5` distance). With serials, H1 and H2 are separated by one episode.
2. **Lead.** Per present: newest pushed serial minus popped serial (the game thread's lead in draws),
   and the time from push to pop. Compare standing and crouched distributions.
3. **Pop and present accounting.** Count `pop_tag` calls, `end_frame` calls, early returns by reason,
   and Presents, per second; they must reconcile.
4. **Stance and frame cost on the same line**, so the crouch dependence is measured, not reported.
5. **A host model of the ring.** `reentry.cpp`'s pairing (pop, peek, c5 arms, drain) extracted into a
   pure function and driven by synthetic push/pop schedules: lead 0, 1 and 2; a draw that never
   presents; a present that finds the ring empty; still and walking cameras. The current code must
   reproduce the one-for-one realign/empty loop under the lead that crouched frames produce, or H1 is
   wrong.
6. **Simulator reproduction** (`tools/xrsim-*`): the seam has `reentry skip2`, `reentry rearm` and
   `reentry pulse`; a controllable render delay or a forced extra single present would show whether
   the loop can be started and sustained without a headset.

## 7. Fix options (after the analysis picks one)

Each ships default OFF with a live A/B, one behaviour change per build, and must keep the tested
profile.

- **F1 - pair by identity, not order.** The tag already carries the written camera position, and the
  trace shows the present's `c5` matches its own tag's position to 0.03 uu and the other eye's to 6.8
  uu. Pop the tag (within the first two or three) whose position matches this present, discard only
  tags older than it, and fall back to order when `c5` is unavailable (VR-97). **Caution the reviewer
  must weigh:** position re-alignment was tried on 2026-09-03 and mis-paired a walking player because
  the engine moves the camera after the tick's write (reentry.cpp comment above `pop_tag`). The match
  would need a tolerance that separates "same eye, moved by travel" from "other eye", measured while
  walking and crouch-walking, not assumed.
- **F2 - make the drain serial-aware.** Drain only tags whose serial is older than the present being
  measured; never pop a tag the next present can still claim. Smallest change if H1 holds.
- **F3 - carry a draw serial to the render thread explicitly** (an unused shader constant register or a
  render command enqueued by the stub) so the present knows which draw it shows without inference.
  Largest change; strongest identity.
- **F4 - a crouch or frame-cost specific trigger**, only if the analysis shows the loop cannot start
  without it. Not preferred: it treats the rate, not the pairing.
- In every option, the hands' eye (the LocalToWorld classifier) must be checked to agree with the
  corrected image eye; a fix that repairs the world and leaves the hands a full IPD off is not done.

## 8. Constraints

- Never launch the game; the tester runs one question per launch and reports.
- Anything that writes engine memory byte-verifies its target; new levers default OFF with a live A/B.
- Every result - confirmed, failed or not run - goes into `FLICKER_REFERENCE.md` in the same commit as
  the work, in its section 8 format. Never quote a chat in anything published.
- The instruments `[Stereo] PairTrace` and `[Stereo] DrawCallerTrace` are installed on the test PC and
  should be reused rather than rebuilt.

## 9. Questions for the reviewer

1. Does the evidence in 4.3 support H1 over H2, and what single measurement would you add to
   separate them if not the serials in 6.1?
2. Is the "push before enqueue, so an empty ring at a pass-1 present is impossible" argument sound
   under UE3's render-thread model and OneFrameThreadLag? What else can reorder a tag against its
   present?
3. Is the realign drain rule (lines 300-322) correct for a game-thread lead of 0, 1 and 2 draws? Walk
   each case.
4. Why would crouching change the lead or the untagged rate? Is frame cost enough, or is there a
   crouch-specific render or camera path (the eye clamp, the stealth post-process, the capsule) the
   analysis should instrument?
5. Why does a pause clear it, exactly - which reset does the resume perform that a realign does not?
6. Of F1-F3, which is the smallest change that removes the class of fault rather than this instance,
   and what does the 2026-09-03 position re-alignment failure imply for F1?
7. What regressions would each fix risk in the flicker classes already closed (FLICKER_REFERENCE
   sections 3.1-3.9), and which host or simulator checks would catch them?
