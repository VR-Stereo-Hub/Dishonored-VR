## Current candidate, 2026-09-14

PR57 updated with accepted PR56 parent cb77328d. Preserve HeadBasedMovement=1
and all accepted cinematic/FOV/mono fixes. New build enables only the standing
UprightPitchArc correction plus ZAccount diagnostics relative to installed239.
PR56 is ready for review; no PR has merged. PR58 follows after this test.

One question: while standing and looking up or down, does rolling your head
left/right now keep the camera as stable as when crouched, without the extra
smile-shaped arc? Compare the same movement crouched. Keep feet still; avoid
deliberate leaning. Ordinary small motion from physically moving your eyes is
expected; the target is the exaggerated curved sweep. A standing-only residual
means this fix is incomplete; a new crouched problem is a regression.

## Historical initial candidate

# VR-106: standing pitched-head roll arc

## Scope and status

Branch codex/vr-106-standing-pitch-roll-arc is based on
codex/vr-50-cinematic-fov-and-hands at6f85417a, as explicitly requested.
PR57 targets the cinematic branch. PR56 and PR57 remain draft/unmerged. This child contains the parent cinematic candidate,
plus one default-off positional correction. No headset test tonight; both builds
must remain independently installable. No game launch and no merge approval.

The reported symptom is a smooth curved camera translation when rolling while
looking up/down, standing but not crouched. It is not an eye flicker. VR-91's
level-roll correction remains valid; this is a distinct steep-pitch follow-up.

## Evidence and limits

Installed baseline234 uses Neck Mode=cancel, standing pivot0.321/0.062 metres,
crouch pivot0/0, RollArc=0. Prior measurements in ENGINE_NOTES establish the
standing pitch arc and the absent crouch arc; zero crouch compensation is correct.
The old roll-free neck calculation falls back to the rolled head-up column when
horizontal forward magnitude is <=0.2, starting at about78.46 degrees pitch.
The camera position path uses the same threshold and then leaves forward/up
pitched and right rolled. Both branches can move a nonzero standing correction
on a roll arc. Crouch's zero correction removes that contributor.

A deterministic x86 regression at85 degrees pitch and +/-40 degrees roll
reproduces lateral modeled motion -20.633/+20.633 uu with the standing pivot,
plus vertical curvature. Crouch's zero pivot produces zero modeled motion.
This reproduces a code defect, not a new rendered/headset measurement. The
reported pitch range is not measured, so a residual at moderate pitch may need
a separate explanation. No new playtest log exists; baseline234 remains untested.

## Candidate

Neck.UprightPitchArc defaults0. With1 and RollArc=0, compute the measured pitch
arc directly in yaw-frame R/U/F: R=0, U=b*(cos(p)-1)+f*sin(p),
F=-b*sin(p)+f*(cos(p)-1), times world scale, then apply the existing mode sign.
Clamp p to the controller's existing +/-16000 rotator limit. This matches the
old roll-free model at ordinary pitch and never reintroduces roll near vertical.
Stance selection/easing and calibrated pivot values remain unchanged.

For camera-lane projection positions, normalize the horizontal forward axis
instead of falling back at0.2; keep world-up and horizontal right. Stereo eye
separation retains the true rolled right vector. At a numerically undefined
exact pole (horizontal magnitude<=1e-6), refuse the positional contribution,
retain the eye offset, and log the refusal. Normal controller pitch is clamped
short of this singularity. No new engine offsets or object writes are introduced;
the existing camera writer and liveness behavior remain in use.

Live A/B: neckupright on/off. F10 Comfort: Upright position at steep pitch.
Save As Defaults persists the flag. Legacy math remains available when disabled.
A rate-limited neck/upright line reports pitch, roll, stance, pivot, legacy/fixed
RUF and modeled difference. It is explicitly a request, not rendered acceptance.
Existing PosTrack.ZAccount will be enabled for this candidate to measure rendered
closure, rejects and ownership. Do not interpret unjoined camera samples as proof.

## Verification and test order

12 x86 tests cover the falsifiable legacy arc, crouch zero pivot, ordinary-pitch
parity, steep-pitch roll invariance, world-up tracked height, unchanged stereo
eye-right, pitch limit, invalid input and the old threshold discontinuity.
Parent39 cinematic math/ownership checks and13 scope checks pass. Release build,
lint and golden INI pass; final identity/remaining checks recorded below.

1. Cinematic234 first, preserved in build/playtest-candidates/cinematic-234.
   Question: in the tilted Empress scene, does physical look remain natural
   without orbit, forced roll or gaze lock? Success accepts parent camera work;
   failure stays on the parent. Mantle remains untested and needs its own launch.
2. After recording/archiving the first run, install the VR-106 bundle with the
   helper tools/install-playtest-candidate.py. The agent runs all commands.
   Question: while standing and looking steeply up/down, does rolling now keep
   the view stable like crouching? Expected: no extra arc, with real leaning and
   ordinary pitch preserved. A standing-only residual rejects the hypothesis;
   a new crouch/lean problem indicates a positional-frame regression.

Every swap verifies DLL/INI hashes, CRLF and a full INI diff and archives both
logs. Verify the new log banner before interpretation. No launch is requested
until the tester resumes. Do not merge either PR based on automated checks.

## Final artifacts and installation, 2026-09-13

Standing candidate237: vr33-hands-working-237-gfd7a830d, compile23:34:10.
DLL SHA256 b9cca3b0e0b77212424d25158807df66178a689f617f4d3d067e5efb975671a5.
Bundle: build/playtest-candidates/standing-arc-237 (DLL, CRLF INI, manifest).
The candidate was installed and hash-verified; full INI diff only added
Neck.UprightPitchArc=1 and changed PosTrack.ZAccount=0 to1. Both logs and prior
files archived at build/playtest-candidates/installs/20260913-233449-048351.
Then cinematic234 was restored for the first deferred test, with both hashes
verified and the exact inverse full INI diff. Restore archive ends233449-390590.
Active install is234, NOT237. Canonical record: build/playtest-candidates/installed.json.
No new game launch or game log exists for either pending candidate.

Final checks:12 x86 standing-arc regressions,39 parent math/ownership checks,
13 camera-scope checks, Release build, lint, nine exports, golden INI and standalone
XR60 frames FOCUSED/zero errors pass. Headset acceptance remains pending.

## Updated installation, 2026-09-14

Installed vr33-hands-working-245-g0cd7b263, Sep14 07:57:48, source 0cd7b2630eb0d0495cfe29368213771394ff122e.
Bundle build/playtest-candidates/standing-arc-245 includes accepted PR56 parent.
DLL SHA256 e67175b9cddc9da5822035310c47a01cc0bd804c8da476fe3f0479943b22d318.
INI SHA256 e9fde6ffb0b01a9c95e5cc0f38ec98c862ce063556349972c1dfab5e35cbb8cc.
Full INI diff: add Neck.UprightPitchArc=1 and PosTrack.ZAccount=0->1 only.
HeadBasedMovement=1 and all accepted cinematic settings preserved. Hashes and
CRLF verified, both logs archived at
build/playtest-candidates/installs/20260914-075910-008829.
12 standing,15 production-facing,17 handoff,39 cinematic math and13 scoped-write
checks pass; release build,9 exports, lint and INI golden pass. No game launch.
PR57 remains draft until the standing/crouched rolled-head comparison is tested.
