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
