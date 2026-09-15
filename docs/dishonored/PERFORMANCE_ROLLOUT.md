# Performance rollout

## Current work, 2026-09-14

PR59 / VR-113 contains the overall audit. PR60 / VR-115 implements the first
candidate, reduced or disabled desktop presentation. Implementation of the
larger plan is now authorized; no performance merge is authorized.

Branch `codex/vr-115-performance-rollout` starts at VR-Main `85f9ef6e4` and ports
PR60 source `160949cb3` as `bc6e2230f`. It retains the accepted image-linked world
orientation, world/hand lag 2 and current camera/weapon behavior. PR63 HUD work
remains independent: its initial playtest was reported favorable, and its
contributor continues it. Neither PR63 nor the rejected PR62 hand experiment
is included here. The latest PR63 log banner matches build282, compiled18:53:23;
its timing windows are not a controlled performance baseline for this branch.

## First candidate and measurement contract

Full remains the default. `VR.ReduceDesktopPresent=0` and
`VR.DesktopMirrorOff=0` have live F10 Display controls and the existing
`desktoppresent full|reduced|off` command. See
[DESKTOP_PRESENT_PERFORMANCE.md](DESKTOP_PRESENT_PERFORMANCE.md) for guards,
current-stream submission, failed approaches and native-driver evidence.

New `Perf.DesktopAb=1` arms a bounded trial; its absent-key default is0.
F10 Display can start or stop it. It disables the legacy latency sweep.
After ten seconds of uninterrupted strict gameplay, the trial runs three
30-second segments: Full, Off, Full. The first three seconds of each segment
are discarded. Pausing, a note, a load or a cinematic aborts the comparison
and restores the pre-trial desktop flags. Completion and explicit stop also
restore those flags. No resolution, quality, lag or image pose setting changes.

Successful eye-copy/release publishes the delivered capture serial. Only a
successful xrEndFrame with two separate game-eye projection swapchains and
both serials advancing contributes a fresh-pair observation. Mono, held,
replayed, one-eye-only and failed submissions are not counted as new pairs.
Intervals span rejected submissions, so stalls remain visible. This measures
application submission cadence, not physical display refresh, GPU completion,
photon latency, or a proof that both eyes belong to the same game simulation tick.
Existing pose/image-age and desktop fallback logs must be checked alongside it.

Storage is bounded to16384 intervals per segment; overflow invalidates the
segment. Severe stalls stay in the quantiles. Logs report count, mean/rate,
p50/p95/p99/p99.9/max and counts over8.333/16.667/33.333ms. At least32 samples
and no overflow are required. The returned Full baseline establishes a noise
floor for each metric; no automatic FPS-benefit verdict is printed.

The old GPU line subtracted post-entry capture from the earlier render span
and subtracted a GPU interval from CPU lock time. Those independent intervals
now carry accurate labels without a fabricated recoverable-cost calculation.
Old `perf/ab` Present-parity statistics remain historical and are not the new
fresh-pair population. Broader mixed tagged/untagged accumulation and GPU-stage
coverage remain in Phase A.

## First headset test

Load the same sewer save, face a quiet stationary view and stay there for
110 seconds after gameplay appears. Keep headset refresh120Hz and current
resolution, avoid menus, movement and combat during the timed portion.
The desktop may freeze in the middle30 seconds; the headset should remain
stereo and responsive. After the timed portion, a brief head turn can check
that the view is still responding. One question: did the headset remain
correctly stereo and responsive throughout? A pass permits timing comparison;
a failure rejects the candidate regardless of its frame-time numbers.
The agent archives and reads the logs, not the tester.

## Full plan, with explicit remaining work

- [x] Restore the audit on current main and preserve the accepted world fix.
- [x] Port guarded desktop modes and add a repeatable Full/Off/Full trial.
- [x] Count successful fresh-eye submissions; fix misleading GPU subtraction.
- [ ] Measure Full/Off/Full on the headset; repeat promising results and test Reduced separately.
- [ ] Complete Phase A: asynchronous D3D11 bridge GPU timestamps, missing D3D9
      mirror/pass timings, mixed-population accounting, per-eye age and bounded
      CPU sampling/scheduling evidence (VR-67, VR-17).
- [ ] Compare pixel load at fixed aspect/FOV if GPU work owns the deadline;
      do not silently lower the accepted quality or resolution.
- [ ] Implement bounded shader-layout caching with real shader lifetime identity,
      negative results and eviction; measure parse calls and CPU cost.
- [ ] Share lazy per-draw state snapshots; then reduce animation-weight locking
      with coherent per-frame publication and immediate ownership handback.
- [ ] Measure individual diagnostics, starting with Cine.Trace; keep failure
      breadcrumbs and separate collection costs from text-output costs.
- [ ] Measure/coalesce liveness rebuilds, name/property discovery and event work;
      current-level IsLiveObject and menu revalidation remain mandatory (VR-102).
- [ ] Repair shared-fence timeout/error ownership before bridge/slot changes
      (VR-114); then measure direct-to-XR copy and queue scheduling candidates.
- [ ] Profile managed-texture uploads, dirty regions and32-bit memory pressure.
- [ ] Identify expensive native passes; share only verified eye-independent work
      and preserve visibility, transparency, weapon depth and lens consistency
      (VR-79). Upscaling/dynamic resolution are later quality tradeoffs.
- [ ] Confirm chosen wins in opening, heavier levels, menu/load/cinematic/weapon
      transitions and physical head movement. One question per launch.

The ordered phases and counterpredictions in PERFORMANCE_AUDIT.md remain the
full specification. One lever per candidate: a reduction in API call count
alone is not a performance result. No shader, liveness, bridge, texture-streaming
or scene-sharing optimization is claimed implemented by this first build.

## Validation

Standalone production benchmark tests cover serial advancement, replay/mono/
one-eye rejection, wraparound, warmup, repeated baselines, severe stalls,
bounded overflow, explicit restart/stop and menu handback. Desktop policy:
79339 assertions plus legacy negative control; production copy/tail:431857;
native D3D9Ex:120 GPU completions/pixel checks, Full return and ResetEx.
Reentry248 and single-tag23 checks pass; frame/weapon/animation/image-orientation
suite passes. Win32 release build, lint and golden INI check pass. Simulator
launch is not run because the user prohibits launching the game.

Installation identity is recorded below after packaging. Headset benefit is
pending. Release defaults remain Full; the installed trial alone arms DesktopAb.

## Installed first trial

Build `vr33-hands-working-276-g48a632e48`, compiled19:23:53, source48a632e48.
DLL SHA256 `e651d5818ca89c0d7f0c888c9d6b24c4e561032091dfeb7e9ffae61fe4195402`.
INI SHA256 `7d0dc649ea56ff4e17906a30f56325480c5cb6457dfa513ce2f9abd78a437a13`.
Bundle: `build/playtest-candidates/performance-desktop-rollout`.
Previous DLL, INI and both logs: `build/playtest-candidates/installs/20260914-192446-013854`.
The full CRLF-verified INI diff adds only Perf.DesktopAb=1,
VR.ReduceDesktopPresent=0 and VR.DesktopMirrorOff=0. Current PR63 HUD keys are
retained but inert in this main-based build. No game launched. Next log must
match276-g48a632e48 /19:23:53 before interpreting the trial. Build numbers
reflect branch ancestry;276 here does not mean the old rejected hand candidate.

## Publication pending explicit approval

The installed trial and local commits are complete. Automatic approval review
rejected the feature-branch push, including after origin was verified as the
public VR-Stereo-Hub/Dishonored-VR repository, because it requires explicit
user authorization for this exact source/documentation export. No push or new
PR occurred. The prepared PR body is local at build/performance-rollout-pr.md.
VR-115 was updated successfully and remains In Progress. Ask permission to
publish this branch and open its draft PR; do not bypass the review rejection.

## First headset comparison, 2026-09-14

Verified installed DLL hash and log banner276-g48a632e48 /19:23:53. Both logs
and INI archived at build/performance-results/desktop-first. All three30s
segments completed; each discarded3s warmup, no overflow, all valid.
The tester reported no headset problem during observation, but did not watch
continuously. This is limited perceptual evidence, not exhaustive acceptance.

| Metric | Full first | Off | Full return |
|---|---:|---:|---:|
| Fresh pair intervals |2349|2652|2287|
| Fresh pair rate /s |87.01|98.28|84.72|
| Mean ms |11.492|10.175|11.804|
| Median ms |10.759|7.894|10.711|
| p95 ms |18.878|21.250|20.486|
| p99 ms |37.350|35.166|40.814|
| p99.9 ms |80.168|85.976|88.825|
| Maximum ms |100.340|103.410|100.523|
| Intervals over16.667ms |158 (6.73%)|279 (10.52%)|197 (8.61%)|
| Intervals over33.333ms |28|31|39|
| Rejected submissions |12|25|7|

Off improved observed throughput by12.95-16.01% over the two Full segments,
and median interval by about26.5%. Tail evidence is mixed: p95 is worse than
both baselines, over16.667ms share increases, p99 is lower, and p99.9/max
do not establish a consistent improvement. Do not call this a smoothness fix
or stable120Hz. Rejected submissions also increased and need follow-up.

The Off desktop windows show5887 omitted Presents and28 native fallbacks,
all context refusals; no query/nonOK failure. Off freezes the last image,
not a black screen. Sampled world orientation still matches captured input;
left/right fallback totals stayed0/2. Held-layer diagnostics during Off
report black=0. These do not measure physical image age or prove the absence
of every transient. Full was restored at completion; installed defaults were
not promoted and no new build or INI edit was made after this run.

Next: repeat the identical scene to confirm throughput/tail changes before
promoting Off; compare Reduced as a separate controlled candidate if the
Off tail penalty repeats. Continue the remaining Phase A timing/CPU work in
parallel with the broader rollout; do not treat removed Present CPU time as
fully recovered render budget. Publication remains blocked pending explicit
user permission, as already recorded. No merge authorized.

## Repeat result and Reduced trial

Second run verified276-g48a632e48 /19:23:53, archived with both logs and INI
at build/performance-results/desktop-second. All three segments completed,
valid with no overflow. This report did not include a new visual verdict.

| Metric | Full first | Off | Full return |
|---|---:|---:|---:|
| Fresh pairs/s |81.86|95.62|84.02|
| Median ms |11.031|8.108|10.907|
| p95 ms |21.364|22.440|21.309|
| p99 ms |42.446|36.959|36.806|
| p99.9 ms |100.051|65.953|75.390|
| Max ms |101.346|93.974|80.132|
| Intervals over16.667ms |193/2209 (8.74%)|267/2581 (10.34%)|195/2268 (8.60%)|
| Rejected submissions |11|29|12|

Off omitted5697 desktop Presents,31 context fallbacks. Its throughput gain
of13.8-16.8% repeats, as do worse p95, higher16.667ms exceedance share and
more rejected submissions. p99 is within the baseline range this time.
Do not promote Off as a smoothness fix. Full remains the default.

Next candidate changes only the benchmark's alternative: Perf.DesktopAb=2
selects Full/Reduced/Full;1 retains Full/Off/Full;0 or an unknown value is off.
F10 Display selects the alternative while stopped. Duration, warmup, fresh-eye
sampling and restore behavior are unchanged. Actual desktop policy is unchanged
from the tested PR60 port. Production host tests now cover all three Reduced
phases and restoration as well as existing Off tests.

The one-question test is the same110-second quiet sewer view at120Hz, then
a brief head turn: does the headset remain correctly stereo and responsive?
Reduced should keep the desktop updating. Logs must prove eligibility and actual
omitted Presents before calling this an effective reduced-delivery comparison.
A gain with more held images or visual faults fails; no gain means this path
is not an adequate solution and the audit proceeds to CPU/GPU discrimination.
