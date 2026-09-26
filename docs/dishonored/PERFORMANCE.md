## 2026-09-26: walking discovery correction accepted locally

Installed DLL hash and log banner match v1.0.1-50-g6bc58a449. Final local report
accepts the run after the periodic walking catch-up correction. Archived current and
previous log/full INI: primary build/hud-regression-20260925/walk-accepted-002458.
Across64 printed collect-cost windows,269 collections have weighted mean6.349ms
and maximum9.653ms, including fresh live table plus discovery. These windows include
changing scene/state and are not a controlled whole-frame comparison. The new
counter had no old-build equivalent; no exact speedup or zero-overhead claim.
The750ms schedule remains; scan membership/range probing changed. Preserve the fix.
CPU flight recorder/pixel/legacy flags are OFF in the accepted local build. Integration
retains the opt-in recorder with one history frame formatted per Present. Normal
build validation does not establish remote end-to-end diagnostic overhead.

## 2026-09-26: periodic walking hitch, candidate discovery optimization

Verified installed95ae3f7af DLL/banner. Archives: primary
build/hud-regression-20260925/walk-judder-000731 (current/previous logs, full INI).
Player confirms potion HUD now placed correctly; quickMode4/quickReady1 and
quickCaptured192->1566 independently prove activation. New reported surface is
whole-world translation during steady walking, brief hold/forward catch-up at
roughly one-second intervals despite similar average FPS. Not HUD decoupling.

In gameplay59682000..59737000, repeated printed frame gaps are mostly53..62ms
waiting for the game thread, with adjacent stalls often750ms apart (also735/765ms
clock quantization). Example59684328->59685078:56/57ms,52.8ms out/idle in each.
The gap logger rate-limits after3 events/window, so printed intervals are censored.
Ordinary ticks are around8..10ms; averages hide individual missing frames.
Live-table summaries max1.24ms then1.11ms do not explain the50ms stalls. Some
separate xrEndFrame stalls and streaming bursts also exist; not all gaps have
one established cause. Flight recorder/pixel diagnostics remain compiled out.

Source lead: hand SkelTick recollects components every750ms. FpCollect probes
376 raw pointer slots per expanded object, calls RangeReadable for every slot,
and LooksLikeObj on arbitrary scalar values. This exact periodicity and the
expensive discovery path make it a strong candidate, not proven stack attribution.

Candidate keeps the750ms schedule, search depth, candidate cap, equipment roots
and menu/load recovery. Rebuild the live-object hash table at collection entry
before retained-object restores or discovering new equipment. Check root and
child membership via IsLiveObject before dereference. Validate the entire scan
range once per object; when partially readable, retain the original per-slot
boundary checks. Typical range queries drop376->1 per expanded object. No camera,
movement, stereo or accepted HUD policy changes.

Add one3s aggregate handmesh/collect-cost line (mean/max/last, including live-table
refresh) to distinguish successful optimization from unchanged hitching. Six host
checks extract the actual production scan and verify both range endpoints, retired
and arbitrary pointer rejection, one-query complete ranges, and guarded partial
page fallback. No in-game timing gain claimed before a matched acceptance run.

Next single question: same-save straight walking, does the periodic hold/forward
catch-up disappear? Compare collection cost and frame gaps; continued stalls with
cheap discovery would reject this hypothesis and direct investigation to the
remaining game-thread work. Never disable liveness checks to gain performance.

## 2026-09-25: remove failed cross-movie activation census

Matched36a8d7f95 DLL/banner and archived logs/full INI under primary
build/hud-regression-20260925/potion-root-return-235618. Actual mode4 has zero
quickCaptured throughout; active potion-capture cost is still unmeasured.

Correction removes the per-poll base-HUD movie lookup and31 sprite comparisons.
The existing direct potion-owner identity/mode/view checks remain, as does the
single relaxed counter on successful outer ownership. No extra GPU resource,
readback, engine call or log stream. Summary remains3s.97 host ownership checks
and100000 transfers pass, including production activation through queue replay.
Host costs16.65ns unowned wrapper/21.51ns queue roundtrip are not in-game timings.

## 2026-09-25: quick-potion return and ownership-link correction

Verified b52c0c579 installed DLL/banner; current/previous logs and full INI archived
under primary build/hud-regression-20260925/quick-potion-return-233600. User reports
acceptable performance.15 existing perf samples have median8.6ms, range7.5..817.8ms
(the whole run includes transitions, so these are not controlled A/B timings).
All28 semantic summaries report movieLink=0, quickReady=0; final transport overflow0.
This supports only the inactive-extension baseline, not the cost of active capture.

Correction changes one native field read and validates the actual view table.
All current clip/movie relationship checks stay on the existing UI poll, max32
comparisons, with no per-draw heap work. Mode is read regardless of linkage to
avoid misleading counters. A successful outer potion owner adds one relaxed
counter increment; inherited children do not repeat this ownership validation.
The existing summary remains once per3s; no new log stream, capture sink, GPU
readback or engine call is added. CPU ownership checks use existing live-object
membership plus guarded native reads. Active potion rendering still adds normal
commands to the already allocated default panel; real active cost remains for
the next matched playtest, not established by the inactive log.

Host checks:85 ownership/reader tests and100000 concurrent transfers pass;
123 native-HUD and503 routing tests pass. Host replay-wrapper15.06ns and queue
roundtrip23.12ns are microbenchmarks, not measured in-game frame savings.

## 2026-09-25: failed tutorial trial and returned performance report

Returned e322911fb is reported slightly slower. Whole-run perf tick medians:
9.1ms/58 summaries versus fe3c3f8768.3ms/101 summaries. Different scenes, menus,
durations and lens settings make this an uncontrolled comparison, not attribution
of0.8ms to the change. The tutorial sink was allocated55995281 and released
56005593, a10.312s interval; it did not remain copying for the rest of the run.
Slots were1375x1425 (Scale0.50) against2750x2850 targets. The extra panel therefore
had real GPU allocation/copy cost while visible, but did not capture the icon.
Remove it. No flicker recorder, pixel probe or new high-frequency log was enabled.

Replacement quick-potion capture reuses the existing default panel, introducing
no additional sink, readback or per-draw logging. Native sprite/movie reads only
run for unowned Display roots while a mode4 wheel view is available; children
inherit scope. Live owner/membership checks run only after a matching movie link.
Optional field resolution retries at most every5s if unavailable. The existing
poll cross-checks the view against known HUD clips; existing3s summary gains three
scalar fields. Host81 checks plus100000 concurrent transfers pass. Queue round
trip21.07ns and unowned replay16.78ns are host microbenchmarks, not in-game timing.
Headset frame-time recovery remains unconfirmed; no claim of restoring a measured
FPS amount from the uncontrolled runs.

## 2026-09-25: semantic tutorial placement cost

No new diagnostic counters, stacks, readbacks or log cadence. Candidate adds one
existing full-resolution capture panel while identified tutorial content draws.
At2750x2850 RGBA8 one image is31,350,000 bytes; this is not a zero-GPU-cost change.
The panel is retired after the existing two-present grace without draws; the
capture subsystem releases inactive targets/slots and skips their copies. Thus
occasional heal reminders do not add an idle copy for the rest of a level.
129 native-HUD policy/math checks pass. No headset GPU timing claim is made.

## Accepted ownership run and small follow-up cost (2026-09-25)

Verified4c38bf526 run52040281..52879xxx reports improved HUD stability; not an FPS
A/B measurement. Last ownership snapshot has66994508 queued,66993732 replayed,
4113901 known HUD draws and869382 native fallbacks, cumulative, overflow0.
The difference is outstanding or retired generation work, not proof of a drop.
Unknown marker roots explain a portion of fallback, not every unknown draw.

Follow-up changes only marker target validation and three clip-to-element routes.
The same3s diagnostic now includes family-root counts and fresh pivots. Count
computation is behind both the log-level and time gates, not done per draw or
on every UI poll. No new GPU diagnostic, draw census, stack tracing or recorder.
New70-check production-reader/transport suite retains100000 concurrent transfers;
latest host queue round trip23.20ns, unowned wrapper18.39ns. Host timings exclude
native marker shader transforms now becoming reachable; do not claim zero whole
frame cost for restoring those existing transforms.

## Semantic HUD transport candidate cost (2026-09-25)

Optimized x86 host tests of production CommandOwners and extracted replay wrapper:
1,000,000 iterations measured about 22.49 ns per put/take and 17.93 ns per unowned
wrapper around a trivial native command. Earlier repeats were 21.26/15.14 ns.
100,000 acknowledged cross-thread address reuses preserve payload identity. These
are host microbenchmarks, not engine FPS, full hook cost or GPU measurements.

Unowned command consumption has an empty-table atomic fast path. Probe misses use
loads before compare/exchange; they do not execute eight locked CAS operations.
The transport allocates a fixed table once, with no per-command allocation or log.
Publication reads the borrowed native allocation record under SEH, avoiding a
VirtualQuery per command. Root discovery/membership still range-checks pointers.
Display performs bounded binary search under a shared lock, then live-membership
validation only for a recognized root. The existing UI poll rebuilds at most 288
root records; full GObjects refresh happens only on load/menu generation changes.
Range checks, registry locking, source-publication hook cost and changed capture
work are outside the tiny queue benchmark, so do not extrapolate a full FPS claim.

New diagnostic output is one aggregate ownership line per three seconds while
active, plus startup/refusal messages. Counters are fixed atomic increments. No
stack captures are armed, no GPU readbacks are added, and flicker CPU recorder,
pixel diagnostics and legacy code compile OFF. This bounds instrumentation work
but does not settle the earlier overall performance report; same-save/view GPU
and native-versus-pereye attribution remains open after functional acceptance.

## Returned bounded owner capture cost (2026-09-25)

Verified ea83dc5be run 48242328 onward. Exactly three renderer snapshots reported
16.6, 22.8 and 17.4 us (56.8 us total) for stack capture, formatting and the first
log write. The following cost line and the once-only native identity block are
outside those timings; this is not an exhaustive logger benchmark. There were
no recurring ownership snapshots after these three and no GPU readback. This
capture cannot explain sustained multi-millisecond frame loss in this run.

Before pause, 48339859 reports 132.7 stereo ticks/s, 7.5 ms/tick and 6.4 ms GPU
span/tick at 144 Hz. This is a different scene from the prior grenade run, not a
controlled performance improvement or release comparison. Opening pause includes
147 ms frame gap, predominantly game-thread wait (142.7 ms); the later 52 ms gap
is predominantly xrEndFrame (45.4 ms). Do not conflate them with the finite probe.

The pause readiness repair adds one bounded atomic heartbeat per Present; no
per-draw search, extra GPU work or recurring log. Disable OwnerTrace in its expected
INI because the native/queue boundary is captured. Broader HUD/performance work
remains open and still requires controlled attribution before changing culling.

## Local HUD regression run: cost attribution and release diff (2026-09-25)

Verified 1ed638c01 DLL SHA256 b6fda98f04b9d8433ff0b6fde35ec821f7acdb94d870d048b9c918dd99dbb569,
log 32232546..32949437 (716.891 s). 2750x2850, 144 Hz, 6.94 ms budget. CPU flicker recorder,
GPU pixel probes and legacy code OFF; resolved Perf.FrameId=0. This excludes the
new recurring flicker recorder as this run's cause. It does not exclude all logging.
51,324 lines versus prior c4f5fe5df 60,335; different gameplay, not an A/B benchmark.

At 32935281 near the grenade throw, 73.7 stereo ticks/s, 13.6 ms/tick, zero untagged;
P1 OUT 7.1 ms (idle 1.3, rendering 5.8), P2 OUT 5.2 ms (idle 0, rendering 5.2), GPU span 10.3 ms
per tick, capture 0.4 ms, GPU idle 1.3ms. GPU span alone exceeds the 144 Hz budget here.
At 32701187, 57.7 ticks/s with P1/P2 render work 7.7/8.0ms and little render-thread idle.
The separate startup 26.3 ticks/s window has 17.3 ms game-thread waiting and 128 untagged;
do not label every slowdown as the same bottleneck or include loads in an FPS claim.

HUD vertex probing at 32935281: 38,349 probes/3 s, 33,631 us total, 0.9 us/probe, 64 refused.
Approximate 11.2 ms of probe CPU per second is below the multi-ms/tick deficit; it is
not the whole HUD cost (routing locks, render-target switches, copies and GPU work
are outside that interval). Native task/awareness broad matching can cause routing
churn independently of its cost. The small new interaction-cache lookup has no
allocation/logging. Wrist capture retains the existing held-item vote and steady
matrix multiply; no new engine-object scan in that draw path. Scoped-axis prior
host 0.082 us/sample is not a headset end-to-end measurement.

Important baseline difference: installed Stereo.Occlusion=pereye now activates the
post-release two-view-state implementation; 1.0.0 and the prior c4f5fe5df remote-base
DLL lack that consumer. The unchanged key was NOT equivalent runtime behavior.
Separate visibility can legitimately add draws. This remains a source suspect,
not proof of the performance regression. Both runs use 2750x2850/144 Hz; no matching
v1.0.0 playtest log was found in the available local playtest archive. Compare native
and pereye at the SAME save/view with paired timing windows before changing that
accepted visibility fix. Preserve resolution, quality, HUD and the hand/swing fixes.

The proposed ownership replacement should remove per-draw broad searches and
cross-thread position mutexes by carrying immutable owner records with render work.
Do not add per-draw GFx queries, object-table scans or GPU readbacks. First prove
the queue/display boundary with the finite OwnerTrace capture described in
HUD_ANCHORS. Actual production-source host tests exercise disabled logging/capture,
per-Present limit, 16-stack lifetime budget, once-per-family native reads and four
bounded failure attempts. Exhausted render gate measured about 3-4 ns/call on host;
not an in-game performance guarantee. Each actual capture reports elapsedUs;
no claim that its isolated stack walk/log burst is free. Normal default remains 0.

## VR-229 local diagnostic cost and normal follow-up build (2026-09-25)

Verified local build v1.0.1-8-gc4f5fe5df, recorder ON, GPU pixel collection OFF,
17263437..18005296 (741.859seconds). 60335 log lines total; 11142 flicker lines,
4476863bytes,99 windows. Approximate diagnostic output6KB/s is modest, but the
largest measured recorder finish/log burst is1.606ms. This includes the history
copy/format/log work and can include buffered file flush or scheduling; it excludes
other per-draw collection and is not an end-to-end off/on benchmark. It therefore
does not establish negligible frame-time impact.

Cause of burstiness: opening a window prints12 historical frames plus the current
frame in one Present, four lines each. Windows are bounded to one per5seconds;
healthy heartbeat10seconds. The logger is buffered with a200ms flush cadence,
not an unconditional per-line flush. Prior host recorder mean1.660us/present and
max0.867ms were a different machine/workload and do not override the measured
local1.606ms peak. No attribution of every perceived hitch to logging is possible.

The local follow-up is an optimized normal build: DVR_FLICKER_DIAGNOSTICS=OFF,
DVR_FLICKER_PIXEL_DIAGNOSTICS=OFF, DVR_WITH_LEGACY=OFF. Thus recorder history,
camera-upload census, formatting and the extra XR snapshot work compile out.
Its expected INI explicitly sets Perf.FrameId=0 because a normal build would
otherwise re-enable GPU probes from the retained FrameId=1 (the remote diagnostic
DLL had forcibly suppressed them). RingLedger's existing bounded reports remain;
new late-expire/progress reports run at most once per3seconds. Hand deferral logs
are capped at two per hand; capture/rebuild logs replace existing lines. HUD
continuity adds no logging and one bounded cache lookup for task-text candidates.
The scoped-axis publisher/read host benchmark on this checkout is0.082us/sample;
no claim of measured headset FPS improvement. Remote9da0a0b48 ZIP remains unchanged.

Next: judge the normal candidate visually, verify the matching log banner and
FrameId disabled. Re-enable the full recorder only for an identified need; if
further recording is necessary, spread historical output over presents before
claiming a negligible tail cost. Keep all performance follow-ups in this file.

## VR-79: turning occlusion queries off costs draws (2026-09-24)

Headset, same day: `off` fixed the one-eye culling and read laggier than native
(perceptual; no pairs/s were taken). The shipped candidate is `pereye`, which keeps
culling per eye and should cost about what native does plus the second eye's own
queries. The measurement below still applies, now as native vs pereye vs off.

`[Stereo] Occlusion=off` (live `occlusion off`) sets UE3's
GIgnoreAllOcclusionQueries so one eye's query results cannot cull the other
eye's draw (ENGINE_NOTES "VR-79"). Every primitive inside the frustum and its
cull distance is then drawn in both passes. The cost has NOT been measured.
Expected to be largest in dense interiors and the city (many hidden rooms behind
walls) and smallest outdoors in open areas. Earlier query-wait measurements
(0.102 ms/pair) bound the wait for results, not the draws culling saves, so they
cannot predict this.

Measurement to run, one lever, same spot, same view: pairs/s and the frame line
with `occlusion native`, then `occlusion pereye`, then `occlusion off`, then
`occlusion native` again, in the Hound Pits hub interior and on a street. `querywait on` in the
same run confirms the switch took (occlusion-path reads drop to about zero).
`pereye` is the per-eye culling that follows from that.

## VR-229 early versus stable cinematic cadence (2026-09-25)

On returned9da0a0b48, early InDialog587411343..587448000 has12 printed performance
windows: median reported stereo tick rate58.0/s (range27.7..66.3), median tick15.95ms,
median GPU per-tick span8.9ms (range6.4..13.9),137 untagged presents summed over those
windows. Later587448000..587486000 has13 windows:70.7/s (69.3..71.7),13.9ms,
GPU7.7ms (6.0..11.0),61 untagged. Headset72Hz,13.89ms budget.
These are medians of printed windows, not percentiles of every frame, and intervals
have different duration/content. No claim that GPU work is free or that the output
change recovers any measured amount. Lower GPU medians and continued diagnostic
recording during the stable period do not support GPU saturation or logging as a
complete explanation. The simultaneous plateau in stale/expiry/duplicate counters
and improved cadence supports addressing stereo interruptions/queue phase first.
The test must still distinguish residual ordinary frame-time judder from eye faults.

## VR-229 returned scoped-eye recorder and bounded output (2026-09-25)

Returned9da0a0b48: GPU frame-id probes verified disabled. Largest printed CPU recorder
finish/log peak0.512ms. The stable later cinematic still records windows, so logging
alone does not explain the early-only judder. No controlled end-to-end A/B exists.

Local actual-recorder host run before output change:100000 presents,50 c5 uploads
per present, real formatting/buffered file output; mean1.987us/present,max1.639ms.
After change:mean2.125us,max1.556ms. These are separate host runs subject to scheduling
and file flush noise, not proof of a meaningful peak-time improvement or regression.
Do not claim negligible tail cost from either mean. The structural improvement is
verified: opening a history window no longer prints12 prior frames plus the current
frame in one call (52 data lines). It prints one frame per Present, four data lines,
plus at most a window header. Same12-before/16-after evidence retained, maximum12
frames of output lag in a64-frame history. Window reopening waits for pending output;
very slow frame rates cannot overwrite the requested history. Abrupt exit may leave
the last pending records unwritten.255 production-recorder checks pass.

New render-progress fix adds only a game-thread boolean/counter and one value in the
existing3s beat. Its purpose is to prevent an unnecessary center-eye draw during one
queued render interval; it can add one extra double draw before a genuine stall is
refused. That is rendering behavior, not diagnostic overhead. Camera/state/session
guards remain. Remote candidate keeps CPU history so an unsuccessful run is still
useful, with GPU probes suppressed regardless of saved FrameId. The maintainer's
normal1ed638c01 build remains installed with the entire recorder compiled out.

## VR-229 scoped-axis follow-up (2026-09-25)

Returned candidate c4f5fe5df verifies GPU frame-id pixels OFF. Recurring recorder
max observed0.586ms (previous diagnostic0.523ms); this is a rare window maximum,
not an every-frame charge or a complete remote performance A/B. No new logging or
GPU probes added for the scoped-axis correction. One script writer publishes three
atomic floats and sequence; render reader makes at most two snapshot attempts,
never waits/spins without bound. Local x86 host100000 publish+read iterations average
0.091us/sample, checksum250000, concurrent no-torn-read stress passes. Host harness
is not optimized game timing and does not prove total render cost on the tester's PC.
The new candidate retains the earlier lightweight recorder; no claim of a measured
headset performance improvement. Source/geometry evidence: FLICKER_REFERENCE top.

## VR-229: diagnostic overhead and false draw stalls (2026-09-25)

Current returned build v1.0.1-6-g31450526c,3025x3135,shared wait0. The recorder's
largest measured finish/logging burst is0.523ms.3024 printed backbuffer sample
issue calls average0.003452ms,p95 0.005,max0.152; this excludes later maps,
D3D11 sampling and GPU synchronization. No matched off/on run exists. Therefore
the old full pixel diagnostic is NOT established to have negligible total cost.

New acceptance candidate keeps CPU history and mono/eye outcomes but forces
frame-id collection OFF, even with Perf.FrameId=1 in the unchanged saved INI.
All four GPU stages exit at the collection gate. Optional pixel investigation
now requires -FlickerDiagnostics -FlickerPixels; both flags default OFF and build.ps1
explicitly clears stale cached flags. Normal builds retain their saved FrameId
policy. Pixel opt-in without the recorder is rejected. No installed INI edits.

Actual recorder host benchmark:100000 synthetic presents,50 c5 uploads/present,
production history/event/formatter code, buffered file logging, recurring windows.
Mean1.660us/present; max finish0.867ms (rare historical-window printing). At144
presents/s the measured mean is about0.024% of one core. Includes recording and
format/file sink work, excludes game-side pose assembly and actual remote disk
behavior. Returned recorder max and host cost support low CPU overhead; they do
not prove an end-to-end FPS difference. Zero added pixel GPU work is enforced by
the candidate collection policy, which is tested even with INI request=true.

The source gate saved Present at draw return and ignored progress inside that
draw. Candidate measures entry-to-entry instead; an old-policy negative control
produces199 false stalls in200 ticks while the new policy produces0. Genuine
stalls still refuse. Additional doubled draws may change total rendering cost;
that is the intended removal of false mono interrupts, not logging overhead.
Evidence, counterprediction and limitations: FLICKER_REFERENCE.md, VR-229 top entry.

## VR-260 affected-player acceptance (2026-09-25)

The affected player reports the fix-only60bbd0afc candidate resolves the severe
performance issue. This is reported acceptance; no returned post-fix timings
were supplied. The pre-fix attribution and native validation below remain the
measured record. No merge or release is authorized by this result.

## VR-260: shared capture rejection and slow CPU fallback (2026-09-25)

Measured in support-20260925-235231-155-28068: log banner 1.0.1,
v1.0.0-8-gf5176aeae, RelWithDebInfo, legacy off; the install record agrees.
The current run uses VirtualDesktopXR 1.0.10, D3D9Ex and 2750x2850.
Do not combine its measurements with the older SteamVR shim log in the bundle.
The configured capture mode is shared, but the startup probe creates a standalone
render target with success and a null sharing handle. The following 0x80070006
is synthesized E_HANDLE; OpenSharedResource was never called. The probe rejects
sharing, leaving mode=sync throughout the measured windows.

Fourteen capture windows cover 254 grabs: weighted mean capture 149.927 ms,
including 145.555 ms in LockRect; window mean capture ranges 131.806-167.005 ms.
That is about 6.7 captures/s before other work, consistent with the reported
single-digit frame rate. This happens already on the mono startup/menu path.
D3D9 and the XR-selected D3D11 device have matching adapter LUIDs on the RTX5080.
The desktop mirror is off; native-present and xrEndFrame timings are small.
Focus is lost later, but the stall exists while FOCUSED. Neither a wrong adapter,
legacy input work nor ordinary stereo draw cost explains this capture stall.
The log's 3072 MB VRAM field is not evidence of actual RTX5080 capacity.

Source-confirmed weaknesses: probe and slots use CreateRenderTarget, whereas
Microsoft's D3D9/D3D11 interop contract specifies CreateTexture with pSharedHandle.
The probe also uses X8 when the real slots would first try A8, so a rejected probe
can prevent a supported format from being tried. Driver rejection of the old
resource/format is the leading explanation, not a remotely confirmed root cause.
Reference: https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-opensharedresource

Candidate uses one-level DEFAULT render-target textures and their level-zero
surfaces, with the same A8-first format rule in probe and slots. It retains the
D3D9 texture owner through capture and releases all views/surfaces/owners on reset
or partial failure. Logs name the exact failing step and do not misattribute a
missing handle to a D3D11 call. Existing fencing/delivery and fallback policy stay
as before; VR-114 fence timeout handling is separate.

Native x86 hardware test (no game): 162 independent D3D11 pixel checks after
alternating D3D9 colors and X8-to-A8 StretchRect, at 64x64 and 2750x2850; three
release/ResetEx/recreate cycles pass. Simulated success-with-null-handle rejects
before OpenSharedResource, and failed-open/unsupported-format cleanup passes.
Optimized x86 build (legacy OFF), repository lint and nine proxy exports pass.
This proves the candidate bridge works locally, not that the remote driver now
accepts it. Command: tools/shared-capture-native-host.ps1.

Next test, one question: with this candidate and the same saved settings, is the
startup/menu still limited to single-digit FPS? Collect support after about 30
seconds. Acceptance requires the candidate banner, shared texture AVAILABLE,
live shared slots and mode=shared with the huge readback cost gone. If sharing
still fails, the named API/format/HRESULT directs the next fix. If sharing works
but FPS stays low, attribute the remaining time from that new run. No game launch
or install was performed on the maintainer's machine.

## Post-merge intro and hub slowdown (2026-09-23, attribution open)

Reported: intro and hub rates fall into the 50s after integrating PRs105-110;
previous intro performance was reported at least around90. This report is from
the RTX4070 Ti SUPER tester, not the RTX4060 rig in VR-160.

Identity: installed DLL SHA256
717432F6AFC8EBAA200B7C936297069F7881FF533457BC6D81BFB95E77A9F993
matches the log banner756-g1726cee95, RelWithDebInfo, legacy off. Current and
previous736-gab7023884 logs both resolve2750x2850, VirtualDesktopXR, Quest3,
144Hz and desktop mirror off. Launcher/Layout candidate759 was not installed.
Archive: build/integrate-105-110/build/perf-last-run-20260923-024218, containing
both logs and the full installed CRLF INI before any subsequent run.

Measured on756, seconds from its first banner:

| Interval | Observation | Limit on interpretation |
| --- | --- | --- |
|37-100s, intro|Mostly53-76 ticks/s, 12.9-18.2ms/tick; drop context unknown throughout|Not a controlled comparison against an earlier intro run|
|88.3s|52.7 ticks/s,18.2ms/tick; P1 OUT11.9ms includes7.9ms render-thread idle; P2 OUT4.1ms includes1.2ms idle|Game-thread feeding is a material bottleneck; this does not identify the costly function|
|120.1s|Drop context first discovered|A level transition also occurred, so subsequent improvement is confounded|
|127-130s|115-120 ticks/s with context known|The merged build can still run quickly|
|134.6-137.6s stereo beats|About45-50 complete pairs/s,52-53 none/s; repeated no-present-since-previous-draw refusals|This is delivery cadence, not raw Present FPS; the interval precedes a loading transition|
|170-191s|About120-125 ticks/s with context known|Later slower windows also have a known context, excluding missing-context discovery as a complete explanation|

No VRAM-budget exhaustion is evident. Capture and xrEndFrame means are small
in the sustained slow intro block. Live-object hash rebuilds average about1ms
at roughly1Hz; the merged hash implementation does not show the old12ms sort
cost. MoveTrace, CamModProbe and Cine Trace resolve off.

Concrete source suspect: PR106 moves drop-context discovery from a20ms-gated
sample into the approximately10ms anim sample without a separate discovery
throttle. An unsuccessful call scans up to1024 GObjects slots, including
IsLiveObject and ObjClassName, whose readability checks call VirtualQuery.
The slow intro keeps known=0, so this repeats throughout. The old build also
scanned1024 slots while DropWatch=1, but at the lower cadence. This is increased
work established from the diff, NOT proof of the full FPS loss. In particular,
the later known-context slowdown needs its own attribution.

Next: measure or exclude discovery cost independently of cached decision
sampling, which must stay responsive for drop attacks. Compare the same save
and view; scene-to-scene rate changes are not a valid A/B. Preserve render size,
refresh and accepted gameplay behavior. Do not widen stereo hold windows or
relax scene gates to conceal the later delivery deficit. The latter signature
has prior context in VR-77 and FLICKER_REFERENCE's cadence routing, but no new
stereo correctness claim or change is made here.

The initial investigation was read-only. Tracking issue: VR-212. The subsequent
candidate and installation are recorded below.

## Menu cadence and camera-upload gate coverage (VR-178, 2026-09-22)

Verified combined650-g76ae6804a, optimized and legacy off. Preserved run:
build/playtest-candidates/menu-choppiness/run650. Source/fix scope and the
one-question headset test are in FLICKER_REFERENCE's VR-178 entry.

The journal's engine rate is high while stereo production is low. Between the
beats at50031250 and50034250, camera-silent skips rise3160->3472 (+312),
present-stall1464->1470 (+6), scene-state stays10573. The latter beat reports
129 draws/s,23 second draws/s,152 presents/s. Delivery at50033437 reports
L15/R15,mono61,none54 per second. These adjacent windows have different boundaries;
do not treat their differences as dropped-frame counts. Pause at50040250 reports
123 draws/s,119 second draws/s,242 presents/s; silent stays3543 from the prior
beat while stall rises1482->1492. Average FPS conceals intermittent stereo output.

The observed-upload exception only covers pause3; journal5/wheel6 throw away its
history. New separately opt-in MenuSceneFreshness retains actual during-draw c5
movement for less than100ms within the same menu epoch/load. Existing scheduling
guards and compositor hold remain. No desktop-output policy or pose-lock change.
72 policy cases and404 pairing checks pass. Headset cadence/left-hand verdict open.
This does not prove the older ReduceDesktopPresent hypothesis in VR-144, nor
attribute left-hand-only motion to the renderer before stereo continuity is tested.

## VR-180: a quarter-second freeze on every trigger pull was a legacy build (2026-09-22)

**Report:** pulling either trigger froze and stuttered the game, every time, fading after
some play. **Not a performance regression in any feature and not the machine:** the build had
`src/legacy` compiled in, and its projectile-spawn tracer walks every engine object for about
four frames after each trigger edge. `docs/TRAPS.md` has the whole account and the fix.

| Build, same source, eight trigger pulls on the simulator | `perf: frame gap` sat in `game_tick` | `spawn: NEW obj` lines |
|---|---|---|
| legacy ON (`614-gc7317261`, RelWithDebInfo) | 11, at 75 to 133 ms | 256 |
| legacy OFF (`614-gcce004d3`, RelWithDebInfo) | 0 | 0 |
| legacy OFF with the VR-180 guard (`600-gd556eb58` + the guard) | 0 | 0 |

In the affected headset log (VirtualDesktopXR) the same stalls read 76 to 88 ms per present in
runs of three. The guard build on the simulator at 2064x2208: 89.7 ticks/s against a 90 Hz
display, tick 11.1 ms. The guard adds nothing to the frame path: one word in the log banner
and one field in `status.json`.

**How to read this next time:** `sat in: game_tick` is the mod's own per-present work. A stall
there that lines up with an input is the mod doing something on that input.

## VR-160: the dev PC's 43-57 pairs/s - what the record already answers (2026-09-20)

**Report:** about 40 fps in the headset on the dev PC, while the sibling BioShock
mod is comfortable on the same PC. Decisions taken for this investigation: the render
size stays 3012x3122 (no resolution sweep), the headset test rate is 120 Hz, and
headset runs are few, so everything a log or the simulator can answer is answered
there first. No new run is claimed in this entry; every number below was already on disk.

### The populations (Q1). Do not combine them.

| Name | What it counts | Where it is read |
|---|---|---|
| ticks/s | game ticks per second. Under `reentry` one tick draws two scenes | `perf: tick N ms (X/s, ...)` |
| presents/s | calls to Present. Under `reentry` this is 2 x ticks/s | the same line, `Y presents/s`; `stereo: beat out/s` |
| fresh pairs/s | complete stereo pairs with both eyes renewed. Equals ticks/s ONLY while every tick draws both eyes | `stereo: beat L/s` and `R/s` |
| the streamer overlay's fps | application submissions as Virtual Desktop counts them | the overlay. WHICH of the above it matches is unverified; headset run 1 reads both in one marked window |

`L/s` is what the player perceives as smoothness. VR-152 showed it can fall 59 % while
the tick mean moves 13 %, because a tick that draws one eye is still a tick. In both
dev-PC logs below the armed invariant holds (`L/s == R/s == out/s / 2`), so here
ticks/s, pairs/s and `L/s` are the same number: 43-55. "40 fps" is that number.

### Two rigs are mixed in this file (the correction that matters most)

| | Rig A (the tester's) | Rig B (the dev PC) |
|---|---|---|
| GPU | RTX 4070 Ti SUPER (STATUS "The rig") | RTX 4060 (`adapter[0]: NVIDIA GeForce RTX 4060` in both logs), Ryzen 5 5600X |
| Runs in this file | builds 239-527: every 9.4-13 ms tick, the 0.64 ms/MP + 5.6 ms fit, "quarter pixels bought about 6 %", the 100-110/s judder report | `vr37-pre-existing.log`, `vr37-HEADSET-run1.log` |
| 3012x3122 judged in the headset | yes, 2026-09-15 | never |

Every headline number above this entry is rig A's. **No like-for-like regression on rig
B is established**: there is no earlier rig-B headset number at this size to regress
from. `arm-res.ps1` predicts 11.6 ms at 9.40 MP and rig B measures 18.3, so that fit is
not this card's. A measurement carries the identity of what it measured; this file
did not carry the machine, and from this entry on it does.

Rig B, active play only (tick < 60 ms; exact over every `perf: tick` line), VDXR,
90 Hz, `stereo reentry`, `res: HONOURED - the game renders 3012x3122`:

| Log | Build | Config | tick median / p90 | cap=lock P1 / P2 | out P1 / P2 | annotations |
|---|---|---|---|---|---|---|
| `vr37-pre-existing.log` 09-18 | `vr33-hands-working-434-g94202ce8` | UNKNOWN (the banner could not say) | 18.3 / 20.6 ms, 337 windows, 0 idle | 2.10 / 2.60 ms | 3.60 / 5.40 ms | 20 RENDER THREAD STARVED, 9 PACE-BOUND |
| `vr37-HEADSET-run1.log` 09-20 | `vr33-hands-working-540-g3051531e-dirty` | Debug (STATUS) | 23.0 / 24.2 ms, 214 active of 541 | 1.55 / 1.10 ms | 5.00 / 5.10 ms | 379 PACE-BOUND (mostly the idle block), 14 STARVED |

The 09-20 log's other 327 windows are the headset sitting idle: the runtime drops to
72 Hz (`hmd 13.89 ms`), ticks read 74-612 ms, and 10,487 of its 10,694 frame gaps sit in
`present-tail (xrEndFrame)`. The 72 Hz block is a cleaner idle filter than a tick
threshold. The 09-18 log never leaves 90 Hz and has no `present-tail` gap at all.

### This card's GPU cost was already in the log

`vr37-pre-existing.log`, active play: `perf: gpu/present render-to-entry=6.1-7.3 ms
capture=0.2-0.3 idle(d3d9)=0.0-0.1 | per tick span=12.1-14.6 dma=0.5 idle=0.1-0.2`,
341-355 of 341-355 resolved, 0 late, at about 57 ticks/s (17.5 ms tick).

Two scene renders cost about 12.3 ms of D3D9 GPU span per pair on rig B at 9.40 MP per
eye. That span alone is over 11.11 ms (90 Hz) and 8.33 ms (120 Hz); with everything
else free it caps rig B near 80 pairs/s at this size. Per this file's own evidence
rules a D3D9 span can contain feeding gaps, so 12.3 ms is an UPPER bound on GPU busy
time. Whether the card or the render thread owns it is the question headset run 1
decides, and it also decides where the other ~5 ms per tick can be won.

### Q0: the build config, static half

- The project sets no optimisation flags, so each config is the generator's default.
  Debug: `/Od /Ob0 /RTC1 /MTd`, `_DEBUG`, `_ITERATOR_DEBUG_LEVEL=2`. "Release" in every
  script means RelWithDebInfo: `/O2 /Ob1 /Zi /MT`. No `/GL` or LTCG anywhere. A true
  `/Ob2` Release is never built (`CMakePresets.json` has `debug` and `release` ->
  RelWithDebInfo only).
- `build.ps1`, `install.ps1` and the `xrsim-*` scripts default to Debug;
  `install-candidate.ps1` and `package.ps1` are always RelWithDebInfo. So tester builds
  were optimised and dev-PC installs were not, unless `-Release` was typed.
- The banner printed version, build id, `__DATE__ __TIME__` and no config. Two logs of
  the two configs differed only in the DLL's size (7.2 MB against 4.8 MB). There are no
  `_DEBUG` or `assert` blocks in `src/`, so Debug's cost is codegen, runtime checks and
  the debug CRT, all on the CPU side.
- **Fixed with this entry (the banner commit and the install-script commit):** the banner, the crash header and `status.json` name the
  config; an unoptimised build logs one `Warn` and tags its own `perf: tick` line
  (optimised lines are byte-identical to before, for the scripts that parse them);
  `install.ps1` prints the DLL's hash and a three-line warning for a Debug install. The
  default stays Debug by decision: the simulator workflow relies on it.

**Prediction, recorded before the simulator A/B runs** (Debug / RelWithDebInfo / Debug,
one save, one spot): Debug inflates `pre`, `tick`, `method` minus `lock`, `end`, and the
share of `out` that is our per-draw hooks. It does not move `cap lock` or the GPU span.
If Debug costs under 1 ms per tick on the simulator it cannot explain 23.0 against
18.3 ms, and those two runs differ for another reason (scene, or build 434 was Debug
as well). The unwelcome answer this can print: Debug is most of the gap, and half of
"40 fps" on 09-20 was the install script.

### Q5: nothing game-side caps the rate

Read from `DishonoredEngine.ini` `[SystemSettings]` and `[Engine.Engine]` on rig B:
`bSmoothFrameRate=FALSE` (so `MaxSmoothedFrameRate=130` is inert), `UseVsync=False`,
`MaxMultisamples=1`, `MaxAnisotropy=4`, `ResX/ResY=3012/3122`, `ScreenPercentage=100`,
`OneFrameThreadLag=True`, `DynamicShadows=True`, `MaxShadowResolution=800`. Neither log
mentions smoothing or vsync. The only pacer in the system is `xrEndFrame`.

### Q6: why BioShock Infinite is comfortable on the same card (architecture only)

From the sibling repo's own docs; no constant is carried over. Infinite uses the SAME
stereo bet: the scene-draw root called twice per tick, two presents per pair. On this
RTX 4060 it logs 77-80 pairs/s, at the runtime's native 2064x2208 per eye (4.56 MP),
and its record says it was already GPU over budget there before its settings pass.

> Infinite is comfortable because it draws less than half the pixels per pair on the
> same card (9.1 MP against Dishonored's 18.8 MP) and hands each eye to the runtime
> with one same-device D3D11 `CopyResource` and no fence. Dishonored is a D3D9 game, so
> each eye crosses to D3D11 through a fenced shared surface (`cap lock` 1.1-2.6 ms per
> present on rig B), and the 3012x3122 default was judged on a card with about twice
> the throughput.

Differences worth a written proposal, not code: Infinite holds ONE XR frame open across
both presents of a pair (one wait, one locate, one prediction, one `xrEndFrame`); its
per-draw detours cost one relaxed atomic load when idle (about 380 draws per present).
Dishonored already runs `xrWaitFrame` on a pace thread, as Infinite does.

### Suspects of ours, found by reading (none measured on rig B yet)

This table is the state BEFORE the simulator legs and is kept as written. Suspects 1 and
2 are decided in "The ranked table after the simulator legs" below. Suspect 3 is VR-162;
suspects 8 and 11 are VR-163.

| # | Suspect | Evidence | Status |
|---|---|---|---|
| 1 | The card at 18.8 MP per pair | GPU span 12.3 ms per pair, `idle(d3d9)` 0.1 ms; rig A is about 2x the card and ran about 2x the rate | open: headset run 1 (GPU engine utilisation beside the log) |
| 2 | Debug build | 09-20 was Debug; all CPU-side | open: simulator A/B, prediction above |
| 3 | `hkSetVSConstF` has no early-out and carries its view-model-hide block, its `DcNotePalette` call and its palette cache TWICE (`vs_const_hook.cpp` 219-256 and 307-361) | verified in source. It is the highest-frequency hook in the mod. `g_hmStaticWindow` is decremented twice per static upload, and two palette caches with different length rules write one buffer | open, and a correctness question before it is a cost question: ticket, measure with NativeProfile's `ConstHook` scope, do not delete blind (hands are headset-judged) |
| 4 | Instruments the shipped ini turns ON against compiled defaults of OFF: `[Hud] Regions=1`, `[Perf] NativeProfile=1`, `[Perf] BridgeGpu=1`; also `[Hands] DrawCensus=1`, `[Cine] Trace=1` | `probe_draw` locks a vertex buffer and hashes up to 8 KB per HUD-class draw; NativeProfile wraps about 20 D3D9 entry points; 15,771 `native-profile:`, 49,304 `cine/trace` and 17,402 `dc:` lines in the 09-20 run. DiagnosticAb's mask (about 1 % on rig A) never covered these | open: one grouped off/on/off rung |
| 5 | Ticks that draw one eye | 8,964 `reentry: gates -> DOUBLE draw after N single tick(s)` lines on 09-20, 6,944 on 09-18. VR-77 is this signature | open: count per minute in the standing-still windows of run 1 |
| 6 | `anim::weight()` takes an exclusive SRW lock, and `GetTickCount64`, on every draw, twice on a qualifying one (`draw_census.cpp:241`, `mesh_split.cpp:2991`) | read, not measured | open |
| 7 | The script lane: `PeHandler` has one early return; about 40 `strstr`/`strcmp` and a `RealName` per dispatch; `UiSurfaceTick` walks 1024 GObjects slots with `IsLiveObject` every 16 ms; an unresolved `CineTraceTick` re-runs about 11 full GObjects walks every 5 s | read, not measured. Lands in `out`, which is NOT "not the mod" (the VR-143 trap). Two log lines on this lane are now bounded (VR-160): `armfollow` change lines stop at 8 per field with the counters kept for `armfollow status`, and the `headtrack:` line is on a 3 s time gate instead of every 150th dispatch. Hygiene, not a rate claim | open |
| 8 | The HUD redirect's per-sink `StretchRect` and clears are charged to `end`, which the gap line names `present-tail (xrEndFrame)` | `frame_hooks.cpp` 242-256: `hudcap::end_frame` sits between the two stamps | an attribution fault in our own instrument; ticket |
| 9 | The frame-gap report | one ungated Info line per gap, plus a 16-record ring format and two more ungated Info lines, all formatted whether or not they print; about 10,500 of each on 09-20 | fires when the rate has ALREADY collapsed, so it is hygiene, not the 40 fps. FIXED (VR-160): the first 3 gaps of any 5 s window are itemised, the rest are counted and summarised once with their worst, their count >= 60 ms and their `present-tail` share; the ring, streaming and memory formats run only for an itemised gap and only when the line will print. Predicted effect on a run like 09-20: about 32,000 lines become about 1,500. Not a frame-rate claim |
| 10 | The log write path | NOT flushed per line: buffered stdio, `fflush` every 200 ms, format before the lock (`log.cpp` 29, 84) | eliminated as a per-line flush |
| 11 | `DVR_SKIP` | `dvr::diag::skip()` has one caller, the echo command. The env knob disables nothing, so the ladder rung that relies on it cannot run as written | fault; ticket. `DISHONORED_VR_XR_SAFE=1` and `[Mode] GamepadOnly=1` are the levers that exist |
| 12 | A game-side cap | smoothing and vsync off (above) | eliminated |

Not repeated, and why: nonblocking Present, the max-frame-latency sweep, the query-wait
helper, dynamic shadows via ini, FrameId-off and pair pacing as a default all failed on
rig A for reasons that do not depend on the card. The resolution route is the one whose
negative was rig A's alone; it stays closed here by decision, not by evidence.

### Headset run 1, the plan and its prediction (recorded before the run)

One launch, RelWithDebInfo, 120 Hz, 3012x3122, one save, one spot, standing still, with
GPU utilisation and the per-process GPU engine counters sampled beside the log. Marked
windows: `stereo reentry` 60 s, `stereo mono` 45 s, `stereo reentry` 60 s, the suspect-4
instruments off as one group 45 s, `hud off` 45 s, `stereo reentry` 60 s to close.

Prediction: the game's 3D engine reads above 90 % under `reentry`; `mono` runs at 1.8-2x
`reentry`'s rate; the instrument and HUD rungs sit inside the spread of the three
baselines. That outcome means the card is the limit at this size and the rest is a
written proposal. The unwelcome outcome the run can print instead: the 3D engine well
under 90 % with the span still near the tick, which means the render thread is starving
the card and suspects 3, 4, 6 and 7 are where the rate is.

Both halves of this were then answered on the SIMULATOR, which renders the same
3012x3122 (results below), so the headset run shrinks to one question the simulator
cannot answer: what the streamer's encode takes from the same card at 120 Hz.

### RESULTS, simulator lane, rig B, 2026-09-20 (Q0 measured half, Q2 rungs 3-4, the card's ceiling)

All legs: rig B (RTX 4060), the simulator (`dvr-xrsim`, 90 Hz, free pace), the newest save
(an interior with the hands drawn; a picture was looked at before any number), simulated
head fixed at `head rot 0 0 0`, `res: HONOURED - the game renders 3012x3122` (the SAME
render size as the headset; only the compositor's 1032x1104 capture is smaller),
`stereo reentry`, `[Capture] Mode=shared` (it is the default now; the 9Ex shared capture
is NOT off), 30 s settle then 38 windows of 3 s. Logs: `D:\dvr-data\logs\vr160-sim-*.log`.
Not headset performance: no encoder, no streamer, no compositor pacing.

**Debug / RelWithDebInfo / Debug** (medians over 38 windows each):

| Leg | Build, config, DLL sha256 | tick median / p90 | pairs/s | `R` P1+P2 | `cap lock` P1+P2 | GPU span per tick |
|---|---|---|---|---|---|---|
| A1 | `550-ged3621c8` Debug `BE1DE2B5` | 20.1 / 23.5 ms | 49.7 | 5.7 + 5.0 | 0.2 + 0.9 | 15.4 ms |
| B | `548-g3487c5d0-dirty` RelWithDebInfo (same source as 550) | 18.9 / 21.1 ms | 52.5 | 3.6 + 3.2 | 1.5 + 1.8 | 13.9 ms |
| A2 | `550-ged3621c8` Debug `BE1DE2B5` | 20.4 / 23.4 ms | 49.0 | 5.8 + 5.0 | 0.1 + 0.7 | 16.2 ms |

- **Debug costs 1.2-1.5 ms per tick here, 6-7 %.** It is real and it is not "40 fps".
- **The prediction was half wrong, usefully.** Debug does inflate the render thread's own
  time: `R` falls 3.9 ms per tick in the optimised build, which is our per-draw hooks and
  the engine's submission running under them. But `cap lock` did not hold still: it ROSE
  2.2 ms per tick and ate most of the saving. `lock` in shared mode is the wait on the
  previous present's blit fence, a GPU wait. Make the CPU side faster and the present
  thread simply arrives at the fence earlier. That is what a card-limited frame looks like.

**`stereo reentry` against `stereo mono`, with the game's 3D engine utilisation** (Windows
`GPU Engine` counter for the game's process, 1 s samples stamped with the log's clock and
joined to the 3 s perf windows; RelWithDebInfo `550-ged3621c8`, DLL `6EE3CD19`):

| Method | scene renders/s | pairs/s | GPU 3D engine | GPU-busy per scene render |
|---|---|---|---|---|
| `reentry`, fast phase (idle 0.4 ms) | 108-112 | 54-56 | 86-87.5 % | 7.9-8.0 ms |
| `reentry`, slow phase (idle 7.5 ms) | 91-93 | 45-47 | 72.7-73.4 % | 7.9 ms |
| `mono` (PACE-BOUND at the simulator's 90 Hz) | 90 | n/a | 72.4-74.8 % | 8.1 ms |

- **The card's ceiling at this size is about 62 pairs/s.** Three independent rows give the
  same 7.9-8.1 ms of GPU-busy time per scene render at 3012x3122 on the RTX 4060, capture
  copies included. 1000 / 8.0 = 125 renders/s = 62 pairs/s at 100 % utilisation. This is an
  instrument that could have failed its own hypothesis: had `mono` and `reentry` disagreed,
  or had the slow phase shown a different cost per render, the counter would not have been
  measuring render work. They agree to 3 %.
- **What it means for the asked rates.** Two full scene renders per displayed frame need
  240 renders/s at 120 Hz (1.92 s of GPU per second), 180 at 90 Hz (1.44) and 144 at 72 Hz
  (1.15). None fits in one second of this card at 9.40 MP per eye. The size at which 90 Hz
  fits needs about 5.5 ms per render; how render cost scales with pixels ON THIS CARD is
  unmeasured (the 0.64 ms/MP fit is rig A's) and stays unmeasured by decision.
- **The recorded prediction for headset run 1 is REFUTED in its wording and confirmed in
  its substance**: the 3D engine never reads above 90 % (max 87.5 %), and the limit is
  still the card. 86 % busy at 55 pairs/s against a 62 ceiling means everything of ours
  that starves or stalls the card is worth at most 12 % in the fast phase. The earlier
  "about 80 pairs/s" ceiling from the 12.3 ms D3D9 span was too generous: the span stops
  at Present entry and leaves out the capture blit and the D3D11 side.
- **The headset will read LOWER than the simulator**, not higher: Virtual Desktop's encode
  and colour conversion share this card. 09-18 in the headset was 54 pairs/s median with
  no slow phase; the simulator's fast phase is 55.

**A 12-13 s cycle on the game thread, found by looking at the series instead of the
median.** Every leg alternates about 6 s of tick 17.5-18.5 ms (`idle` 0.4, render thread
never waits) with about 6 s of tick 21-24 ms (`idle` 6-8 ms, `RENDER THREAD STARVED`),
15-20 of 38 windows. Its size is the same in Debug (7.0-7.9 ms) and RelWithDebInfo
(6.8-8.1 ms). Our script-lane code is several times slower under `/Od`, the engine's is
the same binary in both, so **this cycle is not the mod's code**: it is the engine's or
the level's (a looping scripted sequence is the likely owner). No log line's rate differs
between the slow and fast windows (`tools\perf-tick-cycle.py`, 14 slow against 17 fast). It costs this
save about 9 % of its mean rate. The 09-18 headset log, on a different spot, shows 20
STARVED windows of 337, so it is content, not a constant. Not pursued further: the mod
cannot shorten the engine's game thread, and VR-143's trap is respected (the claim rests
on the Debug A/B, not on `out` being "not ours").

One hitch IS ours and is Debug-only: `perf: frame gap 41-44 ms ... sat in: game_tick`,
a present that spent 19-22 ms in `DvrGameTick` and 36-38 ms inside our hook, about once
a second in the slow phase. 22 of them in leg A2's gameplay, 13 itemised in A1's window, and
**none in leg B**: the RelWithDebInfo measurement window has zero frame gaps of any
owner. So it is a cost of the unoptimised build, it is part of why a Debug build feels
worse than its 6 % median says, and it needs no ticket beyond the banner's warning.

**The frame-gap change works as designed**: a window with 5 gaps printed 3 itemised and
`perf: frame gaps - 2 MORE in the last 5 s not itemised ... worst 41 ms sat in: game_tick,
0 sat in present-tail (xrEndFrame)`. The banner reads `config Debug` / `config
RelWithDebInfo`, the Debug `Warn` prints, and the Debug tick line carries its tag.

### The ranked table after the simulator legs

| # | Suspect | Measured cost | Status | Decided by |
|---|---|---|---|---|
| 1 | The card at 9.40 MP per eye, two scene renders per frame | 8.0 ms GPU-busy per render = a 62 pairs/s ceiling; we run at 86 % of it | CONFIRMED (simulator). The headset number with the encoder on the same card is owed | `mono` vs `reentry` vs the slow phase, three agreeing rows |
| 2 | The engine's 12 s game-thread cycle on this save | 6-8 ms per tick for half the time, about 9 % of the mean | CONFIRMED not ours; content-dependent | identical in Debug and RelWithDebInfo |
| 3 | Debug build | 1.2-1.5 ms per tick, 6-7 % | CONFIRMED, and fixed as a trap (banner, Warn, install line) | A1 / B / A2 |
| 4 | Everything of ours on the render thread and the present path (suspects 3, 4, 6, 8 above: the doubled constant hook, the shipped-on instruments, the per-draw SRW lock, the HUD pass) | bounded above by the 12 % between 55 and 62 pairs/s, all of them together | OPEN, bounded. Worth doing for the 12 %; cannot reach 72 Hz, let alone 90 or 120 | the GPU utilisation rows |
| 5 | `game_tick` 20 ms spikes at about 1 Hz in the slow phase | about 4 % of that phase, Debug only; zero gaps of any owner in the RelWithDebInfo window | CONFIRMED a Debug-build artifact | the itemised gap lines, A1/A2 against B |
| 6 | One-eye ticks (`gates -> DOUBLE draw after N single`) | not measured in these legs (`L/s == R/s`, `none/s=0` throughout) | OPEN for the headset logs only | - |
| 7 | The log | not flushed per line; 3 lines per gap now bounded | ELIMINATED as a rate cost; hygiene fixed | source + the summary line |
| 8 | A game-side cap | none | ELIMINATED | the ini |

**What this says about the architecture (proposal material, no code):** at this size on
this card the only routes to the headset's rate are fewer pixels per scene render, or
fewer scene renders per displayed frame. The first is a settings decision that belongs
to the player and was set aside for this investigation. The second is what this file
already lists as unbuilt: alternate-eye rendering (one fresh eye per displayed frame,
with its temporal mismatch) or a reprojected second eye. Neither is a fix to make inside
a performance pass. The honest sentence for rig B today: **3012x3122 per eye is a
4070-Ti-SUPER-class setting; on the RTX 4060 it renders 54-56 pairs/s at best and no
change to the mod's own code can lift that past about 62.**

### RESULT, headset run 1, rig B, 2026-09-20 (the streamer's share, and a retraction)

Rig B (RTX 4060), VDXR at 120 Hz (`hmd 8.33 ms = 120.0 Hz`), `res: HONOURED - the game
renders 3012x3122`, `stereo reentry`, build `vr33-hands-working-550-ged3621c8`, `config
RelWithDebInfo`, DLL sha256 `6EE3CD19`. The newest save, standing still, 46 windows of 3 s
after the last `state: GAMEPLAY`. Log `D:\dvr-data\logs\vr160-HEADSET-rel-120hz.log`, GPU
samples `vr160-headset-gpu.csv` and `vr160-headset-gpu-all.csv`. The streamer's overlay
read 45-50 fps in the same minutes.

| | tick median / p90 | pairs/s (`L/s`) | `R` P1+P2 | `cap lock` P1+P2 | `tick` field P1+P2 | GPU span per tick |
|---|---|---|---|---|---|---|
| Headset, 120 Hz | 22.1 / 22.3 ms | 44.7 (45) | 5.1 + 6.1 | 2.1 + 2.1 | 1.2 + 1.2 | 17.4 ms, `idle(d3d9)` 0.0 |
| Simulator leg B, for scale (a different view) | 18.9 / 21.1 ms | 52.5 | 3.6 + 3.2 | 1.5 + 1.8 | 0.8 + 1.0 | 13.9 ms |

GPU engines over the same 113 s: the game's 3D engine **76.1 %**, the streamer's video
encode engine 21.3 % (its own silicon), the streamer's 3D engine 6.8 %.

- **Q1 settled: the streamer's overlay counts stereo pairs.** It read 45-50 while the log
  read 44.7 pairs/s and 90 presents/s. "40 fps" is pairs per second.
- **The rate half of the prediction held** (at or under 54: it is 44.7). **The
  utilisation half is REFUTED**: the game's 3D engine was predicted at or above the
  simulator's 86 % and reads 76 %. 76.1 % over 89.4 scene renders/s is 8.5 ms of GPU per
  render (8.0 on the simulator; the streamer's colour conversion shares the card). With
  the streamer's 6.8 % set aside, the card's ceiling in the headset is about 93 % / 8.5 ms
  = 109 renders/s = **about 55 pairs/s**, and the game runs at 82 % of it.
- **So in the headset the render thread is the co-limit, more than on the simulator.**
  The render thread never waits for the game thread (`idle` 0.2, no STARVED window, no
  12 s cycle in this spot) and never waits for the runtime (`wait` 0.0, 0 PACE-BOUND).
  It spends 11.2 ms per tick in `R`, 6.6 ms in our present path outside the fence, and
  4.2 ms at the fence. The CPU-side share of that (our per-draw hooks inside `R`, the
  2.4 ms `tick` field, the 2.4 ms `end` field) is what the 18 % of idle card can be
  bought with: **at most about 44.7 -> 55 pairs/s**, and not past it. The ceiling
  sentence stands with a smaller number: no change to the mod's code lifts this card past
  about 55 pairs/s in the headset at this size.
- The tick is flat to 0.2 ms across all 46 windows, with nothing pacing it. That is a
  steady scene on a steady load, and it makes this spot a good A/B bench.

**RETRACTION.** "The `game_tick` hitch is Debug-only and needs no ticket" (the simulator
results above) is withdrawn. The optimised build in the headset shows 12 of them in 135 s:
`perf: frame gap 40-51 ms ... sat in: game_tick`, irregular, three times as a pair one
second apart. The simulator's optimised window had none, so the owner is something the
headset lane runs and the simulator lane does not, or runs cheaper. No log line clusters
before them (`ledger:` lines follow a gap, they do not cause it). Open, tracked on VR-160 (no separate ticket, by decision). Also seen
and not pursued: 112 `reentry: gates -> DOUBLE draw after N single tick(s)` recoveries in
the same 135 s, about 0.8 a second, with `L/s == R/s` holding at the 3 s scale (VR-77).

Measured in passing, for the Q3 table: `draws/regions` 4035 probes per 3 s at 0.8 us per
probe = 1.1 ms/s; NativeProfile's `DrawIndexedPrimitiveUP-hook-inclusive` 26,000 calls per
3 s at 0.8 us sampled mean = 6.9 ms/s. Both are under 1 % of wall time. The shipped-on
instruments are therefore NOT the 18 %; what is left is the hooks on the indexed draw
path, the constant hook (VR-162) and the present path's own 2.4 + 2.4 ms per tick.

### RESULTS, round 2, rig B, 2026-09-20: four costs of ours on the present thread, and the size test

`perf parts on` (new, default off) splits the present path into named parts. It named three
of these in three simulator legs; the fourth came from `hud off` against `hud on`.

| # | Cost | Where | Measured | Fix |
|---|---|---|---|---|
| 1 | The video-memory sampler (added 2026-09-18) | `gpu_memory::tick`, present thread | gated to 4 Hz as designed, but one sample is two kernel video-memory queries plus a `VirtualQuery` walk of the whole 32-bit address space: 25.8 ms on the simulator, 27.6 ms (max 46.9) in the headset. 0.8 ms per present averaged = four stalls a second. This is the `sat in: game_tick` frame gap, and it explains the retraction above: it is a wall-clock cost, so the optimised build has it too | its own low-priority thread at 1 Hz; it prints its own cost; `[Perf] GpuMem=`, `gpumem on|off` |
| 2 | The HUD panel, twice per displayed frame | `hudcap::end_frame` | per in-use element, per PRESENT: a full 3012x3122 `StretchRect`, a full-size clear, a D3D11 draw and a `Flush`. `hud off` / `hud on` / `hud off`, fast phase: 16.0 / 17.8 / 16.0 ms per tick (62 against 56 pairs/s) | `[Hud] OncePerPair`, `hud pair on|off`: hold the first present of a pair (clear only, last output stays delivered, never two holds in a row) |
| 3 | `status.json`, once a second | `status::tick`, present thread | file create, write, close and rename on the present thread | built on the present thread, written by a worker; the present thread never waits for the disk |
| 4 | The live-object table | `BuildLiveSet` | a copy and `qsort` of about 115,000 pointers with the table's lock held throughout, from five independent periodic timers on two threads | copy and `std::sort` into a scratch buffer with no reader lock, swap under the lock; `RefreshLiveSet(maxAgeMs)` lets periodic callers share one rebuild |

Simulator, fast phase, same save and view, RelWithDebInfo, 3012x3122: **17.8 ms (56 pairs/s)
before, 15.7 ms (63.7 pairs/s) after**, the game's 3D engine at 93 %, zero frame gaps in the
window. `OncePerPair` off / on / off on its own: 16.9 / 15.8 / 16.9 ms; `hud/beat` reads
`held` = presents / 2, deliveries halved, `empty-while-armed=0`.

**Headset run 2** (VDXR 120 Hz, 3012x3122, build `vr33-hands-working-557-g02d34c1e`, `config
RelWithDebInfo`, DLL sha256 `BF472731`, the same save, standing still, 19 windows per leg;
log `vr160-HEADSET2-fixes-120hz.log`):

| Leg | tick median / p90 | pairs/s | our present path (`in`, P1+P2) | frame gaps |
|---|---|---|---|---|
| Headset run 1, before the fixes | 22.1 / 22.3 ms | 44.7 | 10.5 ms | 12 in 135 s, all `game_tick` |
| Fixes, `OncePerPair` off | 20.6 / 20.9 ms | 48.3 | 8.2 ms | 1 in the whole run |
| Fixes, `OncePerPair` on | 19.9 / 20.1 ms | 49.7 | 5.0 ms | (same run) |

The game's 3D engine 81.8 %, the streamer's 3D 7.3 % and encode 23 %: 8.2 ms of GPU per scene
render, a headset ceiling near 56 pairs/s, and the game now at 88 % of it (82 % before).
Hands healthy (`hands OWNER=SkelControl writes=302/3s`). The player reported no HUD flicker
with `OncePerPair` on and an overlay reading of 45-52, and could not judge smoothness
standing still. On that verdict the missing-key default of `[Hud] OncePerPair` is now 1
(no config-version bump, so no ini is rewritten: VR-159). Scope of the judgement: about a
minute, standing, three elements; the lever stays for anyone who sees otherwise.

**The size test, because the question was whether something deeper is wrong.** One
simulator leg at the sibling mod's 2064x2208 per eye (4.56 MP), same build, same save,
`res: HONOURED - the game renders 2064x2208`. Prediction recorded before it: 70-85 pairs/s,
and a rate still near 50 would mean a deeper fault.

| Size per eye | fast phase | GPU 3D engine | slow phase |
|---|---|---|---|
| 3012x3122 (9.40 MP) | 15.7 ms, 63.7 pairs/s | 93 % | 20-21 ms, 48-49 pairs/s |
| 2064x2208 (4.56 MP) | 11.1-11.9 ms, 84-90 pairs/s, touching the simulator's 90 Hz pace | 63-67 % | 20.3 ms, 49 pairs/s |

CONFIRMED: on the render side the pixel count is the limit on this card, and at the
sibling's size the same code reaches the sibling's class of rate with a third of the card
idle. The four files `arm-res.ps1` writes were restored byte-for-byte afterwards.

**And one thing the size test exposed.** The SLOW phase of this save's 12 s cycle does not
move with the size at all: 20.3 ms at both. There the game thread needs about 20 ms per
tick and nothing on the render side matters. The earlier reading "identical in Debug and
RelWithDebInfo, so not the mod's code" is WEAKER than it was written: script-lane code that
spends its time in `VirtualQuery` (every `RangeReadable`) costs the same in both configs,
so that A/B could not have told such code from the engine's. OPEN, and the next test is
direct: the same leg with the ProcessEvent hook not installed. The headset spot shows no
such phase (`idle` 0.2 ms), so this is about other places in the game, not that one.

## Reboot/save comparison and resolution check (2026-09-19)

After a PC restart and a known-good save, the tester reports performance close
to the prior baseline with a possible small residual slowdown. These two changes
were combined; neither is independently established as the cause or fix.
A remembered VD resolution percentage changed from about102 to112, prompting an
actual-dimension audit. Latest verified build486 starts at tick107093; DLL hash
34bc3cb31eb540190a4c0dd2291ac47a0be7a325e9b871622913cabd50969c3d.
Artifacts: build/playtest-candidates/hud-improvements/run486-after-reboot.

Latest and original accepted486 both create3012x3122 game targets and eye
swapchains, with VDXR recommending2688x2880. Archived464,470,473,476,483 and490
also have those same dimensions;480 additionally has its already documented
accidental1355x1405 reset. VDXR currently reports1.000 supersampling and1.000
upscaling.3012/2688 is1.1205, numerically consistent with the reported112 percent;
this is an inference about the overlay number, not verified overlay semantics.
No logged evidence supports a new render-resolution increase across these runs.
The remembered102 percent remains unverified. No resolution setting changed.

Before the reboot report, the streamer was restarted with the game closed and
without settings changes; prior process27772 was replaced by32024 at12:15:02.
Its effect was not tested separately. The poor run preceding that restart is
archived as run486-rollback. Do not call the reboot, save, streamer restart or
resolution a confirmed explanation of the earlier regression.

## Rollback489 performance regression and accepted486 baseline (2026-09-19)

Tester reports poor performance from launch after the rollback. Verified489
banner and installed DLL71540df1; archived full run in
build/playtest-candidates/hud-improvements/run489-rollback. Runtime is VDXR1.0.10,
3012x3122,120Hz, matching accepted486. Whole INI comparison with pre490 differs
only in explicit VDXR selection and reticle appearance. Build optimization flags
and generator instance match the main checkout. No cause established.

Whole-session descriptive comparison (different lengths/scenes, not a controlled
benchmark): accepted486 5.6min,97 tick samples, median9.4ms,p9010.6ms,5.5 gaps>=60ms/min;
rollback489 36.1min,626 samples, median10.7ms,p9016.0ms,11.4 gaps>=60ms/min.
489 logs575 frame-gap events attributed to xrEndFrame across all gap lengths;
486 has none in that category. This locates waits, not their underlying cause.
VDXR's own log has no reported error. SteamVR processes were absent after exit.

Correction to rollback target:489 was the immediate pre-split source, while486
was the last accepted pre-split playtest. Rebuilt3ec56e3bc in isolated checkout,
froze rollback-486 with copied503 installer and dry-run, and installed it with
its entire archived install486 INI except explicit nativeVDXR selection.
Hash34bc3cb31eb540190a4c0dd2291ac47a0be7a325e9b871622913cabd50969c3d;
banner vr33-hands-working-486-g3ec56e3bc. This is a rebuild, not the original DLL.
Full installed INI diff removes reticle colour keys and restores SizeDeg0.500;
CRLF1249/1249. Prior logs preserved. Next question: does performance return to the
previous baseline immediately after loading? Improvement implicates the intervening
reticle change or session state; no improvement leaves runtime/streamer/system
state and the rebuild comparison open. No claimed fix, launch, or merge.

## Vitals-first candidate and pending desktop-present A/B (2026-09-19)

Candidate505 implements the selector/model-draw diagnostic stage of
CODEX_PLAN_VITALS_CHOKE.md. It leaves ReduceDesktopPresent unchanged so the
first question remains model drawing. VR-144 still needs the independent
ReduceDesktopPresent=0 comparison with run499's 28% wheel singles/writes.
No new wheel measurement or performance verdict is claimed.

## Wheel stutter and the crouched-load stand-up stall (2026-09-19, VR-144, VR-143)

- VR-144, the weapon wheel stutter: the share of one-eye ticks while the wheel
  is open (`menu/head ... singles/writes`) was 10% (run470), 25% (run486), 47%
  (run497) and 28% (run499). Build 497 took a head-pose lock on every hand draw.
  Build 499 made it once per present, only while attaching, and the ratio fell
  back to 28%. Remaining suspect: `[VR] ReduceDesktopPresent`, 0 -> 1 between
  run473 and run476, when the ratio first rose. Next: an A/B.
- VR-143, the stand-up stall after loading a crouched save: run499 loaded
  crouched and stood at 19399687. The next perf tick read 20.5 ms at 48.6/s,
  against 9.5 ms at 95-106/s normally. The excess is `out` idle 10.7 ms, outside
  the mod's present path, and the mod's hook scopes were no higher than
  elsewhere. The load window shows 11996 TexLockRect calls in 3 s, so texture
  streaming and the device shadow copy are the first suspects. Not fixed.

# Performance research

## Periodic xrEndFrame hitch: measured, not yet explained (2026-09-18)

**Report:** a large frame drop every 5-10 s, believed to happen while walking.

**Measured in the build464 run** (`vr33-hands-working-464-g0b7171bd1`, 120 Hz,
VDXR, 3012x3122, mirror-off/strict; logs in
`build/playtest-candidates/weapon-mirror/run464`):

- 363 of 366 gameplay frame gaps sit in `present-tail (xrEndFrame)` on the +1
  (pair-closing) present, 60-107 ms, typically 88-94 ms (about 11 display slots).
  Bursts of 1-7 such stalls about 100 ms apart.
- Burst starts sit on a ~4.0 s beat that jitters by about +-1 s (consecutive
  intervals pair up to ~8 s: 3016+5297, 2906+5250, 2625+5453). Peak of the
  interval histogram 4.0-4.5 s.
- **Not movement.** Burst rate while moving (>= 30 uu/s) 12.6/min vs still
  14.3/min; head turning >= 20 deg/s 14.5/min vs calm 14.2/min (458 run the
  same: 11.8 vs 12.0). Bursts also occur with the pause menu open (13 in 464).
  Walking makes a 90 ms freeze visible; it does not cause it.
- **Not the game's draw cost.** In 146 of 154 gap rings the game's own GPU time
  per present is normal (median max 4.7 ms); the last presents' timing queries
  are pending. Only 8 rings show a present over 20 ms.
- **Not the texture-streaming change.** The 452 run from before the
  NumStreamedMips=-1 change (07:34) already had 16.9 stalls/min >= 60 ms.
- **Not phase-locked to the mod's 3 s diagnostic beat** (burst delay after the
  capture beat is spread evenly over 0-3 s). No runtime period change all run.
- **Not the capture queue.** Blit-fence timeouts are 5-7 per run (lifetime);
  20-50% of grabs wait on the fence, so D3D9 queue depth is bounded by the
  capture ring.
- **It got worse over the last builds.** Stalls >= 60 ms per gameplay minute,
  per archived build (noisy, sessions differ): 353-395 1.6-6.6; 397 18.1,
  399 10.1; 407-427 1.7-6.9; 431-448 5.3-11.2; 452 10.3-16.9; 458 15.6;
  464 29.7 (>= 80 ms: 19.2). Same ~90 ms signature throughout, so one
  mechanism fired more often, not a new one. 90 Hz runs (239-264) sat at the
  same low rates as early 120 Hz runs, so the display rate is not the driver.

**Hypotheses left, each with the instrument that can kill it (build465):**

1. Video-memory paging (32-bit process, 4096 texture pack, ~3.3 GB of texture
   creations per census, 3012x3122 targets): `gpumem` samples DXGI
   QueryVideoMemoryInfo LOCAL/NON_LOCAL usage vs budget for the D3D9 adapter at
   4 Hz, with process private bytes and the largest free address range; logged
   every 10 s, on any NON_LOCAL move of 64 MB, and at every frame gap.
   Prediction if paging: usage at/over budget or NON_LOCAL moving at the stalls.
2. Texture-streaming uploads through the Managed=shadow twins (every streamed
   mip is an UpdateSurface): `device/stream` logs the last 2 s in 100 ms buckets
   (uploaded MB / created MB, CPU ms inside UpdateSurface) at every frame gap.
   Prediction if streaming: a burst of MB in the buckets just before a stall.
3. The runtime/streamer side (VDXR/VD encoder or network): what remains if
   both of the above read flat at the stalls. The VD performance overlay
   (network latency, encode) during a burst would then be the next evidence.

No mitigation shipped: pacing to a fixed rate was already tried (2026-09-15)
and felt laggier; nothing else is justified until one hypothesis survives.

## Status: accepted FOV/mirror improvements merged; broader research shelved

The earlier research established a substantial
resolution-independent rendering cost, ruled out several cheap fixes, and measured
ordinary per-eye scene preparation. It did **not** establish a safe way to eliminate
that work or a single CPU/GPU bottleneck. HUD is outside this investigation.

This is the single maintained performance record. Update findings, failed hypotheses,
sources and resumption decisions here. STATUS/NEXT_SESSION only summarize and link.
Engine derivations remain in [ENGINE_NOTES.md](ENGINE_NOTES.md); stereo correctness
and pose history remain in [FLICKER_REFERENCE.md](FLICKER_REFERENCE.md).

Unfinished performance tickets VR-17, VR-67, VR-77, VR-113, VR-115, VR-121, VR-123,
VR-124 and VR-125 are parked in Backlog, unassigned. Completed stability fixes stay
completed. Merging this research does not promote experimental settings.

## Active exception: F11 clarity, FOV and desktop presentation (VR-50, 2026-09-15)

The broader optimization program remains shelved. A new headset observation reopened
VR-50 only: toggling F11 twice yielded a sharp, smooth, smaller square view with stereo
depth and normal head/walking response. This is a reported perceptual improvement,
not a controlled FPS result or proof of a new anti-aliasing mode.

Verified build307 DLL and log banner. Both logs and installed configuration are in
`build/performance-results/f11-discovery-20260915-201742`. Native F11 toggles engine
fullscreen/windowed; the mod's VirtualMode converts fullscreen requests to windowed
while retaining the requested backbuffer dimensions.

| State | Backbuffer / XR swapchain | Horizontal FOV |
|---|---|---|
| Before F11 | 2750x2850 | 108.07 degrees |
| First F11 | 1355x1405 | Falls gradually toward 75 degrees |
| Second F11 | 2750x2850 | 74.89 degrees, remains narrow |

Both source bounding boxes remain full-size. Projection layers stay enabled, runtime
quad stays off, and eye separation remains about 6.8 uu. This was not a mono cinema
screen or an embedded low-resolution rectangle. Immediate Present was applied before
and after the resets, so this did not newly unlock vsync.

**Specific boundary:** the persistent FOV lever's natural-base cache had recaptured its
own widened 108-degree output. It then multiplies the sensor by target/natural on every
script dispatch. The small aspect change (2750/2850 versus 1355/1405) makes the target
slightly smaller; feedback can repeatedly narrow the FOV. Restoring the original aspect
makes the ratio approximately one and retains the narrow result. Logs show 3,710
natural-base captures, with the later 3,709 recapturing about 108 before F11. The old
assumption that this recapture was harmless fails when the target changes.

At the restored width, central density is about 31.34 pixels/degree at 74.89 degrees,
24.00 at 90, and 17.41 at 108.07. Thus the narrow view has roughly 1.80x the normal
central linear density; 90 offers about 1.38x with more angular coverage than 75.
These are projection arithmetic, not optical headset resolution or added AA samples.
A narrower frustum may also reduce visible work, but moving-view tick windows averaged
74.67 before, 85.8 at lower resolution, and 77.63 after restoration. Different views
prevent a causal performance claim. Do not repeat the failed query-helper hypothesis.

**Candidate:** `vr33-hands-working-353-gf0fa9fef4-dirty`, archived under
`build/playtest-candidates/vr50-projection-fov90`, installed with `[Screen] ProjectionFov=90`.
The explicit request makes 90 the runtime, missing-key, generated and packaged default;
0 or live `projectionfov off` restores the old headset-derived route. Valid range 60-120.
It uses the existing temporary reflected CameraCache.POV.FOV scope around both eye draws,
with proportional tangent-space zoom, identity/liveness validation and restoration.
The persistent ratio writer is suspended during the gameplay scope, avoiding feeding the
temporary narrow FOV back into itself. Cinematic handling retains precedence. This does
not repair all legacy natural-base behavior, especially after another F11/aspect change.
No image-owned orientation or stereo-pair synchronization policy was changed.

Build and standalone tests passed: 30,045 FOV/restore checks including 10,000 repeated
scopes, frame math, 248 reentry checks, 23 single-tag checks, 9 exports, golden INIs and
lint. No game/simulator launched. Final rendered acceptance and comfort remain untested.
Both logs, prior DLL and INI archived at
`build/playtest-candidates/installs/20260915-203356-472356` before replacement.
The full installed INI comparison adds only ProjectionFov=90; existing saved HUD and hand
trim changes since the old manifest are preserved. CRLF and installed hashes verified.

**First build353 headset result:** improved appearance versus108 reported, but the
rectangular boundary remains visible. Verified353 log shows gameplay submission90.00
at2750x2850 and camera scopes108.07 ->90 with zero refusals through5,531 writes.
This validates reported clarity improvement and scoped submission, not a complete
world-matrix audit or a measured performance gain. The remaining request is to enlarge
the angular presentation of the90-degree image. A projection layer has no screen-distance
parameter; widening only its submitted frustum magnifies the image and mismatches rendered
rays, potentially changing head-motion gain and stereo geometry. A stereo quad/screen is
a different presentation mode and would need explicit design/testing. Do not silently
replace the accepted projection with that mode. Evidence: `build/performance-results/vr50-fov90-headset-20260915-203910`.

**Live slider follow-up:** build355 (`vr33-hands-working-355-gf8380e1e3-dirty`) is now
installed from `build/playtest-candidates/vr50-fov-slider`. F10 -> View exposes Custom
gameplay FOV, a live60-120 slider and Reset FOV to90. Existing Save As Defaults persists
it. This is UI over the353 setter, with no new render behavior. Default remains90.
Build, exports, lint and diff checks pass; UI/headset operation is not yet validated.
Both logs and prior files archived at
`build/playtest-candidates/installs/20260915-204101-720438`; complete INI diff is empty,
installed hashes and CRLF verified by installer. No game/simulator launched.

**Build355 acceptance and new default:** the user reports100 degrees removes the visible
black rectangle and retains a substantial apparent clarity improvement. The verified355
log records live slider changes and100.00-degree submission at2750x2850. This confirms
slider operation and the preferred FOV; it does not measure a resolution increase from
FOV alone. Accepted evidence is archived under
`build/performance-results/vr50-fov100-accepted-20260915-205050`.
100 is now the runtime, missing-key, generated/package and F10 reset default.

**Current candidate:** build356 (`vr33-hands-working-356-g0b1b9ca55-dirty`), installed
from `build/playtest-candidates/vr50-fov100-pixels110`. User clarified the resolution
control should represent TOTAL PIXELS, superseding the initial per-axis interpretation.
100% always means2750x2850;110% produces2884x2989 (109.988% after pixel rounding).
Both axes scale by sqrt(percent/100), preserving aspect within half-pixel rounding per
axis. F10 -> Display -> Total pixels (%) offers50-200%, previews width/height, and
Set for next launch saves via the existing ResRequest path. Dragging alone changes
nothing. Settings survive restart without Save As Defaults. No new live reset is
introduced: prior engine setres tests were inert. The mod INI is authoritative at the
next launch and reconciles its launch-argument mirror automatically.

100-degree FOV is accepted;110% resolution is a new unaccepted trial. It adds about10%
pixels, not21%. The uninstalled per-axis110% draft was superseded before installation.
Build, exports, golden INIs, lint and arithmetic checks passed. Both prior logs/DLL/INI
archived before install at `build/playtest-candidates/installs/20260915-205309-560273`.
The full installed INI comparison changes only RenderWidth2750->2884 and
RenderHeight2850->2989; saved100-degree FOV and all other settings are preserved.
CRLF/hashes verified. No game/simulator launched; UI operation and rendered size await
headset/log verification.

**Hub mirror-off discovery (verified356,2026-09-15):** the user reports an approximately
30-40% FPS improvement in the slow hub area after disabling the desktop mirror in F10.
Earlier sewer experience showed little perceived benefit. This is an area-dependent
headset observation, not a new matched A/B measurement or proof all performance issues
are solved. Earlier controlled sewer captures did show throughput gains with worse
frame-time tails; preserve those results rather than treating either scene as universal.
Evidence: `build/performance-results/vr50-hub-mirror-off-20260915-210745`. The356 banner
and DLL match; mirror-off logs confirm real skips, e.g.657/665 hooks skipped with zero
non-OK results in one late3-second window. FOV was also adjusted during this run, so
uncontrolled rate changes cannot isolate the reported percentage. No extra capture is
required merely to honor the requested default.

**Build357 defaults and live resize (superseded by359 below):**102-degree FOV, desktop mirror off,
120% total pixels (3012x3122 versus2750x2850; rounding only). Mirror-off is promoted in
runtime/missing-key/generated/package defaults by explicit request; guarded non-XR/menu
presentation fallback remains. ReduceDesktopPresent stays off. All unrelated settings
and accepted image-owned orientation/stereo policies remain intact.

The scale button now queues a byte-verified six-argument engine ResizeViewport call on
the next game-thread draw, before both eyes. The engine owns its window/RHI reset; no
proxy-forced D3D reset or synthetic F11 toggle. Fresh live-table/IsLiveObject owner,
current HWND/thread and vtable/ABI checks refuse unsupported calls. Set persists the
size and applies it in this run; only matched downstream capture shows Applied. A
10-second timeout reports unconfirmed without automatic retries. Static derivation and
23 production-code host fixture checks are in ENGINE_NOTES and viewport-resize-host.
Native D3D9Ex mirror-off regression passed120 GPU markers plus full-return/reset;
248 reentry and23 single-tag checks, release build, exports, golden INIs and lint pass.
No game/simulator launched. Native resize acceptance remains pending in the headset.
The earlier per-axis110% draft never installed; next-launch-only scale is superseded.

**Installed357:** `vr33-hands-working-357-g847030698-dirty`, bundle
`build/playtest-candidates/vr50-live-resize-hub`. Prior logs/DLL/INI archived at
`build/playtest-candidates/installs/20260915-211639-478150`. Full INI diff changes
ProjectionFov100.00->102, RenderWidth2884->3012, RenderHeight2989->3122 only.
DesktopMirrorOff was already1 from the F10 test. All other settings are preserved;
CRLF and hashes verified. Native resize is installed but not game-tested. Exact356
rollback remains archived.

**Latest359: live resize measured; Display-tab FOV flicker fix pending acceptance.**
357 performed one engine Reset to3135x3250 and capture confirmed it; dimensions stayed
there. Repeated zoom-like flicker instead coincided with260 post-resize FOV changes
between104 and108.05 degrees and repeated scope releases. Missing braces in the legacy
F10 Display FOV control wrote0 into the automatic target every idle UI frame, racing
Present's108-degree handoff. Source-confirmed defect, not resolution oscillation.
The initial150ms-expiry hypothesis is superseded by explicit scope releases and this
writer. Corrected control writes only on edits; production regression7/7 passes while
old control fails7/7. No stereo/orientation/timeout changes. Full flicker record and
falsifiable continuation: [FLICKER_REFERENCE.md](FLICKER_REFERENCE.md), latest VR-50 entry.

Installed359 (`vr33-hands-working-359-gb5e0af9dc-dirty`), candidate
`build/playtest-candidates/vr50-display-fov-flicker`: requested defaults103 FOV,
130% total pixels3135x3250, mirror-off. Archive before install:
`build/playtest-candidates/installs/20260915-213922-577631`. Full installed INI diff only
ProjectionFov102->103; live Set had already saved130% dimensions. Other settings and
CRLF preserved. Build/exports/golden/lint passed; no game/simulator launch. Host success
is not visual acceptance.357 reproduction archive:
`build/performance-results/vr50-resize-flicker-20260915-213237`.

**One launch question:** is the view stable with F10 Display open and after Set120%
then130% in the same run? Expect brief resize pauses, one size transition per Set,
steady103-degree gameplay projection and no repeating zoom pulses. Continued flicker
requires reading the newly explicit scope gate reasons and the actual submitted FOV;
do not assume an eye-sync or resolution-flapping cause. No F11 during this test.

## Mirror-off pacing review (2026-09-15)

**Repository defaults verified:** 103-degree FOV, 130% total pixels (3135x3250),
DesktopMirrorOff=1 in runtime/missing-key defaults, generated/release/golden INIs
and F10 reset/fallback values. Both golden comparisons pass. No new binary is
needed for this request; installed359 already contains these defaults. A later
live Set saved120% in the machine INI; this does not change the repo defaults.

**Correction to the broad "worse tails" warning:** rereading the two original sewer
Full/Off/Full captures shows a consistent small p95 regression and faster typical
frames, but not consistently worse extreme stalls. These are fresh stereo-pair
submission intervals, not headset display FPS or measured motion-to-photon latency.

| Run / metric | Full before | Mirror off | Full after |
|---|---:|---:|---:|
| First, fresh pairs/s | 87.01 | 98.28 | 84.72 |
| First, p95 interval ms | 18.878 | 21.250 | 20.486 |
| First, p99 interval ms | 37.350 | 35.166 | 40.814 |
| Second, fresh pairs/s | 81.86 | 95.62 | 84.02 |
| Second, p95 interval ms | 21.364 | 22.440 | 21.309 |
| Second, p99 interval ms | 42.446 | 36.959 | 36.806 |

**More specific boundary:** fully interior three-second capture windows, ending
more than six seconds after each phase starts and before its end, show D3D9 blit
fence waits rising with mirror off:

- First run:149/4178 (3.57%) ->631/4724 (13.36%) ->124/4091 (3.03%).
- Second run:42/3915 (1.07%) ->471/4604 (10.23%) ->76/4054 (1.87%).
- Every logged large frame-gap event in those windows is attributed to
  `present-tail (xrEndFrame)`, in all three modes. Counts are11/11/16 and18/10/13.
  These gap events use a dynamic threshold; counts are not a fixed-threshold
  stutter comparison, nor does API attribution establish the underlying cause.

Mirror-off skips the desktop snapshot/re-blit and native Present after capture/XR.
It issues a current-work D3D9 event and one GetData(FLUSH), accepting S_FALSE as
submitted-but-pending. This is submission, not a completion wait. Capture keeps its
separate ownership fences. Microsoft's [D3D9 queries reference](https://learn.microsoft.com/en-us/windows/win32/direct3d9/queries)
confirms that distinction. Removing Present's waiting plausibly lets capture reach
unfinished work sooner; that is an inference supported by the increased wait
frequency, not proof that an unbounded GPU queue causes every long frame.

**Routes:** preserve mirror-off. Best prospective mitigation is pacing complete
stereo pairs or bounding queued work without restoring desktop presentation.
Existing `Pace.SyncHz` gates only pair opening; the generic per-Present FpsCap is
bypassed by reentry. No arbitrary cap is promoted: a cap may trade some peak FPS
for regularity and cannot shorten a frame already slow inside xrEndFrame. A bound
on outstanding GPU work would need independent completion events; the current
submit-only query deliberately abandons prior results and cannot serve as that
bound. Never remove the capture ownership fences or change image/pose identities.
Reduced desktop cadence already failed to provide consistent tail improvement;
nonblocking Present and maximum-frame-latency sweeps are also exhausted routes.

**Next discriminating check, using existing359:** in the slow hub at fixed103 FOV
and130% pixels, F10 Display's existing Full/Off/Full benchmark compares one stationary
view for100 seconds and restores the original mirror mode. One question: does the
current hub benefit also worsen fresh-pair p95/p99? If both improve, retain off
without adding a limiter. If throughput improves but p95 worsens beyond both full
baselines, test pair-opening pacing against off/unpaced/off with the target derived
from this hub's measured sustainable rate. If the bracketing full runs drift or
settings/view change, the comparison is inconclusive. No benchmark was armed or run
in this review; no new headset run is claimed.

Evidence: existing `build/performance-results/desktop-first`, `desktop-second`,
`desktop-reduced-first`. Current359 DLL/banner verified and both logs/INI/manifest
archived to `build/performance-results/vr50-mirror-review-20260915-224832`. Current
run has no controlled desktop A/B; do not infer a new mirror causal result from it.
No install or runtime policy change. Visible acceptance of the prior FOV fix still
requires the tester's report.

## Pair-pacing test prepared (2026-09-15, build361)

The user requested the pacing comparison directly, superseding the proposed extra
mirror on/off hub test. Installed `vr33-hands-working-361-g140afb6e7-dirty` from
`build/playtest-candidates/vr50-pair-pacing-ab`. Existing pair-opening pacing is
unchanged; this adds automatic A/B/A control and an actual delay-event counter.
No engine-memory, eye-tag, image-owned orientation or capture-fence policy changes.

- `[Perf] DesktopAb=3` arms one comparison per launch. Repo/missing-key default
  remains off; F10 Display has a live pair-pacing benchmark selector and Start/Stop.
-30 seconds of gameplay settle, then30 seconds each: mirror-off/unpaced,
  mirror-off/paced, mirror-off/unpaced. First3 seconds of each segment excluded.
- Target is floor(90% of baseline fresh-pair rate), capped at measured headset Hz.
  The10% margin is an experimental choice, not a measured optimum. Invalid/empty/
  overflowing baseline refuses a target. No new fixed FPS default is promoted.
- Counts successful submissions with both captured serials renewed. Logs p50/p95/
  p99/p99.9/max, rate, fixed-threshold exceedances, held submissions and full-phase
  pacing-delay events. Zero actual delay events means pacing was not exercised.
- End, manual stop or menu/load abort restores original mirror/reduction/pacing/
  target. Changing mirror or pacing during measurement aborts the comparison.
  Installed ini keeps DesktopAb=3 until the agent disarms it after reading results.

**Validation:**26 production-benchmark host checks pass: transition order, automatic
rate selection, display bound, held/warmup rejection, retaining long gaps, insufficient
samples/overflow, external mode changes, completion/abort restoration and original
Full/Off/Reduced behavior. Release build,9 exports, package golden and lint pass.
No game/simulator launched. These prove benchmark control, not headset smoothness.

Both build359 logs/INI/manifest preserved before changes in
`build/performance-results/pair-pacing-before-20260915-230201`; DLL/banner verified.
Install archive `C:/dev/Dishonored-VR/build/playtest-candidates/installs/20260915-230217-845830`.
Full INI diff: DesktopAb0->3, RenderWidth3012->3135, RenderHeight3122->3250; this
restores requested130% pixels for all three phases.103 FOV and mirror-off preserved,
all unrelated settings preserved, installed hashes/CRLF verified. Source snapshot
ships in candidate/source.patch. Exact359 rollback remains archived.

**One launch question:** does pair pacing reduce hitching versus both surrounding
unpaced phases while retaining useful mirror-off throughput? Load the slow hub;
use the30-second grace to settle into one view, then remain there for the90-second
comparison (two minutes total after gameplay starts). No F10/F11/setting changes or
menus during the comparison. Expected: pacing actually engages, rate approaches the
calculated target, and slow-frame intervals improve. Better p95/p99 beyond baseline
spread with modest throughput cost supports this target; lower rate without better
tails rejects it. Zero delays, mode abort or drifting baselines is inconclusive.
Visible discomfort or stereo instability rejects the candidate regardless of averages.
Agent reads and archives the result and disarms DesktopAb; no visual result claimed yet.

## Latest correction:120% default (2026-09-15, build362)

User clarified120% total pixels is the new default, superseding130%. Runtime
missing-key, generated/package/golden INIs and F10 scale fallback now use3012x3122.
FOV103 and mirror-off remain. Installed `vr33-hands-working-362-g40474ba59-dirty`
from `build/playtest-candidates/vr50-pair-pacing-120`; the automatic pair-pacing
comparison above remains armed (DesktopAb=3), now at120% throughout all phases.
No pacing behavior change. Same two-minute launch question and outcome criteria.
Both logs/DLL/INI archived before replacement at
`build/playtest-candidates/installs/20260915-230558-957582`. Complete installed INI
diff changes only RenderWidth3135->3012 and RenderHeight3250->3122; other settings
preserved. Build,9 exports, both golden comparisons and lint pass; installed hashes
and CRLF verified. No game or simulator launched; no new measured pacing result.

## Pacing result and strict mirror-off trial (2026-09-15, build363)

**Verified362 result:** user reports possibly more consistent delivery but a laggier
feel. The automatic trial completed, target66 Hz, with1645 actual pacing-delay events.

| Phase | Fresh pairs/s | p50 ms | p95 ms | p99 ms |
|---|---:|---:|---:|---:|
| Unpaced before | 73.77 | 12.035 | 22.319 | 41.524 |
| Paced66 Hz | 63.37 | 15.141 | 20.490 | 34.008 |
| Unpaced after | 96.58 | 9.524 | 16.104 | 23.171 |

The paced phase is slower than both baselines and improves tails only versus the
first. Baseline drift is substantial, so this is not evidence of a repeatable
smoothness improvement. Pacing is not promoted; automatic benchmark now disabled,
SyncHz remains0. Preserve the adaptive test for future use, not as a default.
Logs/INI/manifest: `build/performance-results/pair-pacing-result-20260915-231337`;
installed362 DLL and log banner verified before interpretation.

**Residual desktop updates:** real context fallbacks, not a cosmetic F10 label.
Late windows show2-4 native Presents/3 seconds, about5-7 ms per actual call.
The prior off guard requires a fresh delivered capture plus a runtime mirror callback.
A missing/held capture restores desktop Present even though the XR session is still
running. Removing these rare calls is not predicted to repeat the large full-mirror
FPS gain; the test targets residual updates and possible local stalls.

**Installed363** (`vr33-hands-working-363-g2714e9b73-dirty`), candidate
`build/playtest-candidates/vr50-strict-mirror-off`: new opt-in
`[VR] DesktopMirrorStrictOff=1` extends mirror-off across missing fresh capture and
missing mirror callback whenever the XR session has begun. Current-frame GPU submit
flush remains, as do image/eye identities and capture ownership fences. Normal
window/device/swap parameters are still required; stopped XR, unsupported parameters
or an explicit submission failure uses real Present. Zero native calls is expected
throughout healthy running-XR windows, including temporary capture gaps. Startup or
stopped-session desktop activity is outside that interval. This does not hide the
window, remove engine rendering, or switch the headset to a different image path.

The stricter option defaults0 in source/generated/package/missing-key settings,
with F10 Display toggle `Keep desktop frozen across VR frame gaps (test)` for A/B.
Main mirror-off default1,103-degree FOV and120% pixels3012x3122 remain unchanged.
Build/9 exports/both golden INIs/lint pass. Native D3D9Ex host verifies240 independent
GPU markers and pixels with zero desktop calls:120 guarded,120 strict without fresh
capture (half also omit the callback). Negative control with strict disabled presents
a capture gap. Stopped-XR, failed submit, unsupported-parameter, full-return and reset
checks pass. No game or simulator launched; rendered/headset result pending.

Before installation both logs/DLL/INI archived at
`build/playtest-candidates/installs/20260915-231719-701999`. Complete INI diff only
DesktopAb3->0 and new DesktopMirrorStrictOff=1. Hashes/CRLF verified. Source patch and
exact prior candidate retained. Existing profilers remain as previously configured.

**One launch question:** does the desktop stay frozen through normal hub play while
the headset remains responsive and free of new stalls? Expected log evidence:
strict=1, strictSkips increasing, actual=0 and nonOK=0 in running-session windows.
Zero actual calls plus normal headset behavior accepts suppression, not a measured
FPS gain. New stalls/eye instability rejects it. Remaining actual calls require
matching session state and logged parameter/query/context refusal before broadening
the guard. Pair pacing and its benchmark stay off throughout this test.

## Accepted profile and publication (2026-09-15)

User accepted363 strict mirror suppression and reports it may feel better. Verified
363 DLL/banner and archived both logs, INI and exact DLL in
`build/performance-results/strict-mirror-accepted-20260915-235444`. All621 logged
strict-mode windows have actual=0 desktop Presents. This confirms suppression;
subjective improvement is not a controlled FPS measurement.

At explicit request the complete saved machine INI is promoted byte-for-byte to
release/golden and the generated default writer, including HUD anchors/placements,
alpha gain/gamma, hand trim, crouch hold mode and existing diagnostic flags. Strict
mirror-off is now a compiled/missing-key default as well. FOV103,120% total pixels
3012x3122, mirror-off and strict-off enabled, pair pacing and benchmarks off.
Existing explicit INI settings still override defaults. New profiles reproduce the
accepted saved settings; the promotion does not add new HUD rendering behavior.

Branch renamed `codex/performance-improvements`; publication explicitly authorized.
Performance PR targets VR-Main and remains unmerged. PR67's crouched pitch fix will
be combined LOCALLY on a separate playtest branch, preserving this accepted profile;
its author branch and both GitHub PRs remain unmerged. Local integration is a test
of the combination, not a claim that the exact standalone PR67 head was tested.

## Results and routes

Numbers below come from different matched workloads. They must not be combined into
one frame budget. Fresh stereo submissions, eye Presents, simulator ticks and headset
refresh rate are different populations. Baseline rendering was 2750x2850 at 120 Hz.

| Route | Evidence and decision |
|---|---|
| Lower resolution | 1375x1425 (quarter pixels) improved hub throughput only about 6%. 3850x3990 hurt substantially, to roughly 45-50 fps. Both a fixed rendering cost and a pixel-dependent cost exist; neither sole CPU nor sole GPU ownership is proven. Original resolution restored. |
| Per-eye scene preparation | Ordinary InitViews costs 1.322 ms/pair in the fixed simulator pub view, including 1.082 ms of frustum culling. Best specific remaining CPU boundary; no sharing or skipping implemented. See detailed result below. |
| Extra left-view preparation | Reflection, not a second world tick: 0.199 ms/pair, including 0.140 ms culling. Lower priority than ordinary views. |
| Engine query-result waits | Real headset build316: 0.102 ms/pair across 14,230 pairs, 0.596% of elapsed time. Not a useful hub target; do not repeat unchanged. This bounds the measured helper, not all occlusion/visibility work. |
| Nonblocking desktop Present | Build313 off/on/off 58.21 / 57.70 / 58.29 ticks/s. 4,818 accepted attempts, zero busy skips. Failed hypothesis; implementation and one-off harness removed. |
| Omit desktop Present completely | Two sewer Full/Off/Full runs: 87.01 / 98.28 / 84.72 and 81.86 / 95.62 / 84.02 fresh pairs/s. Repeatable throughput gain, modestly worse p95, mixed p99/extreme tails (see review above). Earlier opt-in result; now mirror-off is the requested default after the separate hub report above. Sewer numbers are not a hub forecast. |
| Reduce desktop Present cadence | Full/Reduced/Full 86.34 / 90.28 / 89.27 pairs/s; p95 18.385 / 18.488 / 17.656 ms. Much waiting moved into remaining calls (about 1.46 ms/hook, 2.91 ms/actual call). No consistent tail benefit. |
| Dynamic shadows via game INI | Applied settings, simulator 79.94 / 80.63 / 79.26 ticks/s. No useful gain; do not repeat unchanged. Does not eliminate all lighting/shadow work. |
| Suppress selected diagnostics | Build295 baseline/reduced/baseline 63.66 / 64.48 / 64.27 fresh pairs/s, no consistent tail improvement. Keep accepted diagnostics. Mask covered ZAccount, PairTrace, FrameId and AttachCensus only. Cine.Trace/DrawCensus/PoseReport have functional dependencies. |
| Shader reflection cache | Build275 estimated reflection 2.801 ms/s plus bytecode 2.203 ms/s, not ms/frame. Too small for a complex cache as a leading fix. Layout/router/state/locking measurements are inclusive, not additive. |
| Native draw/state submission | Substantial aggregate sampled CPU wall cost. Resource-lock/upload samples small; their maxima do not bound unsampled calls. Driver waiting and deferred work remain unresolved. |
| D3D11 bridge copies | Conversion about 0.10 ms/eye; XR copy about 0.057 ms/eye. Individually small. Timestamp intervals are not additive GPU busy time or end-to-end latency. |
| FrameId readback / maximum frame latency | Earlier FrameId-off test remained inside baseline noise; D3D9Ex latency 1/2/3 sweep applied and read back successfully without benefit. Neither is an untapped proven fix. |
| Head/hand pose lag | Head/view candidate failed its measured prediction. Historical PoseLag=2 weapon improvement is a separate correctness result. Preserve accepted image-owned orientation; do not alter image tags to chase FPS. |

## Representative CPU evidence

The normal headset CPU capture is `build/performance-results/vr125-light-headset`.
Before/during/after rates were 55.47 / 55.80 / 56.80 ticks/s, with representative lag.
In its interior 5-35 seconds, render thread 19568 was running 72.43%, blocked 26.91%,
ready 0.57%. About 7.382 seconds of blocking ended with wakeups from NVIDIA worker
14260. The worker also spent many CPU samples polling. That is not useful scene work,
but neither polling nor the wakeup source identifies an automatically removable wait.
Low whole-machine utilization does not rule out a critical-thread constraint.

Of 21,085 render-thread CPU stacks, InitViews appeared in 2,575 (12.21% inclusive),
its dominant child in 2,062 (9.78%). Later render-stage return RVAs 0046C1F4 and
0046C208 appeared in 36.97% and 33.29%; inclusive shares can overlap. Those later
render/submission paths remain substantial and less precisely attributed.
Separate stage-cycle measurement put 87.95% of measured render-thread cycles outside
Present, where engine rendering and draw hooks execute. The game-thread viewport
calls totalled about 1.23 ms wall time but overlap queued render-thread execution.
**World tick runs once; viewport Draw runs twice. Duplicated NPC AI is not established.**

The heavy combined CPU/GPU capture changed the workload: 45.85 ticks/s versus 57.14
untraced (control about 24.6% faster). Its rolling GPU events began at 312.752 seconds,
after game exit at 239.448 seconds. Zero lost events did not make that GPU timeline
usable. It cannot identify the game's GPU queue bottleneck. A future GPU trace needs
short capture, retained in-game events, matching symbols and an overhead control.

## Latest boundary: InitViews and frustum culling

Build349 (`vr33-hands-working-349-g227da088c-dirty`) measured a fixed Hound Pits pub
view at simulator yaw 90, original engine resolution, 120 Hz source setting. Simulator
output was 1032x1104; this is CPU attribution, not headset performance acceptance.
Selected 13 complete windows span 39.056 seconds and 8,179 InitViews calls. Total
InitViews was 4,152.868 ms (10.633% elapsed). Unknown-eye boundary intervals were
excluded from pair normalization, leaving 2,707.5 equivalent tagged stereo pairs.

| Inclusive boundary | ms per tagged pair | Nested culling, ms per pair |
|---|---:|---:|
| Ordinary left + right preparation | 1.3215 | 1.0818 |
| Reflection preparation | 0.1995 | 0.1398 |
| Total | 1.5210 | 1.2216 |

Culling is about 82% of ordinary preparation and is already contained in InitViews.
Reflection appeared only in left intervals: 2,684 calls, usually ordinal 1 followed
by ordinary ordinal 2. Ordinary right was ordinal 1 (2,707 calls). Every selected
InitViews had one matched child call; no unknown classification, selector changes,
foreign calls, unmatched child, nesting, ordinal clamp or row overflow occurred.
Build347 independently measured approximately 1.523 ms/pair for total InitViews.

Off/on/off rates were 67.63 / 69.05 / 68.16 ticks/s. On-phase render-target workload
was lower, so this is neither a speedup nor a tight small-overhead bound. A subsequent
yaw change retained two nonblack views; simulator finished 23,479 frames with zero
errors, discarded frames or out-of-order submissions. No optimization was applied.

The probe byte-verifies both hook boundaries and uses derived calling conventions.
It classifies a borrowed renderer's family reflection selector only during the call,
retains no engine object and changes no engine result. Addresses, ABI and label
provenance are in ENGINE_NOTES, not duplicated here.

If resumed, first distinguish octree candidate gathering from per-view primitive
tests, using the existing stacks and offline code. A shared conservative candidate
set would need to include both eyes and current dynamic objects. Do not copy the
left visibility result or skip the right pass: existing VR-79 is a stereo-visibility
correctness constraint. Reflection/scene captures can also be view-dependent.

## Other routes worth preserving

These are options, not queued work or promised gains.

- **Submission and visibility history:** correlate later renderer stages, native
  draw/state counts, driver workers and a valid GPU timeline. D3D9 can charge deferred
  work to a later API call. Cheap query reads do not rule out excessive draw counts,
  query issue cost or incorrect shared per-eye visibility history.
- **Per-draw state:** share one lazy per-draw snapshot before attempting a global
  binding cache. Any shader-layout cache needs resource-lifetime identity, negative
  entries, bounded size and reset handling; a reused pointer is not an identity.
  State-block Apply and mod-originated state changes must invalidate caches. Repeated
  animation-weight locking and invariant weapon transforms are measurable candidates,
  but never cache away fresh instance/liveness checks or animation handback.
- **Object discovery and liveness:** early UiDiscover scans measured 367/547 ms in
  build264. This is a load/tail route, not proof of stationary hub cost. Coalesce
  lifecycle-aware live-set refreshes, cache validated metadata and budget discovery
  by elapsed time. Keep current-level IsLiveObject for all engine writers; class names,
  unchanged pointers and permanent readable-page caches are not substitutes.
  ProcessEvent routing must retain synchronous writers on the script lane.
- **Bridge ownership:** drawing directly from the shared SRV into an acquired XR RTV
  could remove an intermediate copy. Formats, alpha, overlays, acquire/wait/release,
  held-frame fallback and reset must remain correct. A third capture slot trades
  memory/latency for reuse slack; no unmeasured queue gain is assumed. VR-114 separately
  tracks capture timeout/error paths that proceed without explicit ready ownership.
  No timeout explained the selected slow window. Never remove image-ownership fences
  or Flush just because an elapsed interval looks expensive.
- **Managed-resource emulation:** READONLY unlock uploads already skip. ShadowFullCopy
  uploads the written mip, not the entire mip chain. Dirty rectangles, repeated uploads
  and streaming pressure merit work only with measured bytes/use/lifetime evidence.
  Delaying upload until bind misses already-bound resources. Preserve mip/reset/readback
  behavior; 32-bit virtual-address pressure remains distinct from GPU memory capacity.
- **Quality and reconstruction:** motion blur, depth of field, ambient occlusion,
  frame smoothing and VSync were already off in inspected settings. Generic UE4/5
  console recipes do not establish a Dishonored control. PoolSize=160 is a streaming
  budget, not total VRAM. Dynamic resolution needs stable output resources and an
  internal viewport/upscale path; recreating swapchains per change would hitch.
  Temporal upscaling additionally needs history/depth/motion and stereo identity.
- **Architecture:** alternate-eye rendering remains unimplemented and is a separate
  tradeoff: 120 alternating images/s is only 60 fresh images/eye with temporal mismatch.
  Single-pass stereo is a major engine/shader project. A different 64-bit executable
  requires new ABI/addresses/hooks and compatibility validation; bitness alone does
  not remove per-view work. Do not restore the retired DXVK path as a routine tweak.
- **Other measured boundaries:** input/runtime calls, pose-publication contention,
  periodic status I/O and per-frame hand correction need actual critical-path evidence.
  Mesh identification/rebuild is not automatically per-frame work. Release optimization
  already exists; LTO/PGO/inlining follow a hot-path profile, not a blanket fast-math edit.
  Runtime/compositor/encoding/clocks/driver policy need independent evidence; a rate near
  60 on a 120 Hz headset does not prove half-rate locking. SteamVR shim needs its own rig.

## Retained tools and removed experiments

All added experiment switches default off. Existing accepted rendering/diagnostic
settings remain intact. Run only one behavioral benchmark at a time.

| Retained code/tool | Concrete future use and limits |
|---|---|
| ScenePrepareProfile, `sceneprepare on/off` | Exact ordinary/reflection parent/child and per-eye classification on another scene or candidate. INI must arm hooks at launch; mismatch refuses safely. |
| QueryWaitProfile, `querywait on/off` | Complete helper wall time by caller/type/eye on a materially different workload. Launch-armed, pass-through; do not repeat the resolved hub question. |
| NativeProfile / RenderProfile | Bounded sampled native API/draw-hook and reflection/router/state timing. Useful for regression attribution; nested wall times and sampled maxima are not additive/exhaustive. |
| BridgeGpu | Bounded delayed timestamp/disjoint rings for conversion and XR copy; no profiling flush or wait. Keep unresolved/late/invalid/overflow counts. |
| CpuScopes, `perf cpu on/off` | Thread-cycle and wall-time boundaries, with thread/epoch checks. Coarse GetThreadTimes CPU-ms output removed because it was phase-biased and sometimes exceeded wall time. Cycles are not milliseconds. |
| Desktop Full/Reduced/Off and DesktopAb | Preserves a real throughput/tail tradeoff for future controlled comparison. Mirror-off now default by request; fresh-capture/session/parameter guards and current-work submission query required. Earlier old-query design failed frame 2 and was corrected. |
| DiagnosticAb and fresh-pair counters | Reversible collector-overhead check after future changes. Counts successful submissions with both eye serials renewed; restores on completion/abort. Not all enabled diagnostics can be suppressed safely. |
| Host tests, symbol resolver, thread profiles, WPR profile/timed recorder | Reusable validation and attribution without rebuilding tools. IP suspension samples are not on-CPU percentages; ETW needs a perturbation control. Helpers do not authorize game launches. |

Removed: nonblocking PresentEx(DONOTWAIT) policy, configuration/command seam,
borrowed Ex-device accessor, its two standalone test files and timed launch-phase
helper. The measured negative and original implementation remain in git history and
on the preserved resolution-floor branch. Also removed the invalid coarse CPU-ms
field and its per-boundary GetThreadTimes calls. No other diagnostic earned deletion
solely because one workload made its measured path cheap.

Shelving validation: Win32 RelWithDebInfo build, all nine DLL exports, frame/weapon/
animation tests, scene/query ABI tests, native/render sampler tests, bridge policy
and real-device lifecycle tests, desktop policy/copy/benchmark/native-device tests,
diagnostic A/B, reentry (248 checks), single-tag (23 checks), lint and both golden
INI checks passed. Game and simulator were not launched for this cleanup. Installed
DLL/INI hashes still match exact307; release/golden and installed INIs remain CRLF.

## Evidence rules and corrected claims

- Preserve exact DLL/PDB/INI identity and both logs before any future install/launch;
  compare the entire installed INI and verify CRLF. Match log banner before analysis.
  Keep matched saves, FOV, resolution, scene and warmup. Retain all baseline phases,
  populations and tails; a mean or a changing view is not enough.
- D3D9 GPU frame spans can contain feeding gaps. Capture DMA is outside the older
  render span and must not be subtracted from it. D3D9 marker gaps are not whole-GPU
  idle, and pending/unresolved queries are not zero time. CPU-side capture subtotals
  do not bound the whole D3D11 bridge. Present wall time is not automatically savings.
- Old reports mixed startup/menu and gameplay, mismatched timestamp windows, and
  called elapsed intervals CPU work. The claimed migration of hitch ownership was
  withdrawn: all 18 supposed outside gaps preceded gameplay; its 19 gameplay gaps
  remained in the submission tail. The 12.4 ms GPU span belonged to 57.0/s, not 73.7/s;
  the 66.7/s untagged post-menu row was unusable.
- Controller sample age alone does not cause double correction. The relevant residual
  is mismatched head/view bases; changing the whole image's Lag cannot isolate a
  weapon-versus-world error. Earlier age arithmetic and pose-plumbing absence claims
  were withdrawn. Accepted stereo fixes supersede those drafts; see FLICKER_REFERENCE.
- Raw flat 240 fps at 1440p is capped and not a matched workload: two 2750x2850 eyes
  contain about 4.25 times the pixels, plus wider view/submission work. Neither doubling
  flat cost nor aggregate CPU/GPU utilization predicts VR throughput.

## Provenance and recovery

Full pre-trim chronology is recoverable at commit `e02c97d74`, in this same file.
Earlier report paths redirect here. Local raw captures and analysis stay ignored;
never commit game-derived dumps. Under `build/performance-results/`:

| Evidence | Directory |
|---|---|
| Representative CPU stacks/waits | `vr125-light-headset` |
| Latest classified culling result, reproducible analysis | `vr125-culling-classification/sim-20260915-183218` |
| Original InitViews result | `vr125-initviews/sim-20260915-181540` |
| Query helper headset result | `vr125-query-waits` |
| Rejected nonblocking Present | `vr125-desktop-nonblocking` |
| Heavy trace and control | `vr125-etw-headset-20260915`, `vr125-etw-off-control` |
| CPU stages, shadows and symbols | `vr125-cpu-scopes`, `vr125-shadows`, `vr125-symbols` |
| Resolution trials | `quarter-pixel-20260914-221912`, `high-resolution-20260914-222549` |
| Native draw/state and reflection | `native-hub-20260914-215400`, `native-state-hub-20260914-220319`, `render-profile-first` |
| Bridge and diagnostic comparison | `bridge-hub-20260914-210808`, `diagnostic-hub-repeat-20260914-214029` |
| Desktop comparisons | `desktop-first`, `desktop-second`, `desktop-reduced-first` |

Candidate manifests and DLL/PDB/INI bundles are under `build/playtest-candidates`.
Exact307 restoration archive: `installs/20260915-183805-018147`. Preserve the branches
`codex/vr-113-performance-audit`, `codex/vr-115-desktop-present`,
`codex/vr-115-performance-rollout`, `codex/vr-121-render-thread-profile`,
`codex/vr-121-native-draw-profile`, `codex/vr-123-bridge-gpu-profile`,
`codex/vr-124-diagnostic-overhead`, `codex/vr-125-resolution-floor` and `performance-fix`.
They are ancestors of the consolidation. Unrelated older perf/camera branches were
not imported; their superseded stereo behavior is not part of this work.

## Primary references

Sources explain mechanisms, not measured Dishonored savings:

- [Epic UE3 level optimization](https://docs.unrealengine.com/udk/Three/LevelOptimization.html): game/render threads, driver overhead, visibility and object/light interactions.
- [Microsoft D3D9 profiling](https://learn.microsoft.com/en-us/windows/win32/direct3d9/accurately-profiling-direct3d-api-calls) and [optimization](https://learn.microsoft.com/en-us/windows/win32/direct3d9/performance-optimizations): deferred charges, batching, state and dynamic buffers.
- [Microsoft D3D9 queries](https://learn.microsoft.com/en-us/windows/win32/direct3d9/queries) and [NVIDIA occlusion culling](https://developer.nvidia.com/gpugems/gpugems/part-v-performance-and-practicalities/chapter-29-efficient-occlusion-culling): polling, flush, latency and overlap.
- [D3D11 Flush](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-flush) and [shared resources](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-opensharedresource): submission does not prove completion.
- [OpenXR wait](https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrWaitSwapchainImage.html) and [release](https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrReleaseSwapchainImage.html): image ownership contract.
- [Microsoft GPU accounting](https://devblogs.microsoft.com/directx/gpus-in-the-task-manager/): engine-specific utilization and summary semantics.

## Accepted combined main merge (2026-09-16)

The tester accepted build369 combining performance PR68 and crouch-camera PR67,
then explicitly authorized both merges. Installed DLL hash and log banner match
vr33-hands-working-369-g6c3ef07b4. Both logs and INI preserved under
build/playtest-candidates/pr67-combined/accepted-20260916. Crouch logs confirm
camera LEFT ALONE and standing pivot retained. PR67 merged as6ec63636e and PR68
as52107a094; resulting source/release/test trees match the accepted combination.
Source branches remain.103 FOV,120% pixels and strict desktop suppression remain
the accepted profile. No new controlled performance percentage is established.
Subsequent HUD work is VR-126 and is documented in HUD_ANCHORS, outside this research.


## VR-143: the stand-up stall - streaming and paging are CLEARED (2026-09-19)

The ticket was opened on run 499's reading that the load window carried 11996
`TexLockRect` calls, making texture streaming and the device shadow copy the
first suspects. Run 514 measured a comparable stall directly and clears both.

Run 514 (`vr33-hands-working-514-g8bba892f7-dirty`), the save load at t=6634593:

- `perf: frame gap 2441ms ... sat in: out/idle (waiting for the game thread) of
  #7664 tag -1 (2437.6 ms of in 1.3 / out 2440.1; wait 0.0 lock 0.6 endFrame 0.1)`
- `device/stream (gap)`: all twenty 100 ms buckets 0.0/0.0 MB, `totals uploads 0
  (0.0 MB, 0.0 ms CPU in UpdateSurface) creates 0 (0.0 MB) releases 0`
- `gpumem (gap)`: VRAM 2003 / 15293 MB and flat over 4 s, system-backed 88 MB
  and unmoving, largest free address range 1322.8 MB and unmoving

So: no texture uploads, no creations, no VRAM growth, no paging, no 32-bit
address pressure, and the render thread idle waiting on the game thread. The
streaming hypothesis does not survive this, and neither does paging.

**The measurement trap that remains.** `out` is "not our present hooks", which
is NOT the same as "not the mod". Every mod tick on the GAME thread - `PeLatch`,
the hand drive, the latches, the property resolvers, the marker hooks - runs
inside `ProcessEvent` and lands in the same `out` bucket as the engine's own
work. Run 499's conclusion that "the mod's hook scopes were no higher than
elsewhere" was read off the present-thread split, which never covered that lane.

**The instrument built for it** (`StandUpProbeTick`, `crouch.cpp`): the first
stand-up after a new pawn starts a bounded 4 s capture, 40 buckets of 100 ms,
recording presents and the script lane's own time and outermost dispatch count
per bucket, then printing them beside `device/stream`, `gpumem` and a `perf`
mark. A bucket the script lane never reached rolls forward EMPTY rather than
being skipped, because a run of empty buckets is the signature that matters: the
game thread was inside the engine and not in our code at all.

The reading is stated on the line and can print the unwelcome answer either way.
Script-lane ms rising with the buckets where presents collapse means the stall is
ours. A flat or empty script lane while presents collapse means the game thread
was in the engine and the mod is a bystander, which closes the ticket rather
than continuing it. Not yet run.


## VR-143 SOLVED: the stall is the crawl tuck deferring every discovery (2026-09-19)

Run 516, the first stand-up after a load, from the probe built for this ticket:

```
standup: presents per 100 ms, oldest first: 2 0 0 0 ... 0 1 1 0 ... 1 2
standup: script-lane ms per 100 ms: 3132 0 0 0 ... 0 64 652 0 ... 88 39
standup: totals over 4000 ms - 7 present(s) (571.4 ms mean), script lane 3975 ms
         in 218 outermost dispatch(es) = 99% of wall, 35 buckets the script lane
         never reached
```

99% of the window inside our own ProcessEvent handler. The empty buckets are the
signature working as designed: the lane could not roll a bucket because it was
still inside one dispatch.

What ran in it, from the same window (t=8176000..8180000):

| t | what |
|---|---|
| 8176000 | `script: EndCrouch`, `script: NotifyTakeHit` - knocked out of crouch |
| 8176000 | `skc: drive is ON but NO SkelControl slots are latched (probeFails=0)` |
| 8176000 | `skc: ==== SkelControl probe over 115054 objects ====` |
| 8176296 | the walk returns - 296 ms - then the ownership dump and `skc/prop` |
| 8178453 | `graft:` x25 |
| 8179140 | `wa/scale`, `wa/comp`, `wa/id` - weapon attach derivation |
| 8179968 | `dc:` x102 - draw capture re-arm |
| 8180000 | `blink: latched the live PowerBlink` - the latch finally gets its turn |

**Cause.** The crawl tuck's `if (t) return;` in `ApplyHandToMesh` sits above
`ApplyHandToMeshInner`, which holds the whole 30.95 discovery block. A crouch
therefore parked every discovery until the player stood, and the load left
nothing warm, so all of it ran at once. `probeFails=0` proves the probe had never
been attempted, not that it had failed.

Note that the walk itself is only 296 ms of the 4000. The rest is the cascade it
gates: graft, weapon attach, draw capture, each re-deriving from scratch.

**Fix.** The tuck runs the discovery and skips only the calibration request and
the drive writes. The fault guard moved above the tuck so the discovery stays
inside the walk's recovery, and every path out clears `g_walkTid`.

**Not yet re-measured.** The prediction this makes, and which the next run can
refute: the same capture should show the script lane spread thin across the
window instead of 99% in it, because the work now happens during the crouch
rather than at the release. If it does not, the cascade has another gate.


## VR-152: a per-draw log that leaked its own cap (2026-09-19)

**Report:** framerate reads high and consistent (100-110) but movement feels
laggy and stuttery; standing still and looking around is smooth; the weapon
wheel shows high FPS and feels just as bad. It was better a day or two earlier.

**120 Hz is NOT the change.** The tester runs 120 and has for days, and the
archived build 512 log is also 120 Hz and was the smooth one. The refresh is a
constant here, so it cannot be the regression. Recorded because the shipped ini
comment argues for 90 Hz and a reader will reach for it.

**What did change, measured over two runs at the same 2750x2850:**

| | build 512 (smooth) | build 517 (laggy) |
|---|---|---|
| mean `perf: tick` | 9.41 ms | 10.70 ms |
| mean rate | 103.2/s | 89.2/s |
| samples | 486 | 884 |
| `pcap/layout` lines | 5.3/s | 26.6/s |

**Cause 1, and the large one: `pcap/layout` leaked its cap.** The per-shader
naming guard reads

    if (saidN < 16) said[saidN++] = key;
    Log("pcap/layout: shader %p declares ...");

It stops REMEMBERING at sixteen and does not stop LOGGING, so the seventeenth
distinct shader onward prints on EVERY DRAW, forever. 77992 of those lines in
one 49 minute run - a five-argument format plus file I/O per draw, on the render
thread. The comment directly above it says it exists to prevent exactly this
("it produced a 25 MB log in a single short run"); the guard just did not hold
past sixteen. Full now means silent, with one Warn saying so.

**Cause 2, mine, from the same day: the awareness census paid for a line it does
not print.** `awareness_report` takes the position mutex and was called on every
parent update (16667 in one run) only to build arguments for a line gated to
once a second - and that mutex is the one `match_awareness_draw` takes per HUD
draw on the RENDER thread. Cross-thread contention at game-thread rate, for
nothing. The gate now runs first. `match_awareness_draw` also gets a lock-free
early out on an atomic publish stamp, because the common case is that no meter
is live at all and a mutex per draw to discover that is pure contention.

**Prediction the next run can refute:** mean `perf: tick` returns toward 9.4 ms
and `pcap/layout` falls to a handful of lines for the whole run. If the tick
does not move, these two were not the cost and the next suspect is the
discovery that VR-143 moved into the crouch.

**Not established:** why the distinct-shader count passed sixteen when it did.
Different levels draw different shaders, and the laggy run is twice as long, so
the 5x rate rise may be content rather than a change in our code.

## VR-158: fullscreen and vsync as live A/B levers (2026-09-20, UNMEASURED)

**Status: built and installed, nothing measured yet.** The prediction below has
not been tested and must not be quoted as a result.

**Where it came from.** Setting the game to windowed through its own settings
menu was reported to cost a large amount of performance, resembling the state
before the desktop mirror was turned off. That is a mechanism worth testing, not
a coincidence: a windowed D3D9 swapchain presents through DWM composition, so
every desktop present is paid for, while a fullscreen exclusive device bypasses
it. If that is what happens, the windowed cost and the desktop-mirror cost
(`DesktopMirrorOff`, already a measured win) are the same cost seen twice.

**What was missing.** Neither lever could be switched during a run. Fullscreen
was the literal `1` in the engine resize call; vsync (`[Perf] ForceNoVSync`, which
defaults to 1) is only read by `UncapPresent` at device create and reset, so it
had never been A/B'd in a headset at the current frame path. Both now switch live
through the resize path VR-50 proved - `fullscreen on|off` and `vsync on|off` on
the seam, and two checkboxes in the F10 Display tab. Each costs one device reset.

**Prediction the next run can refute.** At ONE fixed resolution, with the desktop
mirror already off:

* windowed -> fullscreen moves the tick and `stereo: beat` pairs/s measurably
* vsync on -> off moves the present rate but NOT the pair rate

If fullscreen moves nothing once the mirror is off, the mechanism above is wrong
and the two costs are one; record that outcome here rather than leaving the
prediction standing.

**Read pairs/s, not the tick mean.** TRAPS carries the reading error from VR-152:
median pairs went 109 to 45 across two builds, a 59% loss that the tick mean
showed as 13%.

**Four combinations, one session, one resolution.** Changing the resolution
between legs makes the comparison worthless, and `[Screen] RenderFullscreen` is
written by the toggle, so the ini after the session reports the last leg, not the
shipped default.

### VR-158 run 2026-09-20: three faults, and the game has been WINDOWED all along

First run of the levers. No A/B was obtained; what it produced instead is more
useful than the A/B would have been.

**The finding that reframes the ticket.** The proxy creates the device windowed
ON PURPOSE, and always has:

```
res: CreateDevice - the game asked for 2750x2850 windowed=0 (ask 2750x2850 fullscreen, virtual ON)
res: CreateDevice - VirtualMode: the game asked FULLSCREEN 2750x2850 (our advertised
mode); creating it WINDOWED with the backbuffer kept
```

2750x2850 is not a display mode on this rig (the monitor lists 20 modes, the
largest 5120x1440), which is why VirtualMode exists at all. So **every headset
session to date has run windowed**, and if windowed-through-DWM carries a cost,
this project has been paying it the whole time without knowing. The one
fullscreen reset in the run was `device Reset (2560x1440 windowed=0)` - a real
display mode.

That makes the original hypothesis untestable as stated: fullscreen at the
headset render size cannot be had while VirtualMode is on, and VirtualMode is
required to reach that size. The comparison that IS available is fullscreen at
2560x1440 against windowed at 2750x2850, which confounds mode with pixel count.

**Fault 1 (ours): the vsync toggle collapsed the render resolution.**
`ResLiveSetVsync` passed the DEVICE's fullscreen state into the resize. Under
VirtualMode that reads windowed, so it asked for a windowed 2750x2850 where
VR-50 had always passed 1. The engine clamped it to the desktop:

```
res: Reset - the game asked for 1355x1405 windowed=1 (ask 2750x2850 windowed, virtual ON)
res/live: NOT CONFIRMED state=4 requested=2750x2850 capture=1355x1405 after10s
```

Fixed: vsync no longer touches the fullscreen ask, and a windowed ask larger
than the desktop is refused up front with both sizes on the line.

**Fault 2 (ours): the fullscreen checkbox could never stay ticked.** It read the
device, which is windowed by design here, so it snapped back every frame. It now
shows what was ASKED, reports the device separately, and says when VirtualMode
is the reason the two differ.

**Fault 3 (ours): the vsync ON leg never existed.** `UncapPresent` returns at
its first line when `ForceNoVSync` is clear, so clearing the flag only stops
forcing vsync off - it does not turn vsync on. This game asks for
`D3DPRESENT_INTERVAL_IMMEDIATE` itself (`interval=0x80000000` at CreateDevice),
so the "vsync on" leg ran uncapped and would have reported no difference for the
wrong reason. `g_vsyncWant` now forces both directions and logs the interval at
every reset, including when there was nothing to change.

**What DID work.** The engine resize provokes a real device reset every time
(`device Reset (WxH windowed=1)`, five in the run), so the mechanism for making
a vsync change take is sound. The guarded resize path refused nothing and the
capture confirmed each size it was given.

**Next run.** Vsync is now A/B-able at a fixed size and is the cheaper question;
take it first. For fullscreen, the honest test is VirtualMode OFF at a real
display mode (2560x1440) against VirtualMode ON windowed at the same 2560x1440,
so mode is the only variable.

## VR-44: hot-path probes removed after a lag report (2026-09-21, UNMEASURED fix)

**Report.** Build 619 felt noticeably laggier in the headset.

**Measured.** All three runs were at 144 Hz, so the rate is not a variable here. The
figures below are gameplay `perf: tick` samples (runs over 30 ticks/s):

| Build | Ticks/s (mean) | Game time outside our frame path |
|---|---|---|
| 615 | 115.5 | 2.2 ms |
| 618 (power census added) | 94.0 | 3.9 ms |
| 619 (power seams added) | 96.3 | 3.7 ms |

The step is at 618, not 619. Within the 619 run, the game ran at about 127 ticks/s
(1.5 ms) for 30 s with every hook installed. It then fell to 75-90 ticks/s before any
power was used. So the extra cost scales with the scene and is not a flat per-frame
cost. The scenes were not the same across the runs, so this does not prove the cause.

**Suspects, and what was done about each:**
* **The power census.** Its hook on `0x00B515C0`, the camera accessor, has 95 callers,
  and AI code is among them. The hook ran a full `pushfd/pushad/fxsave/fxrstor` on every
  call before filtering for power-code callers. That cost grows with the number of NPCs.
  REMOVED; its result is recorded in ENGINE_NOTES.
* **The trace census (VR-166).** It hooked `execTrace` and three camera-trace helpers,
  which AI and script traces call constantly. It was already on in 615, so it is not
  the step, but it is a standing cost. REMOVED; its job ended with the razor seam.
* **The aim-assist swap (619).** It ran only on casts from UsePower's slot. It is not a
  hot path, but it was also wrong (see ENGINE_NOTES). RETIRED.

**Still on, and why:**
* The spawn census stays, because spawns are rare and it measures the swarm's landing
  point. It is armed by `[Aim] SourceProbe`.
* The power seams run only on a cast. The one exception is Possession's pick, which
  runs every tick, but only while Possession is held.

**Next run.** Compare ticks/s against 615 in the same kind of scene. If the drop
persists with these probes gone, set `SourceProbe=0` as the next A/B.

**Build 620 (headset).** No lag was reported. The log is short (18 gameplay samples):
104.7 ticks/s, with 3.9 ms outside the frame path. The scene differs from 615's, so this
is a report, not a measured A/B. The suspects stay removed.

## VR-204: two diagnostics that ran all session, default off (2026-09-22, UNMEASURED fix)

**Report:** in the fifth VR-204 run, average fps looked right but a few hitches remained.
That build had a median stereo present rate of 237/s, against 236/s for the trace-only build.

**Log census (180 s run):** `camera/source` wrote 8,625 lines (56/s) and `cine/trace` wrote
7,442 (50/s). Together that is about 106 lines/s for the whole session.

* `camera/source` is the VR-165 census (`[Diagnostics] CamModProbe=1`, shipped ON). Every
  500 ms, `CameraSourceTick` calls `BuildLiveSet()`, a full copy and sort of GObjects
  (115,893 slots on this save), on the script lane. It then prints a table of about 28
  lines. VR-165 is closed. A periodic whole-table rebuild on the game thread is a hitch
  candidate by construction.
* `cine/trace` (`[Cine] Trace=1`, shipped ON) samples every 100 ms and prints 5 lines per
  sample. It rebuilt the live set only once in the run (`liveRefresh=1` on 1 of 1,488
  samples), so its cost is the logging.

**Change:** both now ship 0, in code, the ini writer, the golden and packaged ini, and the
tester's installed ini. The installed ini's original is kept beside the fifth run's logs.
Both keys still turn the diagnostics on for an investigation.

**Not measured:** the hitches themselves. The log has no frame-time histogram, so whether
these two were the spikes is a prediction. Prediction: with both off, the next run's
`stereo: beat` minimum rises toward its median, and a hitch at a 500 ms period no longer
appears. If the hitches remain, the next suspects are the other `BuildLiveSet()` callers
(`cinematic_fov`, `cinematic_pitch`, `game_opts`) and the native-profile hooks
(`NativeProfile=1`).

## VR-204: the live-object table as a hash set (2026-09-22, UNMEASURED fix)

**Report:** after the two diagnostics above were turned off, a few hitches remained at
high fps.

**The run's `perf: frame gap` lines** (the ones logged are 40 ms or more, or 2.5 times the
mean present interval):
* Most steady-play gaps sat in `out/idle (waiting for the game thread)`: 40-60 ms, every
  few seconds, several at exactly 33 ms. Six sat in `present-tail (xrEndFrame)`, at 31-42
  ms: the runtime or compositor.
* The two load-time gaps line up with the `uistate` scan: 101 ms at 43719500 against a
  114 ms gap, and 145 ms at 43795593 against a 164 ms gap. That scan runs once per load and
  is left alone.
* Mod work logged near the steady gaps: none that repeats.

**The mod-side periodic cost:** `RefreshLiveSet` is called by the UI surface poll (1 s),
crouch (1 s), rain, the sword trail and the camera shake (2 s). The shared table was
therefore rebuilt about once a second. Each rebuild copies and sorts about 116,000
pointers, about 12 ms by VR-160's measurement, on whichever thread asked. At 144 Hz that
is at least one dropped frame each time. It sits below the gap logger's 40 ms line, so
the log could not show it.

**Change (`ue3/uobject.cpp`):** the table is now an open-addressing hash set with linear
probing, a load factor of at most 0.5 and an integer avalanche hash. A rebuild is one
linear pass with no sort, and `IsLiveObject` takes one or two probes instead of about
seventeen. The answers are the same: membership in the current GObjects array. Every
30 s the rebuild cost is summarised as `live: N rebuild(s) ... mean X ms, max Y ms`.

**Prediction:** `live:` reports a mean well under 3 ms. The steady out/idle gaps at 40 ms
and above are the game's own, so they should mostly remain. What should go away is the
once-a-second single-frame drops that are too small to itemise.


## 2026-09-23: current public refresh recommendation

The project owner directs the launcher and quick start to recommend 120 Hz, or
144 Hz with Virtual Desktop Beta. This supersedes the old 90 Hz onboarding advice
from earlier render builds. It is current product guidance, not a new measured
benchmark in this session; historical 90/120 observations above retain their
original build context. No timing or pacing implementation changes accompany it.

## VR-212 candidate: bounded missing drop-context discovery (2026-09-23)

Discovery again runs at most once per20ms, with a1s pause after an unsuccessful
full sweep. Pawn changes, a growing/replaced object table and invalidated cached
contexts wake discovery. Cached attack decisions are still sampled every anim
tick. Within each1024-slot slice, readability regions and class verdicts are
reused; both caches expire before returning to the engine, so they retain no
identity across a menu or GC. IsLiveObject and owner validation remain.

This fixes concrete avoidable game-thread work. The entire reported hub slowdown
is not yet attributed or headset-confirmed fixed. No quality/settings reduction.

Candidate761-g14728179e built optimized with legacy off, installed with all64705
INI bytes unchanged and CRLF verified. Launcher embeds this DLL. Native schedule
checks,75 swing-core checks,9 exports, default writer/reset parity and lint pass.
No game launch or post-fix headset result yet. The fixed unnecessary work is
source-verified; the claimed FPS recovery remains pending.

## Mirror policy across runtimes (VR-216, 2026-09-24)

Requested policy: default mirror off on SteamVR too, with visible performance
and compatibility guidance. Remove VR-208's runtime override and disabled launcher
checkbox. The performance hint says disabling the mirror can produce a large
boost with any runtime; it is a conditional user-facing recommendation, not new
cross-runtime benchmark evidence. Existing controlled throughput measurements
above remain Quest/VDXR-specific. No new SteamVR throughput or headset startup
measurement was performed. Native D3D9 GPU submission/pixel checks and all existing
fallbacks pass. Retain the earlier Index startup report in DESKTOP_MIRROR.md.
