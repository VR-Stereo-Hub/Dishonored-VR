# Performance research

## Status: broader research shelved; VR-50 FOV/mirror exception active

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
