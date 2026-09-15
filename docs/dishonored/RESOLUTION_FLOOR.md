# Resolution-insensitive hub cost (VR-125)

Branch codex/vr-125-resolution-floor continues from the archived VR-121
instrumentation branch to preserve source/PDB identity. It does not change
rendering. Installed DLL remains298-g3d80740a9 at2750x2850. Previous branch
and original main base18ae4ebda remain recoverable.

## Evidence and hypothesis

75% fewer pixels improved mean logged throughput only about6%, not literally
zero. Separate runs are not a controlled ABA. Nearly twice the original pixels
caused44-49 ticks/s in consecutive windows, but later workload changed. See
NATIVE_DRAW_PROFILE.md for exact provenance. GPU timestamps already existed;
their elapsed spans include GPU idle gaps and do not establish saturation.

## Next capture

Use Windows Performance Recorder GeneralProfile plus GPU, memory mode, at the
original resolution. CPU sampled stacks and scheduling events identify which
threads consume CPU or wait; GPU events show queue activity and submission.
No network/file-content collection profile requested. Trace is system-wide
metadata, kept locally under ignored build/performance-results, never published.
Check recording start succeeded before asking for a run. Administrative elevation
is required; first non-admin attempt returned0xc5585011. Never cancel another
recording; initial status confirmed no recording. Stop and save this recording
on the next user report, including early abort. Preserve matching DLL/PDB and
both game logs. Do not leave a trace running after completion.

One test: same hub view120Hz, weapons out, stand still60 seconds without menus
or combat, briefly move head/hands, exit and report. Question: did the usual hub
lag remain representative? A representative run permits attribution; materially
changed behavior means profiling perturbation/workload confounds the result.

## Analysis and decision

Filter Dishonored PID and the steady gameplay interval. Inspect CPU sampling
by thread/module/stack; resolve proxy PDB locally, engine frames by module RVA.
Do not equate whole-process CPU percentage with one-thread saturation. Inspect
context switches, ready time and wait stacks alongside GPU queues and Present.
Compare engine, proxy, graphics driver and runtime ownership. Check lost events
and usable stacks before drawing conclusions. CPU samples alone cannot explain
blocked time; GPU spans alone cannot distinguish feeding gaps from execution.
If engine work dominates, find a repeated expensive path before editing. If
waits dominate, identify the waited-on owner and why. If GPU work dominates,
identify resolution-independent geometry/shadow cost and compare resolution
experiments. No speculative synchronization removal, AER rewrite or upscaler.
Select one bounded change only when this evidence identifies a meaningful cost.

## Automated launch recovery and initial run

Current user explicitly authorized agent game launches for this investigation,
superseding the earlier no-launch rule. No merge permission. The simulator is
now usable through Steam with an explicit local control directory. Direct launch
crashed shortly after startup; dump and logs retained, cause not proven.

Standalone selftest initially failed xrCreateInstance -32 because of an enabled
incompatible OpenXR capture layer. A manifest-declared process-local opt-out
allowed60 frames with zero errors. xrsim-selftest now accepts
-DisableLayerEnvironment, restores prior values in finally, and does not change
registry registration. Read the installed layer manifest for its opt-out name.

The simulator selected the LAST adapter tied for largest reported VRAM because
its comparator used >=. This machine enumerates duplicate same-name adapters;
D3D9 and real runtime use the first, simulator picked the second. Changed ties
to retain the first and log the final choice. Verified selftest selected the
same LUID as D3D9; Steam game run changed from sync fallback to shared capture,
and previously failed compositor eye captures succeeded. Not a headset fix.

Launch commands: xrsim-selftest -Release with manifest-derived layer opt-out;
xrsim-launch -Release -AllowStale -ViaSteam -Dir <repo>/build/vr125-sim.
AllowStale deliberately retains the exact validated298-g3d80740a9 game DLL;
only simulator code changed. Set DVR_DATA_DIR to installed Paths.DataDir for
mod commands, and always pass identical explicit -Dir to simulator tools.
Use refresh120; inspect capture between each Return (title, Continue, loading
confirmation). Hound Pits loaded. Gradual head to0 1.6 0 180 0 0 3000 turned
successfully; instant180-degree changes did not establish a reliable orientation.
Screenshots in ignored build/vr125-sim document the views. No save edits.

55.039s process-thread CPU accounting: busiest threads75.94%,71.23%,38.27%
of one logical core. Thread ownership is unclassified, no stack evidence yet.
Open courtyard view roughly89-90 logged ticks/s versus62-66/s headset workload;
not a matched comparison. Runtime/streaming/FOV and exact view differ. Do not
claim a performance improvement or identify a bottleneck from this run.
Next: match the exact heavy view and identify thread owners/active stacks or
CPU-vs-wall spans without elevation. ETW still requires Windows elevation.
Game closed after capture. Full installed INI byte-identical to pre-launch;
DLL hash unchanged. Evidence: build/performance-results/vr125-sim-working,
vr125-direct-crash, vr125-wrong-adapter. No pending test for the user.

## Same-run view and thread attribution

Agent-run permission persists until the user requests a shutdown command.
Automated Steam/simulator run retained installed298-g3d80740a9, original
2750x2850 shared swapchains and120Hz; logged FOV half-angles54/55 and game
horizontal108.1 match the headset log. Recommended eye size differs, actual
swapchains match. Simulator does not model VDXR encoding/transport/scheduling;
never add artificial delay merely to reproduce an FPS number.

Read-only tools/thread-cpu-profile.ps1 samples thread CPU time, user/kernel
split, descriptions and start addresses without suspension or elevation.
Handles close and process start identity is checked. Existing threads only;
thread-ID reuse within a sample remains a limitation. Start address is not a
sampled executing stack, and time not on CPU cannot be labeled waiting vs ready.
It was exercised on four20-second gameplay intervals in the same process.

| Sim head yaw | Mean logged ticks/s | Render CPU, one-core percent | D3D9 worker CPU, one-core percent |
|---|---:|---:|---:|
|0|118.34|53.46|48.15|
|90, pub view|79.32|77.56|71.08|
|180, tower view|82.90|78.06|69.55|
|270|110.40|74.47|70.57|

CPU intervals20.02s; rates are5-6 complete3-second windows approximately aligned
by process start UTC and log boot milliseconds, excluding first3s. Not an exact
fresh-pair ledger or tail analysis. Captures precede sampling. Render thread
32980 identified by the log, starts in game executable; worker35652 starts in
nvd3dum.dll. Another busy thread31920 starts in executable CRT entry; its role
is unclassified. No thread-start address used as proof of its active stack.
Heavier views use roughly9-10ms render-thread CPU and8-9ms driver-worker CPU
per reported tick. These run in parallel and MUST NOT be summed as frame cost.
Meaningful CPU-side render/submission work is established, sole bottleneck and
removable cost are not. No optimized build or gain is claimed.

Next concrete measurement: executing call-stack attribution on the render thread
and D3D9 worker in the reproducible pub view, or scoped CPU-vs-wall attribution
across render-command processing if non-elevated stack capture is unavailable.
Avoid shader-quality reductions, driver setting changes, AER changes or more
small API probes until the CPU work is attributed. Confirm any resulting change
on the real runtime; the sim view still exceeds the headset62-66 ticks/s.

Evidence: build/performance-results/vr125-view-sweep and build/vr125-sim captures.
Game closed after capture; full installed INI and DLL match pre-run. No merge.

## Non-elevated instruction samples (2026-09-15)

Continued VR-125 without another ticket or renderer install. Automated pub-view
run used verified298-g3d80740a9, original2750x2850, simulated120Hz. Archived
both logs before and after; installed DLL and full INI hashes unchanged, game
closed. Evidence: ignored build/performance-results/vr125-ip-sample.

New tools/thread-ip-profile.ps1 uses documented WOW64 context capture from an
external64-bit process. Opens verified-owner thread handles, briefly suspends
one thread, captures control registers, and resumes in compiled finally before
allocation/output. Retained handles prevent thread-ID reuse redirecting samples.
No target-memory writes, injection, symbols or full stack unwind. Jittered
7-17ms requested sampling sleeps are quantized by Windows. Full module paths
separate native D3D9 from the identically named proxy. Module list is a snapshot;
use only with a stable scene, not across module unloading/loading.

Three-sample smoke passed, followed by1500 samples/thread and1000/thread with
reversed thread order, zero context failures. Mean measured suspend/capture/
resume span52us, maximum1.18ms and1.90ms. These are perturbing WALL-TIME instruction
samples, including waits and WOW64 transitions, not on-CPU percentages, exclusive
function times, frame benchmarks or call stacks. Do not infer time savings from
their fractions. The sampler itself can change scheduling and lock behavior.

| Render thread sample location | First1500 | Reverse-order1000 |
|---|---:|---:|
| Game executable |36.27%|35.90%|
| Native Windows D3D9 |13.20%|14.10%|
| Mod proxy |12.87%|13.30%|
| ntdll |26.20%|28.70%|
| NVIDIA D3D9 |4.67%|3.90%|

Before/after unsuspended5-second CPU accounting put render thread near78% of
one core and driver worker69-74%. Correction to earlier interpretation: high
worker CPU does NOT establish equivalent useful submission work. Its sampled
hot region contains a bounded polling loop with PAUSE, counter increment and
back edge. Region share fell17.6% to8.2% when sampling order changed, so polling
exists but exact share is unreliable and potentially profiler-influenced.
The third thread landed in ntdll88-89% of samples; WOW64 transition return IPs
are present and cannot be labeled a particular wait without stack/API evidence.

This establishes mixed engine/proxy/native execution, not a single removable
hotspot. Exact installed-build PDB was not found among archived candidates;
do not resolve this DLL against the newer build-folder PDB. Next: preserve an
exact DLL/PDB pair and add default-off CPU-vs-wall scopes across engine rendering
and the proxy stage boundaries, then use unsuspended measurements in the same
pub view. Full executing stacks remain unavailable. No driver setting change,
AER experiment or performance improvement is claimed. Real-runtime validation
remains necessary before promoting any optimization.

API reference: https://learn.microsoft.com/en-us/windows/win32/api/wow64apiset/nf-wow64apiset-wow64getthreadcontext

## Ticket cleanup (2026-09-15)

Posted the approved previous findings to VR-125. Verified merged PR64 and moved
VR-118/119/120 to Done. Completed profiling VR-121/123/124 moved to In Review;
this is measurement review, not a claim their branches merged. Parked unresolved
VR-115/95/109/50 moved to Backlog. PR56 explicitly retained the remaining
VR-50/109 scope, so they were not closed. No ticket or branch deleted.
Re-query confirmed only VR-125 and the other collaborator's VR-122 In Progress.
Continue this investigation under VR-125 rather than multiplying experiments
into new tickets. No merge authorized or performed.

## Scoped CPU-cycle attribution (2026-09-15)

Built and installed307-ga658ed7a9-dirty, compiled23:39:36. Exact source patch,
DLL, PDB and hashes retained in build/playtest-candidates/vr125-cpu-scopes.
The only installed INI difference was new Perf.CpuScopes=0. Full CRLF diff
verified. Default-off instrument and live `perf cpu on|off` wrap the existing
Present stage stamps plus each original viewport call on the game thread.
No rendering algorithm or engine-memory write added. TLS accumulation and
an atomic generation invalidate intervals across off/on changes; invalid
counter/thread intervals are rejected. Cycles include user and kernel work.

Automated120Hz simulator, original2750x2850, same pub view. One toggle was
initially overwritten by a following seam command before its1Hz poll; the log
proved CPU scopes remained off. Reissued toggle and mark as ONE multi-line
command and verified the on acknowledgement before interpreting measurements.
Always batch related seam commands or wait for their logged acknowledgement.

Accepted on segment lasted51.16s,17 windows, zero counter failures, about8445
render-stage intervals. Per-present weighted means:

| Render stage | Wall ms/call | Million measured thread cycles/call |
|---|---:|---:|
| Outside Present: engine rendering + draw hooks |4.227|15.117|
| Native Present |1.236|0.690|
| Pre-tick |0.123|0.417|
| Game tick callback |0.096|0.346|
| XR end |0.114|0.306|
| Capture/method |0.096|0.260|
| XR begin |0.147|0.047|
| Pre-native-Present |0.001|0.005|

Outside-Present accounts for87.95% of measured render-thread cycles. This is
NOT87.95% recoverable frame time, and does not separate engine from proxy draw
hooks or prove GPU work is irrelevant. Under reentry there are two presents
per stereo tick. Game-thread viewport-first/second measured0.640/0.590ms wall
and2.349/2.155million cycles. Those scopes overlap the render thread and must
not be added to its frame cost. They do not support moving viewport calls to
more threads as the immediate remedy.

Measurement limitation found: GetThreadTimes accounting is too coarse and
phase-biased for these short scopes. Some per-stage CPU ms exceed wall ms
(e.g. pre0.803 vs0.123), despite zero API errors. Do NOT use those per-stage
CPU-ms fields for attribution, calculate blocked time by subtraction, or
interpret them as evidence of a CPU bug. QueryThreadCycleTime is the useful
relative signal here; keep cycles as cycles, never convert to milliseconds.
Reference: https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-querythreadcycletime

Off/on/off complete3s log windows, discarding first4s of each interval:
80.23 /82.36 /80.68 ticks/s (20/15/5 windows). Unequal durations, one run, mean
logged ticks rather than fresh-pair tail analysis. No evident throughput
penalty at this precision, and the higher on mean is NOT an optimization.
Screenshots verified scene/equipment before timing. No headset comfort claim.

Build, exports, INI golden and lint passed. Game closed and accepted298 DLL
and exact original INI restored through the candidate installer; full restore
diff only removes CpuScopes=0. Evidence build/performance-results/vr125-cpu-scopes.
No merge. Next concrete target is the outside-Present region: use the saved
matching symbols to identify costly proxy draw paths, then one bounded
redundant-work/caching experiment if the source and timing justify it. Do not
repeat resolution reduction, desktop skipping or broad threading toggles.

## Game INI survey and dynamic-shadow test (2026-09-15)

Read active DishonoredEngine.ini and compatibility presets section-by-section.
Active SystemSettings has DynamicShadows=True, LightEnvironmentShadows=True,
DetailMode=2, Static/SkeletalLODDistanceFactorMultiplier=1, static/dynamic/
unbatched decals enabled and DecalCullDistanceScale=1. Candidate workload
levers, not recommended defaults. Active UseVsync=False and bSmoothFrameRate
False; AO, motion blur and DOF already off. OneFrameThreadLag=True retained.
Shader compilation threading already enabled; it is not a steady-state renderer
parallelization switch. Texture pool160 is a separate streaming lead, not proof
of the stationary-view bottleneck. Some compatibility buckets retain Vsync=True;
do not confuse those with active settings or the mod's ForceNoVSync override.

Reused exact307 diagnostic and matching symbols, original2750x2850/sim120Hz.
Three automated Steam launches, Continue to same save and gradual yaw90 pub view.
Banners checked every run; logs archived before relaunch. Console scale get/set
DynamicShadows returned empty replies without a clear visual result, so that
live-toggle interval is NOT accepted as a verified setting comparison. Baseline
uses only the interval before the attempted set. Do not assume an empty reply
means successful application; the shipping console may omit a command/output.

Then changed ONLY SystemSettings.DynamicShadows from True to False in the game
INI while closed, preserving CRLF and verifying a complete file diff. Restarted
for the off sample, restored the entire original INI byte-for-byte, restarted
for final baseline. Backed up all game INIs and verified every file identical
to its pretest backup afterward. No graphics default promoted.

| Setting | Complete3s windows | Mean logged ticks/s | Outside-Present wall ms/present | Outside million cycles/present | SRT/present |
|---|---:|---:|---:|---:|---:|
| Original shadows on |7|79.94|4.369|15.601|81.43|
| INI shadows off |9|80.63|4.325|15.474|72.93|
| Original restored |9|79.26|4.392|15.695|78.39|

First4s after each mark excluded; scopes enabled across compared intervals.
CPU-ms attribution remains rejected; use cycles relatively, not converted to
elapsed time. SRT is render-target changes, NOT draw calls or shadow passes.
The decrease is consistent with a real workload change, not a full engine
setting readback. Final baseline SRT differs from initial, so scene variation
remains a confounder. Observed throughput gain is only0.9-1.7% against bracketing
runs, and cycles about0.8-1.4% lower. This does not establish a useful win or
eliminate shadow cost in other scenes. No20-40% gain here; keep original quality.
Not a headset timing/comfort test and not a fresh-pair tail comparison.

Evidence: build/performance-results/vr125-shadows, including raw logs, complete
config backups/diff and summary; eye captures under build/vr125-sim. Closed game,
restored accepted298 and exact original mod INI (only CpuScopes=0 removed).
No new ticket, no merge. Continue VR-125 with matching-symbol identification of
expensive proxy draw work. DetailMode/LOD/decal controls remain optional bounded
quality-cost experiments, not a blanket low-settings prescription.

## Matching-symbol render-thread samples (2026-09-15)

Two2000-sample batches in the pub view on verified307, same archived DLL/PDB
hashes as the scoped timing test. CpuScopes stayed off; NativeProfile remained
on as in preceding comparisons. Read-only5-second accounting identified render
thread42324 at74.4% of one logical core. Only this thread was suspended for IP
capture; no driver-thread sampling order interaction. Mean capture span63.9/
63.4us, maxima1.33/0.50ms. Attribution only, not an FPS benchmark.

New tools/resolve-candidate-symbols.ps1 verifies archived DLL and PDB SHA256
against manifest, then uses local64-bit SDK DbgHelp with exact-symbol loading,
local search path, no environment symbol path or prompts. No game process is
opened for resolution. Smoke lookup found hkPresent/source line in the archive.
All sampled proxy RVAs resolved in both batches. Symbols are leaf locations,
not inclusive call stacks; inline frames, linker thunks and runtime helpers
limit source attribution. Do not feed another build's RVAs to this resolver.

| Leaf function | Batch A | Batch B |
|---|---:|---:|
| hkSetVSConstF |30|40|
| native_profile Scope constructor |19|17|
| memcpy |14|21|
| CmpPtr (live-object table comparator) |12|10|
| WaDrawInner |10|9|
| native_profile Scope finish |6|10|
| native_profile enabled |6|10|
| weapon-frame match |6|8|

Proxy samples223/2000 and267/2000; game executable751/2000 and667/2000;
native Windows D3D9322/2000 and319/2000. Remaining include waits, kernel
transition return sites and other modules. No zero-error sample count is
proof of on-CPU sampling. Vertex-constant handling totals70/4000 wall-time
samples, not1.75% of CPU or guaranteed frame-time savings. No single proxy
leaf dominates. Profiler code itself appears, so diagnostic overhead remains
part of these captures; prior controlled diagnostic results must not be ignored.
Never remove liveness checks to optimize the comparator samples.

Inspected generated dvr_proxy.vcxproj after seeing small helper and incremental
link thunks: tested RelWithDebInfo already uses MaxSpeed (/O2), but inlining is
OnlyExplicitInline (/Ob1) and incremental linking enabled. Release permits
AnySuitable (/Ob2) and disables incremental linking. This is NOT an unoptimized
Debug build. A bounded compiler-inlining comparison is a reasonable next
experiment, with unchanged runtime settings and default-off opt-in build flag;
no claim of gain, no compiler setting or shipping default changed in this test.

Evidence build/performance-results/vr125-symbols: both raw IP batches, symbol
maps, CPU baseline, summary and both game logs. Full installed hashes matched
candidate before restore. Game closed; accepted298 and exact original INI
restored with complete diff only removing CpuScopes=0. No game INI changes,
no merge. Continue under VR-125; do not open a ticket for each measurement.

## Elevated headset trace, 2026-09-15

User reported substantially worse lag than prior evening. Exact307 banner
23:39:36 verified against candidate DLL/PDB; original2750x2850 and120Hz.
GeneralProfile plus GPU captured in memory, saved locally as
build/performance-results/vr125-etw-headset-20260915/headset.etl.
WPR successfully stopped and saved; zero lost buffers/events reported. Do not
read a still-merging ETL: early reads lacked CPU data until save completed.
Non-elevated WPR status incorrectly showed no recording while elevated status
confirmed active collectors; query recorder state in the elevated context.

Analysis interval190-230s from trace start, approximately log642936-682936ms.
Twelve complete log windows inside it average45.85ticks/s and21.65ms/tick,
versus earlier separate headset runs around62-66ticks/s. Not a matched control.
Scheduling reconstruction from CSwitch and ReadyThread agrees with xperf CPU:
render9692 runs25.550s (63.87%), blocked14.068s (35.17%), ready0.367s (0.92%).
Boundary/unclassified time about0.02s. Worker19852 runs28.697s (71.74%);
game-thread11460 runs10.104s (25.26%). Total system busy about27.49%.
These thread times overlap. Ready time is distinct from blocking; aggregate
CPU utilization does not establish readily usable parallel capacity.
About13.166s of render blocking ends at non-DPC wake events on worker19852.
Sampled stacks establish this worker is rooted in NVIDIA D3D9 driver code.
A wake event identifies the wake context, not necessarily the full dependency
chain or a removable wait. Do not infer wait durations from stack hit counts.

Render CPU stack samples (24416 with stacks): engine42.76%, native D3D9
20.56%, proxy13.07%, kernel8.69%, NVIDIA D3D9 driver5.12%, remaining modules.
These are exclusive sample shares of this thread's CPU execution, not total
frame-time fractions or potential gains. Proxy symbols match archived307.
NVIDIA worker27316 CPU stacks: EtwpEventWriteFull inclusive4919 (18.01%);
RtlWalkFrameChain4731 (17.32%), heavily overlapping. Render thread also
shows RtlWalkFrameChain957 (3.92%) and EtwpEventWriteFull692 (2.83%).
Recorder overhead therefore affects a worker the renderer waits on. It does
not prove the entire slowdown is recording overhead; same-day control needed.
The full GPU trace remains available for queue correlation after controlling
measurement perturbation. No rendering fix, speedup or sole bottleneck claim.

Next test: same307, identical INI, recorder OFF, real headset120Hz/original
resolution, same hub view and weapons for60s. One question: does performance
return to the earlier normal hub level? If yes, heavy tracing substantially
perturbed the run; if not, investigate today's baseline before using it as a
control. A future trace should be short CPU-only or custom GPU events without
per-event GPU stacks, bracketed by untraced intervals. Do not repeat the heavy
combined profile as a normal FPS benchmark. Keep VR-125; defer compiler test.

Previous agent-launch authorization ended with the requested shutdown.
Tester launches current headset tests. No merge authorized. Both game logs
archived. Accepted298 was briefly restored, then exact307 reinstalled for the
same-build recorder-off control; full INI diff only CpuScopes=0 addition,
CRLF verified by installer. No game INI changes.

## Untraced control and narrower capture, 2026-09-15

Same307 banner verified (23:39:36), same installed INI and original resolution.
Both logs archived in build/performance-results/vr125-etw-off-control.
Tester reports only slightly worse lag than the prior evening. Sixteen stable
hub windows, log2211171-2256187ms, average57.1375ticks/s and17.41875ms/tick.
Combined-trace windows averaged45.85ticks/s,21.65ms. The control is about24.6%
faster, consistent with significant recorder overhead; separate views/runs
and residual baseline variation prevent assigning the entire delta to tracing.

Correction to GPU availability: raw GPU queue events for the gameplay interval
190-230s are absent. First retained GPU event is312.751821s, AFTER game exit at
239.448196s. The rolling event collector overwrote gameplay while the kernel
collector retained CPU data. Zero dropped events is not proof of retained
interval coverage. GPU queue correlation cannot be performed on this run.
Original ETL preserved; large derivative GPU export stopped after proving this.
Do not publish or commit traces, logs, dumps, or symbol caches.

Source/stack follow-up: HUD end_frame flushes each active converted sink, and
scene read_done flushes its fence. CPU samples include HUD end_frame602/24416
(2.47%) and scene read_done361/24416 (1.48%), inclusive of nested work.
Mixed CPU/scheduling stack counts contain driver waits but are NOT duration
fractions. Batching HUD submissions is a bounded candidate, not an established
large win. Normal scene capture fence waits were zero in inspected late windows.
Do not remove image ownership fences or apply a synchronization change yet.

Added tools/timed-render-trace.ps1 and tools/wpr/dvr-render.wprp. Final profile
collects CPU samples with stacks plus process/image and scheduling events;
no syscall events, no GPU events, no CSwitch/ReadyThread stacks. Local profile
validation and elevated2s smoke confirmed sampled stacks and scheduling data.
An attempted custom GPU provider emitted no queue data at levels5 or255;
it is removed from the final profile, not presented as a validated GPU capture.

Helper requires an administrator token and an idle WPR session, never launches
the game, waits for a new process and expected build plus a populated hub
perf line (SRT>=70). This threshold is a test-specific workload trigger,
not a general gameplay detector. It marks30s baseline,40s trace, saves
immediately, excludes save/rundown plus10s cooldown, then marks30s baseline.
15min launch/load timeout; early game exit saves an owned trace. Never cancels
another recording. Read markers/acks and retained event coverage after the run.
Bounded64KB tail reads avoid streaming a growing log indefinitely.

Next headset run is armed in build/performance-results/vr125-light-headset.
Same307 and INI remain installed; no game launch or renderer change.
Tester: same hub view,120Hz,weapons out,stay3min,then exit. One question:
was lag close to the untraced control throughout? If yes, attribute normal
CPU/wait work with lower disturbance; if no, inspect before/during/after
markers and reject a materially perturbed interval. No frame-rate gain claimed.
Continue VR-125; no new ticket, branch, merge, or default change.

## Lightweight headset result and scope correction, 2026-09-15

Verified307 banner and archived final/previous logs under
build/performance-results/vr125-light-headset. Helper completed, all six phase
marks acknowledged; trace saved/stopped. Same-run mean logged ticks/s before/
during/after:55.4667/55.8/56.8 across9/12/10 complete3s windows, excluding
windows overlapping phase starts. Means17.9556/17.8417/17.57ms. Tester reports
consistent lag. No obvious material throughput penalty from this narrow trace;
not an optimization result. ETL41.1685s, zero lost buffers/events.

Interior trace5-35s: render19568 running21.729464s (72.43%), blocked8.073922s
(26.91%), ready0.171685s (0.57%), unclassified0.014044s. About7.381696s of
blocking ends at wake events from game-process worker14260. Its sampled stack
root is NVIDIA D3D9, and the previously disassembled polling region remains:
RVA0x14d095f alone2932 samples (14.99% of worker CPU stack samples), nearby
0x14d095b309 (1.58%). This is actual on-CPU sampling, unlike earlier suspended
IP batches, but polling time is not automatically reclaimable frame time.
No claim that removing waits or spinning is safe, or that driver waits identify
the ultimate cause rather than a downstream dependency.

HUD route withdrawn from this investigation. Tester reports unchanged
performance across the recent HUD update and explicitly rejects HUD work.
Normal trace HUD end_frame209 samples (0.99% of render CPU stack samples);
scene read_done118 (0.56%). Neither supports treating HUD conversion as a
substantial bottleneck. No HUD source, config, submission, or fence change was
made. Earlier proposed batching work is NOT the next step. Record the tested
negative and avoid revisiting it without new contradictory evidence.

Current target: engine draw/state submission and the native D3D9 driver
dependency, including distinguishing useful worker work from polling. Normal
render CPU leaves include native indexed drawing, texture binding and shader
constant updates; no single engine instruction dominates. Further changes
must target meaningful cost rather than convenient small functions. Preserve
working stereo synchronization. No new build installed, no pending playtest,
no agent game launch, no merge. CPU capture complete; GPU execution attribution
remains unavailable from the overwritten heavy trace.


## 2026-09-15: desktop nonblocking intervention, not yet headset-tested

Revisited preserved VR-115 performance-rollout evidence. Completely omitting
native desktop Present produced about14-17% more fresh pairs in repeated sewer
runs, but worsened frame-time tails. Reduced presentation moved most waiting to
the remaining calls. Those results are not a forecast for the current heavy hub.
Do not repeat those policies unchanged or promote their averages over pacing.

New narrow intervention: after the existing stereo capture and runtime submission,
try native IDirect3DDevice9Ex::PresentEx with D3DPRESENT_DONOTWAIT. Microsoft's
[PresentEx contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9ex-presentex)
returns WASSTILLDRAWING when busy instead of waiting for presentation. Only that
busy result becomes success for the game. INVALIDCALL latches ordinary Present
until reset or a live toggle; device-lost/hung/removed failures propagate unchanged.
A busy desktop update is omitted, never an eye capture. No FORCEIMMEDIATE/FLIPEX
assumption, new COM ownership, engine-memory writer, HUD change or fence removal.

Eligibility requires the exact device returned by our successful CreateDeviceEx,
live XR, projection requested, gameplay verdict, nonnull captured texture and a
current-present +/-1 draw record; any rectangle/window/dirty-region override
falls back. Menus/loading/mono/unknown context use the original Present. A native
reentry guard prevents a driver's internal Present route from capturing twice.
`Device.DesktopNonblocking=0` ships off; `desktopnonblocking on|off` switches live.
Counters identify attempts/accepted/busy/fallback/errors and all context gates.
This may move blocking elsewhere rather than remove it; an improvement requires
higher actual throughput without a pacing or visual regression.

Validation: 32-bit policy host exercised context fallback, busy skip, real device
failures, occlusion status, INVALIDCALL refusal latch and reset. A hidden standalone
native-D3D9Ex window executed200 PresentEx calls on this driver, no refusal/error;
no busy return in that tiny workload, so busy behavior is covered by policy tests,
not hardware reproduction. No game launched. Release build, nine exports, lint,
PowerShell parser and production/golden/package INI checks passed. Existing missing
CpuScopes=0 was synchronized into the release profile to match the writer; it
remains off. No profiler is recording.

Candidate: `build/playtest-candidates/vr125-desktop-nonblocking`, build
`vr33-hands-working-313-ga3dacd055-dirty`, exact DLL SHA256
`dd801875156d552092e13cf6a26d814126a5767aafcbda77380bd0a73e36ac09`.
Installed INI SHA256 `09346198ea0f5b80f62ca28c518c79fc190c5025c1c4da2fcecfeda0d4b459b2`.
Complete install diff contains only Device.DesktopNonblocking=0 added. CRLF
verified; previous307 DLL/INI and both logs archived by the installer. Restore
`build/playtest-candidates/vr125-cpu-scopes` for the exact pre-intervention state.

Headset protocol: launch and continue to the same laggy hub, same resolution,
120Hz and weapon state. Face the same expensive view for two minutes; briefly
turn the head near the end. `tools/timed-desktop-present.ps1` waits for a fresh
matching build and the established populated-hub SRT threshold, then30s off,
40s on,30s off. This threshold is a test trigger, not a universal gameplay flag.
The runtime gate independently checks actual gameplay. No ETW overhead. Read
only complete log windows within each marked phase, verify attempts actually
occurred, then compare native Present cost, fresh pairs/tails and logged tick rate.
The single user question is whether visuals and responsiveness remain normal
throughout. Normal permits quantitative evaluation, not automatic promotion;
any new artifact rejects this candidate. No performance claim before the run.
