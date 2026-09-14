# VR-70: cinematic physical head tracking

Branch: codex/vr-70-cinematic-head-tracking, based on current VR-Main cccb1815
(PR #53). Ticket verified In Progress; VR-43 and open PR #12 are related.
No permission to merge this work.

## Current candidate: hide the cinematic border overlay

Build221-gfa8ae85a (20:26:36) is headset-confirmed for natural pitch motion.
Banner checked and logs archived at build/cinematic/playtest-20260913-203141;
2770 writes/restores,zero refusals. Stable gaze and pitch correction are retained.

The new [Cine] HideBorders lever defaults off,with F10 View checkbox and
cineborders on/off. The installed candidate enables it. A verified seven-byte
fingerprint and decoded direct target guard a single call-site replacement.
The wrapper calls the original HUD mask query and returns false only for the
stripe query while enabled,in an active projection session,outside menus.
All HUD mask fields remain untouched. Native SetBlackStripes consumes the
result during its normal movie update. OFF forwards the exact native result.
No changes to viewport,resolution,FOV,subtitles or head/position tracking.

One question for the next launch:are the top and bottom black borders gone,
with the boat scene visible in the space they previously covered?

Start the opening boat ride,look around normally,and quit afterward.
- Scene fills the former bar areas:confirms overlay removal reveals the image.
- Bars remain:inspect hook installation and native/returned query counters.
- Empty or clipped regions remain:another render boundary still needs work.
The agent reads the logs;never launches the game. No merge authorization.

136 x86 host checks cover every gate combination,exact original return values,
valid fingerprint and single-byte corruption refusal. Build,lint,exports,INI
and standalone simulator checks accompany install. Full installed INI change
is only HideBorders absent ->1; CRLF is preserved. No further subagents are to
be used in this session,per the user's latest instruction.

## Completed before the first boat test

- Reconciled Linear: VR-96 already Done with High priority retained; initial
  fault owner and nine refused/57 valid updates/11 clean pauses recorded.
- VR-98 remains Done; ten note cycles across reload, 0-16 ms, recorded.
- Created completed follow-ups VR-100 (startup mono gate) and VR-101 (reload
  right-eye R/0 repair), linked to merged PR #53. These fixes are not reopened.
- VR-102 tracks the still-open weapon startup freeze. CacheNameLookups stays off.
- Posted one batch project update. No release or milestone created.
- Read PR #12 body and history; no comments/reviews exist. Its presentation
  controls do not implement head tracking, and its later menus-on-panel
  commit is not part of this work.
- Derived native final camera cache fields independently through both getters.
  Added a read-only trace. No new camera writer or change of gameplay policy.

## Measured boat run (2026-09-13)

Installed build 218-ge5c7653f, compiled 19:34:31, was matched to the banner
before interpreting the log. Current and previous logs are archived under
build/cinematic/playtest-20260913-194011 (ignored). Current log SHA256:
e5b8e05a003223450a14fe6225fc75231980e39acf3092186b6bd466c5e6e69e.

The tester turned the head in all directions; deliberate lean and right-stick
comparison were not performed. Do not infer acceptance for either.

The uninterrupted initial full-animation interval is 281 samples (#39-319),
29.907 seconds, with menus clear, StatePlayerMasterSoiree, influence 1/0/0,
projection on and runtime quad off. All have live head/controller/cache samples
and 6-11 PVR writes per sample. Head pitch spans -38.82 to +23.73 degrees,
controller pitch -38.82 to +23.72, but cache pitch only -0.16 to -0.14.
Head yaw spans -90.27 to +64.74, cache yaw -72.91 to -63.14. Authored camera
translation continues. The controller receives head rotation; the authored
cache bypasses it. The cinematic latch stays off. All 25 pre-cinematic clear-menu
Walk samples have equal controller/cache rotation at printed precision.

Exclude later menu resume samples #331-335 and #387-390 from this conclusion:
zero PVR writes and runtime quad fallback make them stale. Sample #336 starts
recovering but is still on quad. Prior-render position is not same-draw evidence.

## Candidate 219 result and reference-lifetime correction

Build219-g0ebd7a3e, compile19:54:54, banner matched before interpretation.
Archive:build/cinematic/playtest-20260913-200152. Partial head look was reported,
with repeated return to the initial gaze and weaker vertical tracking. Head look
was only noticed after the right-stick prompt; no prompt event is in the trace.

The implementation incorrectly reset g_chReference on every non-double draw.
The run has33 entries:initial anchor plus32 avoidable reanchors with clear menus,
quad off,same camera/controller/pawn and unchanged load epoch.20 resets align
exactly with logged SINGLE(no present since the previous draw); rate-limited
logs cannot independently identify each of the remaining12. All2311 scoped
writes restored successfully with zero refusals. The one-second retry delay
also applied after successful captures and extended the untracked gaps.

A separate earlier override exists:Walk samples157-159 show cache pitch fixed
at-26.59 while controller/head pitch moves8.59 to7.90, despite influence0/1/0.
Other Walk samples match PC/cache, including later boat samples195-212. Therefore
Walk or player influence alone cannot prove normal camera ownership. Look-lock
flags are added read-only to the next trace; this earlier phase remains open.

## Candidate 220 result: gaze confirmed, cinematic pitch translation remains

Build220-gdf783ca8, compile20:09:13, matched before interpreting the run.
Archive:build/cinematic/playtest-20260913-201833. The tester confirms stable
head look,including before the right-stick prompt. Exactly one authored-camera
anchor,3210 successful writes/restores,and zero refusals support the corrected
reference lifetime. The earlier onset complaint is no longer reported.

Remaining observation: looking down raises the camera; looking up lowers it.
Installed Neck mode is cancel,with standing pivot0.321m below/0.062m behind.
The normal position request includes subtraction of the engine's player-camera
pitch arc. An animation-owned camera bypasses that arc. Applying its cancellation
there creates artificial translation. The established VR-78/91 trap applies:
never cancel an arc that the current camera owner did not generate.

## Previous pitch candidate and test

The new candidate publishes normal and cinematic position requests together.
Normal gameplay retains its exact existing neck compensation. The authored draw
scope freezes the request without CANCEL compensation; real tracked translation
and intentional ADD mode remain. No global Neck INI value changes. A lock keeps
the paired requests coherent. Trace logs both requests,explicitly distinguished
from synchronized render measurements.35 host checks,32-bit build,lint,exports
and INI golden pass; headset acceptance is pending.

One question:does looking up/down on the boat now feel natural,without the
camera rising when looking down or dropping when looking up?

On the opening boat,keep the stick untouched and gently look down and up while
remaining seated or standing in place. Then quit normally. Real small head
translation may remain; the artificial opposite movement should disappear.
- Natural vertical looking supports removing the unused neck cancellation.
- Continued opposite movement means another position owner remains involved.
The agent verifies the banner and reads/archives logs. No game launch by agent.

## Cinematic black bars: researched, separate next behavioral candidate

The actual Documents game INIs contain no exposed letterbox,black-stripe or
aspect-constraint key. Script declarations expose SeqAct_ToggleCinematicMode's
m_bHideLetterbox and native SetCinematicMode's eighth hide-letterbox argument.
Verified native registration/vtable derivation leads to0x00AAF150:cinematic
entry sets HUD mask0 with0x6010; hide-letterbox calls0x009EA0C0 to clear only
bit0x10 in DishonoredHUD.m_ShowFlags[0] (cinematic level). The field is reflected;
the native observed array base is HUD+0x4E0. See ENGINE_NOTES for provenance.

This proves the game has an explicit HUD letterbox control; it is not an INI
resolution/FOV setting. It does not yet prove all visible boat bars use that
control or that clearing it reveals fully rendered pixels. Trace the consumer
and test that narrow live A/B next. Non-config Camera aspect constraints and
transient HUD.m_bDrawUIBlackStripes are separate candidates,not proven owners.
Dump method stubs must not be read as actual native return behavior.

Follow the session rule of one behavioral change per build:confirm this pitch
correction first,then the letterbox control. VR-43 already includes letterbox
scope and is the related record. No new ticket or game INI edit is needed for
this research. Keep the wider cutscene issue open. No merge authorization.

## Remaining acceptance work

Use the positive boat/scripted camera state and verified owner. Exclude pause,
notes, load and attract/menu scenes; runtime quad fallback is not that identity.
Compose physical head motion over the authored camera and preserve stick input,
authored pitch/roll/translation and cuts. The draw-scoped cache overlay is the
candidate; restore before the next authored update, use one coherent sample
for both eyes and preserve pose-record provenance. Validate current IsLiveObject
and retained identity on every engine writer, refreshing for new load objects.
Do not lift the direct controller fallback guard or modify pawn locomotion.

Ship the behavioral lever off with a live A/B. Test production composition
math/policy and identity/restore cases offline, then build/install and ask one
focused headset question. Normal gameplay and transition checks follow as
separate launches. Keep VR-70 In Progress until behavior is confirmed. Never
merge without explicit approval.

## Candidate validation

`tools/cinematic-head-host.ps1`:24 rotation/policy checks and11 scope checks pass.
The new regression drives32 temporary holds with changing yaw/pitch and retains
one anchor; a real menu/resume captures exactly one new reference.
The scope harness extracts the production begin/end bodies and mocks only
engine dependencies; it checks restoration, foreign writes, dead/replaced
identity, wrong-thread cleanup and failed preparation. This does not prove
engine rendering consumes the scoped rotation.

32-bit RelWithDebInfo build,9 undecorated exports,lint and INI golden pass.
Standalone xr_hello32 simulator smoke:60 frames,0 errors; no game launch.
Existing DVR_CAT macro warnings remain. The installed HeadLook override and
complete INI byte/config diff are recorded in build/cinematic/latest-install.json.

Native follow-through confirms a Scaleform overlay:00B960E0 queries the HUD's
mask10 through009EA130,compares its prior movie flag,and on change invokes GFx
SetBlackStripes with the resulting boolean. This path explicitly controls UI
stripes rather than a viewport rectangle. Visual confirmation that the exposed
pixels fill the headset view still belongs to the upcoming A/B.

Next letterbox implementation seam is fully derived in ENGINE_NOTES:replace
only the direct mask query at00B96117 after verifying its seven-byte setup.
A same-signature wrapper returning false while enabled leaves every engine HUD
mask intact and lets the native movie update hide stripes. Forward when disabled.
This avoids the persistent field override and its restore/identity complications.
Add a default-off live F10/command lever and a separate bars-only acceptance test.
