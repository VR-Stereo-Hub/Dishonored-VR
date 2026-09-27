# Next session: make the game stop limiting itself (the "uncap" deep dive)

Read CLAUDE.md, the newest four sections of docs/STATUS.md, this brief, and in
docs/dishonored/PERFORMANCE.md (grep the headings, read windows; the file is large): "Why DLSS
cannot raise the frame rate here", both "Route 1" / "Route 2" entries, "in the HEADSET the GPU is
the limit", plus the older "Results and routes", "Representative CPU evidence" and the VR-160
section (rig B) for what was already tried and failed. Session protocol applies: new work on its own
branch, measure before theorising, every lever default OFF with a live A/B, no merges.

## The goal, in the maintainer's words turned into a requirement

The game must use the hardware as hard as it can, so a faster CPU/GPU actually turns into a higher
frame rate or a higher resolution. Today it does not: in the headset the machine sits at about
**25 % CPU and 80 % GPU** (Task Manager, headset run 2026-09-27, no SSW, no DLSS, 100 % = 2750x2850,
VDXR at 144 Hz) while the rate stays at **~120-137 pairs/s** and the runtime reports
UNDER-SUBMITTING 0.89x. Nothing is saturated, so the rate is set by SERIALISATION - one critical
path where the CPU waits for the GPU and the GPU waits for the CPU - not by any one resource.

## What is already measured (do not re-derive)

- Headset, per stereo pair (log `build/dlss-install/pre-pe2-*` / the maintainer's logs, perf lines):
  7.3-8.1 ms total; D3D9 GPU span 6.7-6.9 ms; present thread waiting on the capture fence ("cap
  lock") 1.1-1.9 ms on the LEFT present; render thread R 1.3-2.7 ms per present; P1 idle 0.1-1.7 ms.
  Treat the D3D9 span as an UPPER bound on GPU busy (it contains gaps).
- Simulator (no Virtual Desktop encoder/compositor): the game thread looked like the ceiling at
  ~7 ms/tick; the mod's ProcessEvent hook cost ~500 ms/s of it (route 2, branch
  `claude/pe-hook-dispatch`, PR #141, cut to ~395 ms/s: ~155 -> ~172 ticks/s in the simulator,
  no headset gain because the headset is not game-thread-bound). This branch is stacked on it, so
  `pe/cost` (and `pe fn on`) are available.
- Route 1 (extra stereo pair per tick, PR #140): no headset gain, halves the tick rate, weapon and
  hands judder. Rejected; do not revisit.
- A quarter of the pixels saved only ~0.7 ms of GPU per eye: the GPU cost barely follows
  resolution, so the "80 % GPU" is not pixel-bound shading.
- Already tried and failed on some rig (check whether the reason still applies before retrying):
  nonblocking desktop Present, max-frame-latency 1/2/3 sweep, the query-wait helper, pair pacing as a
  default, dynamic shadows via ini. Desktop mirror OFF was a real win and is the default.

## The questions, in order

1. **Where does the GPU sit idle 20 % of the time?** Take a real GPU timeline of a headset-equivalent
   run: PIX timing capture or GPUView/WPR (Windows Performance Recorder, GPU + CPU + DWM providers)
   around 5 s of steady play, short capture, with an overhead control. Name every gap on the game's 3D
   queue and what the CPU was doing in it (our present path, the capture copy, the fence wait,
   xrWaitFrame, the D3D9 driver, Virtual Desktop). `tools/perf-gpu-sample.ps1` gives per-engine busy
   share per second beside the log - use it for every A/B.
2. **Which waits are ours and can be moved off the critical path?** Candidates to MEASURE, not
   assume: the capture fence wait on the left present (the image is consumed a present later - can
   it be deferred so the game renders the next eye while the copy finishes?), D3D9 -> D3D11 shared
   surface sync per present, the HUD capture, clarity/sharpen passes on the present thread, the
   runtime's xrWaitFrame/xrEndFrame placement (the present thread owns every runtime call), the
   game's own Present to the desktop window per eye.
3. **Can the pipeline be deeper?** The game renders one frame ahead (OneFrameThreadLag); the render
   thread then waits on our present. Look at a double-buffered capture/submit where the present thread
   never waits for the GPU: capture slot N+1 while N is copied, submit N one present later with its
   own pose record (the pose travels with the image - VR-65 - so latency can be accounted for).
4. **Driver/runtime levers with zero code:** NVIDIA Low Latency Mode / Max Frame Rate / Threaded
   Optimization per program, Virtual Desktop codec/bitrate/"prioritise" settings, VDXR vs SteamVR,
   HAGS, and Windows Game Mode, each as an A/B with the numbers.
5. **Scaling test that proves "uses the hardware":** at 100 %, 150 % and 200 % render size, and with
   the GPU clocks capped, does the rate move the way a GPU-bound or CPU-bound game would? A game that
   uses the hardware fully shows the rate falling with resolution and rising with a faster card.

## Rules for this work

- Measure first; one lever per A/B, off/on/off; report the median and the tail per window.
- Every new lever default OFF with a live A/B toggle (seam word + ini key; F10 if player-facing).
- Launch the simulator only when the question cannot be answered offline, close it as soon as the
  question is answered, back up and restore the INI byte-for-byte around every run; headset runs are
  the maintainer's - hand over exact steps and what to read.
- Record every result, including failed predictions, in PERFORMANCE.md in the same commit.
- Commits: BioVRDev identity, conventional, no trailers, 2-10 per branch; no personal names anywhere.
- Never merge; PRs against `staging` only when a lever has a measured result.

## Installed state at handoff

Route 2 build `fcbc0c59` (`claude/pe-hook-dispatch`), the maintainer's own INI (DLSS/DLAA off,
2750x2850, SSW off in Virtual Desktop). Backups under `build/dlss-install/pre-*`.

## Copyable starting prompt

Continue in C:\dev\Dishonored-VR\build\worktrees\fsr on branch claude/uncap-deep-dive (stacked on
claude/pe-hook-dispatch). Read CLAUDE.md, the newest four sections of docs/STATUS.md,
docs/dishonored/NEXT_SESSION.md and the PERFORMANCE.md sections it names. Goal: make Dishonored VR
use the hardware as hard as it can - in the headset the PC sits at ~25 % CPU and ~80 % GPU while the
rate stays at ~120-137 pairs/s, so find and remove the serialisation that limits it. Start by getting
a real GPU/CPU timeline of the frame (PIX or WPR/GPUView) and name every gap on the game's 3D queue
and who owns it, then A/B the waits that are ours, deepen the pipeline where it is safe, and test the
driver/runtime levers. Measure before changing anything; every lever default off with a live toggle;
record every result in PERFORMANCE.md. Build and install yourself; ask before launching anything;
close the game as soon as a question is answered; restore the INI after simulator runs. Do not merge.
