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
