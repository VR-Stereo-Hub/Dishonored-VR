# Hand eye identity candidate

## 2026-09-14: VR-95 camera-corroborated hand eye candidate, NOT INSTALLED

Branch `codex/vr-95-hand-eye-identity` starts at accepted VR-Main 85f9ef6e4.
VR-95 was verified in Linear and reopened from Done to In Progress. The latest
request authorizes preparing a new candidate but explicitly prohibits installing
it. Keep installed world build 272-ge3bb7ac2a and its exact accepted INI intact.

Symptom routing: one-frame LEFT-eye-only sideways hand/weapon jump during head
motion, most recently after opening-cutscene exit; sewer-save loading was
reported essentially clean. This is the VR-95 eye-offset class, not VR-112
partial weapon transparency. The latest evidence is archived in
`build/flicker-120/reconfirmed-world` and `hand-test-left-jump`; their verified
identities and limitations remain in HEAD_MOTION_120HZ.md. No new run is claimed.

Source finding: MsDraw bypasses tracked-hand placement during native animation.
Its eye classifier consequently keeps a pre-cinematic position sample. It also
uses hand-model translation to infer the eye, so authored motion and head turns
can shrink a real eye step into the SAME bucket. The old global pose rebasing
candidate did not resolve this and reduced reported world smoothness; none of
that candidate is included here.

New lever `Hands.PaletteEyeRecord`, default 0, live F10 Hands checkbox
"Use camera-verified eye for hands", saved independently. At the first qualified
hand draw in a view, inspect only the front queued render tag and compare its
signed camera position with this draw's c5 shader constant. Accept its eye only
when the tag has a stereo eye, record id and position, queue depth is 1..6, and
the position difference is finite and <= min(0.25 uu, 0.1 IPD). This tolerance
is a conservative guard, not a measured accuracy claim. Published ledger samples
show same-tag residual 0.00 uu versus about 6.81 uu to the adjacent eye.
The queue is NEVER popped, searched, repaired or relabeled by the hand lookup.
Camera movement after the script write can refuse this guard; then the prior
classifier runs unchanged. Unknown means fallback, not proof of a correct eye.

Verified identity overrides small/ambiguous model steps and preserves real eye
repeats. Native animation clears old comparison history while the lever is on.
Hand and weapon offset consumers share g_mpEyeState; controller pose sampling,
normalization, runtime submission, world orientation, positions and frame pacing
are unchanged. No engine-memory writes, offsets or new hooks are introduced.

The old eyecheck looked up a future Present before its record existed. It now
compares the previous observed hand view after that Present has run, at the
existing documented +1 join. Tests cover agreement and disagreement. Unknown
samples remain unknown; zero disagreement without known comparisons is explicitly
inconclusive. Per-view selection logs distinguish V (verified) from fallback,
with record id, camera residual and counts. Identity/camera matching is still a
candidate association that needs headset verification, not proof for every draw.

Host checks: cancelled left steps, initial/post-cinematic identity, genuine
repeats, fallback, and deferred positive/negative audit; all production eye tests
pass. Queue suite: 258 checks pass including nonconsumption, wrong camera,
missing camera/IPD, mono, unwritten position and excessive depth. Existing frame
math, golden INI and lint pass. An initial fallback test retained the historical
predictor from its preceding case; explicitly disabling that separate lever
fixed the test isolation without changing production fallback semantics.

Later one-question playtest, only after installation is requested: after exiting
the opening cutscene, hold both controllers still and turn the head side to side.
Does the one-frame left-eye hand jump disappear while world smoothness stays
intact? Yes supports the eye-offset correction; no requires checking V/fallback
and the deferred eye disagreement before any further timing change.
No game launch, installation or merge is authorized for this candidate.

## Prepared artifact (not installed)

Build 275-g66634db52, compiled 16:00:44, code commit 66634db5270151694ce97f9889a19b85cba65f0b.
Bundle: `build/playtest-candidates/vr-95-hand-eye-identity`.
DLL SHA256 ac3edf424f4b709c5e60aaaacd406c27dc5c045d2d307176a89bc92e7a054b18.
Candidate INI SHA256 c56a948b208819e921f755ebc24bfcb7003b0b5183cb92073d7b1a5588842411.
Read-only candidate verification reports exactly one proposed setting addition:
Hands.PaletteEyeRecord=1. No installation was performed. Before/after hashes of
the installed DLL and INI are identical to the accepted main candidate. Host
checks, release build, exports, lint and golden verification passed.
