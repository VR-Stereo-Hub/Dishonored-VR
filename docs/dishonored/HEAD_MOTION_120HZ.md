# 120Hz physical-head-motion investigation

## Current acceptance: world smoothness approved for main, 2026-09-14

The exact original world candidate 271-g8cd27652 was restored and independently
reconfirmed on the headset. Exceptional world stability returned. Its installed
INI is now copied byte-for-byte to the release and golden INIs, and the default
writer matches it. `Pace.ImageOrientation=1` is the generated and missing-key
default by explicit user request after acceptance; explicit 0 remains respected.

This merge contains the WORLD fix from 8cd27652 only. The unaccepted hand
normalization code from 12a974134 is absent. The branch
`codex/vr-116-flicker-fix-patch` preserves that experiment for later investigation.
The accepted DLL/INI bundle and `vr-116-world-smooth-271` tag remain recoverable.
The user explicitly approved publishing this accepted state to VR-Main.

Latest symptom refinement: residual left-eye hand flicker was observed after
exiting the opening cutscene, while loading a sewer save was essentially clean.
This makes transition state relevant; it does not prove the precise cause.
Track the deferred hand issue with VR-95 and preserve its historical predictor
regression. Do not resume or include hand changes in this merge.

Reconfirmation logs/INI: `build/flicker-120/reconfirmed-world`; banner and DLL
hash match 271-g8cd27652, compiled 13:46:59. Source changes beyond that checkpoint
are documentation and INI/default promotion only. No engine-memory writer,
camera transform, hand pose or frame pacing change is introduced by promotion.

Next: complete and verify the authorized main merge; hand/cutscene work waits.
Historical pending, publication-blocked and candidate text below records earlier
stages and does not override this scope or the new explicit merge authorization.


## HEADSET-CONFIRMED BREAKTHROUGH: world smoothness at 120 Hz

2026-09-14, VR-116. Build **271-g8cd27652** is the preserved known-good
world-smoothness checkpoint. The tester reports an exceptional improvement in
world stability during physical head movement, including during substantial
frame drops. This is the strongest reported world-smoothness result to date.
The remaining slight flicker is confined to hands/weapons and is NOT a failure
of the accepted world result. Preserve this baseline before hand changes.

Verified log banner: compiled Sep 14 2026 13:46:59; installed DLL SHA256
`d3853fb75b71d4cbbdcceea2281b17664939c306290b879918e5d6f40eca5102`.
Exact DLL/INI bundle: `build/playtest-candidates/vr-116-image-orientation`.
Both run logs and INI: `build/flicker-120/accepted-world`.
`Pace.ImageOrientation=1`, world lag 2 and hand lag 2 retained.
Final sampled counters: left accepted 38258/fallback 0; right accepted
38253/fallback 2. These are application counters, not perceptual measurements.

**Preserve the image's own head orientation, not a guessed fixed history age.**
The headset result supports this principle for rotational world reprojection;
it does not establish higher frame rate or positional reprojection correctness.
The old fixed-lag-only diagnosis is superseded for this reported world symptom.
Next: apply image-specific head normalization to shared hand/weapon corrections,
with a separate default-off toggle and the accepted world behavior unchanged.
No merge authorized. Historical pending-test statements below describe earlier work.


## 2026-09-14: severe physical-head-turn flicker at120Hz (VR-116)

Surface: headset world image during cinematics and gameplay, exclusively on
physical head turns. Holding still and right-stick turning do not reproduce.
This routes to image/pose attribution and cadence, not weapon transparency or
static eye-swap flicker. The VR-115 desktop branch is parked, committed at
1d6558a3; its candidate was never installed. New branch starts at VR-Main255d1c91.

Verified installed266-g5aa625ae, compiled11:25:16, DLL SHA256
257ab2551fadfec93d66a1e25067143c6643257709596f8303ecb5482015c7a4.
Both logs and INI archived at build/flicker-120/20260914-133932.
Runtime period8.33ms confirms120Hz. Pace.Lag=2 and Hands.PoseLag=2; both lag
A/Bs are off; SyncHz=0. Lag was not accidentally disabled by default promotion.
In25 steady opening windows, stereo submits/s min/median/max81/90/100;
interval SD2.18..10.32ms. Later windows drop to66..82 pairs/s. Real cadence
pressure is measured, but physical-only symptoms make pose association the
first focused test. Cadence alone is not proven to explain this severity.

Steady-opening ring accounting reconciles, with no late repair/expiry and no
empty pops in those windows. Eye summaries report healthy1/0 ages without
stale eyes or pair aborts. Sampled frame identities show distinct eyes and no
swapped-side result. These summaries are evidence against the old sustained
late-tag failure, not proof that every pixel of every frame is correct.
The old posesub trace prints109 right-eye lines and zero left-eye lines.
Its final mean input/submission orientation difference is0.120deg, moving mean
0.259deg, maximum7.934deg. A large one-off disagreement is measured; its visual
correlation is not. The old posejoin camera/render comparison uses a latest
VP observation and reports large disagreement; it cannot establish an exact
cinematic image projection and is not treated as proof of a camera-write bug.

Candidate Pace.ImageOrientation defaults0, saved by F10 View's Image-linked
head orientation checkbox. When a successful stereo texture copy carries a
valid same-eye camera record, use that record's normalized head quaternion
for the copied image's submission orientation. Missing/expired, wrong-eye,
invalid camera, missing generation or invalid quaternion retains existing lag.
The record already travels with the captured texture; no new history matching
or engine-memory write is added. Camera/hand behavior, pacing, positions and
resolution are unchanged. Positional image attribution is outside this first
rotation-only candidate. This tests camera-input association; it does not
prove that the renderer honored the input. Both eyes log decisions separately.

Eight host checks cover delayed-image pose selection, wrong/unknown eye,
missing record without output damage, invalid camera/quaternion/generation,
and sign-equivalent quaternion. Existing frame/weapon/animation tests, release,
exports,lint and golden INI pass. No game launch. Headset result pending.
Next launch: keep120Hz; in the opening, turn the physical head side to side
with the stick untouched. Does the severe flicker disappear or substantially
reduce? Improvement supports image-linked orientation; unchanged or worse
requires the new per-eye decisions plus render/pose evidence. Do not declare
90Hz a fix, or silently alter numeric lag or install the desktop experiment.

## Installed candidate and publication status

vr33-hands-working-271-g8cd27652, compiled Sep14 13:46:59.
Distinct from the parked, uninstalled desktop271-g160949cb.
DLL SHA256 d3853fb75b71d4cbbdcceea2281b17664939c306290b879918e5d6f40eca5102.
INI SHA256 de20bd794ab0917ca5450178207a333bd2a28066f3edfac253a754b153ac5ce5.
Full INI diff adds only Pace.ImageOrientation=1; CRLF and hashes verified.
Both logs archived at build/playtest-candidates/installs/20260914-134740-056603.
Keep120Hz for the focused physical-head-turn test; no game launch or merge.
Automatic approval review blocked remote publication of this new candidate.
Local work remains committed; explicit publication approval is pending.

## 2026-09-14: hand/weapon follow-up candidate (VR-116)

World checkpoint remains unchanged: source 8cd27652, tag
`vr-116-world-smooth-271`, acceptance documentation commit 827b2db5.
Patch branch: `codex/vr-116-flicker-fix-patch`.

`Hands.ImageOrientation` defaults off, live in F10 View and saved independently.
With both world and hand switches on, hand draws inspect the front queued
camera record on the render/present consumer lane, without popping, repairing
or searching ahead. Unknown/wrong eye, empty/skewed queue, expired record or
invalid head basis retains legacy normalization. This is the next image's
queued record, NOT the previously delivered capture record and NOT the latest
camera publication. The association remains a candidate until headset/log
validation, particularly around late tags; eye identity alone cannot prove
absence of every possible queue fault.

A coherent snapshot now carries the head rotation it was normalized against.
The draw rebases both position and orientation by
`F * transpose(R_imageHead) * R_oldHead * F`. Controller samples, head
translation, camera movement, compositor metadata and numeric lag stay intact.
Weapons consume the same hand correction through WaCommon, including depth and
other material passes. No weapon pose-generation equality gate is restored.
No engine-memory writes or native-animation ownership changes are added.

Host tests cover noncommuting head rotation with a stationary controller,
position/orientation consistency, a fixed-history negative control and invalid
inputs. Queue tests cover nonconsumption, ordered capture, missing/wrong eyes
and excessive queue depth. Per-eye `ms/image-orientation` logs report applied
and fallback counts plus camera/hand generations. Hand result is pending.

Next launch at 120 Hz, weapons drawn: hold controllers steady and physically
turn the head side to side. Does the slight hand/weapon flicker disappear
while world smoothness remains intact? Yes supports image-specific hand
normalization; unchanged/worse requires inspecting applied/fallback counts
before changing pose timing again. Never launch the game or merge this branch.

## Installed hand candidate: 2026-09-14

Build `vr-116-world-smooth-271-3-g12a974134`, compiled 14:09:46.
DLL SHA256 `dead0ade5441462c9380f43280e7e846fa9da34f6cb4a7967194aa9384b94435`.
INI SHA256 `32c998c3485f2f2ed04056011f9f1e37e729ea4a9ca3db8272a6d3a25b9f9c50`.
Bundle `build/playtest-candidates/vr-116-image-hands`; install archive
`build/playtest-candidates/installs/20260914-141029-883111`.
Entire INI comparison adds only `Hands.ImageOrientation=1`; CRLF verified.
World image orientation remains on. Release build, frame math including six
new hand tests, all 253 queue checks, exports and lint passed. No game launched.
The accepted world run still measured 75-77 submissions/s in late 120 Hz windows.
The old cadence log's causal advice is historical, not a valid explanation of
the now-confirmed smooth world in this run. Frame rate and head stability differ.
Hand playtest remains pending; world checkpoint and its DLL/INI are preserved.

## 2026-09-14: hand candidate inconclusive; exact world baseline restored

Build `vr-116-world-smooth-271-3-g12a974134`, compiled 14:09:46,
verified DLL SHA256 dead0ade5441462c9380f43280e7e846fa9da34f6cb4a7967194aa9384b94435.
Both logs and INI archived in `build/flicker-120/hand-test-left-jump`.
Reported: possible small hand improvement, but a one-frame LEFT-eye-only
leftward hand displacement persists during physical yaw while watching hands.
Possible reduced world smoothness was reported with uncertainty. Neither hand
acceptance nor a proven world regression should be inferred from this run.

Final sampled hand counters: left accepted 13365/fallback 1963 (12.81%);
right accepted 13493/fallback 110 (0.81%). Populations are hand draw contexts,
not distinct images or visually marked flicker events. Unknown-eye decisions
are counted in the left bucket by the current diagnostic, so the percentage
is not a pure left-image failure rate. Exact fallback reasons are not printed.
World submission samples end with left accepted 15479/fallback 0 and right
15468/fallback 0. Diff against 8cd27652 confirms no change to world submission
selection, only the separate hand control API. This cannot disprove a timing
or perceptual difference from additional render-thread work.

Routing: section 1's one-eye sideways hand/weapon jump (VR-95), NOT partial
weapon transparency (VR-112). Fixed-head normalization alone is insufficient.
The existing eye classifier holds its old eye on small right-axis jumps;
this can assign a right-eye offset inside a left-eye image. It also gates the
new record lookup, so a wrong classification can deny normalization correction.
Current evidence is suggestive, not event-correlated proof. Do not re-enable
the old blind toggle predictor: its still-head regression remains unresolved.
The old eyecheck reports every comparison UNKNOWN (7089 toggled, 454 SAME,
3 ambiguous). Its text claiming zero disagreement clears the classifier is
invalid when nothing was compared. The lookup asks about a future present;
classification needs a deferred join to the resolved image, not this immediate
future-record query. Fix that instrument before accepting its conclusion.

Restore the EXACT accepted DLL and INI from
`build/playtest-candidates/vr-116-image-orientation`, build 271-g8cd27652.
Keep hand candidate code committed, unaccepted and available separately.
Next launch at 120 Hz in the same scene with physical head turns:
does the exceptional world smoothness return compared with the hand candidate?
Yes isolates the difference to the hand candidate/build path; no leaves scene,
runtime variability or baseline perception unresolved. This is a baseline
comparison, not a claimed hand fix. Afterward, instrument actual per-view eye
identity and deferred classifier agreement before changing eye offsets.
Never launch the game. No merge or external publication authorized this turn.

## Completed merge and installed state: 2026-09-14

World-only PR #61 is integrated into VR-Main at a60516c4b.
Installed build 272-ge3bb7ac2a, compiled 14:27:16, has source identical to
that merge apart from commit/build metadata. DLL SHA256
473bca2b298a988d48307ac23a2a2b257d53f00ac2f8df83dcfa95149c3d373b.
The installed, release and golden INIs are byte-identical CRLF, SHA256
de20bd794ab0917ca5450178207a333bd2a28066f3edfac253a754b153ac5ce5.
The full installation comparison has ZERO INI changes. Both prior logs archived
at build/playtest-candidates/installs/20260914-142912-415128.

Only default promotion differs in source from the repeatedly accepted original
world candidate: no hand-normalization experiment is included. Release build,
frame tests, exports, lint and golden checks passed. No game launched.
Retain both feature branches and the exact original world checkpoint.
Next work is deferred: investigate left-eye hand jumps after opening-cutscene
exit (VR-95); sewer-save loading was reported essentially clean. Do not infer
that all save loads or all cutscenes reproduce it.
