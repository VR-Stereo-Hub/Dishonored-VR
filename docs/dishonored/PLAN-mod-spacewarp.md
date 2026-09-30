# The mod's own spacewarp (MSW) - VR-39 follow-up

Branch `claude/vr-39-mod-spacewarp` (on top of `claude/vr-39-afw-polish`, #159). Status: **rung 1 built and
host-verified; not yet run in the simulator or the headset.** Default off.

## 1. Why

**Measured, run 8** (AFW, 3012x3122, 144 Hz; `stereo: rate` line):

    presents/s=87-91 submits/s=87-91 | UNDER-SUBMITTING 0.60x: display slots are going UNFILLED

- The frame loop runs inside the game's Present hook, so every slot the game is too slow for goes to the
  runtime's reprojection.
- Under Virtual Desktop that means SSW, which runs ON THE HEADSET. It works on the decoded video, with
  motion from block-based optical flow (see the sources).
- That explains the reports:
  - clean while standing and walking;
  - smeared while running (large, fast motion defeats block matching);
  - the HUD warped with the world (the quads are already flattened into the video SSW sees).
- The depth layer (#159) may not help: nothing found says VD's SSW reads submitted depth.

## 2. What MSW does (rung 1)

A thread fills the slots the game misses, on the PC, with information SSW never has:
- each eye's own depth;
- the game's own camera matrices;
- the body's walk and turn;
- the foreground mask;
- the HUD as separate layers.

**Ownership of the frame loop.**
- `g_cycleMx` (recursive) is held by the Present hook from before the depth-share tick to after the
  runtime's present tail. That covers every XR frame call and all of the hook's D3D11 work.
- The MSW thread wakes when no `xrEndFrame` has happened for `lead` x the period (default 0.75). It
  `try_lock`s: if the hook holds the lock, the game is filling this slot.
- It also stands down when: the session is not FOCUSED, detached, a frame is open, a pace wait is
  outstanding, the last real frame was not a stereo projection (menus, cinematics), or the method is not
  AFW (`msw_blocker`).

**One synthesized slot (`msw_cycle`).**
1. `xrWaitFrame`, `xrBeginFrame`, then locate the views at the slot's time. The game's own pose lanes and
   history are untouched: the locate is local.
2. Per eye, `afw::synth_eye`: the eye's own last image and depth, rebuilt at the slot's eye position.
   - The image keeps its own orientation. The compositor reprojects rotation exactly at every depth, so
     only translation needs depth.
   - The world moves by the game's matrices: the camera moves by the head's translation plus the body's
     walk. The walk is extrapolated from the last two images' c5 pair, less what the XR eye poses explain.
   - The body's turn is extrapolated the same way and applied to the submitted pose.
   - The hands are held fixed in tracking space (the mask).
3. Fallback: the eye's own image copied with its own pose. If an eye cannot be written at all, the last
   real layer set is re-submitted (never an empty frame: that shows black).
4. The last real frame's quads (HUD, aim beam and dots) are re-submitted. The compositor draws them at
   their own poses (head-locked ones in VIEW space), so the HUD never smears.
5. With the depth layer on, each synthesized eye carries its own depth.

**D3D11.** The immediate context is made multithread-protected (`ID3D11Multithread`). MSW brackets only its
own draws with `Enter`/`Leave` and restores the pipeline state it touched. XR calls are never made inside
`Enter`: a runtime that uses the context from another thread could otherwise deadlock.
- History: session 34 crashed moving `xrEndFrame` off the present thread with neither the lock nor the
  protection.

**DLSS.** It runs only on real frames, so a synthesized slot shows DLSS's output. The foreground bias
(#159) still applies to every real frame.

## 3. Evidence

- Host (`tools\afw-warp-host.ps1`, 36/36 with the hands case). New case: running 5 cm per 10 ms, the left image at 0 ms, the
  right at 10 ms, slots at 20 ms.

  | Case | World within 1.5 px | p95 | Hands correct |
  |---|---|---|---|
  | Left eye, 20 ms old image | 99.8% | 0.01 px | 100% |
  | Right eye, 10 ms old image | 100% | 0.00 px | 100% |
  | Control: extrapolation off | 19.9% | 9.5 px | - |

  About 2% of pixels show the wrong surface: the disocclusion strip behind the pillar, as the geometry
  predicts (10 cm of travel at 1.5 m against 8 m is about 14 px). Rung 2 fills it from the other eye.
- **Not yet run:** the thread and the frame-loop handoff. The simulator can answer "does it run, fill the
  slots and never go black" without a headset; that launch needs a yes.

## 3b. Run 9 - the first run against a runtime (2026-09-29, build v1.0.1-173)

**MEASURED.**
- The thread ran for about 20 s: no crash, no failure, no black frame, 0 refusals.
- It throttled the game. Before MSW the game made 106-135 presents/s (DLAA off). With MSW on:
  - `msw: 68 slots/s synthesized beside 76/s from the game`;
  - `70 beside 71`;
  - `72 beside 72`.
- Cause: the thread filled any slot with no frame ended for 0.75 of a period (5.2 ms at 144 Hz). A game
  at 7.4-8.4 ms per frame always arrived just after, and then waited for the next slot.
- One window read `15 synthesized beside 16/s from the game` with 3568 "game present in progress"
  skips, just before MSW was turned off. Unexplained (the F10 panel open?).

**FIX (host-untested; needs the next run).**
- **Whether:** only while the game's own frame time exceeds 1.3 periods, going off again under 1.15.
  The frame time is the Present hook's start-to-start interval, less the time it spent blocked on the
  frame loop and in `xrWaitFrame`.
- **When:** a slot is filled only when the runtime's wake-up for it (the last `xrWaitFrame` return plus
  one period) has passed by 0.15 of a period and no game frame has begun.
- `vrpace msw lead <1.0..1.8>` sets that margin.
- New log lines: `msw: ENGAGED` and `msw: standing by`, each with the frame time in ms and in periods.

**Prediction:** with DLAA off (about 120 fps) MSW stands by, and the game keeps its rate. With DLAA on
(about 90 fps) it engages, and the game runs about 72 real plus 72 synthesized frames.

## 3c. Run 10 (2026-09-29, build v1.0.1-178) and what it changed

**Reported:**
- MSW works, with visual issues.
- The reticle does not ghost (it does under VD's SSW).
- With the walking/turning prediction on, the world is smoother but the hands jitter while turning.
  With it off, the hands are normal and the world judders while turning.
- About 125-130 fps with MSW, against 140-144 with VD's SSW.

**MEASURED (log):**
- Engaged, the game made 76-93 presents/s and MSW synthesized 36-60 slots/s: 118-136 in all, never 144.
- The game's own frame time was 7.9 ms standing by and 9.1 ms engaged. MSW's GPU work cost the game
  about 1.2 ms a frame, which moved it across the 1.3/1.15 thresholds: the policy flapped about once a
  second.

**Why VD's SSW reaches 144:** it runs on the headset, so it costs the PC nothing. When engaged, it
locks the application to half the refresh. OFXR-Bridge (an OpenXR layer doing colour-only optical-flow
frame generation) does the same: the application runs at half rate and the layer inserts the rest, with
the flow at 50-75% resolution to save GPU.

**Changed:**
1. **The turn goes into the image.** Before, the extrapolated body turn rode the submitted pose and
   turned the hands with the world. Now the world turns in the image (a rotation of the camera-relative
   point on the matrix path, the yaw rows on the XR path), and the foreground ignores it.
   - Host: a 2 deg/10 ms turn, slot at 20 ms, image-space score:
     - matrices: world 92.4%, hands 100%;
     - XR model: world 92.4%, hands 100%;
     - control (no extrapolation): world 0%.
   - About 7% of that world is unseen by a 20 ms old image.
2. **The half-rate lock (default on under MSW).** After every real frame, the thread takes the next slot
   at once. The game settles at half the refresh with 13.9 ms per frame, and every other slot is
   synthesized; the missed-slot filler stays underneath. The adaptive policy (lock off) gains a 3 s dwell.
   - `[VR] ModSpacewarpHalfRate`, `vrpace msw half on|off`, F10.
3. **A cheaper synthesis.** The seed grid for a slot goes from 2 to 4 texels (`vrpace msw grid <n>`).
   Host at 2750x2850, 20 back to back:

   | Seed grid | Per eye |
   |---|---|
   | 2 | 1.02 ms |
   | 4 (default) | 0.74 ms |
   | 8 | 0.61 ms |
   | 16 | 0.56 ms |

   The full two-source AFW rebuild is 1.50 ms. The accuracy cases pass at 4.

**Prediction:**
- Lock on: `msw: ~72 slots/s synthesized beside ~72/s from the game`, 144 in all.
- The hands steady while turning, with the prediction on.

## 3d. Run 11 (2026-09-29, build v1.0.1-184)

**Reported:**
- Better in many ways.
- Trails behind world geometry, worst while turning.
- Nearby objects' textures pulled along with the hands.

**Measured:**
- The foreground mask never engages: the arms pass binds the scene target in no render-target slot. The
  foreground is therefore "nearer than the depth limit" (about 0.7 m), which takes in nearby walls and
  tables.
- With the hands following their controllers, those moved with the hands.
- The trails while turning: see FLICKER_REFERENCE, AFW run 11 (the matrices refused in fast turns).

**Fixed:**
- A foreground point follows a grip only within 30 cm of it.
- Host: without the mask, a bar 0.3 m away and 0.4 m from the grip stays put, with the hand still 100%
  right. The control (no radius) moves the bar: 0% of it stays.
- The AFW turn fixes are merged in.

**Open:** the hands at the game camera's FOV (108 deg against the world's 103). The mask exists partly to
reproject them with their own FOV. Drawing them at the world's FOV would remove that class of fault;
not changed yet.

## 4. What to expect and how to read it

- **Pacing:** a 90-capable game on a 144 Hz display is paced to the slots it can make. Expect roughly 72
  real plus 72 synthesized, steady, instead of 90 irregular plus runtime-synthesized. The log line gives
  both rates:

      msw: N slots/s synthesized beside M/s from the game (144 Hz display) | eyes rebuilt, copied | skipped ...

- **GPU:** one seed map and a compose per eye per synthesized slot. This is ESTIMATED from the host (1.54 ms
  for a full two-source rebuild at 2750x2850): about 1 ms per eye, about 2 ms per slot, about 11% of the
  GPU at 54 slots/s. Not measured in the game.
- **A/B:** turn SSW off in Virtual Desktop, then:
  - `vrpace msw on|off` (or the F10 AFW box);
  - `vrpace msw extrap off` (head-only synthesis);
  - `vrpace msw lead 0.6..0.9`.

## 5. The ladder

1. **(built)** Own-image synthesis with walk and turn extrapolation, HUD layers re-submitted, depth layer.
2. Disocclusion from the other eye's image (the AFW two-source compose with the synth target).
3. **(built 2026-09-29)** Hands: each foreground pixel moves rigidly with the nearer grip, from the image's
   grip pose to the slot's.
   - Each image carries the grips of the view set its head sample came from (`note_hands`, a 16-entry
     history keyed like `g_viewHist`; `vrpace msw handgen <n>` shifts the match).
   - The slot's grips are located at its display time.
   - Quads placed within 30 cm of a grip (wrist HUD, the aim dot at the hand) move with it.
   - Default off: `[VR] ModSpacewarpHands`, `vrpace msw hands on|off`, F10.
   - Host: the controller moved 3 cm; hand pixels 100% right with it on, 0% with it off (the control).
   - Unverified: whether the image's hands were drawn from exactly that generation's grip. A
     one-generation mismatch would show as hand judder at the game's rate; `handgen` is the lever.
4. Moving characters: object motion. The game draws no velocity buffer (`MotionBlur=False`). The routes:
   - enable UE3's velocity pass with the blur amount at 0;
   - optical flow in the x64 helper: NVIDIA Optical Flow SDK on RTX, or FidelityFX's MIT-licensed optical
     flow from FSR 3.
   - The same vectors fix DLSS's smearing of moving characters.
5. HUD quads re-posed for the slot. Head-locked ones are exact; hand-held ones follow their grip (with rung
   3). Body-anchored ones are still one slot old.
6. Cost: synthesize at a reduced size, or reuse the seed map across both eyes.

## 6. Risks recorded before the first run

- A nested Present (the mirror) would deadlock a plain mutex; `g_cycleMx` is recursive.
- Any D3D11 work on the game thread outside the Present hook (a draw-hook callback) is serialized by the
  multithread protection. MSW restores the state it changed, so the interrupted sequence continues. MRT
  bindings beyond slot 0 are not restored; no such sequence is known outside the hook.
- If the held eye has no AFW image, a real present after a synthesized slot can pair a synthesized image
  with a real pose. This happens only at start-up, before AFW has an image.

## Sources

- [VD SSW runs on the headset](https://www.uploadvr.com/virtual-desktop-synchronous-spacewarp/)
- [Khronos: SSW with OpenXR](https://www.khronos.org/news/permalink/a-virtual-boost-in-vr-rendering-performance-with-synchronous-space-warp-using-openxr)
- [ASW 2.0 positional timewarp from depth](https://developers.meta.com/horizon/blog/developer-guide-to-asw-20/)
- [Application SpaceWarp: depth and motion vectors](https://developers.meta.com/horizon/blog/introducing-application-spacewarp/)
