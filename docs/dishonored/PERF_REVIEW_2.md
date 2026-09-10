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
