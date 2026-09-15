# D3D11 bridge GPU measurement

VR-123, branch `codex/vr-123-bridge-gpu-profile`, independent from accepted main
18ae4ebda. No main merge is authorized. The prior desktop and render-thread
profiling branches remain intact and are not dependencies of this candidate.

## Why this experiment

The VR-121 profile measured reflection at 2.801 ms/s and bytecode reads at
2.203 ms/s. These are milliseconds per second, not per frame. Inclusive weapon
routing was 67.873 ms/s, with nested work that must not be summed. A complex
shader lifetime cache is lower priority than measuring the still-unattributed
D3D11 bridge. VR-115 desktop alternatives did not consistently improve tails;
the normal desktop path stays active. Current main's HUD behavior was reported
good before this experiment. No performance improvement is claimed here.

## Measurement contract

`Perf.BridgeGpu=0` is the generated, release and missing-key default. Set 1 to
collect samples. F10 Display offers a live checkbox; the existing Save action
persists it. The installed playtest profile enables it without changing the
current HUD or any other setting. No automated A/B or rendering changes occur.

Two independent stage labels:

- `conversion`: only the fullscreen blit from the captured texture into the
  RGBA intermediate. Excludes the later overlay, native hand helper, frame-ID
  thumbnails and existing shared-ownership flush.
- `eye-copy`: only the GPU CopyResource into the acquired OpenXR image. Excludes
  acquire/wait/release, compositor, encoding and HUD swapchain copies.

At each native Present a xorshift gate selects either stage with probability
1/16 each, or neither. At most one disjoint bracket is issued per Present;
there is no fixed-stride eye alias and missing stages do not transfer their
sample opportunity to a different stage. D3D11 does not present independently:
the unit here is a native eye Present, not a fresh complete stereo pair.

Each stage has 16 reusable slots (48 queries), allocated lazily. A pending slot
is not overwritten. Poll after at least eight subsequent calls to that stage,
using GetData(DONOTFLUSH), once per eligible slot per call with no retry loop.
Full storage drops a new sample; failed creation disables that stage until
reset. A disjoint frequency, zero frequency or inverted timestamps rejects the
result. Device identity is held by an owned COM reference; runtime teardown
releases every query and the reference without waiting. Disabled startup
creates no queries. A live disable leaves existing closed queries parked until
re-enable or teardown; it never leaves an open bracket.

The 3-second `perf/bridge` report separates stage and eye (-1/0/+1), with
current gameplay/menu context. It reports calls, issued/resolved/pending,
late polls, full-slot drops, invalid results, errors and epoch discards.
Toggle/context changes discard partial statistics and invalidate pending samples.
Pending work can cross reporting windows; issued and resolved are not the same
cohort. Late polls count attempts, not unique stalled frames. Final pending
samples can be lost at teardown. Zero resolved samples mean unknown cost.

Means/maxima cover all resolved samples; p50/p95 use up to 512 stored samples
per stage/eye/window, with explicit overflow. The selected stage interval can
include GPU scheduling effects. It is not total GPU busy time, an end-to-end
frame budget, or guaranteed recoverable time. Stage percentiles must not be
added, and menus or level transitions must not be used as steady gameplay.
Sampling and query insertion can affect scheduling: check diagnostic overhead
before treating a small difference as an optimization.

API references: [D3D11 query semantics](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_query)
and [non-flushing result reads](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_async_getdata_flag).

## Validation

18 policy checks cover timestamp validity, queue saturation, age, context
identity, reuse/reset and stage/eye sampling. Standalone hardware D3D11 test
exercises the production profiler with clear/copy commands: 60 conversion and
67 copy samples resolved, no query errors, context aggregates discarded and
queries released on reset. This is not the game's bridge workload and makes
no performance claim. Only the standalone test submits its own commands with
Flush; the production profiler never flushes or waits.

Win32 release compilation, lint and golden checks pass. The existing unity
DVR_CAT redefinition warning remains. No game or game-based simulator launched.
Final exports pass 9/9; installed identity is recorded below.

## First playtest

Use the laggy hub save at 120 Hz and the current resolution. Face a typical
busy view with both weapons visible for about 60 seconds, avoiding combat
and menus during the measurement. Then briefly turn the head left/right and
look up/down while moving both hands. No wall-facing requirement, no phase
switches and no 110-second schedule.

One question: does the world and both weapons still look and track normally?
Normal behavior accepts the diagnostic for cost ranking. Any new visual or
tracking problem rejects it and requires inspecting the matching build log
before making a performance conclusion. The agent reads and archives the log.

Next decision: if conversion/copy cost is material, design the smallest isolated
bridge optimization. If it is small, profile diagnostics/native draw dispatch
and broader CPU work next. Do not remove synchronization based on a cheap
CopyResource CPU call or infer D3D9 saturation from these D3D11 markers.

## Installed VR-123 bridge GPU profile

Build `vr33-hands-working-295-g63d1cde30`, compiled 21:03:29, source 63d1cde30.
DLL SHA256 `ec410903b25fb54f45a4bb02bf53f330bba1d503883b069e45e96b1b09c76067`.
INI SHA256 `6e1b9832cfd0606ed4c18aa373303c0b8fa670c0acbb7eefd973c0918bf8e916`.
Bundle build/playtest-candidates/bridge-gpu-profile. Prior DLL, INI and both logs
archived20260914-210414-300341. Complete INI diff adds only Perf.BridgeGpu=1;
CRLF and installed hashes verified. 18 host checks, standalone hardware query
lifecycle, release, lint, golden and 9 exports pass. No game launched or merge
permission. Local publication blocked by automatic approval review pending
specific authorization to push the branch and open its draft PR on GitHub.

Next: laggy hub at120Hz, typical busy view, stay in one spot with both weapons
visible for about60 seconds without combat/menus, then brief head/hand movement.
One test question: do the world and both weapons still look and track normally?
Match build295 before interpreting the log. This workload is not directly
comparable to the earlier quiet sewer CPU profile. See BRIDGE_GPU_PROFILE.md.


## Hub measurement result: 2026-09-14

Build295-g63d1cde30 /21:03:29 verified against installed DLL hash and log banner.
Logs and INI archived at build/performance-results/bridge-hub-20260914-210808;
summary.json contains sample-weighted means. Tester reports normal visuals,
tracking and the usual hub lag. Runtime120Hz, no period changes. Gameplay epoch
8627765..8742656 (114.891s), 38 complete bridge windows. This is the reported hub
workload, not proof that every recorded second was stationary.

| Stage | Eye | Resolved samples | Weighted mean ms | Sampled maximum ms |
|---|---|---:|---:|---:|
| Conversion | Left |455|0.10547|2.1859|
| Conversion | Right |441|0.10175|1.8176|
| XR eye copy | Left |451|0.05758|0.7795|
| XR eye copy | Right |412|0.05676|2.6425|

Seven untagged conversion samples are separate (mean0.08614ms). No gameplay
query errors, invalid/disjoint results, late polls, full-slot drops or sample
storage overflow. Window p95 values are not a pooled percentile; raw sample
values were not logged. Sporadic millisecond-scale maxima do not establish
which event or GPU scheduling condition caused them.

Decision: conversion/copy execution is low priority for sustained hub performance.
These small stage means do not explain the large120Hz deficit. This does not
clear ownership waits, runtime/compositor scheduling, HUD copies or unmeasured
D3D9 work. No performance improvement was implemented or claimed.

Late tick windows show roughly14.5..15.9ms with low pacing waits. Native render
execution/wait spans total about9..10ms across two eyes and native Presents about
4..5ms; these are CPU wall spans, not proof of CPU saturation. Some frame gaps
sit in xrEndFrame (for example46.1ms at timestamp8709453), a separate tail cost.
Next experiment should measure diagnostic/native draw-dispatch overhead and
separate scene cost from driver wait. Do not remove capture synchronization or
build a shader cache based on these results. Retain this diagnostic branch.

No build or install change after this run. GitHub publication and Linear result
posting remain blocked pending the previously requested specific authorization;
no blocked external action was retried.

