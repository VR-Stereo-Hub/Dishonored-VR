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
