# Brief: the game's own frame alternates between eye positions

**Status: ANSWERED, 2026-09-06. The fix is written.** Keep this document as the
record of the four hypotheses that were argued and killed on the way, three of
them from counters that could not see the fault - that is the part worth
re-reading before anyone proposes something new in this area.

**The answer was not in the headset path.** `hkPresent` calls the game's
original `Present` for every eye draw, and `mirror_present()` had never
implemented the D3D9 copy, so the game WINDOW had no eye policy and showed
L, R, L, R while the headset received correct pairs the whole time. What a
recording of the window shows is not what the headset was doing.

The fix, the pause-menu session loss found in the same review, and the reading
that was retracted alongside them are all in
**`docs/dishonored/DESKTOP_MIRROR.md`**. Everything below this line is the
state of the investigation BEFORE that was known, preserved unedited.

Four hypotheses have already been argued and killed by measurement. Three of
them were argued from counters that could not see the fault. Do not add a fifth
from reading alone.

---

## 1. The symptom

Dishonored VR (this repo). In the headset and in the game's own window, the
picture flickers in motion. A frame-by-frame look at a recording shows the view
**alternating between two camera positions laterally** - it looks exactly like
alternate-eye rendering. It is worse when the framerate is unstable, and the
game is jittery whenever frames drop. It has done this for as long as the
tester has been running the current stereo method; it is not new to any recent
change.

The tester's monitor is 240 Hz. The behaviour is identical at 80, 90 and
120 Hz headset refresh, so it is not a display cadence beat.

---

## 2. The system, in one paragraph

`d3d9.dll` proxy next to `Dishonored.exe` (UE3 build 9099, 32-bit, D3D9). The
game renders natively; once per `Present` the active **stereo method** turns the
frame into an eye-tagged D3D11 texture and the OpenXR runtime layer submits it.
The method in use is **`reentry`** (rung 3 of the stereo ladder,
`[Stereo] Method=reentry`, the shipped default).

`reentry` works by patching the ONE call site of the UE3 viewport draw root so
that a gameplay tick can draw the scene **twice** - pass 1 from the left eye,
pass 2 from the right - with the camera's eye offset changed between them, and
each draw's present tagged -1 / +1 so the runtime can pair them.

Key files:

| file | role |
|---|---|
| `src/core/framework/frame_hooks.cpp` | `hkPresent`: the whole per-present order (`hkPresent` ~line 110-200) |
| `src/game/dishonored/present_tick.cpp` | the game side of the frame path; **line ~295** does `dvr::camera::set_eye(active()->eye_for_next_frame())` |
| `src/core/gfx/stereo.h` / `stereo.cpp` | the method seam, the beat, the pair probe report |
| `src/core/gfx/reentry.cpp` | the `SequentialReentry` method. `eye_for_next_frame()` returns **-1 always** (line 138); `presents_per_tick()` returns **2** (line 137) |
| `src/game/dishonored/scene_draw.cpp` | the call-site patch, the per-tick gate decision (`SceneDrawDecide`), pass 2 (`SceneDrawMaybeSecond`), the beat (`SceneDrawBeat`) |
| `src/game/dishonored/camera.cpp` | the camera seam. `apply_offsets()` ~line 470 is the writer; `second_pass_for_current_thread()` ~line 395 decides -1 vs +1 |
| `src/core/vr/openxr_runtime.cpp` | the runtime layer. `pair_probe()` / `pair_probe_peek()` report what it actually paired |

Terminology used below: "tick" = one gameplay tick / one viewport draw root
call; "double" = that tick ran pass 2 as well; "tag" = the -1/+1/0 a present
carries; "pair" = the runtime assembling a tagged L and R into one stereo
submit.

---

## 3. What is measured (2026-09-06, headset, VDXR, 90 Hz)

### 3a. The doubling is INTERMITTENT

```
reentry: beat draws/s=132 2nd/s=40 presents/s=173 ...
reentry: beat draws/s=173 2nd/s=0  presents/s=173 ...
```

Only ~30 % of gameplay ticks double, and whole 3-second windows double **zero**
times. Presents = draws + second draws (132+40=172~173), so each second draw
does get its own present.

### 3b. When it doubles, the pairing is HEALTHY

```
stereo: eyes ageL=1 ageR=0 presents at the last stereo submit (max L=1 R=0;
  healthy 1/0) | stereoSubmits=139 pairs=140 aborts=0 (left=0 untagged=0
  expired=0) staleEye L=0 R=0 eaten=0 | window 3 s
```

139 stereo submits, 140 pairs, **zero aborts**, ages exactly the "healthy 1/0"
the instrument defines. In the very next window: `stereoSubmits=0 pairs=0`,
matching `2nd/s=0`.

### 3c. The render really does use two positions one IPD apart

A per-present trace of **c5** (the view constant the game drew with, captured by
the constant hook - downstream of every write, restore and recompute) was added
for this investigation (`camera/eyetrace`, `src/game/dishonored/camera.cpp`):

```
writes eye-1=7660 eye0=0 eye+1=2442 (second-pass branch 2442, skipped 0)
c5 along right over the last 24 present(s): span 6.83 uu   <-- one full eye-to-eye step
writes eye-1=8165 eye0=0 eye+1=0    (second-pass branch 0)
c5 along right over the last 24 present(s): span 0.00 uu
```

The correlation is exact across every window sampled: **the +1 branch fires =>
c5 spans exactly one eye-to-eye step (6.83 uu at IPD 63.2 mm, 108 uu/m); the +1
branch does not fire => c5 span is 0.00.**

### 3d. The camera eye is a CONSTANT -1, independent of the gates

`reentry::eye_for_next_frame()` returns -1 unconditionally. `present_tick`
pushes that into the camera seam every present. `apply_offsets()` then writes
`-3.41 uu along right` on every dispatch, **including on ticks that will not
double**. The +1 only appears inside pass 2, via the per-thread latch.

### 3e. Environment

Render 2750x2850, `VirtualMode=1`, capture `mode=shared`. Measured perf floor
for this mod is ~0.64 ms/megapixel on a ~5.6 ms fixed floor; 120 Hz is known
unreachable and 90 Hz is the established target. `[Hands] Enabled=0`.

---

## 4. Hypotheses already FALSIFIED - do not re-propose these

| # | hypothesis | killed by |
|---|---|---|
| 1 | Display cadence beat (game Hz vs monitor Hz) | monitor is 240 Hz; 80/90/120 all divide evenly; identical at all three |
| 2 | A headset mirror compositing both eyes into the desktop image | the artifact is in the game's OWN window, and in the headset |
| 3 | The second draw produces no present, so both draws land in one backbuffer | `draws/s=202 2nd/s=19 presents/s=221` - each second draw presents |
| 4 | The second-pass latch is stuck set on a thread | `set_second_pass(true/false)` is tightly balanced around the pass-2 draw (`scene_draw.cpp` ~289/~305); the +1 count goes to 0 in windows with no second draw |
| 5 | The runtime never pairs, so tagged frames go out untagged | `stereoSubmits=139 pairs=140 aborts=0`, ages healthy |

Two counters actively misled the investigation and are worth knowing about:

- `stereo: beat ... L/s=0 R/s=0 mono/s=173` reads **0 by design** on the quad
  screen. It is not evidence that no stereo is happening.
- `reentry: beat ... skips foreign=/state=/silent=/stall=/session=` are
  **lifetime cumulative totals** printed inside a per-window line. They cannot
  answer "which gate refused during the three seconds the doubling stopped".
  This is an instrumentation gap, see §6.

---

## 5. Current hypothesis (to be attacked)

**Intermittent doubling is worse than no doubling, because the camera's eye
offset is not gated with it.**

On a tick that doubles: pass 1 draws from -1 and presents tagged -1, pass 2
draws from +1 and presents tagged +1, the runtime pairs them - correct stereo.

On a tick that does NOT double (~70 % of them, and sometimes 100 % for seconds
at a time): the camera is **still at -1**, one draw happens, and it presents
**untagged**. The runtime shows that left-eye-offset image to **both** eyes.

So the view alternates between "a left-eye-offset image shown to both eyes" and
"a correct stereo pair", at an irregular, framerate-dependent rate. The lateral
step between those states is up to a full eye-to-eye separation. That is a
frame-by-frame lateral alternation - which is exactly what the recording shows
and why it reads as alternate-eye rendering - and it gets worse when the
framerate is unstable because the gates are timing-dependent.

It also explains the performance complaint: on the ticks that do double, the
entire scene is rendered twice.

### Proposed fix A (minimal, the one I would write)

**The eye offset must be gated with the doubling.** A tick that will not run
pass 2 must render from eye **0** (centred), not -1. The decision already exists
at depth 0 before pass 1's tag is pushed (`SceneDrawDecide` -> `g_sdTick`), so
the information is available at the right moment; what is missing is the path
from that decision back to `dvr::camera::set_eye`.

Result: single ticks render centred and go out untagged - a mono frame with no
lateral offset. Doubled ticks are unchanged. The alternation collapses from "up
to one IPD" to "half an IPD between a centred mono frame and each eye of a
pair", and if that is still visible, to zero by also holding the *previous*
pair rather than submitting the centred single.

### Proposed fix B (the real one, larger)

Make doubling **continuous** - every gameplay tick doubles - so the stream is
uniformly stereo. Requires understanding why the gates refuse ~70 % of ticks
during ordinary gameplay, which nothing currently in the log can answer (§6).

### Fallback C

While `reentry` cannot double every tick, `[Stereo] Method=mono` is the correct
default: no second draw, no eye offset, no alternation. Costs stereo depth.

---

## 6. Instrumentation gap to close FIRST

`SceneDrawBeat` prints the skip counters as lifetime totals. Before fix B can
be designed, the beat needs **per-window** skip counts so the log can say which
gate refused, how often, during a window where `2nd/s` collapsed. The gates, in
order, are in `SceneDrawDecide` (`scene_draw.cpp`): not armed, rearm-by-request,
poisoned, foreign caller, exiting, no XR session, state not GAMEPLAY,
eyetest/postest running, camera silent (no c5 upload since the previous draw),
no present since the previous draw.

Note `d.gameplay = true` is set **mid-way** through that list, so three gates
can still refuse after the tick has been classified as gameplay.

---

## 7. What I want back from you

Read the frame path end to end first - `hkPresent`, `present_tick`,
`stereo.cpp`'s begin/end frame and its beat, `reentry.cpp`'s tag ring
(push per draw on the game thread, pop per present on the present thread),
`scene_draw.cpp`'s gates and call-site patch, and `camera.cpp`'s writer and its
restore/base logic. Then answer:

1. **Is the hypothesis in §5 right?** Specifically: on a non-doubling tick, is
   the frame really rendered from the -1 eye and submitted untagged, and does
   the runtime really show it to both eyes? Name the lines that make it true or
   false.
2. **Is fix A safe with respect to the tag ring?** The ring pairs by ORDER, one
   push per draw, and an armed single draw pushes a 0 tag. If the camera goes to
   eye 0 on single ticks, does anything in the ring, the pair probe, or
   `openxr_runtime`'s pairing change behaviour? Is there a reason the eye was
   deliberately left at -1 for singles that I have missed?
3. **Where should the gate decision reach the camera?** `set_eye` is called from
   the present thread; the doubling decision is made on the game thread at the
   start of the draw. Those are different lanes and this repo is strict about
   that. Propose the seam.
4. **Why do the gates refuse ~70 % of gameplay ticks?** Which gate is the
   likely one, from reading? `camera silent (no c5 upload since the previous
   draw)` and `no present since the previous draw` are the two that look
   timing-dependent to me, and timing-dependence matches "worse when frames
   drop". Say which you would instrument first.
5. **Is there a better fix than A or B** - in particular, can pass 2 be made
   cheap (it currently redraws the whole scene), or can the single-tick frame be
   held rather than submitted?
6. Flag anything in §3 you think is measured wrongly. Three of my four dead
   hypotheses died because I trusted a counter over the picture.

Do not write the fix yet. Come back with the research and a revised plan.

---

## 8. Reproduction and instruments

- `[Stereo] Method=reentry` (the default) in `dishonored_vr.ini` next to the exe.
- Log: `dishonored_vr.log` next to the exe, rotated one deep.
- Grep `reentry: beat` (doubling rate), `stereo: eyes` (pairing),
  `camera/eyetrace` (what the render used, per present), `perf: present`.
- `stereo mono` / `stereo reentry` at the in-game console (F1) switch live.
- `tools\xrsim-launch.ps1` runs the game against a simulated OpenXR runtime with
  deterministic frame stepping and per-eye captures, no headset - see
  `docs/VERIFICATION.md`. **The tester runs the game; do not launch it.**
