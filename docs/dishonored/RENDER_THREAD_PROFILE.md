# Render-thread CPU profiling

## Branch and intent

VR-121, branch codex/vr-121-render-thread-profile, from accepted main85f9ef6e4.
Independent optimization branches start from this baseline. Keep experimental
branches recoverable; combine measured successful changes on an integration
branch and retest the combination. Stack only genuine dependencies. Shared
measurement tools can be isolated and reused without carrying a rendering
experiment. No merge is authorized.

Desktop trials remain on codex/vr-115-performance-rollout: Off improved fresh
submission throughput with repeated long-frame penalties; Reduced had only a
small effect and no consistent tail benefit. Full remains the baseline. See
PERFORMANCE_ROLLOUT.md for exact evidence. No desktop candidate code is on this
branch; the audit and results documents are copied as shared history only.

The next step measures layout refresh, CTAB reflection, shader bytecode reads,
weapon draw handling, initial buffer queries and animation-weight locking.
It does not introduce a shader cache or alter game state, palettes or poses.
A shader cache still needs real resource lifetime identity; a pointer alone
cannot safely identify a shader across destruction and reuse.

## Instrument contract

Perf.RenderProfile defaults0 when absent;1 enables this diagnostic. F10 Display
has a live checkbox that persists the choice. The Present lane establishes the
render thread; calls from other threads are deliberately excluded. Fixed-size
per-scope counters count all eligible calls and randomly time about1/64 using
a nonzero xorshift stream, avoiding fixed-stride eye/draw-phase aliasing.
No allocations, engine reads or locks are introduced by the sampler.

Every3 seconds, six lines report exact calls, sampled calls, sampled mean and
maximum, and estimated inclusive milliseconds per second. The estimate is sample
mean multiplied by total calls/window seconds. Scopes overlap: weapon draw
contains layout/reflection and some animation work. Never add these estimates
or equate them to recoverable frame time. The maximum is sampled, not the true
maximum; rare stalls can be missed. Low sample counts are inconclusive.
These are CPU wall times including waits, not CPU execution-only or GPU time.

Gameplay/menu transitions discard partial windows. This prevents combining
obviously different states, but scene changes within gameplay are not detected.
No allocation or shader lifetime behavior changes. The default-off path still
has an enable check at instrumented sites; enabled overhead needs comparison
before claiming a small speedup. This build measures cost, not an optimization.
Main's existing performance line limitations remain; desktop fresh-pair tools
are not pulled in with an unrelated experiment.

## Validation and first test

Production profiler host tests cover sampling population, exact call counts,
sample mean, bounded/reset storage, idempotent scope close, disabled scopes,
foreign-thread refusal, window logging and gameplay-transition discard. Lint
and golden checks pass. Release build and exports are verified for installation.
Never launch the game, including through the simulator.

Load the same sewer save at120Hz/current resolution, draw both weapons and
keep them visible while facing a normal quiet corridor for60 seconds. Avoid
menus and combat during that interval. Afterwards briefly move your head and
hands. One question: do the world and both weapons still look and track normally?
A visual problem rejects the diagnostic candidate; an unchanged view allows
using its logs to rank the sampled work. No automatic rendering-mode changes
occur. The agent reads and archives the log.

## Next decision

If reflection/layout work materially occupies the render thread, implement a
bounded lifetime-safe cache in its own optimization branch and compare off/on/off.
If buffer getters or animation locking dominate, choose that smaller change.
If these costs are negligible, move to asynchronous D3D11 GPU-stage measurement
and broader sampled CPU scopes instead of caching code with no measured benefit.
Remaining audit work includes diagnostic collection, discovery/liveness,
texture streaming and scene costs. No120Hz promise or performance gain yet.

## Installed candidate

Build `vr33-hands-working-275-gdeeaf66e5`, compiled20:03:32, source deeaf66e5.
DLL SHA256 `53dc9ee04197e8486eea1b9f48526be68dc2df2b04d111f5c4462aee965735b5`.
INI SHA256 `2288d0b089b9e88092e3363ee827aea5e5ecf8e7d026474eeb8fbdf2aa3a61f8`.
Bundle build/playtest-candidates/render-thread-profile. Previous DLL, INI and
both logs archived20260914-200411-670945. Entire INI diff only adds
Perf.RenderProfile=1; CRLF verified. DesktopAb=0 and prior HUD/desktop keys
are retained but inert where this main-based build has no consumer.
Host profiling suite, clean-source Win32 release, lint, golden and9 exports
pass. Existing DVR_CAT redefinition warning remains. No game launched.
The next log must match275-gdeeaf66e5 /20:03:32 before interpreting timings.
Local commit only: the earlier publication block remains pending explicit
user authorization, so no push or PR retry was attempted.
