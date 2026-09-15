# Dishonored VR performance: canonical research and experiment record

## Maintenance rule

This is the single maintained performance research document, by user request
2026-09-15. Add all new hypotheses, online research, measurements, failures,
candidate identities, acceptance results and next steps here. Do not create
another performance plan/review/report. STATUS and NEXT_SESSION only summarize
current actions and link here. Engine addresses remain in patterns.h and their
technical derivations in ENGINE_NOTES; flicker findings still belong in the
required FLICKER_REFERENCE. Neither is a competing performance research record.
Old performance paths are redirect stubs. Historical source reports are retained
below with explicit provenance, so failed experiments and corrections survive.
The current verdicts below override every older plan or pending-test instruction.

## Current result: autonomous InitViews test complete, 2026-09-15

The user explicitly authorized performing this test autonomously, superseding
the earlier no-launch constraint for this test. Ran build347 through Steam with
the simulator; this is not headset acceptance. No further headset repeat is
needed just to collect these diagnostic counts. The game exited normally via
console quit. Exact307 baseline is restored, and no test is pending for the user.

### Test identity and selection

Verified347 DLL hash and new log banner before interpreting the run. Runtime
reported dvr-xrsim and120Hz. Captures confirm Hound Pits pub view, simulated
head yaw90, existing weapon state, two projection views with actual2750x2850
source textures. Simulator compositor output is1032x1104 and does not represent
VDXR transport, scheduling or perceptual comfort. No save edits or HUD changes.

Kept the fixed pub view across profiler off/on/off. Commands and phase marks
were acknowledged in the log. Complete perf windows exclude the first3100ms
of each phase and end before the next mark. Runtime scene variation remains:
SRT74.5-81.5/present in all three phases. Rates are rounded logged ticks/s,
not a fresh-submission ledger or a statistically powered overhead bound.

| Phase | Complete perf windows | Mean ticks/s | Mean tick ms |
|---|---:|---:|---:|
|Off before|14|67.54|14.729|
|On|15|68.59|14.493|
|Off after|19|67.85|14.684|

No obvious throughput penalty from the narrow probe in this run. The higher
on mean is not a performance improvement; the diagnostic changes no rendering
policy. No claim of headset speed or pacing acceptance.

### Scope result and changed hypothesis

16 complete scope windows,48.075s:9,890 InitViews calls,5027.762ms inclusive
wall time,10.458% elapsed. No overflow or other-thread calls. All calls came
through return RVA0046C0C1. Of6,595 completed intervals,44 were unknown; their
88 calls/40.345ms are kept separate from the tagged pair estimate.

| Completed interval | Intervals | InitViews calls | Wall ms | Largest call ms |
|---|---:|---:|---:|---:|
|Left (-1)|3,275|6,526|2878.788|2.597|
|Right (+1)|3,276|3,276|2108.630|1.470|
|Unknown|44|88|40.345|1.051|

Tagged aggregate4987.418ms divided by3275.5 equivalent pairs (mean L/R
interval count) gives1.523ms per pair. This is a completed-interval estimate,
not cost per verified fresh XR submission. It includes waits. It clears the
predeclared1ms threshold for continuing preparation research.

The simple one-call-per-eye prediction is falsified: this view averages1.993
calls per left interval and1.000 per right interval. It establishes repeated
preparation, but cannot establish identical renderer/view contents, safe reuse,
duplicated AI, or that the extra call is redundant. A capture/auxiliary scene
or other additional view remains possible. The probe intentionally retained
no renderer identity. Do not skip every second call based on this result.

Next engineering target: classify the extra left-interval invocation and the
conditional child at VA00864AD0 (about80% of InitViews' inclusive CPU samples
in the existing headset trace). Use executable control flow and existing trace
before another probe. Separate visibility, occlusion-result processing and mesh
collection before designing reuse. The0.102ms query-helper result remains
eliminated; it does not exclude surrounding preparation cost. Larger draw-pass
stages remain the fallback if preparation cannot safely share meaningful work.

### Validation, artifacts and restore

After measurement a gradual yaw90-to105 head turn still produced two projection
views and nonblack eyes. Final simulator state:32,668 frames,0 errors,0 discarded,
0 out-of-order ends. These are bounded runtime checks, not headset comfort proof.
Image-preview attempts initially failed because the ordinary filesystem sandbox
helper was broken and the PATH Python lacked Pillow. A compact Windows image
library preview succeeded; these did not affect the subsequent marked phases.

Local artifacts: build/performance-results/vr125-initviews/sim-20260915-181540
contains both logs, pre/post INI, analysis.json and its script, initial installation
record, simulator state, and pub/after-turn captures. Raw captures remain ignored.
Before restoring307, verified full installed INI was byte-identical to pre-sim
347 configuration, so no simulator runtime path remained. Restore archive:
build/playtest-candidates/installs/20260915-182316-329268. Complete restore diff
removes ScenePrepareProfile=1 and returns NativeProfile/BridgeGpu/GpuQueries/
FrameId from0 to1, restoring the exact307 baseline. Installed DLL and INI hashes
and CRLF verified.347 candidate remains recoverable at vr125-initviews.
No game process remains, no merge or publication performed.

## Tested candidate: InitViews timing, 2026-09-15

Continued on codex/performance-research. Verified installed.json before edits,
then actual installed DLL SHA256 against the archived307 baseline. The current
log still has the previous316 query-test banner, so it is not a new307 run.
No query-helper repeat, HUD change, synchronization change or game launch.

### Existing capture attribution

Parsed the bounded HTML table from the normal real-headset CPU capture,
build/performance-results/vr125-light-headset/render-cpu.html. Population is
21,085 render-thread sampled CPU stacks in that report, distinct from the
24,416 stacks in the older heavy capture. This is inclusive sampled CPU share,
not wall duration or guaranteed recoverable frame time.

| Sampled return RVA | Boundary established offline | Inclusive samples | Share |
|---|---|---:|---:|
|0046C0C1|Engine-labelled InitViews, before render-pass loop|2,575|12.21%|
|004671D9|Conditional child inside InitViews|2,062|9.78%|
|0046C1F4|First render-pass stage|7,796|36.97%|
|0046C208|Second render-pass stage|7,019|33.29%|

The InitViews child accounts for about80% of the parent's sampled population;
these inclusive rows overlap and must not be added. Engine debug strings
identify InitViews and the later World/Foreground/editor pass labels. The
four-pass loop is not a four-eye loop. This local executable evidence supplies
a specific preparation boundary, not proof that its work can be shared safely.
ENGINE_NOTES records ABI/address derivation; patterns.h holds the signature.
Bounded extracted rows and source hash are archived locally under
build/performance-results/vr125-initviews/sample-boundaries.json.

### Falsifiable measurement and limits

New Perf.ScenePrepareProfile ships0, installed candidate arms1. It times the
complete InitViews function on the Present thread and groups calls, total/max
wall time and caller return RVA by completed render interval (-1/0/+1). Three
second windows report L/unknown/R interval counts, foreign-thread calls and
row overflow. The first partial interval and gameplay/toggle transitions are
discarded. sceneprepare on/off toggles measurement live, with refusal if the
hook was not launch-armed. Fixed tables, no per-call allocation/logging, no
retained renderer identity, no scene changes or new waits. QueryWaitProfile
stays off. Engine writers, image-owned orientation and stereo fences unchanged.

Prediction: in a steady populated hub InitViews executes on both eye intervals
with material aggregate cost. Near one call per L and R interval supports
repeated per-eye preparation, not redundant AI. If total cost is at least1ms
per pair, investigate the dominant child for eye-independent subwork. If it
is below0.3ms per pair, deprioritize this stage and investigate the larger draw
submission boundaries. Intermediate costs require benefit/risk judgment. These
are decision thresholds, not predicted savings. Foreign/unknown coverage,
missing hooks or unstable gameplay prevent an inference of negligible cost.
Wall time includes waits; it cannot be added to nested samples or GPU timings.
No preparation skipping/reuse is implemented without establishing valid state.

The diagnostic is active at launch and needs no expiring external phase helper.
This run measures scope cost and call frequency, not profiler-off speedup.
There is no new throughput or visual acceptance yet. The consolidated branch
also remains untested in the headset until this launch.

### Validation and one-launch protocol

Standalone x86 host passes signature refusal, exact stack-realignment
trampoline,10,000 receiver/result-preserving calls, eye accounting, foreign
thread exclusion, live off/on transition reset, logging and hook removal.
Initial test assertion failed because its synthetic clock advanced past the
three-second reporting boundary; corrected the test clock and reran. This was
a test-window assumption failure, not a changed engine hypothesis.
Generated/golden/release INI checks and lint pass. Release build and all nine exports pass. No game or simulator launched.

Installed build `vr33-hands-working-347-g5a46c6ead-dirty`, candidate
`build/playtest-candidates/vr125-initviews`. DLL SHA256:
`28740649ac05e6ea9e845e3aad3566d7994c9ce411104b27b2669f78dfff27c8`.
INI SHA256:
`cda8d714639d5f43b68d16e26e50d5806a82d65643a13c3ff119cb21accd8032`.
Prior307 DLL/INI and both logs archived before installation in
`build/playtest-candidates/installs/20260915-174718-586872`.
Complete INI diff: add ScenePrepareProfile=1; change NativeProfile, BridgeGpu,
GpuQueries and FrameId from1 to0. Those inherited unrelated profiling passes
are disabled; basic Instruments=1 frame timing and existing state traces remain.
All other INI bytes are unchanged; CRLF and installed hashes verified. Thus
this is not a controlled307-versus347 throughput comparison: instrumentation
configuration and consolidated code differ. No frame-rate gain is claimed.

One launch: same populated hub view, original2750x2850 at120Hz, same weapons.
Remain facing the expensive view for90 seconds, briefly turn head/hands near
the end, then exit. One question: did the usual lag and image behavior remain
representative throughout? Expected unchanged. If yes, read matching complete
windows for InitViews cost/frequency; if noticeably worse or visually changed,
treat the candidate as a regression and restore307 before attributing a win.
No ETW recorder, simulator or agent-launched game. Both logs must be archived
before installation. Exact rollback remains vr125-cpu-scopes/build307.

## Previous baseline verdicts (current installed candidate above)

- Active ticket VR-125, branch codex/performance-research. No merge approved.
- Query-helper diagnostic316 measured; restored previous307 with the probe off.
  Original2750x2850/120Hz and CpuScopes=0 remain the baseline.
- Quarter pixels improved rate about6%; almost twice the pixels worsened rate to
  approximately45-50fps. There is a resolution-independent floor, not proof that
  GPU cost is absent. Simulator performance is not headset performance.
- Light real-headset trace:55.47/55.80/56.80ticks/s before/during/after capture.
  Render thread72.43% running,26.91% blocked,0.57% ready. NVIDIA worker wakes
  most blocking and has a polling hotspot. These fractions are not recoverable
  time. Heavy CPU+GPU recording lowered rate; its GPU events overwrote gameplay.
- Nonblocking desktop Present313 rejected:58.21/57.70/58.29ticks/s off/on/off;
  last on heartbeat4818 accepted attempts,0 busy skips. Previous307 restored.
- HUD excluded. Preserve accepted image-owned orientation and stereo fences.
- Measured engine query-helper cost is only0.102ms/stereo pair (0.596% elapsed).
  Next: inspect existing CPU samples and viewport render preparation/culling
  boundaries for duplicated per-view work. No further unchanged query test.
  Do not infer double AI updates: only viewport Draw is doubled, after world tick.

## Consolidation checkpoint before InitViews candidate, 2026-09-15

Continue all performance work on `codex/performance-research`, created at user
request. VR-125 remains the active investigation; existing experiment tickets
remain their original provenance. No new ticket is needed for consolidation.
All branches below are ancestors of the combined branch and remain preserved.

| Source branch | Preserved tip |
|---|---|
|`codex/vr-113-performance-audit`|`ad3c2b168`|
|`codex/vr-115-desktop-present`|`1d6558a32`|
|`codex/vr-115-performance-rollout`|`39125145e`|
|`codex/vr-121-render-thread-profile`|`d3c64c731`|
|`codex/vr-121-native-draw-profile`|`7afcefd6f`|
|`codex/vr-123-bridge-gpu-profile`|`71ad522f8`|
|`codex/vr-124-diagnostic-overhead`|`dbfdffb0f`|
|`codex/vr-125-resolution-floor`|`62738ec88`|
|`performance-fix`|`958426910`|

The desktop rollout supersedes the earlier desktop candidate with identical
core desktop implementation/tests. Its audit retains the original VR-113 text
plus a later status preface. Those two earlier branches were ancestry-merged
with the current tree retained after verifying this equivalence. Other unique
code branches were merged normally with explicit conflict resolution. Shared
released-eye serial state is deduplicated; desktop and diagnostic benchmarks
consume the same successful-submission identity. All experiment switches ship
OFF; the nonblocking desktop experiment takes precedence when explicitly enabled.
Run only one benchmark/behavior experiment at a time. Consolidation is not
acceptance of rejected experiments or authorization to merge to VR-Main.

Older branches whose names contain perf/research but whose unique changes are
historic eye re-arming, camera/FOV or unrelated stereo strategies are not imported:
`origin/claude/dishonored-vr-perf-9f4b10` and
`head-tilt-fix-and-resolution-research`. Their unique code predates the accepted
stereo/camera fixes. `performance-fix` is already inherited. No branches deleted.

Research from the merged branches is already preserved in this document's
historical appendix. Their old standalone report paths now redirect here.
Your installed307 baseline is unchanged by consolidation; the combined DLL is
built but NOT installed or headset-validated. Keep installed.json as installation
truth rather than inferring installed build from the checked-out branch.

### Next test preparation

1. Read this current section and verdicts, then only the relevant evidence below.
   Existing normal CPU capture: build/performance-results/vr125-light-headset.
   Avoid heavy ETW capture: it perturbed throughput and lost gameplay GPU coverage.
2. Inspect render-thread sampled stacks and the viewport/render-command boundary
   to locate repeated scene preparation, visibility and native draw submission.
   Huge exported .plain files can be single-line: parse them or read bounded slices.
   World tick runs once; viewport Draw runs twice. Do not assume duplicated AI.
3. Identify an expensive measured boundary and a falsifiable intervention or
   narrow timing probe. If deriving an address, use existing RE tools and
   ENGINE_NOTES, put verified addresses/signatures in patterns.h. Preserve HUD,
   stereo synchronization and image-owned orientation. No guessed bypasses.
4. Build and run appropriate standalone tests on this consolidated branch before
   installation. Archive both game logs; install a named recoverable candidate
   based on the currently installed INI, full diff and CRLF/hash verification.
   All unrelated diagnostic/benchmark switches stay off. Build307 rollback is
   build/playtest-candidates/vr125-cpu-scopes. The new probe is not yet designed.
5. Give one question for one launch in the same populated hub view at original
   resolution/120Hz. Define expected outcome and counterprediction. The tester
   launches; never launch the game or simulator. Do not leave a15-minute helper
   to expire silently: use a long configurable launch wait and separate gameplay
   deadline, or an in-process phase controller, and verify phase marks in the log.
6. Compare only complete matching gameplay windows. Record failures as well as
   results here. Do not repeat the eliminated query-helper, DONOTWAIT, diagnostic
   suppression, quarter-resolution or same shadow tests unchanged.

Local validation: Release build, standalone desktop benchmark/native D3D9,
render/native profiling, bridge policy/device lifecycle, diagnostic A/B,
query trampoline and generated/package/golden INI checks pass. No game launched.
Publication remains local: previous automatic approval review blocked GitHub and
Linear payloads. Do not bypass or silently retry that block. A short proposed
Linear update is pending explicit approval; no ticket state change is claimed.

## Query-helper headset result, 2026-09-15

Verified current log banner316-g7e140527d-dirty and installed DLL SHA256 against
its candidate before analysis. Archived current/previous logs and installed INI
under build/performance-results/vr125-query-waits/run-20260915-172646;
analysis.json contains aggregate data. These local artifacts are not committed.

The automatic phase helper expired after its15-minute launch wait, before the
tester launched. Therefore no off/on/off comparison occurred. QueryWaitProfile=1
was active from launch and the byte-verified hook installed successfully, so the
run still measures helper cost. It does not measure instrumentation overhead or
establish a subjective performance/visual result; the tester reported completion
only. Do not request another run solely to replace the absent control phases.

Selected81 complete gameplay query windows,243.335s and14230 completed stereo
pairs, using nearby perf tick windows with SRT>=70/present and>=45ticks/s.
Selected timestamps34642234 through34882562; startup/exit excluded. The selection
is an operational hub-view filter, not a general-purpose game-state classifier.
Total inclusive helper wall time1450.505ms,0.5961% elapsed,0.10193ms/pair.
No row overflow, other-thread calls or helper-false results in this selection.

| Caller RVA | Runtime type | Calls | Total helper ms | Largest call ms |
|---|---|---:|---:|---:|
|005BF54A|EVENT(8)|28624|31.915|0.176|
|005C131F|OCCLUSION(9)|8257273|1418.581|0.932|
|005BD170 /005BD18A|TIMESTAMP(10)|3 each|0.000 rounded each|0.000 rounded|

Verdict: this shared query-read helper is not a major steady hub bottleneck.
Even eliminating all its measured cost would recover only about0.10ms/pair.
This includes native polling inside the helper, but excludes query issue work,
GetType/counter bookkeeping, other native paths, and waits inside Present/draw.
It does not establish that occlusion culling itself is cheap, only these reads.
The high query-call count is not evidence that AI/world simulation ran twice.

Exact307 baseline restored; full INI diff only removes QueryWaitProfile=1,
CRLF and candidate hashes verified. Install archive:
build/playtest-candidates/installs/20260915-172812-753329.
Next action: inspect the existing normal CPU trace
for duplicated per-view scene preparation, visibility and draw submission. Map a
specific expensive boundary before adding another probe or requesting a headset
run. Preserve synchronization and accepted image-owned orientation; HUD excluded.

## Tested candidate: engine query helper timing, 2026-09-15

Offline derivation found the shared native query-read helper and five direct
callers. It polls GetData with FLUSH when permitted; existence does not establish
runtime cost. Address/signature/ABI derivation lives in ENGINE_NOTES and patterns.h.
A15-byte signature guards installation; a6-byte whole-instruction trampoline
preserves thiscall/four-stack-argument behavior. No flag, result or waiting-policy
changes. Diagnostic timing surrounds the complete helper, not individual polls.
GetType is read only on the live argument; no query pointer or COM reference is
retained. Fixed-capacity tables, no per-call logging or allocation. Other-thread
calls and overflow are reported, so missing coverage cannot establish zero cost.
Caller RVA, query type, wait permission, total/max wall time and helper false
returns are grouped by the completed render interval's eye. This is not query
creation/issue-eye attribution. The result is boolean, not HRESULT. Direct native
query paths outside this engine helper remain outside the measurement.

Ships off and installs only when Perf.QueryWaitProfile=1 is armed at launch;
querywait on|off changes measurement live. The diagnostic remains installed as a
pass-through while off. Code and exact candidate remain recoverable on this branch;
no merge approved. Existing rejected DesktopNonblocking stays off.

Validation passed:32-bit synthetic helper tests exercise signature refusal,
trampoline/four-argument ABI, unchanged full return/data, type classification,
completed interval assignment, other-thread exclusion, toggle/transition reset,
and hook removal. Installed game matches the15-byte signature. Release build,
exports, lint and generated/golden/release profile equality pass. No game launched.
Engine cost is now measured above; the original test setup below is historical.

Candidate archive: build/playtest-candidates/vr125-query-waits.
DLL SHA256: `ac836a714818bb8132600c06489d8c1bf45302d94c42a0903f4beca20a37d67f`.
INI SHA256: `e29baf5e99bb3a84cb4433869ba1950c61706138fdd3f9d9555a0ccd6529d92f`.
Full installed INI diff only adds Perf.QueryWaitProfile=1; CRLF preserved.
Prior307 plus both logs: build/playtest-candidates/installs/20260915-153449-315664.
Rollback: install archived vr125-cpu-scopes, checking the full diff again.

Test: same laggy hub view and weapons, original resolution/120Hz, two minutes.
Timed-query-wait helper waits for matching build and populated hub log, then30s
off,40s on,30s off. One question: did lag or visuals change noticeably at any
point? Expected unchanged. If stable, use complete marked gameplay windows and
query totals to evaluate material cost. If more laggy during profiling, first
account for measurement overhead. No speedup expected from this diagnostic.
No WPR/ETW recorder runs. Do not request a new run if counters/coverage already
answer the question. If helper time is negligible, investigate duplicated scene
preparation/culling rather than removing fences. If large, locate the expensive
caller and prove a safe scheduling/reuse change before implementing it.

## Experiment verdicts, in priority order

| Topic | Verdict | Evidence and limits |
|---|---|---|
| Pixel count | Original-resolution floor; high resolution still costs | Quarter-pixel ~6% faster, higher resolution45-50fps; separate runs |
| CPU/driver | Critical dependency identified, exact removable cost unknown | Light ETW scheduling and native draw/state samples |
| Desktop DONOTWAIT | No useful gain; do not repeat unchanged |313 off/on/off, actual path active, no busy skips |
| Desktop omission | Throughput gain with worse tails; parked | VR-115 repeated sewer runs~14-17% faster; not a hub forecast |
| Reduced desktop rate | Small/inconsistent benefit; waiting shifts | VR-115 one-Present-per-pair |
| Dynamic shadows | No useful gain in tested view |79.94/80.63/79.26 simulator ticks/s; not all lighting eliminated |
| Diagnostic suppression | No compelling gain |63.66/64.48/64.27 fresh pairs/s, VR-124 |
| Bridge GPU copies | Individually small |~0.10ms/eye conversion and~0.057ms/eye XR copy, VR-123 |
| Buffer/texture locks | Low sampled cost | Native draw profile; sampled maxima do not bound all calls |
| Shader reflection | Small measured aggregate cost |~5ms/s; not a large frame-budget recovery |
| Engine query-read helper | Not a major hub cost |316:0.102ms/pair,0.596% elapsed; other native wait paths remain open |
| Visibility/culling/captures | Open, conditional second priority | Per-eye redundant work possible, not yet established |
| AER rewrite | Major alternative, not implemented | Existing legacy path broken; stable eyes/world sync required |
| Epic x64 port | Unmeasured alternative | Different executable requires real mod port; bitness promises no gain |

- [Record 12: PERFORMANCE_AUDIT.md, later profile branch](#record-12)

- [Record 13: RENDER_THREAD_PROFILE.md, later profile branch](#record-13)

- [Record 14: PERFORMANCE_ROLLOUT.md, later profile branch](#record-14)

## Evidence rules

Match installed DLL hash and log banner. Archive log plus prev before launch.
Use complete same-phase gameplay windows; separate presents, tagged ticks, fresh
pairs and runtime submissions. CPU wall time includes waiting; sampled CPU stacks
do not measure blocked duration. GPU timestamp spans include feeding gaps and
cannot be added to overlapping CPU time. A high average cannot excuse worse
frame-time tails. Never treat empty console output as an applied setting.
Default-off render levers require live A/B, identity, rollback and one question
per user launch. No new game launch authorization exists. Raw evidence stays in
ignored build/, never Git. No subagents.

## Research index

- [Record 1: RESOLUTION_FLOOR.md](#record-1)
- [Record 2: NATIVE_DRAW_PROFILE.md](#record-2)
- [Record 3: UE3_PERFORMANCE_RESEARCH.md](#record-3)
- [Record 4: PERF_REVIEW_2.md](#record-4)
- [Record 5: PERF_PLAN_2.md](#record-5)
- [Record 6: PERF_REVIEW.md](#record-6)
- [Record 7: PERF_PLAN.md](#record-7)
- [Record 8: DESKTOP_PRESENT_PERFORMANCE.md](#record-8)
- [Record 9: PERFORMANCE_ROLLOUT.md](#record-9)
- [Record 10: BRIDGE_GPU_PROFILE.md](#record-10)
- [Record 11: DIAGNOSTIC_OVERHEAD_AB.md](#record-11)

<a id="record-1"></a>

## Record 1: RESOLUTION_FLOOR.md

Historical source migrated on 2026-09-15. Current verdicts above take precedence.

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


### Headset result: no benefit, previous build restored

Verified installed313 SHA256 and matching log banner (Sep15 09:06:05). Timed
helper completed off/on/off and exited. Archived final/previous logs locally in
`build/performance-results/vr125-desktop-nonblocking`. Exclude3s performance
windows crossing a phase boundary or live-toggle boundary. Complete-window means:

| Phase | Windows | Logged ticks/s | Tick ms | Native Present ms, both eyes summed |
|---|---:|---:|---:|---:|
| Before, normal |10|58.21|17.060|4.840|
| Nonblocking |13|57.70|17.254|4.869|
| After, normal |9|58.29|17.033|4.878|

Last on-phase heartbeat:4818 attempts,4818 accepted,0 busy,64 context fallbacks,
0 errors, no refusal. These are last-heartbeat counts, not claimed final totals.
Actual new API path was active; every attempted call returned success without a
busy skip. The measured wait was not bypassed. On-phase throughput is about0.94%
lower than the mean of the two controls, not a useful difference or speedup.
Tester reports no apparent effect. Reject for promotion and do not repeat this
mechanism unchanged. No need to infer frame-time-tail improvement from averages.
This eliminates this API flag as a useful remedy on this workload, not all
desktop overhead, GPU waits or native draw submission as possible costs.

Restored exact pre-test307 DLL/INI from `vr125-cpu-scopes`. Full installed INI diff
only removes DesktopNonblocking=0; CRLF verified. Restore archive:
`build/playtest-candidates/installs/20260915-091631-188152`. Experiment remains
recoverable and default-off on the branch. No helper/recorder or test pending.
Next attribution must target engine/native draw work and its driver dependency;
a usable gameplay GPU timeline is still absent. Do not continue API-wait guesses
or reinterpret the measured blocked fraction as guaranteed recoverable time.


<a id="record-2"></a>

## Record 2: NATIVE_DRAW_PROFILE.md

Historical source migrated on 2026-09-15. Current verdicts above take precedence.

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


<a id="record-3"></a>

## Record 3: UE3_PERFORMANCE_RESEARCH.md

Historical source migrated on 2026-09-15. Current verdicts above take precedence.

# UE3 performance research, 2026-09-15 (VR-125)

## Finding and limits

Low whole-machine utilization plus weak resolution scaling is compatible with
serialized CPU rendering and GPU feeding gaps. It does not prove one broken INI
or that the unused CPU cores can execute this workload automatically. The light
trace records render-thread72.43% running,26.91% blocked,0.57% ready. On12 logical
processors, one fully occupied thread represents only about8.3% of aggregate
scheduled CPU time. Do not equate low aggregate CPU with a fast critical thread.
Quarter-pixel rendering improved throughput about6%, not zero. Increasing pixel
count substantially reduced throughput. There is both a resolution-independent
floor and a higher-resolution cost; sole CPU or sole GPU ownership is unproven.

## Primary sources

- [Epic UE3 level optimization](https://docs.unrealengine.com/udk/Three/LevelOptimization.html):
  game/render thread idle distinction, driver overhead, object/light interactions,
  scene-rendering counters and decal section multiplication. Intended for UE3;
  modern UE4/5 console-variable lists are not evidence a Dishonored key exists.
- [Microsoft D3D9 profiling](https://learn.microsoft.com/en-us/windows/win32/direct3d9/accurately-profiling-direct3d-api-calls):
  batching and deferred state work can charge cost to later calls. Optimizing the
  API where elapsed time appears need not remove the underlying work.
- [NVIDIA occlusion culling](https://developer.nvidia.com/gpugems/gpugems/part-v-performance-and-practicalities/chapter-29-efficient-occlusion-culling):
  early query-result waits can destroy CPU/GPU overlap; several outstanding
  queries let other useful work proceed before results are needed.
- [Microsoft D3D9 queries](https://learn.microsoft.com/en-us/windows/win32/direct3d9/queries):
  GetData can poll without flushing or request a command-buffer flush; result
  availability and command submission are different from CPU execution cost.
- [Microsoft D3D9 optimization](https://learn.microsoft.com/en-us/windows/win32/direct3d9/performance-optimizations):
  draw/state overhead, batching and correct dynamic-buffer locking matter.
- [Microsoft Task Manager GPU accounting](https://devblogs.microsoft.com/directx/gpus-in-the-task-manager/):
  GPU engines are distinct; the summary chooses the busiest engine rather than
  averaging all engines. Inspect3D/copy/video activity and clocks for VR streaming.

## Ranked unresolved mechanisms

1. Per-eye engine synchronization, or serial dependency on the D3D9 worker.
   Viewport Draw is invoked twice, with bShouldPresent=true each time. This does
   not prove that a frame-completion wait occurs twice. It is the exact boundary
   to inspect. Attribute game-owned EVENT/OCCLUSION query GetData calls, their
   flags, result age, callers and total elapsed polling episodes per eye. Keep
   game queries distinct from the mod's image-ownership fences and diagnostics.
   Existing query CPU samples are small, but CPU samples cannot bound blocked
   wall time. If queries do not own material waiting, abandon this hypothesis.
   Do not blindly remove FLUSH or lie about completion: image corruption and
   stale-resource use are possible. A fix must preserve producer/consumer order.
2. Duplicated CPU scene preparation and expensive driver submissions. The world
   tick occurs before the intercepted viewport draw; we do not deliberately
   invoke AI/world simulation twice. Per-view visibility, draw setup, object/light
   interactions or render callbacks can still repeat. Rendering an NPC can be
   expensive independently of deciding what the NPC should do. Lower resolution
   does not reduce object or draw counts. Our wider VR view is another reason a
   flat-game comparison must match FOV. A second viewport call's0.6ms game-thread
   duration excludes the queued work on the separate render thread.
3. Visibility-history or occlusion-query interaction between the two eye views.
   Executable strings confirm the OCCLUSION query creation path and InitViews
   exist. They do not demonstrate query use, count, reuse or blocking in the hub.
   No existing query instrumentation separates game query types/eye/caller.
   No confirmed generic AllowOcclusionQueries/FinishCurrentFrame setting was
   found. Never transplant UE5 r.* recipes into this build and claim they work.
   If queries are cheap but draw counts are excessive, inspect culling behavior
   and per-eye view history; no shared-visibility shortcut without stereo safety.
4. Repeated auxiliary scene captures. Decompiled SceneCaptureComponent declares
   FrameRate, SetFrameRate, SetEnabled and skip-if-occluded controls. Presence is
   not evidence of active hub captures. Count live captures and render executions
   before changing them. A view-independent capture could potentially update
   once per pair; reflections can be view-dependent and must not be assumed safe.
5. External cap, runtime half-rate policy, clocks or memory/streaming stalls.
   Mod FpsCap=0, ForceNoVSync=1; game UseVsync=False, bSmoothFrameRate=FALSE;
   OneFrameThreadLag=True. This checks configured game/mod asks, not every driver
   profile or runtime override. Around60fps on a120Hz headset alone proves no
   half-rate lock. Need actual pacing/driver-policy evidence and effective clocks.
   PoolSize160 is a legacy texture-streaming budget, not total available VRAM;
   no evidence it causes stationary hub throughput loss. Do not inflate blindly.

## Already checked, and next decision

Native indexed draw/state work is substantial in aggregate; sampled vertex-buffer
and texture locks were tiny. Dynamic-shadow removal produced about1% variation
in prior simulator comparison, not a useful win; do not generalize to all lighting.
Nonblocking Present engaged but produced zero busy skips and no gain. Shared
capture has no per-eye CPU image readback on the installed route. HUD remains
explicitly excluded; accepted image orientation and fences remain protected.

Next investigation: establish whether game-owned D3D9 query polling or a frame
completion wait is repeated per eye, before another performance toggle. Offline
caller identification and existing trace analysis first; only if runtime counters
are required, use bounded aggregated timing with exact query ownership and one
focused run. Pair this with a retained GPU timeline if available, not an unbounded
heavy trace. If that path is cheap, proceed to per-eye scene/draw workload and
culling rather than another synchronization guess. No promised speedup.

Research only: no DLL/INI changes, build, installation, game launch, new ticket,
recorder or playtest. Exact307 remains installed. Earlier experiment source is
preserved on this branch, default off and rejected for promotion.


<a id="record-4"></a>

## Record 4: PERF_REVIEW_2.md

Historical source migrated on 2026-09-15. Current verdicts above take precedence.

# VR-67 / VR-68 revised review - 2026-09-09

**Prioritize pose/view timing and CPU/driver/GPU synchronization. The new run demonstrates a variable application rate near the native-80 regime, but it does not establish a pixel-bound GPU limit or prove the proposed two-generation weapon error. The claimed change in gameplay hitch location is a counting error.**

This report supersedes the conclusions and test order in `PERF_PLAN_2.md`. It reviews the plan; it does not implement a fix or change the installed configuration. The user's target remains 2750x2850 per eye at an 80 Hz headset with stable half-rate SSW. Native 80 FPS is a separate stretch target unless effective runtime behavior requires it.

The reported reliable Wi-Fi connection is a reason to prioritize application and runtime evidence. Encoding happens before transmission and is distinct from radio-link quality, but neither encoding nor Wi-Fi has been identified as the cause here. No network-settings sweep is the first step in this revised plan.

**Evidence.** The working tree is at `f26354c0`; the previous review's A/B corrections and period-change instrument are now committed in `c18ca785`. The new log identifies `vr33-hands-working-74-gd2b2d686-dirty`, built September 9 at 11:12:21, and VirtualDesktopXR 1.0.10. A dirty build banner limits exact source-to-binary attribution. Render size is restored to 2750x2850, capture is shared, the pose setting is Lag=2, and no automatic A/B sweep is logged.

The log is preserved locally at `build/perf-review-2026-09-09/vr67-plan2-original.log`, SHA-256 `11E05EFE8117E7111D8686412D0E23525E15459FFBBB63DD200F09BEB62551AF`. Bracketed timestamps below are the log's millisecond clock. Public runtime source establishes possible behavior, not a verified disassembly of the installed runtime.

**1. Correct the log populations before drawing conclusions.**

The log changes to GAMEPLAY at 8314609 and back to MENU at 8403078: **88.469 seconds**. All 18 `out`-dominated gap messages occur before gameplay. Thus the draft's 50-gap, 52%-tail/36%-out table describes the whole launch, not the gameplay complaint.

| Population | New run | Previous reviewed run |
|---|---:|---:|
| Gameplay duration | 88.469 s | 194.140 s |
| Detected Present-entry gaps during gameplay | 19 | 69 |
| Largest phase: submission tail | 19 | 69 |
| Largest phase: out/idle or out/R | 0 | 0 |
| Gap min / median / max | 41 / 53 / 106 ms | 40 / 62 / 115 ms |
| Recorded EndFrame median / max within those gaps | 46.1 / 99.3 ms | 53.6 / 108.8 ms |
| Detected gaps per minute | 12.9 | 21.3 |

One of the 19 new gaps has tag 0 during the early gameplay transition; the other 18 have tag +1. Excluding transition data more strictly is appropriate for future comparisons. Even this broad gameplay filter removes the alleged `out` shift entirely.

The gap detector is not a complete 12.5 ms or 25 ms deadline monitor: it normally has a 40 ms minimum detection threshold. The table quantifies its detected interruptions, not every missed application deadline or headset display miss. Quantiles use rounded printed values and index `floor(p*(n-1)+0.5)`.

The lower new gap rate is descriptive, not a controlled improvement. Build, runtime period, scene, session length and requested SSW setting differ. The submission tail remains the observed location of the recurring large gameplay interruptions. The draft's 6.7-32.0 ms EndFrame maxima describe selected summary windows; **99.3 ms** is printed in an actual gameplay gap at 8359921.

**2. Some of the throughput table joins the wrong windows.**

Pair each CPU summary with the GPU summary at the same timestamp:

| Timestamp | Pair interval / reported tick rate | D3D9 GPU span per tick | Two desktop Presents | Interpretation |
|---|---|---:|---:|---|
| 8391593 | 12.5 ms / 80.0 per second | 8.6 ms | See log | A clean native-rate window exists |
| 8397593 | 17.5 ms / 57.0 per second | 12.4 ms | 4.8 ms | Slow gameplay with nearly no pacing wait |
| 8400593 | 13.6 ms / 73.7 per second | 9.7 ms | 3.7 ms | Below native rate despite a GPU span below 12.5 ms |
| 8403593 | 12.5 ms / 66.7 tagged ticks per second | 8.6 ms | 2.7 ms | Mixed gameplay/menu window; do not use as steady 66.7 FPS evidence |

The draft assigns 12.4 ms to the 73.7/s window and 9.7 ms to the 66.7/s window. The actual matching values are 9.7 and 8.6 ms respectively. Its 66.7/s row occurs after the MENU transition and contains **40 untagged Presents**. The same window's separate rate line reports about 80 EndFrame calls/s, including the changed submission population.

This explains the apparently contradictory 12.5 ms interval and 66.7/s rate: `perf.cpp` computes tick duration from the sum of per-tag mean durations, but tick rate from the tag -1 count divided by the entire window. When stereo stops partway through a window, those are different populations. There are other genuine 60-70/s gameplay windows, so the user's performance observation remains supported; this particular row is unsuitable evidence.

Across 28 summary windows ending from 8319593 through 8400593, GPU span ranges **7.7-12.4 ms**, not 8.6-12.4 ms. Three windows contain untagged Presents; retain only verified stable stereo windows for a formal comparison. The remaining zero-untagged windows still include rates from 57 to 80 and that same GPU-span range. These are window averages, not a claim that every individual frame fits either budget.

**3. The CPU/driver path has not been cleared.**

In the 57/s window at 8397593:

- Render-thread `R`: 6.1 + 5.5 = **11.6 ms**.
- Original D3D9 Present calls: 1.4 + 3.4 = **4.8 ms**.
- These disjoint elapsed-time buckets total approximately **16.4 ms** of a 17.5 ms pair interval.
- The recorded pacing wait is only about **0.1 ms**.
- The D3D9 GPU span is **12.4 ms**.

That is strong evidence to inspect the engine/render-thread and driver timeline. It is not proof of 16.4 ms of CPU execution: draw calls and Presents can wait on GPU progress or scheduling. The `R` bucket also does not include every game-thread activity or distinguish running from descheduled time. A sampled CPU trace plus GPU queue events is needed to separate execution, driver blocking and thread starvation.

The draft therefore cannot label this solely a pixel/work deficit or exclude CPU/driver limitations. All its quoted 8.6-12.4 ms GPU spans are already below 12.5 ms. The missing time could include unmeasured D3D11 work, pipeline serialization, driver scheduling, CPU work or missed timing opportunities. These measurements also cannot be added as independent CPU-plus-GPU costs because parts overlap.

Keep the observed native-rate deficit and the long submission stalls as separate outcomes. They may share a dependency, but fixing one does not prove the other is fixed. Likewise, a pose inconsistency need not consume much processing time, yet cadence can expose it or amplify its visual effect.

**4. What the 12.50 ms period proves, and what it does not.**

The new instrument reports 12.50 ms once and no changes in its monitored main frame path. That is a useful improvement: rapid changes in this reported value no longer hide behind three-second summaries. Combined with sustained submission rates near 80, it shows the application is not consistently limited to 40 submissions/s in this run.

It does not independently verify whether VD is synthesizing frames on the headset, whether the forced setting was honored, or the actual displayed source-frame cadence. `predictedDisplayPeriod` is a prediction field, not an SSW-active flag or an end-to-end receipt counter. OpenXR explicitly permits it to differ from physical refresh. [Khronos `XrFrameState`](https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrFrameState.html).

In public VDXR `frame.cpp`, the period-doubling decision depends on a successful `ovr_GetPerfStats` call, a nonempty stats result, and `AswIsActive`. Otherwise its local active flag remains false. Thus the public implementation itself motivates checking both actual SSW state and stats availability. This does not prove the installed version is hitting that fallback, or establish the semantics of VD's forced setting. [VDXR frame implementation](https://raw.githubusercontent.com/mbucchia/VirtualDesktop-OpenXR/main/virtualdesktop-openxr/frame.cpp).

Use **12.5 ms as the native-rate comparison budget for this observed loop**. Retain **25 ms as the intended 40-FPS operating point**. The discrepancy between requested half-rate SSW and observed near-native application cadence is an investigation target, not a reason to silently replace the user's goal with mandatory native 80.

The instrument's explanation that doubling necessarily means spacewarp engaged still overclaims. Print the period change as a fact and report SSW state separately when available. Also record periods only from successful waits: the new block currently precedes the `XR_FAILED(r)` check, so an error path could log stale/invalid frame-state contents. Reset its counters per session and distinguish wait success, returned display time, period, shouldRender and session state.

**5. The proposed weapon mechanism confuses object pose with view pose.**

The projection layer describes the camera/view used to render its pixels. The world and a moving weapon can share that view while representing different object simulation or tracking times. A newer controller sample does not automatically require a different projection-layer pose. The requirement is that the controller transform be expressed through the correct tracking-to-world mapping and the view actually used to render those pixels. [Khronos projection-view pose contract](https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrCompositionLayerProjectionView.html).

In a simplified rigid-transform model, let H_r be the rendering head pose, H_d the display head pose, and C_c a controller pose. Ignore translation/depth limitations, projection and fixed coordinate conversions for this illustration. A controller rendered as H_r^-1 C_c and reprojected by H_d^-1 H_r produces H_d^-1 C_c. A newer C_c is not inherently double-corrected.

A real mismatch can arise if the hand is instead made head-relative with a different H_s and inserted into the H_r-rendered view without the required conversion. The residual then contains H_r H_s^-1. **The candidate is a mismatch between the head/view transforms used by the mapping and rendering, not simply the age of the controller sample.** The source makes this worth measuring but has not established its magnitude or which image it affects.

The claim that the world is N-2 while weapons are later than N is also not established by a locate call site. The controller sample is consumed during drawing and travels with that image through delayed capture and submission. Its age at final EndFrame cannot be inferred from the newest sample available at EndFrame. Moreover, world/head views are also located for a predicted time. In this run `ahead=0`, so the head/view and controller locate calls target the same predictedDisplayTime during a given locate cycle. Adding a separate prediction interval only to the weapon side double-counts an unproven difference.

**6. The active hand/weapon path already has pose plumbing.**

The relevant source path is:

| Stage | Observed implementation | Remaining question |
|---|---|---|
| Runtime locate | `openxr_runtime.cpp` locates head/views at `locateTime`; `input_sync` locates grip/aim at predictedDisplayTime; ahead=0 here | Which locate cycle eventually belongs to each rendered eye? |
| Pose consumption | `present_tick.cpp:DvrConsumePoses` copies head into `g_devPose[0]`, grip poses into slots 3/4 | No original XrTime/locate identity accompanies these copies through the entire path |
| Hand publication | `mesh_split.cpp:MpDriveTick` computes head-relative position and orientation, publishes `MpPoseSnap` under a lock | Internal consistency is not proof of alignment with the render view |
| Draw mapping | `MpReadDrawCtx` copies that snapshot and derives camera basis B from shader constants; placement combines B with the head-relative data | Validate the actual view basis and its time/identity against the head used to normalize the controller |
| Weapon placement | `WaPublishCommon` publishes the hand correction with Present, eye and pose generation; `WaCommonFor` checks Present/eye | Audit the active matching/fallback path, component-snapshot freshness, and each weapon pass |
| Capture and submission | Shared capture delivers the previous Present's slot; submission selects the view history using Lag | Join the rendered pixels and their original source samples through this delay |

`MpDriveTick` explicitly computes the orientation equivalent of `F * transpose(R_head) * R_controller`; draw placement adds the shader-derived camera basis. This is more specific evidence than grepping for the submission system's variable names.

The new log's `ms/palette/lane` lines report pose publication and consumption on the **same thread**, with no backwards snapshot-generation observations. That weakens a simple torn-publication explanation on this path, but does not prove the sampled pose matches the engine view. A snapshot can be internally coherent and consistently associated with the wrong frame.

Two citations in the draft need correction:

- `mesh_split.cpp:2109` stores `g_hmdYaw` for a flicker diagnostic. It does not place the weapon.
- The `fp_mesh.cpp:923` read is in another placement path. Its existence does not prove that path owns the observed weapon. The palette/attachment path above is active in the log; establish the executed writer before proposing a fix to a different path.

There is also generation metadata already in the weapon path (`WaCommon.poseGen`). The hand snapshot generation increments at publication, while the OpenXR locate generation increments at a different point and frequency. Comparing their integer values without an explicit mapping would create another misleading instrument. Carry the actual locate ID and XrTime through the snapshot.

The candidate temporal mapping error remains plausible. The stronger statements that weapons did not receive the fix, that Lag=2 doubled their error, and that this is almost certainly the cause should be withdrawn until pixel/view/sample identity is measured.

**7. Replace the proposed falsification tests.**

**Do not begin with global `vrpace lag 0/1/2`.** That changes the view metadata for both world and weapons. It can make the scene move differently relative to the headset without changing the weapon's mapping relative to the world. Improvement would not isolate the weapon path; no improvement would not eliminate all temporal mapping errors. Keep the world-confirmed Lag=2 as the baseline. A later short global-lag test can be supplementary evidence, with automatic restoration and separately assessed world/weapon behavior, not the decisive test.

**Full frame rate does not mean zero reprojection.** Timewarp adjusts a completed frame using a later head estimate even when the app supplies frames regularly. A timing error may become less variable at a locked rate without vanishing. Conversely, ordinary moving-object sampling and synthesis artifacts can get worse when frames are reused. [Meta compositor explanation](https://developers.meta.com/horizon/essentials/the-compositor/).

**Head-still/controller-moving judder does not kill all weapon timing hypotheses.** It weakens a purely head-rotation-driven mapping explanation under controlled conditions. It remains compatible with stale controller samples, unequal eye timing, fallback attachment, animation or frame-synthesis artifacts. SSW estimates motion from rendered images; it does not receive a perfect independent weapon trajectory. [Qualcomm's description of SSW inputs](https://www.qualcomm.com/developer/blog/2022/09/virtual-boost-vr-rendering-performance-synchronous-space-warp).

Use a small motion matrix, one weapon and one repeatable scene, at unchanged resolution and Lag=2:

| Controlled motion | Main discriminator |
|---|---|
| Controller supported stationary in room; rotate head | Head/view-coordinate mismatch, versus visible world-wide reprojection error |
| Controller supported stationary; translate head | Near-object parallax, positional mapping or depth limitations |
| Head still; move/rotate controller | Controller sampling, synthesis or attachment/animation behavior |
| Both still; move through scene with stick | Game-camera/body mapping, viewmodel animation, scene load and cadence |

Supporting the controller matters: holding it by hand while turning the head can introduce real controller motion. Judge the hand, attached weapon, a fixed world landmark and, where useful, a controller-space reference separately. A runtime laser is a different pipeline and timestamp, so it is not automatically ground truth for the engine-rendered weapon.

Capture source-eye and final-headset evidence only for short marked intervals; recording itself can perturb performance. A desktop mirror alone cannot establish headset reprojection behavior or show both eyes. If the error is already present in source-eye pixels, inspect mapping, stereo sampling and attachment. If it appears only after composition, investigate view metadata and synthesis. That split is more informative than changing the global lag and relying on an overall smoothness impression.

**8. The instrument needed before selecting a weapon fix.**

Add a bounded diagnostic record associated with an actual rendered view, not just the Present counter:

- Locate ID, target XrTime and validity for the head, grip and aim samples.
- Hand snapshot publication ID and original locate ID; publication and consumption timestamps.
- The head transform used for controller normalization, tracking-to-world/body transform and actual per-eye rendering view/projection.
- Hand and weapon draw IDs, eye, target, active placement/fallback path, source component snapshot generation and age.
- Source render ID, capture serial and delivered slot, XR eye/image index, selected layer pose and final submission ID.
- Cadence, SSW observation and frame-interval markers for the same sequence.

Compute the mismatch between **the head used in hand normalization and the rendering view's corresponding head**, after converting them to the same coordinate space. Separately compare the rendering view and submitted view metadata. Do not compare the controller orientation to the head orientation and call their normal physical difference a timing error. Report positional residual as well as angle; a near weapon can show a translation or stereo discrepancy even with negligible angular error.

Record whether both eyes of the same stereo pair use the same source hand/head snapshot. Sequential rendering, per-Present publication and delayed capture make this an important question, but the current source inspection does not prove that the eyes disagree. A future pair snapshot must retain eye-specific view offsets while sharing the intended temporal source.

First exercise the instrument in the simulator with known independent head and controller trajectories, including a stationary room-space controller during head rotation, an intentionally offset sample, and interrupted cadence. Existing transform self-tests cover algebra; they do not establish correct association across the render/capture/submission pipeline. The simulator can validate that association and that the instrument detects an injected mismatch, but cannot reproduce real VD SSW or transport behavior.

**Preferred conditional fix:** preserve a coherent source pose and its identity through the draw and capture pipeline, and use the correct transform into the rendering view. If retaining newer controller samples, explicitly map them through that view. Do not read `viewsPrev2` at draw time merely because Lag=2 is selected later; that can apply the pipeline delay twice. Do not unnecessarily add two frames of controller latency to compensate for an unmeasured problem.

Submitting weapons separately is not automatically correct: transparency, depth/occlusion, eye views, timing and synchronization still need solving. A flat quad cannot generally reproduce a 3D weapon. An extra projection layer is a large design change and should not be the first fix.

A mathematically derived rebase between sampled and rendering coordinate frames can be valid. An arbitrary counter-rotation tuned until the headset looks better is not sufficient evidence. Keep correction of the actual rendering transform separate from speculative cancellation of an assumed compositor warp.

**9. Revised performance test order.**

1. **Establish the effective operating mode at 80 Hz and original resolution.** Record the requested SSW setting and actual in-headset SSW indication, runtime/Streamer/headset-app versions, successful wait timing, returned display timestamps, period and actual successful pair-submission cadence. Preserve Lag=2, shared capture and the disabled A/B sweep. If the app still runs near 80 while forced SSW is reported active, treat that as a pacing/telemetry discrepancy to explain, not as proof of inadequate hardware.
2. **Run the controlled motion cases above while collecting basic timing.** This requires no global world-pose change. It decides which weapon-specific instrument or fix is worth building.
3. **Profile one representative slow window and one long submission stall.** In the former, distinguish CPU execution from driver/GPU waits and game-thread dependencies. In the latter, identify the wait inside EndFrame or its downstream dependency. Add delayed D3D11 GPU timings and correlate them with the shared-resource fences, desktop Presents and source-frame IDs. Public VDXR contains previous-submission and graphics-synchronization paths; Wi-Fi is not the only possible cause of a runtime-call stall. [VDXR D3D11 synchronization](https://raw.githubusercontent.com/mbucchia/VirtualDesktop-OpenXR/main/virtualdesktop-openxr/d3d11_native.cpp).
4. **If effective half-rate pacing is missing, compare an explicit 40-pair/s gate at unchanged resolution.** The existing `vrpace sync 40` is a possible diagnostic, not an SSW switch or guaranteed phase lock. Use it only with verified mode and measured pair cadence, compare against gate off, and restore the baseline. A gate that reduces CPU/GPU pressure but makes cadence worse is not a fix. Do not use a per-Present cap that accidentally halves the intended stereo rate again.
5. **Select one optimization from the trace.** Candidate controls include mirror copies, one desktop Present per pair, sampled per-draw hook cost, or bridge scheduling. Keep actual eye captures, resource ownership, pose association, reset and menu behavior correct. Measure total pair tails and latency; a shorter Present call alone is not a win.
6. **Use a fixed-mode resolution sweep only to test measured pixel dependence.** Pair matching scene windows, preserve stream settings, confirm honored sizes, and return to baseline. A flat D3D9 span does not clear unmeasured resolution-sensitive copies or runtime work. A smaller image helping still does not identify encoding or Wi-Fi.

The CPU cost of attachment hooks and the correctness of attachment can interact, but they need separate tests. A visual hide switch does not necessarily stop per-draw matching or state getters. A true work-bypass experiment must prove which work was skipped and must not be misread as evidence about normal hand motion.

**10. Where 72 Hz belongs.**

It is a useful sensitivity test, not a diagnosis or resolution of the user's 80 Hz goal. Native 72 gives **13.889 ms** per application frame, only **1.389 ms** more than native 80. Forced half-rate SSW at 72 instead targets **36 FPS and 27.778 ms** per application frame. Verify the actual mode before quoting either budget.

Changing refresh also changes runtime scheduling, prediction intervals, potential synthesis cadence and work per second. At native 72 it reduces frames per second by 10%; it does not leave the total workload unchanged. In the slow measured window, the 17.5 ms pair interval exceeds both native budgets. Consequently 72 Hz is not guaranteed to hold simply because the partial GPU span is below 13.9 ms.

If 72 is smooth, it shows sensitivity to refresh/cadence/load. It does not uniquely quantify a GPU deficit or prove the weapon transform correct. If 72 still hitches, it does not eliminate GPU work, since averages omit tails and the D3D11 path is unmeasured. Run this after establishing the effective SSW mode, and return to 80 for the actual acceptance target.

**11. Keep the useful instrumentation repairs, finish the remaining ones.**

The previous p99-baseline error, p50-only verdict, duplicate latency-3 treatment, severe-stall accounting and default-on sweep have been addressed in the new source. That is progress; this report does not treat those old implementation defects as still open.

However, the new pair sampler still counts every second Present instead of consuming verified pair identities. Its assumption of `untagged 0` is not valid throughout this log. Some gameplay windows contain untagged frames, and a transition or aborted pair can invalidate parity even while the gameplay flag remains true. Use actual complete-pair IDs and classify unmatched/held/transition frames separately. The automatic sweep is off in this run, so that sampler did not cause the reported behavior.

The original GPU-span subtraction defect, unmeasured D3D11 execution, immediate pending-query zeroes, causal rate labels and pre-EndFrame "pair close" timestamp remain. Fix their semantics before using them to rule out a mechanism. CPU tracing should distinguish running, blocked and ready-but-unscheduled states; nonblocking GPU query collection must retain frame identity and unresolved samples. Logs should state observations and uncertainty rather than encode a preferred causal story.

**12. Answers to the six review questions and acceptance.**

1. **Does the exact pose-generation mechanism hold?** Not as written. A different controller/object time does not automatically imply a different camera view. Head-relative mapping with a mismatched view is plausible, but the N-2 versus later-than-N claim and added prediction interval are unmeasured.
2. **Is global lag 0/1/2 decisive?** No. It changes the whole projection image's metadata. Keep Lag=2 and isolate the hand-source/view association first.
3. **Which fix shape?** Prefer explicit source-frame association and a correct coordinate transform. Rebase only from measured transforms; delay, counter-rotation and separate layers are not justified solely by the current grep evidence.
4. **Did `out` hitches increase?** Not in the compared gameplay populations. All 18 new-run `out` gaps precede gameplay. Sustained render-thread/driver elapsed time is nevertheless worth profiling, based on the slow-window split rather than that false hitch mixture.
5. **Is 72 Hz legitimate?** Yes as a mode-verified sensitivity test. No as proof of GPU causality or completion of the 80 Hz requirement.
6. **Does a full reported period settle forced SSW?** No. The period and near-80 application cadence are real observations; effective SSW and backend statistics remain unverified. Native and half-rate budgets must remain explicitly conditional on the intended and observed mode.

Acceptance for performance is repeated gameplay at **2750x2850, 80 Hz, verified half-rate SSW and stable complete-pair delivery near 25 ms**, with long interruptions substantially reduced and any remaining ones attributed. If native 80 is pursued separately, report its 12.5 ms target separately. Track pair interval distributions and fixed-duration gaps, keep transition data separate, and correlate felt interruptions with successful submission and display telemetry where available.

Acceptance for weapons is stable hand/weapon/world alignment during the motion matrix, correct left/right association, no added tracking delay beyond the chosen design, and no regression to the world pose fix. Report results at both steady and disturbed cadence. A smoother low-refresh run or a lower average frame time does not by itself satisfy that requirement.

All new render levers remain default OFF, live-switchable and fail-soft. Build one behavioral change at a time on a known snapshot; validate correctness in the simulator before headset tests. Runtime calls retain their existing thread ownership. No gameplay test, setting change, build, installation or implementation was performed for this review.


<a id="record-5"></a>

## Record 5: PERF_PLAN_2.md

Historical source migrated on 2026-09-15. Current verdicts above take precedence.

# Plan 2 - the 80 Hz deficit and the weapon judder (VR-67/VR-68 draft, 2026-09-09)

**Review update:** [PERF_REVIEW_2.md](C:/dev/Dishonored-VR/docs/dishonored/PERF_REVIEW_2.md)
revises this draft against the original log and active pose/weapon paths.
Its conclusions and test order supersede the proposals below; this draft is
retained as review input.

## ANSWERED (2026-09-09): the weapon judder is FIXED, `[Hands] PoseLag=2`

The lag finder put the rendered camera at **lag 2** - mean `|dB - dHead|`
0.119 deg against 1.190 at lag 0 and 2.406 at lag 1, over 4085 moving frames,
with a symmetric V around the minimum. The hand was normalised against the
freshest head. A reversing A/B/A/B in the headset confirmed the fix completely.

ENGINE_NOTES carries the mechanism, the numbers and the fix. The performance
half of this document (section 4) is still OPEN and is VR-67.

---

## RESULT: the head/view candidate is DEAD as measured, and the target is sharper (2026-09-09)

The motion matrix came back with the world judder's signature - head rotation
shows it, a stick turn does not, spacewarp off - so the instrument was built and
run. Over 100 reported seconds of head turning:

* **Generation gap 0 on EVERY frame.** `0 of N frames had a generation gap`, in
  all 100 lines. The hand normalisation and the camera write always consumed the
  same locate.
* **87 of 100 windows had a worst residual under 0.1 deg.** Thirteen had more,
  peaking at 9.6 deg, and those do not track head speed.

**The candidate as stated is dead, and the instrument was nearly circular
anyway.** Both values it compared - the head stamped at the pose consume and the
head the camera write snapshotted - derive from `g_hmdYaw` around the same
consume. It could only ever have caught a script-lane/present-lane split, and
there is none. That is a real negative for that split and nothing more.

### What it sharpens

`MpDriveTick` normalises the hand against the **fresh** head. The draw plants it
using `B`, the camera basis read from **the render's own shader constants** -
which is the view the engine ACTUALLY rendered, and the whole reason `Lag=2`
exists is that those pixels are about two generations behind the fresh pose.

**So the mismatch is fresh-versus-rendered, and this instrument compared
fresh-versus-fresh.** The pair that matters is `B` against the head sample, and
it was never measured.

This also predicts the symptom's history better than the retracted version did:
`Lag=2` did not enlarge the hand's error at all - the hand error is `B` versus
fresh regardless of what the layer is tagged with. It **revealed** it, by taking
the world's judder away. Which matches the report that the weapon judder "has
probably been there".

### Instrument defect found in its own first run

The head-speed column read over 300 deg/s in 51 of 100 lines, which no neck
does. A max over a window is destroyed by one tiny interval; intervals under
2 ms are now ignored instead of divided by. **The speed figures in the run above
are not usable** - the residual and generation columns are.

### The next measurement, not yet built

Compare `B` against the history of the head sample and find which past
generation it corresponds to - the same technique that settled VR-65. It must
compare **deltas between frames**, not absolute orientations, because `B` is in
the game's space and the head sample is in XR space, and differencing those
directly is what produced two already-retracted numbers in this project.

---

## RETRACTIONS (2026-09-09, after review 2)

**Four claims below are withdrawn. All four were checked against the log and the
source and all four are wrong.** They are left in place, struck through here
rather than deleted, because the record of a wrong analysis is worth more than a
tidy document.

1. **1.3, "the hitch distribution MOVED", is a COUNTING ERROR.** Gameplay ran
   8314609-8403078. **All 18 `out` gaps occurred BEFORE gameplay** (8291375 to
   8313359 - startup and the menu). Inside gameplay, all 19 detected gaps sat in
   the submission tail, exactly as in the previous run. There is no shift. The
   whole "weakens a single-cause story" paragraph is void.
2. **1.1 joins the wrong windows.** The 12.4 ms GPU span belongs to a
   **57.0/s** window (8397593) that the draft never mentions; the 73.7/s window
   is 9.7 ms. And the 66.7/s row is post-MENU with **untagged 40** - unusable as
   evidence of anything.
   **The window I missed is the interesting one**: 57.0/s, 17.5 ms tick, GPU
   12.4 ms, render-thread R 11.6 ms + desktop Presents 4.8 ms = ~16.4 ms elapsed
   against a **0.1 ms pacing wait**. That is where the deficit lives, and it is
   not obviously a pixel problem.
3. **3.1's mechanism names the wrong variable.** A newer CONTROLLER sample is not
   double-corrected: a controller drawn as `H_r^-1 C_c` and reprojected by
   `H_d^-1 H_r` yields `H_d^-1 C_c` for any age of `C_c`. The residual only
   appears if the hand is made head-relative using a head `H_s` that differs from
   the head `H_r` the view was rendered with, leaving `H_r H_s^-1`.
   **The error is a head/view transform mismatch, not controller sample age**, and
   the "world at N-2, weapon later than N, plus the prediction interval" arithmetic
   is unmeasured and withdrawn.
4. **3.3.1, the lag sweep, is not a discriminator and would have misled.**
   Changing `Lag` changes the tag for the WHOLE image equally, so it moves the
   world and the weapon together and cannot move the weapon-versus-world residual.
   Running it and reading the result would have been the VR-65 mistake again.

Two citations were also wrong: `mesh_split.cpp:2109` is a flicker diagnostic
(`g_mpFlickYaw0 = g_hmdYaw`), not weapon placement; and "a grep returns nothing"
searched the submission layer's variable names. **The hands DO have pose
plumbing** - `MpPoseSnap.gen`, `WaCommon.poseGen`, `g_waPoseGenDiff`.

### What survives, in the corrected form

`MpDriveTick` reads head and hands from the SAME consume (`g_devPose[0]` and
`g_devPose[3+h]`, filled by `DvrConsumePoses`), so the snapshot is internally
coherent. It publishes head-relative data. **The draw then completes it with a
camera basis `B` derived from the shader constants of the actual render.**

So the open question is whether `B`'s head and `g_devPose[0]` are the same
sample - and `STATUS.md` already records that they may not be:

> the camera record still does not guarantee it holds the sample the camera was
> calculated from - both writers calculate from the loose globals and consume the
> coherent sample afterwards

**That is `H_r H_s^-1`, already written down as an open defect before the weapon
judder was reported.** It is a candidate, not a finding, and the motion matrix
decides whether it is worth instrumenting.

---

**Status: draft, for review before anything is built.** Section 1 is measured on
the run described. Sections 3 and 4 are hypotheses with falsification tests.
Nothing here is implemented.

This supersedes `PERF_PLAN.md` where they disagree, and accepts every correction
in `PERF_REVIEW.md`. Two of that review's corrections are load-bearing below.

---

## 0. What changed since the review

The review's central correction was that **the budget is 25 ms, not 12.5 ms**,
because the previous run was at 40 fps with spacewarp. That was correct for that
run. **It is not correct for this one, and the difference is the point.**

The new period instrument - added because the 3 s summary prints only the period
its window ENDED on - reports, once, and never again:

```
xr: runtime period is 12.50 ms (80.0 Hz) at the first located frame
    - predictedDisplayPeriod, NOT the panel refresh.
      Every later change is logged; if none is, the period held all run.
```

**Zero period changes in the whole run.** The runtime asked for **80 fps** for
its entire duration, and the previous run's 25 ms period is absent.

The tester set Virtual Desktop to force SSW enabled for this run. The runtime
still advertised the full 12.50 ms period throughout. **Open question for the
reviewer**: whether VD's forced SSW does not double `predictedDisplayPeriod`, or
whether SSW was not actually engaged. Either way, what the application was ASKED
for is not in doubt, and it is 80 fps.

So the budget question is not "which of 25 or 12.5 is right" - it is that **the
runtime chose differently between two runs and neither of us knew.** The
instrument that would have caught it did not exist until this build.

---

## 1. What is measured, this run

Render size **2750x2850** per eye (restored), `[Pace] Lag=2`, capture mode
**shared** (not deferred - the earlier draft said deferred and was wrong), the
A/B sweep **off** so nothing varied underneath.

### 1.1 The application is missing the deadline it was given

| Window | tick | achieved | budget | GPU per tick | verdict line |
|---|---:|---:|---:|---:|---|
| A | 13.6 ms | 73.7/s | 12.50 ms | 12.4 ms | `UNDER-SUBMITTING 0.92x` |
| B | 12.5 ms | 66.7/s | 12.50 ms | 9.7 ms | |
| C | 12.5 ms | - | 12.50 ms | 8.6 ms | `MATCHED 1.00x` |

The tester independently reports **60-70 fps in the main hub**, which matches the
66.7-73.7/s in the log.

GPU per tick ranges **8.6-12.4 ms against a 12.5 ms budget**. In the busy window
it is at 99 % of budget. This is the throughput deficit, and at 80 Hz it is real
- unlike the previous run, where 14-17 ms against a 25 ms budget was comfortable
and the earlier draft wrongly called it a problem.

### 1.2 The CPU split is NOT the constraint at this rate

```
P1[-1] in 3.2 (pre 0.0  begin 1.6 [wait 1.6]  tick 0.1  method 0.1
               [cap 0.0]  end 0.0 [acq 0.0 xrCopy 0.0 endFrame 0.0]  present 1.4)
       + out 4.1 (idle 0.4  R 3.8)
P2[+1] in 3.0 ... 
```

The two desktop `Present` calls now cost about **2.5 ms per pair**, not the
4.8 ms measured at the half-rate run. The pacing wait has collapsed to 1.6-2.7 ms
because the app is no longer parked. **The Present cost did not disappear; it is
simply smaller here and the budget is half as large.**

### 1.3 The hitch distribution MOVED

50 gaps this run. Location tally, against the previous run:

| Where the gap sat | this run | previous run |
|---|---:|---:|
| `present-tail` (xrEndFrame) | **26 (52 %)** | 71 (81 %) |
| `out` (render thread after Present returned) | **18 (36 %)** | 13 (15 %) |
| other | 6 | 4 |

`endFrame mean=0.29-0.53 ms, max 6.7-32.0 ms` - much smaller than the previous
run's 40-108 ms tail.

**This is a different mixture, and it weakens a single-cause story.** The
`out` share more than doubled. `out` is the render thread executing the frame's
commands after Present returned, which is our own and the engine's work, not the
runtime's. Whatever is hitching is not only in the submission path.

Both runs differed in period AND in scene AND in SSW setting, so this comparison
is suggestive, not controlled.

---

## 2. Two separate problems, and they should not be conflated

1. **A throughput deficit at 80 Hz.** GPU 8.6-12.4 ms against 12.5 ms; achieved
   66-74 fps against 80. Fixing this is a pixel/work question.
2. **The weapons (and possibly the hands) now judder** in a way the tester
   describes as exactly what the WORLD used to do before the VR-65 pose fix,
   and it shows whenever the frame rate is not hard-locked.

Problem 2 is new information, it is specific, and there is a mechanism that
predicts it exactly. It is treated first below because it is cheap to test and
because it may be a regression the pose fix introduced.

---

## 3. Hypothesis: the hands and weapons are placed at a pose the frame is not tagged with

### 3.1 The mechanism

The VR-65 fix made the submitted layer carry the pose the WORLD was rendered
from - two locate generations back (`[Pace] Lag=2`), because this engine has a
separate render thread and a delayed capture stage. The compositor reprojects the
whole submitted image from that tagged pose to the actual head pose at display
time.

**The hands and weapons do not participate in that scheme at all.**

- `openxr_input.cpp:537-540` locates every hand and aim space at
  **`predictedDisplayTime`** - the freshest, forward-predicted pose.
- A grep for `poseLag`, `viewsContent`, `viewsPrev2` or `pose_lag` across
  `src/game/dishonored/` and `openxr_input.cpp` returns **nothing**. The
  generation machinery lives entirely inside the runtime layer's submission path.
- The hand and weapon placement reads the loose `g_hmd*` globals
  (`fp_mesh.cpp:923`, `mesh_split.cpp:2109`), which is the same class of defect
  `STATUS.md` already records as open for the camera: *"both writers calculate
  from the loose globals and consume the coherent sample afterwards."*

So within one submitted frame the world is drawn at generation **N-2** and the
weapon is drawn at **predicted display time**, which is later than N. The
compositor then reprojects everything by `(display - N-2)`. That correction is
right for the world and **over-corrects the weapon by two generations plus the
prediction interval**. The weapon swims against the world by exactly the amount
the head moved in that span.

### 3.2 Why it appeared now

The lag default moved 1 -> 2 in the VR-65 fix. **That doubled the mismatch for
anything not drawn from the tagged generation, at the same moment it removed the
mismatch for the world.** A fault that was half as large and hidden underneath a
much worse world judder would become the most visible thing in the frame.

### 3.3 Predictions that would kill it

This is the section that matters; a hypothesis that cannot lose is not one.

1. **`vrpace lag 1` and `lag 0` should reduce the weapon judder monotonically
   while making the world judder worse.** If the weapon judder is unchanged
   across lag 0/1/2, the mechanism is wrong and this whole section is dead.
2. **The error should scale with HEAD angular velocity, not hand velocity** -
   because the reprojection is driven by head motion. If holding the head still
   and waving the controller reproduces it, the cause is elsewhere.
3. **It should be near-zero when the app makes every deadline** (no reprojection
   correction to apply) and grow as frames are missed - which is exactly the
   "when not at a hard locked framerate" the tester reports, and is the
   strongest existing evidence for it.
4. **The measured angle** between the pose the hands were placed from and the
   pose the frame was tagged with should be non-zero and should track head speed.
   Nothing currently measures this.

### 3.4 The shape of a fix, if it survives

Not chosen here; listed so the reviewer can rank them.

- **(a) Place the hands from the tagged generation.** Give the hand placement the
  same generation the layer will be tagged with, so the whole image is coherent
  and one reprojection is right for all of it. Most correct; needs the hand
  transform to be computed from a stored generation rather than the loose globals.
- **(b) Submit the hands as a separate layer** with their own pose. Correct by
  construction, but a quad or a second projection layer is a large change and
  the hands are drawn by the engine into the same image.
- **(c) Counter-rotate the hand placement** by the tagged-vs-fresh delta. Cheap,
  and a cancellation term is exactly what the VR-31 neck-cancel lever already
  does elsewhere in this project. Risk: it is a correction applied on top of a
  correction, and it will be wrong the moment the reprojection is not applied.

All three must ship default OFF with a live A/B, per project rule.

### 3.5 What this does NOT explain

The throughput deficit. A pose mismatch costs nothing. Problem 1 stands on its
own and needs its own work.

---

## 4. The throughput deficit

The honest position: **at 80 Hz the GPU is at 69-99 % of budget before anything
else is counted, and the app delivers 66-74 of 80.** Options, with what each
would prove:

| Option | What it would establish | What kills it |
|---|---|---|
| Resolution sweep at FIXED 80 Hz and fixed SSW setting, three points | Whether the GPU span is pixel-linear, and where the 12.5 ms line sits | A flat span - then the cost is CPU, submission or resolution-independent passes |
| One desktop `Present` per pair | Whether ~2.5 ms/pair is removable from the critical path | The wait moving into `StretchRect`, a fence, or the next draw |
| Run at 72 Hz instead of 80 | Whether a budget of 13.9 ms is enough, which the GPU numbers say it should be | Still missing slots - then the deficit is not the GPU span |
| Desktop mirror copies off | Whether the full-size snapshot/restore pair costs a measurable amount | No change in pair p99 |

**The 72 Hz option is listed first on cost.** It is a headset setting, needs no
code, and the measured 8.6-12.4 ms GPU span fits a 13.9 ms budget everywhere the
12.5 ms budget does not. If 72 Hz is smooth and 80 Hz is not, the deficit is
quantified without building anything.

---

## 5. Instrument work this needs

Carried forward from the review, still not done:

- **The mismatch angle for the hands** (3.3.4). Nothing measures it. This is the
  instrument that would make section 3 evidence rather than argument, and it is
  the same shape as the VR-65 orientation-difference instrument that settled the
  world judder - which suggests reusing that code rather than writing new.
- **D3D11 GPU timings** for the bridge blit and the XR copy. Still absent, so the
  bridge is unmeasured and cannot be ranked.
- **The GPU span still subtracts capture DMA from an interval that does not
  contain it**, and `idle(d3d9)` is a gap between markers.
- **Gap lines print `gpu pending span 0.0`** because resolution happens later;
  the zero is not evidence of zero GPU cost.

---

## 6. Questions for the reviewer

1. **Does the pose-generation mechanism in 3.1 hold?** Specifically: is
   reprojection applied to the whole submitted image such that content drawn from
   a different pose is over-corrected, and does the two-generation gap plus
   forward prediction produce an error of the magnitude a player would call
   judder at 66-74 fps?
2. **Is prediction 3.3.1 the right discriminator**, or does changing lag confound
   the world and the weapon so badly that the tester cannot separate them?
3. **Which fix shape** in 3.4 - and is (c) acceptable at all, or is a correction
   stacked on a correction a trap?
4. **The `out` share doubled** (1.3). Is that a real signal worth chasing, or an
   artefact of comparing two runs that differed in period, scene and SSW setting?
5. **Is 72 Hz a legitimate measurement** or just a smaller ask? It changes the
   budget without changing the work, which is unusually clean, but it does not
   identify a stage.
6. **The forced-SSW question in section 0**: is a runtime that reports the full
   period while reprojecting a thing that happens, and if so does any of the
   period-based reasoning survive?

---

## 7. Constraints

- Every new render lever default OFF with a live A/B toggle; fail soft.
- `src/core/` changes must default to pre-existing behaviour for the other games.
- The present thread owns every runtime call.
- The tester runs the game; diagnostics must ship enabled in the installed ini
  and be readable from `dishonored_vr.log`.
- One behavioural change per build; build on a snapshot confirmed good.


<a id="record-6"></a>

## Record 6: PERF_REVIEW.md

Historical source migrated on 2026-09-15. Current verdicts above take precedence.

# VR-67 performance review - 2026-09-09

**The working target is 2750x2850 per eye, an 80 Hz headset, and a stable 40 application FPS with SSW. The existing evidence does not establish that rendering this resolution is too expensive for that target. It establishes recurring submission stalls whose cause is still unresolved.**

This report revises the conclusions and test order in `PERF_PLAN.md`. It is an analysis and proposed investigation, not an implemented performance fix. The proposed 2365x2451 test has not been run. No game settings, runtime settings, DLLs, or rendering code were changed for this review.

**Evidence and provenance.** Reviewed the supplied draft, repository source at `d2b2d686`, and the original `dishonored_vr.log` next to the installed game. The log identifies build `vr33-hands-working-72-g0f8f9774-dirty`, built September 9 at 10:27:16, and VirtualDesktopXR 1.0.10. The dirty build banner does not prove that every source byte matches the subsequently committed branch. Its A/B results do match the draft's table. A future measurement build should have a clean commit identifier and DLL hash.

The original log was preserved locally at `build/perf-review-2026-09-09/vr67-original.log`, outside the tracked report. Its SHA-256 is `0CBC69DFBD4E700C8523A6B30B6D78363332F1CC54FA0CC66DB174FE30432052`. Log timestamps below are the bracketed millisecond values, not wall-clock times. Public VDXR source was also inspected as evidence of possible mechanisms; it has not been matched to the installed binary.

**1. The budget question is 25 ms, not 12.5 ms.**

At 80 Hz with half-rate SSW, the application supplies a stereo frame every 25 ms. Each application frame requires both eyes; this mod normally makes two D3D9 Present calls per pair. Thus 80 D3D9 presents/s can mean 40 application frames/s. SSW generates intermediate frames on the headset, reducing the required PC frame rate. [Qualcomm's description of Virtual Desktop SSW](https://www.qualcomm.com/developer/blog/2022/09/virtual-boost-vr-rendering-performance-synchronous-space-warp).

The draft quotes 14.0-17.3 ms of D3D9 GPU span per pair. That is 56-69% of a 25 ms interval, leaving a nominal 7.7-11.0 ms outside those measured spans. Re-reading 64 GPU summary lines from timestamp 6170281 up to the end of gameplay gives 12.7-17.3 ms, with an unweighted mean of window means of 14.37 ms. This is evidence that the measured rendering usually fits the half-rate budget. It is not a complete GPU utilization or per-frame tail measurement.

The approximately 12.5 ms CPU figure is elapsed time on the instrumented thread after subtracting one pacing wait. It includes driver calls and other waits; it is not measured CPU execution time across the game. CPU and GPU work can overlap, or serialize through dependencies. Neither adding 12.5 + 14.0 ms nor treating their maxima as independent throughput guarantees describes the pipeline without a timeline.

The 12.5 ms native-80 budget addresses a different objective. Exceeding it can explain an inability to sustain native 80 FPS, but cannot explain why the existing half-rate workload suffers recurring 50-100 ms interruptions. If SSW was forced on, its engagement says nothing about the application's maximum native rate.

Stable 40 FPS is a reasonable engineering target supported by these averages. An absolute guarantee for every frame, including loads, cannot be inferred from hardware class or average timing. The investigation should explain the gameplay interruptions before proposing a permanent resolution reduction.

**2. The original log narrows the problem more clearly than the draft.**

The draft's 88 detected gaps include startup, menus, and loading. Gameplay starts at 6166906 and changes to MENU at 6361046, giving 194.140 seconds of gameplay. Filtering gap lines to that interval produces:

| Measurement | Gameplay-only result |
|---|---:|
| Detected Present-entry gaps | 69 |
| Largest recorded phase | Submission tail for all 69 |
| Delivered eye tag at those gaps | +1 for all 69 |
| Gap duration, minimum / p50 / p90 / maximum | 40 / 62 / 99 / 115 ms |
| Recorded `xrEndFrame` duration, minimum / p50 / maximum | 32.1 / 53.6 / 108.8 ms |
| Average detected-gap frequency | One per 2.81 seconds |
| Gaps with reported period 25 / 12.5 ms | 57 / 12 |

These quantiles use the rounded values printed in the log, with nearest-rank-index selection `floor(p*(n-1)+0.5)`. They are not recomputed from unlogged high-resolution frame samples.

Gaps cluster: grouping consecutive gap messages separated by at most one second gives 36 clusters, about one per 5.39 seconds of gameplay. This grouping is illustrative, not a count of perceived hitches, but explains why event count and a report of interruptions every several seconds need not disagree. It does not establish a periodic oscillator.

The tail label covers all of `on_present_end`, so the label alone is insufficient. In these events the separately printed `endFrame` measurement corroborates that the actual API call accounts for most of the interruption. Conversely, these are CPU-side Present-entry gaps, not measured headset dropped-frame counts. The detector has a 40 ms minimum relative threshold and can miss smaller 25 ms deadline violations.

There are no further logged OpenXR session-state changes after initial FOCUSED during this gameplay interval. That weakens a focus-loss explanation for these particular events, without eliminating internal runtime scheduling changes.

**3. The capture path and the runtime period were misidentified.**

The log explicitly says `mode sync -> shared`, two shared surfaces at 2750x2850, and delivery from the previous Present's slot. The installed ini also says `[Capture] Mode=shared`. This is shared capture with delayed delivery, not the distinct `Mode=deferred` CPU-readback implementation. The distinction changes which dependencies should be investigated.

The `hmd` field is populated from `XrFrameState.predictedDisplayPeriod`. OpenXR allows that period to differ from the physical display refresh cycle. Therefore `hmd 25.00 ms = 40.0 Hz` does not mean the headset panel is running at 40 Hz, and ratios calculated from it are not physical display-slot occupancy measurements. [Khronos `XrFrameState` reference](https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrFrameState.html).

The log contains gameplay gaps at both 25.00 ms and 12.50 ms. For example, timestamps 6180031 and 6180140 report 12.50 ms while the surrounding summary windows report 25.00 ms. A three-second window's last period can hide transitions inside that window. Twelve gap observations at 12.5 ms do not establish how much time the application spent at that period or whether changing period caused the gaps.

Public VDXR `frame.cpp` updates its predicted frame duration from reprojection-active statistics, doubling the native duration when active. Its `xrEndFrame` also has mutex acquisition, a possible wait for previous asynchronous submission, graphics synchronization, layer processing, and downstream submission. These mechanisms make a mode or scheduling transition plausible and directly refute treating the call as exclusively encoder/network work. This is source-level evidence of possible paths, not identification of the installed binary's blocking instruction. [VDXR frame implementation](https://raw.githubusercontent.com/mbucchia/VirtualDesktop-OpenXR/main/virtualdesktop-openxr/frame.cpp).

**4. Where a wait is observed does not identify who caused it.**

The relevant chain in this mod is D3D9 rendering, a shared-surface copy, a D3D11 consumer blit, an XR swapchain copy and release, then frame submission. There are resource dependencies across two graphics APIs and the runtime. A short CPU call usually records command submission, not completion of the GPU work it requested. In particular, `CopyResource` is asynchronous. [Microsoft `CopyResource` documentation](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-copyresource).

Public VDXR `d3d11_native.cpp` contains a D3D11 serialization path that signals an application fence and waits on the submission device. Depending on configuration, it uses a GPU wait or an event-completion CPU wait. A GPU wait need not itself block the CPU, but can delay downstream progress. The CPU-wait path can directly charge earlier application GPU work to `xrEndFrame`. Verify the installed runtime's active path before selecting a fix. [VDXR D3D11 implementation](https://raw.githubusercontent.com/mbucchia/VirtualDesktop-OpenXR/main/virtualdesktop-openxr/d3d11_native.cpp).

Consequently:

- `acq 0.0` weakens an acquire/wait blocking theory for the measured frames, but does not prove the subsequent copy finished or the downstream queue was empty.
- `xrCopy 0.0` cannot clear D3D11 GPU work, resource synchronization, or driver scheduling.
- A low mean submission rate cannot rule out back-pressure. A blocked consumer can cause precisely that low producer rate. A window average also hides bursts and queue history.
- A largely unchanged median cannot rule out periodic encoder or resource stalls. The draft's own bad-window `endFrame` mean rises from 0.07 to 2.58 ms, about 37-fold. It is false that the mean did not rise.
- Fewer application submissions do not establish unfilled physical headset slots: previously submitted content and generated frames can still be displayed.

The evidence supports a recurring submission-path stall. It does not yet separate application dependencies, VDXR scheduling, the graphics driver, encoding, or transport.

**5. The A/B results are useful negative evidence, not complete falsifications.**

The seven raw segment summaries are reproducible from the original log. Neither FrameId removal nor latency settings 1/2/3 demonstrated a repeatable performance fix. Successful latency setter/getter results establish that the value was accepted, not that this queue was the limiting queue.

The A/B start line records baseline maximum frame latency **3**. The later latency-3 segment therefore repeats that baseline setting; it is not a third distinct alternative. Its variation is additional evidence of noise or changing workload.

Several defects weaken the draft's stronger conclusion:

- `perf_ab.cpp:AbSummary` computes its baseline spread from p50 only. The `NO CHANGE` verdict checks p50 only, despite the report extending that verdict to p99 and hitch counts.
- Its `dP99` compares p99 against the average baseline p50. Thus the printed roughly +100% to +140% figures are not changes relative to baseline p99. The raw p99 values themselves remain usable.
- Baseline p99 rises from 23.76 to 26.98 ms, a 13.6% increase. The baseline over-twice-median fraction rises from 1.68% to 4.99%, nearly threefold. A 3% p50 spread is not a 3% tail noise floor.
- Each retained segment is approximately 18 seconds. The relevant rare-event count is small, regardless of the approximately 1,400 Present intervals collected. Sequential conditions also confound setting changes with scene changes, drift, and clustering.
- Two times a segment's own median is a moving threshold. Under alternating eyes, healthy short and long intervals can differ substantially. It is not a fixed 25 ms stereo-frame deadline.
- Doubling the median of alternating Present intervals does not generally produce the median of stereo-pair intervals. Pair neighboring events by identity before computing pair quantiles.
- `ab_tick` silently excludes intervals of 5 seconds or more. Future reporting should retain a separately classified severe-stall count rather than silently discard them.

As a cross-check, filtering the separate gap log by each segment's start plus the two-second discard gives the following counts. These are gap-message timestamps, not an exact reassignment of every frame crossing a boundary, and are separate from the A/B harness's moving-threshold counts:

| Condition in run order | Detected gaps in retained interval |
|---|---:|
| Baseline | 4 |
| FrameId off | 5 |
| Baseline again | 9 |
| Maximum frame latency 1 | 4 |
| Maximum frame latency 2 | 2 |
| Maximum frame latency 3 | 8 |
| Baseline last | 9 |

This does not prove latency 2 helps. It demonstrates why a p50-only verdict cannot establish equivalence of rare-event behavior. Deprioritize these two levers as immediate fixes, but retain them as controlled checks once the outcome measure is corrected.

**6. Instrument defects to fix before ranking optimizations.**

| Source and observation | Required correction |
|---|---|
| `perf.cpp:gpu_resolve` measures BeginScene to Present-hook entry, while capture is issued later | Label the spans literally; stop subtracting capture DMA from an earlier non-containing span. In shared mode the capture marker can represent a GPU blit, not a CPU readback. |
| `gpu_resolve` attempts resolution five Presents later and retires a pending set as late | Preserve pending samples long enough to resolve safely; report unresolved samples as missing, not zero. Clean windows do not establish all-tail coverage. |
| Gap lines currently print `gpu pending span 0.0` | Join the eventual GPU measurement to the same pair/gap identity. The immediate zero is not evidence of zero GPU cost. |
| No D3D11 GPU timing for the blit and XR copy | Add bounded timestamp/query rings and delayed nonblocking collection. Record bridge submission, completion, and backlog separately. |
| `capture::last_grab` times selected calls | Include surrounding instrumentation and the consumer path. Rounded 0.0 ms is neither zero GPU work nor total capture cost. |
| `stereo.cpp` infers causality from submit/period ratios | Print measured rates and periods; remove claims that rates prove or disprove throttling, dropped display frames, or the cause of ghosting. |
| Pair phase is sampled around `openxr_runtime.cpp:3786`, before right-eye acquisition/copy and `xrEndFrame` | Label it as arrival at the completing-eye path. Add pre-EndFrame and post-EndFrame timestamps. The existing roughly -60 ms phase is not proof of timely completed submission. |
| Acquire/wait and copy are timed, but release has no separate timer/result census at the eye-copy site | Time and record acquire, wait, copy, release and EndFrame separately, with image index, pair ID, and result. |

An average 1.10 application periods per pair does not demonstrate one periodically repeated miss every ten frames. Irregular multi-frame stalls can produce the same average. The earlier ghosting correlation was also superseded by the later reversing pose-lag test documented in `STATUS.md`; it is not proof that a cadence limiter fixes this complaint.

The current `vrpace sync` is a QPC-based interval gate, anchored to local time, before the runtime wait. It is not phase-locked to a measured headset display boundary. It can add a second pacing wait, and it can catch up after smaller stalls. Evaluate it as a rate limiter, with the total pair outcome as the criterion. For the half-rate target, the relevant explicit rate is 40, not 80.

**7. Ranked causes and the observation that would distinguish each.**

| Priority | Candidate | Why it remains plausible | Discriminating evidence |
|---|---|---|---|
| First | SSW state changes or runtime submission scheduling | The runtime period is not constant; stalls occur on pair submission | Hold SSW enabled at original resolution, record every period transition and runtime submission wait. A stable-state run that still hitches weakens mode-switching as the complete explanation. |
| First | D3D9/D3D11/runtime synchronization or queue starvation | Actual shared resources, fences and flushes connect APIs; D3D11 work is unmeasured | Correlate a hitch with producer completion, consumer fence state, runtime wait stacks and GPU queues. Short CPU copy calls do not answer this. |
| Next | Desktop presentation or mirror copies causing contention | Two original Presents cost about 4.8 ms/pair, with additional full-size mirror copies | Remove one cost at a time; require reduced total pair tails and runtime waits, not a wait moving to another call. |
| Next | Runtime compositor/encoder saturation, driver scheduling or resource residency | A modern GPU can still have a serialized queue, intermittent resource contention or delayed execution | Trace the stalled interval across game and runtime GPU contexts, video encode, scheduling and memory residency. Long GPU execution supports a work problem; queued but unscheduled work supports contention. |
| Next | Network or headset receive/decode interruption with upstream effects | Plausible end-to-end streaming failure, but no correlated network evidence was supplied | Keep render size, refresh, SSW mode and codec fixed; compare bitrate conditions with VD telemetry and timestamps. A network reading alone is correlation unless it precedes the failure. |
| Secondary | Periodic diagnostics, engine work, or hooks | Readbacks, log flushing, per-draw routing and engine events exist | Profile actual execution and tag periodic events. Short call-site bypasses must demonstrably skip the machinery, not merely hide rendered hands. |

Shared capture currently uses two producer surfaces and separate producer/consumer fences. `read_done` explicitly ends a D3D11 query and flushes. Its 10 ms fence-wait loops can eventually proceed on timeout; the observed zero lifetime timeout counts argue against that particular failure in this run. Preserve resource ownership while investigating. Removing fences or adding an unconditional GPU-idle wait would change the pipeline and could trade incorrect images or extra latency for an apparently better column.

The full-size bbox diagnostic is still present, separately from FrameId. The source default is 30 seconds; a startup message still describes it as every three seconds. Read the effective setting and actual sample events rather than that stale string. The scale of its observed window cost and lack of gameplay capture-dominant gaps make it a weaker explanation for every hitch. It remains a valid separate diagnostic control.

The current log confirms matching runtime-requested and D3D11 adapter LUIDs, and the shared surface reports the same D3D9 LUID. That weakens accidental cross-adapter capture. Adapter enumeration alone, and the log's 32-bit VRAM display, should not be used to diagnose physical VRAM pressure; measure residency and memory budgets if the trace points there.

**8. Revised test order.**

**First establish the original-resolution, fixed-SSW baseline.** Preserve this log and record build/DLL identity, VDXR and VD versions, actual render and swapchain sizes, physical refresh, SSW setting and observed active state, codec, bitrate, buffering, and any other FPS limiter. Use the same loaded scene and a repeatable route. Set 80 Hz and force SSW enabled for the primary comparison; Auto permits a changing treatment. Disable the automatic performance-lever sweep for these runs so it does not silently vary FrameId and queue latency underneath another experiment.

The installed ini currently arms 2365x2451. Restore 2750x2850 through the existing resolution mechanism before this baseline, then verify the launch ask, actual backbuffer, capture size and XR swapchains. This report has not performed that restoration or launched the game. Keep the already confirmed `Lag=2` pose behavior.

Use roughly three minutes of steady gameplay per condition, with a baseline return and preferably reversed condition order on a repeat. Count only stable gameplay after startup. Longer runs become necessary if events become rare; do not infer a small improvement from two versus four events in 18 seconds. Record felt hitches and VD Game/Encoding/Networking/Decoding readings with approximate aligned times. Avoid adding a new continuous recording workload to only one condition.

**Next repair measurement and capture one explanatory trace.** Prioritize pair distributions, per-frame period transitions, separate API timers and D3D11 completion over an optimization patch. A small ring should retain the lead-up and aftermath of a hitch without per-frame synchronous disk writes. Use an event-based Windows graphics/CPU trace when available to determine whether the stalled thread was running, waiting on a fence/mutex, or ready but not scheduled. GPUView exposes GPU command submission and execution, context switches, synchronization and paging events. Pair it with available VDXR tracing and VD telemetry. [Microsoft GPUView documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/using-gpuview).

If matching runtime tracing is available, inspect previous-submission wait, D3D11 synchronization, downstream EndFrame and reprojection-active events. A trace should identify a dependency, not simply report aggregate GPU utilization. Instrumentation must be asynchronous where possible, bounded, and checked for changing the hitch rate itself.

**Then choose one controlled branch of investigation.** The order can follow the trace; these comparisons are not a request to run everything blindly:

| Comparison | Hold fixed | What a result establishes |
|---|---|---|
| SSW forced enabled versus original setting, with baseline return | 2750x2850, 80 Hz, stream settings, scene and build | Whether stabilizing the runtime mode changes hitches. Continued hitches under a verified stable period reject mode switching as the sole cause. |
| Lower bitrate versus baseline | Original render size, 80 Hz, forced SSW, codec and buffering | Sensitivity to streaming pressure. Reduced hitches alone do not uniquely distinguish network from encoder effects; correlate telemetry. |
| 2365x2451 versus 2750x2850, returning to baseline | 80 Hz, forced SSW, codec/bitrate/quality preset and scene | Whether image-size-dependent rendering, copies or runtime work affects the tails. Log actual encoded size if available. No improvement does not identify Wi-Fi. |
| Pair-rate gate off versus `vrpace sync 40` | Original resolution, forced SSW and all other limiters | Whether restricting bursts helps. Check added wait, phase, latency and catch-up behavior; stop treating it as a proven phase-lock. |
| Desktop pin copies on versus off, using a verified separate live toggle | Headset render/capture/submission and two Presents | Whether mirror-copy traffic matters. This proposed A/B is separate from suppressing a Present; verify headset eye identities. |
| One desktop Present per completed pair versus two | Capture of both eyes, flush/ownership rules, scene and settings | Whether presentation overhead is removable on the critical path. Count rendered eyes, captures, desktop Presents and successful pair submissions independently. |

Bitrate is often the cheaper initial streaming-pressure discriminator, but no setting alone settles encoder versus link. VD's overlay offers useful coarse localization; sparse readings can miss brief events or aggregate queues. A wired or alternate-runtime comparison changes several components and is a later cross-check, not a clean Wi-Fi-only proof.

If these comparisons remain ambiguous, a developer-only synthetic submission test is useful: update a full-size stereo pattern at a fixed 40 FPS through the same D3D11/runtime path, with sequence numbers and matched stream settings. Hitches there weaken the engine-specific explanation; a smooth synthetic run does not clear the bridge under real load or encoding of a complex scene. The existing simulator is valuable for correctness, but cannot reproduce the actual VDXR/encoder/network path and cannot validate this performance claim.

The source currently couples XR swapchain dimensions to the frame texture. Therefore the ordinary resolution toggle changes engine rendering, shared surfaces, copies and submitted images together. To isolate engine pixel cost from submitted-image cost would require a separate, verified scaling experiment with fixed XR output dimensions. That is additional implementation, not something the pending test already provides.

**Native 80 FPS is a later, separate experiment.** The pending proposal changes resolution and SSW state together. Even if it improves, that does not identify the responsible stage. With the same aspect ratio, 2365x2451 has 73.96% of the original pixel count. At twice the application rate, successful native 80 would process about 1.48 times as many eye pixels per second as the original 40 FPS condition. A smaller individual image is not automatically a lighter stream or GPU workload across time.

The proposed 11.8 ms forecast uses a fixed floor from another session. Applied to the quoted 14.0-17.3 ms range with that same 5.6 ms floor, it predicts approximately 11.8-14.3 ms, already straddling the native budget before unmeasured work. Treat it as a provisional model. Two controlled points estimate a slope; a third tests whether it is approximately linear. A flat D3D9 span only weakens the pixel dependence of that measured span, not every possible resolution-sensitive stage.

**Optimize only the stage the evidence identifies.** One-Present-per-pair remains worthwhile, but 4.8 ms of blocking Present time is not 4.8 ms of guaranteed savings. A skipped Present may move a queue wait into a copy, a fence or the next frame. Verify backbuffer lifetime, capture/eye identity, pose-generation alignment, reset, menu transitions and device loss. Run simulator correctness checks before headset acceptance. Sharing eye-independent engine work and reducing hook overhead come after attribution establishes their value.

**9. Acceptance and answers to the draft's review questions.**

Acceptance is the original image resolution at an 80 Hz headset with confirmed SSW, substantially reduced gameplay hitch frequency, and complete stereo frames supplied near the 25 ms schedule. Report pair interval p50/p95/p99/p99.9, gaps over fixed 40/50/75/100 ms thresholds, consecutive missed opportunities, successful submission rate, API stalls, and the total duration in stable mode. Compare against the repeated baseline and attach the actual frame population. Runtime/compositor telemetry, where available, is needed to call something a displayed-frame miss. Keep loading and focus transitions separately visible.

A useful first validation goal is no recurring 50+ ms gameplay interruptions across repeated three-minute trials. That is an engineering target, not a claim already demonstrated. For perspective, even zero independent events in 180 seconds only gives an approximate 95% upper event-rate bound of 3/180 per second under a Poisson assumption; clustered real hitches require greater caution. Zero events in one short run is not an always-40 guarantee.

The five review questions can now be answered directly:

1. **Encoder/link attribution:** not established. GPU/resource dependencies and runtime submission scheduling are concrete alternatives. Fast acquire and copy API calls do not close them.
2. **Resolution fit:** provisional and insufficient to guarantee native 80. Use controlled points at fixed SSW state; add a third point if fitting a performance model.
3. **Present suppression versus measurement:** fix the essential measurement and obtain a trace first. Then optimize Present if it is on the critical path. A cheap verified mirror-copy A/B can precede a larger Present change.
4. **Cheaper encoder/link separation:** fixed-resolution bitrate A/B plus VD telemetry and runtime/graphics traces. No resolution-only binary outcome identifies the network.
5. **Candidates considered closed:** reopen capture GPU/synchronization, queue behavior beyond the tested D3D9Ex setting, tail measurement validity and image/pose identity beyond nonzero tags. FrameId and queue-latency changes have not shown a reliable benefit; they are lower-priority candidates, not universally falsified mechanisms.

All proposed rendering levers retain the project's default-off, live-A/B and fail-soft requirements. Instrumentation may be enabled for the identified test build. Preserve the current runtime thread ownership and resource synchronization. This report provides no basis for declaring the hardware incapable of the requested 40 FPS, declaring the mod cleared, or blaming Wi-Fi.


<a id="record-7"></a>

## Record 7: PERF_PLAN.md

Historical source migrated on 2026-09-15. Current verdicts above take precedence.

# Performance plan - DRAFT for review (VR-67, 2026-09-09)

**Review update:** [PERF_REVIEW.md](C:/dev/Dishonored-VR/docs/dishonored/PERF_REVIEW.md)
revises this draft using the original log and source. Its conclusions and test
order supersede the proposals below; this draft is retained as the review input.
The reduced-resolution test has not been run.

**Status: draft, for external review before any of it is built.** Everything in
section 1 is measured on one identified build; everything in section 4 is a
proposal with an explicit falsification test. Nothing in sections 3-5 has been
implemented.

---

## 0. The complaint, stated precisely

Playing at a headset refresh of **80 Hz with synchronous spacewarp active**, so
the application target is **40 fps**. The tester reports **lag spikes every 3-10
seconds**, consistent regardless of which direction they look. The spikes are
not correlated with head motion, which distinguishes them from the VR-65 judder
(fixed: the submitted pose was one render generation too new).

The measured rate is **one detected frame gap every 2.5 seconds** over a 221 s
run, which is the same phenomenon at the same order of magnitude.

---

## 1. What is measured, on this build

Build: the VR-67 branch (`claude/vr-67-perf-distribution-ab`), Release, installed.
Hardware: RTX 4070 Ti SUPER, single GPU. Runtime: Virtual Desktop (VDXR) over
Wi-Fi to a Quest 3. Render size **2750x2850 per eye** (7.84 MP), `VirtualMode=1`,
capture mode deferred, `[Pace] Lag=2`.

### 1.1 The frame time distribution, from the self-switching A/B

Seven segments of 20 s each, gameplay only, first 2 s of each discarded.
Interval between consecutive **presents** (two presents per stereo pair).

| Segment | n | p50 | p95 | p99 | max | over 2x median |
|---|---:|---:|---:|---:|---:|---:|
| baseline | 1427 | 11.42 | 21.33 | 23.76 | 67.35 | 1.68 % |
| frameid readback OFF | 1419 | 11.60 | 21.13 | 24.88 | 103.46 | 1.41 % |
| baseline again | 1408 | 11.33 | 21.90 | 25.19 | 101.79 | 3.27 % |
| max frame latency 1 | 1420 | 11.15 | 21.64 | 24.48 | 115.17 | 2.61 % |
| max frame latency 2 | 1432 | 11.26 | 21.30 | 22.76 | 86.61 | 1.54 % |
| max frame latency 3 | 1406 | 11.08 | 22.07 | 25.25 | 102.68 | 4.55 % |
| baseline last | 1403 | 11.08 | 22.15 | 26.98 | 104.97 | 4.99 % |

**Noise floor: the three baseline segments span p50 11.08-11.42 ms, so anything
inside 3.0 % is not a result.** Every alternative landed inside it.

### 1.2 The two cheapest candidates are FALSIFIED

- **The `FrameId` render-target readback** (`GetRenderTargetData`, 64x64, one
  pair in eight) - **no change**, +2.9 % on p50, inside the noise floor, and it
  did not move p99 or the over-2x-median count either.
- **`IDirect3DDevice9Ex::SetMaximumFrameLatency`**, never previously called in
  this codebase, swept 1 / 2 / 3 - **no change** at any value. The call
  succeeded and the device read the value back, so this is a real negative, not
  a refused lever.

Both were plausible from source inspection. Neither survived measurement.

### 1.3 The per-tick budget split

```
perf: tick 25.0 ms (40.0/s, 80 presents/s) [hmd 25.00 ms = 40.0 Hz, budget 25.00 ms/tick]
PACE-BOUND (wait 6.3 ms/present)
= P1[-1] n=120 in 14.6 (pre 0.0  begin 12.6 [wait 12.5]  tick 0.1  method 0.1
                        [cap 0.0: lock 0.0 copy 0.0 up 0.0 blit 0.0]
                        end 0.0 [acq 0.0 xrCopy 0.0 endFrame 0.0]  present 1.8)
        + out 4.3 (idle 0.4  R 3.9)
| P2[+1] n=120 in  3.2 (pre 0.0  begin 0.0 [wait 0.0]  tick 0.1  method 0.1
                        [cap 0.0: ...]
                        end 0.1 [acq 0.0 xrCopy 0.0 endFrame 0.1]  present 3.0)
        + out 2.9 (idle 0.0  R 2.9)
| untagged 0 | marker=BeginScene(1.0/present in 240 of 240)
```

Reading it:

| Item | Cost per tick | Note |
|---|---:|---|
| `xrWaitFrame` idle (P1 only) | **12.5 ms** | the app is deliberately parked; this is the pacing budget, not work |
| The two original D3D9 `Present` calls | **4.8 ms** (1.8 + 3.0) | 19 % of a 25 ms tick, **38 % of a 12.5 ms tick** |
| Render-thread `R` (executing commands) | **6.8 ms** (3.9 + 2.9) | |
| Capture (lock/copy/upload/blit) | **0.0 ms** | deferred capture is free at this size |
| `xrEndFrame`, typical | **0.1 ms** | see 1.5 for the tail |
| Real CPU per tick, excluding the wait | **~12.5 ms** | |

### 1.4 The GPU

```
perf: gpu/present span=7.0 ms (3d 6.9 + readback dma 0.1) idle(d3d9)=2.3 ms
    | per tick span=14.0 dma=0.3 idle=4.5 | 240 resolved, 0 late, 0 disjoint, 0 unmarked
```

**GPU: 14.0-17.3 ms per tick** (both eyes), against a 25 ms budget at the current
40 fps target. Zero late or disjoint query sets, so the population is clean.

**The number that matters for the next step: at a native 80 Hz target the budget
is 12.5 ms, and the GPU already needs 14.0-17.3 ms.** The CPU needs ~12.5 ms.
**Both budgets are at or over the 80 Hz line, independently.** That is why
spacewarp is on: the app cannot hold 80 native at this render size.

### 1.5 Where the spikes actually are

88 detected gaps over 221 s. Location tally:

| Where the gap sat | count | share |
|---|---:|---:|
| **`present-tail` (xrEndFrame)** | **71** | **81 %** |
| `out` (render thread after Present returned) | 13 | 15 % |
| other | 4 | 5 % |

Gap sizes: n=88, min 40 ms, **p50 68 ms**, p90 201 ms, max 2165 ms (one load).

A representative gap:

```
perf: frame gap 70ms (5.5x the mean present interval 12.7 ms)
  | sat in: present-tail (xrEndFrame) of #14592 tag +1
    (61.3 ms of in 64.6 / out 5.0; wait 0.0 lock 0.0 endFrame 61.3)
    = 2.8 display slots at 25.00 ms
```

And the same instrument across a good window versus a bad one:

```
good: endFrame mean=0.07 ms  max=0.2 ms  over 120 submits  MATCHED 1.00x
bad:  endFrame mean=2.58 ms  max=74.4 ms over 109 submits  UNDER-SUBMITTING 0.91x
```

**`xrEndFrame` normally costs 0.07 ms and occasionally costs 40-75 ms. That is a
thousand-fold discrete stall, not a load curve.** A rising encode cost would
raise the mean; this does not.

The instrument's own verdict on whether this is back-pressure:

> `UNDER-SUBMITTING 0.91x: display slots are going UNFILLED, so xrEndFrame is
> not throttling a surplus - the present-tail stalls are genuine hitches and
> their cause is upstream of the headset's cadence`

So the runtime is not merely pacing us. Something inside `xrEndFrame` blocks.

### 1.6 One more standing signal, unresolved

```
UNEVEN CADENCE: 1.10 display slots per frame (not a whole number) with sd 12.80 ms
- one frame in 10 is held an extra slot (a 3.9 Hz beat)
```

Measured previously: 1.05-1.11 ghosts, 1.00-1.02 does not. `vrpace sync <hz>`
exists to lock the pair schedule and **has never been A/B'd**.

---

## 2. What the measurements rule out

- **Not the capture.** 0.0 ms per tick in the deferred path.
- **Not the frame-identity readback.** Falsified by A/B.
- **Not the D3D9Ex present queue depth.** Falsified by A/B at 1, 2 and 3.
- **Not GPU query error.** 240 of 240 resolved, none late or disjoint.
- **Not eye pairing.** `untagged 0` across the run.
- **Not head motion.** The tester reports the spikes are direction-independent,
  and the VR-65 pose fix is in and confirmed.

## 3. What the measurements do NOT establish

Stated so the plan is not read as more certain than it is.

- **Whether the 81 % of stalls inside `xrEndFrame` are caused by anything we
  control.** The call body is entirely Virtual Desktop's: encode, then hand off
  to the Wi-Fi link. Our submitted image size is an input to it, and that IS
  ours. Nothing has been measured that separates "the link hitched" from "we
  gave the encoder more than it can absorb".
- **The GPU accounting is known to be partly wrong.** The reported span is
  `BeginScene` to Present-hook entry, and the capture copy is issued *after*
  that entry, yet the report subtracts capture DMA from the earlier span and
  labels the remainder "3d". `idle(d3d9)` is a gap between markers, not proof
  the GPU is idle. There are **no D3D11 GPU timings at all**, so the
  D3D9 -> shared texture -> D3D11 blit -> XR copy bridge is unmeasured.
- **How much of the second eye's render is duplicated engine work.** The stereo
  method re-enters the viewport renderer; the game-thread call is short but the
  render-thread and GPU consequences are not attributed per eye.
- **Whether the `R` time (6.8 ms/tick) contains our per-draw hook cost.** The
  indexed-draw hook routes through weapon attachment and mesh identification
  with device-state getters and COM refcounting on every draw. Never profiled.

## 4. Proposed order of work

Each step names the measurement that decides it, and what result would kill it.

### Step 1 (armed, not yet run): a resolution step-down to test the 80 Hz line

**Hypothesis.** The app is on spacewarp because both budgets exceed 12.5 ms at
7.84 MP/eye. Cutting to **5.80 MP/eye (2365x2451, the same 55:57 aspect)** should
put the GPU near 11.8 ms and let the app hold **native 80 Hz with no spacewarp**.

Fit used: GPU per-tick span 14.0 ms at 7.84 MP/eye on a ~5.6 ms fixed floor
implies ~1.07 ms per eye-megapixel; at 5.80 MP that is 5.6 + 6.2 = 11.8 ms.

**Decides:** whether the frame budget is pixel-bound at all.
**Kills it:** GPU per-tick span barely moves -> the cost is CPU, submission or
resolution-independent passes, and every pixel-reduction idea is dead.
**This is a diagnostic, not a proposed permanent downgrade.**

**It also tests the encoder theory for free**: if the `xrEndFrame` stall rate
falls with the submitted image size, the stalls are encode-bandwidth-bound and
therefore partly ours. If the stall rate is unchanged, they are the link and no
render-side change will touch them.

### Step 2: one desktop `Present` per completed stereo pair

**The largest single measured cost we control: 4.8 ms/tick, 38 % of a 12.5 ms
budget.** The game presents to the desktop after each eye; only one of those two
desktop frames is ever seen.

Constraints that make this non-trivial, and any of them broken makes the result
worthless:

- The capture may be driven by the same hook that performs the Present. Suppressing
  the engine's Present request must not suppress the capture.
- The D3D9/D3D11 bridge's flushes and synchronisation must be preserved.
- Reset, device-loss, menu and transition paths must be preserved.
- Which eye reaches the desktop must be chosen from the identity of the current
  backbuffer, not the delivered capture tag, which can describe the previous
  present.
- Desktop presents must be counted separately from rendered eyes and submitted
  pairs, or a "faster" build that drops an eye will read as a win.

**Kills it:** total tick time unchanged because the wait moved into `StretchRect`,
a fence, or the next draw. **A lower number in the Present column alone is not a
result.**

If this lands, the snapshot/restore pair of full-resolution desktop-eye copies
(`desktop_eye.cpp`) may become unnecessary - but only after confirming the
intended eye reaches the desktop and headset capture is still correct.

### Step 3: fix the GPU accounting, then add D3D11 timings

Not an optimisation. Steps 4-5 cannot be ranked without it.

- Label literal measured intervals; stop subtracting an interval from a span that
  does not contain it.
- Add D3D11 GPU timestamps around the bridge blit and the XR copy.
- Report distributions for complete stereo pairs, not window means. (The A/B
  harness from VR-67 already does this for present intervals and can be reused.)

### Step 4: `vrpace sync <hz>` against the 1.10 slots-per-frame beat

Already implemented, never A/B'd, and the cadence beat is a standing measured
signal with a known threshold (1.00-1.02 good, 1.05-1.11 bad). Cheapest
remaining lever. Add it as a segment to the VR-67 plan rather than as a separate
run.

### Step 5: profile the per-draw and per-`ProcessEvent` hook cost

Only after step 3 makes the CPU split trustworthy. Candidates named by source
inspection, none of them measured:

- The indexed-draw hook: weapon routing runs before the palette gate; buffer
  identification retrieves stream and index-buffer state; census work retrieves
  buffers, declaration and shader, then searches recorded draws. A dense scene
  puts every unrelated draw through this.
- `ProcessEvent`: camera validation and offset application per intercepted event,
  with readability checks that reach `VirtualQuery` rather than comparing pointers.
- The shader-constant hook copies small uploads into a diagnostic ring even when
  no capture is armed.

**Trap to avoid:** hiding the hands visually does not prove their draw-processing
machinery stopped running. Any toggle used for this A/B must be shown to bypass
the expensive work, not just the drawing.

### Step 6: share eye-independent engine work, or shorten the bridge

Largest intervention, lowest confidence, and it must follow step 3. Do not remove
synchronisation as a speed fix: incorrect resource ownership makes an apparently
faster build display overwritten or unfinished eye images.

---

## 5. Questions for the reviewer

1. **Is the encoder/link conclusion in 1.5 justified**, or is there a way the
   stall inside `xrEndFrame` could be caused by something we do on a previous
   frame - a fence, a resource still in use, a swapchain image not released?
   Note `acq 0.0` and `xrCopy 0.0`, so the acquire is not blocking.
2. **Does the step-1 fit hold?** 1.07 ms per eye-megapixel on a 5.6 ms floor is
   derived from a single operating point plus a floor measured in an earlier
   session. Two points would be better; is one run at 5.80 MP enough to trust
   the extrapolation, or should the sweep include a third size?
3. **Is step 2 ranked correctly ahead of step 3?** The 4.8 ms is real and
   measured, but the accounting that would tell us whether removing it helps is
   the thing step 3 fixes. There is a case for reversing them.
4. **Is there a cheaper way to separate "encoder" from "link"** than changing
   the submitted image size - something readable from the runtime, or from
   Virtual Desktop's own overlay, that would settle 1.5 without a code change?
5. **Anything in section 2 that should not be considered closed.** The two
   falsifications are single-run, 20 s each, on one scene.

---

## 6. Project constraints the plan must respect

- Every new render lever ships **default OFF with a live A/B toggle**; a method
  that refuses leaves the previous one running.
- `git diff main...HEAD -- src/core/` staying non-empty means the change is in
  scope for other games too; core changes must default to pre-existing behaviour.
- Retired experiments go to `src/legacy/`, never deleted silently.
- The present thread owns every runtime call. Never take a reference to an
  engine D3D object inside a detour.
- A verified write is not an honoured one; acceptance is a measured downstream
  effect.
- An instrument that cannot fail its own hypothesis is not evidence.
- The tester runs the game; diagnostics must ship default-on in the installed ini
  and be readable from `dishonored_vr.log` without a debugger.


<a id="record-8"></a>

## Record 8: DESKTOP_PRESENT_PERFORMANCE.md

Historical source from `codex/vr-115-performance-rollout:docs/dishonored/DESKTOP_PRESENT_PERFORMANCE.md`. Not a current test instruction.

# Desktop presentation performance candidate

2026-09-14. [VR-115](https://linear.app/vr-stereo-hub/issue/VR-115), branch
`codex/vr-115-desktop-present`, based on accepted VR-Main `255d1c91`.
First implementation from the [VR-113 performance audit](https://github.com/VR-Stereo-Hub/Dishonored-VR/blob/codex/vr-113-performance-audit/docs/dishonored/PERFORMANCE_AUDIT.md).
Not installed or headset-tested. No game launch or merge.

## Purpose and scope

The accepted pipeline performs two original desktop Presents and a left snapshot
plus right restore per normal stereo pair. The audit's historical example windows
spent 2.7-3.9 ms inside the two native calls. These are CPU durations including
possible waits, not a promise of removable cost. Desktop delivery is the first
candidate because it has both measured cost and repeated work that the headset
does not need.

This branch adds two opt-in modes. Both leave the two native scene draws,
per-eye hooks, capture, image/pose tags, OpenXR work and engine hook count in place.
No engine memory writer or liveness cache changes. Shader caching and bridge
consolidation remain separate future improvements.

## Controls

| Mode | F10 Display | INI under VR | Live command |
|---|---|---|---|
| Full, accepted behavior | Both candidate checkboxes off | ReduceDesktopPresent=0, DesktopMirrorOff=0 | desktoppresent full |
| Reduced desktop updates | Reduce desktop presentation checked | ReduceDesktopPresent=1, DesktopMirrorOff=0 | desktoppresent reduced |
| Desktop updates off | Disable desktop mirror checked | DesktopMirrorOff=1, overrides reduction | desktoppresent off |

Both new keys default to 0, including absent-key fallback. F10 persists changes;
SAVE AS DEFAULTS also writes both keys. The command seam is a live A/B and does
not persist by itself. `desktoppresent status` reports the resolved values.
The two-key design permits returning from Off to a previously selected Reduced
mode. The reduced checkbox is disabled while Off overrides it.

Off freezes the last desktop image; it does not hide the window or turn off VR.
The existing `vrmirror off` means bypass the pin callback and is not this new
feature. If that callback is absent, these new modes fall back to native Present.

## Reduced mode

Only an identified current right draw immediately after a successfully displayed
left can omit its original desktop Present and the restore StretchRect. The
left snapshot remains, so unknown frames and fallbacks can use the accepted pin.
The permission is consumed even when it refuses; it cannot suppress consecutive
calls or survive a gap in hook serials. It does not use the delivered eye tag to
identify current D3D9 pixels, preserving the VR-76 distinction.

Required conditions include a fresh delivered capture serial, a produced texture,
live XR session, reentry projection, draw-source pin, valid left snapshot,
adjacent successful left native Present returning exactly D3D_OK, and standard
Present arguments. A positive status such as occlusion is not D3D_OK.

The swapchain must report windowed DISCARD, one backbuffer, no multisampling and
immediate presentation. Actual parameters are queried once per device/reset/pin
lifecycle, not inferred from an INI fullscreen request. Unsupported parameters,
custom rectangles/window/dirty-region arguments, copy failures, repeated or
unknown eyes, absent callbacks, missing output or stale capture use the native
path. Resize/reset/device/source transitions invalidate the permission.

## Off mode and command submission

With a fresh capture, live XR session, runtime mirror callback, standard arguments
and supported swapchain, Off omits all mirror copies and the native Present.
This includes captured mono UI frames. Missing fresh output or lost session falls
back to normal presentation, so the desktop may update during such transitions.
Old pin provenance is invalidated while Off; returning to Full/Reduced requires
a new left snapshot and cannot restore an old pre-Off view.

An event query issues END for the current D3D9 command stream, then makes one
GetData(D3DGETDATA_FLUSH) call. S_FALSE is pending, not completion. There is no
spin, sleep or CPU wait for GPU completion in this production path. The query
does not authorize shared-slot reuse or replace capture's ownership fences.
Query creation/issue/poll errors refuse Off until reset or an Off toggle, and
call the original Present. The query is released before device Reset.

The first implementation retained a pending query and polled it again next
frame. The native test disproved it: frame 2's independent GPU marker remained
pending for two seconds because an already-completed older event could return
without submitting newly queued work. The corrected implementation issues END
every frame, intentionally abandoning the old event result. This query only
requests submission; no consumer depends on its discarded result. Microsoft
documents END reissue in the issued state as abandoning the previous query.
[Query states](https://learn.microsoft.com/en-us/windows/win32/direct3d9/queries),
[GetData](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3dquery9-getdata).

DISCARD does not preserve backbuffer contents after native Present. The host
model poisons discarded buffers and renders new content on each draw. The real
game's buffer/driver behavior still requires headset validation; a source model
cannot prove that every engine path honors this contract.
[D3DSWAPEFFECT](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dswapeffect).

## Timing and evidence

The runtime callback defers mirror work only when a candidate mode is selected.
The final decision happens after on_present_end returns, because a runtime
failure can occur after its mirror callback. In Full mode, mirror work keeps its
accepted callback position. In all modes, capture precedes any desktop action.
The runtime HUD hook is a no-op in this host; this deferral would need revisiting
if it ever wrote into the D3D9 backbuffer.

Every three seconds `desktoppresent:` reports hooks, actual calls, omitted calls,
Off omissions, non-OK native results, and categorized fallback counts. CPU costs
separate native Present, mirror work and Off submission. Means identify their
population: per hook versus per actual call. Native maximum is also recorded.
Per-present records retain action K (Reduced) or O (Off), nativeCalled and timings.
The original mirror shadow/copy counters do not count Off frames.

The older perf game-Present bracket now includes the host tail, including deferred
mirror work in candidate modes. Use the new native and mirror timings for
attribution; do not compare that one old column as if its scope were unchanged.
The hook/pair cadence still includes omitted desktop calls. None of these desktop
counters is a fresh OpenXR pair count or a GPU timer. Existing D3D9 span-label
issues and missing full D3D11 attribution remain VR-67 work.

## Validation

- `tools/desktop-eye-host.ps1`: accepted policy suite, legacy delayed-tag negative
  control, and production copy/tail module against a deterministic D3D9 device.
  79,339 policy and 431,857 copy/tail assertions pass. Exhaustive length-eight
  L/R/unknown sequences cover current and delayed tags. Includes failures,
  unsupported modes, custom arguments, missing callbacks, lifecycle and toggles.
- In that model, 100 normal pairs in Reduced retain 200 synthetic captures,
  perform 100 native Presents and 100 left snapshots, and perform zero restores.
  In Off, 100 synthetic captured frames perform zero native Presents or mirror
  copies and make 100 submission requests. These are operation counts, not FPS.
- `tools/desktop-present-d3d9-host.ps1`: standalone real 32-bit D3D9Ex HAL device,
  System32 D3D9, hidden 64x64 render target, no game/proxy/OpenXR. Queues an
  independent GPU event before the production Off tail and polls it without
  FLUSH afterward. 120 GPU completions and alternating pixel checks pass with
  zero native Presents; Full return and ResetEx pass. Test refuses to run while
  Dishonored is running. It does not model headset pacing or the engine renderer.
- Release build passes. Production default writer, repository release profile
  and golden INI are byte-identical, with only the two new default-off keys and
  their comments added to the accepted profile. Reentry ring and single-tag host
  regressions pass. Final exports/lint and artifact identity accompany the saved
  candidate. No installed settings were changed.

## Deferred test and continuation

The user is validating the entire opening on installed accepted build266 first.
Do not install this candidate or arm a diagnostic during that run. Installed
identity is still in `build/playtest-candidates/installed.json`; this branch's
artifact belongs in `build/playtest-candidates/vr-115-desktop-present/` and must
not overwrite that installed manifest. Check the log banner before interpreting
the opening, then preserve both logs before any later relaunch.

When installation is requested, first ask one question of a fixed, repeatable
scene: does Full -> Off -> Full improve complete stereo-pair frame times without
stale eyes, flicker, extra latency or texture faults? Keep resolution, scene,
refresh, capture, pose lag and diagnostics fixed, warm each segment, and repeat
baseline. Expected supporting evidence: Off omission count rises, native/copy
cost falls, and pair median/tails improve beyond repeated-baseline noise. If the
wait moves into capture/XR or pair times do not improve, reject the performance
hypothesis even if desktop-call count falls. Off's frozen window is expected.

Reduced is a separate subsequent A/B question for users who want a live desktop.
Then separately validate menus, notes, loading, cinematics, weapon/head movement,
return to Full, focus changes and device reset. One question per launch, with
outcomes defined before the tester starts. No FPS gain, headset correctness or
120 Hz result is claimed yet. No merge until explicitly authorized.

## 2026-09-14 resumed on current main

The old271 candidate above is historical and must not be installed over the
accepted world fix. The new rollout ports source160949cb3 onto main85f9ef6e4,
adds an automatic Full/Off/Full trial and accurate fresh-eye submission timing.
See PERFORMANCE_ROLLOUT.md for the current install identity and test contract.
No headset performance gain is established yet.


<a id="record-9"></a>

## Record 9: PERFORMANCE_ROLLOUT.md

Historical source from `codex/vr-115-performance-rollout:docs/dishonored/PERFORMANCE_ROLLOUT.md`. Not a current test instruction.

# Performance rollout

## Current work, 2026-09-14

PR59 / VR-113 contains the overall audit. PR60 / VR-115 implements the first
candidate, reduced or disabled desktop presentation. Implementation of the
larger plan is now authorized; no performance merge is authorized.

Branch `codex/vr-115-performance-rollout` starts at VR-Main `85f9ef6e4` and ports
PR60 source `160949cb3` as `bc6e2230f`. It retains the accepted image-linked world
orientation, world/hand lag 2 and current camera/weapon behavior. PR63 HUD work
remains independent: its initial playtest was reported favorable, and its
contributor continues it. Neither PR63 nor the rejected PR62 hand experiment
is included here. The latest PR63 log banner matches build282, compiled18:53:23;
its timing windows are not a controlled performance baseline for this branch.

## First candidate and measurement contract

Full remains the default. `VR.ReduceDesktopPresent=0` and
`VR.DesktopMirrorOff=0` have live F10 Display controls and the existing
`desktoppresent full|reduced|off` command. See
[DESKTOP_PRESENT_PERFORMANCE.md](DESKTOP_PRESENT_PERFORMANCE.md) for guards,
current-stream submission, failed approaches and native-driver evidence.

New `Perf.DesktopAb=1` arms a bounded trial; its absent-key default is0.
F10 Display can start or stop it. It disables the legacy latency sweep.
After ten seconds of uninterrupted strict gameplay, the trial runs three
30-second segments: Full, Off, Full. The first three seconds of each segment
are discarded. Pausing, a note, a load or a cinematic aborts the comparison
and restores the pre-trial desktop flags. Completion and explicit stop also
restore those flags. No resolution, quality, lag or image pose setting changes.

Successful eye-copy/release publishes the delivered capture serial. Only a
successful xrEndFrame with two separate game-eye projection swapchains and
both serials advancing contributes a fresh-pair observation. Mono, held,
replayed, one-eye-only and failed submissions are not counted as new pairs.
Intervals span rejected submissions, so stalls remain visible. This measures
application submission cadence, not physical display refresh, GPU completion,
photon latency, or a proof that both eyes belong to the same game simulation tick.
Existing pose/image-age and desktop fallback logs must be checked alongside it.

Storage is bounded to16384 intervals per segment; overflow invalidates the
segment. Severe stalls stay in the quantiles. Logs report count, mean/rate,
p50/p95/p99/p99.9/max and counts over8.333/16.667/33.333ms. At least32 samples
and no overflow are required. The returned Full baseline establishes a noise
floor for each metric; no automatic FPS-benefit verdict is printed.

The old GPU line subtracted post-entry capture from the earlier render span
and subtracted a GPU interval from CPU lock time. Those independent intervals
now carry accurate labels without a fabricated recoverable-cost calculation.
Old `perf/ab` Present-parity statistics remain historical and are not the new
fresh-pair population. Broader mixed tagged/untagged accumulation and GPU-stage
coverage remain in Phase A.

## First headset test

Load the same sewer save, face a quiet stationary view and stay there for
110 seconds after gameplay appears. Keep headset refresh120Hz and current
resolution, avoid menus, movement and combat during the timed portion.
The desktop may freeze in the middle30 seconds; the headset should remain
stereo and responsive. After the timed portion, a brief head turn can check
that the view is still responding. One question: did the headset remain
correctly stereo and responsive throughout? A pass permits timing comparison;
a failure rejects the candidate regardless of its frame-time numbers.
The agent archives and reads the logs, not the tester.

## Full plan, with explicit remaining work

- [x] Restore the audit on current main and preserve the accepted world fix.
- [x] Port guarded desktop modes and add a repeatable Full/Off/Full trial.
- [x] Count successful fresh-eye submissions; fix misleading GPU subtraction.
- [ ] Measure Full/Off/Full on the headset; repeat promising results and test Reduced separately.
- [ ] Complete Phase A: asynchronous D3D11 bridge GPU timestamps, missing D3D9
      mirror/pass timings, mixed-population accounting, per-eye age and bounded
      CPU sampling/scheduling evidence (VR-67, VR-17).
- [ ] Compare pixel load at fixed aspect/FOV if GPU work owns the deadline;
      do not silently lower the accepted quality or resolution.
- [ ] Implement bounded shader-layout caching with real shader lifetime identity,
      negative results and eviction; measure parse calls and CPU cost.
- [ ] Share lazy per-draw state snapshots; then reduce animation-weight locking
      with coherent per-frame publication and immediate ownership handback.
- [ ] Measure individual diagnostics, starting with Cine.Trace; keep failure
      breadcrumbs and separate collection costs from text-output costs.
- [ ] Measure/coalesce liveness rebuilds, name/property discovery and event work;
      current-level IsLiveObject and menu revalidation remain mandatory (VR-102).
- [ ] Repair shared-fence timeout/error ownership before bridge/slot changes
      (VR-114); then measure direct-to-XR copy and queue scheduling candidates.
- [ ] Profile managed-texture uploads, dirty regions and32-bit memory pressure.
- [ ] Identify expensive native passes; share only verified eye-independent work
      and preserve visibility, transparency, weapon depth and lens consistency
      (VR-79). Upscaling/dynamic resolution are later quality tradeoffs.
- [ ] Confirm chosen wins in opening, heavier levels, menu/load/cinematic/weapon
      transitions and physical head movement. One question per launch.

The ordered phases and counterpredictions in PERFORMANCE_AUDIT.md remain the
full specification. One lever per candidate: a reduction in API call count
alone is not a performance result. No shader, liveness, bridge, texture-streaming
or scene-sharing optimization is claimed implemented by this first build.

## Validation

Standalone production benchmark tests cover serial advancement, replay/mono/
one-eye rejection, wraparound, warmup, repeated baselines, severe stalls,
bounded overflow, explicit restart/stop and menu handback. Desktop policy:
79339 assertions plus legacy negative control; production copy/tail:431857;
native D3D9Ex:120 GPU completions/pixel checks, Full return and ResetEx.
Reentry248 and single-tag23 checks pass; frame/weapon/animation/image-orientation
suite passes. Win32 release build, lint and golden INI check pass. Simulator
launch is not run because the user prohibits launching the game.

Installation identity is recorded below after packaging. Headset benefit is
pending. Release defaults remain Full; the installed trial alone arms DesktopAb.

## Installed first trial

Build `vr33-hands-working-276-g48a632e48`, compiled19:23:53, source48a632e48.
DLL SHA256 `e651d5818ca89c0d7f0c888c9d6b24c4e561032091dfeb7e9ffae61fe4195402`.
INI SHA256 `7d0dc649ea56ff4e17906a30f56325480c5cb6457dfa513ce2f9abd78a437a13`.
Bundle: `build/playtest-candidates/performance-desktop-rollout`.
Previous DLL, INI and both logs: `build/playtest-candidates/installs/20260914-192446-013854`.
The full CRLF-verified INI diff adds only Perf.DesktopAb=1,
VR.ReduceDesktopPresent=0 and VR.DesktopMirrorOff=0. Current PR63 HUD keys are
retained but inert in this main-based build. No game launched. Next log must
match276-g48a632e48 /19:23:53 before interpreting the trial. Build numbers
reflect branch ancestry;276 here does not mean the old rejected hand candidate.

## Publication pending explicit approval

The installed trial and local commits are complete. Automatic approval review
rejected the feature-branch push, including after origin was verified as the
public VR-Stereo-Hub/Dishonored-VR repository, because it requires explicit
user authorization for this exact source/documentation export. No push or new
PR occurred. The prepared PR body is local at build/performance-rollout-pr.md.
VR-115 was updated successfully and remains In Progress. Ask permission to
publish this branch and open its draft PR; do not bypass the review rejection.

## First headset comparison, 2026-09-14

Verified installed DLL hash and log banner276-g48a632e48 /19:23:53. Both logs
and INI archived at build/performance-results/desktop-first. All three30s
segments completed; each discarded3s warmup, no overflow, all valid.
The tester reported no headset problem during observation, but did not watch
continuously. This is limited perceptual evidence, not exhaustive acceptance.

| Metric | Full first | Off | Full return |
|---|---:|---:|---:|
| Fresh pair intervals |2349|2652|2287|
| Fresh pair rate /s |87.01|98.28|84.72|
| Mean ms |11.492|10.175|11.804|
| Median ms |10.759|7.894|10.711|
| p95 ms |18.878|21.250|20.486|
| p99 ms |37.350|35.166|40.814|
| p99.9 ms |80.168|85.976|88.825|
| Maximum ms |100.340|103.410|100.523|
| Intervals over16.667ms |158 (6.73%)|279 (10.52%)|197 (8.61%)|
| Intervals over33.333ms |28|31|39|
| Rejected submissions |12|25|7|

Off improved observed throughput by12.95-16.01% over the two Full segments,
and median interval by about26.5%. Tail evidence is mixed: p95 is worse than
both baselines, over16.667ms share increases, p99 is lower, and p99.9/max
do not establish a consistent improvement. Do not call this a smoothness fix
or stable120Hz. Rejected submissions also increased and need follow-up.

The Off desktop windows show5887 omitted Presents and28 native fallbacks,
all context refusals; no query/nonOK failure. Off freezes the last image,
not a black screen. Sampled world orientation still matches captured input;
left/right fallback totals stayed0/2. Held-layer diagnostics during Off
report black=0. These do not measure physical image age or prove the absence
of every transient. Full was restored at completion; installed defaults were
not promoted and no new build or INI edit was made after this run.

Next: repeat the identical scene to confirm throughput/tail changes before
promoting Off; compare Reduced as a separate controlled candidate if the
Off tail penalty repeats. Continue the remaining Phase A timing/CPU work in
parallel with the broader rollout; do not treat removed Present CPU time as
fully recovered render budget. Publication remains blocked pending explicit
user permission, as already recorded. No merge authorized.

## Repeat result and Reduced trial

Second run verified276-g48a632e48 /19:23:53, archived with both logs and INI
at build/performance-results/desktop-second. All three segments completed,
valid with no overflow. This report did not include a new visual verdict.

| Metric | Full first | Off | Full return |
|---|---:|---:|---:|
| Fresh pairs/s |81.86|95.62|84.02|
| Median ms |11.031|8.108|10.907|
| p95 ms |21.364|22.440|21.309|
| p99 ms |42.446|36.959|36.806|
| p99.9 ms |100.051|65.953|75.390|
| Max ms |101.346|93.974|80.132|
| Intervals over16.667ms |193/2209 (8.74%)|267/2581 (10.34%)|195/2268 (8.60%)|
| Rejected submissions |11|29|12|

Off omitted5697 desktop Presents,31 context fallbacks. Its throughput gain
of13.8-16.8% repeats, as do worse p95, higher16.667ms exceedance share and
more rejected submissions. p99 is within the baseline range this time.
Do not promote Off as a smoothness fix. Full remains the default.

Next candidate changes only the benchmark's alternative: Perf.DesktopAb=2
selects Full/Reduced/Full;1 retains Full/Off/Full;0 or an unknown value is off.
F10 Display selects the alternative while stopped. Duration, warmup, fresh-eye
sampling and restore behavior are unchanged. Actual desktop policy is unchanged
from the tested PR60 port. Production host tests now cover all three Reduced
phases and restoration as well as existing Off tests.

The one-question test is the same110-second quiet sewer view at120Hz, then
a brief head turn: does the headset remain correctly stereo and responsive?
Reduced should keep the desktop updating. Logs must prove eligibility and actual
omitted Presents before calling this an effective reduced-delivery comparison.
A gain with more held images or visual faults fails; no gain means this path
is not an adequate solution and the audit proceeds to CPU/GPU discrimination.

## Reduced candidate installed

Build `vr33-hands-working-280-g8ab31c78e`, compiled19:47:38, source8ab31c78e.
DLL SHA256 `95e4975a3ad91d7bac55f2766b879e168a0081541217b6a90eb39a92807c4933`.
INI SHA256 `8e3eea040b384c9de669317888c39383293a8a866ccd40adb2335c3cd5ef6b6d`.
Complete INI diff: only Perf.DesktopAb1 ->2; CRLF verified. Previous DLL,
INI and both logs archived20260914-194819-889376. Release build, benchmark
host, lint, golden and9 exports passed. No game launched. Next run must
match280-g8ab31c78e /19:47:38. Full/Reduced/Full110-second test is ready.

## Reduced result, 2026-09-14

Verified build280-g8ab31c78e /19:47:38 and installed DLL hash. All phases
completed, valid with no overflow; both logs and INI archived at
build/performance-results/desktop-reduced-first. This report supplied no
additional visual verdict, so none is inferred.

| Metric | Full first | Reduced | Full return |
|---|---:|---:|---:|
| Fresh pairs/s |86.34|90.28|89.27|
| Median ms |10.893|10.577|10.752|
| p95 ms |18.385|18.488|17.656|
| p99 ms |36.087|31.890|28.255|
| p99.9 ms |97.260|62.419|61.713|
| Max ms |100.674|100.344|69.962|
| Intervals over16.667ms |153/2325 (6.58%)|164/2437 (6.73%)|146/2409 (6.06%)|
| Rejected submissions |1|1|13|

Reduced skipped2723 desktop Presents and retained2727: policy was effective.
Throughput is1.13-4.56% above the two baselines, a modest effect against a
3.39% baseline rate drift. Median improves1.63-2.90%; p95 is worse than both
baselines and p99 falls inside their range. No clear frame-consistency win.

Using the nine desktop windows fully after the3s warmup in each phase,
weighted native Present cost per hook was1.560 /1.457 /1.542ms. Cost per
actual native call was1.560 /2.913 /1.542ms. Nearly halving the calls did not
halve their aggregate CPU cost: the remaining calls absorbed much of the
waiting. This is measured timing redistribution, not proof of a specific
driver or GPU bottleneck. No errors were present in the printed Reduced
window counters. No render/pose behavior or default is promoted.

Decision: park both desktop alternatives as optional experiments and keep
Full. The repeated Off throughput gain is real in these workloads but carries
a repeated long-frame penalty. Reduced is too small/inconsistent to be the
primary performance solution. Stop automatic desktop trials after this run.
Next work follows the audit's Phase A render-thread CPU/GPU attribution,
then measured shader-reflection/state-query/diagnostic optimization. Do not
request another identical desktop trial without a new hypothesis.

Post-trial installed configuration: same280 DLL, Perf.DesktopAb=0. Full
INI diff contains only2 ->0; CRLF verified. INI SHA256
`6ffe0fe51ad6a78e8ecfcce4e91ad9242f194916334160af18921275438e98f2`; prior install/logs archived195539-485737.
No new binary behavior or game launch.


<a id="record-10"></a>

## Record 10: BRIDGE_GPU_PROFILE.md

Historical source from `codex/vr-123-bridge-gpu-profile:docs/dishonored/BRIDGE_GPU_PROFILE.md`. Not a current test instruction.

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


<a id="record-11"></a>

## Record 11: DIAGNOSTIC_OVERHEAD_AB.md

Historical source from `codex/vr-124-diagnostic-overhead`. Current verdicts take precedence.

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


## Historical session summaries migrated from docs/STATUS.md

Superseded; use current state at the top.

## Current VR-125: UE3 online research completed, 2026-09-15

See UE3_PERFORMANCE_RESEARCH.md for primary sources and ranked leads.
Priority: check whether engine-owned query/frame waits repeat per eye; no
existing timing separates game query type/caller/eye from mod ownership fences.
If cheap, pursue duplicated scene preparation/culling/draw submission. Actual
AI/world tick is outside the doubled viewport draw. Low aggregate utilization
is not proof of spare capacity on the critical render/driver dependency.
No new tweak/test armed;307 and exact INI unchanged. HUD excluded. No merge.

## Current VR-125: nonblocking desktop test rejected, 2026-09-15

Verified313 banner/hash; automatic off/on/off completed. Complete windows:
58.21 /57.70 /58.29 ticks/s (10/13/9 windows). Native desktop Present summed
across both eyes4.840 /4.869 /4.878ms. Last on heartbeat4818 attempts,
4818 accepted,0 busy,64 context fallbacks,0 errors. The path engaged but did
not bypass the driver wait; no useful gain. Tester reports no apparent change.
Do not repeat this test or promote the default-off lever. Preserved on branch.
Restored exact307 DLL/INI, full diff only removes DesktopNonblocking=0; CRLF
verified and both logs archived. No recorder, helper, game or playtest pending.
Continue engine/native-driver draw-cost attribution; usable GPU execution
correlation remains missing. Do not infer all blocked time is removable or
promise a large gain. HUD stays excluded. No merge authorized.

## Current VR-125: representative CPU trace complete; HUD excluded

Same-run before/during/after55.47/55.80/56.80 logged ticks/s; tester reports
consistent lag. Lightweight trace saved/stopped with zero lost events.
Interior render thread72.43% running,26.91% blocked,0.57% ready; NVIDIA worker
wakes most blocked time and repeats a sampled polling hotspot. This identifies
a dependency, not removable cost. Details RESOLUTION_FLOOR.md.
HUD changes are explicitly out of scope: tester reports unchanged performance
across recent HUD update; normal HUD conversion only0.99% of render CPU samples.
No HUD code changed. Continue engine/native-driver draw submission attribution.
Installed307/INI unchanged. No pending playtest, game launch, or merge.

## Current VR-125: untraced control and lightweight CPU capture

Same307/original INI: untraced hub57.14ticks/s versus heavy trace45.85.
Heavy trace GPU events rolled past gameplay despite zero lost-event count;
CPU scheduling/samples retained. No usable gameplay GPU timeline in that ETL.
Prepared verified CPU-only profile: samples/stacks plus scheduling, no syscall
or GPU events. Timed helper armed at build/performance-results/vr125-light-headset,
awaiting tester launch. Same hub/weapons/120Hz for3min; question: is lag close
to untraced control throughout? Automatic30s baseline/40s trace/save/cooldown/
30s baseline. Read markers, event coverage and log banner before analysis.
HUD submission batching is a candidate only, not an implemented optimization.
Details RESOLUTION_FLOOR.md. No agent game launch or merge authorized.

## Current VR-125: elevated trace adds measurable driver overhead

Headset combined CPU/GPU trace saved and stopped. Exact307/original resolution.
User reports worse lag; steady windows45.85ticks/s. Render thread63.87% running,
35.17% blocked,0.92% ready; NVIDIA worker wakes most blocked time and spends
18.01% of CPU stack samples within ETW event writing. Heavy trace is perturbed,
not a normal baseline. Full evidence and limitations: RESOLUTION_FLOOR.md.
Next headset test: identical307/INI with recorder OFF, same hub60s. No game
launched by agent. Prior launch grant ended with shutdown. No merge.
Raw ETL, matching symbols, logs and reports are local under ignored build/.

## Current VR-125: proxy functions identified with matching symbols

Two2000-IP batches on exact307/PDB repeat vertex-constant, profiler, memcpy and
weapon-processing leaves; no single large proxy leaf. Counts include waiting
and are not CPU percentages or recoverable time. Evidence RESOLUTION_FLOOR.md.
Added offline hash-verified symbol resolver. Build inspection: RelWithDebInfo
already /O2, but /Ob1 and incremental linking; do not call it an unoptimized build.
Next bounded experiment: opt-in compiler inlining comparison, same source/runtime
settings, then runtime/headset verification before any promotion. Keep VR-125.
Game closed; accepted298 and exact original INI restored. No merge. Agent launch
authorization continues until shutdown requested; no pending headset test.

## Current VR-125: INI shadow test produces no useful hub gain

Surveyed actual engine/compat INIs; active Vsync/smoothing/AO/blur/DOF already off.
DynamicShadows on/off/restored across three automated runs:79.94/80.63/79.26
logged ticks/s. Render-target changes fell, render cycles only about1%; retain
original quality. Empty console replies were not treated as a successful toggle;
accepted off test used one backed-up INI edit plus restart. Full evidence and
limits in RESOLUTION_FLOOR.md, setting candidates in GAME_CONFIG_MAP.md.
Next: matching-symbol identification of expensive proxy draw paths inside the
outside-Present region. Keep using VR-125; no new ticket or merge.
Game closed; accepted298, exact original mod INI and ALL game INIs restored.
Agent launch authorization continues until shutdown requested. No headset test pending.

## Current VR-125: rendering between Presents dominates measured cycles

Automated default-off scoped diagnostic307 tested in the pub view. About88%
of render-thread cycles occur outside Present (engine rendering + draw hooks).
Game-thread viewport calls total about1.23ms wall; adding game-thread workers
is not the indicated next step. Coarse GetThreadTimes per-stage values proved
unreliable; use relative cycles, never convert cycles to ms. Full evidence and
limits in RESOLUTION_FLOOR.md. Off/on/off rates80.23/82.36/80.68 are not a gain.
Next identify costly proxy draw paths using the archived matching DLL/PDB,
then select one bounded redundant-work experiment. All work stays VR-125.
Game closed; accepted298 and exact original INI restored. Launch authorization
continues until shutdown requested. No merge and no pending headset test.

## Current VR-125: instruction sampling narrows attribution

Approved findings posted; ticket cleanup leaves only VR-125 and VR-122 In Progress.
Non-elevated pub-view instruction sampling completed twice. Render samples:
36% game,13-14% native D3D9,13% proxy; these include waits, not CPU percentages.
Driver hot region contains polling; its fraction is sensitive to sampling order.
Do not equate driver CPU usage with useful work or claim a performance fix.
Next: exact DLL/PDB preservation and default-off scoped CPU-vs-wall rendering
attribution, without thread suspension. Details and caveats: RESOLUTION_FLOOR.md.
Game closed; accepted installed DLL/full INI unchanged. Launch authorization
continues until shutdown requested. No merge and no pending headset test.

## Latest VR-125: CPU-side rendering and driver work measured

Automated four-view sweep completed; pub view79.3 ticks/s vs simpler110-118.
Render thread77.6% of one core, separate D3D9 worker71.1% in pub view.
CPU work established, active stacks and removable cost remain unknown.
Next: executing-stack attribution or scoped CPU-vs-wall timing in rendering.
Simulator differs from headset runtime; no performance gain claimed.
Game closed, installed DLL/full INI unchanged. Launch authorization persists
until user requests shutdown. Full evidence in RESOLUTION_FLOOR.md.

## Latest: automated simulator launch recovered

User now authorizes automated game launches for VR-125. Fixed simulator GPU
adapter tie selection; added scoped layer opt-outs to selftest. Steam launch,
Continue, Hound Pits and gradual turn verified. Shared capture and eye shots
work. Preliminary automated timing is not equivalent to headset workload.
Game closed; installed DLL/INI unchanged. See RESOLUTION_FLOOR.md for evidence
and exact continuation. Next identify thread owners and match heavy view.

## New investigation: VR-125 resolution floor

Branch codex/vr-125-resolution-floor, installed renderer unchanged at original
2750x2850. Plan: docs/dishonored/RESOLUTION_FLOOR.md. Use system CPU stacks,
scheduling/waits and GPU correlation rather than further small API probes.
Recorder elevation requested after non-admin profiling-policy refusal. Verify
recording status before test; stop/save on the next user report. No merge.

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

## Latest result: resource/state profiler validated

Build298-g3d80740a9 passed headset validation.38 complete gameplay windows,
114.202 seconds at120Hz. Largest new path: texture setter23.68ms/s;
vertex-buffer locks0.515ms/s, texture locks0.090ms/s. Nested scopes are not
additive. These captured paths do not explain sustained lag; zero calls do
not establish complete API coverage. Full results in NATIVE_DRAW_PROFILE.md.
Next: whole-scene GPU timing / CPU-GPU correlation, then engine attribution.
No repeat required, no performance gain claimed. Installed build unchanged.

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

## Current candidate: native draw profile, VR-121 continuation

Independent branch codex/vr-121-native-draw-profile from main18ae4ebda extends
the existing render-thread profiler to draw, shader-constant and render-target
hooks versus native API calls. Previous diagnostic A/B completed without a
compelling gain; all earlier experiment branches retained. NativeProfile defaults
0; test install enables1 and disarms DiagnosticAb. See NATIVE_DRAW_PROFILE.md.
Headset run passed visually. 34 complete gameplay windows (102.136 seconds):
indexed hook 161.08 ms/s, native indexed 69.18; constant hook 99.84, native
constant 58.50. Nested estimates are not additive or exact removable overhead.
Full result: docs/dishonored/NATIVE_DRAW_PROFILE.md. Next: attribute remaining
rendering/API work and waits; no repeat needed and no performance gain claimed.
No merge.

Installed and hash-verified build `vr33-hands-working-295-gf4062ba4b`, compiled 21:46:15. Release build and host checks passed. Full INI comparison: only NativeProfile=1 added and DiagnosticAb changed from1 to0; CRLF preserved. Both previous logs and binaries archived in `C:\dev\Dishonored-VR\build\playtest-candidates\installs\20260914-214922-606852`. Game not launched.



<a id="record-12"></a>

## Record 12: PERFORMANCE_AUDIT.md (later profile branch)

Historical source `codex/vr-121-render-thread-profile:docs/dishonored/PERFORMANCE_AUDIT.md`. Current verdicts above take precedence.

# Performance audit and improvement plan

Implementation is now authorized. The audit below is historical evidence, not
a claim about the current candidate. See [PERFORMANCE_ROLLOUT.md](PERFORMANCE_ROLLOUT.md)
for current progress, test identity, and remaining phases.

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

<a id="record-13"></a>

## Record 13: RENDER_THREAD_PROFILE.md (later profile branch)

Historical source `codex/vr-121-render-thread-profile:docs/dishonored/RENDER_THREAD_PROFILE.md`. Current verdicts above take precedence.

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

Tracking note: VR-121 exists and is In Progress. The later installation-detail update did not succeed; a read-back confirmed the original description. The full implementation and install evidence remain in this local document for reconciliation.

## First headset profile result

Verified275-gdeeaf66e5 /20:03:32 and installed DLL hash. Both logs/INI archived
at build/performance-results/render-profile-first. Tester reports no visual
problems.42 complete gameplay windows total126.15s; an early state transition
discarded partial data as designed. The first uninterrupted minute gives the
same ranking as the complete gameplay set. It is not assumed that the entire
126s was stationary.

| Scope | Calls | Samples | Estimated inclusive ms/s | First quiet-minute estimate ms/s |
|---|---:|---:|---:|---:|
| Layout refresh |14747408|230454|16.015|15.861|
| CTAB reflection |1490253|23231|2.801|2.848|
| Bytecode reads |1490253|23231|2.203|2.205|
| Weapon draw, inclusive |14539616|227170|67.873|66.036|
| Initial buffer queries |14539616|227121|17.743|17.406|
| Animation weight/lock |29541117|461759|17.051|16.745|

Reflection and bytecode reads together estimate about5ms per second, not5ms
per frame. Their volume is high but the measured cost is small. A shader cache
is therefore lower priority for this workload, particularly given lifetime
complexity. The whole sampled weapon router is roughly6.8% of elapsed time
on this thread, about0.7-0.8ms per frame at90-100fps; nested scope numbers
must not be added to it. This is wall time including waits and sampling
overhead, not a guaranteed recoverable budget or an optimized-build result.

Late mostly tagged windows show existing D3D9 render-to-Present-entry spans
around7.1-7.3ms per pair and roughly10-11ms total frame intervals. The old GPU
line's span-minus-capture and lock-minus-capture claims are invalid, as already
recorded in the audit; use only its independently bracketed intervals. These
observations do not prove GPU saturation or identify the whole GPU workload.

Next priority: async D3D11 bridge GPU measurements and further CPU attribution
of unmeasured native draw/diagnostic work. Do not start a complex shader lifetime
cache on the assumption that 1.49 million reflections must be expensive. Buffer
query/animation optimizations remain smaller candidates after larger costs are
located. No performance gain or stable120Hz result is claimed.

<a id="record-14"></a>

## Record 14: PERFORMANCE_ROLLOUT.md (later profile branch)

Historical source `codex/vr-121-render-thread-profile:docs/dishonored/PERFORMANCE_ROLLOUT.md`. Current verdicts above take precedence.

# Performance rollout

## Current work, 2026-09-14

PR59 / VR-113 contains the overall audit. PR60 / VR-115 implements the first
candidate, reduced or disabled desktop presentation. Implementation of the
larger plan is now authorized; no performance merge is authorized.

Branch `codex/vr-115-performance-rollout` starts at VR-Main `85f9ef6e4` and ports
PR60 source `160949cb3` as `bc6e2230f`. It retains the accepted image-linked world
orientation, world/hand lag 2 and current camera/weapon behavior. PR63 HUD work
remains independent: its initial playtest was reported favorable, and its
contributor continues it. Neither PR63 nor the rejected PR62 hand experiment
is included here. The latest PR63 log banner matches build282, compiled18:53:23;
its timing windows are not a controlled performance baseline for this branch.

## First candidate and measurement contract

Full remains the default. `VR.ReduceDesktopPresent=0` and
`VR.DesktopMirrorOff=0` have live F10 Display controls and the existing
`desktoppresent full|reduced|off` command. See
[DESKTOP_PRESENT_PERFORMANCE.md](DESKTOP_PRESENT_PERFORMANCE.md) for guards,
current-stream submission, failed approaches and native-driver evidence.

New `Perf.DesktopAb=1` arms a bounded trial; its absent-key default is0.
F10 Display can start or stop it. It disables the legacy latency sweep.
After ten seconds of uninterrupted strict gameplay, the trial runs three
30-second segments: Full, Off, Full. The first three seconds of each segment
are discarded. Pausing, a note, a load or a cinematic aborts the comparison
and restores the pre-trial desktop flags. Completion and explicit stop also
restore those flags. No resolution, quality, lag or image pose setting changes.

Successful eye-copy/release publishes the delivered capture serial. Only a
successful xrEndFrame with two separate game-eye projection swapchains and
both serials advancing contributes a fresh-pair observation. Mono, held,
replayed, one-eye-only and failed submissions are not counted as new pairs.
Intervals span rejected submissions, so stalls remain visible. This measures
application submission cadence, not physical display refresh, GPU completion,
photon latency, or a proof that both eyes belong to the same game simulation tick.
Existing pose/image-age and desktop fallback logs must be checked alongside it.

Storage is bounded to16384 intervals per segment; overflow invalidates the
segment. Severe stalls stay in the quantiles. Logs report count, mean/rate,
p50/p95/p99/p99.9/max and counts over8.333/16.667/33.333ms. At least32 samples
and no overflow are required. The returned Full baseline establishes a noise
floor for each metric; no automatic FPS-benefit verdict is printed.

The old GPU line subtracted post-entry capture from the earlier render span
and subtracted a GPU interval from CPU lock time. Those independent intervals
now carry accurate labels without a fabricated recoverable-cost calculation.
Old `perf/ab` Present-parity statistics remain historical and are not the new
fresh-pair population. Broader mixed tagged/untagged accumulation and GPU-stage
coverage remain in Phase A.

## First headset test

Load the same sewer save, face a quiet stationary view and stay there for
110 seconds after gameplay appears. Keep headset refresh120Hz and current
resolution, avoid menus, movement and combat during the timed portion.
The desktop may freeze in the middle30 seconds; the headset should remain
stereo and responsive. After the timed portion, a brief head turn can check
that the view is still responding. One question: did the headset remain
correctly stereo and responsive throughout? A pass permits timing comparison;
a failure rejects the candidate regardless of its frame-time numbers.
The agent archives and reads the logs, not the tester.

## Full plan, with explicit remaining work

- [x] Restore the audit on current main and preserve the accepted world fix.
- [x] Port guarded desktop modes and add a repeatable Full/Off/Full trial.
- [x] Count successful fresh-eye submissions; fix misleading GPU subtraction.
- [ ] Measure Full/Off/Full on the headset; repeat promising results and test Reduced separately.
- [ ] Complete Phase A: asynchronous D3D11 bridge GPU timestamps, missing D3D9
      mirror/pass timings, mixed-population accounting, per-eye age and bounded
      CPU sampling/scheduling evidence (VR-67, VR-17).
- [ ] Compare pixel load at fixed aspect/FOV if GPU work owns the deadline;
      do not silently lower the accepted quality or resolution.
- [ ] Implement bounded shader-layout caching with real shader lifetime identity,
      negative results and eviction; measure parse calls and CPU cost.
- [ ] Share lazy per-draw state snapshots; then reduce animation-weight locking
      with coherent per-frame publication and immediate ownership handback.
- [ ] Measure individual diagnostics, starting with Cine.Trace; keep failure
      breadcrumbs and separate collection costs from text-output costs.
- [ ] Measure/coalesce liveness rebuilds, name/property discovery and event work;
      current-level IsLiveObject and menu revalidation remain mandatory (VR-102).
- [ ] Repair shared-fence timeout/error ownership before bridge/slot changes
      (VR-114); then measure direct-to-XR copy and queue scheduling candidates.
- [ ] Profile managed-texture uploads, dirty regions and32-bit memory pressure.
- [ ] Identify expensive native passes; share only verified eye-independent work
      and preserve visibility, transparency, weapon depth and lens consistency
      (VR-79). Upscaling/dynamic resolution are later quality tradeoffs.
- [ ] Confirm chosen wins in opening, heavier levels, menu/load/cinematic/weapon
      transitions and physical head movement. One question per launch.

The ordered phases and counterpredictions in PERFORMANCE_AUDIT.md remain the
full specification. One lever per candidate: a reduction in API call count
alone is not a performance result. No shader, liveness, bridge, texture-streaming
or scene-sharing optimization is claimed implemented by this first build.

## Validation

Standalone production benchmark tests cover serial advancement, replay/mono/
one-eye rejection, wraparound, warmup, repeated baselines, severe stalls,
bounded overflow, explicit restart/stop and menu handback. Desktop policy:
79339 assertions plus legacy negative control; production copy/tail:431857;
native D3D9Ex:120 GPU completions/pixel checks, Full return and ResetEx.
Reentry248 and single-tag23 checks pass; frame/weapon/animation/image-orientation
suite passes. Win32 release build, lint and golden INI check pass. Simulator
launch is not run because the user prohibits launching the game.

Installation identity is recorded below after packaging. Headset benefit is
pending. Release defaults remain Full; the installed trial alone arms DesktopAb.

## Installed first trial

Build `vr33-hands-working-276-g48a632e48`, compiled19:23:53, source48a632e48.
DLL SHA256 `e651d5818ca89c0d7f0c888c9d6b24c4e561032091dfeb7e9ffae61fe4195402`.
INI SHA256 `7d0dc649ea56ff4e17906a30f56325480c5cb6457dfa513ce2f9abd78a437a13`.
Bundle: `build/playtest-candidates/performance-desktop-rollout`.
Previous DLL, INI and both logs: `build/playtest-candidates/installs/20260914-192446-013854`.
The full CRLF-verified INI diff adds only Perf.DesktopAb=1,
VR.ReduceDesktopPresent=0 and VR.DesktopMirrorOff=0. Current PR63 HUD keys are
retained but inert in this main-based build. No game launched. Next log must
match276-g48a632e48 /19:23:53 before interpreting the trial. Build numbers
reflect branch ancestry;276 here does not mean the old rejected hand candidate.

## Publication pending explicit approval

The installed trial and local commits are complete. Automatic approval review
rejected the feature-branch push, including after origin was verified as the
public VR-Stereo-Hub/Dishonored-VR repository, because it requires explicit
user authorization for this exact source/documentation export. No push or new
PR occurred. The prepared PR body is local at build/performance-rollout-pr.md.
VR-115 was updated successfully and remains In Progress. Ask permission to
publish this branch and open its draft PR; do not bypass the review rejection.

## First headset comparison, 2026-09-14

Verified installed DLL hash and log banner276-g48a632e48 /19:23:53. Both logs
and INI archived at build/performance-results/desktop-first. All three30s
segments completed; each discarded3s warmup, no overflow, all valid.
The tester reported no headset problem during observation, but did not watch
continuously. This is limited perceptual evidence, not exhaustive acceptance.

| Metric | Full first | Off | Full return |
|---|---:|---:|---:|
| Fresh pair intervals |2349|2652|2287|
| Fresh pair rate /s |87.01|98.28|84.72|
| Mean ms |11.492|10.175|11.804|
| Median ms |10.759|7.894|10.711|
| p95 ms |18.878|21.250|20.486|
| p99 ms |37.350|35.166|40.814|
| p99.9 ms |80.168|85.976|88.825|
| Maximum ms |100.340|103.410|100.523|
| Intervals over16.667ms |158 (6.73%)|279 (10.52%)|197 (8.61%)|
| Intervals over33.333ms |28|31|39|
| Rejected submissions |12|25|7|

Off improved observed throughput by12.95-16.01% over the two Full segments,
and median interval by about26.5%. Tail evidence is mixed: p95 is worse than
both baselines, over16.667ms share increases, p99 is lower, and p99.9/max
do not establish a consistent improvement. Do not call this a smoothness fix
or stable120Hz. Rejected submissions also increased and need follow-up.

The Off desktop windows show5887 omitted Presents and28 native fallbacks,
all context refusals; no query/nonOK failure. Off freezes the last image,
not a black screen. Sampled world orientation still matches captured input;
left/right fallback totals stayed0/2. Held-layer diagnostics during Off
report black=0. These do not measure physical image age or prove the absence
of every transient. Full was restored at completion; installed defaults were
not promoted and no new build or INI edit was made after this run.

Next: repeat the identical scene to confirm throughput/tail changes before
promoting Off; compare Reduced as a separate controlled candidate if the
Off tail penalty repeats. Continue the remaining Phase A timing/CPU work in
parallel with the broader rollout; do not treat removed Present CPU time as
fully recovered render budget. Publication remains blocked pending explicit
user permission, as already recorded. No merge authorized.

## Repeat result and Reduced trial

Second run verified276-g48a632e48 /19:23:53, archived with both logs and INI
at build/performance-results/desktop-second. All three segments completed,
valid with no overflow. This report did not include a new visual verdict.

| Metric | Full first | Off | Full return |
|---|---:|---:|---:|
| Fresh pairs/s |81.86|95.62|84.02|
| Median ms |11.031|8.108|10.907|
| p95 ms |21.364|22.440|21.309|
| p99 ms |42.446|36.959|36.806|
| p99.9 ms |100.051|65.953|75.390|
| Max ms |101.346|93.974|80.132|
| Intervals over16.667ms |193/2209 (8.74%)|267/2581 (10.34%)|195/2268 (8.60%)|
| Rejected submissions |11|29|12|

Off omitted5697 desktop Presents,31 context fallbacks. Its throughput gain
of13.8-16.8% repeats, as do worse p95, higher16.667ms exceedance share and
more rejected submissions. p99 is within the baseline range this time.
Do not promote Off as a smoothness fix. Full remains the default.

Next candidate changes only the benchmark's alternative: Perf.DesktopAb=2
selects Full/Reduced/Full;1 retains Full/Off/Full;0 or an unknown value is off.
F10 Display selects the alternative while stopped. Duration, warmup, fresh-eye
sampling and restore behavior are unchanged. Actual desktop policy is unchanged
from the tested PR60 port. Production host tests now cover all three Reduced
phases and restoration as well as existing Off tests.

The one-question test is the same110-second quiet sewer view at120Hz, then
a brief head turn: does the headset remain correctly stereo and responsive?
Reduced should keep the desktop updating. Logs must prove eligibility and actual
omitted Presents before calling this an effective reduced-delivery comparison.
A gain with more held images or visual faults fails; no gain means this path
is not an adequate solution and the audit proceeds to CPU/GPU discrimination.

## Reduced candidate installed

Build `vr33-hands-working-280-g8ab31c78e`, compiled19:47:38, source8ab31c78e.
DLL SHA256 `95e4975a3ad91d7bac55f2766b879e168a0081541217b6a90eb39a92807c4933`.
INI SHA256 `8e3eea040b384c9de669317888c39383293a8a866ccd40adb2335c3cd5ef6b6d`.
Complete INI diff: only Perf.DesktopAb1 ->2; CRLF verified. Previous DLL,
INI and both logs archived20260914-194819-889376. Release build, benchmark
host, lint, golden and9 exports passed. No game launched. Next run must
match280-g8ab31c78e /19:47:38. Full/Reduced/Full110-second test is ready.

## Reduced result, 2026-09-14

Verified build280-g8ab31c78e /19:47:38 and installed DLL hash. All phases
completed, valid with no overflow; both logs and INI archived at
build/performance-results/desktop-reduced-first. This report supplied no
additional visual verdict, so none is inferred.

| Metric | Full first | Reduced | Full return |
|---|---:|---:|---:|
| Fresh pairs/s |86.34|90.28|89.27|
| Median ms |10.893|10.577|10.752|
| p95 ms |18.385|18.488|17.656|
| p99 ms |36.087|31.890|28.255|
| p99.9 ms |97.260|62.419|61.713|
| Max ms |100.674|100.344|69.962|
| Intervals over16.667ms |153/2325 (6.58%)|164/2437 (6.73%)|146/2409 (6.06%)|
| Rejected submissions |1|1|13|

Reduced skipped2723 desktop Presents and retained2727: policy was effective.
Throughput is1.13-4.56% above the two baselines, a modest effect against a
3.39% baseline rate drift. Median improves1.63-2.90%; p95 is worse than both
baselines and p99 falls inside their range. No clear frame-consistency win.

Using the nine desktop windows fully after the3s warmup in each phase,
weighted native Present cost per hook was1.560 /1.457 /1.542ms. Cost per
actual native call was1.560 /2.913 /1.542ms. Nearly halving the calls did not
halve their aggregate CPU cost: the remaining calls absorbed much of the
waiting. This is measured timing redistribution, not proof of a specific
driver or GPU bottleneck. No errors were present in the printed Reduced
window counters. No render/pose behavior or default is promoted.

Decision: park both desktop alternatives as optional experiments and keep
Full. The repeated Off throughput gain is real in these workloads but carries
a repeated long-frame penalty. Reduced is too small/inconsistent to be the
primary performance solution. Stop automatic desktop trials after this run.
Next work follows the audit's Phase A render-thread CPU/GPU attribution,
then measured shader-reflection/state-query/diagnostic optimization. Do not
request another identical desktop trial without a new hypothesis.

Post-trial installed configuration: same280 DLL, Perf.DesktopAb=0. Full
INI diff contains only2 ->0; CRLF verified. INI SHA256
`6ffe0fe51ad6a78e8ecfcce4e91ad9242f194916334160af18921275438e98f2`; prior install/logs archived195539-485737.
No new binary behavior or game launch.
