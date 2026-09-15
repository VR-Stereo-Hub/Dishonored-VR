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

## Follow-up candidate: existing state and resource hooks

Extends this branch and the same default-off NativeProfile switch with20
additional inclusive scopes: vertex/index buffer Lock; texture/cube/volume/
surface Lock and Unlock; viewport, render state, texture, declaration, vertex/
pixel shader, transform and stream-source setters; both user-pointer draw calls.
These measure existing hooks including any native call, shadow upload, accounting
and recursion. They are not exclusive native timings. Same-kind recursive shadow
redirects can overlap; never add these totals. No new hook, resource reference,
engine write or synchronization change. Coverage depends on existing hooks being
installed and calls executing on the Present owner thread. A zero sample count
does not exclude a rare stall or an unhooked call. All scopes retain1/64 sampling.

Validation: host suite checks every scope has a label and receives calls, plus
existing sampling/owner/reset tests. Repeat the same60-second stationary hub
view, then briefly move head and hands; one question is whether the world and
both weapons still look and track normally. No automatic phases.

## Current installed follow-up: resource and state profiling

Build `vr33-hands-working-298-g3d80740a9`, compiled 21:57:19, extends the
visually accepted profiler with20 existing state/resource hook scopes.
Release build, host sampling/label coverage, lint and9 exports passed.
Full installed INI comparison: zero changes; byte-identical CRLF.
Prior DLL, INI and both logs archived at
`build/playtest-candidates/installs/20260914-215824-690251`.
Next: same hub at120Hz for60 seconds, then head/hand movements.
Question: do the world and both weapons still look and track normally?
Normal allows timing analysis; new visual/tracking issues reject the candidate.
Agent has not launched the game. Headset result pending. No merge.

DLL SHA256: `459d4b9c95ade32ca1526521dff1c5df2829e239e895daef1e5184c010265412`.
INI SHA256: `74d092be1ac4fa29e5b1b06fadc0b100a686664fdda82eb2e1986b556dc1241b`.

## Resource/state hub result, 2026-09-14

Installed DLL hash and log banner verified:298-g3d80740a9, compiled21:57:19.
Runtime120Hz; no new visual or tracking problems reported. Both logs, installed
INI and identity archived in `build/performance-results/native-state-hub-20260914-220319`.
38 complete gameplay windows cover114.202 seconds in the final uninterrupted
gameplay segment. Earlier brief gameplay transitions yielded no full windows.

| Scope | Calls | Samples | Estimated inclusive ms/s |
|---|---:|---:|---:|
| indexed-hook-inclusive | 27,955,975 | 437,164 | 163.425 |
| primitive-hook-inclusive | 56,712 | 891 | 1.232 |
| native-indexed-call | 28,026,789 | 438,314 | 69.956 |
| native-primitive-call | 56,712 | 891 | 0.567 |
| vs-constant-hook-inclusive | 84,189,431 | 1,315,320 | 100.680 |
| native-vs-constant-call | 84,543,729 | 1,320,766 | 60.582 |
| render-target-hook-inclusive | 1,166,207 | 18,066 | 2.920 |
| native-render-target-call | 1,985,975 | 31,133 | 2.654 |
| VbLock-hook-inclusive | 56,712 | 879 | 0.515 |
| IbLock-hook-inclusive | 0 | 0 | 0.000 |
| TexLockRect-hook-inclusive | 4,697 | 80 | 0.090 |
| TexUnlockRect-hook-inclusive | 4,697 | 80 | 0.016 |
| CubeLockRect-hook-inclusive | 0 | 0 | 0.000 |
| CubeUnlockRect-hook-inclusive | 0 | 0 | 0.000 |
| VolLockBox-hook-inclusive | 0 | 0 | 0.000 |
| VolUnlockBox-hook-inclusive | 0 | 0 | 0.000 |
| SurfLockRect-hook-inclusive | 0 | 0 | 0.000 |
| SurfUnlockRect-hook-inclusive | 0 | 0 | 0.000 |
| SetViewport-hook-inclusive | 1,864,883 | 29,119 | 1.614 |
| SetRenderState-hook-inclusive | 13,047,264 | 203,840 | 8.981 |
| SetTexture-hook-inclusive | 44,926,834 | 701,769 | 23.680 |
| SetVertexDeclaration-hook-inclusive | 1,809,030 | 28,256 | 1.211 |
| SetVertexShader-hook-inclusive | 2,401,188 | 37,688 | 2.905 |
| SetTransform-hook-inclusive | 0 | 0 | 0.000 |
| SetPixelShader-hook-inclusive | 5,145,715 | 80,588 | 7.555 |
| SetStreamSource-hook-inclusive | 17,115,863 | 267,905 | 10.378 |
| DrawPrimitiveUP-hook-inclusive | 211,543 | 3,248 | 1.386 |
| DrawIndexedPrimitiveUP-hook-inclusive | 4,448,243 | 69,592 | 13.931 |

Duration-weighted estimates, not additive. Sampled vertex-buffer locks average
1.043us (879 samples,8.1us sampled max); texture locks average1.924us
(80 samples,24.2us sampled max). These captured paths do not explain sustained
hub lag. Zero calls only describes this instrument's owner-thread coverage;
it does not establish that no resource of that type was used. Rare stalls can
be missed. Sub-microsecond state timings include timer overhead.

Late broad timing windows remain around15ms/tick, with several milliseconds
per eye in the rendering span and roughly4ms combined native Present wall time.
Neither span proves GPU saturation: engine CPU work and waiting are unresolved.
Next priority: whole-scene GPU timing or CPU/GPU correlation across the render
span, followed by engine-side attribution if GPU execution is small. Avoid
more small-state micro-optimizations until the missing cost is identified.
No performance gain claimed, no new install, no repeat needed for this capture.

## Current test: quarter-pixel hub comparison

Same verified DLL298-g3d80740a9; only installed Screen.RenderWidth/Height
changed from2750x2850 to1375x1425. Full INI diff confirms exactly those two
changes; CRLF preserved. Backup: build/playtest-candidates/installs/20260914-221349-153396.
Candidate: build/playtest-candidates/quarter-pixel-hub. No new code/build needed.
Existing whole-frame D3D9 GPU timestamp instrumentation was already enabled:
late previous-run windows report roughly10.6-11.6ms/tick GPU span. This is not
exclusive GPU busy time; gaps in submission can contribute. Earlier handoffs
incorrectly implied this measurement was absent. Resolution sensitivity is the
next discriminating experiment. Verify actual CreateDevice/capture dimensions
before accepting the result. The INI overrides the launch-file mirror.
Test same hub120Hz, same view for60 seconds; ask whether lag is noticeably
reduced despite the deliberately softer image. Compare timing and tails with
the archived full-resolution run. A gain supports pixel-dependent work; little
gain lowers upscaling priority but alone does not prove a CPU bottleneck.
Restore original dimensions after collecting this result. No game launched.

AER is a separate future investigation. Prior implementation was reported
broken and must not be treated as a working starting point. Proposed alternative:
freeze world simulation between eye renders for matched world state. This still
requires two rendered views per completed pair; savings depend on avoided
simulation/render work and scheduling, not the AER name or a presents/s counter.
No AER implementation or installation authorized by this test configuration.

## Current test: high-resolution hub trial

Quarter-pixel run verified against DLL/banner298-g3d80740a9 and actual
CreateDevice1375x1425. Reported image quality substantially reduced without
noticeable lag relief. Last20 full gameplay timing windows after settling:
original2750x2850 mean logged tick16.01ms /62.165 ticks/s; quarter-pixel15.02ms /
66.125 ticks/s. Roughly6.4% throughput gain for75% fewer pixels. These are means
of3-second diagnostic windows across separate runs, not a controlled ABA or
fresh-pair distribution; no exact tail comparison or CPU saturation claim.
Evidence: build/performance-results/quarter-pixel-20260914-221912, both logs
archived. Strong resolution insensitivity lowers priority of upscaling as the
primary lag fix; engine, driver submission, geometry and synchronization remain.

Installed same DLL with3850x3990,1.4x original dimensions /1.96x original pixels.
Only Screen.RenderWidth/Height changed; full INI diff verified and CRLF retained.
Backup: build/playtest-candidates/installs/20260914-222004-638846.
This supersedes the previous instruction to restore original resolution now,
following the request to test higher image quality. Original remains2750x2850.
Next: same hub120Hz, same view60 seconds, then brief head movement. Question:
is lag noticeably worse than at the original resolution? Similar lag supports
keeping extra clarity provisionally, subject to actual dimensions and log timing;
worse lag means exceeding useful headroom. No new default or merge. Game not launched.

## Resolution trial concluded; original setting restored

Verified build298-g3d80740a9 and actual CreateDevice3850x3990. High-resolution
hub slowdown reported around45-50fps; log contains consecutive20-22ms tick
windows around44-49 ticks/s. Later windows return near69/s, so the whole run
must not be averaged as one fixed hub workload. Test was not completed as
prescribed; sufficient to reject the proposed high-resolution setting, not to
claim a controlled performance percentage. Both logs archived at
build/performance-results/high-resolution-20260914-222549.

Restored installed2750x2850, same DLL and all other settings. Full INI comparison
contains only width/height restoration; CRLF verified by installer. Canonical
installed.json records backup and hashes. No game launched. No repeat requested.
Quarter-pixel offered little gain, nearly double pixels caused substantial loss:
consistent with mixed limits / a resolution-independent floor plus higher-resolution
GPU or transfer pressure. Does not prove a particular engine or GPU bottleneck.
Next development should identify engine/submission/wait costs at original size,
not promise upscaling or AER gains from these observations. No main merge.
