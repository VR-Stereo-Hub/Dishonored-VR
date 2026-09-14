# 120Hz physical-head-motion investigation

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
