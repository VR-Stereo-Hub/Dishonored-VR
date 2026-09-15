# Native draw CPU profiling (VR-121 continuation)

Branch codex/vr-121-native-draw-profile starts independently from current main
18ae4ebda. Extends the existing render-thread profiling ticket with native draw
and draw-state call attribution. The earlier reflection profile, bridge GPU
profile and diagnostic A/B branches remain recoverable, not dependencies.

## Why this test

The verified hub diagnostic comparison showed no compelling gain (63.66/64.48/
64.27 fresh pairs/s baseline/reduced/baseline). GPU conversion and XR eye copies
averaged about0.10 and0.057ms/eye. Prior reflection plus bytecode CPU cost was
about5ms/s. The larger native rendering/driver spans remain unattributed.

This test reuses the bounded VR-121 sampler at eight existing call boundaries:
indexed and primitive hook totals; their raw/native D3D9 calls; vertex-shader
constant hook and native call; render-target hook and native call. Hook totals
include callbacks, nested native API work and instrumentation overhead. Native
calls include driver/runtime CPU work and any wait within that call. They do
not measure asynchronous GPU execution. State changes, buffer locks, engine
render-command processing and API calls outside these boundaries are not covered.

Counters include every eligible render-thread call and randomly time1/64 with
xorshift to avoid fixed eye/draw stride alias. Off costs an enabled check. Enabled
also checks owner thread and updates bounded counters. No new engine/GPU memory
reads or writes, native hook sites, flushes or waits. No draw return or arguments
changed. No cache, quality, pose, HUD, hand correction or synchronization change.

The3-second report lists calls, sample population, sampled mean/max and estimated
inclusive ms/s. Multiply mean by total calls/window seconds for that estimate.
Hook and native samples are independent cohorts: NEVER add nested estimates or
subtract their sample means as an exact per-draw mod cost. Mod-generated native
calls can occur without a matching outer game hook, and some hooks suppress or
multiply draws. Maximum is sampled, rare stalls may be missed. No performance
gain or CPU/GPU saturation diagnosis is claimed before observing the run.

Perf.NativeProfile=0 is the missing-key, generated and release default;1 enables.
F10 Display toggles it live; existing Save persists. The Present thread establishes
ownership. Gameplay/menu or enabled-state transitions discard partial windows.
The old A/B code is absent on this main-based branch; installed DiagnosticAb will
be set0 as well. Existing accepted HUD, tracking and other diagnostics stay intact.

## Validation and test

Production host tests cover sampling population, bounded/reset statistics,
idempotent scope close, disabled/foreign-thread refusal and window/context reset.
Release build, lint, golden and9 exports are checked before installation. No game
or game-based simulator launch. Standalone host clock/log substitutions validate
the sampler contract, not the real workload's timing or perceived overhead.

One test: load the same hub save at120Hz/current resolution, draw both weapons,
stand in the same spot facing the busy view for60 seconds after controllable
play begins. Avoid menus, walking or combat. Then briefly turn the head and move
both hands. Question: do the world and both weapons still look and track normally?
Normal behavior permits using the log to rank the sampled paths. Any new visual
or tracking issue rejects the diagnostic. No automatic phase changes, no need
for a two-and-a-half-minute run. Agent reads and archives the matching build log.

Next decision: materially expensive hook totals with small native calls point
toward mod CPU work; expensive native-call spans point toward driver submission
or waiting, requiring GPU/CPU correlation before identifying the cause. If both
are small, examine uninstrumented engine/API work rather than claiming a solution.

## Installed candidate

Installed and hash-verified build `vr33-hands-working-295-gf4062ba4b`, compiled 21:46:15. Release build and host checks passed. Full INI comparison: only NativeProfile=1 added and DiagnosticAb changed from1 to0; CRLF preserved. Both previous logs and binaries archived in `C:\dev\Dishonored-VR\build\playtest-candidates\installs\20260914-214922-606852`. Game not launched.

Source commit: `f4062ba4b442f57db86b6aa44edacec297f15f04`.
DLL SHA256: `7deb90501bda330b7e22a31f3ea27fc06dd95f60382853f70ef1ecafcae90566`.
INI SHA256: `74d092be1ac4fa29e5b1b06fadc0b100a686664fdda82eb2e1986b556dc1241b`.
Headset validation completed: no new visual or tracking problems reported.

## Hub result, 2026-09-14

Verified installed DLL hash and matching build/compile banner before analysis.
Runtime period was 120 Hz. Archived both logs, current INI and install identity
in `build/performance-results/native-hub-20260914-215400`.
34 complete gameplay windows cover 102.136 seconds. These include the full
gameplay capture, not a separately marked stationary-only segment.

| Scope | Calls | Samples | Sample mean (us) | Estimated inclusive ms/s |
|---|---:|---:|---:|---:|
| indexed-hook-inclusive | 24,631,125 | 385,155 | 0.668 | 161.08 |
| primitive-hook-inclusive | 51,092 | 800 | 2.589 | 1.30 |
| native-indexed-call | 24,684,073 | 386,009 | 0.286 | 69.18 |
| native-primitive-call | 51,092 | 800 | 1.274 | 0.62 |
| vs-constant-hook-inclusive | 73,958,051 | 1,155,955 | 0.138 | 99.84 |
| native-vs-constant-call | 74,255,542 | 1,160,726 | 0.081 | 58.50 |
| render-target-hook-inclusive | 1,058,669 | 16,325 | 0.285 | 2.95 |
| native-render-target-call | 1,798,633 | 28,061 | 0.152 | 2.68 |

Means are sample-count weighted; ms/s estimates are duration weighted across
windows. Indexed hooks and constant hooks dominate these measured boundaries,
but are insufficient to explain the whole render interval. Late logged pair
intervals remain about 15-16 ms. These are diagnostic timings, not an improvement
comparison. Native API spans measure CPU wall time, not GPU execution.
Nested scopes must not be added, and different call populations prevent treating
the hook/native difference as an exact removable cost. Sub-microsecond timings
also include timer overhead. No evidence here justifies replacing synchronization
or compromising the accepted tracking path.

Next: attribute the remaining engine rendering/API work and waits, including
buffer locks and uninstrumented state calls, before selecting a performance
change. Keep profiler default off. No further repeat of this capture is needed.
