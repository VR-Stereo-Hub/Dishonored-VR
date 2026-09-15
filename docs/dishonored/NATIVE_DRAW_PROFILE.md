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
