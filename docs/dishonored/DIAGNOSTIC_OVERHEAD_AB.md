# Diagnostic overhead A/B/A

VR-124, branch codex/vr-124-diagnostic-overhead, independent from main18ae4ebda.
No main merge permission. Earlier performance branches remain intact.

## Decision and evidence

VR-121 measured reflection plus bytecode at about5ms/s; caching these is low
priority. VR-123 hub run on build295 measured conversion0.105/0.102ms per eye
and XR copy0.058/0.057ms, with normal reported behavior and no query errors.
Those stages are too small to explain sustained hub lag. Native draw/driver
work and diagnostic overhead remain candidates. No performance gain claimed.

This experiment measures a defined group of collectors before disabling any
by default. A return to baseline distinguishes an effect from scene drift.
If the middle phase improves beyond both baselines, isolate individual collectors
next; otherwise profile native draw/driver work. Do not compare this hub run
numerically to the previous quiet sewer workload as a controlled pair.

## Exact mask

During the middle phase, an atomic flag suppresses:

- PosTrack.ZAccount collection and Stereo.PairTrace collection, via their
  effective enabled/capturing queries. Camera writes and image-linked pose
  records remain active. These accounting reports span a deliberate interruption
  and are not valid camera investigations across the phase boundary.
- Perf.FrameId thumbnail capture/readback work. Frame serials used by the fresh
  pair counter come from capture delivery, independently of this thumbnail tool.
  Cheap bookkeeping and existing resources remain; this is not complete removal.
- Hands.AttachCensus at WaCensusNote, before repeated vertex/index/shader getters.
  This census is diagnostic and never used as the weapon-correction gate.

Original INI flags are never changed, including if the user saves settings during
the reduced phase. A originally-disabled collector is not forced on in baseline.
The experiment enables no collector the user had disabled. Collectors outside
this list, including ring ledger, GPU timers and performance reporting, remain.

Do NOT include Hands.DrawCensus: its gate also owns stale mesh-lock recovery and
hiding. Do NOT include Hands.PoseReport initialization: it resolves fields needed
by placement. Keep Cine.Trace and CineTraceTick alive: discovery and liveness
refresh share helpers with functional cinematic controls. There is no engine
memory writer or camera/hand/graphics-quality change in this experiment.

## Timeline and measurements

Perf.DiagnosticAb=0 is the missing-key/generated/release default. Installed test
uses1. F10 Display can arm/restart/stop; this arm control is intentionally not
saved by the generic Save button. The INI can arm the next launch. Arming through
config/UI disables the historical Perf.Ab latency sweep. No other experiments
are included in this main-based build; stale INI BridgeGpu/RenderProfile/DesktopAb
keys have no active consumers here.

Wait for strict gameplay permission, then10 seconds to settle. Three33-second
phases follow: baseline, reduced, baseline. Each excludes its first3 seconds.
Total109 seconds after gameplay permission. Menu loss, level-load notification
or device reset aborts the comparison; original collection returns immediately
at the next present tick. Runtime eye-cache resets on occasional untagged frames
are not treated as level loads. Stop/restart/completion also restore collection.
No temporary change survives the process.

Successful xrEndFrame stereo projections are counted only when both released
capture serials renewed. Failed submits, absent serials, held eyes and one-eye
updates cannot masquerade as fresh pairs. The ledger records successful image
release independently of pose age. Accepted serial identity survives segment
boundaries; the interval timestamp resets so transitions do not cross samples.

The existing VR-115 FreshPair policy is reused without desktop rendering changes.
Samples cap at16384 per phase; overflow invalidates a result rather than silently
biasing a distribution. Severe gaps have no upper cutoff. Reports include mean,
p50/p95/p99/max, interval-derived rate, fixed long-frame thresholds, rejected
submissions and validity. These are application fresh-pair intervals, not headset
reprojection FPS, CPU busy time or total GPU occupancy. Compare every metric
with BOTH baselines; this combined mask cannot attribute a gain to one collector.

## Validation

33 host checks run the production comparison module with controlled clock/log
substitutes: waiting, warmup, baseline/reduced/baseline, fresh/stale/failed serials,
wraparound, severe stalls, bounded samples, menu/load abort, restart and stop.
Win32 release build passes (existing unity macro warning). Lint, exports and
golden checks are required before install. No game or game-based simulator launch.

## One headset test

Load the same hub save at120Hz/current resolution. Stand in one spot facing the
same busy view with both weapons drawn. Stay there for two minutes without menus,
combat or walking; small natural head movement is fine. After two minutes, briefly
turn your head and move both hands. There are no manual phase changes or blackout.

One question: did the world and both weapons look and track normally throughout?
Normal behavior allows ranking the logged A/B/A measurements. Any new visual or
tracking fault rejects the candidate before interpreting a speed difference.
The agent verifies the build banner, archives both logs and reads the results.

## Installed diagnostic A/B candidate: VR-124

Build `vr33-hands-working-295-gdb73d0a49`, compiled21:26:07, source db73d0a49.
DLL SHA256 `1b9db8ba0f48bf4ee127afe3919a5dfa82881aad7be66fe63438462f710cbb47`.
INI SHA256 `f690dd877923bfbdd7de7309d528d7dba460029a23cc1f188424c38cc4397c81`.
Bundle build/playtest-candidates/diagnostic-overhead. Previous DLL/INI and both
logs archived20260914-212649-927506. Full INI comparison adds only
Perf.DiagnosticAb=1; CRLF and hashes verified. The295 count is shared with the
previous independent branch: always check suffix gdb73d0a49, not the count alone.
33 host checks, release, lint, golden and9 exports passed. No game launched.

Next: same hub save, stand facing the busy view with weapons drawn for two minutes,
no menus/combat/walking; then briefly turn head and move hands. One question:
did the world and both weapons look and track normally throughout? The benchmark
waits10s then runs baseline/reduced/baseline33s each, restoring normal collection.
A menu/load aborts the comparison. Read matching log for three valid phases.
No main merge authorization. This experiment's Linear updates and draft GitHub
publication are explicitly authorized.


## First diagnostic hub run: normal behavior, incomplete comparison

Verified295-gdb73d0a49 /21:26:07 against installed DLL and log. Archived both logs
and INI at build/performance-results/diagnostic-hub-20260914-213154. Normal
world/weapon behavior reported, runtime120Hz.

| Phase | n | Fresh pairs/s | Mean ms | p50 ms | p95 ms | p99 ms | Max ms |
|---|---:|---:|---:|---:|---:|---:|---:|
| Baseline |1946|64.90|15.408|15.213|18.845|26.057|44.507|
| Reduced |1992|66.44|15.052|15.069|17.807|25.049|40.967|

Both completed phases valid, no overflow. Reduction is about2.37% throughput
versus the first baseline, with smaller p95/p99, but no causal gain is established.
The pause menu opened at10176734. Final baseline started10143796 and needed to
reach10176796: abort62ms before completion. No final baseline distribution was
emitted. Coarser late tick windows also vary substantially, so do not substitute
them for the missing matched distribution. The run correctly restored normal
collection on menu loss; the comparison is invalid as a complete A/B/A.

Repeat on the same installed build, no code/INI changes needed. Load hub, wait
until gameplay is controllable and both weapons are visible, then hold the same
busy view for150 seconds without menus/combat/walking. Only after that interval
turn the head and move the hands. One question: did world and both weapons remain
normal throughout? This adds margin beyond the109-second internal schedule;
the earlier two-minute instruction did not allow enough margin after loading.
No diagnostic defaults promoted, no merge authorized.


## Completed hub diagnostic comparison: no compelling benefit

Verified build295-gdb73d0a49 /21:26:07 and installed DLL hash. Runtime120Hz;
normal world/weapon behavior reported throughout. Both logs and INI archived at
build/performance-results/diagnostic-hub-repeat-20260914-214029.
All three phases valid, zero overflow, COMPLETE valid=1, normal collection restored.

| Phase | n | Fresh pairs/s | Mean ms | p50 ms | p95 ms | p99 ms | Max ms | >33.333ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Baseline1 |1909|63.66|15.709|15.493|19.001|26.173|59.859|5|
| Reduced |1934|64.48|15.509|15.141|19.249|27.109|100.010|14|
| Baseline2 |1927|64.27|15.558|15.267|19.330|27.202|58.716|10|

Reduced throughput is1.29% above baseline1 but only0.33% above baseline2;
baselines themselves differ about0.96%. Median improves modestly, while p95/p99
sit within baseline spread and the reduced phase has more >33.333ms intervals
than either baseline. The100ms maximum is one observed event, not proof that
suppression causes stalls. No confidence interval or repeatability claim is made.

Decision: no compelling sustained or tail benefit; retain existing diagnostic
defaults and preserve this experiment. Do not spend another hub run isolating
members of this group now. Next priority is native draw/render-thread CPU and
driver-wait attribution, separate from these diagnostic and bridge candidates.
No new build, INI edit or merge. DiagnosticAb remains armed in the installed INI
for another launch, but this run completed and restored normal collection.
PR65 remains draft as a recoverable measurement tool, not an accepted speedup.

