# Performance audit and improvement plan

2026-09-14. Tracking: [VR-113](https://linear.app/vr-stereo-hub/issue/VR-113).
Source baseline: `255d1c91` on VR-Main, after the accepted PR56/57/58 integration.
Scope: source audit, existing evidence, and a future measurement/optimization plan.
No implementation, configuration changes, build, installation, or game launch.

## 1. Findings that should guide the work

There are credible opportunities to improve both sustained performance and frame
consistency. The evidence does not support blaming the original CPU texture
readback: the accepted profile uses pipelined shared D3D9Ex surfaces, and the
inspected slow gameplay window spends only 7 microseconds per present in its
capture CPU subtotal. That does not clear the GPU bridge, driver scheduling,
desktop presentation, or synchronization elsewhere in the pipeline.

The best investigation order is:

1. Correct and extend timing attribution, then establish a repeatable 120 Hz
   workload with separate CPU, GPU, and completed-pair measurements.
2. Measure the two desktop Presents and desktop mirror work, and the missing
   D3D11 GPU portion. These have the strongest combination of measured cost and
   avoidable architectural work.
3. Remove repeated shader reflection and redundant per-draw state queries and
   locks. These are concrete source-level opportunities without reducing image
   quality, but their FPS benefit has not been measured.
4. Measure the diagnostic profile, repeated object-table construction, and
   per-event feature dispatch. Improve discovery separately from steady rendering.
5. If scene GPU work remains above budget, examine resolution and individual
   expensive scene passes, then consider selective stereo work sharing. Treat
   alternate-eye rendering and image reconstruction as explicit quality tradeoffs.

Stable 120 Hz is a reasonable target to investigate on the observed RTX 4070 Ti
SUPER / Ryzen 5 5600X system. It is not established by the flat 240 fps result.
Some existing gameplay windows already have a D3D9 GPU marker span above the
entire 8.33 ms target, before separately accounting for the bridge and compositor.
Both CPU/driver overhead and GPU work need investigation; one small cleanup is
unlikely to guarantee 120 everywhere.

## 2. Provenance and limits

### Source, installed binary, and log are separate identities

| Item | Verified identity |
|---|---|
| Reviewed source | VR-Main `255d1c91`; initial worktree clean |
| Installed build | `vr33-hands-working-266-g5aa625ae`, compiled Sep 14 11:25:16, from `build/playtest-candidates/installed.json` |
| Installed DLL SHA256 | `257ab2551fadfec93d66a1e25067143c6643257709596f8303ecb5482015c7a4` |
| Installed INI SHA256 | `364e79997823cd18e97b398a324edc20aaae9c222377e0c8d1f01db5dc060508` |
| Local compiled artifact | `build/src/RelWithDebInfo/d3d9.dll`, same SHA256 as installed |
| Existing log banner | `vr33-hands-working-264-gfe400945`, compiled Sep 14 11:06:45 |
| Existing log SHA256 | `5b773a5ec88e80b5368107c706be63865b18fd31bca2d76644125087fdf42db6` |
| Log verification | Byte-identical by SHA256 to `build/mono-ui-test/accepted-20260914-111707/dishonored_vr.log`, the accepted build264 archive |
| Audit preservation | Both logs and current INI copied to `build/performance-audit-20260914/`, local only |

The installed banner and log banner do **not** match. No build266 performance
test is claimed. The build264 log can be examined as an identified historical
run, not as the requested opening benchmark or proof of current installed FPS.
The accepted stack record documents build266 as the accepted behavior plus
default promotion. That relationship does not turn an old run into a new test.

The `release` build preset is `RelWithDebInfo`; generated project settings use
MaxSpeed optimization, the non-debug runtime, and `DVR_WITH_LEGACY=0`. This is
not an accidentally installed Debug build. No build was needed to establish it.

Read-only hardware inventory reports Ryzen 5 5600X, 6 cores / 12 logical
processors, RTX 4070 Ti SUPER, and approximately 32 GB installed RAM. The
historical log independently places D3D11 on the RTX 4070 Ti SUPER and uses
VirtualDesktopXR 1.0.10. Driver inventory reports `32.0.16.1074`. GPU clocks,
power state, temperature, encoding load, runtime/streaming settings during the
reported flat comparison, and the exact FPS counter used are not known.

### Active profile

The full installed INI matches the accepted promoted profile. Relevant values:

| Area | Values |
|---|---|
| Render size | `RenderWidth=2750`, `RenderHeight=2850`, `VirtualMode=1` |
| Stereo | `Method=reentry`, `Armed=1`, `C5Pair=1`, `HoldUntagged=3`, both tag repairs enabled |
| Capture | `Mode=shared`, `SharedWait=0`, `BboxMs=30000` |
| D3D9 device | `Ex=1`, `Managed=shadow`, `ShadowFullCopy=1`, `ShadowSurfaces=0` |
| Pacing | `Ahead=0`, `Lag=2`, `Strict=0`, `SyncHz=0`, `FpsCap=0`, `ForceNoVSync=1` |
| Performance probes | `Instruments=1`, `GpuQueries=1`, `FrameId=1`, `FrameIdEvery=8`, `Ab=0` |
| Other probes | PairTrace, DrawCallerTrace, RingLedger, DrawCensus, MatCensus, AttachCensus, PoseReport, AttachScaleTrace, Cine.Trace, Anim.DropWatch, PosTrack.ZAccount enabled |
| Lifecycle | `UiKeepOnMenu=1`, `AttachKeepOnMenu=1`, `AttachKeepOnNote=1`, `CacheNameLookups=0` |
| Hands/input | Native hand/weapon correction enabled, `VRHands.Enabled=0`, `MotionAim.Enabled=0`; Blink/melee and crosshair dot enabled |

INI flags do not prove all their code executes continuously. Some are one-shot,
some are bounded probes, and some old probe bodies compile only with legacy.
Conversely, diagnostic work can execute before a throttled log call. Trace each
flag to its consumer before declaring it free or expensive.

The historical device creation is actually **windowed**, `2750x2850`, one
backbuffer, DISCARD, no multisampling, immediate presentation, lockable backbuffer.
VirtualMode changes the game's fullscreen request into this windowed device.
Therefore the flat and modded runs may also differ in the Windows presentation
path. The advertised `@240` virtual display mode is not headset refresh.
The inspected gameplay timing windows report an 11.11 ms runtime period (90 Hz).

## 3. What the 240 versus 70-90 comparison establishes

Reported flat baseline: 2560x1440 at maximum settings, held at a 240 fps cap.
Reported modded opening: approximately 70-90 fps. The exact VR measurement
population, render settings at that time, and matched replay are not preserved.

| Quantity | Flat comparison | Current VR profile |
|---|---:|---:|
| Pixels per view | 3,686,400 | 7,837,500 |
| Views per complete scene frame | 1 | 2 |
| Scene pixels per complete frame | 3.686 million | 15.675 million |
| Nominal complete frames per second | 240, capped | 120 target |
| Nominal scene pixels per second | 884.736 million | 1,881 million at target |

The VR pair has **4.25 times** the flat frame's pixel count; at 120 pairs/s it
asks for **2.126 times** the flat 240 fps pixel throughput. At 70-90 pairs/s it
would still ask for roughly 1.24-1.59 times that flat pixel throughput. These are
pixel counts, not a performance model: geometry, wider visible scene, shadows,
postprocessing, driver cost, CPU/GPU overlap, and encoding do not scale uniformly.

A cap at 240 only establishes that the flat workload fits within about 4.17 ms
per frame. It does not reveal its uncapped headroom or whether the CPU or GPU
sets the limit. Nor does two-eye rendering necessarily double simulation: this
mod re-enters the viewport draw root, not the entire world simulation tick.

Moving from 90 to 120 requires reducing delivered-frame time from 11.11 to
8.33 ms, a 25% reduction. From 70 to 120 requires 14.29 to 8.33 ms, a 41.7%
reduction. Equivalently, the speedups are 33.3% and 71.4%. Those are substantial
targets. The objective should be 120 **fresh complete stereo pairs** with
consistent deadlines, not 120 eye Presents or 120 compositor refreshes of older
images. A raw game/desktop counter can count both eye Presents.

## 4. The actual frame path and its costs

```text
Game/script thread
  ProcessEvent observer, camera/input/animation and lifecycle work
  viewport draw decision
    left camera -> native scene draw queued
    right camera -> second native scene draw queued, if permitted
           |
           v
Render/present thread, normally twice per stereo pair
  native D3D9 scene + draw/constant hooks + hand/weapon correction
  hkPresent
    commands/status/game-state checks
    OpenXR begin/pose/input path (one XR wait/locate per healthy pair)
    per-present mod tick
    reconcile eye and image identity
    D3D9 backbuffer -> shared slot                    StretchRect
    wait for slot ownership/completion as needed
    previous shared slot -> RGBA intermediate        D3D11 fullscreen draw
    optional overlay/own-hand callback, frame-ID probes
    D3D11 read-completion event + Flush
    intermediate -> acquired eye swapchain           CopyResource
    release eye image; close XR frame on completing eye
    current-draw desktop snapshot or restore         StretchRect
    original D3D9 Present
```

Sources: [frame hooks](../../src/core/framework/frame_hooks.cpp),
[scene draw](../../src/game/dishonored/scene_draw.cpp),
[capture](../../src/core/gfx/capture.cpp),
[reentry](../../src/core/gfx/reentry.cpp),
[runtime](../../src/core/vr/openxr_runtime.cpp), and
[desktop mirror](../../src/core/gfx/desktop_eye.cpp).

In normal steady stereo there are two capture copies, two fullscreen D3D11
blits, two swapchain copies, and approximately two desktop-mirror copies per
complete pair, plus two original Presents. Exact counts vary on mono, refused
tags, no-frame paths, and reset. The D3D11 intermediate is reused, not allocated
every frame. Shared handles and SRVs are opened/created when slots are created,
not per grab. The blit shader and pipeline objects are initialized once.

One full-size 32-bit image is 31.35 MB decimal / 29.9 MiB. Four full-surface
transfers per eye are approximately 250.8 MB of destination payload per pair,
or 30.1 GB/s at 120 pairs/s. Reading the sources as well gives a rough 60.2 GB/s
traffic model before scene rendering. This is arithmetic, **not measured DRAM
bandwidth**: caches, compression, formats, and presentation alter actual traffic.
The likely benefit of removing a pass includes scheduling and synchronization,
not just its bytes. Each additional full-size ring texture also adds about
29.9 MiB; a deeper queue is not a free fix.

## 5. Existing measurements and what they actually mean

### Two timestamp-matched build264 gameplay windows

Both windows below occur more than four seconds after the last logged GAMEPLAY
transition. They are illustrative 3-second windows from the accepted weapon
playtest, not a route benchmark. All columns on a row come from the same log
timestamp. A scan of the file found 52 windows satisfying that state-age filter;
that filter does not prove a constant camera, scene, or interaction.

| Log timestamp, ms | Tagged rate printed by perf | Presents/s | Printed tick mean | D3D9 marker span per pair | Two original Present means summed | P1+P2 render-thread R | Runtime wait, summed | Capture CPU / present |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 15465718 | 89.0/s | 179 | 11.2 ms | 7.7 ms | 2.7 ms | 5.1 ms | 2.3 ms | 9 us |
| 15474718 | 63.7/s | 128 | 15.7 ms | 11.3 ms | 3.9 ms | 10.5 ms | about 0 ms | 7 us |

The slow window reports 383 resolved GPU records, zero late/disjoint/unmarked,
one untagged present, and zero capture read/blit fence waits in its capture
window, with zero lifetime timeouts. This is strong evidence against a CPU
readback/fence wait explaining **that sustained slow window**. It supports
investigating scene/render-thread work, desktop Present, and downstream GPU
work. The 3.9 ms spent inside Present is not a promise that skipping a Present
recovers 3.9 ms: the call can absorb waits caused elsewhere.

Do not add the GPU span to the CPU columns: those timelines overlap. Do not
interpret the printed rate as a count of verified fresh XR pair releases. The
perf window groups P1/P2 by eye class, and its tick number combines class means;
with untagged work or unequal populations it need not equal 1000/rate.

### Instrumentation defects and missing coverage

Source: [perf.cpp](../../src/core/framework/perf.cpp), particularly
`gpu_resolve` and the window formatter; [perf_ab.cpp](../../src/core/framework/perf_ab.cpp).

- `gpuSpanUs` is BeginScene-to-Present-entry. The capture bracket is issued
  inside Present, after that interval. The display subtracts capture DMA from
  a span that does not include it. The printed `3d = span - dma` and derived
  `lock - dma` interpretation are not reliable attribution. This was already
  noted in VR-67 and still exists in the reviewed source.
- The field named `readback dma` times the shared StretchRect in shared mode.
  It is not evidence of a hidden full-frame CPU readback.
- `idle(d3d9)` is a gap between D3D9 timestamps, not proof the GPU is idle.
  D3D11, compositor, encoder, CPU starvation, and driver scheduling can fill it.
- CPU duration of `CopyResource` or a draw only measures command submission,
  not GPU execution. There is no dedicated D3D11 timestamp attribution of the
  fullscreen blit, swapchain copy, frame-ID stages, or their queue delay.
- The D3D9 span itself may contain GPU queue gaps. It is not pure shader busy
  time. Use GPU scheduling/engine activity alongside markers before attributing
  its entirety to pixel shading.
- `read_done` and `Flush`, frame-ID stages, and target management sit outside
  capture's small `last_grab` subtotal. A 7 us subtotal is not a 7 us bridge.
- On a bbox sample, `lock_copy` assigns to the same lock accumulator used for
  earlier fence waits. This can replace rather than accumulate a phase cost.
- The existing A/B controller measures completing-eye intervals and tails,
  excludes transition warmup, and repeats baseline. Reuse that work, but extend
  its fixed deadline bins for 8.33/11.11/16.67 ms and distinguish completing-eye
  observations from successful fresh-pair releases. It is disabled in this INI.

These are reasons to repair measurement first, not reasons to defer the whole
audit. They prevent an apparent optimization from just moving a wait to another
column or increasing throughput of held images.

### Prior evidence to retain

[ENGINE_NOTES](ENGINE_NOTES.md), sections on capture cost, tick budget, and the
2026-09-09 performance measurements, document:

- Full CPU readback was a major cost in the old sync/deferred paths. The
  shared D3D9Ex path already removed that steady full-frame round trip.
- Earlier FrameId on/off and D3D9Ex maximum frame latency 1/2/3 comparisons
  were within the baseline noise floor. Do not sell them again as proven wins.
- An older run placed 19/19 gameplay hitches in the submission tail, with
  xrEndFrame at 32-108 ms versus a much smaller typical call. That locates the
  wait, not its root cause, and is not a result reproduced in this audit.
- Reducing bbox sampling did not reduce the previously investigated frame-gap
  rate. Bbox readback remains avoidable periodic work, not the established cause
  of that fault. See [TRAPS](../TRAPS.md).
- Old simulator measurements used other hardware/configurations, including an
  RTX 4060. Their millisecond costs must not be projected onto the current rig.

## 6. Prioritized optimization candidates

Impact is a ranking of opportunity, not a promised FPS gain. Effort is relative:
small = local bounded change; medium = subsystem change and lifecycle tests;
large = frame architecture or engine research. Every future behavior change
needs its own A/B and regression checks.

| Priority | Area | Evidence / likely value | Effort | Main risk |
|---|---|---|---|---|
| P0 | Correct timing and pair accounting | Enables trustworthy choices; no direct FPS promise | Medium | Instrumentation distorts or mislabels workload |
| P1 | Desktop Present/mirror frequency | Measured 2.7-3.9 ms in two original calls in example windows; potentially substantial | Large | Backbuffer lifecycle, engine progress, tag/capture order |
| P1 | Shader-layout cache | Definite repeated bytecode read/reflection on shader switches; plausible steady CPU gain | Small-medium | Pointer reuse, negative/partial layouts |
| P1 | Shared draw-state snapshot and animation weight publication | Repeated COM getters and exclusive locks per draw; plausible CPU gain | Medium | State blocks, resets, pass correctness |
| P1 | Diagnostic work and log output | 14,997 cine/trace lines in the preserved run; avoidable CPU/I/O and object scans | Small-medium | Removing evidence or accidentally disabling behavior |
| P1 | D3D11 bridge pass consolidation | Definite full-size intermediate blit + swapchain copy; unmeasured GPU cost | Large | Swapchain ownership, overlay, alpha, color and hold semantics |
| P1/P2 | Repeated live-set rebuilds and discovery | Full copy/sort under exclusive lock; load scans already measured in hundreds of ms | Medium-large | Stale objects, GC, cross-thread coherence |
| P2 | ProcessEvent dispatch consolidation | Many feature calls and VirtualQuery-backed reads on unrelated events | Medium-large | Missing required write/event cadence |
| P2 | Correct stereo scheduling (VR-77) | Single-draw bursts discard useful pair opportunities | Large | More GPU work, liveness and mono transitions |
| P2 | Texture streaming/shadow uploads | Whole mip pushed per write; possible burst bandwidth and memory pressure | Medium-large | Black mips, partial writes, resets |
| P2 | Shared-slot scheduling | Potential waits/flush cost; not large CPU cost in example slow window | Large | Cross-API data race or added latency |
| P2 | Resolution and native quality controls | Large pixel multiplier; high potential if GPU-bound, with image tradeoff | Small for static tuning; large for dynamic | Image clarity, FOV, unstable sizes |
| P3 | Selective stereo scene reuse / new renderer | Potentially largest structural saving, least established | Very large | Eye-dependent effects/culling and engine command ownership |
| P3 | Build/code generation and small housekeeping | Installed build already optimized; secondary gains | Small-medium | Numerical/ABI regressions, poor return on effort |

### 6.1 Desktop presentation and mirror

`frame_hooks.cpp:222` always calls the original Present after mod work. In healthy
reentry that happens twice per pair. `desktop_eye.cpp:104` snapshots a left image
or restores it over the right backbuffer so the window remains stable. Thus a
headset-first application is still paying for two desktop deliveries and large
mirror copies.

Plan:

- First time each original Present and both mirror copy directions separately,
  on CPU and GPU, joined to current-draw and delivered-image identities.
- Design an optional one-desktop-present-per-pair path, keeping both eye
  captures and all game/render progress notifications. Do not simply return
  D3D_OK before the hook's work or suppress the viewport call.
- Separate mirror refresh from eye cadence. Consider updating a retained mirror
  once per pair or at a user-selected lower rate, and eventually a smaller
  desktop target. Existing eye pinning must stay stable through single ticks.
- Check the windowed VirtualMode presentation path, lockable-backbuffer flag,
  driver queue behavior, and DWM cost individually. Merely requesting immediate
  presentation does not establish a nonblocking driver call.

The game uses DISCARD. Microsoft documents that discarded backbuffer contents
are not preserved after Present. Skipping or moving a Present changes buffer
lifecycle assumptions and must be tested with clears, reuse, reset, menu, movie,
and single-draw paths. See [D3DSWAPEFFECT](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dswapeffect).

**Counterprediction:** if measured Present time just moves to a fence, XR release,
or the next frame, and fresh-pair deadlines do not improve, no throughput gain
was achieved. Even then lower mirror GPU work may help power or encoding, but
that needs separate evidence.

### 6.2 Shader reflection and draw-state work

`palette_capture.cpp:75` (`PcRefreshLayout`) calls GetVertexShader each time.
It avoids GetFunction/PcReflect only if the shader equals `g_pcLayShader`, a
single last-shader entry. The separate 16-entry `said` array suppresses repeated
logging; it is **not** a layout cache. A/B/A switching parses A again. The weapon
path calls this even for many draws that ultimately cannot be corrected.

Plan a bounded cache of full and partial layouts, including failed reflection,
keyed by an established shader resource lifetime. Intercept creation/destruction
or otherwise establish a lifetime identifier; clear/reset at appropriate device
boundaries. A raw pointer reused for a different shader cannot hit the old
layout. Preserve the validated BoneMatrices register bounds and overlap checks.
Measure GetFunction bytes/calls, unique shaders, cache hits/misses/evictions, and
CPU time. A good warm steady scene should parse a shader once per lifetime,
not once per bind transition. Do not take long-lived engine COM references in
draw detours to make the cache appear safe.

`weapon_attach.cpp:728` looks up vertex/index buffers for every indexed draw
once contracts exist. `draw_census.cpp:183` can query them again for the hand
mesh lock. Census, layout, and correction paths request additional shader,
declaration, viewport, and constant state. First share one lazy, per-draw snapshot
among consumers, which limits invalidation complexity. Only then consider a
binding cache updated by SetStreamSource/SetIndices/SetVertexShader, with state
block Apply, reset, and mod-originated state changes covered.

`anim_state.cpp:157` acquires an **exclusive** SRW lock and reads the clock on
every `weight()` call, although its interpolation result is cached per Present.
`native_draw()` invokes it at the draw entry and again in the weapon router;
other corrections also need weight. Publish one coherent render-frame weight
and generation to reduce repeated locking. Never freeze animation handback or
use a frame cache across an ownership transition without defined semantics.

The known-weapon path still performs correct per-draw transform/instance checks.
Cache invariant native reference inverses and candidate transforms per valid
component publication/eye/pass where possible, while retaining the draw's fresh
L2W and instance verdict. `AttachMaxTry` counts diagnostic over-budget attempts;
it does not cap placement. Making it a hard cap would starve later weapon draws.

**Counterprediction:** fewer reflection calls/getters without lower render-thread
CPU time or improved deadlines means these were not the limiting cost. Keep the
cache only if its complexity is justified by measured benefit.

### 6.3 Diagnostics and logging

The current profile intentionally promotes the accepted diagnostic settings.
Do not silently change those defaults in this audit or treat all diagnostic
switches as expendable gameplay switches.

The preserved 34,189-line log contains 14,997 `script/cine/trace` lines, 2,802
`hands/dc`, 1,729 `present/crosshair`, and 1,213 `present/ledger` lines. These
are whole-run text counts, not a benchmark of logging CPU time. `CineTraceDraw`
samples at 100 ms, emits five lines per sample, reads reflected state, and
can call BuildLiveSet during failed-liveness recovery, at most once per second.
It is reached at ordinary gameplay draw entry,
not only during a cinematic. At full cadence its five lines alone approach
50 lines/s. `log.cpp::writev` formats each line and serializes ring/file writes
under a lock, with periodic flushes.

Plan individually controlled A/B trials of Cine.Trace, draw census reporting,
ledger/pair trace, and remaining probes. Separate logging cost from the data
collection cost: lowering the log level does not remove computations already
performed outside the log macro. Keep low-cost aggregate timings and failure
breadcrumbs; consider a bounded binary/scalar ring and off-thread text
serialization for high-volume diagnostics. Preserve crash startup logging.

There are also two full-frame CPU allocations in shared capture: the SYSTEMMEM
surface and heap pixel buffer are created even though only bbox/snapshots need
them. BboxMs=30000 still causes a full synchronous download and row copy on a
sample. Lazily allocate diagnostics, downsample before readback, and read a
staging ring without blocking if a future image-quality probe is needed.
Changing BboxMs to zero still leaves the initial/size-change sample. Prior bbox
and FrameId negative tests make these lower-confidence throughput improvements.

The new lens trace performs its device queries **after** its two-second gate.
PoseReport resolves/dumps primarily at startup, rather than reading all sockets
every frame. The hidden F10 overlay immediately returns. Do not count these as
full per-frame draws or scans merely because their INI values are 1.

### 6.4 Liveness, object discovery, and event dispatch

`uobject.cpp:78` constructs the live-object set by copying GObjects and qsorting
it under an exclusive lock. Every IsLiveObject call acquires its shared lock and
binary searches. `UiSurfacePoll` calls BuildLiveSet every second after its 50 ms
poll gate. Enabled cinematic tracing can request a refresh when controller or
camera liveness fails, rate-limited to once per second; it does not rebuild
unconditionally. Animation/handback/controller recovery can request more builds. Independent
requesters can repeat equivalent work or hold up readers on the other lane.

Do not solve this by dropping IsLiveObject, assuming class names mean liveness,
or keeping stale tables through a load. Plan one lifecycle-aware refresh service
with request coalescing, explicit generation, and immutable published snapshots.
Build/sort a coherent snapshot away from the readers' lock where engine lifetime
permits, then publish atomically with a sound reader lifetime scheme. Consider
an indexed membership structure only after measuring size, lookup count, memory,
and publication cost. Writers must still prove current-level membership and
revalidate retained menu identities, even if an address has not changed.

`FindNameIdx` scans GNames; each NameFromIndex performs memory validation.
FindPropOffsetChecked resolves two names and then scans GObjects. The existing
optional name-index cache is off and the accepted startup follow-up leaves it
off. This audit does not establish that turning it on is correct or sufficient.
It still needs bounded memory, positive-hit validation, safe handling of new
names/reloads, and testing. A negative lookup must not hide a later-loaded name.

`UiDiscover` walks classes and then instances/properties while holding the UI
table lock. The build264 log directly measures scans at 367 and 547 ms early in
the run. Its printed 2,595 count comes from a first-pass `seen` counter, not a
complete population count for both passes: do not use it to calculate per-object
cost. The earlier resume scan problem is mitigated by UiKeepOnMenu; real loads
still need fresh discovery. Plan reusable class/property metadata with validated
identity and bounded incremental instance discovery. Budget work by elapsed
time as well as item count: 1,024 items can still be expensive with repeated
VirtualQuery calls. Define conservative gameplay/UI behavior while incomplete.

`PeHandler` invokes a long feature chain before its view-rotation fast path:
latching, UI discovery, collision state, cinematic hooks, draw-hook management,
animation, FOV, camera validation, pose reporting and other feature ticks.
Many callees already throttle; repeated entry and checks still need profiling.
Plan to route by validated event/name IDs early, coalesce once-per-tick work,
and keep the minimum synchronous camera/action writers at the events where the
engine honors them. Measure calls/time per feature and per event class, including
rare retries. Moving every check to Present would violate the script-lane contract.

`RangeReadable` is backed by VirtualQuery, so repeated fine-grained reads can
be costly. Batch contiguous validation inside one coherent, live-object read
scope where possible. Do not replace it with a permanent readable-page cache or
remove liveness to improve a microbenchmark.

**Expected value:** likely strongest for startup, load, and periodic tails;
steady FPS improves only if event/validation work or lock contention owns the
critical path. Track first-stereo time and weapon-ready time separately under
existing VR-102.

### 6.5 Bridge consolidation and safe slot scheduling

The current shared path already avoids steady CPU round trips, but it is not
zero-copy: StretchRect feeds the shared slot, BlitQuad converts/samples into an
RGBA intermediate, and CopyResource moves that into an XR image. Candidate
designs, ordered by containment:

1. Acquire/wait for the appropriate XR image, draw the shared SRV directly into
   its RTV, compose overlays there, and release it. This can remove the
   intermediate-to-swapchain full copy and intermediate allocation.
2. When formats and alpha/color behavior permit and overlays are absent, compare
   a direct shared-slot-to-XR copy against a fullscreen draw. BGRA/RGBA
   incompatibility is real; do not copy between incompatible format families.
3. Much later, investigate making the engine's final color target itself a
   suitable shared surface. This could remove the initial StretchRect but needs
   verified render-target ownership, MSAA resolves, postprocessing, format
   compatibility, device reset, and engine-resource lifetime handling. It is not
   achieved by opening arbitrary D3D9 textures or runtime-owned XR images.

These change when XR acquire happens and may expose an earlier swapchain wait.
Keep an explicit fallback for no-frame holds and last-layer reuse. The direct
path must not overwrite the other eye or a released compositor image. Observe
OpenXR's acquire/wait/release contract; successful wait is required before
application image access and release order is prescribed by the API.
[OpenXR wait](https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrWaitSwapchainImage.html),
[release](https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrReleaseSwapchainImage.html).

Capture uses two shared slots. Before reuse it waits on a D3D11 read event;
before delivery it waits on a D3D9 copy event. Each loop can spend up to 10 ms.
`read_done` ends an event and calls Flush every consumed present. Microsoft
documents a cost to unnecessary Flush calls, and that Flush submits work without
proving completion. Shared-resource visibility and ordering still require a
correct submission/completion contract. Measure flush/queue timing before
attempting batching; a blind removal can trade frame time for wrong-eye pixels.
[Flush](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-flush),
[shared resources](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-opensharedresource).

A three-slot experiment might reduce reuse waits but also adds memory, latency,
and identity complexity. SharedWait=0 already delivers the preceding present.
Any latency change requires measuring the image's actual pose generation and
hand normalization; keep accepted Lag=2 as the baseline. Never declare a faster
queue correct solely because it does not tear while the head is still.

**Source-confirmed failure path, separately tracked as
[VR-114](https://linear.app/vr-stereo-hub/issue/VR-114):** both wait helpers return
void and clear issued state after timeout; grab proceeds to overwrite/deliver
the slot. Errors also fail to propagate. A safe future optimization first needs
explicit ready/pending/error ownership and a coherent hold/recovery path. No
timeout was present in the example slow window, so this is not its diagnosed
FPS cause. No fix is included here.

### 6.6 Texture streaming and D3D9Ex managed-resource emulation

`d3d9ex.cpp::shadow_unlocked` mirrors managed textures into DEFAULT resources.
It already skips READONLY unlock uploads. The preserved log confirms tens of
thousands of such skipped copies. `ShadowFullCopy=1`, despite its name, pushes
the **written mip level** using UpdateSurface, with UpdateTexture fallback.
It does not mean every unlock blindly copies the whole mip chain.

Remaining candidates: track actual dirty rectangles/boxes, combine multiple
writes before first GPU use, cache safe mip-surface access, and avoid repeated
uploads of unchanged data where a valid dirty contract exists. Measure by
resource type, mip, bytes, unlock rate, fallback reason, time and level-load
phase. A full-level copy for a tiny partial update is an opportunity; a texture
only written once at loading does not explain continuous 70 fps later.

Keep mip-streaming and reset behavior correct. The shadow exists because a
naive DEFAULT+DYNAMIC replacement mishandled readback and produced texture
faults. Delaying updates until a bind is also unsafe if the resource was already
bound and is drawn again without rebinding. Track use, not just binding changes.
Do not disable ShadowFullCopy to chase speed without distance/mip correctness
tests. Native flat performance benefits from D3D9 managed-resource behavior that
the Ex bridge has had to emulate, so a matched single-view Ex test is useful.

Track process virtual address space, shadow/system memory, VRAM budget, resource
count and reset recovery. The process is 32-bit; unused CPU capture buffers and
shadow duplication may matter for pressure/stalls even with plenty of GPU RAM.
There is no current measured out-of-memory or leak finding in this audit.

### 6.7 Scene rendering, resolution, and architecture

The viewport root is called twice, potentially repeating visibility setup,
shadow preparation, render-target clears, post effects, HUD work, and driver
submission as well as the necessary eye-dependent geometry. The sub-millisecond
game-thread `call2` measurement is a command-queue cost, not the cost of executing
the second view on the render thread and GPU.

Begin with a pass census and timestamped GPU work at fixed scene and FOV. Identify
passes by resource/shader/draw context and observed behavior, not guessed call
counts. Candidate shared work includes view-independent shadow maps, animation
preparation and other per-tick data. Screen-space effects, culling, reflections,
occlusion queries, transparent ordering and viewmodel depth/color may be
eye-dependent. Reusing their left-eye result can damage the right eye. Existing
VR-79 covers stereo visibility; do not worsen it for apparent performance.

If GPU-bound, a reversible static resolution trial is the fastest discriminator:

| Linear dimension scale | Example resolution | Pixel work relative to current |
|---|---:|---:|
| 100% | 2750x2850 | 100% |
| 90% | 2475x2565 | 81% |
| 80% | 2200x2280 | 64% |
| 70% | 1925x1995 | 49% |

These preserve aspect. Lowering both dimensions by 20% reduces pixels by 36%,
not 20%; FPS will improve by less if fixed CPU/driver work dominates. Reducing
the desktop window alone will not reduce the engine eye render size. Changing
the virtual FOV to show less work is not equivalent to optimizing the same view.

Review actual native `[SystemSettings]` and per-pass costs before quality
changes. Current inspected engine config has frame smoothing false, motion blur
false, depth of field false and ambient occlusion false; the historical device
had no MSAA. These are not untapped savings to claim. Dynamic shadows, LOD,
reflections and other remaining settings need controlled measurement. Texture
resolution reductions primarily help memory/bandwidth pressure and are not
automatically the best steady-FPS tradeoff. See [GAME_CONFIG_MAP](GAME_CONFIG_MAP.md).

Dynamic resolution would require a stable maximum output/swapchain and variable
internal scene viewport or an explicit upscale pass; simply changing capture
dimensions each frame currently recreates resources/swapchains and would hitch.
Spatial upscaling can trade clarity for speed after a lower-resolution render.
Temporal reconstruction needs suitable history, depth, motion and stereo
identity support; a final-color-only capture is not a ready temporal-upscaler
integration. No DLSS/FSR-style gain is assumed.

AlternateEye remains `implemented() == false` in aer.cpp. Implementing it could
reduce draw workload, but 120 alternating eye images per second provides only
60 fresh images per eye, with inter-eye temporal differences. It does not meet
the same 120 fresh stereo-pair target. Treat it as a separate user-selected
tradeoff. A single-pass stereo renderer is a major engine/shader project, not a
routine D3D9 switch. The removed DXVK/Vulkan branch is not part of this plan.

### 6.8 Remaining coverage and lower-priority work

- **OpenXR/input:** healthy pair pacing already waits/locates once per pair;
  do not promise a gain from implementing that again. Time input sync, action
  state queries, locate, haptics, and mutex contention, but inspected Present
  game-tick means around 0.1 ms per eye do not make input the leading suspect.
  Keep required runtime calls on their established owning thread.
- **Pose metadata:** render observation uses a shared critical section with
  script publication. Check lock contention and duplicate generation copies.
  Expensive render-yaw diagnostics also exist; inspect their actual cadence
  before redesign. Do not replace measured image age with the newest pose.
- **Native hand geometry:** mesh split reads/builds geometry on identification
  or rebuild and reuses buffers. Per-frame skinning/correction and cap passes
  remain; profile those separately. Do not report all geometry extraction or
  CreateVertexBuffer calls as per-frame allocations.
- **HUD/extra layers:** the game HUD can be rendered in both native views, but
  the imported runtime HUD provider is a stub. Own VR hands are disabled.
  The crosshair dot is active; measure it, but do not attribute a full second
  scene render to a small compositor quad.
- **Status/commands/hotkeys:** status JSON does synchronous file output/rename
  once per second; command polling is also bounded. Move serialization/I/O to a
  worker only after immutable snapshots and measured stalls justify it. Batch
  duplicate hotkey polling if it appears in CPU samples.
- **Build optimization:** release optimization already exists. LTO/PGO or data
  layout improvements are follow-ups to hot-path profiles, not substitutes for
  fewer API calls. Avoid broad fast-math changes: strict lens/depth matching
  already needed epsilon-level fixes. CPU intrinsics should target measured
  math cost and respect supported hardware.
- **External runtime:** collect GPU encoding/compositor timings and dropped or
  reprojected frames separately. Do not blame Wi-Fi, SteamVR, drivers or CPU
  hardware without evidence. This run uses native VDXR, not the SteamVR shim.
  The shim requires a separate acceptance/profile pass on a rig that uses it.

## 7. Future execution and measurement plan

No tests in this section have been requested from the tester or run in this
audit. The user launches the game; the agent builds, installs, arms diagnostics,
reads logs, and archives both logs before every relaunch. Every install gets a
whole-file before/after INI diff and CRLF verification. No merge without approval.

### Phase A: trustworthy baseline and tools

1. Preserve baseline DLL, PDB, full mod/game configuration and runtime settings.
   Record hardware, actual buffer size/format, FOV, driver/presentation mode,
   target headset refresh, runtime period, and any reprojection policy. Keep
   panel refresh separate from predicted display period and app pair rate.
2. Add counters and rings for real game tick ID, eye draw attempt, capture serial,
   pose generation, XR acquisition/release, fresh-pair completion, and hold reason.
   Fix perf span labels and accumulation. Separate valid/late/disjoint GPU samples.
3. Add asynchronous D3D11 GPU timestamps around bridge stages; add D3D9 timestamps
   for mirror, capture and major native passes. Consume completed queries later,
   never force a GPU finish to measure it. Sample expensive CPU scopes or use
   a system trace rather than timing every tiny helper at unlimited rate.
4. Collect CPU samples and scheduling waits for game/render/pace threads alongside
   GPU engine activity. Choose a tool that can observe this 32-bit D3D9/D3D11
   process; verify profiler overhead and avoid an incompatible OpenXR API layer.
5. Report per fixed segment: fresh pair count/rate; pair p50/p95/p99/p99.9/max;
   missed 8.33 ms deadlines; >16.67/>33.33 ms intervals; held/untagged/stale-eye
   counts; per-eye age; CPU work and waits; GPU busy/queue markers; memory peaks.
   Keep menus, loads, startup and actual gameplay in separate populations.

### Phase B: controlled discrimination

Use identical fixed-view segments or a repeatable route, sufficient warmup,
baseline/alternative/baseline repetition, and several samples per condition.
The opening cinematic is a nonstationary workload; do not compare unrelated
moments in it as an A/B. Include a repeatable gameplay view for attribution,
then validate the opening as a separate workload. A 90 Hz runtime cannot
demonstrate stable 120; use it only for attribution at 90, then repeat at 120.

Each trial below is one performance question per launch. Agent-managed
live switching is used only where the lever already exists or has been safely
implemented. Cross-launch trials use one configuration change and a baseline
return. Success includes unchanged stereo correctness, not just a better counter.

| Trial question | Controlled change | Expected result if hypothesis holds | Meaning of the other outcome |
|---|---|---|---|
| Does pixel load own the missed deadlines? | Current versus 80% dimensions, same aspect/FOV/scene | Scene GPU time and fresh-pair intervals fall materially | CPU/driver/scheduling floor remains, or the size change was not honored |
| Does cinematic trace collection cost useful frame time? | Cine.Trace on/off/on only | Less trace/scan/lock work and improved pair tails | Trace is noisy but not a material limiter in this segment |
| Does repeated shader reflection matter? | Validated cache off/on/off | Warm misses collapse and render-thread CPU time falls | Parsing is not on the limiting path; do not claim an FPS win |
| Do repeated state queries/animation locks matter? | One verified snapshot optimization at a time | Fewer getters/locks with lower CPU time and stable correction | API calls were not the bottleneck or cost moved elsewhere |
| Does desktop presentation limit throughput? | Verified two-versus-one desktop present per pair | Lower total critical-path time and better fresh-pair deadlines | Wait moved to another stage or the removed work was not limiting |
| Is the bridge GPU cost substantial? | Verified direct-to-XR path versus intermediate | GPU bridge span/traffic and pair tails improve | Scene/compositor dominates or earlier acquire offsets the saving |
| Does texture streaming cause traversal hitches? | One dirty-upload/batching optimization | Lower uploaded bytes/time on the same traversal and no missing mips | Streaming was not causal or synchronization now occurs at first use |
| Are unnecessary single-draw decisions losing pairs? | VR-77 scheduling candidate only | Fewer refusals/holds, more fresh pairs at comparable work budget | Guard was reacting to real overload; removing it can worsen timing |
| Does the final combination sustain the target? | Accepted improvements, fixed 120 Hz configuration | Fresh stereo pairs meet the 8.33 ms target with headroom and stable tails | Use per-stage residuals to select next work; do not call reprojection a pass |

Capture-off or mono experiments can provide workload upper bounds, but change
pairing, content or pacing and cannot isolate one copy's cost by themselves.
Do not have the user play through a frozen headset image as a normal performance
test. These controls need a specifically defined stationary/simulator question.

### Phase C: implement the smallest proven win

After attribution, start with the bounded shader cache or diagnostic change if
they own meaningful CPU time; start with desktop/bridge work if measured GPU or
driver scheduling dominates. Preserve one behavioral change per candidate.
Do not bundle a smaller render size with an implementation optimization and
attribute all of the gain to the code.

Host regression coverage should exercise actual cache/snapshot/lifetime policy,
partial and negative shader layouts, shader pointer reuse, state-block changes,
slot ready/pending/failure, asymmetric/late eye tags, reset and menu/load
generations. User-launched simulator validation checks deterministic identity
and output correctness; its performance is not a prediction of VDXR plus encoder.

Only then run headset regressions: both-eye freshness, fast head turns, near
weapons, sword/crossbow/pistol/bolts, native animation handback, pause/note close,
save reload, cinematic entry/exit and desktop pin. Define one question for each
launch, with explicit pass/fail interpretation. Accepted pose lag, inverse-lens
consistency and current-level IsLiveObject checks are constraints throughout.

### Definition of a meaningful improvement

An alternative must beat repeated-baseline noise for the metric it claims to
improve, with no loss of fresh pairs or increased image age. Report absolute
milliseconds and relative change. Do not add unrelated savings estimates:
removing a wait can reveal another bottleneck, and CPU/GPU execution overlaps.
Stable 120 should be judged on sustained completed-pair deadlines across the
opening and representative heavier levels, not only a menu or an empty room.
Keep load hitches and startup readiness as separate targets.

## 8. Tracking and continuation

- VR-113 owns this audit and its prioritized plan. Optimization candidates are
  not approved implementation work merely because they appear here.
- VR-67 retains existing performance-distribution and timing-accounting work.
- VR-17 retains burst/hitch attribution; VR-77 retains single-draw scheduling.
- VR-102 retains startup weapon readiness and discovery follow-up.
- VR-114 records the source-confirmed shared-fence failure behavior found here.
- VR-79 remains the adjacent stereo visibility constraint.

No scene-pass reuse, new API layer, engine offset, quality reduction or default
change has been selected. The next step, when implementation is requested, is
Phase A plus the first discriminating measurement, not a wholesale renderer
rewrite. The known source opportunities and their counterpredictions above make
that first measurement actionable without inventing a cause or a gain.
