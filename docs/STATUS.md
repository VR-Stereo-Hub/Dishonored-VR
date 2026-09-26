## Hand/weapon flicker on fast head yaw (2026-09-26)

Branch `claude/hand-headturn-flicker` off `staging`, not merged, NOT installed (by request). No
Linear ticket (the workspace is at its free-plan issue limit).

- Audit and measurements: FLICKER_REFERENCE top entry. The hands were placed against a head
  sample a fixed two presents back, which the `hv:` line measured on another generation in 8-15%
  of fast-turn frames (up to 1.7 deg); the eye came from a hand-jump guess the yaw sweep disturbs.
- Candidate `[Hands] PoseFromView` (default 0, F10 Advanced > Hands > Head-turn smoothing): a hand
  draw finds its own view by c5 in the pose records and uses that view's head sample and eye;
  no match leaves today's path. Host tests (negative control included), build and lint pass.
- To test: install, tick the checkbox, turn quickly left/right; then untick. Log:
  `hands/poseview:` (the snapshot offset must grow with turn speed).

## 2026-09-26: session accepted; staging integration and next-session baseline

Current state: user accepts the final local run and explicitly authorizes all session
work into staging. PR132 consolidates PR131 cinematic progress/scoped-axis work and
PR128 texture-backed shared capture, preserving their commits and branches. Existing
staging features, including trigger-only sword animation, are retained. VR-Main is
not a merge target. Historical candidate/pending instructions below are superseded.

Accepted locally: hand alignment, cohesive semantic HUD ownership, marker sizing,
lower hint alignment, standalone low-health potion placement, world pause/submenus,
and removal of the reported periodic walking catch-up. Shared-capture slowdown fix
has affected-player reported acceptance. Remote VR-229 early prison judder still
awaits the separate6187b2fd4 package result; do not infer that verdict from local runs.

Installed and retained: v1.0.1-50-g6bc58a449, optimized x86, recorder/pixels/legacy OFF.
DLL SHA256 42a68dc554ebef2cafd27546567a80baf2ae64dfc2e8dc2865b950b0894526db.
INI SHA256 b430fbfe625c409c127516191efee476febb62774800477e7e8979c4e9a970a7,
1535 CRLF, zero bare LF. Installed manifest: primary build/playtest-candidates/installed.json.
Latest banner matched; logs/full INI archived at primary
build/hud-regression-20260925/walk-accepted-002458. No new installation or game launch.
The integrated source additionally contains the latest queued-render correction
and shared-capture fix; it is build/host validated, not the installed build50 binary.

Validation: optimized x86 build;97 semantic-owner checks and100000 transfers;
123 native HUD,503 routing,15 native identity,6 pointer-scan checks;
1686 normal/1687 diagnostic pairing,255 recorder,30054 cinematic FOV,
17 handoff,138 animation checks; native interop162 pixel checks/three resets;
default writer/profile/golden byte comparison. Full results in local build/integration-*.
Performance acceptance and nonzero scan costs are recorded in PERFORMANCE.md.

Next steps: read this entry and docs/dishonored/NEXT_SESSION.md, then take the user's
new task. Start a new codex branch/worktree from fetched origin/staging; do not switch
Claude's primary local/test-282-283 checkout. Preserve current DLL/INI baseline,
rain settings, trigger-only policy and HUD ownership. Do not reopen failed potion
position/census guesses. No extra headset launch required for this handoff.

## 2026-09-26: potion accepted; periodic walking hitch candidate

Matched95ae3f7af run and installed DLL; archived current/previous log and full INI
under primary build/hud-regression-20260925/walk-judder-000731. User confirms potion
HUD placement; quickReady1/quickCaptured rising confirms activation. Preserve fix.

New whole-world walking catch-up has actual53..62ms game-thread waits, often750ms
apart despite8..10ms average ticks. FpCollect runs every750ms and performs repeated
memory probes on every raw slot. Candidate changes its traversal cost, not its
schedule or discovery scope: fresh live set before restores, IsLiveObject before
child access, one range query with original boundary fallback. Six extracted
production scan checks pass. Bounded3s collect-cost distinguishes measured speed
from a merely matching cadence. Installed v1.0.1-50-g6bc58a449 after optimized
build, lint and nine exports passed; see PERFORMANCE and
FLICKER_REFERENCE. No game launch, no stereo/HUD change, no merge/external upload.
DLL SHA256 42a68dc554ebef2cafd27546567a80baf2ae64dfc2e8dc2865b950b0894526db.
INI SHA256 b430fbfe625c409c127516191efee476febb62774800477e7e8979c4e9a970a7.
Entire installed/expected/backup INI matches;1535 CRLF, zero bare LF, no changes.
Game closed and previous hashes verified. Prior accepted DLL/INI/logs backed up:
primary build/playtest-candidates/installs/20260926-001413-008307. Candidate/PDB:
primary build/playtest-candidates/walk-hitch-6bc58a449.
Next one question: does the periodic hold/forward catch-up stop on straight walking?

## 2026-09-25: potion activation veto confirmed; direct owner correction

Returned v1.0.1-46-g36a8d7f95 matches installed DLL/banner, archived with previous
log/full INI at primary build/hud-regression-20260925/potion-root-return-235618.
Reminder still peripheral. Actual mode4 persists with quickReady0/quickCaptured0;
HUD movie census22matches/9different vetoes activation. This was another unarmed
candidate, not evidence about panel position. The nine differences are unexplained.

Remove unrelated all-HUD-clips census prerequisite. Production RefreshQuickMovie
activates from live manager membership/mode4/supported movie view; current Display
receiver must match that exact view and pass the existing identity/epoch checks.
97 host checks/100000 transfers include unarmed activation through queued replay.
No config, position, rain or menu changes. Installed v1.0.1-48-g95ae3f7af
on2026-09-26 after optimized build, lint and nine export checks passed. See ENGINE_NOTES
for measured failure and PERFORMANCE for unchanged capture topology/reduced census.

DLL SHA256 7e8cb4e1b0ff2c3bbb3db01fac02f5a13c00576ae15b1ab41f7e0ae4a1ff3377.
INI SHA256 b430fbfe625c409c127516191efee476febb62774800477e7e8979c4e9a970a7.
Complete installed/expected/backup INI byte-identical:1535 CRLF, zero bare LF.
Prior DLL/INI/logs: primary build/playtest-candidates/installs/20260926-000214-196133.
Candidate/PDB: primary build/playtest-candidates/hud-potion-activation-95ae3f7af.
Game was closed; previous installed DLL/INI hashes matched before replacement.

Next one-launch question: is the independently appearing low-health D-pad/potion
fully visible on the floating gameplay HUD? Read quickMode/quickReady/quickCaptured
before drawing placement conclusions. No game launch, no merge, no external upload.

## 2026-09-25: correct failed potion movie link; audit remaining HUD families

Returned v1.0.1-44-gb52c0c579 matched installed DLL/banner; archives in primary
build/hud-regression-20260925/quick-potion-return-233600. Reminder still peripheral,
performance reported acceptable. All28 movieLink samples0: prior route never
activated. quickMode0 was a short-circuit default, not a measured mode.

Derived actual native movie owner at sprite+BC from constructor and getter;
+90 is a resource definition. Validate primary view table and all supported HUD
clips; independently read mode and count successful potion ownership. Keep existing
mode4/default-panel policy, no extra sink, no INI/placement/rain/menu changes.
85 reader/transport checks plus100000 transfers,123 native-HUD and503 route tests
pass. Installed v1.0.1-46-g36a8d7f95; optimized build, lint and nine exports pass.
Full derivation in ENGINE_NOTES; script coverage
inventory in HUD_ANCHORS; matched-run performance limitations in PERFORMANCE.

DLL SHA256 42ca641b7d45f7e4d33d3d9b772dcfd43f22c6ee3b79d50d5f12affd45e2e234.
INI SHA256 b430fbfe625c409c127516191efee476febb62774800477e7e8979c4e9a970a7.
Entire installed/expected/backup INI byte-identical;1535 CRLF, zero bare LF,
zero settings changes. Game closed and prior hashes matched before install.
Prior pair/logs: primary build/playtest-candidates/installs/20260925-235039-235218.
Candidate/PDB: primary build/playtest-candidates/hud-potion-root-36a8d7f95.

Next single launch question: does the standalone
low-health D-pad/potion join the visible floating lower-HUD panel without opening
the wheel? Read movieLinked/mismatch, actual quickMode, quickReady and
quickCaptured before making further placement changes. Broader audit identified
separate global/FX and DLC05 trial families; these need selective native mapping,
not blanket capture. No game launch or merge. External disclosure still pending.

## 2026-09-25: quick-potion ownership correction; tutorial trial rejected

Installed v1.0.1-44-gb52c0c579; optimized build, lint and nine exports pass.
DLL SHA256 fc0ab766a28d82d4e1245473171d77e8209aa0de1f67a17efde561e6b3caae5e.
INI SHA256 b430fbfe625c409c127516191efee476febb62774800477e7e8979c4e9a970a7.
Full byte comparison with expected and backup confirms only tutorial WinY reset;
1535 CRLF, zero bare LF. Rain18/KeepSize0 retained. This INI also matches the
accepted fe3c3f876 placement profile exactly; relevant setting semantics unchanged.
Previous pair and logs: primary build/playtest-candidates/installs/20260925-231630-153352.
Candidate/PDB: primary build/playtest-candidates/hud-quick-potion-b52c0c579.
Game was closed, expected prior DLL/INI hashes matched; never launched the game.
Next single question: does the low-health D-pad/potion join the visible HUD panel?
A failure requires checking movieLink/quickMode/quickReady before further changes.

Verified returned e322911fb DLL/banner; current/previous logs and full INI archived
under primary build/hud-regression-20260925/heal-return-230454. User clarifies the
missing low-health item is only the D-pad/potion artwork, not tutorial text.
The previous identity assumption was wrong. Native/script audit now identifies
QuickPotionMenu in the POWER-WHEEL movie, mode4. It is absent from the HUD32-clip
array, so the semantic unknown/native fallback explains its uncaptured position.

Remove the failed private tutorial panel and restore its WinY to0.000. Candidate
routes only the validated mode4 wheel movie to the existing default HUD panel.
Read-only reflected manager membership/mode/pMovie, live UObject identity and
current native sprite/movie relationship authorize capture; normal wheel mode
and all menu routing stay unchanged. No additional capture sink or engine writes.
The optional relationship is cross-checked against current known HUD clips before
use and fails native if unavailable. Existing3s summary records movieLink,
quickMode and quickReady. Headset acceptance and live relationship guard pending.
81 actual reader/transport checks,100000 concurrent transfers,123 native HUD and
503 routing checks pass. Performance and failed hypothesis are documented below.
Rain reset was explicitly requested: Lens18/KeepSize0 confirmed in returned log.
Preserve that reset on install. No launch or merge; external disclosure pending.

## 2026-09-25: remaining heal reminder and rain follow-up

Installed v1.0.1-42-ge322911fb after successful optimized build and nine exports.
DLL SHA256 c15e845fa185ee2260974afe0ed5cb4e1dfe62aa75230fc8e482f1bb2380d033.
INI SHA256 1457d5db02ac98d6eb60e66443f6dc8a4f3ab6a4dbde613fa7f365dad08b3420.
Full expected/backup comparison: only tutorial WinY0.000->0.200;1535 CRLF,
zero bare LF. Current Lens13/KeepSize1 preferences retained. Previous pair/logs:
primary build/playtest-candidates/installs/20260925-224920-433895.
Candidate/PDB: primary build/playtest-candidates/hud-tutorial-e322911fb.
Game was closed and previous hashes matched; no game launched or merge performed.
Next single launch question: is the low-health reminder fully visible/readable?
Rain clarification remains pending; no rain fix is claimed.

User accepts marker sizing and lower sneak/vault alignment on fe3c3f876;
low-health reminder remains peripheral and rain appearance is reported changed.
Verified installed DLL and matching log, archived current/previous log and full
INI in primary build/hud-regression-20260925/rain-heal-return-223356.

Tutorial roots15/16 were identified but unmeasured, so still shared default's
scale1.420 and offsets .274/-.093. Give identified tutorials a private full-image
panel using their own existing controls; no rectangle-based ownership or clipping.
Release that occasional sink after the existing two-present delivery grace to
avoid continuing its copy after the reminder fades. Intended local placement is
tutorial WinY0.200, WinX0 and scale1; the20cm lift is a comfort choice, not a native
measurement. All accepted gameplay ownership, markers and menus stay unchanged.
129 native-HUD checks including six semantic panel cases pass. No new logging.

Rain remains OPEN, not claimed fixed. Rain/lens/scene-draw source is unchanged
from accepted pre-ownership0a4c7c254. Startup Lens18/KeepSize0 changes during this
run to13/1; preserve current preferences pending identification of close lens rain
versus outdoor particles and last accepted rain baseline. Unknown semantic draws
now stay native, so an indirect routing difference is not excluded by this diff.
No speculative rain engine or rendering patch. A clarification is pending.
Do not push diagnostic findings externally while disclosure approval is unresolved.

## 2026-09-25: semantic ownership accepted; marker and hint correction

Matched installed4c38bf526 DLL/banner, archived current/previous logs and whole
INI under primary build/hud-regression-20260925/semantic-return-221601. User reports
cohesive HUD with no observed ungrouping/jitter. Remaining marker size and lower
hint placement issues are scoped follow-ups, not failure of queue transport.

Marker roots were rejected by owner==HUD. Native constructor/caller disassembly
shows +8 is task/collectible/enemy identity. Fix independently validates that live
target plus current HUD array membership and native family. Correct context,
special/mantle and QTE clip routes to share sneak/player-state's default panel;
retain info/talk/use on prompt. Heal-reminder placement improvement is expected,
not yet measured. Accepted pause and ownership transport remain unchanged.

70 production-reader/transport checks,100000 concurrent transfers,123 native-HUD,
503 routing checks pass; normal optimized build and lint pass. Family/pivot
counters fit in the existing3s summary and only compute when it prints.
Installed v1.0.1-40-gfe3c3f876; DLL SHA256
04819c3e7b2a2bb7da3b7912b268d486d51881c102cd27e5e0f42e210c617459.
Installed/expected/backup INI match byte-for-byte; SHA256
b430fbfe625c409c127516191efee476febb62774800477e7e8979c4e9a970a7.
1535 CRLF, no bare LF, zero INI changes. Previous DLL/INI/log pair archived in
primary build/playtest-candidates/installs/20260925-222521-353610.
Candidate/PDB: primary build/playtest-candidates/hud-placement-fe3c3f876.
Optimized build and nine exports pass. All six latest user INI adjustments retained:
task/rune inset0.260; default X0.274/Y-0.093/scale1.420; prompt scale1.320.
One next launch question is existing size-slider response on a complete objective.
No game launch, no merge; external diagnostic disclosure remains pending.

## 2026-09-25: native widget ownership implementation candidate

User accepts world pause on 0a4c7c254; verified DLL/banner and three RIDING Pause
entries. Logs/INI archived in primary build/hud-regression-20260925/pause-accepted.
Implemented native HUD root identity through queued render commands on existing
codex/vr-188-hand-hud-followups. No change to the primary collaborator checkout.

The candidate replaces gameplay rectangle/hash association with validated native
widget membership. Prompt children share prompt; cooking shares the aim reticle;
task/Heart/awareness instances remain native. Unidentified work stays native.
Queue lifecycle guards cover address reuse, generation, saturation and exceptions.
SemanticOwnership defaults off; intended local candidate INI adds only that key=1
against the accepted pause pair. OwnerTrace remains 0; recorder/pixels/legacy OFF.

Validation: optimized x86 build; 49 ownership checks including actual assembly
stub and 100000 cross-thread transfers; 123 native-HUD, 503 old-routing and 107
menu-policy checks. Host timing limits in PERFORMANCE; native derivation and
contracts in ENGINE_NOTES; behavior and remaining acceptance in HUD_ANCHORS.
One launch question is Emily prompt cohesion while pitching the head with the
controller held on her. Objective depth, full gauge behavior and in-game cost
remain open; do not claim all original regressions resolved. No game launched.
Installed v1.0.1-38-g4c38bf526, DLL SHA256
06c59fd845e0e5090378234696b2b03ea917fde9b554b34df6b56ce4d9c3db24.
Expected and installed INI SHA256
6c0e336882e1c1b4e4d2d9d55e142785f2f9c20206a70974053d8890c43e1877.
Full byte comparison confirms the sole addition [Hud] SemanticOwnership=1;
1535 CRLF, zero bare LF. Previous DLL/INI/current+previous log archived together
under primary build/playtest-candidates/installs/20260925-215531-781812.
Candidate and PDB: primary build/playtest-candidates/hud-semantic-4c38bf526.
Game was closed; prior hashes still matched the accepted pause pair. No merge authorized.
Prior Linear diagnostic-disclosure approval remains unresolved; keep new work local.

## 2026-09-25: ownership capture returned; pause readiness repair

Verified returned ea83dc5be DLL/hash/banner and archived logs plus full INI under
primary build/hud-regression-20260925/owner-return. Prompt still changes to the
objective under head pitch. Pause was reported as the whole flat screen; log
confirms configured pause=world but ui/ride refused healthy=0, failed=0. Capture
was armed with empty HUD frames before entry. Readiness incorrectly depended on
visible widgets rather than a proven operational pipeline.

Installed v1.0.1-36-g0a4c7c254 (optimized x86), DLL SHA-256
1744b77c562765d315ab3828b67941da8f9a5995305e18d74d52fb51d4323405. Expected full INI
SHA-256 372a802192237a8944365f5319ee72c31d31272038f78093d0706db3dd8c81ee matches
the installed file; only OwnerTrace 1->0 changed, CRLF preserved. Backup pair and
logs: primary build/playtest-candidates/installs/20260925-210631-493709. Build,
118 native-HUD checks, 107 ride-policy checks, lint and nine exports pass. Game
not launched. Next single behavioral question: after the interaction HUD fades,
does pause remain a world-positioned panel over the scene instead of the flat
whole-frame fallback? This does not accept or fix prompt/objective ownership.

Candidate keeps a previously exercised capture path ready through armed empty
frames, with the same 500 ms inactive grace and resource/failure/reset gates.
No new engine writes or recurring diagnostics. Native HUD host checks cover
faded widgets, the owner-poll gap, clock wrap, failure, lost handoff and reset.
HUD ownership remains unfixed: the new capture proves native widget updates and
rendered draws cross a deferred queue. Task character/vtable, queue execution and
producer paths are now established offline; next implementation must transport
owner identity through the queue, including filter composites and cached work.
Do not substitute another rectangle or content hash as semantic ownership.

Three timed renderer captures total 56.8 us in this run; see PERFORMANCE for
measurement limits. No additional diagnostic launch is needed for this boundary.
Linear findings permission is still pending. No merge is authorized.

## 2026-09-25: HUD acceptance failed; native ownership audit

Verified local 1ed638c01 and archived all logs/INI. Other hand/swing/menu follow-ups
reported satisfactory. HUD prompt/objective overlap, objective depth, cook-gauge
placement/size and performance are NOT accepted. The latest precedence patch
only protects task text; returned content is also claimed as task icon/continuity.
Task/Heart/awareness bounds and gauge heuristic predate v1.0.0. Full release source
and INI comparison, decompiled HUD/task/objective/Heart/charm/Flash reread completed.

HUD_ANCHORS contains the evidence and replacement contract. ENGINE_NOTES records
the read-only native identity derivation. PERFORMANCE holds cost attribution:
recorder/pixels off, 73.7 ticks/s and 10.3 ms GPU near grenade, pereye culling now active
on a newer DLL, no controlled release performance baseline. Do not claim a full
performance cause or undo accepted hand/swing/visibility behavior without evidence.

Installed capture build v1.0.1-34-gea83dc5be with DLL SHA-256
63fc60a7bc28e115f7cd2ce664b602655516f4cb45be37945c124b9c68de3fe7. Full installed
INI matches the archived expected candidate; the only semantic change is
[Hud] OwnerTrace=1. CRLF preserved. Previous DLL/INI/current and previous log are
backed up under primary build/playtest-candidates/installs/20260925-172045-668268;
all ten prior logs were already archived for the audit. Normal optimized x86 build,
15 identity, 107 native HUD and 503 routing checks pass; lint clean. Flicker CPU
recorder, GPU pixel probe and legacy code compile OFF. Game was not launched.

Linear VR-186 was reopened. Automatic approval review blocked uploading the new
HUD findings; explicit permission requested and still pending. New investigation
commits remain local while that diagnostic disclosure is unresolved.

Prepared default-off finite OwnerTrace to establish native clip-to-render transport:
16 renderer stack snapshots, once per route family; at most 4 native attempts per
marker family, successful capture once. No GPU readback or engine writer. This
is a structural rework prerequisite, NOT a HUD fix. Next single capture question
is the reported prompt split during head pitch, then quit so the log can be read.
Do not require another remote prison test or merge this draft. Implementation and
headset acceptance of semantic ownership, marker depth and gauge routing remain open.

## 2026-09-25: local hand/HUD follow-ups and compatible build baseline

Branch codex/vr-188-hand-hud-followups starts at staging474fc8a55. Carries the
VR-229 source candidates31450526c/c4f5fe5df/9da0a0b48 plus a deferred empty-hand
wrist-reference capture (VR-188) and observed-interaction/task-text precedence
(VR-186). Local stable intro/submenus were tested on c4f5fe5df, not remote9da0a0b48.
The remote ZIP and draft PR131 remain awaiting their own report.

Sword regression is explained by the old DLL/new INI pair: c4f5fe5df interprets
HandAnimMelee=1 for all melee; the previous868d09649 log has source=SWING with
hand-back off. Staging already contains the trigger-only policy (VR-220). No new
sword policy change is needed. CLAUDE.md now requires expected target-build INI
semantics, whole-file comparisons, CRLF, and compatible DLL/INI rollback pairs.

Validation: optimized build, frame_test (five new wrist cases),503 HUD routes,
1678 stereo pairing checks,30054 cinematic FOV/handback checks,138 animation catalog
checks, golden/default profile, lint and9 exports pass. No game launched. Measured
recorder burst1.606ms; follow-up builds with CPU/GPU diagnostic compile flags OFF
and expects FrameId=0 in the local INI. Details: PERFORMANCE.md, ARM_HAND_SPLIT.md,
HUD_ANCHORS.md, FLICKER_REFERENCE.md. New visual fixes remain candidates.

Installed candidate v1.0.1-32-g1ed638c01, optimized x86, DLL SHA256
b6fda98f04b9d8433ff0b6fde35ec821f7acdb94d870d048b9c918dd99dbb569.
Primary ZIP: build/test-packages/DishonoredVR-hand-hud-followups-1ed638c01.zip,
15907944bytes, SHA256954af6e4d6b89ce0294ecb012bed81cc19c911066b38b5e03a1305f030f4c441.
Backup in primary build/playtest-candidates/vr188/install-20260925-130330 includes
DLL, entire INI and all logs. Full target/backup INI diff has exactly two changes:
Perf.FrameId1->0 and explicit Anim.HandAnimMeleeSwing=0 (was absent/default0).
Other bytes preserved;1533CRLF,zero bareLF. Expected/installed whole INI SHA256
15baadbfcfe11bf45075477ef9acb341359f129258de018234bf17689beac7b4 matches.
No game launch; current log still c4f5fe5df until the next tester launch.

Next ONE launch question: after opening cinematic, does the right hand remain
aligned? A pass supports the capture fix; a repeat requires the matching reference
lines. HUD/sword acceptance remain open and can use this same candidate. Remote
9da0a0b48 ZIP stays unchanged. Do not merge without explicit permission.

## 2026-09-24: staging is the integration branch; VR-Main is the release (VR-218)

The 1.0.1 hotfix chain (PRs #114-#117, tag `v1.0.1`) was fast-forwarded onto `VR-Main` on the
user's instruction, so `VR-Main` is `f5176aeae` = `v1.0.1`. `staging` was created from that
tip. Open PRs #74, #62, #2 and #118 now have base `staging`.

From here: branch off `staging`, PR against `staging` (`gh pr create --base staging`; the
default branch stays `VR-Main`), `Fixes VR-<n>` on the first line, merge into `staging` only
with the user's explicit yes. `VR-Main` moves only by the release PR (`staging` -> `VR-Main`)
the user merges, and its tip is always the latest release tag. Linear: Done = merged to
`staging`, Released = carried into `VR-Main` and tagged. Rewritten: `CLAUDE.md`,
`docs/LINEAR_AND_GITHUB.md`, `CONTRIBUTING.md`, `AGENTS.md`, the PR template, the decision log.

**Owed by the user (UI-only):** the Linear automation row "On PR or commit merge -> Done"
restricted to base `staging` (Settings > Team > Issue statuses and automations); optionally a
branch protection rule on `VR-Main` so only the release PR can write it.

Next: VR-219 (snap turn) and VR-220 (trigger-only sword animation), each on its own branch off
`staging`.
## 2026-09-25: snap turn (VR-219), simulator-proven, headset owed

Branch `claude/vr-219-snap-turn` off `staging`. `[Turning] SnapTurn=0` (default off), `SnapAngle=45`,
`SnapThreshold=0.6`, `SnapRearm=0.3`, `SnapRepeatMs=0`; word `snapturn`; F10 > Controls > Turning.
The present lane detects the stick edge (`snap_turn.cpp`, after the F10 pointer block in
`pad_bridge.cpp`) and eats RX only while the script camera writer is fresh; the script lane
takes the step once in the head writer's fresh branch as BODY yaw (`rot[1] += headDeltaU + snapU;
YawPublish(viewInU + snapU, headDeltaU)`), so the pawn turns with the view in both movement modes.

**Simulator (Debug build, `tools\xrsim\snap-turn.xrs`, 86 steps, under `movement head` and
`movement character`):** four pushes at 30 deg each printed FIRED / APPLIED / HONOURED; view and
body since mark both 119.99 deg, head 0.00; a held stick fired once; a 0.4 push neither fired nor
smooth-turned (view stayed at 59.996); a head turn afterwards moved the view (70) and not the body
(90); a stick held into and out of the pause menu did not fire (the first cut fired on the resume:
the detector now disarms while the lane is blocked); `snapturn off` put RX=29043 back on the pad
line. 14 HONOURED, 0 NOT HONOURED, 0 NO CONSUMER over the session; four steps added 0 stale eye
submits (132 -> 132), eyes 0/0. `tools\yawtest-host.ps1` bookkeeping: 8 of 8 PASS (case 8 is the
snap step as body yaw). Logs: `build/playtest-candidates/vr219-snap-turn/sim-run2/`.

**Headset owed:** one push = one crisp step; hands, sword, reticle and prompt stay in front and a
hit lands on what is now in front; walking goes the new way; pause menu and wheel still navigate;
keyhole and cinematics turn smoothly. The tester's installed build 711 and its ini were restored
after the sim runs; install a Release build of the branch to judge it.

**Deliberately not here:** the yaw OWNERSHIP half of `tools\yawtest-host.ps1` does not compile
(its slice predates the live-object table; the bookkeeping half runs again after the slicer was
pointed at `yaw_book.h`); a ticket is filed.
## 2026-09-25: the sword animation follows the trigger, not the swing (VR-220), simulator-proven

Branch `claude/vr-220-trigger-sword-anim` off `staging`. A trigger sword attack plays the game's
swing on the tracked hand and hands it back; a physical swing keeps the arm. `melee.cpp` publishes
each FIRE (tick, pulse close, count, real-trigger overlap; atomics, stores only); `anim_state.cpp`
classifies each attack once (per state entry, or per combo clip more than 100 ms in): SWING if the
state was entered with the pulse open or within 80 ms of its close (combo: within 600 ms of the
fire), else TRIGGER; only TRIGGER sets the hand-back. `kGateBody` reads `cameraAction`, not `game`,
so a trigger hand-back does not refuse the swing that follows. `[Anim] HandAnimMelee=1` (moved 0 -> 1
once by `HandAnimMeleeRev`), `HandAnimMeleeSwing=0`; `anim melee on|off|swing on|off|status`; F10 >
Hands > Game arms during actions. The hand-back owns the RIGHT hand only (the headset asked for
the left to stay free): `Snapshot.handMask`, per-hand `blend(D, hand)`, per-hand SkelControl
release; `HandAnimMeleeBothHands=0`. A first cut deadlocked the present thread on the first
status write (`weight_for` under the shared lock); fixed, launch clean, `swing-anim.xrs` (56)
and `snap-turn.xrs` (88) pass on the merged RelWithDebInfo build (sha256 A699EDD4...).

**Simulator (`tools\xrsim\swing-anim.xrs`, 56 steps; Debug and RelWithDebInfo):** trigger pull ->
`source=TRIGGER -> hand-back ON`, `features.anim.meleeSource=trigger`; swing-edge move -> `swing:
FIRE`, `source=SWING ... hand-back off` (fire dt 15 ms, pulse open), `HONOURED`; a swing 120 ms after
a trigger pull fired, was not `BLOCKED ... owns the body`, and classified as a combo (fire dt 266 ms,
pulse closed 141 ms before); `anim melee swing on` -> the same swing `hand-back ON`; `anim melee off`
-> the refusal line. `swing-edge.xrs` passes on both builds. `swing-gates.xrs` failed 4 of 4 runs on
a different leg each time, always a hand move cut short by a sample gap, never the body gate: VR-222.
Migration (plain Steam launch, the tester's archived ini): `HandAnimMelee 0 -> 1 (one-time ...)` and
`HandAnimMeleeRev=1` written; a deliberate 0 with the key stayed 0. TRAPS: `-ViaSteam` restores the
ini and wipes a startup write. Logs: `build/playtest-candidates/vr220-trigger-anim/`.

**Headset owed:** a trigger slash animates and the hand returns (150 ms in, 250 + 150 ms out); a
physical swing keeps the hand on the controller with the hit landing; combos; a swing right after a
trigger slash; block; mantle; a cinematic; a drop takedown; the trail still hidden. The tester's
installed build 711 and ini were restored after the runs.
## Index controller tuning from the headset (VR-224, 2026-09-24)

Branch `claude/vr-224-index-tuning`, stacked on VR-223's branch, not merged.

- Two commits cherry-picked from a community fork (`index-controller-offsets`,
  author kept): an Index frame correction, hold and sword trims in the SteamVR
  shim, an empty-left-hand re-pose and aim lift in the mod, a force-sensor grip
  binding. Every part is now behind `[Controllers] IndexTuning` (-1 auto, the
  default: on for a launcher headset of Valve Index, Bigscreen Beyond 1 / 2 or Vive Pro 2).
  Details and the table: docs/INSTALLER.md, VR-224 section.
- Verified: Release build, lint, default-profile-host (golden and packaged ini
  regenerated, +4 lines), offscreen render of the picker's Index note. Installed.
- NOT verified: anything on an Index rig. This machine's headset is a Quest 3,
  so here the tuning resolves off and nothing should change; the log line
  `config: [Controllers] IndexTuning=-1 -> off` says so.

## Launcher headset selection (VR-223, 2026-09-24)

Branch `claude/vr-223-launcher-headset` off `staging`, not merged.

- The launcher asks which headset the player has before anything else. With
  none recorded it is a modal with no close control; Continue unlocks on a pick
  (or a typed name for Something else). Change on Setup and Manage reopens it.
- The list is the BioShock Remastered VR mod's Setup.bat question, same order.
  Recorded only: no setting follows from it.
- Stored in %LOCALAPPDATA%/DishonoredVR/launcher.ini [Headset] Model. Printed in
  the launcher log, in the mod log (`config: headset (user reported in the
  launcher): ...`) and in the support bundle manifest. Why not the mod ini:
  ARCHITECTURE decision log, 2026-09-24.
- Verified: Release build clean, lint clean, support-collector-tests PASS,
  offscreen renders of `headset-required`, `headset-other`, `headset-change`,
  `manage`, `setup-found` checked by eye.
- NOT verified: a real first run clicking through the picker, and the mod log
  line in a live session. The install was refused because the game was running
  (d3d9.dll locked); run `tools\install.ps1 -Release` once it is closed.
- Next: per-headset controller defaults (BRVR's d-pad modifier and WMR layout
  fixes) would be a separate ticket if wanted.
## Crossbow stray piece on the mirrored side (VR-225, 2026-09-24)

Branch `claude/vr-225-crossbow-mirror-caps` off `staging`, not merged.

- Reported: firing the crossbow shows part of what looks like the empty model on
  one side; it stays once the crossbow is empty.
- Cause as reasoned, NOT measured: the VR-138 mirror reflects the reference pose
  before skinning, so copied triangles and hole caps on the limbs keep the limbs'
  own bones and swing about the wrong pivot when the limbs move. Full write-up:
  WEAPON_MIRROR_PLAN.md section 6f.
- Change: `[Mirror] BodyBoneOnly=1` copies and caps only geometry rigid on the
  weapon's body bone; `mirror body off` restores the old copies for an A/B.
- Verified: Release build, lint, golden ini. Installed (this build is off
  staging, so it does not carry VR-223/VR-224).
- HEADSET-CONFIRMED 2026-09-24: the piece is gone while firing and when empty.
- Headset question: is the piece gone when firing and when empty, and is the
  far side still filled? Log: `mirror/skin:` and the `on a moving bone` counts.
## One eye hides objects from the other (VR-79, 2026-09-24)

Branch `claude/vr-79-occlusion-per-eye` off `VR-Main`, not merged.

- Reported again: covering an NPC's head with the sword in the left eye only
  makes it vanish from the right; doors and mechanisms too.
- Cause: `reentry` draws both eyes through one view state, so occlusion-query
  results from one eye cull the other (ENGINE_NOTES "VR-79").
- `[Stereo] Occlusion=native|pereye|off`, live `occlusion <mode>`, default native.
  `off` (the engine's TOGGLEOCCLUSION switch) was HEADSET-CONFIRMED to fix it but
  read laggier. `pereye` gives the right eye its own engine-allocated view state
  for pass 2, so each eye culls only what it cannot see and culling still saves
  its draws.
- Verified: Release build, lint, golden ini. Installed with `Occlusion=pereye`
  in this PC's ini. This build does NOT carry VR-225 (the crossbow fix).
- Headset questions: does the sword/head test pass with `pereye`, and does it
  feel like native rather than like `off`? Log: `occlusion/pereye: allocated`
  once, then `beat swaps` climbing. Watch for anything wrong in the right eye
  only after a level load (the GC risk in ENGINE_NOTES).
- HEADSET 2026-09-24: `pereye` fixed the one-eye culling with no visible perf cost. Grass
  blinking out for a frame or two while walking is NOT pereye: it happens under native too, in
  both eyes (VR-226, open).
- A second run on the build with the F10 switch for the three modes started in pereye but never
  swapped: no `occlusion/pereye: allocated` line and no beat, so OcclusionPass2Begin found no
  local player and returned without logging why. Suspect: the IsLiveObject check on the player
  controller or local player against a live-set snapshot that predates the level load. pereye can
  therefore silently not engage on a given run. Fix when this is picked up again: log the refusal
  reason (throttled) and drop the snapshot liveness requirement for the controller the head
  tracker already validates. The F10 switch stays, in the Advanced view (2026-09-25).

## VR-229 queued-render candidate packaged (2026-09-25)

ZIP in primary checkout: build/test-packages/DishonoredVR-VR229-prison-judder-fix-6187b2fd4.zip,
15894487bytes. Build v1.0.1-12-g6187b2fd4, optimized x86, legacy OFF,
CPU recorder ON (output spread), GPU probes OFF. DLL SHA256
1774ce5d05e837b4a7f34a5502b022e1e439665931264db973387e7951863dc6.
ZIP SHA256b6ae714fa367ac529160b4586d9b2fd8a55faeca1fe268f47fea5fb3c2b50de4.
Clean source identity, PE machine,9 exports, ZIP CRC/member hashes, build flags and
lint verified. The support manifest's actual d3d9 hash matches the returned9da ZIP;
its installer record still names the underlying release and is not the running DLL.
No INI change: same config consumers as9da, including HandAnimMelee=0 in this tester's
INI and forced pixel suppression. DLL/README/manifest/checksums only, no installer.

Local installed hand/HUD candidate1ed638c01 hash remains
b6fda98f04b9d8433ff0b6fde35ec821f7acdb94d870d048b9c918dd99dbb569.
No install or launch during this work. Remote question: smooth prison from beginning
through fade and10seconds of gameplay, with head-turn fusion retained? Return support
either way. Candidate sufficiency remains OPEN. Shared source in PR132 needs explicit
integration when results are accepted; neither PR is authorized to merge.

## VR-229: scoped-eye partial acceptance and queued-render candidate (2026-09-25)

Returned support-20260925-150139 current banner9da0a0b48 matches the scoped-eye ZIP.
Headset report accepts head-turn eye separation repair; prison judder remains early
then resolves. Stale/expiry/duplicate counters rise to192/58/526 then flatten in the
later dialogue; a later transition adds further events. See FLICKER_REFERENCE for
identity, exact intervals, counterpredictions and limitations. Do not call fully fixed.

Candidate tolerates one unchanged Present interval after observed progress, while a
second quiet interval still refuses and fresh-camera/other gates remain. Production
helper old200 singles/400 queued ticks vs new0; pairing1686 normal/1687 recorder,
cinematic30054 pass. Recorder preserves the same window but emits at most one frame
per Present;255 actual recorder checks pass. Host peak remains scheduler/IO-sensitive,
so no negligible-tail-cost claim (PERFORMANCE.md).

Next: finish clean optimized packaging on the returned tester baseline; one remote
prison-through-fade stability question, support ZIP either way. Local hand/HUD build
1ed638c01 is installed in a different worktree/PR132 and MUST remain untouched during
this investigation. PR131 tracks this remote candidate; shared source with PR132 must
be reconciled before any explicitly authorized merge. No game or simulator launched.

## VR-229 scoped-eye replacement packaged (2026-09-25)

Replacement ZIP: build/test-packages/DishonoredVR-VR229-scoped-eye-fix-9da0a0b48.zip in primary checkout,
15894528 bytes. Build v1.0.1-10-g9da0a0b48, optimized x86, legacy OFF, CPU recorder ON,
GPU pixel probes OFF. DLL SHA256 1150f68ce9e5bd7957e70dd07973f78cd9ff3fbe00f91c093121ca7ed0b4c3c4.
ZIP CRC, all member checksums, extracted DLL bytes, embedded clean source identity,
x86 PE and9 exports verified. Normal and recorder builds pass, lint clean.
Source commit9da0a0b48 pushed to draft PR131, base staging; no merge/release.
The prior c4f5fe5df candidate remains installed locally and is untested by the
maintainer; this follow-up did not install or launch anything. Recommend the
replacement instead of testing the rejected prior candidate. Remote acceptance
and pause-submenu benefit remain OPEN. One prison head-turn/fade test, then support.

## VR-229: returned candidate rejected; scoped eye-axis repair (2026-09-25)

Current state: support-20260925-130534 current log verifies v1.0.1-8-gc4f5fe5df.
The tester reports reload-dependent Empress/prison alternation and new severe
head-turn separation. The previous candidate is NOT accepted. Maintainer installed
that exact DLL earlier at explicit request, but has not launched it; no installation
or game launch during this investigation. Previous DLL/INI/logs remain backed up.

Measured/source-confirmed: scoped camera writes offset eyes along the composed
head-look right vector, while reentry reads cached native camera rows. Returned
P67390 is a full6.57uu stereo step; the published axis differs by54.1degrees and
reports5.322uu perpendicular motion. The actual record orientation reduces that
to about0.001uu. Existing camera-confirmation guards cannot recover a late tag with
the wrong basis. Correct the published stereo axis, preserving translation axes,
camera writes and arbitration thresholds. Applies to cinematic/pitch/menu scopes.

Validation:225 rotated late-tag schedules pass; old-axis control has2184 identity/
repair failures. Actual rounded P67389/90 camera-step regression passes. Pairing
1678 normal/1679 diagnostic checks pass, cinematic math/scope checks pass.
Bounded atomic publication/read host cost0.091us/sample; no extra per-frame logging
or GPU probes. Remote sufficiency remains OPEN. Full evidence and caveats in
[FLICKER_REFERENCE](dishonored/FLICKER_REFERENCE.md); costs in PERFORMANCE.md.
Next: finish optimized builds and package one replacement ZIP, no local install.
One test question: does the formerly bad prison scene stay fused through normal
head turns and its fade into gameplay? No additional diagnostic matrix requested.

## VR-229 packaged acceptance build (2026-09-25)

ZIP: build/test-packages/DishonoredVR-VR229-prison-fix-c4f5fe5df.zip in the primary
checkout, 15893518 bytes. Build v1.0.1-8-gc4f5fe5df, optimized x86, legacy OFF,
CPU recorder ON, GPU pixel probes OFF. DLL SHA256
115f3827362e58b7a83a43dbc2d8d459256154859f5cb701565a63318c2bf518.
Nine exports, ZIP CRC, extracted DLL hash, x86 PE, clean build identity and
compile flags verified. Normal build also passes with both diagnostic flags OFF.
Draft PR131 targets staging. The package retains the previous tester baseline;
it does not bundle newer staging features. No local install or game launch.
One acceptance run: prison cinematic through fade and10seconds of gameplay,
then quit and send support. Source/host-confirmed gate defect; remote result open.

## VR-229: prison flicker repair candidate (2026-09-25)

Returned diagnostic31450526c reproduces the prison failure and healthy Empress/
gameplay controls. Whole-eye delivery, not square FOV. Source defect: present
progress during the previous draw is ignored by its return-time baseline,
provoking SINGLE draws. Candidate compares draw entries; all other gates and
pairing safeguards stay. Production regression old-policy199 false stalls vs
new0; normal/diagnostic pairing1447/1448 pass. Remote sufficiency is still open.

CPU flight history remains enabled in the test DLL, with frame-id GPU probes
forced off regardless of INI. Pixel issue timer alone did not measure total cost.
Actual recorder host benchmark averages1.660us/frame with50 uploads and buffered
file logging; full reasoning in PERFORMANCE.md and FLICKER_REFERENCE.md.
No maintainer install or game launch. New codex/vr-229-prison-present-progress branch retains the tester baseline;
PR126 was closed after collaborator integration, so a new draft review follows; prior VR-227/228 fixes retained, VR-260 remains separate. Next: one prison
cinematic through fade and10seconds of gameplay, return support ZIP either way.

## VR-229: remote diagnostic coverage expanded (2026-09-25)

User has a newer collaborator build locally: NO INSTALLATION and no game launch.
Prepared a self-arming ZIP-only diagnostic build in build/worktrees/vr-227.
The previous expiry-only trace missed consecutive frames after its lifetime cap.
New recurring history joins raw tag/camera decisions, capture identities, delivered
pose records and actual XR release/submission. Independent-label pixel bursts and
camera-upload census cover mono/black images, half-IPD/zero camera samples, source
writes, slot reuse and pose/cadence alternatives. Pairing behavior is unchanged.
[Evidence, hypothesis matrix and next test](dishonored/FLICKER_REFERENCE.md).

Host validation:1142 diagnostic pairing checks,1141 normal,133 actual recorder
checks pass. Includes one-hour recording and missing-left negative control.
Normal and diagnostic optimized x86 builds pass, legacy off; lint and9 exports pass.
No headset/game validation. Clean test build `v1.0.1-6-g31450526c`.
DLL SHA256 `1522d2325f19b302a609490a34c26b4e4a31b539df3e31d3bb5f1e628bbb569d`.
ZIP in primary checkout: `build/test-packages/DishonoredVR-VR229-prison-flicker-diagnostic-31450526c.zip`.
ZIP is15,894,046 bytes; CRC, embedded build/diagnostic strings, x86 header and
extracted DLL/checksum manifest verified. DLL-only, self-arming, no INI/installer.
Installed copy remains untouched. Existing draft PR126 targets staging.
Next: remote prison cinematic plus10seconds of gameplay at unchanged settings,
quit and collect support ZIP. One question: did prison eye flicker reproduce?
No reproduction is not a fix. Use first divergent frame to design a regression
before implementing a repair; do not install on the maintainer's machine.

## VR-228/229: pause FOV candidate and prison flicker diagnosis (2026-09-24)

Original VR-227 gameplay fix is locally reported good on the installed aa3af7216.
Both local and supplied remote log banners verify that build. Logs archived under
primary `build/support-20260924-231346`, local logs in its `local` subdirectory.

VR-228: pause in InDialog releases the108.1-degree scope and claims41.2. Candidate
allows the existing verified head-look menu permission, with UI-epoch live-owner
revalidation. VR-229: remote prison cinematic has stale-left submissions, repeated
late repairs and unresolved confirmations while FOV remains108.07. All40 detailed
windows were consumed before the scene; no proven flicker fix. Added rate-limited,
read-only expiration reasons under RingLedger, with pairing decisions unchanged.
Evidence and next steps: [FLICKER_REFERENCE](dishonored/FLICKER_REFERENCE.md).

Validation: cinematic30054, feedback1284707, ownership16, pairing416 pass.
Per user request, ZIP only for now; installed game and INI remain untouched.
Optimized x86 build `v1.0.1-4-g903891e7e`, legacy off; lint and9 exports pass.
DLL SHA256 `b768aa622c9860b4a99a49fa79bcc101499f10289f185ba5f5b030da56a3a1b6`.
Primary-checkout package: `build/test-packages/DishonoredVR-VR228-pause-FOV-VR229-diagnostics-903891e7e.zip`.
ZIP CRC/extracted hash, x86 header, embedded build and new diagnostic string verified.
DLL-only, no installer/INI. Separate local and remote single-question instructions.
No game launched by this task.
Next local question: does pause retain full-size world during low-FOV dialogue?
Separate remote question: does prison eye flicker reproduce for the new diagnostic?
Return its support ZIP; unchanged pairing means non-reproduction alone is not a fix.

## VR-227: affected-player pass and local install (2026-09-24)

The affected player reported that test build `v1.0.1-1-gaa3af7216` fixed the issue.
This is reported acceptance; no new support log was supplied for independent review.
At the user's request the exact ZIP DLL was installed locally, SHA256
`2f11878281c86d5b86feaaee730c9bd54d57f3b1ee48756d52795980891217c1`.
Previous DLL, INI, install record and available session logs archived in the primary
checkout at `build/playtest-candidates/vr-227/20260924-220256`.
Full installed INI byte comparison: zero changes, CRLF verified; LockFov=1 already.
No game launched. Next: same painting-dialogue/full-view question for local verification;
check the new log banner against the installed build before reading the result.

## VR-227: cinematic square-view candidate (2026-09-24)

Branch `codex/vr-227-cinematic-fov-test` starts at staging `f5176aeae`.
The shared checkout changed concurrently, so the candidate is isolated in
`build/worktrees/vr-227`; only the FOV patch was transferred. No occlusion change.

Supplied support archive: current log and install record match1.0.1,
`v1.0.0-8-gf5176aeae`,3012x3122. During dialogue the sensor reaches51.60;
the persistent writer retains it. At cinematic exit the3s draw bridge expires
and gameplay claims47.60. The two older logs are1.0.0, not1.0.1 retests.
See [ENGINE_NOTES](dishonored/ENGINE_NOTES.md#vr-227-cinematic-persistent-fov-recovery-2026-09-24).

Candidate: the existing Cine.LockFov option now also requests the full persistent
FOV during validated cinematic states and bounded locomotion recovery. Existing
live-object owner checks remain before writes. UI epochs, new ownership/load,
failed validation and disabled/ineligible states discard recovery. Ordinary
gameplay zoom cannot arm it. Diagnostic adds cinematicRecovery and master state.

Host checks:1284707 feedback/recovery,16 ownership,30045 cinematic/handback pass.
The negative control reproduces51.60 persistence and47.60 gameplay claim. Recovery
is bounded at3s; an extremely slow/unresponsive native camera can outlast it.
A synthetic1% blend per10ms did outlast the bound; this is not headset acceptance.
No game launched. Optimized isolated build, lint and9 exports pass.
Delivered test build `v1.0.1-1-gaa3af7216`, legacy off, from clean commit aa3af7216.
DLL SHA256 `2f11878281c86d5b86feaaee730c9bd54d57f3b1ee48756d52795980891217c1`.
ZIP: `build/test-packages/DishonoredVR-VR227-cinematic-square-test-aa3af7216.zip`
in the primary checkout (DLL, README, manifest and checksum only). ZIP CRC, extracted
DLL hash, x86 header, embedded build ID and new diagnostic string verified.
Local installation deferred because another collaborator has an active build;
no installed files/INI changed. User requested a remote test ZIP.

Next single launch question: at unchanged highest resolution, does the view stay
full through the painting dialogue and for10seconds after control returns?
Full coverage supports the candidate; a square means the fix is insufficient.
Return the support ZIP from that run either way; verify its test-build banner.
If this passes, test the Empress scene and ordinary spyglass zoom separately.

## VR-260 affected-player pass (2026-09-25)

The maintainer reports that the affected player confirmed the fix-only ZIP
60bbd0afc resolves the startup slowdown. Acceptance is reported, not independently
measured from a new support log. PR128 remains unmerged; no release or maintainer
installation. See PERFORMANCE.md for the source/cost evidence and limits.

## VR-260 shared capture compatibility candidate (2026-09-25)

Isolated branch codex/vr-260-shared-texture-capture starts at staging f5176aeae.
A separate performance support run falls from requested shared capture to sync
at startup. Capture averages about150 ms, mostly LockRect. The candidate replaces
the standalone shared surface with a texture-backed resource and aligns probe
formats with live slots. Native pixel/failure/reset tests, optimized x86 build,
lint and nine proxy exports pass; affected-PC
acceptance remains pending. Measurements, exclusions and the one-launch test are
in docs/dishonored/PERFORMANCE.md, VR-260. Do not install on the maintainer's game;
their newer build and Claude's checkout remain untouched. The VR-229 prison
flicker tester remains a separate pending investigation.

## 1.0.1 release verification (2026-09-24)

All hotfix changes are stacked on codex/vr-216-steamvr-mirror-default. Publication
is authorized; VR-Main is not merged. Release scope is VR-213 through VR-216,
not completion of the broader Stable milestone.

Optimized build5033e4764 ran through Steam on dvr-xrsim (Quest 3): title, Continue,
loaded Distillery District, gameplay, L3 + R3 panel open/close, pause/resume, and
clean console quit. Both eyes rendered nonblack projection frames. 167 FOV samples
over 166 seconds settled at 108.07 degrees from natural75; resolution stayed
2750x2850. This PC has NEVER reproduced the reported FOV contraction. This is
regression coverage, not reproduction or affected-player acceptance. Actual
headset comfort, SteamVR startup recovery, and GOG Galaxy launch need those rigs.
Incompatible x64 OBS/VD implicit layers were automatically excluded for this
32-bit process by the existing guard; no registry changes. Initial standalone
sim self-test needed its manifest-declared OBS opt-out, then passed60 frames.

Final host gates pass: FOV feedback1284665, ownership16, cinematic30045,
installer86, launcher57, log history27, real helper replacement/backups/refusal,
installer rollback/reset/keep settings, lint and9 exports. Full installed INI
is byte-identical with CRLF after install and simulator runtime restoration.
Local-only logs/captures: build/release-validation-1.0.1. No game-derived artifacts
are included in the release. Updater tests now take the CMake version so 1.0.2
needs no hardcoded test-version edits. Public asset verification follows upload.

Next: retain v1.0.1 as the immutable baseline for 1.0.2. Bump CMake and matching
versioned launcher/tag, build/package, upload both assets before publishing.
The updater requires GitHub SHA256 metadata and validates the embedded version.

## SteamVR mirror default prepared (VR-216, 2026-09-24)

Branch codex/vr-216-steamvr-mirror-default is based on VR-215. SteamVR no longer
silently forces the desktop mirror on. The launcher checkbox is enabled for all
runtimes; existing off defaults and explicit saved choices are honored. Launcher
and in-game tooltip explain the potential large performance benefit across
runtimes. Native D3D9 testing passes 240 GPU markers/pixel checks and fallback/
reset checks. Installer smoke and optimized build pass; SteamVR settings previews
rendered at 100/150%, normal-scale layout inspected.

The user confirmed the SteamVR startup recovery warning must suggest mirror ON.
Launcher and overlay now include that guidance. Game-run verification and GitHub
1.0.1 publication are authorized; VR-Main merge remains outside this request.
Release verification is in progress.

## Reliable support collection and ten-run history (VR-215, 2026-09-24)

Stacked on VR-214, branch codex/vr-215-support-log-history, still 1.0.1.
Released 1.0.0 native collection reproduces Windows error 3 on a fresh TEMP
profile: CreateDirectory was called on a nested path with no parent. The helper
now creates parents and retries under LocalAppData when TEMP is unusable. It
reports the actual failing path and flushes the launcher log before collecting.

The game and launcher keep ten sessions: current, .prev, and .prev2 through .prev9.
A failed archive move does not truncate the current log. Support ZIPs have a hard
24,000,000-byte ceiling (headroom below the requested 25 MB), compressed byte
accounting, bounded context reservation and newest-first game history. If the
current log alone is too large it retains its header and tail with an explicit
omission marker. Manifest includes source size/time, collected hash, truncation,
omitted files and read failures. Missing INI/data drive and unreadable optional
files do not abort the rest. Large files stream; no whole-log RAM buffer or raw
staging copy. Optional dumps remain opt-in and obey the same cap.

Validation: production logger 27 checks (14-run retention, one-deep migration,
locked archive), Windows PowerShell 5.1 budget suite 17 checks, existing collector
suite, and the real launcher helper. The old EXE fails, the new EXE succeeds on
the same fresh profile, all ten fixture sessions survive in order, and unusable
TEMP falls back. A 32 MB incompressible log produces a 23.74 MB ZIP retaining both
ends. Game launch/headset test not performed. Supplied attachment was a launcher
log, not FOV telemetry; user explicitly deferred further FOV-log verification.

Installed optimized candidate: 1.0.1, build v1.0.0-4-gff1b03f08 (legacy off).
DLL SHA256 a5f498a58029da8da338f2847ed3ce9d68eea52b1d0ba6aaff416e0c720f9846.
Launcher SHA256 61f0b06f8cb8e44ee5c247de5d8d06134e70335b84b1e52a904c1c4d6b028415.
Entire installed INI byte-identical/CRLF, zero-setting diff, SHA256
ff6fc98a6a15ef0aca8e4eb1bdd5377e5c845d440852bfe1d28a374f8a77473b.
Previous DLLs, INI, record and both logs archived in this worktree under
build/playtest-candidates/installs/20260923-214047-vr215. The existing log banner
is stale 1.0.0 build761, not the previously installed 1.0.1 build97b5a3f77; it
was archived without treating it as a current playtest. Actual installed launcher
collection succeeded: 5,136,385-byte ZIP. Native Unicode-path check, 86 installer
host checks, optimized build, lint and all nine exports pass. The running previous
candidate in output/hotfix-1.0.1 was left open; new distributable is under
output/hotfix-1.0.1/log-history and the stable shortcut target is updated.

Next: tester can use Collect logs after reproducing an issue; history begins
accumulating with this build and cannot recover sessions already overwritten by
1.0.0. No merge or public release authorized.

## Launcher self-update and GOG candidate (VR-214, 2026-09-24)

Branch codex/vr-214-launcher-updates is stacked on the VR-213 FOV hotfix,
version 1.0.1. The native launcher checks the official GitHub stable releases on
startup, shows notes and an Update button, downloads the exact versioned EXE,
verifies size/SHA-256/VERSIONINFO, and hands replacement to that new EXE after
the old process exits. The updated launcher applies its bundled mod, preserving
a full previous-file backup and rolling back partial failures. No game is launched.
About has manual checks and cached recent changelogs. Overwrite INI defaults on,
with a warning when off. Steam/GOG discovery reads both registry views, library
manifests and GOG metadata, with common-path fallbacks and a home folder picker.
Win64 folders and 64-bit executables are rejected; GOG launches through Galaxy.

Validation: 80 installer host checks; 57 offline updater/discovery checks; prior
live GitHub check/download verified public 1.0.0 (59 checks before one added
Win64-empty-folder check). Real helper entry point waits for parent exit, replaces
its target, preserves backup, and refuses wrong digests/locked targets. Installer
smoke passes, including default whole-INI reset/CRLF, keep-settings, and rollback
of every original file after locking the second DLL. Fifty native screen previews
were rendered at 1.0/1.5 scale; update/GOG/Win64/About/warning layouts inspected.

The actual Steam game's previous VR-213 candidate and INI remain the playtest
baseline. Launcher-only installation does not apply a mod update to the game.
No headset run or actual GOG launch has been performed. An end-to-end update from
a published newer release remains a release-time acceptance check because 1.0.1
is not public yet. See VERIFICATION and LINEAR_AND_GITHUB for the update contract.
Nothing has been merged to VR-Main or published.

## 1.0.1 FOV feedback hotfix candidate (VR-213, 2026-09-23)

Based on released v1.0.0 (02d5cf5d4), isolated on
codex/vr-213-fov-feedback-1.0.1. The persistent writer multiplied rendered
readback by target/natural. With natural 110 and target 108.0666, that ratio
is below one, so delayed or interpolated self-feedback collapses toward 20.
The earlier scoped-FOV guard only protects active draw scopes.

The candidate caps the scaling baseline at the target. Normal readback reaches
the requested FOV without repeated contraction; narrower native zoom remains
available. Expansion behavior is unchanged. Camera/controller ownership is
revalidated with IsLiveObject, identity slots and a fresh table after load/menu
changes. No resolution/configuration defaults changed. Version is 1.0.1.

Host results: the old recurrence reaches 20; 32 target/interpolation combinations
converge with the fix. Delayed dispatch, target changes, zoom, invalid inputs and
draw scopes pass. The production ownership guard passes 16 lifecycle checks;
existing cinematic FOV suite passes 30045 checks. Optimized 32-bit build, lint
and all9 exports pass. Installed 1.0.1 candidate build v1.0.0-1-g136be1eaf,
RelWithDebInfo, legacy off. DLL SHA256:
43349836dad56533cac37e7cbe8d06a77d018a3924b4a440806554d09b637218.

Full installed INI is byte-identical (empty full diff, zero setting changes),
CRLF verified: RenderWidth=2750, RenderHeight=2850, ProjectionFov=103.00.
Previous DLL, complete INI and both logs are archived under this worktree's
build/playtest-candidates/installs/20260923-200056-718548. Previous log banner
matched the previous installed DLL (1.0.0, build761-g14728179e). No new log exists
for this candidate yet; verify the next launch banner before interpreting it.
Bounded fovlever feedback/owner diagnostics are built in at Info level, with no
INI edit needed. DLL-only candidate ZIP and manifest are in
build/playtest-candidates/vr-213-1.0.1; the ZIP deliberately contains no INI.

Next launch question: at the unchanged resolution, does gameplay keep its full
view for 30 seconds after loading a save? Stable coverage supports the feedback
fix; continued contraction means readback or another FOV writer still feeds back.
Read the matching installed-build log and archive both logs before another launch.
Native spyglass/cinematic headset regression checks remain separate follow-ups.
No game launched, main merge or public release performed.

Details: [ENGINE_NOTES](dishonored/ENGINE_NOTES.md#vr-213-persistent-fov-feedback-101-hotfix-2026-09-23).

## 2026-09-23: post-merge performance candidate (VR-212)

Branch codex/vr-212-drop-discovery is based on the unmerged launcher continuation.
Last installed/run build756 has a sustained slow intro (53-76 ticks/s), substantial
game-thread waiting, and repeated missing drop-context discovery. PR106 removed
the old20ms discovery throttle. The candidate restores that cadence for discovery
only, backs off1s after a failed full sweep and reuses class/readability checks
within each script-lane slice. Attack decisions retain their existing tick cadence.

Native discovery schedule tests cover absent contexts, load/table growth, owner
changes and retry timing. Optimized build761-g14728179e,9 exports,75 swing checks,
default-profile/reset checks and lint pass. Installed DLL SHA256
95622F87CD80C2E412A192E7D92D6FF5F089C27A9D7B21C2906233AE1691CC20.
All64705 installed INI bytes are unchanged, CRLF verified; original DLL, INI and
both logs are in build/vr212-before-install. Launcher embeds this candidate,
SHA256539A24E5995A27E09119DD9F04E76691EFBCB8101011AFC065116A71FF6D14C1,
and the stable desktop-shortcut copy is updated and opened. Game not launched.
One-question headset check: same intro/hub spot, recovery toward prior FPS range.
Full measured
record and remaining hub-cadence uncertainty: docs/dishonored/PERFORMANCE.md.
Headset performance acceptance remains open. No merge authorized for this branch.

## 2026-09-23: launcher 1.0.0 polish and F10 Layout (VR-198, PR 111)

Current state: codex/vr-198-launcher remains unmerged. Public version is now
1.0.0 (older 41.x labels were development versions). The executable is
DishonoredVR-Launcher-v1.0.0.exe. About includes releases,
owner-approved credits and Ko-fi copy; the supplied emblem is the EXE icon.
Fresh setup defaults to Auto/Balanced, removes head-based walking from the
launcher and recommends 120 Hz or 144 Hz with Virtual Desktop Beta.

Update/Reinstall can optionally replace INI/F10 settings with shipped defaults.
The checkbox defaults off, persists per user and backs up the entire INI before
an enabled reset. Ordinary updates preserve the current-schema INI byte-for-byte.
F10 now has Layout in all tiers, with the controller picture, Full view, Fit, 1-8x zoom,
four-direction pan buttons, drag and scrollbars.

Collect logs previously failed in the launcher's child PowerShell because
Get-FileHash was not found. Full system PowerShell selection, local module-path
repair and module-independent SHA256 fix this. GUI and regression mode share
the same collector. Actual launcher collection: 19 files, four binaries, zero
manifest errors. Scratch tests verify both preservation and reset, exact backup
bytes and CRLF. Native F10 pointer tests verify zoom/pan/Fit; launcher fixtures
are reviewed at 100/150% DPI. See docs/INSTALLER.md for details.

Next: user review of the refreshed launcher and eventual headset validation of
the Layout tab. No game installation or launch is authorized for this candidate;
another session updated the installed game during this work from accepted F10
build736 to integrated build756-g1726cee95 (the current DLL and log banner agree).
This launcher candidate was not installed into the game; the external game/INI
updates were left alone. PR 111 is not merged and no release/tag has been published. An online updater remains a separate feature.

Final launcher: build759-g7f958ac8c, DishonoredVR-Launcher-v1.0.0.exe. Its default
Desktop support destination was also verified: ZIP created, 19 files, four binary
hashes, no manifest errors. Main README remains untouched.

## Session 2026-09-22 (drop takedowns, VR-203): branch `takedown-tweak`, PR #106, not merged

A drop onto a guard often became an ordinary slash. The cause is timing. The game
re-decides the drop target every airborne tick. An attack pressed while that decision
still reads "no target" goes to the ordinary attack, and the VR-111 log shows the target
arriving about 30 ms after such a press (ENGINE_NOTES "Drop takedown timing").
`[DropTakedown] Assist=1` holds an airborne sword attack (trigger or swing) until the game
finds a target. It then presses it, and the game's own kill runs. On landing or after
`HoldMs` the held attack is delivered normally (`Fallback=1`).
`ReachScale` lengthens the game's look-ahead (shipped `m_fHitWindowInSeconds` 4.0 s).
The controls are in F10 > Controls > Drop takedowns, and the live word is `drop`.

**Headset run, 2026-09-22:**
* At the shipped reach (1.00), 2 held swings found no target, and both landed as
  ordinary attacks.
* At 1.50 to 2.00, 18 swings were held on a real drop (vz below -1000). 13 became drop kills.
  The other 5 found no target before landing or timing out; whether a guard was below them is
  not in the log. One more swing met a target directly and was passed untouched.
* The defaults are the tester's values: `ReachScale=2.00`, `HoldMs=420`.

Next: more drops at the new defaults, and whether 2.00 ever takes a guard the player
did not aim for.


## 2026-09-23: launcher continuation, Bindings, logs and shortcuts

PRs 105-110 are merged into VR-Main at 1726cee95, with all source branches kept.
Follow-up work is on codex/vr-198-launcher in the integration worktree under
build/integrate-105-110. VR-198 remains in progress.

DishonoredVR-Launcher replaces the setup branding and shares the accepted F10 art.
Runtime/quality, desktop mirror, physical crouching, close rain overlay and
head-based movement are visible during setup; controller shortcuts are expandable.
SteamVR's forced mirror is explained without destroying the native preference.
The installed-player screen launches through Steam and offers Collect logs,
Desktop shortcut and Start menu shortcut. Shortcuts use a stable per-user copy.
The owner-supplied image is embedded in a separate Bindings page with fit/zoom,
scrolling, maximization and current shortcut values.

[BRVR research](BRVR_LAUNCHER_RESEARCH.md) records the installer/logging review and
semantic comparison of all four installed binding JSONs against the donor source.
Missing haptic/touch outputs, Index force binding and provisional WMR quadrants
are documented for a separate validated port; no engine input behavior changed.

Validation: release build, lint, installer host checks including .lnk roundtrip,
scratch smoke including complete ini comparisons for all seven preferences,
invalid input refusal, update preservation, CRLF and backup restore. Support ZIP
collection completed with zero errors. All 18 UI fixtures rendered at 100/150% DPI;
visual review caught and fixed truncated choice labels. Body scrolling preserves
footer actions. The final launcher is prepared for desktop review, not installed
into the game.

The real installed DLL is still accepted F10 build736: SHA256 begins 172AAFCD3416,
and the latest game log banner matches. Both game logs and the full installed ini
were archived under build/accepted-f10-736. The ini hash begins 0A09F442AC49;
no game launch or real installation occurred during this work.

## 2026-09-23: integrate PRs 105-110 and accept the F10 layout

The tester accepted build 736's F10 alignment, spacing and Debug-tab navigation.
Integrated PRs 106, 107, 110, 108, 109 and 105, preserving both documentation
histories. The movement/drop sampling hooks keep both implementations; the old
inline drop watch is replaced by drop_assist. Consolidated the duplicated
DropTakedown defaults without changing the tuned values.

Combined release build, lint, nine proxy exports, production/golden/shipped
default equality, installer host and scratch install/update/uninstall checks
passed. HUD anchor (908), mono UI (31) and native desktop-present (240) checks
passed. No real game launch or installation performed. Index hardware testing
remains outstanding; the known desktop-eye host failure is tracked separately
in VR-209. Installer work continues from PR 105 as a launcher under VR-198.

## Session 2026-09-22 (walk speed by direction, VR-204): branch `claude/vr-204-crouch-walk-slowdown`, not merged

A crouched walk slowed when the left stick pushed off straight ahead. The `move/trace`
diagnostic found two causes, both in the game:
* A full diagonal push was flagged as a walk, at half speed. The game reads each stick axis
  through its own 0.3 deadzone, on top of ours.
* Sideways ran at the game's strafe multiplier, 0.727 of straight ahead.

The stick is now delivered pre-compensated for the game's deadzone, which is headset-verified:
full diagonals no longer walk. Standing, the strafe and backward multipliers are raised to
1.0. Crouched, the strafe multiplier scales the RUN speed, so setting it to 1.0 made a
crouched strafe run at standing speed. Crouch speed divided by run speed came out 11% fast
in the third run, so a closed loop was tried. It never fired in the fourth run, and the ratio alone was
judged good, so the loop was removed. The third run also caught a performance
regression (about 118 to 89 fps) from a continuous object scan. A "once per level" scan
retried forever and was worse. There is no scan now: the tweak objects are read through the
pawn's own pointer (`m_pPawnTweaks.m_pAttributeTweaks[4]`). Neither has a toggle, by request. Built and
installed; the strafe ratio is not yet headset-tested. In the next log, look for:
* `pad/move:`, which gives the game deadzone read from `DishonoredInput.ini`;
* `move/speed:`, which gives the shipped multipliers and each `correction`;
* `move/trace:` lines (off by default; `[Anim] MoveTrace=1` turns them on), where `mod=` should be the same at every `stickAng`.
Details: ENGINE_NOTES "Walking speed by stick direction".
## Session 2026-09-23 (Index report, VR-207/VR-208): branch `claude/vr-207-index-menu-and-mirror`, not merged

A report from an Index on the SteamVR shim (build `525-g548c31693`) raised two faults:
* **Pause menu stuck at the first direction (VR-207).** The world-anchored menu window was
  parked once per session: 14 opens, 1 park. It now re-parks at the head's yaw whenever it
  reappears after more than 250 ms away. See HUD_ANCHORS.md, top section.
* **Crash with the shipped mirror-off profile (VR-208).** The SteamVR shim now keeps the
  desktop mirror presenting whatever `DesktopMirrorOff` says; other runtimes are unchanged.
  The fatal crash in the report is the game's own trap 6 (`+0x60907e`), and its causal link
  to mirror-off is unproven: the logs of the crashing runs were overwritten. See
  DESKTOP_MIRROR.md section 9.

Built; not installed, not headset-tested; this rig has no Index. The next shim log should
show `desktoppresent: the desktop mirror stays ON on the SteamVR shim` and one
`HUD window parked` or `re-parks` line per menu opening.
## F10 polish build736 installed (VR-206, 2026-09-23)

Explicit installation approval received. Installed vr33-hands-working-736-gab7023884
from the archived candidate; DLL hash and embedded build identity verified. Previous
DLL, complete ini and both logs saved under
build/playtest-candidates/installs/20260923-002234-384927.
Full ini comparison is byte-identical, with zero setting changes and CRLF verified.
Game not launched. No merge.

Next launch question: in Debug, can the always-visible tab-list arrow reliably select
Runtime and Log? Success supports the navigation fix at the accepted narrow size;
a disappearing arrow or unreachable tab means the fix needs further work.
Future candidates still require separate explicit installation approval.

## F10 accepted profile and navigation polish (VR-206, 2026-09-23)

Build733 log banner and installed DLL hash match. Headset report accepts the visual
direction with minor alignment, collapsed-spacing and Debug navigation follow-ups.
Archived the complete ini and both logs under
build/playtest-candidates/runs/vr-206-accepted-20260923-001000.
All current values are saved in tests/golden/f10-tuned-2026-09-23.ini. Shipped gameplay
and display defaults mirror that profile, including UiScale=1.00; portable runtime
discovery stays auto/blank. Installed runtime selection and ini are untouched.

Default menu placement follows the final logged 1027,1021 / 649x685 rectangle at a
2750x2850 eye texture, stored as resolution-relative layout constants. Tier buttons now
fit their labels and padding; the close button gets suitable horizontal padding.
Collapsed sections lose the extra spacer rows. Tab art uses native scrolling-strip
clipping, and selected-label tint no longer darkens bar arrows or popup entries.
The always-visible tab-list arrow reaches every tier-eligible tab.

Prepared vr33-hands-working-736-gab7023884; optimized build, lint and 9/9 exports pass.
Default writer/reset tests and byte-identical package/golden checks pass. Native preview
at 649x685 and 1.00 text passes layout/widget checks; a pointer test opens the tab list
and selects the offscreen Log tab. Build and prepare only, no install without a new
explicit go-ahead. PR #109 remains unmerged above rain-fixes.

## F10 revision installed with approval (VR-206, 2026-09-22)

Installed vr33-hands-working-733-g7d78ac20f from the archived candidate after explicit
approval. DLL SHA-256 and embedded build identity verified. Previous DLL, ini and both
logs archived under build/playtest-candidates/installs/20260922-235702-071438.
The existing ini had mixed line endings; normalized to CRLF. Complete file comparison
confirms only line-ending changes and zero setting changes. Game not launched.

Next launch question: does F10 remain readable and free of unintended control overlaps
while switching Basic/Advanced/Debug and scrolling? A clean layout supports headset
acceptance; overlap or clipping outside the scrolling body requires a layout follow-up.
No merge performed. Future builds still require explicit installation approval.

## F10 reference refinement and layout QA (VR-206, 2026-09-22)

Reworked the first image-backed pass: restrained skyline, thin title, torn parchment
headers, folded italic notes, slate gradients and brass radio/check outlines. Recenter,
Save and Reset occupy one row. Square defaults retain the reference proportions; automatic
text scale follows panel size, with minimum width for button labels and a wrapped footer.
More adjustments expands depth/yaw/roll and the advanced numpad preference.

Preview now carries the full relevant Hands inventory and shares the production scrolling
body/footer layout. Pointer-event checks and computed layout bounds pass. Visual QA covers
1254-square Basic/Advanced, 900-square at 1.54 text, 1254-square at 2.0 text, actual tooltips,
and scrolling to the bottom. Headset acceptance remains pending. See F10_ART_THEME.md.

Prepared vr33-hands-working-733-g7d78ac20f under build/playtest-candidates. Optimized
Win32 build, clean lint and 9/9 exports pass.

Build only. DO NOT INSTALL without explicit go-ahead. Branch f10-improvements, draft PR
#109 based on finalized unmerged rain-fixes / PR #108. No installed files changed.

## F10 image-backed overhaul prepared (VR-206, 2026-09-22)

New f10-improvements branches from finalized rain-fixes at 128632768. Rain PR is #108,
unmerged; previous #104 closed during the GitHub branch rename. Awning TODO is VR-205.

Generated three reusable art masters and embedded them in the DLL: painted skyline
background, parchment and worn metal. Shared native ImGui wrappers apply materials to
controls, sections, tabs and notes. Default panel is centered and square (46% of shorter
eye dimension), resizable, with independent scrolling tab contents. Tier logic, live
controller hint, reset/save actions and existing setters remain intact. Hands size and
Sleeve initially open. See dishonored/F10_ART_THEME.md and assets/ui/f10/README.md.

Native offscreen Basic/Advanced previews and pointer-event checks pass. Optimized game build,
lint and 9/9 exports pass. Prepared vr33-hands-working-731-g9b2e4a44a, archived under
build/playtest-candidates/vr-206-vr33-hands-working-731-g9b2e4a44a.
DO NOT INSTALL until explicit go-ahead. Headset layout/controller acceptance remains open.
No installed file was changed for this task.

## Rain branch finalized, not merged (2026-09-22)

Rain recovery is headset accepted on build725; the close-overlay hide/restore is accepted.
Final branch is rain-fixes. No merge authorized or performed. Optional outdoor rain from
dry shelter is deferred as TODO VR-205. The next F10 art/layout overhaul is VR-206 and
branches from this finalized tip as f10-improvements. Do not install that work without
explicit go-ahead. Remaining broader F10 reset/save-persistence checks are recorded below.

## Headset recovery acceptance and under-cover scope (2026-09-22)

Build725-gbaecc7491 banner and installed DLL SHA-256 match. Headset report provisionally
accepts the recovery fix. Log shows correction to 10000 while uncovered, and exposed
particle requests remain enabled. Archive: build/playtest-candidates/runs/vr-202-fixed-20260922-224551.
A separate request concerns visible outdoor rain disappearing under awnings. During the
covered interval around ticks 40609750..40613750, uncovered=0 and MaxParticles=0; exiting
restores MaxParticles=40. This is native camera-wide suppression, not the recovery fix.

The traced RainDrops update implements volume fading/recycling, not per-drop roof tests.
Engine ParticleModuleCollision is declared, but its existence does not establish that the
rain asset has a compatible collision module or that it can be enabled with one boolean.
WorldRainComponent declares enable/intensity/wrap controls; no roof-mask control appears
in its script declaration. Do not promise dry sheltered areas from simply bypassing the
camera shelter decision. Selected scope: an optional toggle that preserves dry shelter while showing outside rain,
provided the work remains modest. A blanket shelter bypass does not satisfy this. The
traced camera-wide path cannot distinguish drops outside an awning from drops beneath it;
proper filtering requires additional per-drop/region roof handling. Deferred as a separate
feature rather than adding a leaky toggle to the accepted recovery fix. No installation or
awning behavior change has been performed.

## Install 2026-09-22 (VR-202): recovery fix approved and installed

Explicit user go-ahead received. Installed `vr33-hands-working-725-gbaecc7491`, with
DLL/ini SHA-256 verified. Full ini comparison shows only Rain Recovery added as 1;
Trace was already 1. CRLF preserved. Prior DLL, ini and both logs archived under
`build/playtest-candidates/installs/20260922-224035-841574`. Game not launched.
Next launch, one question: at the same exposed location, does falling rain now remain
visible through repeated eye-level/upward head tilts, including holding near the previous
transition angle? Remaining visible supports the recovery fix; continued disappearance
means the correction is insufficient. Headset acceptance remains pending.

## Session 2026-09-22 (VR-202): behavioral rain recovery fix candidate

Found native recovery recurrence rate *= 100*dt below its terminal threshold 10000.
It grows at 60/90 Hz but decays toward zero above 100 updates/second, matching zero recovery
rate and fully transparent rain layers in the measured run. Candidate promotes the current
uncovered camera to the native terminal rate, preserving native shelter and particle logic.
Opt-in Rain Recovery=1, live rainrecovery on|off, default off. See ENGINE_NOTES top entry.

Prepared `vr33-hands-working-725-gbaecc7491`: optimized build, lint and 9/9 exports pass.
Archived DLL and planned Recovery=1 override locally. Not installed.

Build and prepare only. DO NOT INSTALL until another explicit user go-ahead. On approval,
preserve current ini and apply Rain Recovery=1/Trace=1, with whole-file diff and CRLF checks.
Next test is behavioral: repeated stationary head tilts should retain exposed falling rain.
Headset acceptance remains pending; this is a fix candidate, not a confirmed resolution.

## Session 2026-09-22 (VR-202): repeated pitch transitions zero rain opacity

Verified build720 banner and installed DLL hash. Both 40-particle layers become fully
transparent during repeated upward views and recover on lowering the view. Near the final
transition angle they fluctuate together; the third layer stays nonzero. Positive particle
requests persist. This establishes CPU opacity loss, not a generic whole-component cull.
Archive: build/playtest-candidates/runs/vr-202-alpha-20260922-223037.

Prepared `vr33-hands-working-723-g41bd201e5`: optimized build, lint and 9/9 exports pass.
DLL archived locally; no installation performed.

Next candidate adds signed fade-state counts, particle height and actual current rain-module
extent to test the native volume-exit/fading path. See ENGINE_NOTES top entry. Diagnostic
only; prepare/build but DO NOT INSTALL until another explicit user go-ahead.

## Install 2026-09-22 (VR-202): opacity diagnostic approved

User explicitly approved installation. Installed `vr33-hands-working-720-g632dca2fb`;
DLL and ini SHA-256 verified by the installer. Entire current ini preserved byte-for-byte,
CRLF verified, Rain Trace=1/Hide=0/Distance=-1. Both previous logs and prior DLL/ini archived
under `build/playtest-candidates/installs/20260922-221820-803121`. Game not launched.
Next launch, one question: does rain still cycle off/on while position and sky view stay
steady? Hold that view for about 30 seconds after seeing the cycle, then quit. Per-instance
alpha falling during absence supports fading; stable alpha directs work toward materials
or rendering. Lack of reproduction leaves that distinction unresolved.

## Session 2026-09-22 (VR-202): stationary cycling, prepare only

Recovered build718 from the rotated previous log. Final steady sky view retains positive
rain requests, 88..90 live particles across three instances, and advancing render time.
Pitch-only reproduction did not repeat. Native fading can retain zero-alpha particles;
the next read-only candidate separates per-instance counts and opacity. See ENGINE_NOTES.
Archive: build/playtest-candidates/runs/vr-202-cycle-20260922-220215.

Prepared `vr33-hands-working-720-g632dca2fb`, optimized/legacy off. Build, lint and 9/9 exports pass.
Archived DLL under `build/playtest-candidates/vr-202-vr33-hands-working-720-g632dca2fb`.
No installation performed. Read the current ini at any later authorized install to preserve
concurrent changes.

Do NOT install until explicit user go-ahead. Another agent is swapping builds concurrently.
Prepare/validate only; eventual test is steady-view cycling, with layer alpha distinguishing
simulation fading from material/render behavior.

## Session 2026-09-22 (VR-202): repeatable pitch-dependent sky-rain absence

The final stationary look-up/eye-height reproduction supersedes the positional hypothesis.
Build 716-g52b55216b and installed hash match. Last 42 seconds: 162/168 samples request rain,
including sustained upward views; only six brief native shelter suppressions. All 42 summaries
show active, unhidden particles with spawning enabled. Shelter is not a sufficient explanation.
Archived both logs and ini: `build/playtest-candidates/runs/vr-202-pitch-20260922-213428`.

Native rain Spawn, Update and SpawnCount establish the actual emitter-instance count layout.
Added read-only rain/particles logs for live count, bounds, render time, forced inactivity,
world emitter placement and camera pitch, once per second under existing Rain Trace=1.
See ENGINE_NOTES top entry for derivation, counterpredictions and interpretation limits.
Installed `vr33-hands-working-718-g433e81335` (optimized, legacy off). Build, lint and
9/9 exports pass; installed DLL SHA-256 matches. Full ini unchanged and CRLF preserved.
Both previous logs archived under `build/playtest-candidates/installs/20260922-214244-326840`.
No rain behavior changes. Next test: stationary eye-height/up/eye-height, ten seconds each,
repeat once and quit. One question is whether the same pitch-dependent disappearance returns;
the new log distinguishes simulation counts from bounds/render-update behavior.

## Session 2026-09-22 (VR-202): positional sky-rain investigation

Lens-only rain hide and restore are accepted in the headset on c17016e63; matching banner
and DLL hash verified. The log confirms lens hide and restore with the rain box unhidden.
The remaining sky-rain absence is reported as possibly positional, with ground splashes
continuing. Archived run: `build/playtest-candidates/runs/vr-202-20260922-212046`.

Decompiled rain classes plus native disassembly reveal a camera shelter test jittered
+/-35 uu horizontally and traced 5000 uu upward for default rain direction. Its result
sets the particle MaxParticles to the configured count or zero; impacts have an independent
path. Old drops=40 logging measured the configuration, not that effective parameter.
See ENGINE_NOTES VR-202 for the measured code path, corrected RVA provenance and limits.

A read-only diagnostic now logs shelter decisions, actual particle parameters, impact
counts and camera position once per second under Rain Trace=1. It changes no weather
behavior. Installed `vr33-hands-working-716-g52b55216b` (RelWithDebInfo, legacy off);
DLL hash, 9/9 exports and lint verified. Whole ini unchanged with CRLF preserved,
Rain Trace=1 and Hide=0. Install archive: `build/playtest-candidates/installs/20260922-212739-025799`.

Next launch, one question: does falling rain consistently disappear and return
between the same nearby positions while splashes continue? Hold the same view direction,
stand 10 seconds in the raining spot, 10 in the non-raining spot, then 10 back at the first.
The log will distinguish native suppression from active particles failing to draw.

## Playtest follow-up 2026-09-22 (VR-199): narrow rain hide to the close overlay

The headset report accepts the F10 changes provisionally and confirms rain disappears, but
reports that sky rain and ground splashes disappeared too. Build banner and installed hash
match `vr33-hands-working-712-gc1a25b64d`. Its log proves both the camera box and the
`Over_camera_rain_01` looping lens component were hidden. Only Hide=0 -> 1 was exercised;
restoration and reset persistence are not established. Logs and ini archived under
`build/playtest-candidates/runs/vr-199-20260922-202644`.

Required behavior: hide ONLY the close overlay; keep sky rain and splashes. The follow-up
removes the camera box from hide targets and labels the Basic control Hide close rain
overlay. The lens target is now measured by name, but whether this particle asset alone
preserves all expected sky/ground rain needs a new headset A/B. The preexisting intermittent
missing-sky-rain report is tracked separately as VR-202; ground splashes can remain during that issue.

Installed follow-up: `vr33-hands-working-714-gc17016e63`, optimized, legacy off.
DLL hash and 9/9 exports verified; full ini diff is only Rain Hide=1 -> 0. CRLF preserved.
Install archive: `build/playtest-candidates/installs/20260922-202922-127478`.

Next launch, one question: with sky rain and splashes visible, does enabling Hide close rain
overlay remove only the close layer while both remain, and does disabling restore that
layer? If sky rain or splashes disappear too, the lens asset needs finer separation.

## Session 2026-09-22 (VR-199): F10 improvements candidate

Branch `f10-improvements` from VR-Main `375dda772`. Removes reticle hand selection and
beam from F10, and normalizes old configurations to left-hand/no-beam. The themed header
now explains click versus hold on L3 + R3. Reset to Defaults sits beside Save as Defaults;
it queues a reset for the next launch, backs up the ini as `.pre-reset`, and preserves
runtime selection, runtime JSON and DataDir. Settings are disabled while reset is queued.

Basic > Comfort > Rain now contains Hide rain effects. The existing camera-box hide could
not remove the separate lens sheet documented in run470. The candidate also hides live
camera-owned looping lens particle components whose template identifies rain, using native
SetHidden. It refreshes liveness after menu/load epochs, revalidates current ownership and
component identity before restore, and logs unidentified looping templates. See
`dishonored/ENGINE_NOTES.md` VR-199 entry. This remains a candidate until a rainy-area A/B.

Validation: optimized build, lint, golden/default-profile parity, reset host checks
(success, preserved paths, backup failure, staging failure), and offscreen theme preview.
The game is never launched by the agent. Installation records and complete ini/log backups
live under `build/playtest-candidates/`. Installed `vr33-hands-working-712-gc1a25b64d`
(RelWithDebInfo, legacy off), with source/installed SHA-256 matched and 9/9 exports.
The entire installed ini is byte-identical to its predecessor, including CRLF; Rain Trace=1
was already armed. Prior ini and both logs: `build/playtest-candidates/installs/20260922-201638-383971`.

Next launch has ONE question: in a rainy area, does Basic > Comfort > Rain > Hide rain
effects remove the close rain layer and restore it when turned off? Disappearance and
return support the fix; unchanged rain means the log's target/template evidence determines
the next step. Leave Reset to Defaults for its own subsequent persistence test.

Still pending from #102/#103: headset inspection of all F10 tiers, save-on-change across
relaunch, options-menu sensitivity 30, and a matching-build log check for interact/flicker.
## Session handoff 2026-09-23: the installer (VR-198), PR open, not merged

Branch `claude/vr-198-installer` off `VR-Main` `375dda772`. `DishonoredVR-Setup.exe`: one
32-bit exe with the mod embedded, drawn with Dear ImGui in the F10 theme (`dvr::ovl`), that
finds the game through Steam, installs the three DLLs and the tested ini, applies the four
game-ini values, and asks the headset runtime and the render size. Re-run it offers Update,
Change settings, Disable/Enable VR, Collect support bundle, Uninstall. `docs/INSTALLER.md` is
the reference; `src/tools/installer/`; `tools/package.ps1` ships it beside the zip.
* Verified without a click: `tools\installer-render.ps1` (14 screen states at 1.0 and 1.5),
  `tools\installer-host.ps1` (unit tests, all pass), `tools\installer-smoke.ps1` (an install
  into a scratch folder: the ini differs from the shipped copy in exactly the five keys, the
  four game-ini lines and nothing else, idempotent, uninstall restores the backup; all pass).
* On the dev PC: the window detects the real game, VDXR, SteamVR and the RTX 4060 (7 GB budget,
  so Performance is preselected); `--apply --op update` then `change` wrote the real ini, and
  one Steam launch read them back. That launch found the ini at version 13: the mod's own
  refresh replaced the file and kept only the runtime, dropping the size and writing
  `DataDir=D:\dvr-data`. The installer now does that refresh itself (old file kept beside it,
  runtime and size carried across), which is the one behaviour a player upgrading from a
  tester zip would have hit. The dev PC's ini is back at 2064x2208 and `D:\dvr-data`.
* Deliberately not here (tickets to file): the compiled `D:\dvr-data` fallback for a missing
  `[Paths] DataDir` in `config.cpp`; `package.ps1` does not refuse a `-dirty` tree although
  LINEAR_AND_GITHUB says it does; a left-handed option; an online update check.

## Session 2026-09-22 (F10 cleanup and theme, VR-196/VR-197): merged to VR-Main (#103)

The F10 panel is rebuilt with a Basic / Advanced / Debug selector, regrouped tabs, collapsed
sections, a tooltip on every control, and save-on-change for every Basic and Advanced setting.
`docs/dishonored/F10_AUDIT.md` has the per-control disposition, the tester's 15 answers, and what
the audit found (two settings no save wrote, many only SAVE AS DEFAULTS kept, two ImGui ID
collisions). Built and installed. **Not yet seen in the game**: next, a launch to check each
tier and that a change survives a relaunch.

VR-197 themes the panel after Dishonored (ink, bone, brass, oxblood, parchment tooltips;
Constantia headings over Segoe UI) and keeps tooltips only where a control is not
self-explanatory. `tools\ovl-theme-preview.ps1` renders the theme offscreen.

## Session handoff 2026-09-22 (misc fixes pt 2): merged to VR-Main

Branch `claude/misc-fixes-pt-2` (renamed from `claude/vr-102-startup-lookups`), merged with the
tester's permission. It carries VR-165 (PR #101) and VR-190 (PR #100) as well. Headset-confirmed
unless marked:
* VR-102: the startup freeze (a GObjects name/property walk per lookup, about 100 ms each) and the
  1.6 s stall on entering gameplay (five censuses walking with a VirtualQuery per object). Names
  and properties are indexed once, and `GObjForEach` memoises readability per region.
  `CacheNameLookups=1` is the default.
* VR-191: the main menu gets the native stick axes the pause menu has.
* VR-192: the carried object's hold is anchored at the reticle it was tuned at
  (`CarryHoldReticleX/Y`), so re-tuning the reticle does not move it.
* VR-193: a separate left-hand trim for powers (`TrimLP*`), detected from
  `m_EquipUsageInfo[Secondary]`. Hand and held-object adjustments step along the VIEW
  (`AdjustInView`), in F10 and on the numpad. A held item no longer overwrites the empty hand's
  wrist latch.
* VR-194: the reticle stays on under the F10 panel, and the cursor draws only over the panel.
* VR-195: the grab prompt flicker. The hand ray is anchored on the game camera
  (`HandRayGameAnchor`). The tester reported it fixed; the FLICKER_REFERENCE entry is still
  marked a candidate until a log confirms `interact/flicker` stays silent.
* Gamepad look sensitivity 30 (PSI 78/79) in the startup defaults. The log shows -1 -> 30; the
  menu slider has not been checked.
* Defaults = the tester's last run. `kConfigVersion` 15 (14 already shipped in the 2026-09-22
  tester zip), so every existing ini is rewritten once; the runtime choice and DataDir carry
  over.

Next: the sensitivity slider in the options menu, then `interact/flicker` in the next log.

## Session handoff 2026-09-22: VR-165 FIXED (the camera that leaves the body)

Branch `claude/vr-165-camera-displacement`, PR to `VR-Main`, not merged.
* Cause: after a collision pop over 50 uu (a chain release, a knockback) the engine glides the
  camera back, reading the last final location from `camera+0x330`, the field the mod writes its
  head/eye offset into. Our offset re-enters every update and the glide settles at 9.5x our
  offset (measured 9.5-10.0) instead of ending. ENGINE_NOTES and FLICKER_REFERENCE carry the
  derivation and the measurement; the springs hypothesis is refuted.
* Fix: `[CameraShake] PopSmoothing=0` (default) holds the game's own
  `m_bAllowCamSmoothingForCollisionPop` off, so a pop snaps. Headset-confirmed. Live:
  `camshake allow popsmooth on|off`, F10 > Camera shake.
* Also: the census (`camera/springs`, `camera/collide`), the `camspring` kick word (its
  simulator sequence `tools/xrsim/camspring.xrs` never ran), `RflResolveBatch`, and the name
  cache fix (a full cache froze the game at 0 fps: TRAPS.md).
* Known: `camera/displaced` warns on the keyhole, where the game moves the base camera itself.
* Next: the startup freeze (about 10-15 s after launch), its own ticket and branch.

## Next session: VR-165, the camera that leaves the body

Branch `claude/vr-165-camera-displacement` off `VR-Main` `a0ecbb042`. **Read
`docs/dishonored/PLAN-VR-165-camera-displacement.md` first**: it carries the measured facts,
the eliminations, the suspect list with the game's own spring config, the instruments already
on VR-Main (`camera/springs`, `camera/displaced`, `camshake capture`, the RE toolkit) and the
plan in order. The explosion run that reproduced it without a chain is archived at
`build/playtest-candidates/vr185-186-hud-groups/run4`; it predates the spring census, so no
run has spring data yet. First steps are widening the census and a spring-kick seam word, both
before asking for a headset run. PR #100 (tester ini refresh) is open and unmerged.

## Tester build 2026-09-22: 686-ga351bfc31 (VR-190)

`dist/dishonored-vr-41.0.0-tester-20260922-a351bfc31.zip`, `d3d9.dll` sha256 `0A3C57F6...`, the
same DLL installed on the dev PC. `kConfigVersion` 14: a tester's older ini is rewritten once
with the tested profile on first launch (`config: wrote fresh ini (was outdated, now v14)`);
the VR runtime choice and DataDir carry over. Previous tester zips: build 601 (2026-09-21) and
533 (2026-09-20). Branch `claude/vr-190-refresh-tester-ini`, PR open, not merged.

## Session handoff 2026-09-22 (final): VR-165 lead, carry reticle, new defaults

`claude/misc-fixes` (#99). Installed `d3d9.dll` sha256 `E6941D34...`, not run.
* VR-165 (the chain camera bug) reproduced by an explosion's knockback: the camera sat 125-281
  uu from the pawn while PlayerControl's own source stayed at a healthy 77 uu, so the offset is
  added by the additive influence graph after the base camera; weights all normal. New
  `camera/springs` census names the spring state per sample; `camera/displaced` marks an
  episode. FLICKER_REFERENCE top VR-165 entry. Next run: reproduce and read those lines.
* The reticle stays up while carrying an object (the carry hands the arms back to the game,
  which blanked the dot; the throw uses the same ray).
* Defaults from the run: Other items 9.0 / -53.4, the carry hold (-9, 16, -32, 40, 4, -36),
  Element.default.WinX/WinY 0.244 / -0.063.

## Session handoff 2026-09-22 (latest): Sleeve presets baked, other-items reticle (VR-188, VR-189)

`claude/misc-fixes` (#99), not merged. Installed: this commit's RelWithDebInfo (`d3d9.dll` sha256
`3E26128A...`), not run.
* VR-187 (wheel sticks) and VR-188 (sleeve rotation) ran clean in the headset; the Sleeve presets
  are baked: Hands -4.90/0.64, Cuffs -10.00/0.57 (V-marked), Forearm -26.40/0.57 (held value).
* VR-189: F10 HUD > Reticle "Other items X / Y" turns the shared aim ray for everything except
  the pistol and the crossbow (any ammo or upgrade, and the DLC crossbow), by equipped class.
* Tuned: Other items X -14.4 / Y -30.0 and the Cuffs sleeve are now the shipped defaults; the
  reticle sliders reach +/-90 (Y was capped at -30), and F10 now saves them to the ini (it did
  not). Installed `d3d9.dll` sha256 `E4A68128...`, not run. Next run: fine-tune Y past -30.
* Known: `tools\default-profile-host.ps1` fails on drift between the packaged ini and the writer
  that predates this work (trims, cooldown, UiScale, camera shake); only VR-189's keys were added.

## Session handoff 2026-09-22 (later): wheel sticks, sleeve rotation, Sleeve presets (VR-187, VR-188)

VR-185 and VR-186 are headset-confirmed (build 674). Same branch, `claude/misc-fixes` (#99), not
merged. Installed: this commit's RelWithDebInfo (`d3d9.dll` sha256 `7B5D59EA...`), not run.
* VR-187: with `WeaponDial=1` the wheel takes only the hand's direction; both sticks ignored.
* VR-188: the hand turned at cut -10.1 because the palm anchor's vote landed on the hand bone and
  the calibrated offset was dropped (ARM_HAND_SPLIT top section). Anchor pinned to the hand, the
  vote kept off the hand bone, the offset kept across rebuilds. F10 > Hands: Sleeve preset
  (Hands / Cuffs / Forearm) and Sleeve length slider; Cuffs and Forearm are PROVISIONAL.
* Next run: step the sleeve through the whole range (the hand must not turn), pick the cuff look
  and press V, pick the forearm look and press V; open the wheel while running. Then bake the two
  `MARKER #n (V) sleeve:` lines into `sleeve_presets.h`.

## Session handoff 2026-09-22: HUD markers by position, widget groups (VR-185, VR-186)

**Branch.** `claude/misc-fixes` (draft PR #99, stacked on #98), not merged. Installed build is
this commit's RelWithDebInfo (`d3d9.dll` sha256 `310DF2D5...`); not run yet. Pre-run logs
archived in `build/playtest-candidates/vr185-186-hud-groups/before`.

**What changed** (`HUD_ANCHORS.md`, top section; TRAPS top entry):
* VR-185: task markers are claimed by the point the task parent hook publishes (the rune and
  awareness pattern): icon, title and distance. Before, a marker was recognised only after it
  had been seen clamped to a screen edge, so one in view at a load was never recognised and its
  text rode the window while the icon stayed in the image. Diagnosed from the code and the
  2026-09-22 logs; the headset verdict is owed. The text window is a bound: the
  `hud/task-parent` census prints the widest accepted draws so it can be tightened.
* VR-186: widget groups from the draw stream (`core/gfx/hud_group.h`): back-to-back touching
  draws are one widget, and a piece with no row of its own takes the strongest piece's element.
  Applied one present late. The isolated-square-icon rule (the suspected cause of the vault
  icon, the sneak background and the A-button plate going to the image) runs only while the
  task hook is not live.
* Every decision names its rule: `hud/why` (per new key or changed decision), `hud/why-census`
  and `hud/group` every 3 s. Host: 493 hud-route checks.

**Headset run:** load a save looking at a marker; walk to a vault ledge; crouch; open a
dialogue choice. Then read `hud/why`, `hud/why-census`, `hud/group`, `hud/task-owner`,
`hud/task-parent` (the `text=` count and widest offsets).

## Session handoff 2026-09-22: object throwing, hand effects, wrist anchor (VR-181..VR-184); HUD next

**Branches.** `claude/vr-181-object-throwing` is PR #98 (to VR-Main, not merged). `claude/misc-fixes` is draft PR #99, stacked on #98, and it is where the next session continues. The installed build is the tip of `claude/misc-fixes`.

**Headset-confirmed this session:**
* VR-181: carried objects are held at the controller, turn with the wrist, and are thrown along the controller ray. The left trigger throws. The flicker was fixed by anchoring on the game camera, not the last render sample. The tuned hold offsets are the shipped defaults.
* VR-182: hand effects follow the drawn hands (the Heart glow, the Blink and Possession effects). `hands/fx_follow.cpp`. The bone pose comes from the engine's TransformFrom/ToBoneSpace, and the tick is guarded against re-entry.
* VR-183: an empty hand (powers) is placed from the wrist bone, so finger animation cannot swing it. A hand holding an item keeps the calibrated frame, so the reticle stays aligned.
* VR-184: in the mod's vertex copy, hand vertices' forearm-bone influences point at the hand bone, so the wrist cut and cap stay rigid.

**Traps added** (TRAPS.md): RangeReadable is a system call, so never make it per object in a scan. A ProcessEvent call from the script tick re-enters the script tick.

**Next: the HUD.** VR-185 (objective markers on the wrong layer and split from their title, reproducible by loading a save while looking at a marker) and VR-186 (widget pieces split across layers: the vault icon, the sneak background, the dialogue A-button background; group them).

## Misc fixes branch: carried objects thrown by hand, VR-181 (2026-09-22)

`claude/misc-fixes` off `VR-Main` after #96 (the tested build set plus VR-178) merged.
First item: a carried bottle, rock or crate is thrown along the controller ray
(`[Aim] CarryThrowFromHand=1`, word `carryaim`, F10 Aim row "Carried objects (throw)").
Derived offline: ENGINE_NOTES "The carried-object throw seam". The release routine turns
the pawn's aim rotator into a direction and sets the body's velocity to dir * speed plus
the pawn's velocity. The seam replaces the direction only. Built Release and installed;
not run. **Headset check:** pick up a bottle, look one way, point the controller another
way and throw. It should go where the controller points. The log should show a
`carry/aim: carried object thrown along the HAND` line with pitch and yaw before and after;
a `REFUSED` line names why. The same notes cover what a physical throw would replace.

## Menu freshness candidate after combined PR acceptance (2026-09-22)

The user accepted the combined PR build650 apart from menu choppiness, intermittent
mono/stereo and stepped left-hand motion. Work continues locally on
codex/vr-178-menu-scene-freshness from combined76ae6804a. PR87/88/91/93/94 current
heads remain ancestors. Linear VR-178 verified and marked In Progress; VR-144
andVR-128 are related investigations, not declared fixed.

Implemented default-off Hud.MenuSceneFreshness with F10 toggle. The pause-only
previous-draw camera evidence was discarded in journal and wheel; the new bounded
evidence survives only within the current riding/head-look menu epoch and level.
No new engine writer, hand correction, image/pose or pairing change.
72 new policy and404 pairing checks pass, optimized build/export/default/lint checks
pass. Older menu-immersion harness is stale and not counted as a pass.
Detailed evidence/limitations: docs/dishonored/FLICKER_REFERENCE.md top entry and
docs/dishonored/PERFORMANCE.md top entry. No game launched.

Installed651-g410cff3ed, RelWithDebInfo, legacy off. DLL SHA256
D24143368C2B1693D6DFE8D7039CC4858A9A0D2B339F20A24C3B1A5553F89176.
Full installed INI comparison adds only Hud.MenuSceneFreshness=1;1293 CRLF,zero bare LF.
INI SHA2563F780450A7D4B603D9B6C02CBAA633239DE80CBB94395A99E69F2C204D8B5611.
Prior DLL, latest saved INI and both logs archived in
build/playtest-candidates/menu-choppiness/before-menu-fix. Installed DLL and candidate
hashes match;9 exports verified again after install. The log remains650 until launch.
Next: one launch to judge20 seconds of objectives/journal stereo continuity with
slow head/left-hand movement. Read the new banner before interpreting the run.
A steady world with continued hand stepping is a distinct follow-up; no blanket fix
or merge readiness is claimed before that result. No merge to VR-Main authorized.

## Session handoff 2026-09-22: ONE pull request carries VR-170, VR-171, VR-172 and VR-180

- The legacy build guard (VR-180, its own block below) was merged into this branch on 2026-09-22 at
  the owner's request, so the PR's body now opens with four `Fixes` lines and the separate guard PR
  was closed as superseded. The combined tree was built optimised and checked on the simulator
  before it was pushed: banner `legacy off`, eight trigger pulls with 0 frame gaps sat in
  `game_tick`, and the sword and camera shake settings resolved as shipped.

### The three feature tickets (written 2026-09-21)

- Branch `claude/vr-170-171-172-sword-and-camera-shake`, off `VR-Main` `d556eb58`, pushed, ONE PR
  open against `VR-Main` with `Fixes VR-170`, `Fixes VR-171`, `Fixes VR-172`. NOT merged. The three
  single-ticket PRs were closed as superseded by it; their branches are merged into this one
  unchanged, and the three handoff blocks below are theirs and still hold.
- This branch's tree is byte-identical to the local integration build that was tested as a whole:
  `swing-soft.xrs`, `trail-hide.xrs` and `camshake.xrs` all pass on its RelWithDebInfo build
  (`d3d9.dll` sha256 `A4DA6561...`), which is the build left installed on the dev PC.
- Merging the three together needed keep-both resolutions in the docs, `fwd.h`, `commands.cpp` and
  `config.cpp`; nothing else conflicted, and the default ini carries each new section once
  (`default-profile-host.ps1` and the golden check pass on the combined tree).
- Owed, all in the headset: the swing census from a real session, whether the sword's ribbon is
  gone, and the three kinds of camera shake the simulator could not reach (a hit taken, the sword
  landing on an enemy, an explosion).

## Session handoff 2026-09-21: the swing threshold and the hump census (VR-170)

### Where things are RIGHT NOW

- Branch `claude/vr-170-swing-threshold-census`, off `VR-Main` `d556eb58`, pushed, PR open
  (`Fixes VR-170`), NOT merged. It also carries the research brief for VR-173
  (`docs/dishonored/PLAN-contact-sword.md`). VR-171 (hide the sword trail) and VR-172
  (camera shake control) are separate branches from the same session.
- Simulator-verified, headset verdict owed. The record is `PHYSICAL_SWING.md` section 2b.

### What changed

- `[Melee] EdgeSpeed` ships at **3.0** (was 3.6, one rig's number that sat just under that
  player's slowest swing). Existing inis are moved once by a marker-keyed migration
  (`EdgeSpeedRev`), no `kConfigVersion` bump. Measured on the dev PC's ini, which held 3.60:
  `config: [Melee] EdgeSpeed 3.60 -> 3.00 (one-time ...)` on the first launch, no line on the
  next.
- **The hump census.** Every live hand movement above the re-arm level is counted by peak
  speed, split into attacked / did not attack, printed once a minute while it grows and on
  `swing census`; a movement within 20 % under the threshold is a named NEAR MISS. This is the
  half of the distribution a FIRE line never showed, and what the next threshold change is
  read from.
- **A travel guard (`EdgeTravelM`) exists and ships OFF.** Measured in the host tests and on
  the simulator: a real swing has travelled only 0.15 m when it crosses the threshold, so the
  small guard first considered decides nothing. Its value is to come from a player's census.
- F10 > Controls > "Motion sword" opens by default; the speed slider is "swing speed needed
  (m/s)".

### What was found on the way

- **The simulator's display clock leaps about 135 ms across a 50 ms game-thread hitch**, the
  detector rightly re-seeds, and that landed inside about half of all 200 ms simulated-hand
  swings. `swing-soft.xrs` therefore uses `swing sim` for its threshold legs (TRAPS has the
  measurement and the suspect that was cleared). It also showed a census hole, fixed: a hump
  ended by a tracking gap is reported `CUT SHORT`, not dropped.
- `release/dishonored_vr.ini` had drifted from the production writer on `VR-Main`
  (`default-profile-host.ps1` failed before any change of this session); regenerated.
- The worktree's git identity was a personal one; the four commits were re-authored to the
  repository's noreply identity BEFORE the first push. Nothing personal reached the remote.

### Next steps

1. Headset: soft swings register; walking, turning and reaching do not attack. Send the log:
   the `swing: census` lines decide whether 3.0 stays and whether `EdgeTravelM` gets a value
   (`least travel at a fire` against the travel on any unwanted `hump ... -> ATTACK` line).
2. VR-173 when wanted: paste section 5 of `PLAN-contact-sword.md` into a fresh session.
## Session handoff 2026-09-21: the sword's swing trail is hidden (VR-171)

### Where things are RIGHT NOW

- Branch `claude/vr-171-hide-sword-trail`, off `VR-Main` `d556eb58`, pushed, PR open
  (`Fixes VR-171`), NOT merged. Same session as VR-170 (its own branch and PR) and VR-172.
- Mechanics simulator-verified; the PICTURE is not, and cannot be on the simulator (below).
  The record is ENGINE_NOTES "VR-171".

### What was found

- **The swoosh is not a stock anim-trail notify.** `TrailsNotify` and its two siblings are in
  the name table and the ProcessEvent observer saw 0 of them in 4 sword attacks. A hide built on
  that route was removed unrun.
- **It is one particle component on the player pawn, template `Sword_Trail`**, added 282-290 ms
  into the first attack and kept attached afterwards. `swordtrail census` found it and stays as
  the instrument that names whatever an attack adds to the pawn.
- **The hide** is the engine's native `SetHidden` on that component (the rain box's pattern),
  `[SwordTrail] Hide=1` by default at the owner's request, live `swordtrail on|off`, F10 checkbox.
- **Found by the combined default-on run and fixed:** hidden as it appeared, then forgotten one
  scan later because a component 258 ms old is not yet in the 2 s live-object table; the lever
  could then not show it again. An attached component is now live because the pawn's own list
  handed it over on that scan. `trail-hide.xrs` leg 0 covers it.
- **The simulator never showed the ribbon**, about 30 captures with the hide off. So there is no
  capture A/B: it could not have failed. `trail-hide.xrs` asserts the mechanics and says so.

### Next steps

1. Headset: swing the sword with the checkbox on and off (F10 > Controls > Motion sword). On: no
   ribbon. Off: the ribbon as before. Watch an enemy swing: its trail must still be there.
2. If a ribbon survives with the lever on, run `swordtrail census`, swing once, and send the
   `trail/census:` lines: another template name goes into `[SwordTrail] Template`.
## Session handoff 2026-09-21: the game's own camera shake, attributed and removed (VR-172)

### Where things are RIGHT NOW

- Branch `claude/vr-172-camera-shake-control`, off `VR-Main` `d556eb58`, pushed, PR open
  (`Fixes VR-172`), NOT merged. Same session as VR-170 (PR #89) and VR-171 (PR #90), each its
  own branch. The record is ENGINE_NOTES "VR-172".
- Simulator-verified for landing, the weapon kick, bob and roll. Damage taken, a sword landing
  on an enemy and explosions could not be staged and are held by the influence's name only.

### What was found

- The shakes are Arkane camera influences, all at weight 1 all the time, so a weight attributes
  nothing: the method was the same staged action with one handle held at zero, read from
  per-tick rows. Landing dip 45.8 uu = `PhysicalReact`; pistol kick 2.84 deg = `Recoil`; the
  jump's push-off lag 10.4 uu = `BumpSmoother` (the stair smoother, kept); bob and roll = two
  camera floats the head-bob option already had at 0; `m_fReactionWeight` = a master over the
  group, deliberately not used (it would take Lean and Aim with it).
- **A walk still moves the camera 1.5 uu and standing still 0.4 uu with everything at zero.**
  That is the animated first-person body the camera rides on, not a shake; VR-175.
- Three readings were retracted on the way and are recorded (ENGINE_NOTES, TRAPS): the push-off
  was first credited to `HitReact` off a capture that had opened too late; four rounds read
  shake-free because the single-slot seam dropped the harness's release; the event's trailing
  ints are not the stick's DeltaRot.
- `hud-elements.xrs` fails `quadLayers (3) eq 2` on `VR-Main` with this feature off as well:
  filed as VR-176, not touched here.

### Next steps

1. Headset, F10 > Controls > Camera shake: walk, sprint, fire, jump and land with the master on,
   then off, to feel the difference. Then the three the simulator could not reach: take a hit,
   land the sword on an enemy, stand near an explosion. If any of those still moves the view,
   `camshake status` and the log's `camshake: beat` line say what is held.
2. With `Landing` removed, check a knockdown and the camera near walls still behave
   (`PhysicalReact` also carries `m_bHandleCameraCollision` in the game's own ini).
3. Judge the stair smoother: stairs, and a jump's push-off, with `Smoother` on and off.
## Head aim customization, VR-166 / VR-167 / VR-168 (2026-09-21)

Branch `claude/vr-166-head-aim-customization` off VR-Main `d556eb587` (which carries the
merged #82, #85 and #86). NOT merged, no PR merge authorized. Installed and pushed:
`vr33-hands-working-615-g9843251f5` (confirmed), then the probe-gating build. Installed ini deltas vs defaults: `[Aim]
SourceProbe=1` (the read-only probes/censuses below need it), `[Hud] Element.reticle=window`,
`ReticleOnAim=1`, `[Draws] Census=0`.

**Headset-confirmed:**
* F10 Aim has a Head/Controller table: crossbow+pistol, Blink, Interactions, Grenades,
  Spring razors, each saved to the ini at once.
* Interactions follow the weapon ray (`interact_aim.cpp`: first-pass line check
  `0x00AA60B1` + usable selector `0x00AB70F0`).
* Grenades follow the weapon ray (`throw_aim.cpp`: rotator seam `0x00C3908C` in the
  throw routine `0x00C38F70`).
* The grenade cook ring rides OUR aim dot, right size, and the dot hides meanwhile
  (`[Hud] ReticleOnAim`, `hudroute::centered_gauge`). It drew in 229/231 presents.
  Small flicker PARKED at the tester's call (FLICKER_REFERENCE).
* VR-167: notes, journal and pause no longer snap the view back on close (menu-hold
  fallback in `head_track.cpp`, plus the note-to-wheel handover in `menu_immersion.cpp`).
* VR-168: after a possession the head writer mis-parsed the camera-modifier
  ProcessViewRotation (a pawn pointer passed as a DeltaTime), so slides went mono. Fixed by
  an object test; confirmed with two possessions.

* **Spring razor placement follows the weapon ray** (build 615, headset-confirmed): 13
  placements, every one 0 uu off the hand ray and 4-99 uu off the head's. Seam: the razor's
  wall-placement trace `0x00C32C30` reads the camera POV through esi; `0x00C32C91` swaps in
  one built from the hand ray (ENGINE_NOTES "The razor placement seam"). `[Aim]
  GadgetFromHand` and the F10 "Spring razors" row now drive it; the dead gadget seam at
  `0x00C300DD` and the razor write-watch probe are gone.

**Open (PR #87 is ready for review, NOT merged):**
1. **VR-169**: a razor placed close to the player is invisible (still works). Suspect the
   weapon matcher claiming it as the held razor. `wa/razor:` lines (now behind `[Aim]
   SourceProbe`) name the gate; the ticket has the next measurement.
2. **Pickup with controller aim is finicky** on small objects. Proposed, not built: trace
   from the head THROUGH the hand ray's target.
3. **Powers** (Windblast, Swarm, Possession): part 2, VR-44, branch
   `claude/vr-44-head-aim-pt2-powers` off this one. Static map in ENGINE_NOTES "Where the
   powers read their aim": Windblast reads the camera POV at `0x00BF9570`; Possession and
   Swarm are predicted to take their aim from the UsePower aim-assist search `0x00C12B00`.
   Census measured (build 618); seams built in `power_aim.cpp` (`[Aim] PowersFromHand`):
   All three headset-confirmed: Windblast `0x00BF9615` and Possession `0x00BF8F4C` (619),
   Swarm at its GetPlayerViewPoint seam `0x00BE9337` (620: landed 26 uu off the hand ray,
   827 off the head). The power and trace censuses were removed after a lag report; the
   lag was reported gone on 620 (PERFORMANCE.md). PR #88 ready, NOT merged.

Probes left, all read-only and armed only by `[Aim] SourceProbe` (code default 0; the
installed ini has it at 1): the SpawnActor and trace censuses and the helper probe in
`aim_source.cpp`, and `wa/razor:` in weapon_attach.
## F10 panel from the motion controllers, VR-174 (2026-09-21)

Branch `claude/vr-174-f10-menu-motion-controls` off VR-Main, draft PR #91. NOT merged.
This ports the BioShock trilogy mod's F10 motion controls (plan, as-built notes and the
lines to read: `docs/dishonored/F10_MOTION_CONTROLS.md`):
* tap both stick clicks to open the panel, hold them to recenter;
* the right-controller ray is the cursor (through the eye FOV) and the trigger clicks;
* the right stick scrolls, and nudges the pointed-at slider;
* the panel is sized to the eye texture, with a text-scale slider;
* while it is up, the right trigger and stick are withheld from the game and the laser and
  dot are hidden.

Headset-confirmed on build 602. The default size and place are the ones the tester chose
(the geometry probe's fractions). The #87/#88 aim work is on its own branches and is not in
this one.
## Playtester crashes, VR-177 (2026-09-21)

Branch `claude/vr-177-crash-fixes` off VR-Main. NOT merged. There are two reports from one
playtester on build 533 (a rooftop freeze, and a crash after Piero's cutscene). Neither
left a fault record, and the reason was our instruments (TRAPS.md, top entry):
* the watchdog's stack scans spent the crash fingerprinter's 3-fault budget at startup;
* those scans faulted while another thread was suspended, which is a deadlock hazard;
* the watchdog's `pacetrace.log` was not in the support bundle.

All three are fixed on this branch.

Evidence so far:
* **Piero (503):** the dialog ended normally. About 35 s later the GPU queries for 4
  presents never resolved, and the game thread then sat 56 s inside its own frame, with
  memory and VRAM healthy. After that the process died. A GPU/driver stall is suspected,
  not proven.
* **Rooftop (701):** the log just stops.

Next: a tester build from this branch. The next occurrence should arrive with a
fingerprint and watchdog stacks. The Piero UI-hold clue is noted on the ticket; the 503
timing does not tie the stall to the shop itself.
## Session handoff 2026-09-22: the trigger-pull freeze was a legacy build, and cannot recur silently (VR-180)

- Branch `claude/vr-180-legacy-build-guard`, off `VR-Main` `d556eb58`, pushed, PR open
  (`Fixes VR-180`), NOT merged. Tools and two strings only; no feature code is touched.
- **The fault:** every trigger pull froze the game about a quarter second. The build installed
  for the 2026-09-21 headset session had `src/legacy` compiled in, because `tools\build.ps1` only
  passed the legacy switch on a first configure and the build directory's CMake cache still held
  `ON`. Its projectile-spawn tracer walks every engine object on each trigger edge. Measured in the
  headset log and by a simulator A/B (11 stalls of 75-133 ms against 0); `docs/TRAPS.md` has it all.
- **The guard:** `build.ps1` reconfigures the switch on every call and says what it built; the
  log's first line and `status.json` say `legacy ON|off`; `install.ps1` refuses an optimised legacy
  build without `-AllowLegacy`; `package.ps1` refuses it outright. Proven end to end: a `-Legacy`
  build was refused by the installer, and the plain build straight after it came out clean by
  itself and showed 0 stalls in eight trigger pulls.
- **Left on the dev PC:** an optimised, legacy-OFF build. Check the log's first line reads
  `legacy off` before a headset session.
- **Who else could have had it:** anyone given a zip packaged from a build directory that had seen
  `-Legacy`. Any `[legacy]` line in a tester's log answers it for that tester.

## Session handoff 2026-09-20 (night): the dev PC's frame rate, attributed (VR-160)

### Where things are RIGHT NOW

- Branch `claude/vr-160-perf-4060-attribution`, off `VR-Main` `5dfe6c9d`, pushed, PR open
  (`Ref VR-160`), NOT merged. VR-160 is In Progress: one headset number is still owed.
- **The dev PC now has the RelWithDebInfo build of this branch installed**
  (`vr33-hands-working-550-ged3621c8`, `config RelWithDebInfo`, d3d9.dll sha256
  `6EE3CD19...`). Its ini is byte-identical to the one the session started with; the
  DLL and ini it replaced are in `D:\dvr-data\backup-vr160\`.
- The full record is the VR-160 entry at the top of `docs/dishonored/PERFORMANCE.md`.

### What was found

- **The record mixed two machines.** Every fast number in PERFORMANCE.md (9.4 ms ticks,
  about 100 pairs/s, the 0.64 ms/MP fit, "quarter pixels bought 6 %") is the tester's
  RTX 4070 Ti SUPER. The dev PC is an RTX 4060, and 3012x3122 was never judged on it.
  There was no regression to find.
- **The card's ceiling at 3012x3122 is about 62 pairs/s**, measured on the simulator
  (which renders the same size): 7.9-8.1 ms of GPU-busy time per scene render, the same
  under `stereo mono`, `stereo reentry` and the slow phase. The mod runs at 86-87 % of
  that (54-56 pairs/s). All of the mod's own overhead together is worth at most 12 %.
  Two scene renders per displayed frame cannot reach 72, 90 or 120 Hz at this size on
  this card.
- **Debug costs 6-7 %** (20.1 / 18.9 / 20.4 ms, Debug / RelWithDebInfo / Debug) plus one
  40 ms `game_tick` hitch a second that the optimised build does not have. The log could
  not say which config wrote it; now the banner, the crash header, `status.json` and a
  Debug tick line all do, and `install.ps1` warns loudly. Default stays Debug.
- **A 12-13 s game-thread cycle on this save** (6 s at 17.5 ms, 6 s at 22-24 ms) is
  identical in Debug and RelWithDebInfo, so it is the engine's or the level's, not ours.
- BioShock Infinite is comfortable on the same card because it draws 9.1 MP per pair
  against Dishonored's 18.8 MP, with the same two-renders-per-tick method.
- Nothing game-side caps the rate; the log is not flushed per line; the shared (no CPU
  copy) capture is already the default.

### Fixed on the branch (one commit each)

The build config in the banner and status.json; the loud Debug line in `install.ps1`;
frame gaps itemised three per 5 s window with the rest counted (a headset-idle run wrote
32,000 lines of them); `armfollow` and `headtrack` lines bounded; `boot.ps1` passing its
key by name (it could not walk in at all); four analysis scripts under `tools\perf-*`.

### Found and not fixed

- VR-162: `hkSetVSConstF` carries its view-model hide, its `DcNotePalette` call and its
  palette cache twice. Correctness first, cost second; hands are headset-judged, so one
  block per build.
- VR-163: `DVR_SKIP` disables nothing, and the HUD redirect's copies are timed inside the
  field the gap line calls `present-tail (xrEndFrame)`.
- The shipped ini turns on `[Hud] Regions`, `[Perf] NativeProfile` and `[Perf] BridgeGpu`
  against compiled defaults of off. Unmeasured on this card and bounded by the 12 %.

### Next steps

1. **Headset, the user, one short run** (steps on VR-160): the RelWithDebInfo build at
   120 Hz, standing still for three minutes, while the GPU sampler runs. It answers the
   one thing the simulator cannot: what the streamer's encode takes from the same card.
2. Decide the route, which is the user's call and not a code fix: a render size this
   card can hold, or an architectural change (alternate-eye or a reprojected second
   eye). PERFORMANCE.md states the arithmetic; nothing is built.
3. VR-162, then the shipped-on instruments, for the bounded 12 %.

## Session handoff 2026-09-20 (evening): the motion sword is headset-judged and merged

- **MERGED: PR #81 into `VR-Main` as `50249bde`, 2026-09-20, with permission.** It
  carried VR-37 (the slash) and VR-155 (the sneak kill); #83 was folded into it and
  both tickets are Done. The landed tree is identical to the head that was tested.
- Headset run on the dev PC (VDXR, 3012x3122): 47 swings, 46 honoured, 0 refused,
  2 `HONOURED kill`. Verdict: nothing to change. The shipped defaults are now the
  judged build's: `Detector=edge`, `Stab=1`, `StabStyle=plunge`, `HonourHaptic=1`.
  Record and the caveat (both kills came through the SLASH detector; the slow-plunge
  path is simulator-proven only): `docs/dishonored/PHYSICAL_SWING.md` section 2a.
- Open: VR-156 (a readable kill-available signal; it also carries the one headset
  observation still owed, a SLOW plunge under 3.6 m/s producing the kill).
- `VR-Main` = `50249bde`. The dev PC runs the Debug build of the merged head.
- Found and not fixed, VR-159: an ini older than `kConfigVersion` is rewritten
  wholesale at launch, which silently drops a machine's render size and F10 tuning
  (the dev PC's 3012x3122 would have become 2750x2850). The dev PC's ini is pinned at
  `Version=13` by hand meanwhile.

## Session handoff 2026-09-20: the motion sword (VR-37)

### Where things are RIGHT NOW

- Branch `claude/vr-37-physical-swing`, off `VR-Main` `fd5fbde9`, pushed, NOT merged.
  Everything else in the 2026-09-19 handoff below still stands.
- **Swinging the right controller swings the sword, proven on the simulator, not
  yet judged in a headset.** Full record: `docs/dishonored/PHYSICAL_SWING.md`.
- The dev PC has this branch's Debug `d3d9.dll` installed. The DLL and ini it
  replaced are in `D:\dvr-data\backup-pre-vr37\`. `swing save` was exercised during
  testing, so the INSTALLED ini on this PC reads `[Melee] Detector=edge` (the
  shipped default is `sustain`).
- Logs of every run: `D:\dvr-data\logs\vr37-*.log`.

### What was measured

- The old detector never fired: three scripted 0.68 m swings, sword drawn, every
  gate open, 0 attacks, ten "flicks" of 4-39 ms with 8-14 m/s peaks. The render
  presents twice per tick (`out/s=171 L/s=85`) and it was fed once per present.
- `Detector=edge`: a swing fires at 4.3-4.6 m/s and the game is in
  `StatePlayerMeleeAttack` 15-16 ms later (HONOURED), every time. A 0.68 m reach in
  900 ms peaks 1.61 m/s and does not fire. A body turn (head and hand together)
  does not fire, and DOES with `swing rel off`.
- Gates: block grip, power wheel, pause menu, sheathed sword each give exactly one
  `swing: BLOCKED` line with the reason.
- `Output=rb` on this machine: the game BLOCKS and the check says NOT HONOURED, so
  the right trigger is the attack here. Another install may differ; its log says.
- `Detector=sustain` on the clean sample feed fires too (`run 122 ms 0.50 m`), so
  the headset A/B is a fair one.

### Next steps

1. **Headset, the user:** `swing mode edge`, swing, read PEAK in F10 > Controls >
   Motion sword (or `swing status`), set `EdgeSpeed` a little under it, play a few
   minutes watching for false attacks while walking, turning, reaching and using
   the wheel. Then `swing mode sustain` for the comparison. The tuning table is
   PHYSICAL_SWING.md section 6. The verdict decides the shipped `Detector` default,
   which flips in its own commit.
2. **VR-155, the sneak-kill thrust: BUILT on `claude/vr-155-sneak-thrust`** (stacked
   on the VR-37 branch, pushed, not merged). `swing stab on` while crouched: a thrust
   fires at 0.20 m of extension and is HONOURED; standing it is silent; a jab, a
   floor reach and a slash are not stabs (`swing-stab.xrs`, 7 legs). NOT observed:
   the kill itself. The default motion became a PLUNGE after the first headset
   feedback: the sword sits in a reverse grip, so a forward thrust is not a move
   anyone makes (`StabStyle=plunge|thrust`, `swing-plunge.xrs`). The dev PC's
   installed ini is preset for the run (edge, Stab=1, plunge) and pinned at
   `[Meta] Version=13` so the 09-19 default refresh does not drop its 3012x3122 to
   2750x2850. Not seen yet because the dev PC's newest save is the Hound Pits pub, which has nobody
   to kill. Headset: crouch behind an unaware guard, thrust, look for
   `swing: HONOURED kill`. Tuning: PHYSICAL_SWING.md section 7.
3. VR-156, research: a readable kill-available signal for the thrust's ready cue.

### What is deliberately not here

- The shipped default stays `Detector=sustain` until the headset verdict.
- No aim for the blade: the swing decides when, the game decides where.
- The choke gesture (VR-145) is untouched; what this work learned about the pad
  binding is on that ticket.

## Session handoff 2026-09-20: the game's own option settings, and two live display levers

### Where things are RIGHT NOW

- `VR-Main` = `fd5fbde99`. **Working branch: `claude/vr-157-game-opts-probe`** (`ef299585c`)
  = VR-Main + two commits. NOT merged.
- Installed on the dev PC: Release from `ef299585c`, both new levers in.
- Carried over from yesterday and still open: VR-154 (unverified, one SteamVR run
  with a recenter settles it and VR-146), VR-152 judder (REOPENED), VR-153
  death/respawn (fixed but unshipped on `claude/vr-152-pair-rate`).

### VR-157: where the player's option settings actually live

**None of the twelve are in the game's 21 inis.** They are in the Steam Cloud
profile blob, `userdata/<id>/205100/remote/OPTIONS.sav` - 722 bytes, bit-packed
UE3 `OnlineProfileSettings`, not text. The id table is
`tools/uscript/dishonored/Engine/OnlineProfileSettings.uc`; the twelve ids and
their `[SystemSettings]` mirrors are tabulated in `GAME_CONFIG_MAP.md`.

The mirror is not authoritative and has been caught disagreeing:
`DishonoredEngine.ini [SystemSettings] bAllowLightShafts=True` while the in-game
menu reported light shafts off. That disagreement is what makes the probe worth
having rather than a table lookup.

`gameopts` on the seam (read-only, needs GAMEPLAY) prints both sides per
setting: the profile value through the engine's own `GetProfileSettingValueInt`,
and the live renderer value through `scale get <key>`. It verifies each id's
name with `GetProfileSettingName` and marks a row `MISMATCH - do not believe
this row` when the table is wrong for this build, and it distinguishes
`NO ANSWER` from a value of 0 so a missing id cannot read as a setting that is
off.

**Still open after one run of it**: which of `Gamepad_bAutoAim` (81) and
`Gamepad_bFriction` (83) the menu labels "Auto Aim" and which "Aim Assist". The
INT localization files carry neither string, so it needs the log next to the
menu, not another grep.

### VR-158: fullscreen and vsync are now live levers

Both were unswitchable during a run. Fullscreen was the literal `1` in
`UWindowsViewport::Resize` (VR-50); vsync is read only by `UncapPresent`, which
runs at CreateDevice and Reset. `fullscreen on|off`, `vsync on|off`, and two
checkboxes in F10 Display. Each costs one device reset.

**Nothing is measured.** The prediction is in `PERFORMANCE.md`: at one fixed
resolution with the mirror already off, windowed -> fullscreen should move the
tick and pairs/s; vsync should move the present rate but not pairs/s. If
fullscreen moves nothing, the windowed cost and the desktop-mirror cost are the
same cost and the mechanism is wrong - record that outcome rather than leaving
the prediction standing.

Read pairs/s on `stereo: beat`, not the tick mean. TRAPS carries the VR-152
reading error where a 59% pair loss showed as 13% on the tick.

### Next steps

1. One gameplay run with `gameopts` - it answers VR-157 outright and pins the
   aim-assist label. No headset needed.
2. The VR-158 four-way A/B at one resolution, in the headset.
3. The VR-157 WRITE path is deliberately not built. `SetProfileSettingValueId`
   (`0x005CC400`) and `SaveProfile()` exist, and `OnLeaveOptions` (`0x009F7640`)
   is the game's own apply path, but which consumers honour a write live is
   exactly what the probe is for. Fullscreen and vsync must stay OUT of any
   profile write - the mod already owns both.
4. Still carried: VR-154, VR-152, VR-153 as above.

## Startup defaults accepted; PR ready, chain bug OPEN (2026-09-20)

PR86 finalized for review, NOT merged; remains stacked on claude/vr-164-pause-hands.
Current branch codex/vr-165-camera-source-review. Installed/tested build559:
`vr33-hands-working-559-g7efcfcdc7-dirty`, SHA256
`48b0bc36ca5e03f44d912da8ede4c106f0b91c355515ef753a5ca3938a34d18a`.
Latest log banner/hash verified; all ten requested profile values match. Tester
confirms startup head bob off and sound working on the next run. Earlier silence
was intermittent/unexplained; the audio diagnostic did not constitute a fix.
Accepted DLL/INI/both logs archived in build/playtest-candidates/accepted559.

Default preset: Kill Cam off; Head Bob0; Chain Climbing Relative off; Crosshair
Style off; Auto Aim off; Aim Assist off; Model Details high; Light Shafts off;
Antialiasing MLAA; Rat Shadows off. [GameOptions] DefaultsAtStartup=1 by default
in code/generated INI/golden fixture and explicitly in packaged release INI.
F10 Advanced > Apply VR defaults at startup saves opt-out for future launches.
Startup mode0 writes continue until first gameplay (profile reloads can overwrite
earlier writes); later menu changes survive. Audio, fullscreen and vsync excluded.
Package change intentionally copies ONLY the requested startup-policy setting:
tested INI also contains a machine-specific runtime manifest, unrelated grip edits
and active camera probes, which are outside this defaults-finalization request.
No binary change since accepted559; build/lint/exports and86 host checks passed.

**VR-165 chain-camera bug remains OPEN / In Progress.** This PR improves its
instruments, not its cause. Relative climbing off is a preference, not proof of
resolution. Resume from FLICKER_REFERENCE and camera-source ENGINE_NOTES; use
healthy -> chain X-release bug -> Blink comparison. EyeHeight stayed85; influence
weights were static; missing vectors are unavailable, not zero. Group1 null was
explicitly observed. Old frequency/radius claims were artifacts; do not reuse.
First inspect current raw pawn/camera source deltas and correlate what changes
and returns after Blink. No guessed clamp/reset. One question per tester launch.
Do not merge this PR or its base without explicit authorization; no subagents.

## Head bob accepted; audio follow-up (2026-09-20)

Run558 banner/hash verified. Tester confirms head bob now turns off at startup.
Four mode0 calls observed; the third restored head bob1 before our write reset0.
Startup window closed at first gameplay as intended. Keep this successful path.
No sound reported on that run; cause UNKNOWN. Preset targets exclude audio IDs
126..129 and133. Engine Launch.log contains no useful audio initialization/error
information; do not claim routing, zero volume, or the hook is proven responsible.

Installed `vr33-hands-working-559-g7efcfcdc7-dirty`, SHA256
`48b0bc36ca5e03f44d912da8ede4c106f0b91c355515ef753a5ca3938a34d18a`. Small READ-ONLY diagnostic update:
log audio IDs/types/raw bits before shared refresh, and include them in the
existing gameplay profile read. No audio writes or routing changes. Release,
lint, nine exports, 86 host checks pass, including preservation of all five audio
values. Full installed INI byte-identical and CRLF verified. Prior558 DLL, both
proxy logs, INI and engine Launch.log archived at
`build/playtest-candidates/installs/20260920-212602`.

Next launch ONE question: is sound audible after loading gameplay? If yes,
record intermittent silence without claiming this diagnostic fixed it. If no,
compare gameopts/audio startup values with gameplay IDs126..129/133. Zero/missing
values warrant tracing profile reload/shared refresh; nonzero profile volumes
require checking live audio consumers and Windows/VR output/session mute. No
forced volume changes without evidence. Existing detailed handoff follows below.

## Startup reload follow-up and session handoff (2026-09-20)

Installed `vr33-hands-working-558-gf83dc0edb-dirty`; SHA256
`f0d861f7f479bdc90c6023d1e059cdce6eb4b1179ff7c005b2be5195a131eb42`.
Release build, lint, exact nine exports and 80 production host checks pass.
Installed INI byte-identical to previous (full comparison), CRLF verified:
[GameOptions] DefaultsAtStartup=1; [Diagnostics] GameOptsWrite empty.
Prior557 DLL/INI/log and prev.log archived in
`build/playtest-candidates/installs/20260920-212026`. Never launch the game.

**557 failed in gameplay:** tester still observed head bob. Log banner and
installed hash matched557 before interpretation. At timestamp44673515 profile
1770D800 id108 changed float1 ->0, all ten preflighted writes/readbacks succeeded.
At44727031 the diagnostic selected the SAME populated profile1770D800 and read
head bob float1 again. Thus a profile overwrite occurred after our startup write;
the write/hook did not simply fail. Its exact writer and timing remain unknown.
The original callback latched done after the first success and hid all later
calls, so this run cannot establish how many additional startup applies occurred.

**Last fix candidate:** remove first-success completion. Intercept every mode0
apply until first verified gameplay in GameOptsApply, independent of diagnostic
auto-read being enabled. Then close the startup window permanently for this
process. Modes1/2 (menu applies) are never forced. Saved opt-out still disables
writes. No retained engine pointers; every write refreshes BuildLiveSet and
preflights all ten values. First24 native apply entries log profile, mode and
startup-closed state, including ignored modes. This is a targeted hypothesis,
not proven acceptance: it only fixes the overwrite if another mode0 apply
occurs before gameplay. No menu-dependent fallback or periodic gameplay forcing.

**One question next launch:** is head bob off on entering gameplay, without
opening Pause/Options or touching sliders? Off supports live startup propagation.
Still on: read new apply-observed lines and id108 readback before another edit.
- If additional mode0 calls restore0 and gameplay still reads1, locate the later
  profile writer/load completion; do not add a longer timer blindly.
- If only one early mode0 call appears, this hook is too early for final profile
  loading. Trace the asynchronous profile read completion and the other callers
  of shared helper0x0093B7E0; mode1/2 logs distinguish other apply paths.
- Profile0 with bob still on is the consumer-notification problem, distinct from
  this run's measured overwrite. Do not claim profile match proves acceptance.

**Resume map:** branch codex/vr-165-camera-source-review, existing stacked PR86;
no merge authorized. User wants default-on startup restoration with saved F10
Advanced opt-out; later deliberate changes survive the session. Do not switch
to continuous enforcement. Core code game_opts.cpp, entry constants patterns.h,
early installation proxy/dllmain.cpp, F10 overlay.cpp, config default config.cpp.
Host harness tools/game-opts-host.ps1. Addresses/derivation in ENGINE_NOTES under
Startup VR preset interception. Prior verified menu setter is0x00BCB870, guarded
open pause menu/listeners; ProcessEvent returning did not prove native execution.
Head bob108 is float type5, range0..1. Other preset targets int105=0,109=0,99=0,
81=0,83=0,120=1,121=0,122=1,123=0. Fullscreen116/vsync117 excluded deliberately.
Build556 native menu route + subsequent boot maximum bob were tester-confirmed;
automatic reset to0 remains unconfirmed. Other targets need downstream checks.
Chain-camera issue remains separate/unresolved. Preserve camera probe evidence.

## Automatic startup preset candidate (2026-09-20)

Installed `vr33-hands-working-557-gba3ac15a4-dirty`, SHA256
`a30434f808e2b95360d04becf207ec91089a18b54cd6c5afdf25ae286804de32`.
Archive: `build/playtest-candidates/installs/20260920-210853` (prior DLL,
full INI, both logs, and full INI diff). Installed INI changes only:
GameOptsWrite cleared; [GameOptions] DefaultsAtStartup=1 added. CRLF verified.
Release build, lint, nine exports and 79 production host checks pass.

Latest run556 banner/hash verified. Profile head bob was already float1 before
menu apply, and maximum bob working immediately after boot is tester-confirmed.
The native menu write's persistence is observed; startup preset interception is
NEW and not yet headset-confirmed. No chain-camera resolution claimed.

New preset intercepts the engine's shared settings apply BEFORE its refresh and
listener dispatch, only mode0 and once after successful validation per process.
All ten entries preflight together, with refreshed liveness and exact types.
Later menu edits survive. F10 Advanced > Apply VR defaults at startup saves
[GameOptions] DefaultsAtStartup; absent key means on, off preserves preferences
on future boots. Policy read occurs in the engine callback outside loader lock,
so startup before Direct3DCreate9 is covered. No persisted completion marker.
Derivation and limits: ENGINE_NOTES, "Startup VR preset interception".

Next launch, one question: is head bob OFF immediately in gameplay without
opening Pause/Options or moving sliders? Previously maximum is the baseline.
Off supports startup live propagation; still bobbing requires checking hook,
mode0 observation, validation and consumer effects in the new log. No launch
by agent, no merge. Other preset settings still require downstream acceptance.

## Verified native settings candidate (2026-09-20)

Installed `vr33-hands-working-556-g90543a1ad-dirty`; SHA256 `4eb0154e2069d8c06a2444ca1eaee4bdb7485c093e4e7da128a08a0853997743`.
Prior DLL, full INI and both logs: `build/playtest-candidates/installs/20260920-205135`.
Full installed INI byte-identical, CRLF verified; GameOptsWrite=108=1.0 remains
armed. Release, lint, nine exports and42 production host checks pass. No launch.

Fixes: apply returns a defined result; float readback compares floats; malformed,
nonfinite, duplicate and unsupported requests are rejected before any call.
Raw fallback removed. Existing-target equality is not reported as a change or
proof of gameplay acceptance. Only an open pause-menu instance with a live
settings-listener list is eligible. Readiness is polled once per second after
initial gameplay read; closed/missing menus consume no apply attempts.

Dispatch now directly calls the verified native implementation (address and
prefix in patterns.h), checking the live pause instance's vtable target and
code bytes. This avoids claiming ProcessEvent return proves native execution.
Current liveness is rebuilt before each engine call. At most three eligible
attempts; exact profile match stops retries, while the live effect remains
explicitly unverified. No persistence call or merge.

Next launch, one question: does opening Pause apply maximum head bob without
moving the slider? Load gameplay, walk briefly, open Pause for about3 seconds,
resume and walk. Bob becoming active supports live apply. Menu/profile1 with
no bob means a consumer is still not updated. No eligible menu or failed
readback is a refusal, not an apply success. If bob was already maximal before
Pause, behavioural change is inconclusive. Read the log against this banner.

## Head-bob storage versus apply (2026-09-20)

Run551 confirmed profile108 float1 ->0 and menu0, but bob stopped only after
manual menu change. Decompiled scripts plus verified native handler trace
identify the missing shared-settings refresh and listener notification.
OnSettingChange uses profile PropertyId directly. No native apply call has yet
been added; no new build installed in this research step. Next implementation
must validate a live initialized menu/listeners and use the engine apply path,
then test actual bob without manual slider changes. Preserve the independent
chain investigation. Full evidence/derivation: ENGINE_NOTES, "Head-bob apply
path located". Maximum profile value measured1, not100.

## Camera delta audit after run 549 (2026-09-20)

Current state: installed `vr33-hands-working-551-g90ea17a76-dirty`, SHA256
`d1a6c5449f0d57e0e1365ff3cf0ed1f92722f05d7625b9ea524b5f13d518b494`. Release/lint/nine exports pass.
Full installed INI compared with prior install, CRLF preserved; camera probes
armed and GameOptsWrite empty. Prior install archived under
`build/playtest-candidates/installs/20260920-201617`. No game launch by agent.

Run 549 matches the prior installed candidate's banner (compiled 19:43:20).
The DLL present at review was already candidate 550, so it is not run 549's
binary. Both logs preserved in `build/playtest-candidates/camera-source/run549`;
the DLL in that review archive is 550 and must not be attributed to the log.
Measured: 272 samples with EyeHeight/BaseEyeHeight=85; 272 explicit group-1
not-live rows. Shipped declarations initialize group 1 to none. Old logs do
not distinguish null from non-null rejection. Large eye deltas do not prove
subtraction failure: operands and cache freshness were not recorded.

Changes: log raw camera/pawn world positions, read validity and controller
ownership beside the delta. Refuse delta on failed reads/nonfinite inputs or
owner mismatch. Debug POVs carry source-minus-pawn plus raw source values;
freshness remains unverified. Null group slots are explicitly EMPTY, rejected
non-null pointers retain their address. Existing head-bob work preserved.

Next launch, one question: does the PlayerControl pawn-relative source share
the displacement after chain X-release and return after Blink? Compare standing
pitch before, after release, then after Blink. Matching source movement points
upstream; unchanged source with changed cache points downstream or stale debug
fields; invalid reads/ownership or no reproduction remains inconclusive.

## Camera source and option review (2026-09-20)

Current state: branch `codex/vr-165-camera-source-review`, based on the attached
review-plan commit `bf5733638`; no merge. The chain root cause remains open.
Installed candidate: `vr33-hands-working-549-gbf5733638-dirty`, DLL SHA256
`69405023822796d766ee637019ef25f88c4a2c807f90e5d97abaf97db3c8eff8`.
Prior DLL/INI/both logs archived in
`build/playtest-candidates/installs/20260920-194126`. Full INI comparison shows
only GameOptsWrite cleared and CamModProbe/SwingTrace explicitly set to 1;
CRLF verified. Final parser-correction install archived at
`build/playtest-candidates/installs/20260920-194338`; its full INI is byte-identical.
No new playtest banner exists yet.
The old ModifierList probe does not inspect Dishonored camera influences.
New script-lane `camera/source` snapshots read m_InfluenceGroups, influence
weights/targets and exposed source vectors, pawn EyeHeight/BaseEyeHeight,
velocity and POV-minus-pawn. Samples include healthy states, carry object
identity, report unavailable values, and make no frequency or cause claim.
The invalid radius estimate was removed.

Option review fixes: enumerate all categories and subcategories, derive x86
struct extents from reflected final fields including bool storage, accept a
successfully resolved offset zero. Native setter/PSI equivalence remains
unverified, so no OnSettingChange, OnApplyVideoSettings or SaveProfile call.
Raw writes require a refreshed live-object table, validated unique owner/id
records, owner 2/type 1 and an approved id/value. Head bob floats are displayed
as floats and refused by the integer writer. Fullscreen/vsync are refused.
Empty console replies are explicitly unavailable, never renderer evidence.

Validation: Release build, lint, nine exports, and 17 host checks of the
production validator/writer passed. Game was not launched by the agent.
Next test: stand still and pitch normally, reproduce X-release from a chain,
stand still and pitch again, then Blink and repeat. One question: which source
field changes with the enlarged eye offset and returns after Blink? A source
change names the next native writer to inspect; unchanged sources leave the
fault downstream; no reproduction or unresolved fields is inconclusive.
See [camera evidence](dishonored/FLICKER_REFERENCE.md) for the corrected
elimination and [engine notes](dishonored/ENGINE_NOTES.md) for layout reasoning.

## Session handoff 2026-09-19 (late): SteamVR, the judder, and two retractions

### Where things are RIGHT NOW

- `VR-Main` = `50e35be44`. PRs #75, #76, #77 and #79 merged today with permission.
- **Working branch: `claude/vr-154-steamvr-origin-only`** (`c6942c577`) = VR-Main
  + the VR-154 origin fix + `tools/vr-runtime.ps1` + its packaging. NOT merged.
- Installed on the dev PC: `d3d9.dll` SHA256 `0E759C8B...`, built from `548c31693`
  on that branch. Its INI has `Runtime=steamvr`, `XrRuntimeJson` empty.
- The tester holds a zip with that EXACT dll (byte-identical, not a rebuild - the
  banner embeds __DATE__/__TIME__ so no rebuild can match), an INI defaulting to
  `Runtime=steamvr`, and `vr-runtime.ps1`.
- `claude/vr-152-pair-rate` (`6bf9dd6fc`) holds three commits NOT on VR-Main and
  NOT in the zip. One of them is a real fix (see VR-153 below).

### Fixed and confirmed by the tester today

VR-147 Blink latch (drop + re-latch in 31 ms against 23.8 s), VR-148 awareness
markers (2996 published, 0 refused, 5238 draws matched), VR-143 the crouch
stand-up stall - one early return had parked the whole discovery block, and the
stand-up probe measured 3975 ms of script lane in a 4000 ms window.

### TWO THINGS I GOT WRONG - read these before trusting the record

1. **"SteamVR's LOCAL origin is on the floor, so the jump is a standing height."**
   Withdrawn. A guess about where SteamVR puts LOCAL, and it reasoned from a
   standing wearer who is SEATED. The VR-154 mechanism does not depend on it and
   still holds; the magnitude and direction are not claimed.
2. **"Use the stereo method's pass eye instead of inferring it."** Proposed,
   built, and reverted the same hour. It is already in the graveyard, falsified:
   `55_game_dishonored_hands_mesh_split.inc` records 0 of 83,400 corrected draws
   finding a doubled pass, because the passes run on the game thread and the
   draws on the render thread. The lever exists as `[Hands] PaletteEyeFromPass`,
   default off and inert, kept as the proof. **Grep the graveyard before
   proposing an approach.** The hypothesis sits 20 lines above its own refutation
   and reads like a plan if you stop early.

### Open, in the order I would take them

1. **VR-154, unverified.** One SteamVR run with a headset recenter settles it AND
   VR-146. Read `postrack: origin moved` (did it fire) and the two
   `postrack: reference taken` lines either side of the recenter - they MEASURE
   the origin shift instead of assuming it. The same line prints head roll:
   near 0 = upright in a gravity-aligned space (seated reads the same), near
   +/-180 = the LOCAL space is inverted, which is VR-146's answer.
   VR-146 is also reframed: a recenter fixing the orientation rules OUT an
   inverted image, because a flipped texture does not care about a recenter. Do
   not revive the whole-image flip.
2. **VR-152 judder, REOPENED.** It was auto-closed by #79's `Fixes` line and the
   judder is not fixed. What landed: the `pcap/layout` cap leak (77992 log lines
   per run -> 15, confirmed) and the awareness census mutex. The prediction that
   the tick would return toward 9.4 ms was REFUTED - it went 10.70 -> 11.11 ->
   13.00 across later runs. Watch `stereo: beat ... L/s`, not the tick mean:
   median pairs/s went 109 (build 512) -> 45 (build 522), a 59% loss the tick
   mean showed as 13%. TRAPS.md carries that reading error.
   Wrong-eye draws per 1000 classified across 51 archived builds: settled era
   0.000-0.039, build 512 0.087, 527 0.862 - the worst since 385. BUT 489 already
   hit 0.433 with a 249k sample and none of my code, so the baseline is wobbly
   and a step change is NOT established. `SAME%` is the strongest correlate
   (+0.39); session length is not (+0.09).
   The tester has a Puppis router and a wired cable arriving, so the next run is
   the first clean-network baseline. Get that before attributing anything.
3. **VR-153 death/respawn, FIXED BUT UNSHIPPED.** The stuck-menu rescue could
   never fire - `skcInGameplay && (g_menuOpen || g_inMenu)` with skcInGameplay
   defined as `!g_menuOpen && !g_inMenu` is `(!A && !B) && (A || B)`, false in
   every state, and `menu: flag cleared` appears zero times in a 95 MB log. The
   death screen fires `Req_CanLoadGame`, nothing closes it, and for 77 seconds
   the runtime sat on the mono screen (the "small square") with the right stick
   passed through as menu navigation. Fix is on `claude/vr-152-pair-rate`
   (`c41539bfb`). **Ask before cherry-picking** - the user asked for the SteamVR
   fix only. F9 forces gameplay mode meanwhile.
4. VR-149 bone charms: still unproven, no bone charm has been revealed by the
   Heart in any run. `hud/heart-symbol` names the symbol when one is.
5. VR-151 HOW-TO-USE is stale (says stereo and motion controls "are being
   rebuilt", and tells the reader to set resolution in the game's video options,
   which is inert). The fix was written and the PR CLOSED at the user's request -
   do not reopen it unasked. The zip the tester holds has the stale text.
6. VR-150 `camera-clamp-host.ps1` does not compile (its Writer regex predates
   `g_viewScope`). Pre-existing, from `5dee90153`.

### Process notes worth keeping

- `[VR] XrRuntimeJson` sets `XR_RUNTIME_JSON`, which the OpenXR loader reads
  BEFORE the system default. A path left there silently beats whatever is picked
  in SteamVR or Virtual Desktop. That ate an evening; `tools/vr-runtime.ps1`
  exists so it cannot happen again.
- The log rotates ONE deep. A SteamVR run was lost to it today.
- `tools/install-candidate.ps1` is the tester's one-button install: refuses while
  the game is running, archives the previous DLL/INI/both logs first, leaves the
  INI alone.

## ONE cause behind both the Blink head-aim and the stand-up stall (2026-09-19)

Run 516 measured it. The stand-up probe reported, for the first stand after a
load: 40 x 100 ms, 7 presents in the whole window (571 ms mean), and script lane
3975 ms of 4000 = 99% of wall. The stall is OURS, and the window names it.

The cause is one early return. ApplyHandToMeshInner holds the 30.95 block - the
SkelControl probe, the Blink latch, BlinkHookTick, BlinkDestTick, BlinkTraceTick,
CrouchStateTick - and 30.95 deliberately hoisted that block above every early
return INSIDE Inner. But Inner is the LAST call in ApplyHandToMesh, below the
crawl tuck's `if (t) return;`. So a crouch parked the entire discovery.

That single fact explains both reports:

- Load a save while crouched and the tuck holds from the load until you stand.
  No probe runs, no Blink latches, so Blink uses the engine's own head vector.
  Switching power and back "fixed" it because anything that released the tuck
  let the discovery run. The tester's own sequence, exactly.
- Standing releases the tuck and every deferred step fires in one burst on the
  game thread: the 115054-object SkelControl probe and its property walk
  (296 ms for the walk alone), the graft, the weapon-attach derivation, the
  draw-capture re-arm. Measured in run 516 at t=8176000..8180000, with
  `script: NotifyTakeHit` in the same millisecond - being knocked out of crouch
  does it too, which the tester reported and which confirms the trigger is the
  RELEASE, not the input.

Fixed: the tuck now runs the discovery and skips only the calibration request
and the drive writes, which is all it ever meant. The fault guard moved above
the tuck so the discovery stays inside the recovery the walk has always had,
and every path out of there clears g_walkTid. The work now spreads over the
crouch instead of being saved up for the moment the player stands.

NOT fixed, same class, named in the code: the `AnimReleaseControls()` and
`!g_handMesh` returns still sit above the guard and the discovery.

Also cleared this session: VR-143's texture-streaming and paging suspects, by
reading run 514's comparable load stall (0.0 MB uploaded and created, VRAM flat).
PERFORMANCE.md carries it.

Build 518 installed via the new tools\install-candidate.ps1. Release, lint,
exports, 908 hud-anchor, 107 native HUD, 20 crawl-strength and 138 animation
catalog checks pass. No merge.

Next: one run. Load a crouched save and stand - `standup:` should now show the
lane spread thin instead of 99% of wall, and Blink should be on controller aim
from the load without touching the power wheel.

## Blink silence, the stand-up stall probe, one-click install (2026-09-19)

Still on claude/hud-improvements-pt-2 (PR #77). Build 514 was played; three
results and one new fault.

CONFIRMED FROM THE TESTER'S LOG. The VR-147 Blink latch fix works: the drop fired
at the save load (pawn 185EA000 -> 185E3C00) and re-latched 31 ms later, against
the 23.8 s the same transition cost in run 512. VR-148 awareness markers install
and match: 2996 published, 0 refused, 5238 draws matched, 4 ambiguous, and the
widest accepted draw was 61x62 authoring px, comfortably inside the 160x160
bound - which is the measurement that would tighten it. VR-149 has no result yet:
no bone charm was revealed by the Heart in that run, so `hud/heart-symbol` has
nothing to say.

NEW FAULT, and the expensive one. A later run had Blink back on head aim and the
log carried NO `[blink]` lines at all. The latch logs its successes and says
nothing about a sweep that finds nothing, and the aim hooks only install once the
latch exists, so a dead Blink hook and a healthy one produce identical text. Two
states hide behind that silence and want opposite responses: the power not
existing in the level yet (an early save, not a fault) versus the object existing
and the liveness walk refusing it (a fault). Config was diffed between the working
and broken runs and is byte-identical, so it is not a setting. The fruitless sweep
now warns with the population it examined. TRAPS.md carries the class.

VR-143, the stand-up stall: texture streaming and paging are CLEARED, not by a
new run but by reading run 514's comparable load stall - 2437.6 ms of 2440.1 in
out/idle waiting for the game thread, with device/stream at 0.0 MB uploaded and
created and VRAM flat. What remains unmeasured is that `out` means "not our
present hooks", not "not the mod": every mod tick on the game thread runs inside
ProcessEvent and lands in the same bucket. The new probe captures the first
stand-up after a load at 100 ms resolution and times that lane. Not yet run.

`tools\install-candidate.ps1` is the tester's one-button install: it refuses
while the game is running, archives the previous DLL, INI and both logs first,
leaves the INI alone, and prints the installed hash and the settings that matter.

FOUND AND NOT FIXED: `tools\camera-clamp-host.ps1` no longer compiles. Its regex
takes `^struct Writer \{.*?^\};` but the production declaration ends `} g_viewScope;`,
and `write_offset` has referenced `g_viewScope` and `scoped()` since 5dee90153.
Pre-existing on this branch, not from this work. Filed as VR-150.

Next: one run. Blink somewhere Blink exists, and read `blink:` - either a latch
line or the new fruitless-sweep warning with its counts. Reveal a bone charm with
the Heart for `hud/heart-symbol`. Load a crouched save and stand up for
`standup:`. Then tighten the awareness window against the 61x62 measurement.

## HUD improvements pt 2: blink latch, bone charms, awareness meters (2026-09-19)

Branch claude/hud-improvements-pt-2, off codex/hud-improvements after that branch
got its PR (#76, out of draft). PR #76 now carries one extra commit that makes the
2026-09-19 run's own F10 tuning the shipped defaults, including the return to
2750x2850; the runtime selection is deliberately not baked.

Three faults, all read out of the run 512 log rather than guessed.

BLINK AIM AFTER A SAVE LOAD (VR-147) - root cause found and fixed, not yet re-run. The log
reads `blinkdir: 943 calls (0 ours) | ray ready 0, refused 0` at t=4027265, 23.8 s
after the player controller changed at t=4004171 and 0.7 s before the PowerBlink
re-latched at t=4027984. BlkAlive tested pointer, index and class identity, all of
which a destroyed UObject keeps until the collector sweeps its GObjects slot, so
BlinkLatch saw a live latch while the game's real Blink was a new object and every
hook call fell through the `self != g_blkObj` guard to the engine's head aim. The
latch is now tied to the pawn it was taken under (PeLatch already notices a new
pawn) and the drop is logged with both pointers. TRAPS.md carries the class.

BONE CHARMS (VR-149) - mechanism confirmed, symbol still unread. Bone charms use the same
Heart marker, update and parent call as runes; only the Flash symbol differs, and
`runeMarker` was the only accepted spelling. Run 512 refused 23787 Heart calls on
that test. The bone charm spelling is not in the exe as ANSI or UTF-16 and the
packages are compressed, so it is NOT guessed: the gate now validates the bounded
shape of any Heart symbol, `hud/heart-symbol` names each distinct symbol a run
sees, and `[Hud] NativeHeartAllSymbols` accepts them all.

ENEMY AWARENESS METERS (VR-148) - first candidate, unverified. Derived the third native
marker family offline (vtable 0x11635c0, update 0xbbd630, parent call 0xbbd784,
constructor 0xbce9a0, which pushes the wide `head_jnt` while its update pushes
fadeIn/visible/quickFadeOut); ENGINE_NOTES carries the full route and the grenade
and DLC families it was separated from. The meters were riding the `default` row's
window panel because no rectangle can claim a marker that moves with its enemy.
The new hook publishes the engine placement and the router leaves matched draws in
the game image. The match window is an explicit BOUND and the log reports what
would tighten it.

Both new levers ship default OFF per the repo rule and are ON in the installed INI
as the trial, which is the same pattern NativeRuneMarkers used. Release build,
lint, exports, the default-profile byte check and 908/107/465/104 HUD host checks
pass. No game or simulator launch. No merge.

Next: one headset run. Read `blink: dropping the PowerBlink latch` after a save
reload, `hud/heart-symbol` with a bone charm revealed by the Heart, and
`hud/awareness-parent` with an alerted guard on screen. Then bake the bone charm
symbol as a measured constant and tighten the awareness match window.

## Selective cleanup for SteamVR continuation (2026-09-19)

Latest user instruction supersedes the exact486-only handoff: remove ONLY failed
physical choke and split/attached/model health-mana experiments. Keep possession,
rain/lens, wheel blackout, crash/stability, weapon models/animations, reticle UI
and defaults, and SteamVR diagnostics. Original combined vitals remains.

Production code is c01058558 plus305f1d3dc reticle defaults and the two source
diffs from e30554204 SteamVR diagnostics. Release profile preserves all unrelated
current settings; only Choke section and VitalsMode/VitalsDebug are removed.
Research/code remains in codex/archive-hud-choke-20260919 and tracked archive.
Do not restore or revive the experiments. No branch history rewritten.

Installed vr33-hands-working-512-g3f4d323e0 from clean source commit 3f4d323e0.
Release build, lint, 908 HUD checks and 9 export checks passed. Installed DLL
and INI hashes independently match installed.json; the entire accepted486 INI
is byte-identical, with CRLF and explicit VDXR selection preserved. Prior DLL,
INI and both logs archived under build/playtest-candidates/installs/
20260919-125236-071385. Build512 has not been headset-tested.
SteamVR inversion remains open; retained diagnostics do not claim it fixed.
No game or simulator launch, no PR/merge. Continue on codex/hud-improvements.

## Claude handoff: accepted486 PR, SteamVR extraction, deferred HUD/choke (2026-09-19)

The user accepts the current486 state and requests PR/branch separation with
health/mana splitting and choke removed from active delivery but research retained.
This supersedes the earlier implementation plan. Claude is to execute
docs/dishonored/CLAUDE_HANDOFF_BUILD486_BRANCH_SPLIT.md.
PR75 already has identical production content to486; PR76 is the mixed work to
retire only after preservation. SteamVR e30554204 is diagnostics, not a fixed
orientation path. Research snapshot and incomplete calibration patch are tracked
in docs/dishonored/archive/vitals-choke-20260919. Current installed DLL/INI unchanged.
No PR/branch reorganization or push has been performed by this preparation step.

## Post-reboot baseline check (2026-09-19)

Build486 remains installed on VDXR. Reboot plus a known-good save reportedly
returned near prior performance; a small residual difference is unconfirmed.
Actual3012x3122 render dimensions and2688x2880 runtime recommendation match the
accepted pre-split logs. No resolution change made. PERFORMANCE.md records
the evidence,112-percent inference, and combined-test limitation.

## Restored accepted pre-split486 after poor489 run (2026-09-19)

Installed rebuilt3ec56e3bc, banner vr33-hands-working-486-g3ec56e3bc, with the
complete archived install486 INI except explicit nativeVDXR. Frozen rollback-486
installer dry-run and full actual INI diff verified, CRLF preserved.489 was the
immediate pre-split source but486 was the last accepted pre-split playtest.
Performance regression measured, cause unproven; details and comparison limits
in docs/dishonored/PERFORMANCE.md. Next: one launch to compare immediate gameplay
smoothness with the prior accepted baseline. No source branch reset or merge.

## Restored pre-split build489 (2026-09-19)

At tester request, stopped the vitals/choke plan and SteamVR investigation and
restored the installed game to c01058558, immediately before the health/mana
split. Release rebuilt in isolated build/pre-split-source; current branch source
and later commits preserved. Installed banner vr33-hands-working-489-gc01058558,
SHA256 71540df18a6f9dd13f4ef9812db5b333c368782f25c71178ec530d8353d9bae6.
Frozen installer copied from503 in build/playtest-candidates/hud-improvements/rollback-489;
dry run and install verified. Pre490 HUD section restored, Choke section removed,
Runtime=native with explicit 32-bit VDXR manifest. Full INI diff reviewed,
CRLF1253/1253; logs/INI archived before installation. Release, lint and908 HUD
host checks pass. No game launch, no merge. Candidate506 test is superseded.

## Native SteamVR orientation investigation, candidate 506 (2026-09-19)

VR-146: native SteamVR/OpenXR 2.17.10 starts this x86 game without the bundled
shim. The first build503 run used forced Meta compatibility; disabling SteamVR's
openxr.metaUnityPluginCompatibility (2 -> 0) changed the reported runtime to
plain SteamVR/OpenXR, but did not correct the reported inversion. Runtime=native
and SteamVR's steamxr_win32.json are selected explicitly in the installed INI.
The tester reports inverted world and menus with upright hands in gameplay.

The verified build503 log reports head roll near +/-179 degrees. This is a lead,
not proof of bad tracking: the old log does not establish whether the headset was
being worn at each sample. Candidate506 adds a native-SteamVR-only audit comparing
VIEW-space head and both eye quaternions, norms, validity, session state, derived
camera roll and angular disagreement at one predicted timestamp. It samples
once per three seconds for up to 120 samples. No orientation correction yet.
The whole-image flip experiment was compiled and host-tested but never installed;
it was removed from production source after the upright-hands clarification.
Recoverable scratch source is under build/steamvr-image-flip-uninstalled.

Installed candidate: build/playtest-candidates/hud-improvements/install-506,
banner vr33-hands-working-506-ga1e15d41e-dirty,
DLL SHA256 8fcb8458070fadc6fdfa9ed5b6a5b17139c76e4e09c727cd3fd392ce1646625a.
Installer copied from install-503, dry-run on a copy of the live INI, then run.
Full INI comparison: no changes, CRLF 1283/1283. Both old logs and INI archived
under before-install-20260919-111559, native client log separately archived.
Release, lint, 923 HUD checks, 9 choke checks and byte-identical default profiles
pass. No game/simulator launch, no merge. Headset result pending: hold upright
in main menu then gameplay, compare the reported surfaces with the raw pose audit.

Vitals stage1 is committed as a1e15d41e and inherited by506, but the magenta/bar
question has not been run;505 remains frozen. Game-space capture still waits on
model draw proof. Choke calibration/output and desktop-present A/B remain pending;
uncommitted calibration prototypes are preserved only under build/choke-in-progress
and are not in506. See ENGINE_NOTES.md for SteamVR evidence and next decisions.

## Vitals selector and model diagnostic candidate 505 (2026-09-19)

Implemented the first gated stage of CODEX_PLAN_VITALS_CHOKE.md. Candidate505
is frozen with a copied/hash-checked install.ps1 and a successful live-INI-copy
dry run (only VitalsMode=model and VitalsDebug=1; CRLF preserved).
Static fix: later HUD sinks invalidated the vitals copy. Both-eye draw proof now
protects the XR fallback; refusal counters and independent magenta squares make
the next run falsifiable. HUD_ANCHORS.md has the owner audit and full identity.
Release, lint, HUD/choke/default-profile host tests pass. No launch or merge.
Next: install505 and observe squares/bars; game-space capture waits on that
result. Choke calibration/output and desktop-present A/B remain later stages.

## Run503 and the Codex plan (2026-09-19)

Run503 (build 503, banner verified; logs in build/playtest-candidates/hud-improvements/run503):
the vitals were invisible everywhere. The palm attach switched off the XR quads,
and the in-scene draw never drew, with its refusals unlogged. The physical choke
held RB twice but the game never choked; the pad binding set 2 maps RB to attack.
The tester also cannot switch ReduceDesktopPresent off while the desktop mirror
is off, and many overlapping vitals settings are live at once (VitalsBack is
still on). All of it, with the fix design, is in
docs/dishonored/CODEX_PLAN_VITALS_CHOKE.md for Codex to implement. The next
session starts at docs/dishonored/NEXT_SESSION.md. No new install. No PR
change, no merge.

## Candidate 503: vitals drawn on the hand model, physical choke (2026-09-19)

Candidate 503 = `vr33-hands-working-503-g7159acacf` (DLL 30d2e34d...), frozen with
install.ps1 in build/playtest-candidates/hud-improvements/install-503, NOT
installed (the tester was playing). The installer sets [Hud] VitalsInScene=1 and
[Choke] Gesture=1; a dry run on the live ini added only those, CRLF intact.
- VR-142: the vitals parts are drawn INSIDE the hand's own draw (D3D9 texture
  copies of the vitals sink, the drawn palm, this draw's ViewProjection, a fan
  clipped at the split line). Needs one re-attach (records the palm map's
  mirror flag). HUD_ANCHORS.md top.
- VR-145: the physical choke (right hand quickly to the left shoulder, held
  there, holds RB). CHOKE_GESTURE.md; host tests in tools/choke-gesture-host.ps1.
Open: VR-143 (crouched-load stand-up stall), VR-144 (wheel stutter; the next
A/B is ReduceDesktopPresent).

## Run499: palm attach worse, wheel stutter halved, crouched-load stall (2026-09-19)

Run499 (build 499, banner verified; logs in build/playtest-candidates/hud-improvements/run499).
The drawn-palm attach is worse and does not track animations: a compositor quad
cannot be locked to the hand model (scale 100 vs 108 uu/m, render latency).
HUD_ANCHORS.md top. Proposed: draw the vitals INTO the game frame in the hand
draw. Not built; awaiting the tester's go-ahead. The wheel opens better. The
one-eye ratio went from 47% to 28%, and the next A/B is ReduceDesktopPresent (VR-144).
New VR-143: a ~3 s stall on the first stand-up after loading a crouched save,
outside the mod's present path (PERFORMANCE.md top). No new install.

## Candidate 499: vitals on the drawn palm; wheel stutter measured (2026-09-19)

Run497 (logs in build/playtest-candidates/hud-improvements/run497, previous run
in the .prev log): the panels did not follow the hand model across a stance
change or an animation. The wheel stutters: the share of one-eye ticks in the
wheel is 10% (run470) -> 25% (run486) -> 47% (run497). Candidate 499 =
`vr33-hands-working-499-g1cd485a08` (DLL 1fc2128e...), frozen with install.ps1 in
build/playtest-candidates/hud-improvements/install-499, NOT installed (the tester
was playing): the attach captures against the DRAWN palm (re-attach needed), and
497's per-hand-draw lock is gone. No ini change. HUD_ANCHORS.md top section.
If the wheel ratio stays high, the next A/B is [VR] ReduceDesktopPresent (1 since
run476).

## Candidate 497: attached vitals follow animations, 10 s countdown (2026-09-19)

Candidate 495 is installed (live log banner); the tester accepted the attach
step. Candidate 497 = `vr33-hands-working-497-g3a3643256` (DLL b049184e...),
frozen with install.ps1 in build/playtest-candidates/hud-improvements/install-497,
NOT installed (the tester was playing). The countdown defaults to 10 s, and the
attached panels follow the drawn hand during game animations (the hand draw
publishes its offset from the controller as an XR-space move). No ini change (a
dry run on the live ini changed nothing). HUD_ANCHORS.md top section.

## Candidate 495: attach the vitals by holding the hands to them (2026-09-19)

Candidate 493 was installed by the tester (banner verified in the live log).
Its back-of-hand guess put the panels at the far end of the hand model and the
sliders could not reach. Candidate 495 = `vr33-hands-working-495-g7c1fc3370`
(DLL b270ae83...), frozen with install.ps1 in
build/playtest-candidates/hud-improvements/install-495, NOT installed (the tester
was playing): F10 "Attach to my hands" freezes both panels in front of the head,
counts down, then stores each panel's pose in its hand's grip frame. The
back-of-hand sliders reach +-0.4 m. The installer sets VitalsBack=0 (a dry run
on the live ini changed only that line, CRLF intact). HUD_ANCHORS.md top.

## Run490 result and candidate 493: vitals on the back of the hand (2026-09-19)

Run490 (banner verified; logs in build/playtest-candidates/hud-improvements/run490):
the reticle controls and the health/mana split work in the headset. The left
panel sat oddly (its HandX was set in the same direction as the right's, and the
part textures were not trimmed). The tester's reticle (white, 0.69 degrees) is
now the default. Candidate 493 = `vr33-hands-working-493-g3dc587932` (DLL
8e430f57...), frozen with install.ps1 in
build/playtest-candidates/hud-improvements/install-493, NOT installed (the tester
was playing): VitalsMirror, VitalsAutoCrop, and the new VitalsBack mode (both
panels as watch faces on the back of the hands, moving with them; the installer
turns it on, F10 turns it off). A dry run on run490's ini added only those three
keys, CRLF intact. HUD_ANCHORS.md top section.

## codex/hud-improvements: reticle look and split vitals, candidate 490 (2026-09-19)

New branch off codex/misc-fixes (PR #75, not merged). Candidate 490 =
`vr33-hands-working-490-g91b18ebb5` (DLL 3bbfc88c...), built and frozen in
build/playtest-candidates/hud-improvements/install-490 with install.ps1. NOT
installed yet: the tester was playing. The installer archives both logs and the
ini, copies the DLL, sets `[Hud] VitalsSplit=1` and mirrors HandR onto HandL
(X negated). A dry run on run486's ini changed only those lines, CRLF intact.
- VR-141: the reticle (the controller dot) has size, distance and RGB colour in
  the F10 HUD tab, saved on change; default white (was a fixed red).
- VR-142: the vitals image is cut along a diagonal into a health panel (handR)
  and a mana plus equipped-item panel (handL), with line and crop sliders.
  HUD_ANCHORS.md top sections.
Next: attach both vitals panels to the wrist, like the hand-held notes.

## Run486 result and the branch PR (2026-09-19)

Run486 (banner verified; logs in build/playtest-candidates/wheel-blackout/run486):
the tester accepted the weapon mirror as good enough (pistol straddle 103
triangles, crossbow 8, depth bias 1). No shot or sword swing ran in this log, so
HandAnimMelee/HandAnimFire are still unverified in the headset. codex/misc-fixes
is opened as a PR to VR-Main (not merged). The HUD work continues on
codex/hud-improvements.

## Installed486: swing/shot hand animation, barrel gaps, copy depth bias (2026-09-19)

Run483 (banner verified; logs in build/playtest-candidates/wheel-blackout/run483):
pistol mostly filled (kept 1684) with two gaps on the barrel; crossbow decent,
but its top left flickers against the model and a few left-side areas are still
missing. Installed486 = `vr33-hands-working-486-g3ec56e3bc` (DLL 58078be3...):
- Mirror: plane-crossing faces mostly on the modelled side are mirrored
  (`[Mirror] Straddle=2.0`), and the copy is depth-biased behind real surfaces
  (`[Mirror] DepthBias`, code 0, installed 1; `mirror bias <n>` live).
  WEAPON_MIRROR_PLAN 6e.
- Animation: sword swing (StatePlayerMeleeAttack) and shot (any *fire* clip in
  StatePlayerAction; run483 measured Pistol_Fire) can play the game animation on
  the tracked hands with the arms hidden, like mantling. F10 Animations tab;
  `[Anim] HandAnimMelee/HandAnimFire`, code 0, installed 1. ANIM-HANDOFF-PLAN end.
Installed ini: the three keys above added (CRLF verified). No PR, no merge.

## Installed483: rain defaults saved, pistol coverage tightened (2026-09-19)

Run480 (banner verified; logs in build/playtest-candidates/wheel-blackout/run480):
crossbow much better, a few areas still missing; the pistol misses more than 476;
no flicker. An accidental F11 toggled the engine's fullscreen twice (Reset to
1355x1405 and back to 3012x3122 within ~1 s): FOV stayed 108.06, the stereo beat
recovered to 103/103, and the mod ini was unchanged, so no harm. Installed483 =
`vr33-hands-working-483-g0d5325169` (DLL 4ec3a876...): rain lens defaults = run480's
final state (distance 18, keep-size off, follow-head on, rain box shown), and a
mirror copy counts as already modelled only within 0.3 uu of a same-facing
triangle (`[Mirror] CoverTol`; WEAPON_MIRROR_PLAN 6d). Installed ini: Lens
Distance 2->18, FollowHead 0->1 (CRLF verified). No PR, no merge.

## Installed480: blackout fixed, rain back to 470, mirror by facing (2026-09-19)

Run476 (banner verified; logs in build/playtest-candidates/wheel-blackout/run476):
VR-140 FIXED, headset-confirmed. No blackout across 22 wheel opens; `pp/repair`
fired 46 times, all on a NaN Cooling timer. Rain: the tester preferred
installed470's look, so the defaults are back to 470's end state (lens 2 uu,
keep-size off, rain box shown, follow-head off). The FollowHead and RainStrength
levers stay available, both off by default.
Mirror: the crossbow's left side was invisible because SymmetricSkip (476)
refused it; its vertices are symmetric but its faces are not. Installed480 =
`vr33-hands-working-480-ge7493c9f2` (DLL 124adab4...): the modelled side is
picked by outward-facing area, a copy is skipped only over same-facing surface,
SymmetricSkip is removed, and hole caps stay on (WEAPON_MIRROR_PLAN 6c).
Installed ini: Rain Hide 1->0, Lens Distance 1->2, FollowHead 1->0 (CRLF
verified). No PR, no merge.

## Installed476: blackout cause measured and repaired, rain lens follows the head, hole caps (2026-09-19)

Run473 (banner verified; logs in build/playtest-candidates/wheel-blackout/run473):
VR-140 CAUSE MEASURED - a wheel close during the UberUI fade-in turned the game's
`m_UIStateDuration` into NaN; the effect sat in Cooling forever and the world
post-processed to black. Camera fade and colour scale were clean (FLICKER_REFERENCE
VR-140 items 11-12, ENGINE_NOTES top). Installed476 =
`vr33-hands-working-476-g5735d27b8` (DLL c86dfafe...):
- VR-140: `pp/repair` rewrites the timer only when it is already non-finite.
- VR-137 rain: the tester's settings are the defaults (rain box hidden, lens at
  1 uu, no rescale); the lens effects are re-placed per eye from the rendered
  camera (`[Lens] FollowHead=1`); `[Lens] RainStrength` (F10, 100 = native) caps
  the looping lens effect's fade weight.
- VR-138: open holes in the weapon meshes are capped (`[Mirror] Caps=1`); a model
  already symmetric (crossbow x 0.976) gets no copy (`SymmetricSkip=0.90`), which
  should end the right-side flicker; BackFaces off.
Installed ini: Rain Hide 0->1, a [Lens] section, Mirror BackFaces 1->0 (CRLF
verified). `release/dishonored_vr.ini` was already stale (no Rain/Lens/Mirror);
untouched. No PR, no merge.

## Session end: installed470 results, handoff (2026-09-18)

Installed470 (`vr33-hands-working-470-gc4becc905`, DLL 572c930c...) run by the
tester; logs archived in build/playtest-candidates/wheel-blackout/run470.
- VR-139 hitch: SOLVED outside the mod (VD network; router restart + H.264+).
- VR-140 black world after wheel flicks: reproduced. The UI-blur restore
  hypothesis is RETRACTED (weight read 0, watchdog silent). New lead: black
  starts ~400 ms after the game's own wheel movie finally closes following
  mid-close re-opens; black at the backbuffer. FLICKER_REFERENCE VR-140 item 7-8.
- VR-138 mirror: pistol nearly filled (kept 1248/2772), a little missing near the
  handle; crossbow bottom-left still missing (kept 50; its plane comes from the
  symmetric top). Back-face pass armed; no visible help reported.
- VR-136/137 rain: the pane is `DisEmitterCameraLensEffect_Looping` (a lens
  effect), moved to the eyes by the LENS distance slider; tester wants it scaled
  down. Rain-box distance and hide work on a different effect.
- VR-135 possession: accepted (rat); widened to all classes, people untested.
Installed ini: 464's plus `[Mirror] BackFaces=1`. No PR, no merge.

## Installed470: wheel blackout fix, fuller mirror, back faces (2026-09-18)

Run467 (banner verified, logs in build/playtest-candidates/hitch-instrument/run467):
the periodic hitch was the Virtual Desktop network (tester: VD network spiked
every ~5 s; a router restart plus H.264+ made it smooth at 100-110 fps; gpumem
read VRAM 15% of budget, nothing paged - paging killed). Pistol mirrored but
incomplete; crossbow symmetric (47 triangles), so its gaps are one-sided faces.
The world went black after a 62 ms wheel re-open (VR-140): the menu-blur exit
restore is the inferred cause. Installed470 = `vr33-hands-working-470-gc4becc905`
(DLL 572c930c...): no restore write on menu exit plus a gameplay watchdog;
looser mirror fill; `[Mirror] BackFaces=1` added to the installed ini (only
change, CRLF). No PR, no merge.

## Installed467: periodic hitch instrumented, mirror plane by symmetry (2026-09-18)

Run464 (banner verified; logs in build/playtest-candidates/weapon-mirror/run464):
the weapon mirror REFUSED both weapons (no cut face), so nothing changed on
screen. The periodic drop is measured: ~90 ms stalls inside the runtime's
xrEndFrame on a ~4 s beat, the same rate walking or standing (12.6 vs 14.3/min)
and in the pause menu; game GPU time normal; not the streaming change (the
pre-change 452 run has it); rate grew from ~2-6 to 10-30/min across builds
353..464. Full record: PERFORMANCE.md top section. Installed467 =
`vr33-hands-working-467-g8f79caeae` (DLL e939e54b...) adds read-only `gpumem`
and `device/stream` lines at every frame gap (paging vs streaming) and finds
the mirror plane by symmetry. Installed ini unchanged (238d9d68...). No
watcher armed. No PR, no merge.

## Installed464: weapon mirror built and armed (2026-09-18)

Installed464 = `vr33-hands-working-464-g0b7171bd1`, DLL affccb00..., symbols by
hash. VR-138 implemented per WEAPON_MIRROR_PLAN.md (deviations recorded at its
top): pistol and crossbow get a second draw, reflected in reference-pose space
on the palette, cull flipped, through our index buffer of missing triangles
only; plane measured from the cut face or `[Mirror] Plane_<asset>`. frame_test
pins the math. Installed ini: only `[Mirror] Enabled=1` added (238d9d68...,
CRLF). No run since 462; its logs were already archived. Unverified in game.
Launch question: see NEXT_SESSION (mirror first; the rain question waits).

## Installed462: all possessables, near-eye rain and lens levers, mirror plan (2026-09-18)

Tester confirmed possession stereo on 458 (rat). Installed462 =
`vr33-hands-working-462-gdd19939ac`, DLL b212d154..., symbols archived by hash,
458 logs/ini/crash archived in build/playtest-candidates/near-eye-effects.
Installed ini unchanged (a55a9106...); new keys run on code defaults (native).
Streaming config unchanged. Memory watcher was stopped by the tester; not re-armed.

- VR-135: possession now validates every DisPossessablePawn class (DLC06/07
  pawns were missing) through the engine's SuperField chain, trusted only after
  it reproduces the player pawn's ancestry; full class list as fallback.
- VR-136: the rain pane is DERIVED: the camera re-places its rain emitter each
  frame at the view ray's exit from `m_RainBoxExtent` (500 uu), a slab ~5 m
  ahead that turns with the head (143 steady samples: fwd 499..662, right ~0).
  `[Rain] Distance` / F10 slider / `raindistance` writes the extent (0 = on the
  head). Hide lever unchanged.
- VR-137: low-health vignette = `m_pCurHealthLensEffect`, an
  EmitterCameraLensEffectBase at `DistFromCamera` 90 uu. `lens/fx` logs each
  effect's measured position; `[Lens] Distance` + `KeepSize` move it nearer.
- VR-138: weapon mirroring planned in docs/dishonored/WEAPON_MIRROR_PLAN.md
  (reference-pose reflection on the palette, own index buffer of missing
  triangles only, measured cut plane). Not implemented.

Launch question: in the rain, does F10 "Rain distance" at 0 turn the pane into
rain around you? See NEXT_SESSION. No game launched by Claude, no PR, no merge.

## Installed458: possession stereo armed, rain measured (2026-09-18)

Continue codex/misc-fixes. Installed458 = `vr33-hands-working-458-ge5246ba2f`,
DLL 8aef77f4..., symbols in build/symbol-archive/<DLL hash>. Manifest and the
452 logs/ini archived pre-install in build/playtest-candidates/possession-rain.
Installed ini: only `[Cine] PossessionStereo=1` and `[Rain] Hide=0 Trace=1`
added (whole-file diff, CRLF verified). Streaming config untouched (13 x -1 in
both game INIs). Memory watcher armed (3000 MB, signed x86 procdump) at
D:/dvr-data/support-watch/20260918-082952-438; it is tied to the Claude session
that started it, so verify it is still running before relying on a dump.

VR-135 (possession mono): cause MEASURED in the 452 log. At the pawn switch the
capsule liveness refused `DisPossessionProxyPawn` (by design), so stereo/state
fell to FALLBACK pawn=0 valid=0 and the re-entry drew once for the whole
possession while the view and camera uploads stayed live. Commit 4b0fdb3f8 adds
a read-only validated-possession term (pawn back-pointers name our live
controller and a live player pawn) consumed ONLY by the presentation verdict;
menu/UI/view/upload terms and every gameplay guard unchanged. Unverified.

VR-136 (rain pane): commit e5246ba2f logs the camera rain box (`rain/box`:
extent, drops, emitter position in the camera frame) and adds a targeted hide of
only that emitter via native PrimitiveComponent.SetHidden, shipped OFF. Near-eye
placement waits on the rain/box numbers from a rainy run. Unverified.

Result of the 08:33 run (tester launched; watcher dump is a normal-exit
termination dump, peak private 2497.5 MiB / virtual 3318.4 MiB): both rat
possessions VALIDATED and presented stereo (beats L/s=R/s, mono/s=0); about
0.6 s mono at each entry while the engine's own camera uploads paused. Headset
verdict still wanted. The three caught startup exceptions match 452's exactly
(same two functions, shifted +0x8f0 by the new code): pre-existing, not new.
Rain: extent 500 uu cube, 40 drops. (The "74..118 m, not the drop box" reading
was retracted the same day: that was only the first sample per level; steady
samples put the emitter 5..6.6 m straight ahead. See the section above.)

## Claude takeover: accepted streaming run, rain and possession pending (2026-09-18)

Continue codex/misc-fixes. Installed452 unchanged; latest DLL/banner verified,
both logs/current profile archived under build/playtest-candidates/claude-handoff452.
User reports no crash.9m46s memory capture: peak private2056.9MiB, virtual2819.3MiB;
normal exit0. Prior freeze snapshot private3164.5MiB, virtual3930.7MiB. This supports
memory optimization, not a controlled FPS result or universal crash-free claim.
Streaming config accepted; preserve13 group overrides to-1 in both game INIs.
Watcher completed and is NOT armed. Exit dump is not crash evidence.

User requests near-eye rain or targeted disable, plus stereo during possession.
Decompiled possession power stage3 and camera state2 provide candidate explicit
signals; current player FSM reader intentionally excludes possessed nonplayer
pawns. Neither feature has been patched. Health vignette remains invisible and
unresolved. See rewritten docs/dishonored/NEXT_SESSION.md for bounded next actions,
source files, exact evidence, and all session rules. User now requires new commits
credited to configured GitHub identity, overriding older neutral-author policy.
No game launched, new DLL installed, PR opened or merge performed this turn.

## Build452 memory investigation and streaming test (2026-09-18)

Saved dump analyzed with exact452 symbols:2596 live CPU texture twins,
10200 created minus7604 released, zero twin allocation failures in this run.
The old shadowBytes counter is cumulative, not live memory.2580 2D twins
describe1754.64MiB of pixel payload including mip chains;16 cubes excluded.
86 textures with4096 maximum dimension account for638.67MiB;351 at2048
account for752.46MiB. Payload estimate excludes driver overhead/alignment.
Detailed derivation and limits are in ENGINE_NOTES below its new investigation.

Both current game INIs set NumStreamedMips=0 for13 SystemSettings groups.
Dump confirms large texture objects have full resident/requested mip chains.
This is strong retained-workload evidence, not proof all crashes are fixed or
that no other leak exists. Do not attribute who installed those settings.

User authorized closing the newly running game. Closed before config edits.
Prepared/applied reversible test: only those13 entries per file changed to-1.
Both full INIs backed up/diffed, CRLF verified, mod INI unchanged. Build452 retained.
Evidence/config manifest: build/playtest-candidates/texture-streaming452.
Both logs/current profile archived in its preinstall support ZIP.
Texture pack,4096 limits, pool160, headset resolution and F10 values unchanged.
32-bit memory watcher PID28440 armed at3000MB, one dump on threshold/exception/
exit; no game launched. Verify watcher output before next session's launch.

One launch question: does the previously failing play/pause sequence complete
without freezing with streaming restored? Expected: lower retained memory and
normal pause. Freeze with lower memory weakens this mitigation; renewed memory
exhaustion means streaming is insufficient. Stability in one run is not a release
guarantee. Vignette placement remains unresolved and was not changed in this test.

## Build452 pause hang: live dump proves engine memory fatal (2026-09-18)

Tester reports hit-camera behavior correct in this run; health vignette invisible
after frame routing, so the effect placement change is not accepted. Rain unchanged.
Build452 log banner and installed DLL hash verified. No binary or INI changed.

Captured still-live PID27044 with full ProcDump (3605MB), a later64-bit mini
snapshot and a32-bit mini snapshot. Local dumps are D:/dvr-data/dumps/
pause-freeze452-27044*.dmp. Logs/current profile and hashes preserved in
build/playtest-candidates/pause-freeze452/support-20260918-072855-650.zip.
Exact452 symbols already archived by DLL hash. No uploads or process termination.

Full dump contains the engine fatal buffer indicating virtual-memory exhaustion.
The error text is game-generated; its generic disk-space advice is not a diagnosis.
32-bit mini dump shows main thread25820 in engine fatal cleanup, waiting, while
threads14964 and15576 wait for the allocator critical section owned by25820.
This supports an out-of-memory fatal that hangs in cleanup, not a demonstrated
new pause rendering deadlock. Process private bytes3318243328; virtual bytes
4121595904. Dump memory map below4GiB:165.28MiB free in total, largest21.875MiB.
A free-space snapshot does not identify the failed allocation size or owner.
Unlike443, no final SYSTEMMEM shadow failure is logged; engine allocator fatal
is directly evidenced by the retained message and stacks.

Tool correction: procdump64 captures AMD64/WOW64 contexts; the existing x86
reader produces invalid register values on those and must not be trusted.
The32-bit procdump.exe gives valid x86 registers/frame chains. Watcher now selects
the signed32-bit sibling when handed procdump64.exe; full64-bit dump remains
useful for memory inspection. Capture tested directly on the live failed game.

Next: attribute retained memory/texture twins using exact symbols/full dump,
then choose a measured footprint reduction. No crash-prevention fix is claimed.
Do not repeat the vignette-to-frame approach as a successful placement fix.
Restore a visible dedicated effects surface in a future candidate; do not
silently accept disappearance. Native rain emitter observed with40 drops;
health lens pointer null in final trace is not proof no red HUD draw existed.

## Installed452: released wheel camera recovery (2026-09-17)

Installed vr33-hands-working-452-g1d029a1eb.210 controller checks, release build,
9 exports and diff checks pass. Prior DLL/INI and both logs archived at
installs/20260917-230637-442851. Independent whole-file comparison proves the
only installed INI change is Element.vignette=window to frame; CRLF and installed
DLL/INI hashes verified. All other current F10 values preserved. Rain unchanged.
Symbols archived by DLL hash. No game launched. Test question remains below.
Latest448 log is preserved;450 was installed but not represented by a new log.
No crash-prevention claim and no headset acceptance of452 yet.

## Released wheel camera ownership candidate (2026-09-17)

Latest on-disk run is448 (log ends22:29:16), not installed450. Installed450
DLL SHA256 matches its manifest; no effects/owners telemetry has run yet.
Do not label this as450 playtest evidence. Existing448 log shows530 Walk samples
with Wheel ownership and zero head rotation writes; tracking remains valid.
By contrast2232 Walk/Other samples have writes. At53567609, Walk/UpperIdle,
script menu0, UI-derived menu1, cinematic0, head valid, writes0, script age1091ms.
This establishes stale menu ownership, not proof of the exact reported hit frame.

Fix candidate extends released-wheel handling to the shared UI owner. A focused
VR controller with released grip, no script menu and no cinematic invalidates
the native wheel-open bit after250ms. Remaining menus are still scanned. The
existing wheel visual closing lease remains independent. No new engine writes
or hit-reaction suppression. Native head injection resumes via its existing
absolute pitch and menu-exit yaw path.210 controller policy checks pass.

Tester clarified effects should be retained near the real view boundary.
Installed Element.vignette was window, scale1.620. Candidate routes that
full-screen category to frame (native scene image, not a small window quad).
This broad category also includes full-screen fades; it is not a verified
health-only material ID. World anchor would retain window dimensions, so it
would not address the small border. Rain is preserved unchanged: its native
particle owner must be measured before adjusting distance/extent.
Read-only hit/health/rain diagnostics remain enabled for the candidate.

One launch question: after opening/releasing the wheel and taking hits, does
head pitch remain correct without pausing? Success supports stale UI ownership;
failure with resumed head writes points back to the native hit reaction.
Headset result pending. No game launched.

## Installed450: effect owner diagnostics (2026-09-17)

Installed vr33-hands-working-450-gba2294ed7. Release and9 exports pass.
Existing Cine Trace1 verified active. Complete INI unchanged byte-for-byte;
CRLF and installed hashes independently verified. Both logs/priorDLL/INI archived
at installs/20260917-223627-579719. No game launched. Hit tilt and effect suppression
remain unresolved; candidate only identifies the live native owners.

## Hit-camera, health lens and rain ownership research (2026-09-17)

Build448 stick recovery reported accepted. DLL/banner verified; logs/latestINI
archived under wheel-release/hit-tilt448. New report: hit leaves upward view bias
until pause; low-health border distracting on frame; rain removal requested.
Decompiled declarations identify DishonoredCamera_HitReact (PhysicalReact spring
base), camera.m_pHitReact_Influence, pawn.m_pCurHealthLensEffect and
m_HealthEffects (post-process plus lens emitter), DisTweaks_EmitterCameraLensEffect,
DisSeqAct_SetRainEmitter and camera.m_pRainBoxEmitter/m_NumRainDrops.
Rain drops/impacts have separate particle modules. Lens base sets foreground depth
priority and exposes BaseFOV/DistFromCamera. This is not proof the symptom is a
Scaleform HUD element; moving the generic vignette row could target the wrong draw.

No gameplay hit-react disable: the cheat also affects native strong reactions.
No guessed shader/geometry suppression. Candidate adds bounded read-only logging
of reflected live hit weight/target, health emitter/index and rain emitter/drop count
to the existing Cine Trace. No camera writer or rendering change.
Next launch question: after taking a hit, does the upward bias persist until pause?
Expected evidence is effects/owners plus synchronized existing camera trace; this
is diagnostic, not a claimed fix. Low health/rain target presence can be read from
the same log if present, without another test request.

ProcDump captured a1.23GB full dump on normal exit (code0), not threshold/crash.
One-shot watcher completed; no longer armed. Keep as baseline, not crash evidence.

## Installed448: wheel input recovery candidate (2026-09-17)

Installed vr33-hands-working-448-gf2e1d52b0.202 controller tests, release and9
exports pass. Both logs/prior DLL/full INI archived at
installs/20260917-221854-588188. Entire INI unchanged; CRLF and DLL/INI hashes
independently verified. External memory watcher45180 is waiting, stderr empty.
No game launched. Right-stick recovery after counterattack remains headset-pending.

## Released wheel input recovery and armed memory capture (2026-09-17)

Full memory capture explicitly authorized. External watcher PID recorded in
build/diagnostics/watch-next.pid, waiting via signed ProcDump for user launch;
Memory threshold3300MB, one full dump on threshold/exception/termination.
Initial Windows PowerShell worker failed module autoload; restarted with the
verified available pwsh host. Waiting message confirmed, no game launched.

Build443 counterattack report archived after DLL/banner verification under
wheel-options/counterattack443. At52632281 reflected Wheel ownership opens during
blocking; combat continues after grip release while context6 remains published.
Log explicitly maps raw right input to delivered left axes with right axes zero.
Lean is0. Exact reason the native UI flag persists remains unproven.

Targeted input guard: while physical wheel grip is released, a lone wheel UI bit
cannot select wheel-axis routing or ordinary menu shaping. Script menus/cinematics
still win; presentation state and engine memory are unchanged. Other stale UI
effects remain outside this input fix. Log pad/wheel-release names the refusal.
Next launch question: after counterattacking and releasing the wheel grip, does the
right stick keep turning rather than moving? Failure requires final-axis evidence.
The separately armed memory capture is observational and may briefly pause play.

## Build443 reported freeze repeats allocation failure (2026-09-17)

After longer play, tester reported a freeze instead of the prior crash dialog.
Process was already absent when inspected; no live hang dump was possible.
Installed443 DLL/banner verified, both logs/latest profile preserved under
build/playtest-candidates/wheel-options/freeze443.

Final log at51895218: SYSTEMMEM1024x1024 DXT5 single-level shadow creation fails
8007000e. VirtualFree67.1MiB, largestFree0.9MiB, committed3563.1MiB,
liveTwins3003; physicalAvailable14933.9MiB. Same allocation failure class as
the previous crash, now with a smaller requested texture and smaller free block.
Reported freeze cannot be independently classified as a deadlock without stacks.
Do not treat it as evidence of a new wheel/camera defect. No prevention fix yet.
External full-dump permission remains pending; watcher not armed. No game launched
or settings changed. Priority is capture before exhaustion and identify memory
owners/lifetimes. Full-memory dumps may contain private process data.

## Crash support tooling (2026-09-17)

VR-133: Microsoft-signed ProcDump12.01 downloaded locally under build/diagnostics.
External capture tested on a disposable non-game fixture: valid minidump contains
threads, modules and MemoryInfoList. No game launched or dump captured.
Full game-memory capture was blocked by automatic approval review pending explicit
consent; user question remains pending. Watcher is not armed.

Added tools/collect-support.ps1 and release/Collect VR Support.cmd. Installed the
collector beside the game and added it to package.ps1. Real support ZIP created in
build/support-tests. Tests cover locked log, spaces, custom DataDir, optional missing
files, whitelist, dump exclusion by default and explicit dump inclusion.
Added opt-in tools/watch-crashes.ps1: exact-path/PID attachment, memory CSV, one
dump on memory threshold/exception/termination, then preserve logs. Normal exits
can trigger and must not be labelled crashes. No automatic uploads.
Added archive-symbols.ps1 to packaging; exact443 DLL/PDB retained by DLL SHA256.
See docs/CRASH_SUPPORT.md for player workflow, limitations and crash-prevention plan.
Build443 DLL and installed INI unchanged. Allocation crash is not fixed.

## Build443 texture allocation crash confirmed (2026-09-17)

Wheel options reported working before a rendering-thread Texture LockRect
D3DERR_INVALIDCALL crash. Installed443 DLL hash and log banner verified. Both
logs, latest INI and any existing crash text preserved under
build/playtest-candidates/wheel-options/crash443. Crash text may predate this run;
only verified443 log lines are attributed here.

Final log at50621312 records HRESULT8007000e creating a1920x2048 single-level
SYSTEMMEM shadow texture, format894720068 (DXT5). VirtualFree63.6MiB,
largestFree2.1MiB, committed3572.8MiB, liveTwins2869. System available commit
6974.2MiB and physical available15603.1MiB: evidence points to process address
space exhaustion/fragmentation, not system RAM exhaustion. Failure follows pause
menu entry. shadow_register_texture returns without a twin on allocation failure;
the DEFAULT texture remains, so its later lock cannot use the required SYSTEMMEM
redirect. This matches the screenshot failure and earlier427 allocation signature.

Immediate failure chain established; dominant memory owner, possible leak versus
asset load, and texture-pack contribution remain unmeasured. Do not attribute it
to head rotation or call it fixed by reverting working wheel controls.
No new candidate or setting changes. Next engineering step is measure live shadow
bytes accurately by format/mips and address-space use across load/menu boundaries,
then choose a bounded reduction or allocation-path fix. Do not report failed
texture locks as success or discard shadows that READONLY locks may require.

## Installed443: entry angle controls (2026-09-17)

Installed vr33-hands-working-443-g057805b9a; release,9 exports and2249 wheel
checks pass, including all four entry-angle combinations and mid-gesture stability.
Full INI unchanged byte-for-byte; CRLF and installed hashes independently verified.
Both logs/prior DLL/profile archived in installs/20260917-213358-688802.
Prior log banner did not match441; no441 playtest interpreted. No game launched.
Next launch question: do the two F10 entry toggles independently control opening
tilt and horizontal angle after closing/reopening the wheel? Expected: selected
axes follow entry head orientation, unchecked axes retain upright positional facing.
Failure indicates option persistence or orientation selection needs investigation.

## Wheel entry angle options (2026-09-17)

Added F10 Weapon dial controls for Follow head tilt on opening and Follow horizontal
head angle on opening. Hud WeaponDialEntryTilt/WeaponDialEntryYaw default0.
Both off retains441 hand-origin upright positional facing. Both on restores full
opening head orientation. Individual toggles replace only yaw or pitch/roll.
Visual, distance normal and gesture axes share that frozen orientation.
Options save immediately and take effect on next opening, not mid-gesture.
Build441 has no claimed headset acceptance. No camera or mantle changes.

## Installed441: hand-origin wheel facing (2026-09-17)

Installed vr33-hands-working-441-g3ffbee221. Release,9 exports and2213 wheel
checks pass. Entire INI byte-identical; CRLF and installed hashes verified.
Both logs/prior DLL/INI archived in installs/20260917-213123-337079.
No game launched; headset opening-facing test below remains pending.

## Wheel opening faces eye position (2026-09-17)

Build439 accepted for mantle and upright wheel. Verified installed DLL/banner;
both logs and latest INI archived under mantle-upright/accepted439.
Remaining wheel issue: opening while looking away leaves an awkward yaw.
Keep its hand-origin position and face the opening eye position in the horizontal
plane, rather than inherit head rotation. Freeze orientation afterward.
Visual, gesture axes and distance offset share the same opening normal.
No mantle, camera, blur or settings changes. Existing wheel tests now measure
movement along the actual panel right axis, not world X for an off-center panel.
Next launch: does opening the wheel while looking left/right leave it at the hand,
upright and squarely facing your position? A tilted/edge-on wheel fails facing;
movement after opening fails the frozen anchor.

## Installed439: mantle-only visibility candidate (2026-09-17)

Clean source59cc23a01 installed as vr33-hands-working-439-g59cc23a01.
Release build,9 exports and lint pass in addition to the host checks below.
Both logs/prior DLL/full INI archived at
build/playtest-candidates/installs/20260917-212054-067201.
Complete INI comparison changes only NoBlurWheel0->1; reversing that replacement
reproduces every prior byte. Installed DLL/INI hashes and CRLF independently pass.
No game launched. Mantle visual correctness and exit remain headset-unverified.

## Mantle-only pose/visibility correction and upright wheel (2026-09-17)

Build437 rollback accepted: normal movement restored. Dark Vision test with
NoBlurWheel0 also reported successful; pause with blur suppression was successful.
That does not eliminate a wheel-specific suppression conflict. Restore NoBlurWheel1
at the user's request; Dark Vision remains an open, reproducible-risk hypothesis.

VR-134 now adds native hand pose ownership only for explicit master Mantle when
MantleHandBack is enabled. The mantle Arms checkbox selects full native geometry
versus existing split hand geometry, preserving the native palette/depth once
handoff reaches native. Other master/upper/left states keep build437 pose policy.
No cancellation-eligibility pose trigger, camera edit or engine-memory writer added.
Hidden-mantle geometry policy is frozen with pose weight per render frame and held
through the existing release hysteresis. Existing action cancellation is unchanged.
Other states' Arms controls still choose native versus tracked poses; independent
geometry for those states is unfinished.

Wheel opening uses the existing yaw-only upright capture for both its visual plane
and gesture axes. Opening position, distance offset, crop and later fixed anchoring
are unchanged. Pitch/roll at entry no longer tilt the wheel.

Host checks:138 animation catalog/policy checks plus22 handoff checks,2204 wheel
checks,908 HUD anchor checks pass. These establish policy/math, not visual comfort.
Next launch question: with Mantling enabled and Show game arms unchecked, does a
mantle retain animated hands with forearms hidden and return to normal tracking?
Tracked/frozen hands or full forearms fail the separation; a bad exit fails release.
Do not interpret these checks as headset acceptance or a fix to Dark Vision.

## Dark Vision plus weapon wheel blackout isolation (2026-09-17)

Build437 rollback headset-confirmed normal for movement. New report: Dark Vision
works until opening the weapon wheel; world turns black while highlighted people
remain visible. Menu still appears to affect color/bloom despite blur suppression.
Verified437 DLL and log banner; logs/profile archived under
build/playtest-candidates/darkvision-menu/reported437.

MenuEffectsTick suppresses only reflected DisPostProcessManager.m_UIPPWeight.
Log confirms suppression during wheel context6. This is not proof that every UI
post-process effect is disabled, nor that this write causes the blackout.
Surface is the world scene with surviving Dark Vision silhouettes, not an
established eye-pair synchronization fault.

Prepared config-only A/B on the same437 DLL: NoBlurWheel1->0, every other INI byte
preserved, full diff and CRLF verified. Both logs/prior DLL/INI archived under
build/playtest-candidates/installs/20260917-211007-808112.
Question: with Dark Vision active, does opening the wheel still black out the world?
Expected if suppression conflicts: native menu background returns and scenery stays
visible. If black persists, suppression alone is insufficient; inspect native menu
post-process composition and head-look rendering separately. No headset result yet.
No code fix claimed and no game launched. Other menus and repo defaults unchanged.

## Installed437: recovery rollback to build433 (2026-09-17)

Installed vr33-hands-working-437-g0d415c741 on codex/misc-fixes.
All production source matches build433 commit5dee90153 exactly. Release build,
9 export checks,96 catalog/policy checks and22 handoff checks pass.
Both logs, prior DLL and full INI archived at
build/playtest-candidates/installs/20260917-210002-856329.
Independent installed DLL/INI hash checks pass. Entire installed INI is
byte-identical to the previous one, including latest F10 values; CRLF verified.
No game or simulator launched. Headset recovery test pending.

One launch question: are normal crouching, jumping and looking around restored,
including the same window exit? Success supports the build435 regression being
removed. Remaining disruption requires investigating the earlier path or session
state. Independent arm visibility is still unfinished; do not test that here.

## Build435 rejected: restore build433 pose policy (2026-09-17)

Tester reports unwanted crouch animation, native animation close to the face and
loss of normal control/view after jumping through a window. Installed435 DLL hash
and banner verified; both logs and latestINI archived in
build/playtest-candidates/animation-visible-hands/reported435. Run ends in normal
PreExit; this is not evidence of a crash.

Confirmed design error: native_pose_requested used cancellable_action as a native
pose trigger. Generic upper/left StatePlayerAction also covers movement transitions,
not only deliberate item interactions. Logs show repeated GAME ownership during
Jump/Falling/Walk with JumpIn/JumpLandSmall sequence history and native split-hands
reason, despite saved Jump/Falling/Walk arms being0. Sequence history is supporting
context, not authoritative playback identity. This broadens hand ownership beyond
the requested mantle fix. Exact close-face and view-disruption causes remain open.

Revert all435 production changes and their policy tests to exact433 source: original
arm-driven classifier, draw bypasses, mesh palette/depth path and F10 wording.
Keep433 action-cancellation controls, camera/keyhole fixes and latest saved settings.
No new camera compensation or guessed offset. The original limitation returns:
unchecking Show game arms also restores tracked hands, overriding native hand poses.
Do not describe that option as independently controlling geometry in this rollback.

Future work must explicitly distinguish pose choice from forearm geometry, preserve
ordinary movement ownership, and first validate one named mantle path. A generic
FSM StatePlayerAction match or cancellation eligibility is not a native-pose policy.
The435 test proved that helper-level policy checks cannot establish comfortable
native rendering or correct movement integration.435 is rejected, not accepted.

Next launch is recovery only: are normal crouching, jumping and looking around
restored, including the same window exit? Normal behavior supports435 as the
regression; a remaining fault requires tracing433 or persistent session state.
No new animation-visibility test in that launch.

## Installed435: retain animation with hidden arms (2026-09-17)

Clean source bdd7d207a installed as vr33-hands-working-435-gbdd7d207a.
Release,9 exports,103 catalog/policy checks plus22 handoff checks, lint and
default-profile parity pass. Both logs/priorDLL/fullINI preserved at
build/playtest-candidates/installs/20260917-204023-021893. Whole INI comparison
changes only Jump action0->1, adds Mantle action1 and Mantle Arms1->0.
Reversing those changes reproduces every prior byte. CRLF and installed DLL/INI
hashes independently verified. No game/simulator launched. Headset test pending.

Build433 mantle regression confirmed: Arms off returns hands to controller tracking
while the native FSM remains Mantle. Logs/latestINI archived in
animation-action-controls/reported433 after DLL/banner verification.
VR-134 now separates native pose ownership from full-arm visibility. Hidden arms
draw clipped/rounded animated hands with the original native palette; weapons retain
native animation. Camera and cancellation hook unchanged.103 policy/catalog checks
plus22 handoff checks pass. See ANIM-HANDOFF-PLAN.md current section.

Next launch isolates mantle: action enabled, forearms hidden, native hands/weapon
should still animate through the climb. Static/tracked hands falsify pose separation;
full forearms indicate split routing failure. Restore the prior jump-disable test to
enabled so it cannot interfere with reaching the ledge. Keep all other saved settings.

## Installed433: independent action controls (2026-09-17)

Installed vr33-hands-working-433-g5dee90153 from clean source. Release build,
9 exports,96 catalog checks,22 handoff checks,1000 production hook ABI calls,
cinematic math/scope restoration tests, default-profile parity and lint pass.
Archive: build/playtest-candidates/installs/20260917-202500-036762.
Both logs preserved. Full installedINI diff contains exactly one added setting:
Anim.Action.0.StatePlayerMasterJump=0. Removing it reproduces every prior byte;
CRLF and installed DLL/INI hashes independently verified. No game launched.

codex/misc-fixes, VR-134. Build431 door/keyhole fix reported accepted; both logs,
latestINI and verified DLL identity preserved in misc-special-camera/reported431.
Lean and texture allocation crash remain separate open items in VR-133.

F10 Animations:18 Enable action toggles reject native FSM requests before entry;
all40 Show game arms choices remain independent of cancellation and camera ownership.
Automatic/recovery/story states cannot be cancelled by this UI. Existing saved arm
values are retained. Native animation view left/right is a default-zero manual trim,
not a proven automatic correction. Both eyes share a frozen scope value; menus skip it.
Details and limitations: ANIM-HANDOFF-PLAN.md and ENGINE_NOTES.md current sections.

Next launch: does Jumping disabled prevent the jump, with Enable action checked
live restoring it? No jump followed by normal jump supports the new boundary.
Jumping while disabled means missed ownership/request path; inability to jump after
enabling means a cancellation regression. Check build banner and read the log.
Do not combine this with the later arm/alignment perceptual test.

## Installed misc candidate431 (2026-09-17)

Installed vr33-hands-working-431-g2591ebc5c from clean source on codex/misc-fixes.
Release,9 exports,908 HUD checks,85 animation catalog checks plus existing
animation blend checks, cinematic math and extracted production camera scope
checks pass. Default-profile byte parity and lint pass. Both logs, prior DLL
and complete INI archived at build/playtest-candidates/installs/20260917-125202-935413.
Whole INI comparison: only ReadingTilt -31.000->0.000, ReadingTiltReference=1,
and Cine.SpecialHeadLook=1 added. Reversing those changes yields identical prior
bytes; CRLF and installed DLL/INI hashes independently verified. No game launch.
F10 Animations exposes arm-state choices; F10 View has the armed special-camera
option. One question this launch: natural head look during Y-lean and normal
camera movement after release? Keyholes and animation checkbox perception remain
separate follow-up validation. Texture allocation crash is diagnosed, not fixed.

## Current: misc fixes after merged PR73 (2026-09-17)

Controller/reading work merged to VR-Main in PR73 (374440668); preserved its
branch. Active branch codex/misc-fixes. Accepted427 ReadingTilt=-31 becomes0,
with versioned migration preserving the same physical angle. Latest427 logs,
INI and crash dump archived under reading-fixed-grip/accepted427.

VR-134: F10 Animations lists all40 shipped player FSM entries (23 master,11
upper,6 left) with independent live/saved arm checkboxes, active state display,
filter and reset. Existing handback defaults remain until overridden. States,
not individual clips: sequence history can be stale. Existing body correction,
blend and liveness rules retained. Camera classification ignores these new
visibility overrides, so checking walking does not claim special head look.

VR-133:119 lean trace samples retain player influence0/1/0 and PVR head writes;
native lean adds about10 degrees of camera roll after that write. Decompiled
lean limits pitch/yaw; keyholes relocate the controller. Candidate extends the
existing draw-scoped head owner only to these two explicit states, restores
native fields after both eyes, and carries physical yaw once on return to Walk
with refreshed identity validation. Native translation remains; special views
use raw HMD position rather than gameplay neck cancellation. Default-off
Cine.SpecialHeadLook has F10 View toggle; arm it for the next lean test.

Crash is a separate unresolved allocation failure: SYSTEMMEM DXT5 twin
1920x2048 lv1 returned0x8007000e before LockRect INVALIDCALL. Installed exe is
already large-address-aware. Preserved dump lacks memory-info stream; do not
claim leak, texture-pack causality or camera causality. Add failure-only virtual
memory/commit diagnostics and memory-info metadata to ordinary crash dumps.
No destructive shadow eviction or device-policy change. No game/simulator run.

Next launch question: while holding Y and leaning, can the head turn naturally
and return to normal movement after release? Stable hold/exit supports scoped
ownership; hold-only failure points at native modifiers, exit-only failure at
handoff. Any repeat crash requires allocation diagnostics. Keyhole validation
and animation checkbox perception follow separately, not in this launch.

## Current: accepted reading attachment and controller merge (2026-09-17)

Build427 fixed reading attachment accepted after ReadingTilt=-31.000 adjustment.
Rebase that exact physical angle to zero; ReadingTiltReference=1 versions the
trim so older saved angles migrate without changing appearance. Both427 logs
and latest INI archived in reading-fixed-grip/accepted427; DLL/banner verified.
VR-132 tracks accepted controller and reading work. New lean/keyhole camera
instability and subsequent texture allocation failure are VR-133; animation
checkboxes are VR-134 under VR-89. These follow on codex/misc-fixes after the
explicitly authorized controller PR/merge. Crash log reports SYSTEMMEM twin
allocation 0x8007000e before texture LockRect failure; camera causality unproven.

## Current: fixed captured reading attachment (2026-09-17)

425 automatic entry tilt was active in logs but did not resolve the reported
inconsistency. Its per-opening grip reference remained variable. Verified425
DLL/banner and archived both logs/latest profile in reading-auto-tilt/reported425.
The last full Note pose at16752906 supplies gripQ=(.672240,-.104614,-.336110,.651290)
and pageQ=(-.049763,-.026783,.011967,.998330), autoTilt=-45.267, manual=0.
This is the last recorded book sample, not a claimed exact shutdown sample.
Replace entry fitting with fixed inverse(gripQ)*pageQ reference. Preserve the
placement basis separately by removing the old pitch from pageQ before deriving
its grip-relative rotation. This reproduces both orientation and the existing
position offsets at that sample; both then follow the current hand rigidly.
No head/opening pose participates. Keep one additive ReadingTilt slider for
notes/books/journal. Remove the failed automatic pitch helper; history remains
below. Headset confirmation pending. Test: open the same book with different
initial hand poses, then return to the comfortable pose. Does its angle remain
correct and identical? Consistent but wrong means trim/reference needs adjustment;
variable means another attachment path remains. No game or simulator launch.

Installed vr33-hands-working-427-g15503fbcc from clean source.900 HUD anchor
checks, default-profile parity, release build,9 exports and lint pass. Both logs
and prior DLL/INI archived at build/playtest-candidates/installs/20260917-122258-812613.
Full installed INI is byte-identical to prior saved profile; CRLF and DLL/INI
hashes independently verified. Headset result pending.

## Current: automatic reading entry tilt after423 (2026-09-17)

423 DLL/banner verified; both logs/latest INI archived in reading-tilt/reported423.
Single manual tilt still depended on opening hand position because initial page
was forced upright at every height. Compute initial pitch from actual panel
center to eye height in the opening yaw plane, then latch it for this opening.
Keep hand-follow rotation and position offsets; do not swivel as the head moves.
Single ReadingTilt remains additive trim. Behind-head/degenerate fit falls back
to upright. No calibration UI, additional axes, camera or stereo changes.
The exit log again lacks complete left-grip pose; do not claim the preferred
pose was recovered. Latest saved trim is0. Added bounded entry/full-pose logs
for subsequent reports. Standalone tests vary entry wrist rotation and hand
height/yaw. Installed vr33-hands-working-425-g17b228d64.564 anchor checks, release,9 exports
and lint pass. Both logs/full prior DLL/INI archived at
build/playtest-candidates/installs/20260917-115527-386246. Entire installed INI
byte-identical; CRLF and DLL/INI hashes verified. No game/simulator launch. One launch question: opening the same book
with your hand low versus near eye level, does it face you comfortably in both
cases without changing the slider? Yes supports geometry-based entry tilt;
wrong/inverted or unchanged pitch falsifies the fit/sign/context path.

## Current: single reading tilt slider (2026-09-17)

User replaced calibration request with one rotation axis; existing attachment
is satisfactory. Remove calibration/timer/quaternion configuration UI and path.
Retain419 opening/hand-follow attachment, with shared ReadingTilt in degrees
(default0, range-180..180) for notes/books/journal. F10 HUD > Notes and journal
on the hand exposes only Reading tilt, saves live. Rotation is local pitch
around the existing panel center; hand-relative center and all position offsets
remain unchanged. Retain per-panel width/right/up/depth controls. Old calibration
keys ignored. Three-axis draft never built or installed. No guessed comfortable
pose.469 anchor tests pass, including identity at0 and signed local pitch.
Installed vr33-hands-working-423-g4770f6d8d;469 anchor checks, release,9 exports,
lint and production default-profile parity pass. Both logs/full DLL/INI archived
at build/playtest-candidates/installs/20260917-114630-645045. Entire installed
INI byte-identical; CRLF and DLL/INI hashes verified. No game/simulator launch. One launch question: does Reading tilt adjust the book
angle without moving its attachment point? Yes supports center-preserving tilt;
position movement or extra rotation means the placement composition is wrong.

## Current: saved reading attachment after accepted419 (2026-09-17)

419 controller changes headset-accepted. Verified DLL/banner and archived both
logs/latest profile in controller-cross-hand/accepted419. User ended the run
with a comfortable book pose and requested a persistent default attachment.
The log does not contain the complete reading grip quaternion/relative pose;
scalar hand-angle/depth diagnostics cannot reconstruct it. No pose was invented.

Current readers recapture upright orientation relative to the entry grip on
each menu opening. Add shared ReadingSavedGrip (default1), validated relative
quaternion ReadingGripQx/Qy/Qz/Qw and ReadingGripValid (default0). Without valid
calibration retain existing behavior; no arbitrary new angle is installed.
F10 HUD > Notes and journal on the hand offers five-second calibration: close
F10, hold left hand comfortably, look where the page should face. Capture stores
inverse(grip)*head orientation, preserving offsets/size, applies to notes/books/
journal and persists automatically. Three-second tracking retry then fail-soft;
invalid capture retains prior saved grip. Toggle restores old per-opening mode.
New NoteHandUp/JournalHandUp sliders complement existing right/depth offsets.
No weapon-dial lifecycle, camera, image orientation or stereo policy changes.
472 anchor checks pass, including saved reload, changed opening gaze, rotated
hand, invalid quaternion refusal and preservation of previous calibration.
Installed vr33-hands-working-421-gddc7000cd from clean sourceddc7000cd.
Release build/9 exports/lint and472 anchor checks pass. Both logs/full previous
DLL/INI archived at build/playtest-candidates/installs/20260917-114102-728633.
Entire installed INI byte-identical to latest save; zero settings changes,
CRLF and DLL/INI hashes independently verified. No game/simulator launch.
Exact preferred attachment awaits the one-time F10 capture; no guessed pose
promoted into repository defaults. Branch local; no PR/main merge requested.
One launch question: after using the five-second capture in a comfortable pose,
does reopening a book from a different wrist position preserve the same fit in
your hand? Stable fit supports the saved relative transform; changed fit means
an entry path still replaces it. Check hud/reading-grip and installed saved keys.

## Current: cross-hand controls and powers-menu scroll (2026-09-17)

417 verified against installed manifest/hash/log, both logs/latest INI archived
at build/playtest-candidates/controller-emulation/reported417. Report: Y lean
needs the right stick; left thumbrest modifier did not work; remove grip choice;
right-stick scroll absent in journal powers menu. Logs prove both thumbrests
reported. Left modifier4 was paired with left stick (flip0); no direction bits,
whereas R3 produced all four D-pad directions. Same-hand pairing is the measured
configuration mistake, not missing left touch. Updated policy automatically
pairs either thumbrest with its opposite stick, including old INI combinations.

Remove grip mode from UI and composer; keep left-rest numeric value4. Old mode3
normalizes toOff and neither grip is consumed. Gameplay-only Y sends physical
right-stick axes to native left-stick lean axes at the final pad boundary and
zeros right axes; it outranks D-pad and excludes menus/wheel/cinematics/F10.
No engine-memory changes or camera writes. Lean behavior still needs headset
confirmation; it uses the existing native Y action. Native non-wheel menus now
retain continuous right-stick vertical input after menu shaping. Horizontal
menu shaping, wheel input and modifier ownership retain precedence.

196 host checks (removed grip tests, added cross-hand/lean/menu axis checks),
default writer/package/golden byte parity pass.
Installed vr33-hands-working-419-g1f10404e6 from clean source1f10404e6.
Release/9 exports/lint pass;196 controller checks and default-profile parity pass.
Both logs/full prior DLL/INI archived in
build/playtest-candidates/installs/20260917-104459-011878. Entire installed INI
byte-identical to latest save; zero settings changes. DLL/INI hashes and CRLF
independently verified. Headset result pending; source local, no new PR/merge.
Ticket publication was explicitly requested, but automatic review rejected even
the minimal summary to Linear; exact-text approval question pending. No issue ID.
One launch question: in the journal powers menu, does the right stick now scroll
up/down normally? Success confirms final-axis restoration; no response with
nonzero pad/axes RY means the menu needs another native input path; zero RY
means a context/modifier gate is still consuming it. Do not launch game/simulator.

## Current: controller emulation improvements (2026-09-17)

PR72 merged accepted marker/startup work into VR-Main at6a403c600; preserved
feature branch. New branch codex/controller-emulation-improvements starts there.
Installed413 DLL/banner verified and both logs/latest profile archived in
build/playtest-candidates/vr129-rune-icon-continuity/accepted413 before new work.

New F10 Controls tab: DpadModifier0..4 matches BioShock1's explicit choices,
DpadFlip0 selects left stick,1 right; flip automatically swaps thumbrest choice.
PauseChord1 enables X+Y as menu. Default modifier1/right thumbrest, flip0/left.
Menu tap pulses Start150ms on release; modifier+menu immediately holds Back;
500ms unmodified hold also yields Back. Back ownership survives releasing the
modifier first. Chord consumes X/Y until both release. Y now forwards native Y.

Pure composer precedes Dishonored pad shaping. All four D-pad directions are
HELD: game Select on press/Use on release, not BioShock ammo pulses. Consume the
selected stick, R3 health input or left-grip wheel input only when assigned.
Wheel can still use hand direction with D-pad shortcut assignment. Recenter and
accepted menu navigation retained. No camera/stereo/engine-memory writes added.
221 host checks and production default-writer/package/golden byte parity pass.
Linear ticket creation pending explicit external-publication permission after
approval review rejection; no ticket identifier invented.
Installed vr33-hands-working-417-gdc6751a0f from clean sourcedc6751a0f.
Release/9 exports/lint/221 controller checks and default-profile byte parity pass.
Both logs, previous DLL and full INI archived at
build/playtest-candidates/installs/20260917-092418-488098.
Entire installed INI differs only by three new Controllers keys:
DpadModifier=1,DpadFlip=0,PauseChord=1. Removing those lines reproduces
prior INI byte-for-byte; CRLF and installed hashes independently verified.
No game/simulator launch. Controller source remains local, no new PR/merge.

One launch question: does right-thumbrest + left stick select/use all four
shortcuts without walking? Correct selection supports runtime-to-pad mapping;
no response implies binding/threshold gap, walking implies consumption failure.

## Accepted marker candidate413 (2026-09-17)

Tester accepted the objective/rune work for merge after413; this is practical
acceptance, not a claim that every possible single-frame artifact is eliminated.
Installed DLL hash and413 log banner verified; both logs/latest profile archived
in build/playtest-candidates/vr129-rune-icon-continuity/accepted413.
107 native HUD checks,7814 native marker ABI checks and9 startup-policy checks
passed across this branch, plus release build and9 export validation.
Next work: controller emulation parity with BioShock1 modifier/menu controls.

## Current: rune icon continuity after411 (2026-09-17)

411 verified; group, startup and camera behavior reported good, only occasional
brief inner-rune-icon transfer during head turn remains. Both logs/latest INI
archived under vr129-live-rune-ownership/reported411. New candidate bridges only
already native-confirmed small icon content for two frames/100ms maximum, no
self-renewal, current draw center, cleared by menu/reset/toggle.107 HUD checks
pass. Exact flash absent from sampled log; phase mismatch remains hypothesis.
Full scope/limitations in ENGINE_NOTES and FLICKER_REFERENCE. Preserve entire
latest installed profile unchanged; no camera/intro/stereo policy changes.
Next question: does the inner rune icon stay native through the turn that
previously caused the flash? Persistence falsifies coverage; unrelated HUD
remaining native indicates false association.
Installed vr33-hands-working-413-g612760965 from clean source612760965.
Release build,9 exports,107 HUD checks,lint pass. Both logs/full prior INI/DLL
archived at build/playtest-candidates/installs/20260917-084907-002805.
Entire installed INI byte-identical to latest saved profile; zero settings
changes, CRLF and DLL/INI hashes independently verified. Headset result pending.
Continue codex/objective-marker-fixes; no game/simulator launch or main merge.

## Current: live rune ownership trial after409 (2026-09-17)

Branch codex/objective-marker-fixes.409 verified and reported: rune title/distance/
locator still change layers; whole group initially on window. Logs and full INI
at build/playtest-candidates/vr129-rune-inner-artwork/reported409. New numeric
native-parent snapshots bypass edge learning and use measured aspect-fit mapping
for the whole bounded rune group; spatial association remains a candidate, not
GFx draw identity. New NativeRuneOwnership ON, older NativeMarkerChildren OFF.
Latest task/rune insets22% and native size0.330 retained with all saved settings.

F10 capsule-limit checkbox removed; explicitly install EyeClamp=0 since the log
records the successful off comparison but saved INI lacks the value. No neck or
stereo changes. Intros now handled by proxy -nostartupmovies from its existing
launch hook, default on even with absent INI. Prior manual game config changes
are reverted tofalse to test the actual proxy policy. Details in ENGINE_NOTES,
HUD_ANCHORS and FLICKER_REFERENCE.97 HUD,7814 ABI,9 startup host checks pass.
Installed vr33-hands-working-411-gfaebfbbac from clean sourcefaebfbbacce0871dd8334a2bd1d420c723b23259.
DLL SHA256 f23073806db02e547ff7ab5f666f30db0da78f21852711923d462e9a59d50fe1.
Release build,9 exports,lint pass. Both logs/full prior INI/DLL archived at
build/playtest-candidates/installs/20260917-083639-637886. Entire INI diff only
NativeMarkerChildren1->0, add NativeRuneOwnership=1 and EyeClamp=0. Installed
DLL/INI hashes and CRLF verified. Both game startup flags restoredtrue->false
with full backups/diffs in candidate/startup-config-before-proxy. No game or
simulator launched; new integration/headset result pending.
One launch question: is the entire rune group native from first reveal and does
it remain together through turns and edge transitions? Splitting falsifies
coverage; unrelated HUD joining means false association. Keep objective settings.
No PR/merge authorization; changes remain local.

## Current: rune inner-artwork candidate, pitch ceiling identified (2026-09-17)

Continue codex/objective-marker-fixes.407 objectives accepted; rune inner image
alone still on window. Verified DLL/banner; both logs/latest INI archived at
build/playtest-candidates/vr129-native-rune-boundary/reported407.
New child association is a guarded heuristic with bounded content cache, not
native instance ownership. F10 HUD > Objectives > Keep marker inner artwork
native (test), missing-key default off, next candidate on.55 host checks pass.
Preserve saved task22% and rune17% insets and all other latest F10 values.

VR-87 vertical pitch symptom matches final camera ceiling in both stances;
existing EyeClamp exposed in F10 Comfort, retained ON for rune test. Full
measurements/limits in ENGINE_NOTES and FLICKER_REFERENCE. Camera comparison
is a separate next test; no neck retuning or clearance-safe fix claimed.
VR-131 startup-only suppression restored in both game INIs; only one key each,
backups/full diffs/CRLF verified. No movie assets changed or game launched.
Next launch question: does rune inner art stay inside the outline through head
turns, center and edge? Together supports fix; split means association miss;
unrelated captured content means false association. Read native-child logs.
Installed vr33-hands-working-409-gc9d1d4a41, clean sourcec9d1d4a41034d6442e42358fa807f1724b984709. DLL SHA256
40ae3ddae27306d2ce42fc06b0c4bf3434815d802cab19b40c1c662fb9d5ffde.
Release build and9 exports pass;55 native HUD checks, lint and diff checks pass.
Archive: build/playtest-candidates/installs/20260917-080727-649109.
Full mod INI diff: only add NativeMarkerChildren=1. Installed hashes and CRLF
reverified. No game/simulator launch, no push/PR/main merge. Camera comfort
control is available but unchanged ON, pending separate test.

## Current: native objective/rune boundary trial407 (2026-09-16)

Branch codex/objective-marker-fixes, renamed by request. Installed clean source
17ec5db4bc9932e072921cbf58a949a31115647d, build407-g17ec5db4b. DLL SHA256
5b8bffd97393bbe8d621e145b6d80dbc4b8efe12ac10c4cdf4669ea890e82720.
Native task and rune parent hooks each inset visible offscreen markers; native
child direction, distance, visibility and Heart reveal logic stay engine-owned.
F10 HUD > Objectives has separate native objective/rune toggles and inset sliders.
Trial switches ON, both margins12%; repository missing-key fallbacks remain OFF.
Current ownership heuristics remain; semantic parent-to-D3D draw ownership is not
complete. Read ENGINE_NOTES top native task/rune derivations before continuing.

7814 production wrapper/policy checks,44 existing native HUD checks,14 offline
call/ABI/constructor checks, release build,9 exports and diff checks pass. No
simulator/game launched.406 was installed but never launched; current log still
belongs to verified accepted401, not407. Both logs, DLL and full INI archived at
build/playtest-candidates/installs/20260916-232954-462421 before replacing406.
Entire INI diff407: add NativeRuneMarkers=1 and RuneMarkerEdgeInset=0.120 only.
Previous406 install added NativeTaskMarkers=1 and TaskMarkerEdgeInset=0.120 only;
all accepted401 values retained. Installed DLL/INI hashes and CRLF verified.
Candidate/manifest: build/playtest-candidates/vr129-native-rune-boundary.

One launch question: do the objective and rune offscreen indicators move inward
with their respective F10 inset slider while retaining direction and their artwork?
Expected: parent artwork shifts together; on-screen target position stays native.
Success validates both native boundaries before ownership replacement. No movement
requires checking hook/owner/flag diagnostics; partial movement or wrong direction
falsifies the assumed parent coverage. Read hud/task-parent and hud/rune-parent only
after checking the407 banner. No claim of completed plane-switch or yaw fix.
Local commits only; no new merge authorization. Preserve both logs before relaunch.

## Current: objective script review (2026-09-16)

Local codex/vr-129-objective-review from merged PR71. Reviewed task/target and HUD
UnrealScript declarations, native marker Flash hierarchy/actions and all2554
verified native exec registrations. Findings: ENGINE_NOTES top VR-129 review.
Current draw-shape/proximity routing cannot guarantee whole-marker ownership;
upright correction only rotates on-screen geometry and does not fix yaw projection.
The native task-marker parent/update path is the next investigation boundary.
No runtime code, build, installed401, F10 defaults or game files changed. No new
launch requested. VR-129 remains open; this review is not a perceptual fix.

## Current: HUD improvements merged in PR71 (2026-09-16)

Build401 (vr33-hands-working-401-g4f974a5da) accepted for publication/merge.
DLL hash and log banner verified; both logs and latest full F10 INI archived at
build/playtest-candidates/vr129-side-alpha/accepted401. User explicitly authorized
PR and merge of codex/vr-129-hud-fixes into VR-Main. PR71 merges this work;
the local and remote feature branch are preserved.

Latest profile captured byte-for-byte in release INI, golden and production
WriteDefaultIni. Only new tuning since previous defaults: WheelPartsAlphaMode=mix,
Gain0.890,Gamma0.660,Mix2.090 (Floor remains0). Accepted INI SHA256
9c429cb77e9bc730d696357b46ed7640f5d6a4f33b8a25f25084ae8fe91a68af.
Installed401 remains the tested binary; no reinstall is required for a defaults
literal update. No game/simulator launch. Production writer/package/golden parity,
lint and diff checks pass; clean release build and all9 exports pass.
PR: https://github.com/VR-Stereo-Hub/Dishonored-VR/pull/71

Accepted scope: wheel closing visual ownership; independent D-pad/potion window
panels with shared alpha; rounded wrists; accessible native objective controls;
complete tuned defaults. Host evidence:104 HUD controls,912 wrist/crop,44 native,
465 route,462 anchor,45 menu checks and actual D3D11 WARP mask/hue/crop checks.

VR-129 remains open for occasional objective/window transfers, incomplete native
label association and the earlier rare wheel-exit zoom. Capture-miss logging is
diagnostic, not proof of semantic ownership. VR-130 rounded wrists are accepted.
No new test requested for this merge. Next work should start from updated VR-Main,
read current HUD_ANCHORS/FLICKER_REFERENCE, and verify installed.json/log identity.

## Current handoff: VR-126 refined dial and menu immersion (2026-09-16)

- Branch codex/hud-weapon-dial; PR67/68 already merged, no HUD merge authorized.
- Build372 dial accepted and tuned: width0.350m,travel0.040m,crop0.400x0.400.
  Verified logs/INI archived in build/playtest-candidates/vr126-weapon-dial/accepted-tuning.
- New work: tiny-motion direction selection, camera-parallel panel, true circular
  feathered crop, closer/farther slider, per-menu live head look and UI blur controls.
- Installed374 (`vr33-hands-working-374-ga51e1799f`), clean source a51e1799f.
  Candidate build/playtest-candidates/vr126-dial-immersion; both logs and previous
  DLL/INI archived in build/playtest-candidates/installs/20260916-010814-146048.
  Full INI diff:16 new controls only; every previous value retained. Hashes/CRLF
  verified. HeadLookWheel/Note=1,NoBlurWheel=1,direction-only/circle=1,deadzone2mm,
  distance0; tuned width0.350m/crop0.400x0.400 retained. Not headset-tested yet.
- Current candidate identity is in build/playtest-candidates/installed.json.
  Preserve all installed settings/CRLF; compare the entire INI and archive both logs.
- Read only the current VR-126 refinement section in [HUD_ANCHORS](dishonored/HUD_ANCHORS.md) for
  details, evidence, failed hypotheses, validation and the single launch question.
- Next question: with Wheel open, does turning reveal fresh world scenery beyond
  the old FOV rectangle and return normally on release? Camera and UI-blur fixes
  remain headset-unverified. Scope and effect diagnostics distinguish the outcomes.
- Standalone2185 dial checks, actual GPU mask test,16 production menu lifecycle
  checks, existing camera/FOV and HUD tests pass. No game/simulator launched.
- No subagents, source branch deletion or new main merge. User launches only.

### Minimal next-chat prompt

Continue VR-126 on codex/hud-weapon-dial. Read AGENTS/CLAUDE, current STATUS and
HUD_ANCHORS current refinement section; FLICKER_REFERENCE current menu-world entry
for camera work.372 dial accepted; preserve tuned35cm width and0.4x0.4 crop. New
candidate adds2mm direction-only selection,camera-plane orientation,circle mask,
distance offset,and per-menu head-look/UI-blur controls. Check installed.json and
verify log banner before analysis; archive both logs and preserve full INI/CRLF.
User checks head turns beyond old FOV while Wheel open. Inspect menu/head and
menu/blur; a write is not visual acceptance. No subagents or game/simulator launches.

## Performance research shelved, 2026-09-15

Historical shelving checkpoint; the VR-50 exception above supersedes installation and
playtest status. Unfinished performance tickets
are Backlog and unassigned. [PERFORMANCE.md](dishonored/PERFORMANCE.md) is the compact canonical
record of measured results, rejected routes, useful tools and resumption options.
Ordinary per-eye preparation measured 1.322 ms/pair including 1.082 ms culling;
no safe work-sharing optimization is established. Default-off profilers and the
measured desktop throughput/tail experiment are retained. Failed nonblocking
Present and invalid coarse CPU-ms instrumentation are removed.
Exact installed307 remains unchanged. Research cleanup is build/host-validated;
no new install or headset acceptance is claimed. Source branches are preserved.
The user authorized a consolidation PR and merge to VR-Main; this is research
preservation, not promotion of experimental rendering settings.

## Current state: every HUD element on its own anchor, and a real alpha (VR-118, VR-119, VR-120), headset run 1 good, PR #64 open - 2026-09-15

Branch `claude/vr-120-hud-elements` off `claude/vr-117-hud-redo` (PR #63, still NOT
merged; this branch's PR #64 is stacked on it and says `Ref`, not `Fixes`, until
VR-117 lands). NOT merged to VR-Main. Never merge without permission.

What this branch delivers, on top of VR-117's redirect and quads:

- **VR-118 answered.** The HUD's 2D transform is the vertex shader's own `Transform`
  at c6..c9 (four columns; the textured variants add a `TextureMatrix` at c10..c13),
  read from each shader's disassembly at first sight (`d3dcompiler_47`, never a
  hard-coded register); c0..c3 were stale constants. The fixed-function hypothesis
  was tested and died in one window (0 SetTransform calls, every HUD draw with a
  shader). The probe now reads 9324 rectangles per 3 s with 0 refused at 1.0 us each,
  and `draws regions` clusters them into the element census (`draws/cluster`).
- **VR-119: the alpha.** `[Hud] AlphaMode=repair|captured|mix` (ships `repair`, the
  41.2 picture), gain/floor/gamma/mix sliders, a backdrop plate per anchor kind, all
  on the F10 HUD tab and `hud alpha ...`; `captured` forces the coverage equation on
  every redirected draw (the game's own equation replaces alpha per draw with
  ONE/ZERO, which is why black strokes vanished under the repair). `dump hud` writes
  the alpha as grey; the sim's `quadAlphaPct` fails a mode that collapses to zero.
  Measured: the vitals' strokes solid in `captured` (7.96 % of the corner at alpha
  >= 200) against mottled in `repair` (2.54 %).
- **VR-120: the element table.** Rows: `default`, `vitals` (health + mana: they
  interleave), `reticle`, `prompt` (measured regions), eight unmeasured rows that
  ride `default` until `hud region <name> ...` names them, `vignette`, and the six
  screens by their UI owner context. Anchors per row: off, frame, window, world,
  handL, handR; two hand panels; sinks per (anchor, crop|all); one quad per cropped
  element with a stable crop-sized swapchain slot; a screen set off or frame takes
  the mono screen. Ships with everything on the window (the VR-117 picture) and
  `Regions=1`; `Regions=0` is the one-quad A/B. Design and every number:
  `docs/dishonored/HUD_ANCHORS.md` (sections 2 and 7), ENGINE_NOTES ("How the
  Scaleform HUD identifies its elements", "The HUD's blend equation").

Simulator, the sewer level (`console open L_PrsnSewer_P` from the MAIN menu, then one
Return; sent from the title screen it left the game on the loading board for eight
minutes): `hud-elements.xrs` 33/33, `hud-quads.xrs` 35/35, `hud-panel.xrs` 24/24,
`pause-ride.xrs` 31/31, `wheel-ride.xrs` 27/27, `hud-alpha.xrs` 32/32. Host: 20
hud-route, 30 hud-anchor, 107 ui-ride checks. Lint clean, exports 9/9, golden ini
MATCH, the installed ini a byte copy of `release/dishonored_vr.ini`. Cost `perf: tick
13.5 ms (73.7/s)` with the probe, the routing and two quads (VR-117: 13.2 ms, one
quad). Two faults found and fixed on the way (the lazy sinks and the armed-only
router deadlocked; a hand-anchored element placed by its screen offset), one setup
trap (the console open from the title screen); all in TRAPS.

Installed on this PC for the headset run: the RelWithDebInfo build of this branch
(d3d9.dll SHA256 starting F1E5782EF0EC218C, 2026-09-15 03:34) with the repo default ini
(SHA256 starting C0DC094D, a byte copy of `release/dishonored_vr.ini`); the Debug runs'
logs are under `D:\dvr-data\logs\vr120-run*.log`.

First headset run (2026-09-15, Release build 287, the repo ini): the preset, the
vitals on the left hand, the `default` row on the left hand / in the frame / off, the
vitals off, the pause, a note and the wheel riding, all judged good (HUD_ANCHORS section
7 has the log's lines). The alpha modes were not changed during that run, so `repair`
is the only mode judged so far; the VR-119 list (captured vs repair, floor, gain, a
backdrop) is still open. The shipped preset and the alpha default stay as they are
until the user names a change.

## Next steps

1. The alpha modes in the headset (VR-119's list: `captured` vs `repair` on the window and
   a hand, floor 0.2, gain 1.5, a 0.3 window backdrop). Flip `AlphaMode` and the preset in
   `WriteDefaultIni` + `tools\ini-golden.py` + the release ini if the user says so.
2. Review and merge VR-117 (PR #63) first; then retarget this PR to VR-Main and change
   its `Ref` lines to `Fixes VR-118, VR-119, VR-120`. Never merge without permission.
3. The eight unmeasured rows (equipment, subtitles, objective marker, toast, tutorial,
   detection, skip gauge, dark vision) need a level where they draw; the recipe is
   `docs/dishonored/HUD_ELEMENTS_HOWTO.md`: `draws on` +
   `hud regions on`, read `draws/cluster`, name each with `hud region <name>
   x0,y0,x1,y1` live, then move the rectangle into `kRows` in `hud_layout.cpp` and
   `WriteDefaultIni`. The objective marker moves with the world and cannot be claimed
   by a rectangle; if it matters, that is the Scaleform display-object identity
   (instance names), a Research ticket, not built.
4. Copy the headset log out before every relaunch.

Session log 2026-09-15 (this branch): VR-119 and VR-120 created; VR-118 answered (the
transform, the shader-read columns, the clusters); the alpha capture; the element
table with six anchors and two hands; seven simulator runs on this PC, six
sequences green; the PR stacked on `claude/vr-117-hud-redo`.

## Current state: the HUD on its anchors (VR-117), headset-confirmed, PR #63 ready for review - 2026-09-15

Branch `claude/vr-117-hud-redo` off VR-Main 85f9ef6e, PR open, NOT merged. The game's
Scaleform HUD leaves the eye textures and is shown on quad layers: a head-locked or
world-parked WINDOW (1.25 m at 1.30 m, the PR #12 preset) and the tracked HAND (38.92's
wrist HUD: 0.22 m, 0.06 m above the grip, billboarded), switchable per element with
every placement value in `[Hud]` and on the new F10 HUD tab. In-game screens (pause,
note, journal, store, mission stats) RIDE the window with the world in stereo behind
them under a ride predicate on the reflected UI owner; the main menu and loading
screens keep the mono screen. Ships ON (`[Hud] Panel=1`, the user's call) with the
whole HUD as one element; per-element routing (`[Hud] Regions`) is an instrument
until the region table is measured. Design and every measurement:
docs/dishonored/HUD_ANCHORS.md. Ticket VR-117 (VR-8 and VR-38 canceled into it).

Simulator, on the sewer level (`console open L_PrsnSewer_P`; the newest save on this
PC is a death loop at the intro boat): `hud-panel.xrs` 24/24, `hud-quads.xrs` 35/35,
`pause-ride.xrs` 31/31. The redirect takes 20.4 HUD draws per present with no empty
armed present on either re-entry pass; the pause rides with the projection up
(`L/s=46 R/s=45 mono/s=0`) and 94.9 menu draws per present on its own sink, 10 s with
no stale-flag clear, resume with both eyes fresh; `hud menu off` gives the old mono
pause as the A/B. Cost 0.3 ms per tick. Host: 107 ride-policy and 30 anchor checks,
plus the existing verdict suites, green. Three faults found and fixed on the way (a
health blink cancelling the open-gap stand-in; the HUD quads vanishing on held
presents so a riding menu blinked; a focus loss latching a permanent thread refusal).

This PC's setup was reset to the tested profile first: `release/dishonored_vr.ini`
byte-copied over the installed ini (SHA256 de20bd79 before this branch added the
`[Hud]` keys; the backup is `dishonored_vr.ini.pre-hud-redo-20260914`), the game's
`-VRBaseline` and `-Console` applied (the mouse-smoothing key was missing from this
machine's input ini and the script now appends it), 2750x2850 armed in all four
places. The host suites needed `tools/lib/msvc.ps1` to find the Build Tools here.

First headset run (2026-09-15, Quest 3 / VirtualDesktopXR): the window, the hand
panel, the pause and a note judged good. Two reports fixed from its log and
re-verified on the simulator: a ~10 Hz HUD flicker between the window and the frame
(the redirect's gate followed the per-present eye tag; it now follows the
projection mode) and the weapon scroll / grip-hold loadout dropping the world flat
(both are the power wheel, which now rides the window, `WindowWheel=1`). On the
sewer level the beat now reads `presents=467 armed=467` in every window (it read
`presents=450 armed=433` before) and `wheel-ride.xrs` passes 27/27 beside
`pause-ride.xrs` 31/31. Release build 278 of this tree is installed with the repo
default ini (WindowWheel=1) for the second headset run.

Second headset run (2026-09-15, release build 278, repo default ini): no flicker, the
weapon scroll and the grip-hold loadout stay in the window, the screens judged good.
PR #63 is out of draft and waits for review; the merge is the user's call.

Next session's prompt: `docs/dishonored/HANDOFF-HUD-ELEMENTS.md` (branch off
`claude/vr-117-hud-redo`, not VR-Main): a real alpha capture with F10 controls, and
every HUD element on its own anchor (off, frame, window, world window, left hand,
right hand), elements identified by what they are (the Scaleform movie or display
object), measured first. The VR-118 transform stays the fallback route.

The VR-118 route as it stood
(rung 3): the region probe reads the HUD's vertices (DrawIndexedPrimitiveUP, SHORT2
shape coordinates, 0.5 us per draw) but the c0/c1 transform hypothesis is wrong on
this GFx build (nonsense rectangles), so per-element routing is NOT in this branch;
the next step is the HUD vertex shader's bytecode to find the transform's registers,
then `[Hud] Region.*` so health, mana and the equipment separate onto the hand.
Never merge without permission.

## Earlier records

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

## Earlier acceptance and scope

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

## Earlier records

## Current state: VR-116 120Hz physical-head-turn flicker investigation

VR-115 desktop branch is parked at committed1d6558a3; no uncommitted changes
needed a stash. Its build271 was never installed. Do not mix that experiment
into this investigation. New branch codex/vr-116-120hz-head-motion from main255d1c91.
Verified main build266 run shows120Hz, world/hand lag2, automatic A/Bs off.
Physical head turns alone trigger severe world flicker; stick turns and still
head do not. Both cinematic and gameplay scenes affected. Both logs archived
at build/flicker-120/20260914-133932. Ring/eye summaries remain healthy; uneven
cadence and occasional pose disagreement are measured, causality unresolved.

Default-off Pace.ImageOrientation candidate uses same-eye captured camera-record
orientation for submission, with numeric lag fallback; positions and game/hand
behavior unchanged. F10 View provides live A/B and Save. Eight new host checks
and existing frame suites pass; release,exports,lint,golden pass. No game launch.
Exact findings, rejected claims and one-question120Hz test:
[HEAD_MOTION_120HZ.md](dishonored/HEAD_MOTION_120HZ.md).
Build/install identity will be recorded after packaging. No merge authorized.

## Earlier records

## Current state: standing roll accepted; VR-87 remains open

Build245 confirms roll while looking up/down. Remaining inverse vertical motion
is the existing eye-ceiling limitation VR-87, supported by joined log accounting.
See STANDING_PITCH_ROLL_ARC.md for evidence. PR57 ready, unmerged; PR58 next.
Installed build remains245. No new install or INI change. Preserve all three PRs
unmerged until remaining tests complete. Never launch the game; no subagents.

## Earlier records

## Current state: standing camera-roll candidate after accepted PR56

Installed245-g0cd7b263 (Sep14 07:57:48), standing-roll fix and ZAccount ON.
Full INI diff contains only those two changes; hashes/CRLF verified.

PR56 is ready for review and unmerged. Build239-gadebc947 confirmed head-based
movement, alongside the accepted cinematic comfort/FOV and mono handoff fixes.
PR57 now includes that parent through cb77328d. Its next build retains all
accepted settings and enables Neck.UprightPitchArc=1 plus PosTrack.ZAccount=1.
Read docs/dishonored/STANDING_PITCH_ROLL_ARC.md for the one-question test.
PR58 mono UI work remains a separate later candidate. Merge all three only after
the remaining tests finish. Character-mode drift (VR-109) remains open.
Never launch the game, no subagents. Agent installs and reads/archives both logs.
Canonical installed identity: build/playtest-candidates/installed.json.

## Earlier records

## Current state: three stacked candidates, no merge approval

The new branch codex/vr-107-mono-anchors-and-ui-state starts at d27fdf6c on
PR57's standing-camera branch. VR-107 adds configurable mono anchoring;
VR-108 and existing VR-74/VR-71 cover loading/menu stereo and input ownership.
Read docs/dishonored/MONO_ANCHOR_UI_STATE.md for implementation and test plan.
Both new levers default off; preserved build240 enables them together.

PR56 (cinematic build234) and PR57 (standing build237) remain draft/unmerged.
Keep build234 installed first; build237 and the new cumulative UI candidate
remain separately archived for agent-managed swaps. No headset testing has
occurred on these candidates. One question per launch; never launch the game.
No subagents. Build240 was installed and hash-verified, then build234 restored and verified.
The next step is deferred headset testing in order234,237,240. The agent archives both
logs and checks full INI diffs/CRLF every swap. installed.json under
build/playtest-candidates is the active installation authority.

## Earlier session records

## Current state: two candidates awaiting separate headset tests
## Current state: PR56 accepted; next is standing-roll PR57

Verified239-gadebc947 confirms head-based movement. PR56 finalized for review;
keep all three PRs unmerged until the remaining tests finish. Character-oriented
drift remains open under VR-109. Next rebuild PR57 with this accepted parent,
enable UprightPitchArc and ZAccount, and test standing pitched-head roll versus
crouching. Preserve HeadBasedMovement=1. Agent handles installs/logs, no game
launch, no subagents, one question per launch. See the latest cinematic notes.

## Previous records

## Current state: head-based movement239 installed

Installed239-gadebc947 (Sep14 07:44:40), HeadBasedMovement=1. Full INI
diff adds that key only; hashes/CRLF verified and both logs archived.

Cinematic237-g563e14d6 confirms the mono handoff fix. Character movement drift
persists; VR-109 remains open for that part. VR-110 adds a selectable, saved
head-based mode that uses native view facing, preserving the character option.
Read CINEMATIC_FOV_AND_HANDS.md latest section for evidence and the single test.
Current branch codex/vr-50-cinematic-fov-and-hands, draft PR56. No merge approval.
PR57/58 are preserved and need the accepted parent correction before new tests.
Never launch the game; no subagents. Agent builds/installs and archives logs.
Canonical active build: build/playtest-candidates/installed.json.

## Earlier records

## Current state: VR-109 correction installed, headset test pending

Installed: 237-g563e14d6, Sep14 07:27:03 (cinematic correction, NOT the
standing candidate237-gfd7a830d). Full INI diff empty; both logs archived.

2026-09-14. Verified build234 (23:11:43 Sep13) playtest confirms cinematic roll/
pitch comfort, FOV and free look. New VR-109 tracks brief mono exits and diagonal
movement after cinematic ownership. Fix extends draft PR56 on
codex/vr-50-cinematic-fov-and-hands. No merge approval. Read the latest section
of docs/dishonored/CINEMATIC_FOV_AND_HANDS.md before continuing.

PR57/build237 and PR58/build240 remain preserved, unmerged and untested. They
predate this correction: propagate an accepted parent fix before their next
builds/tests. Never launch the game. No subagents. One question per launch.
Agent archives both logs, verifies the banner and installed hash, builds/installs
and checks the full INI diff and CRLF. Canonical install manifest is
build/playtest-candidates/installed.json. Mantling still lacks explicit acceptance.

## Previous session records

## Current candidate: upright cinematic tracking

Cinematic PR56 is draft/unmerged; build234 is preserved for the first test.
Draft PR57 targets the cinematic branch (PR56). Both remain unmerged.
Current branch codex/vr-106-standing-pitch-roll-arc was created from parent
codex/vr-50-cinematic-fov-and-hands at6f85417a. VR-106 adds a default-off fix
for the standing steep-pitch roll arc. Read STANDING_PITCH_ROLL_ARC.md in
docs/dishonored for evidence, candidate, controls and the two-build test order.
The agent manages installs and archives both logs, with full INI diffs/CRLF.
Canonical active installation: build/playtest-candidates/installed.json once
swaps run; preserved bundles are under build/playtest-candidates. Build234
must be left installed first for tomorrow. No test tonight, no game launch,
no subagents, no merge approval. Mantling still untested.

## Previous records (latest candidate above supersedes earlier plans)

## Combined build steering, 2026-09-13

The user explicitly requests both FOV suppression and native cinematic hands/arms
in the same build. LockFov and CinematicHandBack are implemented and installed
ON together for the next test. The earlier separate-build plan below is superseded.
Current branch codex/vr-50-cinematic-fov-and-hands; VR-50 and VR-104 In Progress.
Read CINEMATIC_FOV_AND_HANDS.md (under docs/dishonored) for current scope and test.
No new merge approval. Never launch the game; no subagents.

## Current state: cinematic FOV candidate, 2026-09-13

PR55 merged823c53d5 and VR-103 verified Done after full headset acceptance.
New branch codex/vr-50-cinematic-fov-and-hands starts from that merge. VR-50
In Progress: suppress cinematic speaker/choice FOV narrowing with default-off
LockFov. VR-104 In Progress: native hands/arms handback, investigated and queued
for its separate behavior/test after FOV acceptance. No hands changes installed.
Read docs/dishonored/CINEMATIC_FOV_AND_HANDS.md. Current install identity lives in
build/cinematic-fov/latest-install.json. Never launch the game; one question per
launch, read/archive logs. No subagents and no merge permission for this branch.

## Current state: VR-103 accepted for merge, 2026-09-13

PR55 candidate225 d015b1aa21:05:03 is headset-confirmed for all reported cinematic,
dialogue-choice and gameplay handoff stereo interruptions. Logs verify Listening,
Choosing and latch-delayed Walk remain stereo; notes/menus fall back correctly.
Explicit merge approval received. Next branch investigates cinematic FOV shrinking
(VR-50) and native hand/arm animation. No subagents; never launch the game.

## Current state (2026-09-13): PR54 merged; VR-103 candidate

Cinematic head look, pitch and black borders are headset-confirmed and merged
in PR54 (39a68a50); VR-70 is Done. New VR-103 is In Progress on
codex/vr-103-stereo-state-transitions from that merge. The candidate separates
stereo scene eligibility from gameplay/input locks using decompiled state
names and live scene activity. It ships default-off; candidate225 d015b1aa21:05:03 is installed with
StereoState=1 and awaiting headset testing. Build,24 policy checks, lint,9
exports, golden INI and60-frame standalone simulator pass. Full installed INI
diff contains only the new key; CRLF verified.
See docs/dishonored/STEREO_STATE_TRANSITIONS.md for evidence and the one-question
dialogue transition test. Latest install manifest: build/stereo-state/latest-install.json.
Never launch the game; read/archive logs yourself. No subagents this session.
No merge permission for VR-103. PR54 approval does not extend to this work.

## Earlier (2026-09-13): load/reload fixes confirmed and merged (PR #53)

Headset-confirmed: startup stereo without jumping/crouching, prompt notes across
save reloads, stable right-eye presentation after reload, and safe pause after a
crouched reload. VR-96's cause was a crawl-release write through three old hand
control pointers recycled into upgrade objects. The writer now checks fresh
liveness plus retained identity before either strength store. The final run
exercised nine rejected stale updates, 57 valid updates and 11 pauses, with no
exception and clean exit. Evidence is in ENGINE_NOTES and FLICKER_REFERENCE.

PR #53 merges branch commit `dbf61fa7` into VR-Main under explicit maintainer
authorization. The complete installed INI
and saved F10 profile are repo defaults. The packaged ini is its byte copy; the
production default writer/golden match it. Diagnostics remain enabled, name cache
off, no config version bump. Defaults and build verification are recorded in the
branch closure. No game was launched by the agent and no release is declared. The final merged
build is installed and verified against `build/crash-triage/latest-install.json`;
read that manifest for its exact banner/hash before the next playtest.

**Next session:** physical head tracking during cinematics. Right-stick turning
works, but real head movement does not move the view. Start from the merged
VR-Main, create/verify the Linear ticket and a new branch, then trace which game
state, camera writer and runtime layer own the cinematic view before changing it.
Read [NEXT_SESSION.md](dishonored/NEXT_SESSION.md) for scope, preserved settings,
Linear updates, references, and the requirement to keep normal gameplay stable.

Linear tools were absent in this task and UI runtimes failed to start. Do not
claim board synchronization completed. Verify VR-96's closure and apply the
prepared batch updates next session. Weapon startup freezing remains open;
CacheNameLookups is implemented but stays off and needs a separate timing test.

## Earlier (2026-09-13, session 39): VR-93, VR-80 and VR-98 merged. Next: stereo after a load

**The after-note flicker is fixed (VR-80).** It was a late eye tag: after a crouched note
close a present could show a draw's image before that draw's tag reached the ring, the ring
ran one tag behind until a drain, and each cycle put the left image in the right eye. The
repair settles the late tag at the next present when that present's camera step confirms it,
and relabels the capture slot still waiting to be delivered (under `SharedWait=0` the pixels
arrive one present after the label). Headset-confirmed: 71 repairs relabelled, 0 refused,
reported essentially clean. The full account is FLICKER_REFERENCE 3.15 "The solution"; the
tools that found it (the ring ledger with draw ids, and the host model compiling the shipped
pairing) are reusable.

**A note now switches mono and back with its screen (VR-98).** The note's view silence went
through the load rules (750 ms to mono, a second back); it now counts as a menu's. Measured
0-15 ms each way, was 0.65-0.95 s. Headset-confirmed.

| Lever | Shipped | Installed on the test PC |
|---|---|---|
| `[Hands] AttachKeepOnMenu`, `AttachKeepOnNote` (VR-93) | 0 | 1 |
| `[Menu] UiKeepOnMenu` (VR-93) | 0 | 1 |
| `[Stereo] LateTagRepair` (VR-80) | 0 | 1 |
| `[Menu] NoteFastMono` (VR-98) | 0 | 1 |
| `[Stereo] RingLedger`, `PairTrace`, `DrawCallerTrace`; `[Menu] UiFlags` (diagnostics) | 0 | 1 |

All five behaviour levers are headset-confirmed and ship OFF under the default-off rule;
whether to promote them to fresh-install defaults is an open decision for the maintainers.

### Found and filed, not fixed

- **VR-99**: why the tag margin collapses after a crouched note close and a pause restores it;
  an occasional single frame remains; whether `LateTagRepair` should default on.
- **VR-96 (High)** GC crash and **VR-97** zero-`c5` skew are unchanged from session 38.

### Next

**Stereo after a load.** Reported: on a level load the hands track at once but the weapons do
not; 2-3 s in the game drops to 0 fps for 3-4 s, the weapons then track, but the view stays
MONO until the player moves vertically (jump, crouch, stairs) and then goes stereo at once. The
goal is stereo as soon as the weapons track, and weapons tracked as early as the hands. A new
ticket and branch, with a plan for review before code. VR-94 (issue A) and VR-95 still wait.

## Earlier (2026-09-13, session 38): VR-93 done behind levers, PR open. VR-80 logging next

**A pause, a menu or a book no longer relearns the weapons (VR-93).** Every exit from
GAMEPLAY used to run the level-load transition: weapon contracts and the candidate list were
dropped and a ~500 ms UI rescan was queued, so each resume froze the view for half a second
and then relearned the weapons for another third. Now a menu over a live pawn - and a book,
which reads LOADING - SUSPENDS those records, and the first script tick after it validates
every retained object against a freshly built live-object table (class and FName) before
anything uses it. NO_PAWN, a different pawn, the load-game screen, or leaving for anything
else drops everything as before. A kept resume queues no rescan.

| Lever | Shipped | Installed on the test PC |
|---|---|---|
| `[Hands] AttachKeepOnMenu` | 0 | 1 |
| `[Hands] AttachKeepOnNote` | 0 | 1 |
| `[Menu] UiKeepOnMenu` | 0 | 1 |
| `[Menu] UiFlags` (read-only reporter) | 0 | 1 |

Headset, builds 190-193: pauses and books resume with the first DOUBLE 14-41 ms after
GAMEPLAY (was ~530), zero re-adoptions; a save load from the pause menu drops correctly.
Measured on the way: a respawned pawn keeps its FName, so FName alone is not an identity
(TRAPS). The flag reporter resolved all 13 script-declared screen flags; `m_bNoteVisible`
is verified, the rest are unexercised (GAMEPLAY_STATE section 9).

### Found and filed, not fixed

- **VR-96 (High)**: the garbage collector crashes on a float 1.0 stored in an object
  reference (`Dishonored.exe+0x65894`), three runs since 09-11, the last during a takedown.
  Our 1.0 writers are listed on the ticket.
- **VR-97**: while `c5` reads zero, a re-arm's ring skew goes uncorrected for seconds.
- **VR-80, sharpened**: after a note closes, the passes' cameras can come out inverted
  against their eye tags and the c5 arm then starves the left eye for 10 s or more
  (FLICKER_REFERENCE 3.15). Not caused by VR-93.

### Next

VR-80 logging on `claude/vr-80-note-exit-eye-trace`: a per-present record of (ring tag, the
eye the writer applied, `c5`) from the first resumed pair, to say whether the writer or the
ring is off. Then issue A (VR-94, the world ghost on a dropped frame), which waits for the
VR-93 merge. VR-95 is still open (the still-flicker A/B).

## Earlier (2026-09-13): VR-91 fixed, VR-95 diagnosed. Blink (VR-36) merged earlier

**Rolling the head no longer slides the view sideways (VR-91).** The neck arc was built
from the head's full rotation INCLUDING ROLL, while the yaw-only reference it is subtracted
from is not, and `[Neck] Mode=cancel` negates the difference - so a 30 degree left roll
moved the rendered camera 17.6 uu RIGHT and a 34 degree right roll moved it 19.3 uu left.
The tracked head does the opposite and correct thing (7.1 uu left when rolled left), so the
arc was inverting the real motion and more than doubling it. It is the VR-78 lesson on a
second axis: cancel removes the ENGINE's neck arc, the engine's is a PITCH arc, and
cancelling one that was never there subtracts a real motion twice. Fixed by building the arc
from a roll-free frame; the neck term went to exactly 0.00 and the residual to 0.51 and 1.19
uu. `[Neck] RollArc=1` is the A/B back to the fault.

**The instrument is the reusable part.** `z_account` gained a ROLL mode (`[PosTrack]
ZAccountRoll`, `camera zaccount roll on|off`) that bins by head roll and reports the LATERAL
terms - render, eye, position, tracked head, neck - and names the owner. The pitch mode
REJECTS any sample rolled past 12 degrees, so it was structurally blind to this whole class;
a host check asserts that rather than describing it. 42 checks, and the two that matter are
the ones that CLEAR us: an eye-only fault and a wrong-signed head both leave the position
residual FLAT.

### VR-95 is diagnosed, its correction ships OFF, and it has an open regression

Arms and weapon jump sideways by about one IPD for a frame, in the LEFT eye only, during a
fast head roll. Nine V-marker episodes, 90 flagged presents, and **every one reads "eye R but
tag L, decision S"** - not one the reverse. The classifier reads the eye from the right-axis
jump in the hand's LocalToWorld translation; a jump under 0.45 IPD is `S`, which HOLDS the
previous eye. `S` means "too small to tell apart", not "the same eye". Head roll adds a drift
to every jump - it moves the hand AND rotates the axis the jump is projected on - and because
the jump is asymmetric (-5.60 entering right, +5.09 entering left, against a 2.84 band) only
left-entering presents are ever misread.

`[Hands] PaletteEyePredictToggle` predicts the toggle instead of holding, capped at two in a
row. It took mismatches from 90 to 1. **It also introduced hand and weapon flicker while
standing still, which none of these counters can see**, so it ships OFF and is disarmed in the
tested install. The live A/B is F10 Hands; the next measurement is whether the still-flicker
follows the checkbox.

**`docs/dishonored/FLICKER_REFERENCE.md` is now the mandatory first read for anything called
flicker**, and CLAUDE.md says so. Sections 3.11 and 3.12 carry this work, including three
instrument failures worth more than the fix: a cross-check joined at the wrong present (`pres`
instead of `pres + 1`, a near-total inversion that means nothing in an alternating stream),
the marker's tag reference being FITTED to maximise agreement with the classifier it judges,
and `tools/palette-eye-host.ps1` not having COMPILED since VR-76 added `MfOpen` - so its nine
checks had not run in weeks. All fifteen pass now.

| Lever | Shipped | Live |
|---|---|---|
| `[Neck] RollArc` | **0** (the fix) | ini; 1 restores the measured fault |
| `[PosTrack] ZAccountRoll` | 0 | `camera zaccount roll on\|off` |
| `[Hands] PaletteEyePredictToggle` | **0** | F10 Hands, live |

### Next: two smaller issues from the Codex investigation

`docs/dishonored/FLICKER_FRAME_DROP_AND_RESUME_PLAN.md` holds a completed read-only
investigation with ranked hypotheses and a patch sequence. **Issue B first** (a same-world
menu is processed as a destructive load: `GameStateTick` calls `WaInvalidateContracts` and
`FpInvalidateCandidates` on any `wasGameplay && !nowGameplay`, and the same transition
schedules a UI rescan measured at 516 ms). **Issue A second** (a one-frame world ghost on a
dropped frame; 33 gameplay frame gaps of 48 to 538 ms, 27 of them in xrEndFrame, with no
marker joined to a seen event yet). Read the plan's restart checklist before touching code.

Open: VR-95 (the still-flicker regression and the unverified roll fix), VR-89, VR-87, VR-86
(shelved), VR-85, VR-75, VR-77, VR-79, VR-80, VR-81, VR-32, VR-58, VR-92.

## Earlier (2026-09-12, late): VR-78 fixed and merged. Next is the animation handoff

**VR-78 is fixed.** Crouched, the engine does not pitch its camera about a neck at all, so
`[Neck] Mode=cancel` with the standing pivot subtracted an arc that was not there: looking
down pushed the view back and up, looking up forward and down. `[Neck] CrouchPivotBelowM`
/ `CrouchPivotBehindM` now hold the crouched pivot and ship at 0. Measured twice on the
headset by a new instrument and judged fixed there. Numbers in ENGINE_NOTES, "Crouched,
the engine has no neck arc".

**The instrument is worth more than the fix.** `[PosTrack] ZAccount` (`z_account.h`,
default off, `camera zaccount on|off|reset|status`) pins each draw's camera write to its
eye tag and joins it to that present's c5, then reports per stance and pitch what moved
the rendered camera beyond the tracked head, term by term, with rejection counts. It
found the crouched fault in one run. `tools/zaccount-host.ps1` holds its 30 checks.

**VR-86 is shelved with an answer**: the game's crosshair setting writes no ini at all.
It lives in the checksummed, Steam-Cloud-synced `OPTIONS.sav` (profile setting id 99),
so there is no installer key to write. Details on the ticket.

Found and filed, not fixed: **VR-87**, the eye ceiling's final cap trims tracked head
height above the reference by 3 to 7 uu in both stances.

### Next: detect full-body animations and hand the hands back to the game

Takedowns and other scripted full-body moves should play with the game's own hands and
weapon instead of ours. Step one is a reliable flag for "an animation owns the body right
now". Plan for review in `docs/dishonored/ANIM-HANDOFF-PLAN.md` on its own branch.

## Earlier (2026-09-12, end of session): VR-Main is clean. Next up is minor bugs

`VR-Main` at `33e1e60c`. Four merges landed today and nothing is in flight.

**Read `CLAUDE.md`'s "Resources you already have" section before deriving anything.**
It is new, and it is the index to the instruments this project has paid for - the
class-to-vtable walk, UE3's native registration table (a function NAME to code), the
runtime property resolver and `propwatch`, the decompiled script dump, and the map of
the game's own config folder. Several sessions have re-derived things one of those
answers in a single command.

### Landed today

| | |
|---|---|
| VR-82 | Pistol shots follow the controller laser. Its own fire seam, traced and hooked. Headset confirmed |
| VR-83 | The F10 hand/weapon size slider is reachable again; it was gated behind an unrelated probe that resets on every level load |
| VR-84 | The ini save was writing `AttachRigRadius` from the wrong buffer, which is what silently broke weapon tracking twice. `ModelScale` ships at 0.85. The golden ini check could never fail, and now can |
| VR-85 | Shelved with an answer, not abandoned. The tooling and the engine findings shipped |

### Next session: minor bugs

First is **VR-78** - crouched, looking down raises the camera and looking up lowers it.
An inverted pitch contribution that only appears in the crouched path; `area: camera`,
needs a headset to judge. Start from `crouch.cpp` and the camera seam, and note that
VR-7 (deep crouch climbing the view) and VR-55 (sliding leaves the view taller) are
neighbours in the same code - check whether one fix covers more than one ticket before
writing anything.

Also queued: **VR-86**, turn the game's crosshair off from the installer so a fresh
install needs no menu visit. The game's own settings can already do it; the job is
finding which key the menu actually writes (diff the config folder before and after
toggling it) and applying that from `setup-game-ini.ps1 -VRBaseline`. Do the diff
first - writing a plausible key the game ignores is TRAPS section 1.

### Open and untouched

VR-75 cutscenes in stereo, VR-77 single-draw bursts, VR-79 one-eye occlusion culling,
VR-80 note-exit flicker, VR-81 the DesktopEyeSource F10 control, VR-32 independent
hand/weapon scales, VR-58 verify the numpad adjust in a headset. The crouch controls
are still hostage to the SkelControl probe gate, noted in VR-83.

## Earlier (2026-09-12): VR-85 SHELVED with a definitive answer; the tools ship

`claude/vr-85-interact-head-or-controller`, off VR-Main at `55cdb2b8`.

**The feature is shelved on purpose, not abandoned in confusion.** Aiming
interaction from the controller cannot be done by writing the engine's focused-actor
field, and that is now measured rather than suspected.

What the branch delivers:

* **`tools/ue3-natives.py`** - the class-to-vtable walk that three seams were derived
  by hand, plus UE3's **native function registration table**, 2554 name-to-thunk
  pairs. That is a route from a function NAME to code where only class names were
  searchable before. It re-derives the published crossbow numbers before answering
  about any new class and refuses on a mismatch.
* **`PropWatch`** (`ue3/prop_watch.cpp`) - the reverse of `FindPropOffset`: it finds a
  property by watching which one CHANGES when the name is what is missing. It found
  its target on the first run. Ships OFF.
* **`docs/dishonored/GAME_CONFIG_MAP.md`** - the game's own 21 config files, what each
  governs, and the debug instruments it ships.

What was learned, all in ENGINE_NOTES:

* The focused interactable is `DishonoredPlayerController::m_pCrosshairActor` (+0x69C)
  and `m_pCrosshairHighlightActor` (+0x6A0).
* **Those fields are a RESULT, not an input.** A write-and-observe experiment read back
  `wrote 1, survived to the next tick 0` - the engine recomputes them after our tick,
  every tick, so nothing downstream can read what we write. Aiming interaction has to
  happen at whatever COMPUTES them, and that writer is unfound.
* Interaction has no script surface at all: no exec in any of the 2554 registrations.
* Selection is a single winner from a narrow trace, about a hand's width of tolerance.

Three faults of mine are in TRAPS, each with the reading that would have caught it
sooner: a read-only probe that still cost the frame budget, a liveness guard that could
not detect the freeing it existed to catch (it crashed the game twice), and the golden
ini check that could never fail.

**Next**, when this is picked up again: find the writer of those two fields. The
natives table and `PropWatch` are the tools for it. Do NOT re-arm
`src/legacy/interact_focus.cpp`.

## Earlier (2026-09-12): VR-82/83/84 merged and confirmed

`claude/vr-85-interact-head-or-controller`, off VR-Main at `55cdb2b8`.

**The goal**: interact with whatever the head OR the controller is pointing at, so a
player does not have to line their head up with something they are already pointing a
hand at. The controller ray already exists and is proven (VR-57, VR-82); this is about
where the interaction query points.

**Status: research, no code.** What is established, all in ENGINE_NOTES:

* Interaction is **entirely native**. The script dump carries declarations only, and a
  sweep of all 2554 native exec registrations finds no interaction or use function on
  the player controller - not even an input handler. There is no script surface to hook.
* `DisInteractableInterface`'s `CanInteractParams` carries `m_DisTraceFlags`, so
  selection is a flagged trace; `[Engine.PlayerController] InteractDistance=512` is its
  length.
* The usable-HIGHLIGHT path is located: the cheat `ToggleUsableHighlight` flips bit
  `0x400` at cheat-manager `+0x5C`, and a `.text` sweep finds exactly two readers, both
  in one function around `0x0060E4AF`.

**The remaining step** is the writer of the current-usable field that highlight path
reads. That is the seam.

New capability that came out of this and is worth more than the ticket: the image
carries UE3's **native function registration table**, 2554 name-to-thunk pairs, giving a
route from a function NAME to code where only class names were searchable before.
`tools/ue3-natives.py` keeps both that and the class-to-vtable walk, and **refuses to
answer about a new class unless it first re-derives the published crossbow numbers**.

Also landed this session, all merged and headset-confirmed: VR-82 (pistol shots follow
the controller), VR-83 (the F10 size slider is reachable again), VR-84 (the ini save was
corrupting `AttachRigRadius`, and the golden check could never fail). `ModelScale` ships
at 0.85. `docs/dishonored/GAME_CONFIG_MAP.md` maps the game's own config folder.

**Next**: find the current-usable writer, or take the cheaper route first - the game
ships an interactable debug box, a usable highlight and an interaction debug page, none
of which are in use yet.

## Earlier (2026-09-12): the pistol's fire seam is hooked, since CONFIRMED (VR-82)

`claude/vr-82-pistol-fire-seam`, off VR-Main. The pistol's native firing routine was
traced offline and hooked at its own pre-spawn join, so pistol shots now go through the
same published ray and the same solver as the crossbow's. Built, installed, 6115 offline
checks pass. **No game or simulator has been launched: runtime behaviour is untested.**

The derivation re-walked the crossbow's route and reproduced every one of its published
numbers before trusting any pistol number. Full table in ENGINE_NOTES, "VR-82 native
pistol fire seam"; the design is `dishonored/VR-82-PISTOL-FIRE-SEAM.md`.

The one real difference from the crossbow, and the reason this was not a one-line change:
the pistol derives its spawn POSITION from the aim direction, standing the bullet off by
`m_fBulletSpawnDistance` (150 uu). Position and direction are one decision, so the hook
reconstructs the engine's own origin, aims from there, and rebuilds the standoff along the
corrected direction. Two traps are recorded in TRAPS: the join cannot go where it looks
like it should, because a later native call takes the direction local by address; and a
refusal line that prints a number must be checked against the case it explains.

`[Aim] FireFromHand` still switches both weapons at once - `fireaim on|off` on the seam,
or the F10 Aim checkbox, now labelled for both. The per-shot log line names the weapon.

**Next: the first headset run.** One question only - with the pistol equipped, does a shot
land on the laser dot. The log line to read is `fireaim #n pistol:`; a run where the hook
declines prints the reason and the numbers instead.

## Earlier (2026-09-12): MERGED to VR-Main, crossbow done

PR #37 and #38 are merged (VR-Main at `866e5c43`). The crossbow aims from its own
barrel and the work is on the main line; both branches are done.

**The single next job: pistol shots follow head aim.** The native fire hook installs
for the player's CROSSBOW firing context only - it says so on installation,
`fireaim: INSTALLED at 0x00c38bbb -> ... player crossbow firing context only` - so no
guide can move a pistol shot. The pistol's GUIDE is already correct, since it shares
the latched bolt axis, so this is purely the fire seam: trace the pistol's pre-spawn
join the way `0x00C38BBB` was traced for the crossbow, verify its bytes, and aim it at
the same published endpoint through the same solver. Everything downstream already
exists and is tested.

Start from `dishonored/VR-57-MODEL-RAY.md` section 7 and `ENGINE_NOTES`'s VR-57 section.

## Earlier (2026-09-12, end of branch): the crossbow aims from its own barrel (VR-57)

`claude/vr-57-laser-to-bolt`, off `claude/vr-57-crosshair-on-hand-ray`. Headset
confirmed by the tester: the guide sits on the crossbow's bolt line, the bolt lands on
it for all three bolt types, it follows the numpad hand trim, it holds across weapon
switches and reloads, and it survives a relaunch including a save loaded with the
pistol out. Full write-up in `dishonored/VR-57-MODEL-RAY.md`.

What ships ON by default now, with the tester's own calibration baked in as the
default: `[Aim] ModelRay`, `FollowHandTrim`, `FireFromHand`, `[Crosshair] Dot`, and the
measured `[Hands] ModelAxisL*` paired with the grip it was measured against. A first
launch therefore has a correct guide rather than none. `kConfigVersion` is bumped to 11
so an existing install picks the new defaults up. The deliberate exception to
default-OFF is argued in the feature doc.

Two earlier branch items also landed: the hand trim's rotation range went from 45 to
180 degrees per axis, with one shared limit so the ini load can no longer clamp back
what the numpad tuned; and `c5` is now read in world terms, having been found to carry
the camera position NEGATED, which had put every written aim point 348 m away.

Next, and the reason for the next session: **pistol shots still follow head aim.** The
native fire hook installs for the player's crossbow firing context only, so nothing
the guide does moves them. Its fire path needs tracing and hooking on its own address.
The pistol's GUIDE is already correct - it shares the bolt axis - so this is purely the
fire seam.

Also open and untouched: the aim assist's pull toward the game's own solution, a
measured axis for weapons with no usable projectile (needs vertex subsampling and
multi-bone handling past the reader's 1024-vertex limit), and the VR-77/78/79/80/81 and
VR-75 items carried from before.

## CURRENT (2026-09-12): native crossbow hand-aim candidate (VR-57)

Implemented the byte-verified native pre-spawn firing hook. It aims the existing
spawn position at the controller dot endpoint before the engine builds rotation
and velocity. No camera/body rotation swapping or retained projectile pointers.
The x86 build and offline geometry/bridge/compositor tests pass. The candidate is
installed with FireFromHand on, legacy/cache drive and diagnostic probes off.
No game or simulator was launched; runtime behavior remains unverified.

Next: user play when ready; weapon alignment, native traces and assist are separate.
The complete implementation and evidence are in
[dishonored/VR-57-NATIVE-FIRE-HANDOFF.md](dishonored/VR-57-NATIVE-FIRE-HANDOFF.md).

## Earlier (2026-09-12, later): the projection layer is aligned; the ray is the suspect (VR-57)

Build 111 added a HEAD-anchored control dot - straight ahead of the located view,
no controller anywhere in it - and the layer-alignment numbers beside it. One
headset launch settled all three tests in `dishonored/VR-57-AIM-PIPELINE.md`
section 7, and **all three came back clean**:

* the control dot sits on the game's own crosshair (tester),
* the claimed fov equals the fov the game rendered (108.07 vs 108.07 deg, tan
  1.3780 vs 1.3780, src=readback),
* the layer's pose tag equals the located pose with the head still (0.00 deg,
  0.000 m at poseLag 2).

So the compositor quads and the rendered world DO share a frame. The leading
hypothesis is falsified and the controller ray is back under suspicion. The
control dot is kept as the calibrated reference.

Two side findings, both consistent. The game's crosshair LAGS the head while the
control dot does not, because the crosshair is painted into an image submitted
with a pose two generations old and a 2D HUD element cannot be reprojected - so
the game's crosshair is only a valid reference while the head is still. And bolts
landed at that crosshair, which is expected: `DriveFromHand` was off for this run
on purpose, since it moves the reference.

One mistake, recorded in `TRAPS.md`: the control dot shipped at two distances with
the written prediction that they would appear concentric. Two points at different
depths on a cyclopean ray cannot coincide in either eye - at the measured 63.2 mm
IPD they split by 1.0 deg, outward in each eye, which is what the headset showed.
The near dot is removed.

Next: build 112 draws the controller dot and the control dot together at 8 m. The
tester sights along the controller at the control dot and the separation is the
pose error, measured against a reference that has now been confirmed. The beat
prints the prediction in degrees (`DOT APPEARS az/el`) so the report can refute
it. The two remaining candidates are a genuinely wrong aim pose and a comparison
made against the separately-rotated weapon MODEL rather than the controller; the
second is the stronger and is what 112 separates.

## Earlier (2026-09-12): hand aiming - the ray is right, the drawn beam is not (VR-57)

The full pipeline, every measurement and the leads are in
`dishonored/VR-57-AIM-PIPELINE.md`. Read that first; this is the summary.

Built on `claude/vr-57-crosshair-on-hand-ray` (builds 100-110, nothing merged):
one aim ray from the runtime's AIM pose, a dot and beam drawn from it as
compositor quads, a read-only probe of the game's own aim-assist cache, and a
lever that WRITES that cache from the ray so the shot follows the controller.

What holds: the game computes the fire direction itself, and its equipped
weapon caches the assist result per tick (`DisItemContext_FireCrossbow`,
`m_CachedAimAssistPos` at +0x00d0) with a direction that tracks the view
(dot +0.998) and a projected screen point. Writing it moves the shot; 485,083
writes, zero refused. The ray's mapping into game axes is exact - the angle off
the head in XR equals the angle off the view in game (30.0/30.0, 25.3/25.3).

What does not: the tester reports the beam sitting about 45 deg left and 10-20
deg up from where the controller points, while the same ray measures near
straight ahead when the arm is extended and still (az -12, el +5 over 36 steady
samples). The dots are placed at the controller's own position along that ray,
and the layer budget draws all 10 points, so the suspect is the alignment
between compositor quads and the game's RENDERED WORLD (the claimed FOV, the
submitted view poses, the eye tag) rather than the ray. Shots are also pulled
back toward the game's crosshair from either side, which is the aim assist
clamping how far it will move a shot.

Next, in order: a head-anchored control dot (if that also sits wrong, the ray is
exonerated and the fault is the layer), the FOV audit against the game's own
rendered FOV, then the submitted view pose. Then the assist clamp, then the
barrel axis for the model. Fresh leads to mine in the decompiled scripts are
listed in the pipeline doc, section 9.

The weapon model is separately rotated from the controller; SHIFT+F7 improved
it and the numpad adjust finishes it. A crash after a pause menu is recorded but
not attributed: the same d3d9 signature appears in runs from 2026-09-03 and
2026-09-09. The write is now gated to gameplay regardless.

## CURRENT (2026-09-11): VR-57 visual controller guide installed, user test pending

Step 1 on `claude/vr-57-crosshair-on-hand-ray` publishes a fixed-distance dot and
four beam markers from one validated XR aim-pose ray. F10 Aim and `crosshair`
commands control both. The runtime adds explicit points after held-projection
recovery, with freshness, budget and submission-result telemetry. MotionAim stays
off; shots, native reticle, Blink and hand placement retain existing behavior.

Installed `vr33-hands-working-100-geb8f27d4-dirty`, SHA256
`F1164C5F354F9501DE4AD612EA37A5EB5AE126A6C1ABFD86BBA7A807CF1C9DD4`.
Only new Crosshair settings were appended: dot/laser on, left hand, 8m, 0.5deg.
Tree dot/laser defaults are off. Release build, 70,234 aim/compositor host
assertions, mirror regression, hand/weapon tests and exports passed. No game or
simulator launch. The guide appears only in the headset compositor, not the
D3D9 desktop mirror. Source changes remain uncommitted for review.

Next: user tests appearance/controller following, then static barrel alignment
on a separate launch. The old failed shot direction is not yet isolated to pose
choice; no measured weapon-axis comparison is invented. Full review answers,
installed hashes, backups, instrumentation and testing:
`dishonored/VR-57-CODEX-HANDOFF.md`. Trace and actual projectile aiming come later.

## Earlier (2026-09-11, later): the last hand/weapon flicker is fixed (VR-76)

The one-frame rightward jump of the hands and weapon is gone. The tester played the
prologue through to the hub on `vr33-hands-working-95-g18d39cee-dirty` with
`[VR] DesktopEyeSource=draw` and saw no remaining jump. That run's log: 106
`desktopeye:` windows, 1,400 single-draw ticks, 1,397 raw leaks counted by the
old-policy shadow, one right frame shown under `draw` (startup warmup), zero copy
failures. The tree default is now `draw` (module, loader fallback, generated ini,
golden); `tag` stays as the A/B. Merge to VR-Main was authorized for PR #35
(VR-73) and the VR-76 PR together.

Found in the same run, ticketed and deliberately not fixed: VR-78 (crouched, head
pitch moves the view the wrong way vertically), VR-79 (an object hidden from one
eye vanishes from both), VR-80 (after closing a note, a rare sustained both-eye
flicker; the log shows the tag stream resuming one-sided). Still open: VR-77
(single-draw bursts, the trigger of the mirror leak and a source of held frames),
VR-75 (cutscenes in stereo, not started), VR-74 (main-menu view under the ground).

Next: pick from VR-80, VR-78, VR-77, VR-79, VR-75. The `V` marker stays in the
build for the next reproduction.

## Earlier (2026-09-11): VR-76 mirror candidate installed, user testing pending

The desktop pin can now use the current draw's eye instead of a delayed capture
tag. All runtime present paths, including pairHold and zero tags, reach the
post-capture hook. Unknown frames hold briefly; surface lifetime and source
switches invalidate the pin. New V-marker fields and window counters report
current/delivered identity, old-policy raw leaks, successful actions and failures.

Installed build `vr33-hands-working-95-g18d39cee-dirty`, SHA256
`264038D60BFBF6098328B8A4F034DD9BFF5EF4B9A6B0061917EB61FEFE1BC489`.
Only installed ini addition: `[VR] DesktopEyeSource=draw`; tree default remains
tag. Release build, 79,339 policy assertions, 72 copy-module assertions, exports,
frame tests and standalone simulator self-test passed. No completed in-game
simulator or headset validation. All further game launches/tests belong to the
user. Earlier launch attempt was cancelled; logs preserved in local build data.

Next: user-run mirror test with real bursts and legacy shadow leaks, then a
separate headset-only verdict. Keep VR-76 open until then. VR-77 burst generation
and VR-75 cutscene changes remain separate. Full Claude handoff, review
corrections, backup paths and installed hashes:
`dishonored/VR-76-CODEX-HANDOFF.md`. Source changes are uncommitted in this branch.

## Earlier (2026-09-11): the intro boat is fixed end to end (VR-73, PR #35)

Both VR-73 fixes are headset-confirmed: no fall at the dock (`671ea554`), and the arrival's
stuck mono quad now clears by itself (`d0697d25`) - the arrival toggle turned out not to
start a cinematic at all (every engine lock read 0), so the latch clears after its 2 s
no-lock window; that window is the brief mono spell still visible. A real cutscene just
after (locks set) stays on the head-locked quad until it ends, as designed.

Next: VR-75 (cutscenes in stereo, mono only for menus; shorten the no-lock window), not
in PR #35. Still owed from VR-73: a weapon view model's owner verdict with a weapon drawn,
and a crawl under furniture (38.23 crouch wall). VR-74: the main-menu view under the ground.
The next session is a residual one-frame flicker of the hands and weapons (a rightward
shift, clearest on the desktop mirror) - an investigation with a log marker key.

## Earlier (2026-09-10, night): boat fall FIXED in the headset; arrival mono fix pending (VR-73)

The ownership fix (`671ea554`) is headset-confirmed: no fall, and the log shows the boat
listed FOREIGN and never CLEARED. The same arrival then exposed the cinematic latch stuck
ON (mono quad until a pause). Clearing it on `bCinematicMode` alone failed in the
headset (the flag never read 1). The current build reads four engine input locks on the
toggle's own object and clears the latch after they release, or when none appears within
2 s of the toggle; unverified, not yet run (no headset session left on 2026-09-10).

What the arrival is, from the tester: not a camera cutscene but a scripted moment where the
game holds the player still - movement (left stick) is locked by the game, looking around
(right stick, head) still works - and then hands control back. So the next run should
show one of the movement locks (`bIgnoreMoveInput`, `bCinemaDisableInputMove` or
`m_bInputIgnoreInput_Cinematic`) reading set at the arrival and clearing when control
returns, followed by `cine: latch cleared - the engine's input locks were set ...`. If
instead the log says no lock was set within 2 s, the movement lock lives somewhere these
four fields do not cover. Open design question for after that run: the latch shows the
head-locked mono quad for the whole scripted moment; a look-around moment may be better
kept in stereo.

Next run: the prologue again without pausing - pass is stereo and movement returning by
themselves after the arrival, and still no fall. Still owed: a weapon view model's owner
verdict with weapons drawn, and a crawl under furniture (38.23 crouch wall).

## Earlier (2026-09-10, night): the intro boat fall is the hand collector (VR-73)

New game, prologue: at the top of the water lock the player and the NPCs on the boat drop
through it when the arrival script destroys the lift actor; the NPCs die and the mission
fails. It reproduces on both machines and not with the mod off. The hand system's
candidate walk reaches the boat's own mesh (`EmpressBoat_anim`) through the pawn standing
on it and cleared its `BlockActors` mid-ride in every failing run - a regression from
`a6e00a6f`, which made that walk re-run from the script tick. Branch
`claude/vr-73-boat-dock-fall` restricts every candidate write to components whose engine
Owner chain reaches the player, and restores only what was written, to its prior value.
Details and the four falsified leads: ENGINE_NOTES, VR-73.

Next: one headset run of the prologue on this build with the tested ini unchanged. The
log must show both Owner offsets found, the boat listed as FOREIGN and never CLEARED, and
the body mesh and view models listed as the player's; then the arrival keeps everyone on
the boat. Watch crawling for a returning crouch wall. The main-menu camera dropping under
the ground (`camera/clamp-rebase` with a -4154 uu offset) is a separate fault, not fixed
here.

## CURRENT (2026-09-10, late): the shipped defaults match the tested configuration (VR-72)

A fresh install of VR-Main resolved a different, unstable configuration from the
headset-confirmed machine: world pose lag 1, both automatic A/B experiments on, the
bone palette and weapon placement off, motion aim on and gamepad-only on. Branch
`claude/vr-72-fresh-install-defaults` sets the generated default ini and the loader
fallbacks from that machine's ini (226 keys added, 5 values and 15 fallbacks
changed); the regenerated golden resolves every key to the tested value.
`tests/golden/known-good-2026-09-10-stability.ini` records the ini itself. No
config-version bump, so existing inis are untouched (VR-11).

Not in the headset yet: the build carrying these defaults has not been launched. The
installed DLL is still the confirmed `417bfad9` diagnostic, whose ini already holds
these values.

Also reported: on the other developer's VR-Main build the player and nearby NPCs fall
through the intro boat as it docks. The confirmed configuration is not reported to do
this. Not yet ticketed.

## CURRENT (2026-09-10): PR #33 stability fixes, merge authorized

The final controlled build `417bfad9` is headset-confirmed for head-turn stability,
weapon lock through swaps, and the downward-motion flicker fix. PR #33 carries
the same two corrective changes on current VR-Main, plus regression tests and
the full evidence/handoff. The user explicitly authorized completing the PR and
merging it to VR-Main. This supersedes the earlier plan to hand off the merge.

The installed DLL remains the confirmed `417bfad9` binary. Only `[MotionAim]
Enabled=0 was applied afterward to restore native reticle aiming. No global
aiming default changed; the next reticle shot remains unverified. The source
integration builds and passes host checks, but it is not the exact historical
binary used for the headset verdict. Preserve that distinction when packaging.

Next: verify the reticle shot on next launch, and verify any newly built mainline
package against the preserved working binary/configuration. No release is declared.
The session handoff and local backup manifest remain the complete provenance.

## CURRENT (2026-09-10): stability confirmed; reticle aim reset; handoff ready

The tester confirmed `417bfad9` eliminates the remaining downward-motion flicker.
Its log identifies the correct build and records eight `camera/clamp-rebase`
executions. Head-turn stability and weapon swaps were already confirmed on its
parent. Brief startup settling remains accepted.

The final crossbow complaint is supported by logged projectile redirection from
the view ray to an upward/backward hand ray. The installed motion_aim.cpp is
unchanged from the earlier baseline. Only `[MotionAim] Enabled=1` was changed to
0 in the installed ini, restoring native reticle aiming on next launch. Exact
one-byte diff verified; the confirmed DLL is unchanged. Reticle shot verification
is pending. No rebuild or further gameplay-code change was made for aiming.

Full handoff: `dishonored/SESSION_HANDOFF_2026-09-10.md`. It includes source/PR
provenance, test results, the confirmed binary/configuration, backups, and the
important distinction between the installed diagnostic and the later integration
branch. PR #33 already exists; finishing its review/integration and mainline merge
is handed over. This final handoff is local; no further push or merge was performed.

## Earlier candidate (2026-09-10): downward clamp before its visual result

The `b3a1ff46` headset run confirmed stable world/weapon head turns and stable
weapon attachment after startup, including equipment swaps. The remaining
flicker was specific to downward character movement (crouch, descending slopes,
falling); tracked head/controller vertical movement did not reproduce it.
Brief startup settling is accepted for this task.

Installed candidate `vr33-hands-working-60-g417bfad9` starts directly from that
confirmed build. It fixes one interaction: FovLeverApply's Z-only ceiling write
could invalidate the camera writer's whole-vector ownership check and accumulate
old eye offsets in X/Y. The clamp now retains that ownership only for an exact
previous write to the same camera and field. Ceiling/easing, eye selection,
pose lag, weapon contracts and settings are unchanged. Details and counterexamples:
`dishonored/DOWNWARD_CLAMP_REVIEW.md`.

Nineteen production-function clamp checks pass (the old raw clamp fails six),
as do the nine eye checks, 88 frame checks, release build, lint and export check.
The integration branch also builds. The installed candidate is the controlled
historical-base build, not the later mainline integration. Its SHA-256 is
`1781FDAA987233F5E2E9A0A448A4CB47E1A4AE595FD2D0A99AF1C9C7C722D762`.
Installed hash and x86 machine type were verified; ini bytes are unchanged.

The confirmed DLL, ini and log are saved in `build/vr69-bisect/before-downward-clamp`.
The new game run is pending; no simulator game run or headset result is claimed.
Next: compare downward movement with ascent/head turns/equipment swaps, and read
the bounded `camera/clamp-rebase` evidence. Restore `b3a1ff46` if any previously
working behavior regresses. PR #33 remains draft; nothing is merged.

## Earlier candidate (2026-09-10): eye-decision restore before its visual result

The first comparison preserved world/weapon head-turn stability but reproduced
persistent stereo weapon flicker, outward in each eye, with occasional longer
displacements and apparent size changes. Its log keeps three active weapon
contracts but reports 8,538 unknown-eye placement evaluations out of 143,598,
with zero large-step ambiguities. That narrows the next controlled change to
the eye decision rather than assuming the weapon repeatedly lost its contract.

The installed second comparison is `vr33-hands-working-59-gb3a1ff46`: the first
candidate (`08cbb368`, source equivalent to its earlier dirty build) plus the
pre-regression `MpEyeForPresent` and `MpWorldTarget` bodies. Candidate recovery,
contract lifecycle, capture, runtime and the clean weapon-pose-lag port are
unchanged from the first run. The inactive eye experiment remains in legacy
source. Ini/settings are unchanged; the game was not launched by the agent.

The real production eye-decision function fails five host assertions before
the restore and passes all nine afterwards, including delayed stereo draws
after a script-side single-draw tick. All 88 existing desk tests, normal and
legacy release builds, and the x86 export check pass. The normal DLL was staged
before the legacy check and its installed hash verified. This verifies the code
path, not the final visible result.

The first-result log and ini are preserved under `build/vr69-bisect/first-result`;
the first DLL and original pre-comparison DLL remain backed up. Full provenance
and next decision gates are in `dishonored/LOCKON_REVIEW.md`. Next evidence:
whether the outward ghost/displacement stops while head-turn stability stays
correct. No branches were removed and VR-Main was not merged.

## Earlier comparison (2026-09-10): setup before the first result

VR-69 is now following the three-system comparison in `dishonored/LOCKON_REVIEW.md`.
The installed candidate is `9fe2af45` plus the exact clean weapon-pose patch
`22f275ba`: candidate/contract work from PR #26, with Hands PoseLag=2 and the
existing Pace Lag=2 setting. No eye-decision changes were added. Release/x86
build, all 88 desk cases, nine exports and lint passed. The DLL hash was checked
after installation; the installed ini was not changed. The game was not launched.

The previous DLL, ini, launch file and available logs are preserved under
`build/vr69-bisect/before-first-candidate`. Candidate source, patch and manifest
are under `build/vr69-bisect`; these local artifacts are not committed. Next
evidence needed: whether the weapons settle and stay settled while playing,
whether flicker returns on swaps/loads, and whether world or weapon judder occurs.
The final combined fix is not established by this build.

Important review correction: PR #27 also contains `fc343404`, which preserves
live stowed weapon contracts. Weapon lock work is spread across PRs #26 and #27,
so a clean first comparison is not proof that swap retention is complete. PR #32
was already closed. No branch was deleted and VR-Main was not changed. The
integration branch is `claude/vr-69-combined-stability`; the first diagnostic
candidate lives in an isolated detached worktree.

## CURRENT (2026-09-09, evening): the WEAPON judder is fixed too

**`[Hands] PoseLag=2`.** Headset-confirmed by a reversing A/B/A/B. The tester's
verdict was that it fixed the weapon judder completely and the game is smooth
even with the throughput deficit still open.

Installed and known-good: 2750x2850, `VirtualMode=1`, `[Pace] Lag=2`,
`[Hands] PoseLag=2`, 80 Hz, spacewarp off.

### The fault

The engine renders a frame from the head **two locate generations back** - the
same fact that put `[Pace] Lag=2` in place for the world. `MpDriveTick`
normalised the hand against the **freshest** head, so the hand was expressed
relative to one head and planted in a view built from another. The residual is
two generations of head rotation.

`Lag=2` did not create it. It **revealed** it, by taking the world's judder away.

### How it was found, in order

1. **A motion matrix, no code.** Head rotation showed it; a stick turn did not.
   That last row is the discriminator: a stick turn moves the game camera without
   moving the head, so controller sampling and viewmodel animation are excluded.
2. **The first instrument was near-circular and returned a clean zero.** It
   compared the head stamped at the pose consume against the head the camera
   write snapshotted - both derived from the same consume. Generation gap 0 on
   every frame. The negative result is what named the correct pair: fresh versus
   **RENDERED**, not fresh versus fresh.
3. **A lag finder settled it.** Mean `|dB - dHead|` over 4085 moving frames:
   lag0 1.190, lag1 2.406, **lag2 0.119**, lag3 2.405, lag4 1.192 deg. Ten times
   clear, with a clean V around the minimum. It compares **deltas only**, because
   the camera basis is game space and the head is XR space and differencing those
   is what produced two retracted numbers here.

### Performance: measured, and two candidates falsified

Open, and tracked as VR-67. What is now known:

* **The runtime's period is not fixed and nothing could see it before.** One run
  was asked for 40 fps (25 ms), the next for 80 (12.50 ms). Every change is now
  logged; that run showed zero changes.
* **At 80 Hz the app delivers 57-80 of 80**, matching the reported 60-70 in the
  hub. GPU 7.7-12.4 ms per tick against a 12.5 ms budget.
* **The worst window is not obviously pixel-bound**: 57/s, 17.5 ms tick, GPU
  12.4 ms, but render-thread R 11.6 ms + desktop Presents 4.8 ms against a
  **0.1 ms** pacing wait. CPU/driver/synchronisation is not cleared.
* **Falsified**: the FrameId render-target readback, and D3D9Ex maximum frame
  latency at 1/2/3. Both inside the noise floor on median, tail and hitch count.
* **All gameplay hitches sit in the submission tail** (`xrEndFrame`), 19 of 19 in
  the last measured run. Where a wait is observed does not name what caused it -
  that call takes a mutex and can wait on the previous submission and on D3D11
  synchronisation, so our own GPU work can be charged to it.

`docs/dishonored/PERF_PLAN_2.md` and `PERF_REVIEW_2.md` carry the full record,
including four claims made and retracted along the way.

### Next steps

1. VR-67, the throughput deficit. Start with the 57/s window's split, not with a
   resolution change.
2. Finish the instrument repairs in `PERF_REVIEW_2.md` section 11 - the GPU span
   still subtracts an interval it does not contain, and the D3D11 bridge is
   unmeasured.
3. VR-64, the weapon swap flicker. The `wa/key:` instrument has shipped and has
   never been read.
4. VR-57 the crosshair (Urgent), VR-58, VR-56.

---

## CURRENT (2026-09-09, late): the head-turn judder is FIXED

**`[Pace] Lag=2`.** Headset-confirmed. The tester's summary was that the game
feels an order of magnitude better to play, and smooth enough that synchronous
spacewarp works well on top of it. This was the oldest and most damaging
complaint in the mod.

Merged to `VR-Main`. Installed and known-good: `vr33-hands-working-65-gd89beb93`
with `[Pace] Lag=2`, `[Stereo] LagAB=0`, 2750x2850, VirtualMode=1, 60 Hz.

### Why

The pose submitted with an eye image is chosen from a history of located views by
a fixed generation offset. The old offset of 1 was calibrated against BioShock 1's
SINGLE-THREADED renderer; this game has a separate render thread and a delayed
D3D9 capture stage, so the pixels reaching the compositor are one generation
older than that assumption and the pose described a head that had already moved.

Only a PHYSICAL turn showed it because the compositor reprojects for head motion
alone. Measured by switching it live in one run: the submitted orientation sat
0.119-1.079 deg from the sample the camera consumed under lag 1, and 0.000-0.040
deg across a full twenty seconds of lag 2 at head speeds to 106.6 deg/s, then
returned to 0.47 within two seconds of lag 1 coming back. The tester felt the
same three phases in the same order without being told which was which.

### Two wrong turns worth not repeating

**An ini key that exists beats every compiled default.** Two builds changed the
default from 1 to 2 and then to 0; the installed ini names `Lag=1`, the loader
reads a default only when the key is ABSENT, and both ran at lag 1. Their results
were read as the new value failing, and pose selection was wrongly declared
eliminated. The loader now logs the effective value and whether the ini overrode
it.

**Submit cadence was not the cause.** The tester falsified it directly: buttery
smooth at 61-72 submits/s with an inconsistent presentation rate, the same
cadence that had juddered.

### Open, and honest about it

* The camera record still does not guarantee it holds the sample the camera was
  calculated from - both writers calculate from the loose globals and consume the
  coherent sample afterwards. The signature was fixed; the callers were not.
* `g_viewsGen` is stamped one increment behind.
* The render leg - what rendering actually consumed - was never obtained; the
  world view-projection is still unidentified. `c0..c3` is uploaded ~33 times per
  view and the block sampled was not a perspective world view.

The fix stands on a measured mechanism and a reversing A-B-A, not on those.

### The resolution ask: FIXED and CONFIRMED (VR-66)

The stale command line suspected here was **ours**. The size had two homes and
one writer: `dishonored_vr_launch.txt` drives the `-ResX/-ResY` the engine
obeys, `[Screen] RenderWidth/Height` drives the mode VirtualMode advertises.
Hand-editing the ini moved only the second, so the engine asked for the launch
file's five-day-old 2750x2850, that size was no longer advertised, and UE3 fell
back to a real display mode - the fullscreen 2560x1440. Neither ini ever
contained that number.

The ini is now the authority: the ask is resolved from `[Screen]` on the
engine's first `GetCommandLine` call (outside the loader lock), one resolved ask
feeds both the command line and the advertised mode, a disagreement logs
`launch: THE TWO ASKS DISAGREED` and rewrites the file, and the hooks install
even with no launch file.

**Confirmed in a run.** 3190x3306 (the same 55:57 aspect, 10.55 MP against 7.84)
was armed and honoured end to end: `res: HONOURED`, `capture: 3190x3306`,
`xr: swapchain pair 3190x3306`, bbox 100% x 100% FULL. **The engine never had a
ceiling** - it was asking for a size nobody was advertising. 2750x2850 is
restored; the size is now a performance question, not a correctness one.

### Next steps

1. Run the VR-66 verification above at a raised size, and read the three lines.
2. VR-64, the weapon swap flicker. The `wa/key:` instrument naming which of the
   fourteen contract key fields differs on a re-match has shipped and has never
   been read.
3. Repair the three instrument defects above, so the next pose question can be
   answered by measurement rather than by an A/B.
4. VR-57 the crosshair (Urgent), VR-58, VR-56.

---

## CURRENT (2026-09-09, later): attachment fixed twice; the flicker narrowed

Branch `claude/vr-62-startup-phase-timing`, pushed, **not merged, no PR**.
`VR-Main` is still at `b38519c3`. Last headset-tested build was
`vr33-hands-working-54-gda1d760d`; the build after it is not yet tested.

### Headset-confirmed this session

| What | Result |
|---|---|
| Weapons never attached after a second load | **FIXED** - attaches instantly on every load |
| Weapon flicker in stereo | **Largely gone** - the unreadable-step instrument fell from 31 windows with bursts of 565/396/350/293 presents to one window of one present |
| Hands flickering in mono | **FIXED** - a regression this session, caused and corrected in the same session |
| Mono window after a load | 2.1-2.3 s, against 24 s when VR-62 opened |

### Falsified this session, and one number retracted

Taking the weapon eye from the drawing pass executed **zero times in 83,400
draws**. The stereo passes run on the GAME thread and the palette draws on the
RENDER thread, so there is no stack to look up. The lever stays, default OFF and
inert, as the record. **The 39% disagreement figure reported earlier came from
reading a bare global across those two threads and is retracted** - it should not
be cited. ENGINE_NOTES carries all of it.

### Not yet tested: the bolt after a weapon swap

Reported: equip the pistol, switch back to the crossbow, and the **loaded bolt**
is unattached in its default position. Everything else stays attached.

Same class of fault as the load one, one level down. The list has an owner for
EMPTY and had none for STALE. Three things were needed and only the first is
obvious - an equipment-change trigger keyed on the item OBJECT rather than its
class name, a SETTLE WINDOW because the child component need not exist in the
same tick as the equipment event, and the equipped items as COLLECTION ROOTS
because the bolt is a child of the crossbow and not of the pawn. Plus retirement
of contracts by ownership, since the existing retirement can only be reached by a
contract whose buffers are still being drawn.

New levers, all default ON: `[Hands] AttachCollectEquippedRoots`,
`AttachSwapSettleTries=5`, `AttachSwapSettleGapMs=400`.

### The residual flicker, stated precisely

> The unreadable-step fallback is substantially less active in the tested build.
> Residual flicker remains unexplained. The instrument does not independently
> verify every inferred eye.

It records *unreadable* steps, not verified wrong-eye decisions, so a step the
heuristic reads confidently and gets wrong is invisible to it. The eye inference
is NARROWED, not cleared. Uninvestigated signals already in the log: the method's
`pushed eye +1 TWICE in a row` warning, which over-claims (its predicted
`abortLeft` stayed 0 and only 4 stale-eye submits occurred all session); the tag
ring's `realigned 97 times, 3855 agree / 296 disagree`; and
`UNEVEN CADENCE: 1.18 display slots per frame` at 147-162 presents/s against
90 Hz, which `vrpace sync <hz>` can A/B and never has been.

### VR-62 itself

Untouched behaviourally. Both falsified levers stay OFF (`[Menu]
GhostClearByRate`, `[Stereo] GateOnSceneLive`). The UI probe found
`bMovieIsOpen` and remains observation-only: several movie objects read open
persistently during gameplay, so "some movie is open" is not a blocking-menu
test. The per-class census in the log is the raw material for that and has not
been analysed.

### Next steps

1. Headset-test the bolt-after-swap fix (sequence in the session notes).
2. File a Linear ticket for the bolt fault, separate from VR-62.
3. Only then, the residual flicker - starting with what it affects, not with
   another correction.

---

## CURRENT (2026-09-09): VR-62 - the mono window, three attempts falsified

**`VR-Main` is at `b38519c3`. VR-59, VR-60 and VR-61 are merged and Done.** The
weapons work: a fired bolt stays where it lands, the pistol stays on the hand at
every angle, and the mod reads equipment from the engine.

Open work is VR-62 on branch `claude/vr-62-startup-phase-timing`, **not merged,
no PR**. The installed build is `C02CB40A26477DF2`.

### The state of VR-62

A startup scoreboard exists and works. Three attempts to shorten the mono window
were each falsified in a headset, and **both behaviour changes are default OFF**;
the build behaves like the known-good one, with better logging.

| Attempt | Lever | Result |
|---|---|---|
| Clear the ghost menu flag on dispatch RECENCY | `[Menu] GhostClearByRate` | 24 s to 1.5 s, **and the main menu goes stereo** |
| Double on SCENE LIVENESS instead of the verdict | `[Stereo] GateOnSceneLive` | **hands and weapons flash behind the pause menu** |
| Force a candidate re-collect on a load | (fixed, not a lever) | partial list with no body mesh, **nothing attaches all session** |

### What is established, and should not be re-derived

1. **Nothing after the gameplay verdict holds the picture.** The verdict, the
   `[game] state: GAMEPLAY` transition and the first DOUBLE draw land in the same
   millisecond. The wait is entirely in deciding the game is in gameplay.
2. **Two of the verdict's five terms are slow by construction.** `menuOpen` is set
   by a `Dis_OpenPauseMenu` dispatch during a load when no menu is open (a
   ghost), and `viewLive` deliberately requires a full second of continuous
   dispatches to leave LOADING - measured at +1.52 s and +1.72 s.
3. **The view-dispatch rate differs by a factor of eighty** between a settling
   level (about 1/s) and a running one (about 78/s). Any dispatch-based test has
   to separate those two states.
4. **The main menu keeps dispatching view rotations** (its 3D background) and can
   have a live pawn, so neither dispatch flow nor `CylTruthLive` separates it
   from gameplay. ENGINE_NOTES 38.17 recorded this before; attempt 1 re-broke it.
5. **The camera upload serial keeps moving while a menu is up**, so "the scene is
   drawing" cannot tell a pause menu from a load.

### The rule the three failures share

Every attempt replaced a slow conservative test with a fast one. Each was right
about the slowness and wrong about the replacement, because **the fast signals do
not separate the states that matter.** The next attempt needs a signal that
distinguishes a main menu from gameplay, and a pause from a load, DIRECTLY -
not a faster version of one that cannot.

The most promising unexplored lead: `g_mainMenu` is set from named ProcessEvent
dispatches rather than inferred, so it may be a real discriminator. Read how it
is set in `ue3/process_event.cpp` before trusting it.

### What is safe and staying

* The startup scoreboard (`startup.cpp`), read-only, one line per load naming the
  term that settled LAST. It records the LAST false-to-true transition, because
  recording the first made it blame the wrong term - the clock starts as the game
  leaves gameplay, when the outgoing pawn is still alive.
* Weapon contracts are dropped when the game leaves gameplay, and a contract
  whose component has been missing for about a second is retired. Without this a
  level load left every contract pointing at a destroyed component, which is a
  LOCKOUT rather than a refusal: a refused draw returns before the matcher, so
  the contract can never be re-adopted.
* A candidate list with no body mesh is discarded and re-collected, bounded at
  120 attempts.

### PLAN FOR THE NEXT SESSION - paste it here before starting

> **This section is empty on purpose.** Drop the agreed plan in, replacing this
> quote, before any code is written. A plan that lives only in a chat is lost the
> moment the chat is, and three attempts were already spent last session on ideas
> that were sound in isolation and wrong against facts recorded further up this
> file.
>
> Whatever goes here should name, for each step: the SIGNAL it depends on, which
> two states that signal separates, and how the run would show the step failed.
> The three falsified attempts all skipped that last part.

---

### Next steps

1. Find a real main-menu discriminator, then re-try attempt 1 behind its lever.
2. VR-16, the weapon and hands flicker for the first seconds after a load - the
   tail of the same settle.
3. VR-49, the parent ticket, still carries the eye-starvation half of the settle.
4. VR-57 the crosshair (Urgent), VR-58, VR-56.

## PREVIOUS (2026-09-08, night): VR-59, VR-60 and VR-61 all confirmed in a headset

Three tickets are fixed and headset-confirmed this session. **Nothing is merged.**
Two PRs are open against `VR-Main` and its stack.

| Ticket | What a player sees | PR |
|---|---|---|
| VR-59 | a fired bolt stays where it lands, visible and solid | #23 into `VR-Main` |
| VR-61 | (no visible change) the mod can read what the game is doing | stacked |
| VR-60 | the pistol stays on the hand at every angle | stacked |

### The through-line, which is the useful part

All three were the same mistake at different depths: **the mod acted on a guess
about game state because it had no way to ask.**

* VR-59: a contract identifies a GEOMETRY and was used as an INSTANCE. 196,619 of
  196,623 corrections were made on buffer identity alone while the only test that
  checks where a draw is ran 4 times in 13 million draws.
* VR-60: the pistol had no identity at all, so it was verified against another
  weapon's component. Its 45-degree detach cone was `AttachPassRadius` converted
  into an angle by geometry - a held weapon orbits the head, so 60 uu at 45
  degrees puts the view model about 78 uu from the camera. **Any threshold would
  have produced some angle, because the identity was what was wrong.**
* VR-61 is the general answer: ask the engine.

### VR-61, and the research failure worth keeping

**This repo already had a property resolver and a session reinvented it.**
`FindPropOffset` / `FindBoolProp` in `ue3/uobject.cpp` have resolved properties by
name since 38.x and are load-bearing in four modules. The mistake was going to
another project for a technique before grepping here for prior art.

The existing design is also the better one and it stays: every UProperty is itself
a UObject whose Outer is the declaring class, so a GObjects scan finds it and
nothing has to be derived - no chain offsets, no candidate layout, no search to
validate. 578 lines of derivation were deleted.

What was actually missing was two small things, and both are now in
`ue3/reflect.cpp`: a CACHE (each lookup is a full GObjects scan, and the trilogy
mod measured a name scan on a cadence stuttering that game at 2-3 Hz) and a TArray
READER (the capability VR-60 needed).

### What the engine now tells us, measured across sheathe and swap

```
Primary   DishonoredWepSword   EQUIPPED   (every time weapons are out)
Secondary DisWepCrossbow  <->  DishonoredWepPistol
sheathe:   Secondary -> none, then Primary -> none
unsheathe: both return together
```

A flag that reads the same in every state is not evidence it is the right flag, so
it was identified by making it MOVE. Two corrections came out of that:

1. **`PawnInventorySlot.m_RequiredUsage` is a constraint on what may occupy a
   slot, not what is in the hand.** Reading it reported an empty item in both
   hands while the player was visibly holding a sword.
2. **`DishonoredInventoryItem` carries `m_EquipUsage` and `m_CurSocket`.** The item
   answers for itself, which is the equipped-versus-holstered distinction VR-59
   attempt 1 needed and could not get from component presence.

Also recorded as an observation and NOT a conclusion: sheathing reads socket
`none`, never `holstered`, so nothing should assume a sheathed weapon is Holstered.

### Branch stack, which matters for merge order

```
VR-Main
  claude/vr-59-fired-bolt-instance-identity   PR #23   (Fixes VR-59)
    claude/vr-60-pistol-not-in-snapshot       ancestor only, no PR
      claude/vr-61-property-resolver          PR       (Ref VR-61, also fixes VR-60)
```

The VR-60 branch holds only analysis docs and is an ancestor of the VR-61 branch;
VR-60's fix is a commit on the VR-61 branch, because it depends on VR-61's reader
and the two were verified in one run. **Merge #23 first**, then the stacked PR
retargets to `VR-Main` and closes VR-60 and VR-61.

### Next steps

1. Review and merge #23, then the stacked PR. The merge is the gate and it is the
   user's call.
2. **The arms during takedowns and chokes**, now unblocked:
   `eDisPlayerActionUsage_Fullbody` is the discriminator, and `GAMEPLAY_STATE.md`
   section 2 lists the rest of the wanted flags.
3. **VR-49, the 20-90 s settle** (Urgent). The asset-to-hand decision is now
   readable from the engine rather than inferred, which may shorten it.
4. **VR-57, the crosshair** (Urgent). Still head-locked. One ray.
5. **VR-58**, **VR-56**.

## PREVIOUS (2026-09-08, night): VR-61, the UE3 property resolver, first run pending

VR-59 is fixed and headset-confirmed; PR #23 is open against `VR-Main` and NOT
merged. VR-60 (the pistol) is blocked on VR-61 by choice, because reading the
inventory is the clean fix and a second heuristic is not.

Branch `claude/vr-61-property-resolver`. Built, installed, 88 host cases, lint
clean. **The derivation has never run against the game.**

### What this is

`src/game/dishonored/ue3/reflect.cpp` resolves a property BY NAME to its byte
offset on this build, by walking the UClass property chain. The argument for it is
`docs/dishonored/GAMEPLAY_STATE.md`: almost every hard bug in this mod came from
acting on a guess about game state because there was no way to ask.

**Only four slots were unknown** - `UField::Next`, `UStruct::SuperStruct`,
`UStruct::Children`, `UProperty::Offset` - because `kNameOff` / `kClassOff` /
`kOuterOff` are already derived on this build. The BioShock trilogy work had to
derive those first and called it the hard part.

### The oracle, which is what makes this not a guess

`kWaComponentLocalToWorld` (0x60) and `kWaComponentTranslation` (0x90) are
measured on this build and read every frame, and UE3 names those properties
`LocalToWorld` and `Translation`. **A layout is accepted only if it resolves both
names to both offsets.** A wrong layout cannot reproduce an answer we already
know, so the search cannot quietly settle on one.

If derivation fails, the log says which stage and with what numbers - including
the case where the constants themselves are wrong for that class, which would
make the search impossible and the constants the bug.

### Three traps taken from the trilogy mod rather than rediscovered

1. **NEVER init-driven.** Our DLL loads from `DllMain` during import resolution,
   before the exe's CRT static initializers, so GNames is empty then. Deriving at
   startup fails every boot and looks like a broken instrument. This derives
   lazily on the script lane and retries every 2 s until it succeeds.
2. **A name-pool scan is hundreds of milliseconds and must never be on a
   cadence** - it stuttered that game at 2-3 Hz. Every resolve here is cached per
   (class, name) for the process lifetime.
3. **`ObjectArchetype` is class-classed and SHARED**, which falsified it as a
   chain link there: two nodes cannot share a `Next`. It is excluded explicitly.

A fourth is ours: the four-way search was ~810k candidate layouts, each doing two
chain walks. On the game thread that is a hang measured in minutes, and **a
diagnostic that freezes the game is not a diagnostic.** The slots are separable,
so derivation is staged into ~1,000 walks; the oracle still judges the finished
layout, so staging changed the cost and not the standard of proof.

### The first consumer exists to make the resolver falsifiable

A resolver that derives a layout and reads nothing has proved only that it did not
crash. So it reads the one thing the component walk provably cannot: pawn ->
`m_pInventory` -> `m_Slots`, a TArray of `PawnInventorySlot`, each slot carrying
the item AND its `EDisEquipUsage`. Every step resolved by name. Logs CHANGES only.

The struct STRIDE is the single unresolved number in that path, because element
layout is not in the property chain. It is validated rather than trusted: a stride
that yields no readable item pointer is reported as **a wrong stride, not an empty
inventory** - two things that look identical without that line.

### What the next run has to answer

Read in this order:

1. `rfl: UE3 property layout DERIVED ...` with the four slots and the oracle it
   was validated against. If instead `rfl: not derived yet - <reason>` repeats,
   the reason names the stage.
2. `rfl/state: equipment CHANGED - Primary ... Secondary ...` on every weapon
   swap. **That line is the proof**: it is data out of a TArray, which is what
   VR-60 needs and what the pointer walk cannot reach.
3. Nothing should look or feel different. This subsystem is read-only, off the
   frame path, and touches no render lever.

`[Hands] StateFlags=0` turns the reader off; the resolver itself has no lever
because nothing consumes it yet.

## PREVIOUS (2026-09-08, night): VR-59 is FIXED and headset-confirmed

**A fired bolt stays where it lands.** Confirmed in a headset: bolts are visible
and solid, hold their position and rotation, show no coupling to the hand in any
weapon, and newly fired bolts behave correctly too. The held bolt and the
crossbow are unaffected. PR open against `VR-Main`.

### What fixed it, in one sentence

A contract identifies a GEOMETRY and was being used as an INSTANCE. Every draw on
a weapon's buffers is now verified against the component that contract was
matched to, using the engine's own transform from the live snapshot, and a draw
that matches nothing is handed back exactly as the engine drew it.

The measurement that proved the architecture was the problem: `known-buffer
passes 196955, corrected 196619, matched 4`. Ninety-nine point eight percent of
corrections were made on buffer identity alone, while the only test that checks
where a draw is ran four times in 13 million draws.

### The two lessons, both paid for by a headset run

**A reference has to be maintained on the path that uses it.** `lastL2W` is
written only where the transform matcher adopts a contract, so a gate built on it
compares against something that is almost always stale. It refused 53,238 held
draws in one run with the present gap growing to 21,367.

**Refusing to correct a draw and refusing to draw it are different operations,
and the second one deletes the object.** Dropping claims a draw duplicates
geometry rendered correctly elsewhere - true of another pass of the held weapon,
false of a world instance. The rule was written into `may_suppress` and then not
applied at two of the four exit paths, which cost a run where the bolts were
invisible rather than misplaced.

### VR-60 is next, and its symptom CHANGED for the better

The pistol used to turn invisible when aimed away from where a bolt was. It now
stays visible and instead detaches to its default position with its own idle
animation, reattaching when the aim comes back into range. That is this fix
working: the draw was being dropped and is now handed back, so the failure mode
went from deletion to falling back on the engine.

The cause is unchanged and is VR-60's job: the pistol is not in the component
snapshot at all, so it has no member candidate, reaches a contract only through
the buffer lookup's `vb || ib` OR, and is therefore verified against ANOTHER
asset's component. Past the radius from the bolt, it is correctly refused - the
verification is right and the identity it is given is wrong.

Branch `claude/vr-60-pistol-not-in-snapshot`, off the VR-59 branch because the
fail-safe behaviour it builds on is not merged yet.

## PREVIOUS (2026-09-08, night): VR-59 attempt 2b - refusing is not deleting

Attempt 2 verified correctly and then DELETED what it refused: fired bolts were
invisible for a whole run while the held bolt and the crossbow were fine. Two
places consumed a refused draw, and both are closed. Built, installed, 88 host
cases. Not yet in a headset.

### The lesson, which is worth more than the fix

**Refusing to correct a draw and refusing to draw it are different operations,
and the second one deletes the object.** Dropping a draw is a CLAIM: that this
draw duplicates geometry the frame renders correctly elsewhere. That is true of
another pass of the held weapon and false of a world instance, which is the only
copy of itself there is.

The rule was written into `may_suppress` in attempt 2 and then not applied at
either site that needed it:

1. **`AttachDropUncorrected` sat past the verification block** and consumed any
   draw with no correction. Verification refused the bolt correctly, and this
   line ate it two branches later. Releasing `onWeaponBuffers` did not help -
   that only guards the SECOND suppressor, out in `WaDraw`.
2. **`instVerdict` defaulted to `HELD`.** A draw whose geometry does not match the
   contract exactly never reaches verification at all - a different range in a
   shared buffer, which is exactly what a fired bolt and the pistol produce - so
   it arrived at the drop path carrying a default that said "this is the held
   item". Unverified now means unverified, and only with the lever off does it
   mean held.

A guarantee that is stated in a pure helper is not a guarantee until every exit
path is routed through it. There were four such paths and two were missed.

### The new counter that would have caught it in one run

`wa: handed back to the engine N draw(s) rather than dropped (dropped-as-
duplicate M)`, and it says on the line: **if handed-back is 0 while fired bolts
are invisible, a refused draw is still being consumed somewhere.** That is the
reading the last two runs needed and did not have.

### Still open, and expected to persist

**VR-60**: the pistol turning invisible when aimed away from where a bolt was.
It is not in the component snapshot at all, so it has no member candidate and
reaches contracts only through the buffer lookup's `vb || ib` OR - its visibility
is decided by a distance test belonging to another asset. Attempt 2b should stop
it being DELETED (an unverified draw is now handed back), but the pistol still has
no attachment of its own and that is VR-60's job.

## PREVIOUS (2026-09-08, night): VR-59 attempt 2 - verify every draw

`VR-Main` is pushed at `555e8ff4`, PRs 18-22 closed. Attempt 2 of VR-59 is built,
installed and covered by 86 host cases; **not yet in a headset.**

### The measurement that settles the architecture

From the attempt-1 run: **`known-buffer passes 196955, corrected 196619`, and
`matched 4`.** 99.8% of all corrections were made on buffer identity alone, while
the transform matcher - the only thing that checks WHERE a draw is - adopted four
contracts in 13 million draws.

**A contract identifies a GEOMETRY and was being used as an INSTANCE.** That one
sentence explains every symptom reported, and they are all one bug:

* a fired bolt rotates in place - it inherits the held bolt's delta, conjugated
  about its own origin;
* two fired bolts rotate together, each about its own origin - same delta, same
  contract;
* after a weapon switch every bolt orbits the muzzle at whatever radius it had
  when the switch happened - the delta becomes a fixed transform relative to the
  hand;
* each bolt vanishes past an angle - beyond `AttachPassRadius` the draw is
  refused, falls through to a matcher that cannot place it, and is dropped.

And the reference that gate compares against, `lastL2W`, is written ONLY where
the matcher adopts a contract - so it was current 4 times all run. A gate on a
reference nothing maintains both misfires and misses.

### The rule that replaces it

**A draw is the held item only if it is where the engine says the held item is. A
draw that matches nothing is handed back exactly as the engine drew it.**

It names no asset, no weapon and no count, which is the point - a throwable that
does not exist yet is covered without new code. Per draw:

1. The contract carries the COMPONENT it was matched to (`compObj`), not just its
   buffers. A second instance of that mesh can never inherit its correction.
2. `WaVerifyDraw` compares the draw against that component's transform from the
   live snapshot, expressed in the draw's space through the bridge the matcher
   already builds. The snapshot is engine-read and republished twice a frame, so
   it cannot go stale while the weapon is in view.
3. A recent-verification cache (`heldAt`, refreshed on EVERY verified draw by both
   routes) answers when no correction was published this Present, so a quiet
   frame does not blink the weapons.
4. Nothing to compare against means REFUSE. Absence of evidence is not consent -
   that was the 41.x defect exactly.

**Suppression is now only for a draw that PASSED verification.** That is what
keeps a world instance visible: its colour and lighting passes are its own, not
duplicates of anything we drew, and suppressing them is what made fired bolts
vanish. The shared `dm` delta is also gated on the verdict, so a refusal is no
longer advisory.

The tolerance is `AttachPassRadius` reused, not a new number: the census measured
0.3 uu between an uncorrected pass and its corrected twin, and two instances of a
mesh are hundreds of units apart. Both bounds are asserted in the host suite.

### Levers

`AttachVerifyInstance=1` (OFF restores trusting buffer identity - the behaviour
of every previous build, so the two compare directly), `AttachHeldMaxPresents=2`.
The two falsified attempt-1 gates stay at OFF, kept so their measurement is
reproducible.

### What a headset run has to answer

`wa: instance verify` reports held / elsewhere / unverifiable, which reference
answered, and the worst offset accepted next to the farthest refused - so the
radius can be judged from both sides rather than argued. **HELD should be the
large majority while a weapon is out; 0 with flat weapons means verification is
failing, not idle.** If `component` is 0 the engine-read route is not running and
only the cache is holding it up, which would be a latent failure.

The risk to watch is the opposite of the old one: too much refusal. If a held
weapon goes flat or blinks, `AttachHeldMaxPresents` and the radius are the levers,
and `AttachVerifyInstance=0` returns to the old behaviour.

Also open: **VR-60**, the pistol is not in the component snapshot at all, so it
has no member candidate and reaches contracts only through the buffer lookup's
`vb || ib` OR. That is why its visibility depended on aim angle.

## PREVIOUS (2026-09-08, night): VR-33 merged; VR-59 attempt 1 FALSIFIED

`VR-Main` is pushed and at `555e8ff4`; PRs 18-22 are closed and their tickets
are Done. **The VR-59 fix was tried in a headset and both of its gates were
falsified.** Both are now default OFF, the build is rebuilt and installed, and
the run produced exactly the measurements needed to design the real fix.

### What the run said

The held bolt stopped following the hand and only inherited camera rotation - it
was drawing natively. The pistol vanished depending on the angle between where
it pointed and where the bolt had been. No bolt was fired at all, so every
symptom was on HELD geometry: a straight regression.

**`AttachRequireFreshRef` starves the held weapons.** 53,238 refusals in one
run, all on held `crossbow_01` and `bolt_01`, with the present gap growing
monotonically to 21,367. `lastL2W` is written ONLY where the transform matcher
adopts a contract; the buffer-identity route that actually corrects the auxiliary
passes never refreshes it. Once the matcher misses, the reference is stale
forever and the gate blocks the only remaining route. **A reference has to be
maintained on the path that uses it.**

**`AttachRequireLiveMember` cannot answer its own question.** All 19 published
snapshots in the run were identical - the same six components, including
`bolt_01 (pArrowMesh_HighRes)` - across crossbow, sword and pistol being held in
turn. `FpCollect` walks the pawn INVENTORY, and `DisWepCrossbow` says why: the
loaded bolt is `m_pArrowMesh_HighRes`, a component of the WEAPON, which stays in
inventory when stowed. Presence is not equipment.

### What the scripts gave, and why it matters

In `docs/dishonored/ENGINE_NOTES.md`. The loaded bolt and a fired bolt share
**only** the mesh asset `bolt_01`; the component name, the component class and
the owning actor all differ, and a fired bolt is a separate ACTOR
(`DisProjectile_Arrow`) that never reaches the snapshot. That is why every
asset-name and buffer-identity route can be fooled, and it is the shape any real
fix has to take.

The pistol is not in the snapshot at all, which is a separate finding worth
acting on: it has no member candidate and can only reach a contract through the
vertex-OR-index-buffer match, which is what made its visibility depend on aim
angle.

### The next attempt, designed but NOT written

Compare each draw against the contract's COMPONENT position from the engine
snapshot, expressed in draw space through the bridge the matcher already builds -
not against `lastL2W`. The snapshot is engine-read and refreshed every 4 ms
whether or not the matcher succeeded, so it cannot go stale the way `lastL2W`
does. Store the component pointer in the contract at adoption so the right
component is looked up each frame.

What is kept from attempt 1: the `held_instance` predicate and its 13 host cases,
`AttachVetoReleasesBuffers` (sound - a vetoed draw must not be suppressed), and
`AttachInstanceVetoRelaxed`.

### Previous entry for this session (the merge, still accurate)

## PREVIOUS (2026-09-08, later): VR-33 merged, VR-59 attempt 1 written

Two things happened this session. **PRs 18-22 are merged into `VR-Main` locally
and are NOT pushed yet** - the push was blocked by a tool permission, so the
remote `VR-Main` is still at `f44f4761` and all five PRs are still open. The
first job of the next session is that one command, or to say so plainly if it is
still refused.

Then VR-59, the fired bolt, which is written, built, installed and covered by
host tests but **has not been in a headset**.

### The merge, and why it is five commits and not one

PR 22 turned out to be a strict SUPERSET of 18, 19, 20 and 21: it was rebuilt off
`VR-Main` and carries every feature from all four, plus later tuning that
supersedes theirs (`HeightOffsetM` -0.090 -> 0.060, `PosTrack Scale` 98 -> 108).
Merging 22 alone would have landed everything but left the other four PRs unable
to close themselves, so they were merged in order 18 -> 19 -> 20 -> 21 -> 22
instead, each as its own merge commit.

Every conflict on the way to 21 was two branches appending to the same region - a
new `CURRENT` section, a decision-log entry, `#include` lines in the unity TU -
and was resolved as a UNION, so each feature keeps its own section. The final
merge took 22's side throughout, because 22 is the integrated superset, and then
22's own cleanup was applied (the 23 scaffolding docs it replaced with one
durable record, and two experiments it retired to `src/legacy/vr33/`, which is
why nothing was lost).

**The end state was verified by construction: the merged tree is byte-identical
to PR 22's tree**, which is the headset-confirmed build. `git diff HEAD
origin/claude/vr-33-rotation-grip-and-weapons` is empty. Lint clean, Release
builds, nine exports undecorated.

### VR-59: a distance can never answer an instance question

The branch is `claude/vr-59-fired-bolt-instance-identity` off the merged
`VR-Main`. **No PR** - deliberately, on request.

The three radius gates could not close this and were never going to. A bolt fired
into a surface a metre away is inside all of them on merit. What identified the
real defect was that the fault behaves COMPLETELY DIFFERENTLY depending on what
is held: with the crossbow out a fired bolt only inherits its rotation, but with
the pistol out the bolt jumps onto the aim direction and tracks the pistol - **at
any distance, near or far.** A fault that reaches an arbitrarily distant instance
proves no radius was gating it.

**A contract outlives its weapon being stowed, and the instance gate ran only
when a fresh reference existed.** `g_waMesh` is keyed on buffers and evicted only
when the table fills. The pass-radius check ran behind
`if (lastL2WOk && present - lastL2WPresent <= 2)`. Stow the crossbow, the loaded
bolt stops drawing, that reference goes stale, and **the gate is skipped
entirely** - while the hand that contract belongs to keeps publishing a fresh
correction every frame, because the hand is always drawn. The absence of evidence
was being read as permission.

The dark stub left standing where the bolt landed is the same cause, not a second
bug: some passes were corrected and the rest were SUPPRESSED by
`AttachSuppressUnplaced`, whose documented cost is exactly this when it lands on
a world instance - those colour and lighting passes are the bolt's own, not
duplicates of anything we drew.

### What replaced them

`dvr::wf::held_instance` in `weapon_frame.h`, pure and fully exercised by
`frame_test`. Four verdicts, and the ORDER is the authority they carry:

* `STOWED` - that asset is not a live member of that hand in the current
  component snapshot. **Strong, and it is the engine's own answer**: `FpCollect`
  walks out from the pawn through the inventory chain only, so a world projectile
  cannot appear in it however close to the camera it sits.
* `ELSEWHERE` - a fresh reference exists and this draw is past
  `AttachPassRadius` from it. Strong.
* `NO_REF` - nothing has vouched for this geometry for `AttachRefMaxPresents`
  presents. **Weak on purpose.** It refuses the correction, but it may not
  release buffers or overrule the relaxed band, because a weapon just re-equipped
  has a stale contract by definition - treating it as strong would stop a
  re-equipped sword relocking and would draw a ghost copy while it tried.
* `HELD` - corrects.

Five levers, `[Hands]`, **all default ON**: `AttachRequireLiveMember`,
`AttachRequireFreshRef`, `AttachRefMaxPresents=2`, `AttachVetoReleasesBuffers`,
`AttachInstanceVetoRelaxed`. Each one off restores the pre-VR-59 behaviour of
that single step, so they A/B alone; the host suite asserts that too.

### Verified on the desk

75 host cases pass (13 new, all on the instance verdict), `tools\lint.ps1` clean,
Release built and installed, installed DLL hash matches the build. The installed
ini has `AttachWeapons=1` and `AttachSnapshotMaxMs=100` and none of the five new
keys, so all five take their ON defaults.

The new cases deliberately hold the DISTANCE inside the radius and vary only the
instance evidence - a suite that separated them by distance would be testing the
gate that already failed.

### Next steps

1. **Push `VR-Main`** (`git push origin VR-Main`), then confirm PRs 18-22 closed
   and VR-30, VR-31, VR-33, VR-51, VR-53 moved to Done.
2. **A headset run for VR-59.** Fire a bolt into a wall a metre away, look at it,
   then switch weapons and look again. `wa: instance gates` reports every
   counter; the line says on itself that ALL ZERO IS THE HEALTHY READING while a
   weapon is held and drawing, because it counts draws that are not the held
   instance. A non-zero `stowed` count with the bolt sitting still is the fix
   working.
3. **Watch for the regression this could cause**: re-equipping a weapon must
   still relock, and must not show a ghost copy while it does. That is what
   `NO_REF` being weak protects, and it is the one thing in this change that
   trades against the fix.
4. **VR-49, the 20-90 s settle** (Urgent). The weapon lock is part of it and the
   decisions are cacheable.
5. **VR-57, the crosshair** (Urgent). Still head-locked while the weapon points
   where the hand points. One ray.
6. **VR-58** (numpad adjust, ModelScale) and **VR-56** (no back faces on the
   weapon models).

---

## PREVIOUS CURRENT (2026-09-08): VR-33 is DONE and in review

The hands and the held weapons are on the tracked controllers, headset-confirmed
and stable. The branch is `claude/vr-33-rotation-grip-and-weapons`, twelve
commits, and the PR is open against `VR-Main`. **Nothing here is merged.**

### What a player sees now

* Hands track position and rotation, and hold still when the head moves.
* The crossbow, the loaded bolt and the sword follow their controllers, keeping
  the game's own hand-to-weapon and weapon-to-bolt relationships and their
  internal animation.
* No duplicate weapon standing at the position the engine drew it.
* A rare single-frame blink on the weapons, and a fired bolt close to the player
  can still be picked up. Both are ticketed, neither reads as broken.

### The three things worth knowing before touching this

**The grip transform is a REFLECTION.** The draw's camera basis is right-handed
and the engine is left-handed, so the pose mapping is a mirror. Three Euler
angles cannot carry one; the saved record stores a parity sign beside a proper
rotation and refuses a pre-version record rather than loading a hand inside out.
A build that demands a proper rotation refuses every draw.

**A weapon mesh is drawn by several passes, and any pass we do not place draws
itself at the native position.** That is what the duplicate copies were. The
contract key must include the VERTEX SHADER; without it an uncorrected pass
looks like the contract's own draw.

**Suppressing a pass is only safe when it would otherwise draw a second copy.**
Several of a weapon's passes are its colour and lighting contributions.
Suppressing those leaves the ambient term alone: a translucent weapon that
vanishes in shadow. That was measured, not guessed.

### The blink, and the shared gate behind it

The two weapons share no contract, buffers, component or match - only their
inputs. **They blink together, which is what identified the cause**: a shared
gate, not two independent failures. The gate was the component-snapshot
freshness bound, tightened from 100 ms to 20 ms while chasing a view-model sway
theory the headset then falsified. The theory was dropped and the bound was not.
Back at 100 ms the blink is rare.

`AttachSnapshotMaxMs` is the lever. **Tightening it blinks both weapons.**

### Next steps

1. **Merge VR-33** when the reviewer is satisfied - the user's call, not an
   agent's.
2. **VR-59, the fired bolt** (High). A bolt fired into something close by is
   inside every distance gate and is the same mesh drawn from the same buffers.
   The gates cannot close it; the fix is an instance identity read from the
   engine. This is the next session's focus.
3. **VR-49, the 20-90 s settle** (Urgent). The weapon lock is now part of it.
   The asset-to-hand and asset-to-space decisions do not change between runs
   even though the buffers do, so they are cacheable.
4. **VR-57, the crosshair** (Urgent). It is still head-locked while the weapon
   points where the hand points. One ray.
5. **VR-58**, the numpad adjust and ModelScale, neither ever exercised in a
   headset. Both default to the identity, so the build that was tested is the
   build that ships.
6. **VR-56**, the weapon models have no back faces. The asset was authored to be
   seen from one side.

### Verified on the desk

49 host cases (28 hand, 21 weapon), `tools\lint.ps1` clean, nine exports
undecorated, the installed DLL matching the build. The simulator was not used
for this work: every question on it was perceptual.

### The record

`docs/dishonored/VR-33-HANDS-AND-WEAPONS.md` is the one durable document -
mechanism, levers, and a graveyard of every approach that cost a headset run.
Section 8 is worth reading before any similar work: five separate filters in
this investigation each produced a confident zero while excluding the thing they
were built to find, and an enumeration solved in one run what they had hidden
for eight builds.

## PREVIOUS CURRENT (2026-09-08): VR-33 weapon attachment direct repair, test pending

Release fix installed; 28 hand tests and 21 weapon tests pass. The first headset run confirmed weapon
motion, but wrong hands, a large offset and flickering old-position silhouettes.
The follow-up corrects hand settings and permits qualified non-camera passes.
Correct alignment and ghost removal remain pending the next headset run.

No sweep or blinking is expected. Equip crossbow/sword, move each controller,
then check the loaded bolt, head motion, both eyes and re-equip/reload.
Read `wa: beat v2`, `wa: interval nearest`, and per-asset `wa: contract` counts.
Sword defaults RIGHT (1); crossbow/bolt LEFT (0). Hand calibration is retained.

Full implementation and remaining assumptions:
[VR-33 direct fix handoff](dishonored/VR-33-HANDS-AND-WEAPONS.md).

The old sweep-based test instructions below are historical and superseded.

## Historical (2026-09-07, night): VR-33 - weapon attach armed, model scale added

### TEST TOMORROW, in this order

**1. THE WEAPONS (priority).** Launch, draw the crossbow, stand still somewhere
quiet facing a wall, let the sweep run (~26 s of blinking), then keep playing.
Nothing to press.

* If it worked: the weapons follow your hands, and the log has
  `wa: ADOPTED 'crossbow_01' ...` followed by `wa: ... ATTACHED`.
* If it did not: the log now says WHY. Read these two tables, in this order:
  * `wid: draws per phase` - total draws in each phase. **If the total does
    not fall when a component is hidden, its draws are still not in the
    population** and nothing below it means anything.
  * `wid: the 16 largest baseline signatures` - each buffer pair's whole phase
    vector. A weapon's pair should read high, 0, high, 0, high across its own
    four phases. This separates "drops on one hide only" from "never drops at
    all", which have looked identical for three runs.

**2. THE SIZE, once the weapons are settled.** F10 -> **"hand / weapon size"**,
or `[Hands] ModelScale` in the ini. Try **0.75**. It scales the hands AND
anything held in them by one factor, about the tracked palm.

* EXPECTED: hands and weapon shrink together, the grip stays put, the world
  does not change size.
* Ships at **1.00**, which is exactly the identity it replaces - so test 1 is
  measured on the same geometry every previous build was.
* Watch for hand shading getting brighter or darker as you move the slider:
  two of the three hand shaders push normals through these palette rows, and
  if they do not renormalise, a uniform scale changes normal LENGTH. Direction
  is safe either way.
* Also watch the wrist cap for a hole opening at the cut.

### Where the weapon work actually stands

The sweep is CORRECT and has been for three runs: all 16 phases, every hide
and restore verified against `HiddenMaterials`, and the tester watched the
sword, crossbow and bolt each blink twice on command. What kept failing was
never the sweep - it was what the sweep could SEE.

Three faults found and fixed, in order:

1. **The report was lost to a lane that stopped** (`5df74130`). `WiTick` runs
   on the script lane; the last phase ends on a deadline, so the report needed
   one more tick of that lane, and the lane went quiet half a second later. 26
   seconds of correct measurement, no output. The terminal step now runs from
   whichever lane reaches the deadline first, behind an interlock.
2. **The signature was too fine** (`c1fc4c55`). It hashed the draw's own range,
   so a mesh drawn from a shared buffer became many short-lived signatures. Now
   keyed on the vertex+index buffer pair - the key the mesh lock already uses,
   and the one that names a GEOMETRY.
3. **The recorder never saw the weapons at all** (`bc73daed`). It sat behind
   the palette gate (a fresh c6 upload within the reuse window) - right for the
   census, wrong for identification. `vs_const_hook.cpp` already describes the
   crossbow's body as a STATIC attachment, which that gate excludes. It now
   sees every indexed draw and records whether a fresh palette was pending.

**The evidence that named fault 3 was in the log for two runs and was not
read**: reject counts of 146/154/151/153, then 51/50/51/48 - near-identical for
every component INCLUDING the player body. Counts that uniform are not a fact
about components. That is what "the thing being measured was never in the
population" looks like, and it survived a change of signature key because the
key was never the fault. Hence the two new tables above: a third failure
cannot now be silent.

### The attachment itself

`weapon_attach.cpp` (+ state chunk 57b). The sweep's owned buffer pairs are
handed straight to placement - identification is the INPUT to attachment, not
a report somebody reads and types back. Those draws then take the same rigid
palette correction the hands take, from the same `g_mpPalmTarget`.

A weapon assembly is rigid, so its frame is the palette's first bone rather
than an averaged anchor; every bone gets one common transform, which is what
keeps the loaded bolt animating with the stock instead of being pinned
separately. A mesh with no palette is treated as one bone, three registers.
c6 is current device state, so the game's own block goes back after the draw.

Fail soft throughout: no target palm, an unreadable palette, a frame that will
not normalise or a non-finite result all draw the engine's own weapon and log
which. Blast radius is bounded to buffer pairs the sweep proved, so the
confirmed hand path cannot be reached. `AttachWeapons=0` removes it.

### The model scale (`21d3e3eb`)

PageUp/PageDown were never a size knob. `g_posScaleUU` sets the stereo
separation - a property of the PROJECTION - so it resizes the whole frame at
once. The hands were not scaling with the world, they were scaling because of
it, and that knob could never have made them smaller relative to the room.
Hand TRAVEL is already independent (`WorldScaleUU`, `PaletteDriveGain`), which
is why tracking felt right while the models were too big.

`[Hands] ModelScale` is a uniform factor about the target palm, so the grip
stays where tracking put it. The weapon path takes the same factor about the
same palm, so the two cannot drift apart. `delta_from_target` now hands back
the palm in local space (it always computed it and threw it away).

The F10 "hand / weapon size" slider drives this instead of `HandSize`.
`HandSize` wrote `SkelControlBase.BoneScale` engine-side, which only reaches
SkelControl-driven bones and could never resize a separately-componented
crossbow. The key still loads for the legacy drive; config warns with the
product if both are off 1.0.

### The levers as installed

`AttachWeapons=1 AttachSwordHand=0 AttachCrossbowHand=1 WeaponId=1
WeaponIdMs=1500 MatCycle=0 PaletteRotate=1 ModelScale=1.00 HandSize=1.00
Adjust=1 AdjStepT=1 AdjStepR=3`, per-hand trim under `TrimLTX..TrimRRZ`, grip
calibration under `GripL*`/`GripR*`. `MatCycle` must stay 0 - the identifier
drives the same hide/restore calls and refuses while the cycler is armed.

### Still untested from the previous session

**The numpad hand adjust has never been pressed in a headset.** Numpad 9
cycles LEFT position / LEFT rotation / RIGHT position / RIGHT rotation and
names the mode; 8/2 forward/back or pitch, 6/4 right/left or yaw, 0/5 up/down
or roll; 7 cycles the step. Every press logs and saves. The census gives up
Numpad 4-9 while `Adjust=1`, the cycler gives up 2, and the mesh split's mode
cycle moves from Numpad 0 to Numpad 1 - the startup log names all of it.

**The grip restart path is confirmed** only insofar as the calibration loads;
the hands were reported correct on the runs since.

### Verified on the desk, not in the headset

28 frame-maths cases pass (`build\src\RelWithDebInfo\frame_test.exe`),
including the new `model_scale_about_the_palm`, which FAILED on its first run
and was right to - it measured distances from `D(palm)` when the palm is a
point in the OUTPUT space that `D` maps the source anchor onto. `lint` clean,
exports clean. **The attachment and the model scale have never been in a
headset.**

### Next steps

1. The two tests above.
2. If the weapons attach, the remaining W-items are in
   `docs/dishonored/VR-33-HANDS-AND-WEAPONS.md`: re-acquire on equip
   change (buffer pointers can differ after a re-equip), and ownership during
   reload and release.
3. `pcap/layout` was printing at draw rate and produced a 25 MB log in one
   short run; now once per shader. Worth a look for other unbounded per-draw
   lines in the same family.

## PREVIOUS CURRENT (2026-09-07, later): VR-33 - the numpad adjust and a weapon identifier that can fail

### WHAT TO TEST, in order. Everything is installed and armed; just launch.

The build is Release, installed, and the ini is set. Nothing needs typing and
no key needs pressing to arm anything.

**Test 1 - the calibration survives a restart.** Launch and look at your hands
before touching any key. The grip was solved and saved last session
(`GripLVersion=2 GripLParity=-1`, `GripRVersion=2 GripRParity=-1`).

* EXPECTED: the hands are at the right angle immediately, the same as they
  were after SHIFT+F7 last time. The log says
  `config: the <side> hand's grip calibration LOADED - version 2, parity -1`
  for both hands, and NOT the "NO CALIBRATION" warning.
* IF THEY ARE MIRRORED OR INSIDE OUT: the saved record is not being applied.
  Say so; the log line above is the one that matters.
* IF THEY ARE AT A WRONG ANGLE BUT NOT MIRRORED: the record loaded but is
  wrong. SHIFT+F7 will re-solve it.

**Test 2 - the numpad adjust.** Press **Numpad 9** four times, slowly.

* EXPECTED: four log lines naming `LEFT hand POSITION`, `LEFT hand ROTATION`,
  `RIGHT hand POSITION`, `RIGHT hand ROTATION` in that order, each printing
  that hand's current trim.
* Then in `LEFT hand POSITION`, press **Numpad 8** a few times. EXPECTED: the
  LEFT hand moves along its own fingers, 2 cm per press, and the right hand
  does not move at all. Numpad 6/4 move it across the palm, 0/5 out of it.
* **Numpad 7** cycles the step (0.5 / 2 / 5 cm). In a rotation mode it cycles
  0.1 / 0.25 / 0.5 / 1 / 2 / 5 / 15 degrees.
* Every press writes to the ini, so stop wherever it looks right and it will
  be there next launch. No key press is needed to save.
* IF A KEY DOES NOTHING: check the log for that press. The build prints a line
  for every press, including a CLAMPED line at the limits and a warning if the
  press is saved but cannot move the hand yet. A press with NO line at all is
  the interesting failure - that means the key is not reaching us.

**Test 3 - the weapon identifier.** Nothing to press. Draw the crossbow, stand
still somewhere quiet facing a wall, and keep it in view.

* EXPECTED, before you draw anything: `wid: WAITING - N component(s) resolved
  and none of them is a weapon`, listing what it has. This is the fix: last
  run it swept anyway and produced a confident wrong answer.
* EXPECTED, a few seconds after the crossbow is out: `wid: sweep planned -
  baseline plus N component(s) x 4 phases`, then the crossbow BLINKING off and
  on twice per component, about 1.5 s each. **Stand still for the whole
  sweep** - the test is "this draw stops and comes back with the hide", and a
  view that changes under it produces the same signal.
* EXPECTED at the end: a report. The three outcomes and what each means:
  * `'crossbow_01' OWNS signature ...` - this is the answer, and it is what
    the weapon attachment needs.
  * `*** THIS REPORT IS VOID ***` - the signature table filled up. Not a
    failure to report; it means try again somewhere emptier.
  * `'crossbow_01' owns NO signature` - hiding it stopped nothing that came
    back. Also an answer, and a different problem.
* The report also prints how many signatures vanished on ONE hide but not the
  other and were REJECTED. A nonzero number there is the instrument working:
  those are exactly what the previous build called owned.

### What changed this session

**The hand trim moved to BioShock Remastered VR's numpad scheme, per hand**
(`7c621cc2`). F5 could never have worked: it is the game's quicksave and
`head_track.cpp` reads it at two more places, so one press fired three
features. The scheme is adopted key for key because those are the keys the
tester already has in his fingers.

The trim is now per hand. One shared value assumed the residual after
calibration is common to both palms; it is not, since the two grips are solved
from two separate poses. The old shared keys seed both sides once so nothing
already dialled in is lost, and the migration is logged.

Every numpad key was already claimed behind a feature gate, so `[Hands]
Adjust=1` makes an EXPLICIT claim rather than adding another reader: the
census gives up Numpad 4-9 entirely, the material cycler gives up 2, and the
mesh split gives up Numpad 0 with its mode cycle moving to Numpad 1. One
startup line names what took what. Numpad + - * / . are untouched.

**The weapon identifier was rebuilt so it can fail its own hypothesis**
(`7bfa2675`). All three defects named in the previous entry are fixed:

1. it refuses to plan until a component that is not the player body has
   resolved AND the candidate list has been unchanged for three seconds;
2. a table overflow VOIDS the report instead of footnoting it, and the table
   is sixteen times larger;
3. each component gets HIDE, SHOW, HIDE, SHOW, and a signature is attributed
   only if it vanishes on both hides and returns on both shows. The report
   counts the single-cycle near-misses it rejected, which is the number that
   shows the old sweep was measuring noise.

A fourth thing turned up on the way: `MatShowSection` returns true when the
native was CALLED, not when the section actually hid - it logs `[DID NOT
TAKE]` separately and still returns true. Phases are now judged by reading
`HiddenMaterials` back, and a phase whose flags disagree with what it asked
for is poisoned and its component reported UNTESTED.

### The levers as installed

`Adjust=1 AdjStepT=1 AdjStepR=3 Palette=1 PaletteWorld=1 PaletteEyeOffset=1
PaletteDepthRange=1 PaletteRotate=1 WeaponId=1 WeaponIdMs=1500 MatCycle=0`,
per-hand trim under `TrimLTX..TrimRRZ`, grip calibration under
`GripLVersion`/`GripLParity`/`GripLX..Z` and the right-hand equivalents.
`MatCycle` must stay 0: the identifier drives the same hide/restore calls and
refuses outright while the cycler is armed. `PaletteRotate=0` returns to the
headset-confirmed translation-only build.

### Verified on the desk, not in the headset

27 frame-maths cases pass (`build\src\RelWithDebInfo\frame_test.exe`),
including `hand_trim_is_in_the_palm_frame` and `hand_trim_carries_the_weapon`
which pin the trim the numpad now drives. `tools\lint.ps1` clean, exports
clean. **Nothing here has been in a headset** - the numpad bindings, the
per-hand split, the restart path and the whole identifier are unverified.

### Next steps

1. The three tests above.
2. If the identifier names the crossbow's draws, weapon placement is unblocked:
   `docs/dishonored/VR-33-HANDS-AND-WEAPONS.md` is the full spec, and
   the weapon target comes from the SHARED `palm_target` helper the hands
   already use, so a weapon can be placed before either hand draws.
3. The hand adjust has no auto-repeat, deliberately - BRVR has none and an
   unrequested one overshoots. If the tester wants it, it is four lines.

## PREVIOUS CURRENT (2026-09-07): VR-33 - hands CONFIRMED, weapons are next

### Headset-confirmed this session

**The hands track position AND rotation correctly.** After one SHIFT+F7 grip
capture they snapped to almost exactly the right pose, tracked both position and
rotation, and stayed still when the head moved. The tester asked for this to be
kept. Recoverable tag: commit `82abe020`.

Before the capture they were mirrored and inside out. That is the same
arithmetic as the restart bug and both are fixed in `9d55ccde`: the solved grip
is a REFLECTION (det -1), three Euler angles cannot carry one, and identity was
therefore the wrong uncalibrated default. The record now stores a parity sign
beside a proper rotation, is versioned, refuses pre-version records, and saves
itself so a calibration survives a restart with no key press.

### The calibration save WORKS, measured

```
ms/palette/grip: SOLVED for the RIGHT hand ... parity -1, proper rotation
                 +29.52 -47.63 +22.47 degrees
ms/palette/grip: the left hand's calibration is SAVED (version 2, parity -1)
ms/palette/grip: the right hand's calibration is SAVED (version 2, parity -1)
```

Both hands solved parity -1, as predicted, and both wrote a version-2 record.
The restart path itself is still UNVERIFIED - nobody has relaunched and checked
the hands come back right without a key press. That is headset test 1 below.

### THE WEAPON IDENTIFIER PRODUCED A FALSE POSITIVE. Read this before reusing it

It ran, and its report is wrong. Three defects, all visible in its own output:

```
wid: sweep planned - baseline plus 1 component(s)
wid: 128 distinct skinned draw signature(s) over 16974 draw(s),
     55272 that did not fit the table
wid:   'Skm_Player' OWNS signature ... | c6 x3 (1 bones) prim 486 stride 12
```

1. **Only ONE component resolved.** `Skm_Player` alone; no `crossbow_01`, no
   `bolt_01`, no `Wpn_PlySword01`. The sweep fired about 17 s after the config
   loaded, before the weapons existed as components. It must WAIT until the
   assets it is there to identify are actually resolved, and refuse rather than
   sweep a list that cannot answer the question.
2. **The signature table saturated.** 128 held, **55,272 draws did not fit**.
   Once full it cannot record a new signature, so later phases are blind. An
   overflow must INVALIDATE the report, not appear as a footnote under
   attributions that it silently broke.
3. **The attributions are not weapons and are probably not the player.**
   `c6 x3` is a ONE-bone palette at stride 12 - world props, not the skinned
   first-person mesh. What the report actually measured is "these draws stopped
   during a 1.5 s window", which a camera move or an object leaving the view
   produces just as well as a hide does.

**The instrument cannot fail its own hypothesis, which is this project's oldest
recurring fault** (`VR-33-HANDS-AND-WEAPONS.md` section 4). The orphan count was meant to be
the control and a saturated table defeats it. The fix is not a bigger table
alone: a signature must vanish on EVERY hide and RETURN on EVERY restore, over
at least two hide/restore cycles, before it is called owned. A single
disappearance is not evidence.

### FEEDBACK FROM THE HEADSET RUN

1. **F5 is already bound.** The hand trim added in `9d55ccde` uses F5, and
   `head_track.cpp:620` and `head_track.cpp:1064` already read it. One press
   fires both, so the trim is unusable as shipped and must be rebound.
2. **Nothing was seen to blink** - consistent with the above: only the player
   body was ever hidden, for 1.5 s, once.
3. **The alignment needs small tweaks**, position and rotation, per hand.

### What the tester asked for next, specifically

**Adopt BioShock Remastered VR's numpad adjust scheme, per hand.** Four modes
cycled with **Numpad 9**, in this order:

```
  0  LEFT hand POSITION
  1  LEFT hand ROTATION
  2  RIGHT hand POSITION
  3  RIGHT hand ROTATION
```

The scheme is adopted from the maintainer's own BioShock Remastered VR mod,
where it has been in use for a long time; these are the keys the tester already
has in his fingers, which is the reason for matching them exactly:

| Key | Position mode | Rotation mode |
|---|---|---|
| Numpad 8 / 2 | forward / back (cm) | pitch (deg) |
| Numpad 6 / 4 | right / left (cm) | yaw (deg) |
| Numpad 0 / 5 | up / down (cm) | roll (deg) |
| Numpad 7 | cycles the step: 0.5 / 2 / 5 cm | 0.1 / 0.25 / 0.5 / 1 / 2 / 5 / 15 deg |
| Numpad 9 | cycles the mode, and the log line NAMES the mode and hand | |

Every change is logged and written back to the ini, as BRVR does.

**THE COLLISION, measured, and it must be handled before this ships.** Every
numpad key in this repo is already claimed, each behind a feature gate:

| Keys | Owner | Gate | Free right now? |
|---|---|---|---|
| 1 / 2 / 3 | the material cycler | `g_matCycleCfg` | yes - `MatCycle=0` in the installed ini |
| 4 / 5 / 6 / 7 / 8 / 9 | the draw census and its eighth-cutter | `g_dcOn` | only while the census is off |
| 0 and `/` | the mesh split | `g_msOn` | **NO - the split is what draws the hands** |
| CTRL+2 | hand move | `kCtrl` | n/a |

So Numpad 0 and 5 (up/down in BRVR's scheme) collide with the live mesh split.
**Do not just add another reader.** Give the adjust mode an explicit claim: when
`[Hands] Adjust=1` the adjust block takes the numpad and the split, census and
cycler blocks are suppressed for those keys, with one log line saying the numpad
is claimed and by what. One key firing two features has cost this project a
session already (`7099c3b0`, `0e1ccbb0`).

The existing hand trim (`g_mpTrimT` / `g_mpTrimR`, palm-frame, saved on every
press) is the right backing store for the two LEFT/RIGHT modes - but it is
currently ONE shared trim for both hands and needs splitting per side. It is
already applied through `palm_target`, so a per-hand version needs no new maths.

### Then: actually attach the weapons

The identification instrument is built and armed but has produced nothing yet.
Its output is the input to placement, and placement was deliberately NOT written
blind. `docs/dishonored/VR-33-HANDS-AND-WEAPONS.md` is the full spec;
the short form is:

* one stable grip/root frame on the crossbow;
* `D_assembly_C = WeaponGripTarget_C * inverse(S_C)`, and every member takes it
  through its own `LocalToWorld`: `D_j_local = inverse(L_j) * D_assembly_C * L_j`;
* the loaded bolt keeps its animated relationship to the root - independently
  pinning each part to a controller offset would cancel its animation;
* the weapon target comes from the SHARED `palm_target` helper, which is already
  extracted and tested, so a weapon can be placed before either hand draws;
* weapon draws must CONSUME the eye decision, never feed the hand classifier.

### The levers as installed

`Palette=1 PaletteWorld=1 PaletteEyeOffset=1 PaletteDepthRange=1
PaletteRotate=1 WeaponId=1 WeaponIdMs=1500 MatCycle=0`, grip calibration saved
under `GripLVersion`/`GripLParity`/`GripLX..Z` and the right-hand equivalents.
`PaletteRotate=0` returns to the translation-only build.

### Verified on the desk, not in the headset

27 frame-maths cases pass (`build\src\RelWithDebInfo
rame_test.exe`, and the
same suite runs from `DllMain` into every log). The 28 saved packets replay
through the shipped decomposition: dominant slot 10 left / 35 right, uniform
scale 0.9995117 to 0.9995123, worst anisotropy 6.0e-07.

Measured this session and in ENGINE_NOTES: the XR-to-game pose mapping is a
MIRROR (`det(B*F) = -1`); the pose tick and the hand draws are ONE thread
(14224), 0 stale snapshots over 11,881 publications; and two of the three hand
shaders run normals and tangents through the same palette rows, so a rigid
correction carries the tangent frame and `WorldToLocal` must be left alone.

## PREVIOUS CURRENT (2026-09-07): VR-33 - rotation and grip built, then confirmed

### Run 1 found the fault, and the instrument named it

The first rotation build refused on **every** draw: `rotate=1 placed 0 refused
116908 (B*F is not a proper rotation)`. The hands looked unchanged and SHIFT+F7
appeared to do nothing, because the grip capture sits behind the same gate.

**The guard was wrong, not the game.** The draw's camera basis is RIGHT-handed,
so the pose mapping between XR and the game's camera-relative frame is a
MIRROR - exactly what a right-handed runtime and a left-handed engine produce.
A reflection is a coordinate convention: it carries through by full basis change
and CANCELS between the controller orientation and the grip transform, so what
reaches the palette is a proper rotation either way. The guard now requires
orthonormality only, records the parity, and a new self-test case
(`improper_basis_roundtrip`) pins the whole chain under a mirrored mapping:
capture exact, `det +1.0000`, a 37 degree controller turn giving a 37.00 degree
hand turn.

Two things the run confirmed on the way: the **fail-soft held** - all 116,908
draws still placed translation-only, so nothing regressed and the run simply
looked like the previous build - and the **pose tick and the hand draws are one
thread** (14224), with 0 stale snapshots over 11,881 publications.

A pending grip capture now logs a warning naming why it has not been consumed,
so a press can never silently do nothing again.

### What is armed right now

The installed build is Release with `[Hands] PaletteRotate=1` and the grip
transform at identity. **Launch the game; nothing else needs doing.** The full
test list, with expected outcomes and what each failure would mean, is section 3
of `docs/dishonored/VR-33-HANDS-AND-WEAPONS.md`.

**The hands will start at a wrong ANGLE.** G is identity until it is captured,
so they track the wrists at a fixed offset. **SHIFT+F7** solves G for both hands
and prints six numbers for the ini; the hands snapping to the game's own
orientation at that instant is what the calibration means.

`PaletteRotate=0` returns to the headset-confirmed translation-only build.

### The one question this run answers

Does the candidate palm frame (`src` in `ms/palette/frame:`) move when the GAME
animates the hand, and stay put when only the fingers move? Nothing offline can
answer it: all 28 saved packets are one near-idle pose. If `src` never moves
during a melee swing or a power, the dominant palette slot is not the palm's
frame and the orientation source changes to a validated landmark fit.

### What was corrected before building

An external review found three blockers in the first draft of the plan, all
fixed:

1. **The motion gate was impossible.** It expected the game's palette to turn
   when the tester rolled a physical wrist, with no mechanism connecting them.
   Withdrawn; three orientations (`src`, `ctl`, `out`) are now logged under
   separate names and are expected to be INDEPENDENT while rotation is off.
2. **The orientation conversion reintroduced head-driven rotation.** A pose
   orientation maps controller-local axes into another frame; a similarity
   transform changes the basis of a rotation operator, which is a different
   object. The correct form is `O_C = B * F * transpose(R_head) * R_ctl` with
   `F = diag(1,1,-1)`, matching the physical mapping the working position path
   already performs. The counterexample is now a shipped test that the rejected
   formula fails by 180 degrees.
3. **The grip capture mixed spaces.** The source frame is component-local and
   the controller camera-relative; it is carried through the draw's own
   LocalToWorld first.

### Measured before any of it shipped

* **20 of 20 frame-maths cases pass** (`src/tools/frame_test`, and the same
  suite runs from `DllMain` into every log). They include the head-turn
  counterexample, the grip round trip, the pivot, and the proof that rotation
  OFF is bit-for-bit the shipped translation behaviour.
* **All 28 saved packets decompose** through the shipped code: dominant slot 10
  in every one, uniform scale 0.999511659 to 0.999512255, worst anisotropy
  6.0e-07, worst orthonormality residual 9.8e-07. Three orders of magnitude
  inside the tolerance.
* **The single slot's frame moves 1.05 degrees** across those packets where the
  weighted blend looked frozen to five decimals - so the slot does respond to
  the engine's animation. It is not yet evidence that it follows the PALM.
* **The three hand shaders' normal path is closed.** Two of them run normals and
  tangents through the same palette rows, so a rigid correction carries the
  tangent frame; their `WorldToLocal` is a view/light conversion and is NOT
  touched. ENGINE_NOTES carries it.

### Next steps

1. The headset run above. Report the six grip numbers and whether `src` moves
   with the game's animation.
2. Put the solved G in the ini and re-judge orientation.
3. Then the weapon assembly: a stable grip/root source frame, ONE root
   correction, and every member's animated transform preserved RELATIVE to that
   root. Independently pinning each animated part to a controller-relative
   target cancels its own animation.
4. Then firing. The visual weapon stage explicitly accepts that a released bolt
   returns to the native origin; correcting it is the firing stage.

Carried and deliberately not done: a render-view ticket for the eye, the
`same eye` counter (an instrumentation limit, not 776 repeated eyes - no
threshold tuning), head look-ahead against hand sampling time, one canonical
metres-to-units conversion (logging only for now), and mesh geometry size.

## PREVIOUS CURRENT (2026-09-07): VR-33 - the hands are at the controllers and correct

### Confirmed in the headset

The hands **track the controllers, are correctly scaled, occlude against world
geometry, and hold position through head turns.** Tagged `vr33-hands-working`.

**The full record is `docs/dishonored/VR-33-HANDS-AND-WEAPONS.md`** - the mechanism, the
engine facts, the seven approaches that failed and why, and the instrument
failures that cost the most. Read that before touching this code; most of what
looks like an obvious improvement has already been tried and measured.

### The mechanism, in one paragraph

The bone palette's output is the component's LOCAL space; `LocalToWorld` (c231)
takes it to a camera-relative world frame and `ViewProjectionMatrix` (c0) to
clip. Placement re-skins the palm from the game's own palette every frame and
translates by `target - q`, so the animated baseline is subtracted rather than
left underneath. The camera basis comes from the VP's rows. The eye is decided
once per Present from the right-axis jump in `LocalToWorld`'s translation, whose
sign gives left or right absolutely. Register numbers are parsed from each
shader's CTAB, never hard-coded - three shaders draw this mesh and they
disagree.

### Next phase

1. **Rotation and grip.** The hands keep the engine's animated orientation, so
   they look posed rather than gripping. Full rigid composition, per-component
   conjugation `D_local = inverse(C) * D * C`, a stable palm frame from
   deliberately chosen landmarks rather than the current position patch, and
   basis conversion by conjugation - the translational basis has determinant
   -1 and is a coordinate convention, not a rotation.
2. **The weapon assembly.** Crossbow, loaded bolt and reload parts, moved by the
   same common transform through each component's own `C`. A hand-local delta
   cannot be copied into a weapon-local palette.
3. **Gameplay consumers.** Firing aim, projectile origin, melee. A GPU edit does
   not move them.

Carried and deliberately not done: one canonical metres-to-units conversion
(`[Hands] WorldScaleUU` 100 against `[PosTrack] Scale` 108), pose timing (head
look-ahead against hand prediction), a render-view ticket to carry the eye
rather than infer it, and a harness case for the feature-gate regression that
cost three runs.

## PREVIOUS CURRENT (2026-09-07): VR-33 - the hands are at the controllers; the stereo eye offset is the last placement fault

### Where this is

**Placement through the measured chain WORKS.** The hands track the
controllers, hold position through head turns, and now occlude correctly
against world geometry. The tester rates it the best result of the session.

The path is measured, not inferred. Read from the shaders' own disassembly:

```
p_local = ( sum_i w_i * BoneMatrix[idx_i] ) * ( v0 * MeshExtension + MeshOrigin )
p_cam   = LocalToWorld * p_local            (camera-relative world, uu)
clip    = ViewProjectionMatrix * p_cam
```

Placement is `target_local = Rl^T * (d_cam - t)`, `T = target_local - q_local`,
with `LocalToWorld` and `ViewProjectionMatrix` read from the device at each
draw through the register indices that shader's own constant table declares.
`ENGINE_NOTES.md` carries the full finding.

### The remaining fault: the eye offset is not applied

**Symptom:** correct in each eye individually, far too large with both open.

**Diagnosis:** `method=reentry` is live and the scene is genuinely drawn twice
(`L/s=88 R/s=88 mono/s=0`), so the world has correct stereo. But `d_cam` is
computed from the HEAD CENTRE and used unchanged for both eyes. Placing the
hand at the same camera-relative offset in each eye puts the two hand images an
IPD apart in world terms - the disparity of an object at infinity. A hand-sized
object at infinite disparity reads as a giant hand far away, which is exactly
what is reported.

**The fix** is to subtract that eye's offset: for each eye,
`d_cam_eye = d_cam_head -/+ (IPD/2) * right_axis`, with the right axis already
recovered from the ViewProjectionMatrix. IPD is known (63.0-63.1 mm measured).

**The blocker** is that the draw does not yet know which eye it is drawing.
The capture records the eye as `-2`, "not identified", which was flagged as a
known gap when the mono method made it harmless. Under `reentry` it is not
harmless and is now the last thing between this and correct hands.

### What is measured and settled

* Three shaders draw this mesh. Only one is depth-crushed (`MaxZ 0.001`); the
  other two use the full range. Placement currently applies to all three.
* Register layouts differ per shader and are parsed from each shader's CTAB.
  One shader defines c4 as an immediate that disagrees with the device.
* The shader does NOT normalise skin weights, but the anchor's weights sum to
  exactly 1.0000, so it is harmless here.
* The depth-range lever works: restoring `MaxZ` 1.0 for our draws gives correct
  occlusion against world geometry.
* Scale is still formally unmeasured, but the "huge" report is now attributed
  to the eye offset rather than to scale, and should be re-judged after it.

### Next steps

1. Identify the eye at the hand draw under `reentry`, and apply the eye offset.
2. Re-judge apparent scale once stereo is correct.
3. Then Build B: controller orientation and a grip-to-palm transform.
4. Outstanding and deliberately not done: pose timing (head look-ahead vs
   hand), the harness case for the feature-gate regression, render states in
   the capture, and a rigorous uu/m measurement.

## PREVIOUS CURRENT (2026-09-07): VR-33 - the SkelControl lane is closed, the palette route is next

### Where this branch is

The arm/hand split is healthy and confirmed in the headset: hands cut at the
wrist, arms hidden, caps present, cap colour approved, ring at the measured
-4.9. Two PRs are open and unmerged - #19 (VR-31, the split) and #20 (VR-53 and
VR-51, the desktop mirror eye pin and the pause-menu session loss).

**VR-33's native route is closed, and it is now evidence rather than an
artifact.** The earlier "zero of 64 SkelControl objects advance" reading was a
truncated scan - the sweep stopped when its 64-entry comparison table filled,
so 64 was the array's capacity, not a population. Fixed and re-run the same
day with the hands subsystem LIVE (`[Hands] Enabled=1`, `[Mode] GamepadOnly=0`,
the state every earlier measurement lacked), 34 consecutive samples read:
**103,117 GObjects entries walked, 66 SkelControls, all 66 tracked, 0
untracked, 0 advancing.** No SkelControl is evaluated on this build, so a write
to one cannot move anything. The old table had missed exactly two objects, so
the retracted reading was right by luck; it is measured now. This also does
retire the 38.x "9,000 writes a second outrun the recompute" reading. See
ENGINE_NOTES, "SkelControls are NOT evaluated on this build".

The scan's cost is measured too: **505-520 ms of game-thread stall once per
second**, attributed by the perf line to `out/idle`, ~46 display slots at
90 Hz. It is unplayable while armed. It is back OFF in the installed ini.

### What IS established, and is worth keeping

* `handAttachment_L/R_jnt` are children of `hand_L/R_jnt`, from validated
  engine parent walks. If anything ever does move a hand joint, the weapon
  attachment is beneath it.
* The camera is on the spine/head branch; the two arms meet only at `Root_jnt`.
  A per-side edit at or below a hand cannot disturb the view or the other hand.
* Skeleton indices are NOT palette slots: `hand_L_jnt` is 54 and
  `handAttachment_L_jnt` 56, against a 48-entry palette.
* The arm mesh's declaration uses streams 0 and 1; a stale stream-2 binding was
  what kept costing the wrist caps. Bound is not used.
* Full bone table, class Super offset (+0x44), socket table with parent bones,
  and the control field offsets - all in ENGINE_NOTES.

### The next step

**The draw-scoped palette backend, hands only.** Build a private palette per
hand by applying one common rigid delta D to every skinning matrix that draw
consumes, and draw each hand under its own palette. Finger animation survives
because `sum_i w_i (D M_i) v = D (sum_i w_i M_i v)`, so this is not a static
hand. The two hand classes already have independent index ranges in `MsDraw`,
which is where it goes.

Its honest cost: it moves pixels only. Weapons, muzzle effects and firing aim
stay on the engine's transform, which a GPU edit does not touch, so the weapon
half of VR-33 needs a separate mechanism. The tester has already accepted that
the crosshair can be faked separately.

The gate is passed - the tick scan has been run and the native lane is shut.
The palette backend is the route, and it is the work in front of this branch.

**What it is not**: it moves pixels only. Weapons, muzzle effects and firing
aim stay on the engine's transform, which a GPU-side edit does not touch, so
the weapon half of VR-33 needs a separate mechanism. The crosshair can be
faked separately and that has been accepted.

### How the pieces already on disk fit

The backend is a join of two things that exist, not new machinery.

* `hkSetVSConstF` already carries a whole-palette rewrite - the 30.70/71 hand
  drive in `core/framework/vs_const_hook.cpp` applies a per-hand rotation and
  translation to every bone matrix in a `c6` upload. That is exactly the
  `D * M_i` the finger-animation argument needs.
* What it lacks is DRAW SCOPE: it identifies a rig by upload `count` and
  ordinal, so it cannot give the two hands different deltas when they share
  one upload.
* The missing half is on the other side. `MsDraw` in
  `game/dishonored/hands/mesh_split.cpp` already has independent per-class
  index ranges (`MS_CLS_HAND_A` / `MS_CLS_HAND_B`).

So: cache the game's last `c6` block, and have `MsDraw` upload `D_L * M`
before the hand-A range and `D_R * M` before hand-B, restoring the original
block afterwards. D3D9 constants are current state, not one-shot - the trap is
already recorded at `hands/draw_census.cpp:334`.

### Build and deploy state

The installed `d3d9.dll` is code-current with this branch - built and installed
2026-09-07 12:55, three DOCS-ONLY commits behind HEAD. Its last run stamped
`alpha-334-g88eefb61-dirty`; HEAD is `alpha-337-g220c5a28`, and the difference
is ENGINE_NOTES and STATUS only. Rebuild anyway before trusting a build id.

**Verified in the headset:** the split, the clip, the caps and cap colour, the
ring at -4.9, the arms staying hidden, and the SkelControl lane being inert.
**Built but NOT headset-verified:** the desktop mirror eye pin and the
pause-menu session fix on PR #20 - both still need the run in
`docs/dishonored/DESKTOP_MIRROR.md` section 7.

**All diagnostics are now disarmed on the dev rig**: `[Hands] BoneQuery=0`,
`HandMoveTest=0`. `PoseReport` has no key and defaults on; it is read-only and
prints once. The GObjects tick scan is gated behind `HandMoveTest` and is the
thing that cost frame rate - leave it off.

### Traps this session paid for

* A too-narrow grep produced two confident false claims ("the codebase has
  never called a UE3 function", "g_peReentry is read nowhere"). Both were
  wrong; grep the tree, not one file.
* An instrument that cannot fail its own hypothesis is worse than none: a
  function-local `static` made a false negative read as a measurement, and a
  walk that printed ROOT for three different endings made a broken chain look
  complete.
* The GObjects-wide tick scan is heavy enough to be felt in the headset. It
  ships OFF and should stay off.

## PREVIOUS (2026-09-06): VR-33 - hands and weapons where the controllers are

This is the working branch for VR-33 and it is the CONTINUATION OF BOTH open
PRs: the arm/hand split (VR-31, PR #19) and the desktop mirror eye pin plus the
pause-menu session fix (VR-53 / VR-54, PR #20). Both are merged in here, and
neither has been merged to `VR-Main`.

### Read this before installing anything

**The two PR branches do not work on their own.** Branch VR-31 has no API layer
guard, and without it `xrCreateInstance` fails with `XrResult(-32)` on both the
native runtime and the SteamVR shim, so the game runs flat with no VR at all.
The guard is on the VR-53 branch. That was found by installing the VR-31 branch
alone on 2026-09-06 and losing VR entirely.

**So install from THIS branch**, not from either PR branch, for as long as both
PRs are open. The PR branches are for review; this one is what runs.

### What VR-33 is

The hands and the weapons go where the VR controllers are. VR-30 took the head's
yaw out of the pawn's facing, which is what lets a hand sit still in the world
while the head moves; VR-31 decided the presentation and cut the hands free of
the arms. This branch is the transform work that follows from both.

Nothing has been written for it yet.

### Open questions carried in from the two PRs

* **The cap's colour has not been re-confirmed** since the UV decode was
  widened. The mode had never run - this asset packs TEXCOORD0 as `FLOAT16_2`
  and the check demanded a `FLOAT2` - so every cap took ring vertex 0, an
  arbitrary choice that happened to look right.
* **Whether the ring's shape changes with the POSE** is unanswered. The cut is
  computed in the bind pose and seen in the animated one.
* **The desktop eye pin and the pause-menu fix are unverified in the headset.**
  Section 7 of `docs/dishonored/DESKTOP_MIRROR.md` has the three checks.

### References

* `docs/dishonored/ARM_HAND_SPLIT.md` - the split, every key and hotkey, the traps
* `docs/dishonored/DESKTOP_MIRROR.md` - the eye policy and the hold fix
* `docs/dishonored/BRIEF-eye-flicker.md` - the hypothesis graveyard, ANSWERED

## MERGED IN (VR-53 / VR-54, PR #20) (2026-09-06): VR-53 / VR-54 - the desktop had no eye policy, and a hold banked empty layers

This branch is the frame path and nothing else. The arm/hand split worked on in
the same sessions was split out onto its own branch and is VR-31.

**The full reference is `docs/dishonored/DESKTOP_MIRROR.md`**; the hypothesis
graveyard that led to it is `docs/dishonored/BRIEF-eye-flicker.md`, now marked
answered. This section is the handoff summary only.

### What was actually wrong

`hkPresent` calls the game's original `Present` for EVERY eye draw, and
`mirror_present()` in the runtime layer had never implemented the D3D9 copy -
its own comment said so. So the game WINDOW showed L(k), R(k), L(k+1), R(k+1)
while the headset received correct pairs the whole time. A recording of the
window alternates between two camera positions one IPD apart, which is exactly
what alternate-eye rendering looks like, and the diagnosis had been aimed at
the headset path for several sessions on the strength of it.

The headset was never doing AER. The desktop had no eye policy at all.

`core/gfx/desktop_eye.cpp` pins it: snapshot on the left eye's present, re-blit
over the right eye's present AFTER that eye's XR capture. The runtime layer
owns the WHEN and the new module owns the HOW, so `openxr_runtime.cpp` gains a
hook pointer and nothing else.

### The pause-menu session loss (VR-54)

On a hold-only present the submitted copies are `holdProj` / `holdViews` /
`holdQuad`, not the empty `proj` / `projViews` / `quad` locals - but the hold
sets `layerCount = 1` and the snapshot bank keyed off `layerCount`, so it
overwrote a good snapshot with zeroed structures and left it marked valid. The
next hold submitted null handles and a zero view count, `xrEndFrame` answered
`XR_ERROR_HANDLE_INVALID`, and the session stood down. Banked on
`builtNewLayer` now.

Still open and tracked separately: a saved layer holds swapchain HANDLES, not
pixels, and OpenXR composites the most recently RELEASED image, so preserving a
completed PAIR needs retained images rather than a retained structure.

### Retracted, not tuned

The "30 % of ticks double" reading and the stand-down guard built on it are
REMOVED. That window straddled a pause menu, an `xrEndFrame` failure and
session teardown; the windows either side read 78/78, 86/86, 87/87, 81/81. The
guard would have disarmed a healthy renderer every time a session dropped. The
`camera/eyetrace` line is corrected too - its ring samples constant uploads,
not presents.

### The run this needs

Look at the game window: one view, no alternation, while the headset keeps
correct stereo depth. `desktopeye:` in the log every 15 s should show snapshot
and re-blit counts EQUAL and non-zero. Then open and close the pause menu
several times: no `XR_ERROR_HANDLE_INVALID`, no session teardown.

### Not addressed here

Performance: ~78 complete pairs/s against a 90 Hz headset, ~9.7 ms of D3D9 GPU
span per tick, 15.7 Mpixel per pair at 2750x2850. That is the next subject.
## MERGED IN (VR-31, PR #19) (2026-09-06): VR-31 - the hands are cut from the arms, clipped and capped

This branch is the arm/hand split and nothing else. The desktop mirror eye pin
and the pause-menu session loss found in the same sessions were split out onto
their own branch and are VR-53 and VR-54.

**The full reference for everything below is `docs/dishonored/ARM_HAND_SPLIT.md`**
- the derivation, every ini key and hotkey, how to read the log, the traps, and
what is still unverified. This section is the handoff summary only.

### Where it stands

The arms and hands are one skinned triangle list with one material, so the
geometry is cut by us. The cut is derived per arm from BONE INFLUENCE, shaped
as a plane perpendicular to the forearm, with the triangles that straddle that
plane CLIPPED at it so the boundary is the plane itself rather than a row of
triangle edges. The open end that leaves is CAPPED with a two-sided disc
coloured from the mode of the boundary ring's own texture coordinates.

The ring ships at the tester's measured position: `[Hands] WristCutA` /
`WristCutB` = **-4.9**, read off the `ms/wrist` line at the end of the
2026-09-06 headset walk. It is asset-relative, so it survives a level load.

### Verified in the headset

* The split itself: the hands draw, the arms do not.
* The plane, the clip, and the ring walk with Numpad + / -.
* The cap, at the point where it was still falling back to ring vertex 0 for
  its colour.

### NOT verified

* The cap since the UV decode was widened. The log had been answering `NO
  TEXCOORD0` because this asset packs its texture coordinate as `FLOAT16_2` and
  the check demanded a `FLOAT2`, so the colour MODE never ran and every cap took
  ring vertex 0 - an arbitrary choice that happened to look right. The mode now
  runs, so **the cap's colour may differ from the one that was approved**. That
  is the first thing to look at on the next run.
* Whether the ring's shape changes with the POSE. The cut is computed in the
  bind pose and seen in the animated one. If the ring's shape changes as the arm
  moves, a slant has a different cause and a different fix than the axis; if the
  slant is fixed relative to the arm whatever the pose, it is the axis. One run
  settles it and nothing else can.

### The run this needs

Look at your hands. The wrist ends in a solid disc, no see-through, from every
angle including down the arm from the elbow end. Walk the ring with Numpad
+ / - and watch the colour track it - sleeve on the cuff, skin past it. Then
say whether the colour is the same one as before, because the decode fix could
have changed it.

In the log: `ms: cut end CAPPED` names the colour path that ran and the winning
ring vertex; `ms: triangles by class` carries the whole result in one line.

## PREVIOUS (2026-09-06, session 20d): VR-31 - a reversed winding was eating a third of the cut

The clip RAN - the log says 54 triangles cut into 108 new vertices, stream 0
ours - and the result was still ragged, with some of the boundary on one clean
line and the rest missing or floating loose. That pattern named the bug.

### The bug: one third of every clip was wound backwards

`MsClipTri` picked the three corners by ASCENDING INDEX (`in[0], out[0],
out[1]`) instead of in cyclic order. When the odd vertex out is the MIDDLE one
of the three - one case in three - that reverses the triangle, and a reversed
triangle is culled. Two thirds of the cut came out on a clean line and one
third vanished, which is exactly what was reported.

Fixed by taking the odd vertex `i` and its neighbours as `(i+1)%3` and
`(i+2)%3`, so both halves keep the source triangle's winding.

### The slant: the ring may be square to the wrong direction

"Taking too many on the other side of the forearm" is a ring that is not
perpendicular to the arm. The axis was the longest direction of the arm's
triangle cloud (PCA), which a tapered sleeve can lean off the bone.

`MsBoneAxis` derives it from the SKELETON instead: the hand bone minus the bone
it hangs off, that bone being the hand's graph neighbour whose centroid is
farthest away. No names, no hierarchy, same as everything else here. Both axes
are computed, the angle between them is logged per side, and `ms axis
bone|pca` switches live - which one is right is a question about this asset, not
about geometry, and one press settles it. Default is the bone.

### Also

`+` / `-` auto-repeat after 400 ms, because at the ultrafine step the ring moves
0.1% of the arm per press and placing it by hand would be a hundred taps.

### Still unruled-out, and the check for it

The cut is computed in the mesh's BIND pose and seen in the ANIMATED one. If
the ring's shape CHANGES as the arm moves, that is the cause and the fix is a
different one; if the slant is fixed relative to the arm whatever the pose, it
is the axis. That is a thing the tester can see in one run and nothing else can
answer.

## PREVIOUS (2026-09-06, session 20c): VR-31 - the straddling triangles are CLIPPED

Headset run on the plane cut: the steps were much better, the edge was still
jagged - spikes hanging off the cuff in the screenshot. Diagnosed and fixed
here, untested.

### Why a plane was not enough

The plane fixed the JUMPS but not the OUTLINE. A triangle straddling the cut
was still kept or dropped whole, so the boundary was a sawtooth one triangle
high; on the coarse cuff geometry that is a set of spikes. No edge RULE fixes
that - keeping only triangles entirely past the plane trades spikes for
notches. The boundary has to stop being made of original triangle edges.

### The clip

A triangle crossing the plane is split into the part on each side, with new
vertices interpolated along the two crossing edges. Both halves are emitted
into their own class, so `arms` stays the exact inverse of `hands` and `all`
still rebuilds the original mesh. The boundary is then the plane itself.

That needs a VERTEX buffer of ours as well as an index buffer, because the new
vertices do not exist in the game's. Every stream-0 element is interpolated by
its declared type; blend INDICES come whole from the nearer parent vertex,
because a bone index is a name and not a quantity, and the weights go with them
so they cannot end up naming the wrong bones. Both buffers are `D3DPOOL_MANAGED`
with room to grow, so moving the ring does not recreate them and `hkReset` has
nothing to release.

**The one precondition, checked and logged**: stream 0 must be the only stream,
because re-basing the index list onto a buffer of ours would desynchronise any
second stream the game had bound. A mesh with more streams falls back to whole
triangles with the reason named.

### Smaller steps

`Numpad .` cycles the knob's step: 2% of the arm, 0.5%, 0.1%. Default 0.5%
(`[Hands] WristStep`).

### New defaults

`WristEdge=3` (clip), `WristStep=1` (fine). `ms edge 0|1|2|3` still selects the
whole-triangle rules for comparison - 3 is the only one whose boundary is the
plane rather than a row of triangle edges.

### The run this needs

The wrist should end in a clean ring with no spikes. `ms status` says how many
triangles were cut and how many vertices were made; if `stream 0 belongs to the
game` appears, the clip was vetoed and the reason is in the read lines. Then
`Numpad .` for finer steps, `+` / `-` to place it, and the `ms/wrist` numbers
become `WristCutA` / `WristCutB`.

Compare against `ms edge 1` to see what the clip is worth - that is the best of
the whole-triangle rules and should still show a notched edge.

## PREVIOUS (2026-09-06, session 20b): VR-31 - the cut is a PLANE across the forearm

The bone-influence split works and was confirmed in the headset: both hands
present, both arms gone, at `WristScale` 0.70 on both sides (radius 21.7,
2109 hand triangles per arm). Two faults in the SHAPE of the cut, both fixed
here and neither yet tested.

### The sphere moved in whole bones

The tester's walk is in the log and it is unambiguous: from scale 0.50 to 0.67
the triangle counts did not change AT ALL, then one press flipped 156 triangles
per arm at once. A bone is inside the sphere or outside it, so every triangle
that bone dominates changes class together. What is left is the outline of a
bone's influence region, which is why moving the knob made the edge jagged
rather than moving it.

### A plane cuts a cylinder in a circle

`MsPlaneDerive` per arm:

1. **The limb axis** by power iteration on the covariance of the arm's triangle
   centroids. Two passes - a rough axis over the whole arm, then a refined one
   over a band around the rough cut, so the ring is perpendicular to the
   FOREARM and not to the whole limb with the hand's mass pulling on it. The
   refinement logs how many degrees it moved, and refuses past 40.
2. **The starting position** keeps exactly the number of triangles the sphere
   was keeping at the tester's 0.70. Changing the shape of the cut does not
   move it: the look that was settled on survives, it just stops being blobby.
3. **The knob moves the plane in LENGTH** - 2% of the arm per press - so the
   ring travels smoothly instead of waiting for a bone to flip.

The sphere is kept, not deleted: it is the seed, the fallback
(`ms shape sphere`), and the thing that proved the classification works.

### New keys and defaults

`[Hands] WristPlane=1` (plane), `WristScaleA/B=0.70` (the tester's measured
seed), `WristEdge=0`, `WristCutA/B` unset (derive). Setting `WristCutA/B` pins
the ring in mesh units from the hand bone and survives a level load - the knob
and `ms cut` both print exactly that number, so a good look becomes the default
without another walk.

`ms cut <a> [b]` places the ring, `ms shape plane|sphere`, `ms edge 0|1|2`
(kept when the centroid / all three vertices / any vertex is past the plane).

### The run this needs

Look at the hands: the starting picture should be the SAME amount of forearm as
the tester left it, but ending in a clean ring instead of a jagged edge. Then
`Numpad +` / `-`: the ring should slide smoothly, a small step each press, with
no chunk of triangles vanishing at once. When it looks right, read the
`ms/wrist` line for the two numbers and they become `WristCutA` / `WristCutB`.

If the ring comes out slanted rather than square across the forearm, the
`forearm axis refined N degree(s)` line says how far the second pass moved and
is where to look. `ms edge 1` is the alternative if the one-triangle sawtooth
is visible up close.

## PREVIOUS (2026-09-06, session 20): VR-31 - the cut is DERIVED from bone influence, per arm

Branch `claude/vr-31-arm-hiding-floating-hands`, based on
`claude/vr-30-decouple-arm-hand-movement` (PR #18 still open). Installed
Release. **Untested - built and installed, not run.**

### What replaced the eyeballed mask

The 32-slice mask cut by PERCENTAGE of the triangle order, and the two arms sit
in different, non-aligned regions of that order, so a mask tuned on the left
hand took too much of the right. No amount of further eyeballing fixes a cut
that cannot express the shape.

`src/game/dishonored/hands/mesh_split.cpp` classifies each TRIANGLE by the
bones that actually move it, which is per-arm by construction:

1. **Read once.** The draw's index range and vertex window are copied out of
   the game's own buffers - the first time this project has read them - and
   VALIDATED against facts the data must satisfy (indices inside the draw's own
   vertex window, bone indices below the palette size, weights summing to 1,
   finite positions, a non-degenerate bbox). A `D3DUSAGE_WRITEONLY` buffer may
   legally hand back an uninitialised page on a read lock; that is why the
   descriptors are logged BEFORE the lock and why a failed validation refuses
   instead of carving the mesh up from noise.
2. **Two arms from the SKINNING GRAPH.** Two bones are adjacent when some vertex
   is meaningfully moved by both. Separate limbs share no vertex, so the graph
   falls into two components - no bone names, no hierarchy, no engine
   structures. A geometric widest-gap split is the fallback and says so in the
   log when it is used.
3. **The wrist from the bone SPACING.** The hand bone is the one with the most
   neighbours (a forearm joins two things; a hand joins the forearm and every
   finger). Sort the side's bones by distance from it and the biggest gap in
   that list is the wrist. Derived, not eyeballed.
4. **Per triangle**, the class holding most of its influence weight wins, so a
   triangle straddling the wrist follows the bones that move it. Stray sleeve
   shards go with the arm bones they are weighted to, which is why they should
   disappear without a special case.
5. **One index buffer of our own** (`D3DPOOL_MANAGED`, so `hkReset` has nothing
   to release), classes emitted contiguously with both hands adjacent - so
   "both hands, no arms" is ONE draw call, not a run per fragment.

### Also fixed: the stale-lock weakness

The mesh lock is now **auto-armed by the mesh's measured signature** (prims
4448, verts 2771, skinned declaration, triangle list - all ini keys), and
`DcTick` releases an automatic lock that has not been drawn for 300 frames so a
level load that recreated the buffers re-arms on the new pair. An automatic lock
**fails soft**: a mesh it cannot split is drawn exactly as the game asked, on
both the indexed and non-indexed paths. A hand-armed lock (Numpad 6) is
unchanged and still drops what it is told to.

### Defaults and controls (everything ships ON)

`[Hands] ArmSplit=1 ArmSplitAuto=1 ArmSplitMode=1` (mode 1 = HANDS).

    Numpad 0   next mode: hands -> all -> arms -> other -> off -> hands
    Numpad +   keep MORE as hand (the cut moves up the arm)
    Numpad -   keep LESS as hand (the cut moves toward the fingers)
    Numpad *   which arm + / - moves: both -> side A -> side B
    Numpad /   re-derive the whole split from the buffers
    Numpad 5   release everything, and stop the automatic re-arm

Seam: `ms status | off | hands | arms | all | other | rebuild | wrist <n> |
side <n>`. The 32-slice mask and `dc mask s19` are untouched and remain the
fallback if a driver refuses the read.

### The run this needs

Load a save with the sword and crossbow out and read `dishonored_vr.log` for
`ms:`. Expected: `ms: ib fmt=... usage=...`, then a validation line with zeros,
then two graph components, two wrist radii, and a class count with BOTH hand
classes non-zero. In the headset both hands should be present and both arms
gone, with no percentage tuning at all. `Numpad 0` once shows the whole mesh
through our buffer (the A/B - it must look exactly like stock); a second press
shows the inverse cut, arms with no hands, which is the cheapest proof the
classification is real and not a coincidence.

If a hand is too short or too long, `Numpad +` / `-` moves that wrist and the
log prints the radius - ONE number per arm instead of 32 slice bits.

### Still not done

- The acceptance list: Blink, Devouring Swarm, Windblast, weapons, head and
  stick turns, crouch, reload, checkpoint reload. Power effects are
  socket-driven, so verify the SOCKETS follow, not just the fingers.
- `HmDrawIntoEye` asks the METHOD whether it wants a projection layer, not
  whether the RUNTIME submitted one.
- `FindRefSkel` does not enforce the validation its comment claims.
- Controller wrist placement is still a separate gate; `[Hands] Enabled=0`.

## PREVIOUS (2026-09-06, session 19): VR-31 - FLOATING HANDS WORK, the cut needs per-arm ranges

Branch `claude/vr-31-arm-hiding-floating-hands`, based on
`claude/vr-30-decouple-arm-hand-movement` (PR #18 still open). Installed
`alpha-316`. **No PR opened - the user asked for commit and push only.**

### The result: the game's OWN hands, floating, with their own animation

Headset-confirmed twice with screenshots: Corvo's real hand holding the sword,
and the real hand holding the crossbow, with the arm gone from the cuff. This is
Arkane's geometry, Arkane's skinning and Arkane's animation - nothing is
replaced - so the powers animate the fingers exactly as they always did, which
was the whole requirement.

### How it works, and why it needs no index buffer

The first-person arms and hands are ONE skinned mesh (`bones=48 skin=IW`, 4448
triangles, 2771 vertices) drawn by three vertex shaders. The mesh is a triangle
LIST, so any contiguous run of its triangles can be drawn on its own by shifting
`startIndex` and shrinking `primCount`. **Nothing is read, captured or
replaced** - the game's own buffers stay bound and we ask for part of the list.

The target is identified by BUFFER PAIR (stream-0 VB + IB), not by bone-palette
size, so every pass over it is caught including ones the palette-gated census
never saw. `DrawPrimitive` is hooked as well as `DrawIndexedPrimitive`.

### THE MEASURED CUT, and its limitation

The tester's mask, 12 of 32 slices: **`0x3001D237`**

    slices  1,2,3   triangles  0%.. 9%
    slices  5,6     triangles 12%..18%
    slice  10       triangles 28%..31%
    slice  13       triangles 37%..40%
    slices 15,16,17 triangles 43%..53%
    slices 29,30    triangles 87%..93%

Restore it with **`dc mask s19`** instead of repeating twelve headset presses.

**It is NOT the default and must not become one yet.** It was tuned for the
LEFT hand and takes too much of the right. The reason is structural and is the
next problem to solve: **the two arms occupy different, non-aligned regions of
the one triangle list**, so a single set of slices tuned by eye on one hand
cannot be correct for the other. The slices are also PERCENTAGES of this mesh's
primCount, so the mask means nothing on a different asset or LOD.

### Next steps

1. **Two independent masks, one per hand.** The cut has to be chosen per arm,
   because the arms are not symmetric in the triangle order. Either two masks
   the tester tunes separately, or - better - classify triangles by BONE
   INFLUENCE so the cut is derived rather than eyeballed.
2. **Finer than 32 slices** where a slice straddles hand and arm. 32 was enough
   for the left hand and not obviously enough for the right.
3. **Sleeve shards.** Small black triangles survive the cut (visible in the
   session screenshots); they are weighted to arm bones and sit outside the
   marked runs.
4. **Then the acceptance list** from review: Blink, Devouring Swarm, Windblast,
   weapons, head and stick turns, crouch, reload, checkpoint reload. Power
   effects are socket-driven, so verify the sockets follow, not just the
   fingers.
5. **Controller wrist placement is still a separate gate** and untouched;
   `[Hands] Enabled=0`.

### Known weaknesses in the instrument, deliberately not fixed

- The mesh lock is armed from a censused row, so a level load that recreates the
  buffers leaves it stale and silently drawing everything. Needs a re-arm path
  before this is a shipping lever rather than a diagnostic.
- `HmDrawIntoEye` asks the METHOD whether it wants a projection layer, not
  whether the RUNTIME submitted one.
- `FindRefSkel` does not enforce the validation its comment claims.

## PREVIOUS (2026-09-06, session 19): VR-31 - the cycler is WALKED, the hands had an uninitialised matrix

Branch `claude/vr-31-arm-hiding-floating-hands`, based on
`claude/vr-30-decouple-arm-hand-movement` (PR #18 still open, so this PR takes
`Ref VR-31`). Installed build `alpha-308`.

### VR-31's question is ANSWERED: route (d) cannot give hands

The cycler was walked in the headset on `alpha-307`. Four presses of Numpad 3
removed, in order: **both arms and hands together**, sword, crossbow, bolt -
exactly the plan the engine's own `GetNumElements` produced. Attribution is now
MEASURED, not recalled.

So `Skm_Player` carries arms and hands as ONE material section, route (d) gives
**floating weapons**, and it can never give floating hands.

**Note the limit of that conclusion** (raised in review, and it is right): one
shared material section rules out a MATERIAL-only split. It does not prove our
own hands are the only possible route - preserving Dishonored's original
animated hands would need a different geometry-FILTERING approach (route (b),
the c6 bone palette, x144 = arms, is untouched and is where that would start).
Custom hands are the route being taken, not the only one that exists.

### The hands drew every triangle and showed nothing - the cause is found

First run of the rebuilt hand pass: `calls=480 draws=480 tris=120960 last=drew`,
252 triangles per present (both models, complete), nothing on screen.

**Cause, found by code review rather than another run:** the transform did
`float B[9]; RtdBuildYPR(rad, B);` and `RtdBuildYPR` is compiled ONLY under
`-DDVR_WITH_LEGACY=ON`. Every shipped build takes the empty stub in
`src/legacy/legacy_stubs.inc`, so `B` was never written and every hand vertex
was rotated by nine floats of stack garbage. The counters stayed healthy because
the geometry really was built and submitted; and a NaN passes the near-plane
reject, because `NaN > -0.03f` is false.

Fixed in this build:

- **`HmBuildYPR`, local to `hand_mesh.cpp`** - the twelve lines needed, not the
  legacy subsystem. Y-up controller convention (yaw about Y, pitch about X, roll
  about Z), so the ini keys mean what they say. Verified numerically: exact
  identity at zero trim, equal to `Ry*Rx*Rz` to 2.2e-16 over 64 combinations.
  Inert in the current configuration - every shipped trim value is 0.0.
- **`submitted` and `ON-SCREEN` are different numbers now.** The beat also counts
  `nonFinite`, `nearPlane` and `degenerate` (zero projected area draws no pixel
  and has a perfectly ordinary bounding box) and prints the NDC bounding box, so
  "invisible" resolves to an axis and a magnitude.
- **Real depth.** `pos.z = 0.5f * (-z)` over `w = -z` divides to exactly 0.5 at
  every distance, so the depth buffer could order nothing. Standard mapping now.
- **The calibration triangle** (`[VRHands] CalibTriangle`, `vrhands calib on`),
  default OFF: one fixed-NDC triangle through the same shader, buffer, states
  and target. The FALLBACK if the hands are still invisible once the geometry is
  sound - and its outcomes are not fully exclusive, so the beat's counters, not
  it, are the first read.

**How far the bug class extends, measured:** of 23 stubs in `legacy_stubs.inc`,
exactly one had an out-parameter a live caller read unconditionally, and it is
this one. `RtdSnapshot` also has out-parameters but returns false and its caller
gates every read on that. The other 21 are void no-ops, as intended.

### Still open, deliberately not fixed in this build

- **The projection guard reads the wrong thing.** `HmDrawIntoEye` asks the
  METHOD whether it wants a projection layer, not whether the RUNTIME submitted
  one. It did not misfire on this run. Fixing it means adding an accessor to
  `openxr_runtime.cpp`, the file this project keeps closest to the BioShock
  copy, so it is not being done mid-diagnosis.
- Hiding the game's arms once ours are visible should use the **proven
  material-section hide on `Skm_Player`**, not `[VRHands] HideGameArms` - that
  flag's upload-size filters also target weapons and carry static-geometry
  heuristics, so it is not equivalent.

### The requirement changed: the REAL hands, because the powers animate them

Custom meshes cannot carry Arkane's power animations, so the working custom
renderer is now the FALLBACK and route (b) is the goal. Full plan and the nine
review corrections are in ENGINE_NOTES, "route (b) starts at the DRAW".

**Step 1 is built and installed: the DRAW census** (`hands/draw_census.cpp`,
`[Hands] DrawCensus`, ships ON). Upload SIZE cannot settle an arm/hand split -
two chunks can share a size, several draws can reuse one upload, and `HideSizes`
only contains what someone already saw. So `DrawIndexedPrimitive` is hooked
(vtable 82) and every palette-fed draw is identified by what the renderer held:
stream-0 VB and stride, IB, vertex declaration, vertex shader, index range,
primitive count. **Numpad 6** next, **4** back, **5** all visible - one row
hidden per press, the log names it. `dc report|status|hide <n>|show` on the seam.

Two rules it observes: no engine D3D reference survives the detour (each `Get*`
AddRefs and is released on the next line, pointer kept as an identity token
only), and the hotkey posts a request that the tick acts on - the detour itself
runs on the RENDER thread.

### Next steps

1. Walk the draw cycler (Numpad 6). Two rows sharing a bone count but differing
   in VB/IB or index range are separate geometry a size-based hide cannot tell
   apart - that is the question. If one row is arms-without-hands, route (b) is
   a draw skip and it is nearly done.
2. If arms and hands share one draw: collapse-to-wrist as a cheap prototype -
   with the wrist point derived in the palette's OUTPUT space, NOT read off a
   translation column (skinning matrices fold in the inverse bind pose), and a
   ZERO 3x3 linear part, not identity.
3. If that seams: the index-buffer filter - a replacement index list of hand and
   cuff triangles only, original vertices, weights and animated palette kept.
   Survives arms and hands sharing both material and chunk.
4. Controller wrist placement is a SEPARATE gate; `[Hands] Enabled=0` today.
5. Read the hands beat line. `ON-SCREEN=0` with `submitted>0` is a geometry fault
   and the NDC box names the axis; `nonFinite>0` means the transform is still
   producing NaN; healthy counters with a blank screen means arm the calibration
   triangle.
2. Then hide `Skm_Player` by material section for the floating-hands verdict.
3. Floating weapons as a shipping lever (hide `Skm_Player`, default OFF, live
   A/B); check the shadow and Blink's aim.

## PREVIOUS (2026-09-06, session 19): VR-31 - our own hands are WIRED (unverified)

Branch `claude/vr-31-arm-hiding-floating-hands`, based on
`claude/vr-30-decouple-arm-hand-movement` (PR #18 still open, so this PR takes
`Ref VR-31`, not `Fixes VR-31`). Installed build carries the hand wiring.

### The cycler is installed and has NEVER been run

`alpha-305-g23ef0ae5` (the cycler commit) is the installed proxy - verified by
reading the build tag out of the installed `d3d9.dll`. The newest log on disk is
`alpha-304-ge1135d25`, the run that proved route (d) with the timed sweep. So
**the walk needs no build; it needs a headset session.**

The plan the cycler builds is deterministic and already on that log, so the
recording sheet can be written now. Four positions, Numpad 3 stepping forward:

| position | component | what the engine calls it |
|---|---|---|
| 1 | `Skm_Player` | the first-person body, section 0, LOD 0 |
| 2 | `Wpn_PlySword01` | the sword |
| 3 | `crossbow_01` | the crossbow |
| 4 | `bolt_01` | the loaded bolt |

Numpad 1 steps back, Numpad 2 restores everything. Each press logs its position
and component, so the walk only needs the tester to say what vanished at each
one.

**One thing the census already settles, so the run is not spent on it:**
`Skm_Player` is the ONLY component carrying first-person body geometry. Arms and
hands cannot be on separate positions - there is nowhere else for them to be.
The question the walk actually answers is position 1: do both arms AND both
hands go, and do the weapons keep drawing? (ENGINE_NOTES, "What the cycler can
still settle".)

### Our own hands: the caller was the missing piece, and it is back

`core/gfx/hand_mesh.cpp` has drawn nothing since 41.0. `HmRenderEye` and
`HmEnsurePipeline` lost their only call site when the side-by-side pipeline was
deleted (`cc2fa936`), so `[VRHands] Enabled=1` changed nothing on screen and
logged nothing about it. Rebuilt this session:

- a `HandDrawFn` seam beside `OverlayDrawFn` in `core/gfx/stereo.h`; the active
  method calls it between the game image and the F10 panel, passing **the eye
  tag of the pixels already in the target** (not the eye the next game draw
  renders - one line apart in `reentry::end_frame`);
- the pass's own D32 depth buffer, sized to the method's output and cleared per
  draw. The painter's sort is gone; without a bound DSV the depth state is inert
  and the back of a hand paints over its front;
- **the frustum corrected to the projection layer's CLAIM** (the engine's
  rendered hfov, derived the way the runtime derives its own half-angles)
  instead of the headset's raw half-angles - 108.1 deg against the headset's
  numbers on the last measured run, and a slide against the world that grows
  with the gap;
- a refusal on the mono screen that says which rung would work, because a silent
  skip and broken hands look identical in a log;
- a 3 s beat printing calls / draws / triangles that **names the reason for any
  zero**, and `handModelTris` / `handModelWhy` in `status.json`.

**Ships ON** (`[VRHands] Enabled=1`) for the test sessions - the same deliberate
exception as `[Device] ShadowFullCopy` and `[Hands] BoneVisHide`, so a run shows
the hands without anyone sending a seam word first; it reverts to OFF when the
verdict is recorded. `vrhands on|off|status` is the live A/B.

**`[VRHands] HideGameArms` ships OFF with it, on purpose.** It collapses the
game's own view-model rigs by upload size (the 30.77 vs-const path) - a SECOND
behavioural change in the same build as the first draw of ours, against the
author's one-change-per-build rule. It also makes run 1 diagnostic instead of
pass/fail: with the game's arms still drawn, they are the reference our hands
are judged against. If our hands land right, that flag is the whole remaining
step to floating hands.

**UNVERIFIED - nothing here has been seen in a headset or on the simulator.**

### Next steps

1. Walk the cycler (4 positions above) and record position 1's answer.
2. Run 1 of the hands: nothing to switch on. Read the beat line. Expect
   the first run to need trim: `[VRHands] Scale`, the per-hand `PosX/Y/Z` and
   `Yaw/Pitch/Roll`, all already in the ini, none of them exercised since 30.x.
   The overlay panel for them was deleted in 31.5 and would have to come back if
   trimming by ini proves too slow.
3. Floating weapons as a shipping lever: hide `Skm_Player`, default OFF, live
   A/B, and check what it does to the shadow and to Blink's aim.
4. Routes (b) and (c) are not needed for either. (a) is closed.

## PREVIOUS (2026-09-06, session 18): VR-31 - route (d) WORKS, floating weapons are in reach

Branch `claude/vr-31-arm-hiding-floating-hands`, based on
`claude/vr-30-decouple-arm-hand-movement` (PR #18 still open, so this PR takes
`Ref VR-31`, not `Fixes VR-31`). Installed build `alpha-305`.

### PROVEN, headset-judged: ShowMaterialSection hides first-person geometry

Four automatic steps, geometry disappeared and returned on each. **Route (d)
works.** Full record in `ENGINE_NOTES.md`, "SOLVED: route (d) WORKS".

The plan the engine's own `GetNumElements` produced:

| step | component | sections |
|---|---|---|
| 1 | `Skm_Player` (the first-person body) | 1 |
| 2 | `Wpn_PlySword01` | 1 |
| 3 | `crossbow_01` | 1 |
| 4 | `bolt_01` | 1 |

**Every component has exactly ONE material section.** Route (d) therefore hides
per COMPONENT, not per body part.

- **This gives floating weapons now**: hide `Skm_Player`, and the sword,
  crossbow and bolt keep drawing as separate components.
- **It cannot give floating hands**: arms and hands share one section, so
  hiding the arms hides the hands too. No finer material cut exists in the
  asset, and none can be made from outside it.

**The tester's per-step attribution is NOT yet established.** The recollection
of which step removed which limb was approximate. The table above is the PLAN,
not what was seen. The numpad cycler exists to settle it deliberately.

### The cycler

`[Hands] MatCycle=1`, ships ON. **Numpad 3** next, **Numpad 1** back,
**Numpad 2** everything visible. Each press logs the component, section and LOD
it hid. The hotkey only posts a request; every dispatch runs on the SCRIPT
lane in `MatCycleTick`, because ProcessEvent from the present thread is the
lane error this project has a rule about. The timed sweep (`[Hands] MatAuto`)
is back OFF now the route is proven, so the two cannot fight.

### Next: floating hands needs a different SOURCE for the hands

Not a finer cut of the game's mesh - that split does not exist. The mod already
has its own: `core/gfx/hand_mesh.cpp` draws hand geometry and the SkelControl
drive already places hands from the controllers. Hide `Skm_Player`, draw our
own hands. That is the BioShock shape reached from the opposite direction, and
it also answers the powers requirement - our own hands can be shown for powers
and hidden otherwise without touching the game's mesh at all. Untested.

### Next steps

1. Walk the cycler and write down which component is which limb, so the
   attribution is measured instead of remembered.
2. Floating weapons as a shipping lever: hide `Skm_Player`, default OFF, live
   A/B, and check what it does to the shadow and to Blink's aim.
3. Floating hands: `hand_mesh.cpp` + the SkelControl placement, with a
   visibility policy that shows hands for powers.
4. Routes (b) and (c) are not needed for either of the above. (a) is closed.

## PREVIOUS (2026-09-06, session 18): VR-31 - route (a) closed, route (c) found

Branch **`claude/vr-31-arm-hiding-floating-hands`**, now cut from
**`claude/vr-30-decouple-arm-hand-movement`** (rebased 2026-09-06, session 18).
Linear **VR-31** is In Progress.

### The base, and what that means for the PR

This branch **carries the VR-30 fix** and is safe to build and install from. It
is based on the VR-30 branch, not on `VR-Main`, because VR-30 is still open in
**PR #18**. So:

- the PR body's first line is `Ref VR-31`, not `Fixes VR-31` - only the PR that
  reaches `VR-Main` closes the ticket, and this one will not until #18 lands;
- **PR #18 merges first.** After it does, this branch rebases onto `VR-Main` and
  its PR retargets there.
- The tester's ini carries `[Camera] ArmBodyFacing=1` (the VR-30 fix, on),
  `ArmStripMeshRot=-1` and a leftover `BodyYawLock=-1` whose key no longer
  exists in this code and is ignored.

### VR-30 is DONE (2026-09-05, session 17)

Headset-judged: head yaw leaves the arms and weapon alone, the right stick turns
view and body together, both at once works. `FaceRotation` is virtual at vtable
slot 252 (`0x00AB0D40`); the mod replaces the **Yaw it is asked for** with
`view - our own injected head contribution` and lets the engine's own function
run. Measured 3365 replacements, 0 stale, `perf: tick` unchanged at 11.3-11.8 ms
against an 11.11 ms budget. Full record in `ENGINE_NOTES.md`, "SOLVED: VR-30".

### VR-31: the question, and where the research already is

**The deliverable is a verdict, not a feature**: floating hands via the
bone-density trick, or floating weapons with hands only for the powers.

**The 30.13 result is NOT missing.** Session 18 found it. The original author
recorded it in a code comment in the state chunk beside the experiment
(`src/mod/state/40_game_dishonored_hands_arms_hide.inc`, and in the original
single file at `git show 824e08d8:src/dllmain.cpp` line 11119), which is why
three passes over the corpus called it unrecorded:

    30.14: the +0x288 byte poke was wrong - that array turned out to be some
    per-bone animation control (arms froze to the view and rode the head).

So the write was made, and it had a visible effect that was not hiding. The
probable reason: `0xFF` is not a legal `BoneVisibilityStates` value, but it is
exactly `SkelControlIndex`'s "no control on this bone", so 30.13 most likely
attached the arm bones to SkelControl #2 rather than hiding them. That also
weakens the existence proof route (a) rested on - the game may not be hiding
`spine_3_jnt` at all, it may be pointing it at a controller. Full record in
`ENGINE_NOTES.md`, "The 30.13 bone-visibility result was recorded all along".

**What session 18 built instead of repeating it.** The offsets do not have to
be guessed: this build ships UE3 reflection (`FindPropOffset` reads
`UProperty::Offset` out of GObjects, as the crouch cylinder and the graft
already do). `arms vis on|off|status|chain`, ini `[Hands] BoneVisHide`, **ships
ON for the test sessions** (a deliberate exception to "every lever ships OFF",
the same call as `[Device] ShadowFullCopy`, so a run prints the census without
anyone sending a seam word; it reverts to OFF when the verdict is recorded):

- asks the engine where `BoneVisibilityStates`, `SkelControlIndex` and
  `RequiredBones` really live and prints all three against `0x288`, naming
  which array 30.12 actually found;
- writes `BVS_ExplicitlyHidden` into the arm chain at the offset the engine
  named (never hands or fingers), saving and restoring the exact bytes;
- refuses, with numbers, if the property does not exist, if the array is empty
  (`num=0` - the engine never allocated it, which closes route (a) with a
  reason and is the same fact 30.17 saw), or if its length is not the rig's
  bone count;
- runs a write-survival census every 2 s while on - `held` / `reverted` /
  `other` - so "the write did not survive" and "the write survived and the
  renderer ignores this array" stop looking identical in a headset.

Route (b), the c6 bone palette (**x36 = sword, x144 = arms, x204 = NPC**,
machinery in `src/legacy/rtd_drive.cpp`), is untouched and is where an all-held
census sends the question next.

### VERDICT (2026-09-06, runs 1-4): route (a) is CLOSED, and route (c) is new

Four runs, all read-only, all with the arms staying visible - which was the
correct result each time: the lever refused to write and said why.

**Route (a), per-bone visibility, is closed on three independent instruments:**

1. `BoneVisibilityStates` does not resolve on `SkeletalMeshComponent`
   (`RequiredBones` resolved at `+0x23c` in the same call, so the resolver was
   working and the `0` is a real absence);
2. no `UProperty` of that name exists on **any** class in GObjects, so it is
   not hiding on a subclass;
3. of the five per-bone byte arrays on the rig, none has its values confined
   to `0..2`, which is what a visibility array must look like.

**`+0x288` is `SkelControlIndex`, measured.** 30.13 pointed the arm bones at
SkelControl #2 rather than hiding them, which is exactly the symptom 30.14
recorded. The existence proof route (a) rested on is gone with it: the game is
not hiding `spine_3_jnt`, it is pointing that bone at a controller. And 30.17
is consistent with it - `HideBoneByName` allocating nothing is what a missing
array would produce - but the native's implementation was never inspected, so
that is the likeliest reading and not a demonstrated cause.

**Route (c) appeared in the same run.** `SkelControlIndex` on the arm chain:
**8 of 10 bones read `255` (free)**, and the 2 that are taken are exactly the
two collarbones, driven by controls **0** and **1**. One byte per bone, and the
mod already drives SkelControls. What those two controls are is not yet known -
the mod's own control enumeration was off this run (`[Hands] Enabled=0`), so
naming them is one cheap run with the hand drive on.

Full record, including the scan's element-size caveat, in `ENGINE_NOTES.md`,
"VERDICT (2026-09-06, run 4)".

### The three routes, as they now stand

| route | state | cost | unknown |
|---|---|---|---|
| (a) per-bone visibility | **CLOSED**, three instruments | - | none, it is finished |
| (b) c6 bone palette | **OPEN**, proven writable | the hottest D3D9 entry point | none material: x36 = sword, x144 = arms, x204 = NPC, machinery in `src/legacy/rtd_drive.cpp` |
| (c) `SkelControlIndex` | **OPEN**, new | one byte per bone | what a free bone can be pointed AT - the index selects from the AnimTree's control lists |

**The lever is back to default OFF** now the verdict is recorded. What is kept
is the diagnostic that closed the route: `arms vis status` re-runs the whole of
it on demand, `arms vis chain` prints the arm chain.

### Next steps

1. **The route decision is the user's**, and it is the real open question. (b)
   is the safe one - proven writes, mesh separation already measured, and the
   only doubt is frame cost on the D3D9 hot path. (c) is the cheap one and the
   more interesting one, but it needs the AnimTree control list understood
   before a single byte is written, and 30.13 is the standing warning about
   pointing bones at a control somebody else owns.
2. Either way, one cheap run with `[Hands] Enabled=1` names SkelControls 0 and
   1 (and settles whether #2 is `LookAtControl_Camera`, which would close the
   30.13 story completely).
3. PR is not open yet. It takes `Ref VR-31`, not `Fixes VR-31`, while the base
   is the VR-30 branch.

### Session log

### 2026-09-13 - session 38: a menu is not a load

**VR-93 was solved by timing the settle before touching it.** The pre-fix resume split into
a 499 ms UI rescan holding the game thread, 15 ms of re-adoption and 343 ms of
recalibration - so the relearn and the hold were two fixes, shipped and measured apart.

**The negative control changed the design.** The identity check was built on the theory
that a respawned actor gets a new FName; the save-load run measured the same FName on the
new pawn, and the load was caught only by its new address. The game's own load event is now
a hard drop.

**Three faults turned up that are not this one**: a pre-existing garbage-collector crash
(VR-96), the ring correction going blind while `c5` reads zero (VR-97), and a sharper
signature for the after-note flicker (VR-80) whose own counterprediction then falsified the
`c5` explanation within one run.
### 2026-09-09 - VR-66: the stale command line was the mod's own file

The render size had two homes and one writer. `dishonored_vr_launch.txt` carries
`-ResX/-ResY` and is read in `DllMain`, before the engine's entry point - that is
the route the engine obeys. `[Screen] RenderWidth/Height` in the mod ini is read
much later at `EnsureConfig` and drives the mode `VirtualMode` advertises. Both
were only ever written together by `ResRequest`; a text editor writes one.

So the two failed attempts asked with a text editor, moved the advertised mode
to 3200x3300, and left the engine being told 2750x2850 - the file's value from
five days earlier, still on disk with that timestamp. The engine asked for a
size that was no longer in the mode list and UE3 fell back to a real display
mode, which on this monitor is 2560x1440. **Neither ini ever contained 2560x1440;
the mod supplied it.**

`[Screen]` is now the authority. The ask is resolved from it on the engine's
first `GetCommandLine` call - the CRT startup glue at the exe's entry point, past
the loader lock, so the ini read `DllMain` is forbidden to do is safe and still
early enough. One resolved ask sets both the command line and the advertised
mode, so they cannot diverge; a disagreement logs both values and rewrites the
file; and the hooks now install with no launch file at all, which an ini-only
ask previously needed and never got.

**Two lessons, both already in this file in another form.**

*An ini key that exists beats every compiled default* (VR-65) has a sibling: a
file that exists beats the ini you edited. Anywhere one setting has two
persistent homes, the one nobody thinks to edit is the one that wins.

*Every refused guard says why, with the values.* The `CreateDevice` mismatch
warning named the game's own ini - the route measured inert on this build - and
did not name the command line, which is the route that decides. It now prints
what the engine was handed, how many import slots were patched, and whether the
size the engine wanted is one of the adapter's real modes, which is the fallback
signature.

**Not verified.** No run at a raised size has happened. The mechanism comes from
the logs and the launch file's timestamp.

### 2026-09-07 - VR-33: the hands reach the controllers

**Verified in the headset:** hands track the controllers, are correctly scaled,
occlude against world geometry, and hold position through head turns. PR #21.

**The cleanup is verified too**, after a scare worth recording. The trim did not
compile: `g_mpEyeHunt` lost its declaration and the capture packet still
referenced two retired modes - but MSBuild was linking STALE OBJECT FILES, so
three builds reported success while the DLL kept an older build id. Comparing
the id embedded in the installed DLL against `git describe` is what caught it;
a "clean build" had been meaningless. Fixed in `fe531095`, rebuilt with a
forced recompile, and confirmed in the headset with the ids matching.

**What the session established.** The bone palette's output is the component's
LOCAL space; `LocalToWorld` (c231) maps it to a camera-relative world frame and
`ViewProjectionMatrix` (c0) to clip. Register indices differ per shader and are
parsed from each shader's CTAB - three shaders draw this mesh and one defines c4
as an immediate that disagrees with the device. The shader does not normalise
skin weights. The view model is depth-crushed to MaxZ 0.001. The eye separates
Present-to-Present in LocalToWorld's translation by 6.76 uu against a predicted
IPD of 6.31.

**What was falsified**, each with the measurement that killed it: a truncated
GObjects census reporting its array capacity as a population; a relative drive
whose cancellation argument omitted the animated hand underneath it; a
head-space neutral; a yaw residual, killed by phi holding constant at 144 deg
across a 145 deg head swing; a calibrated origin that became a 1.4 m lever on
the head; and three eye classifiers - a game-thread flag that reads false on the
render thread, a learned midpoint that was not head-invariant, and an ordinal
that sampled per HAND so its pair was one draw compared with itself.

**Instrument lesson, and the reason the above cost so much.** Three instruments
produced confident readings later withdrawn, and once an absence was reported
that had never been established. All four survived because they were checked
against expectation rather than against their own ability to fail. The rule
adopted: an instrument must declare and log the unit it sampled, and "nothing
was sampled" must never be able to read as "no difference was found".

**Seen and not chased:** each shader draws the mesh three times per frame, and
only one pass is depth-crushed. The purpose of the other two is unknown.

**2026-09-06, session 19 (part 3)**: floating hands WORK, using the game's own
hands. Built the draw census, then the mesh lock (buffer-pair identity, both
draw entry points), then live triangle-range slicing - the mesh is a triangle
list, so sub-ranges of the game's own index buffer can be drawn without reading
or replacing anything. Two headset screenshots confirm real hands with the arms
gone. The tester's cut is `0x3001D237`, restorable with `dc mask s19`; it is not
a default because the two arms sit in non-aligned regions and it was tuned on
the left. Also found and fixed: single-matrix draws were saturating the census
table, and a full table silently disabled suppression - which was the whole
explanation for the faint arm that survived earlier hides.

**2026-09-06, session 19 (part 2)**: the cycler walk came back and matches the
engine's plan exactly - arms+hands together, then sword, crossbow, bolt. VR-31's
route (d) question is answered. The hands showed nothing despite healthy
counters; review found `RtdBuildYPR` is a legacy-only symbol that resolves to an
empty stub in every shipped build, so the hand transform ran on an uninitialised
matrix. Replaced with a local `HmBuildYPR` (verified identity at zero and equal
to Ry*Rx*Rz to 2.2e-16), separated `submitted` from `ON-SCREEN` in the beat,
fixed the constant-0.5 depth, and added a calibration triangle as the fallback.
Swept all 23 legacy stubs: exactly one was unsafe. Corrected an error in part 1's
write-up - the stereo tagging claim was read off bring-up beats and gameplay is
properly tagged.

**2026-09-06, session 19**: no headset run. Established that the cycler build
(`alpha-305-g23ef0ae5`) is the INSTALLED proxy and has never been run - the
newest log on disk is `alpha-304`, the timed-sweep run - so the walk needs a
session, not a build, and wrote the 4-position recording sheet from the census
already on that log. Then found that `core/gfx/hand_mesh.cpp` has been dead code
since 41.0: `HmRenderEye` and `HmEnsurePipeline` lost their only call site when
the side-by-side pipeline was deleted in `cc2fa936`, so `[VRHands] Enabled=1`
changed nothing and said nothing. Rebuilt the caller as a `HandDrawFn` on the
stereo seam, gave the pass its own depth buffer, corrected its frustum to the
projection layer's CLAIM rather than the headset's half-angles, made the mono
screen refuse out loud, and added a beat that names the reason for a zero.
Builds clean, lint clean, exports OK, installed Release. **Nothing verified** -
no headset, no simulator run.

**2026-09-06, session 18**: opened. Branch cut, VR-31 moved to In Progress,
research located and extracted. Found that the 30.13 experiment's result was
recorded after all and that `+0x288` is very likely `SkelControlIndex`, not
`BoneVisibilityStates`; corrected two dead-end entries in `ENGINE_NOTES.md` and
added the record. Built the reflection-resolved route (a) lever and its
write-survival census, default off. Builds clean, lint clean, exports OK.
Rebased onto the VR-30 branch and installed Release. **Four tester runs**:
route (a) is closed on three instruments, `+0x288` is `SkelControlIndex` by
measurement, this build has no `BoneVisibilityStates` anywhere, and the arm
chain is 8-of-10 free in `SkelControlIndex` - which is route (c). Two of the
four runs were spent on instrument preconditions I got wrong (the scan needs a
live rig, and the candidate list does not exist unless the hand drive is on);
both are recorded in ENGINE_NOTES so the next instrument does not repeat them.
Lever back to default OFF, diagnostic kept behind `arms vis status`.

**2026-09-05, session 17**: VR-30 solved and closed. PR #18 open against
`VR-Main`, unmerged. Also filed **VR-52** (place the hands absolutely from the
controllers) with the anchor bug and the acceptance direction recorded, since
that is a separate problem from this one.

## PREVIOUS (2026-09-05, session 17): VR-30 IS SOLVED - FaceRotation is the seam

Branch **`claude/vr-30-decouple-arm-hand-movement`**, cut from `VR-Main`.

**Headset-judged fixed.** Head yaw leaves the arms and weapon alone, the right
stick turns view and body together, and both at once works. That is VR-30's full
acceptance, both halves.

### The fix, in one paragraph

`FaceRotation` is what faces the body to the view. It is **virtual at vtable slot
252** and resolves to **`0x00AB0D40`** on this build. The mod hooks it and
replaces the **Yaw it is asked for** with a separated body heading
(`view - our own injected head contribution`), then lets the engine's own
function run so its dependent bookkeeping still happens. Pitch, roll, delta time
and every other actor pass through untouched. `[Camera] ArmBodyFacing`, ships
**OFF**, `arms facing 1|off` is the live A/B. Measured: **3365 replacements, 0
stale**, 1014 calls on other pawns untouched.

Full derivation, the falsified attempts with their numbers, and the identity bug
are in `docs/dishonored/ENGINE_NOTES.md`, "SOLVED: VR-30".

### What was removed, and why

- **`[Camera] BodyYawLock` is gone.** It wrote the pawn's `Rotation.Yaw` every
  dispatch and was measured futile: **4 of 186 writes survived** to the next
  dispatch. Its target was correct; the engine simply re-derives the pawn's
  heading from the controller every tick. Users with the key in their ini can
  delete the line; it is ignored.
- **The per-dispatch `FaceRotation` name match is gone** from `PeHandler`. Its
  question is answered (0 dispatches across a full run - the route is
  native-to-native) and the answer is in ENGINE_NOTES.

### What is kept as instruments

- `armfollow/yaw:` - the four-way yaw census (head, controller, pawn, view with
  per-second deltas). It found the stale-controller bug and the write-survival
  number. Now on the 1-in-128 slow path.
- `armfollow/nfp:` - the one-shot native-facing probe that derived the vtable
  slot. Costs nothing after it fires.
- `[Camera] ArmStripMeshRot` - ships OFF, kept as the reproducible A/B for a
  documented dead end (304,244 writes, no effect).
- `tools/yawtest-host.ps1` - compiles the yaw bookkeeping out of `head_track.cpp`
  **verbatim** and runs its seven cases on the host. No game, no headset.

### Performance

`perf: tick` on the winning run reads 11.3-11.8 ms against an 11.11 ms budget at
90 Hz, unchanged from before the branch. The hook adds a handful of integer
compares to a function that runs about once per frame per pawn. The census's
clock read was moved to the slow path (c5ecea34's rule); the answered
per-dispatch name match was deleted.

### Next

VR-30 is done. The hands are still placed by the engine - **absolute controller
placement is a separate ticket**, and `ENGINE_NOTES` records that SkelControl
world-space translation is absolute (attempt 5's pin test) and that the world
block's anchor is wrong: `camera+0x80` is not a position (DISCARDED 120/120),
`camera+0x330` is.

## SOLVED (2026-09-05, VR-15): the black texture bug was a MIP fault, and it ships fixed

PRs #14 (the crouch fix) and #15 (the 90 Hz defaults) are **merged to `VR-Main`**. Branch
`vr-15-black-texture` carries the fix, headset-judged by the tester.

**The fault**: a surface was largely black far away and correct up close, the black
receding as the player walked in. Distance selects the MIP LEVEL, so the bad data was in
the small mips and level 0 was fine.

**The cause**: the managed-pool shadow pushed every write with `UpdateTexture`, which
**takes no level**, and writes to levels above 0 were not being carried. `shadow_unlocked()`
was not even given the level - the unlock hook had it in hand and dropped it - so no
instrument could have seen a per-level fault. The 2026-09-04 log had the corroboration
sitting in it unread: **`level>0=50189`** locks in one load with **`dirtyRects=0`**.

**The fix**: `[Device] ShadowFullCopy` pushes exactly the level the unlock wrote, with
`UpdateSurface`, which names its two surfaces and cannot be vague about which level it
copied. It **ships ON** - a deliberate exception to "every render lever ships OFF", the
same call as `[Stereo] HoldUntagged`, because OFF is a visible rendering bug.
`device shadowfullcopy off` restores the fault and is the A/B.

### The frame-rate cost, and what was done about it

The first cut made the frame rate less stable. Three causes, two of them **older than this
session's work**:

1. **The push ran on READONLY unlocks** - 12408 of them per load, each a whole-texture GPU
   copy for a lock that wrote nothing. Now skipped; the lock kind is recorded in the
   lookup the lock hook already had to do, so it costs nothing to know.
2. **A refused `UpdateSurface` was re-attempted on every unlock forever**, paying for the
   failure and the fallback both. A refusing texture is now remembered.
3. **The 60 s upload census walked all 32768 twin-map slots on the present thread** just to
   decide whether to print. Self-inflicted this session; the decision now reads counters.

`shadow_unlocked` also took its critical section twice per unlock and now takes it once.

**Unmeasured**: nobody has put a number on the improvement. `perf: tick` before and after
is the measurement, and the `device/upload` line's "work skipped" count says whether the
READONLY skip is firing at all (a 0 on a loaded level means it is not).

### Still open on this branch

`[Device] ShadowSurfaces` (ships OFF) covers a different hole: the game locking a SURFACE
taken off a texture, which the shadow's redirect cannot see by construction. **Its hooks
have still never executed** - no run has exercised them. The `device/upload` line says
whether that path is used at all.

Mechanism and the full reasoning are in `docs/dishonored/ENGINE_NOTES.md`, "SOLVED: the
black texture bug".

### What was NOT done, and why

**The simulator run did not happen.** Two launch attempts: the first died in a C++ runtime
error before the D3D9 device was created (the mod fell through to the system's Virtual
Desktop runtime rather than the simulator, `[VR] XrRuntimeJson` being empty), so none of
the new hooks executed and the run says nothing about them either way. The second attempt
was stopped. **So the new vtable patches - `GetSurfaceLevel` 18 on textures and cube
textures, `LockRect`/`UnlockRect` 13/14 on surfaces - have been compiled and installed but
never executed.** Treat the first run with this build as a bring-up test: if the game dies
at its first texture, those slots are the suspect and `device/census` will say whether the
surface hook installed at all.

## CONFIRMED ON A THIRD RUN (2026-09-04): 2750x2850 at 90 Hz is the default and it holds up

Third headset run on the shipped defaults: smooth, weapon aligned, no ghosting, and no frame
rate below 80 observed. The 90 Hz result reproduces. The defaults now
carry it (`[Screen] RenderWidth=2750 RenderHeight=2850` plus the 90 Hz note beside them).

**One open issue on that run, and it is NOT new**: the session flickered for roughly 25 seconds
at the start before locking. It normally lasts a few seconds; this run it ran long, which is
what made it measurable for the first time. Diagnosed below, **not fixed** -
the first thing to try is a lever that already exists and has never been judged.

### The startup flicker: one eye starving while the level streams

`stereo: beat` across the run tells the whole story:

| t (s) | out/s | L/s | R/s | none/s | draws/s | |
|---|---|---|---|---|---|---|
| 8.5 - 17.5 | 21 - 85 | 0 | 0 | 0 | - | menu/loading, mono by design |
| **20.5 - 38.5** | 87 - 91 | **12 - 19** | **52 - 73** | 10 - 17 | **51 - 72** | **starved: the flicker** |
| **44.5 onward** | 180 | **90** | **90** | **0** | **90** | **locked, stays locked** |
| 77.5+ | 155 - 234 | 0 | 0 | 0 | - | pause screen, mono by design |

`perf: tick` in the starved window reads **17.5 ms against the 11.11 ms budget**, split
`P1[-1] n=36` against `P2[+1] n=156` with `untagged 107`. `reentry: beat` shows pass 2 running
throughout (`2nd/s == draws/s`, all skip counters zero), so the second draw is not missing -
**the game is simply producing 51-72 ticks/s against 90 display slots/s.** Below the display
rate the pair schedule cannot land one pair per slot, the tag stream goes lopsided, and 1016
same-eye pushes accumulate (always `+1`, so LEFT is the eye that starves). One eye refreshing
at ~18 Hz beside one at ~73 Hz is the flicker, and it looks like alternate-eye rendering
because structurally that is what it is.

**Same root as the ghosting, at a different ratio.** Tick slightly over the period gives the
beat (doubled edges); tick far over gives eye starvation (flicker).

### The fix theory, in order, and NOTHING here is implemented

1. **`vrpace strict on` first.** It already shows the fresh eye to BOTH eyes when one is stale,
   which converts the starved window from alternating eyes into a briefly flat picture. It
   ships off, toggles live, and has never been judged. **Try this before any code is written.**
   It wants an A/B rather than a default flip, because it will also fire on the rare
   mid-gameplay stale eye and cost depth for that frame.
2. **If that is not enough**, the shape of a real fix is to extend the `HoldUntagged` idea from
   untagged presents to unbalanced pairs: hold the previous good pair rather than submit a
   lopsided one, bounded so a permanent hold cannot freeze the image.
3. **Measure before either**: why LEFT specifically. Hypothesis - the shared-capture deferred
   delivery (`SharedWait=0` hands over the PREVIOUS slot) repeats a tag when presents arrive
   irregularly. `capture sharedwait on` is the A/B that tests it. This is a hypothesis, not a
   measurement.

## NEXT SESSION (2026-09-05): make the startup phases hook instantly

**The goal**: a load should come up in stereo, aligned, immediately. Today it walks through
mono, then the arms hook and reposition, then stereo hooks, then the weapon and hands flicker
until they lock - **15-25 seconds of settling, every load.** The 2026-09-04 run measured 26 s.

### The measured startup timeline (from the s15c log, times from proxy load)

| t | what happens |
|---|---|
| 0.00 s | pad IAT hook; the engine command line is extended (`-ResX/-ResY`) |
| 0.6 s | config read: hands, hand render drive, crouch, crash handler |
| 0.86 s | `res` adapter-mode hooks installed |
| **3.11 s** | **device hooks** (Present/Reset/SetVSConstF/SetRenderTarget/BeginScene) + the creation census hooks |
| 4.86 s | `[game] state: NO_PAWN` |
| **5.08 s** | XR session live; `reentry: ARMED` (call site patches at the next script dispatch); **`blockhunt: walking 65821 objects`** |
| 17.1 s | `[game] state: MENU` |
| **18.4 s** | `[game] state: GAMEPLAY`; 14058 D3D creations logged at first GAMEPLAY |
| **20.5 s** | stereo tags start - but **starved** (L/s 12-19 against R/s 52-73) |
| **44.5 s** | **locked**: L/s = R/s = 90, nothing untagged, and it stays that way |

**So the settle is two separate problems and they should not be conflated:**

1. **0 -> 18.4 s is mostly the GAME loading**, not us. Our own hooks are all in by 5.1 s. The
   only clearly-ours cost in that stretch is `blockhunt` walking **65821 UObjects** at 5.08 s -
   worth timing before assuming it is free, and an obvious candidate for caching its results
   (the offsets it finds are build-constant) or deferring it off the critical path.
2. **18.4 -> 44.5 s is the eye starvation, and it is OURS to handle.** Diagnosed in the section
   above: while the level streams the tick runs 51-72/s against 90 display slots/s, so the pair
   schedule cannot land one pair per slot and one eye starves. **This is the 26 seconds the
   player actually sees**, and it is where the work is.

### Where to start, cheapest first

1. **`vrpace strict on`** - already exists, never judged, one command. It should turn the
   starved window from alternating eyes into a briefly flat picture. **Do this before writing
   any code.**
2. **Measure why LEFT starves specifically.** Every doubled push is `+1`. Hypothesis: the
   shared-capture deferred delivery (`SharedWait=0` hands over the PREVIOUS slot) repeats a tag
   when presents arrive irregularly. `capture sharedwait on` is the A/B that tests it. This is a
   hypothesis, not a measurement.
3. **Then consider holding unbalanced pairs**, extending the `HoldUntagged` idea: hold the last
   good pair rather than submit a lopsided one, bounded so it cannot freeze the image.
4. **Separately, time the startup hooks themselves.** There is no instrument that says how long
   each phase took - `blockhunt`, the census hooks, the reentry call-site patch, the first
   GAMEPLAY transition. Without that, "make startup instant" has no scoreboard. A phase-timing
   line is probably the first thing to build.

**Do not re-open**: the ghosting (solved, cadence, 90 Hz), the bbox readback (gated, prediction
falsified), motion blur (already off), a per-eye tag asymmetry (impossible - the pair shares one
locate). The 60 fps dips are the Wi-Fi encoder, not the frame path.

## SOLVED (2026-09-04): the GHOSTING was the cadence beat, and 90 Hz is the fix

**No ghosting reported at 2750x2850 on a 90 Hz headset.** Same build, same scene, same render
size at 120 Hz: ghosting still reported. One setting changed.

| | 120 Hz | 90 Hz |
|---|---|---|
| display period | 8.33 ms | 11.11 ms |
| `perf: tick` p50 (p90, max) | 9.1 ms (10.8, 12.9) | 11.3 ms (12.0, 15.1) |
| **display slots per frame** | **1.05 - 1.11** | **1.00 - 1.02** |
| EVEN / UNEVEN windows | 9 / 20 | **33 / 16** |
| MATCHED / UNDER-SUBMITTING | 11 / 26 | **38 / 22** |
| ghosting reported | yes | **no** |

At `off` slots of drift per frame, one frame in `1/off` is held for an extra display slot, and
consecutive frames shown for different durations IS the doubled edge. At 1.11 that is every 9th
frame; at 1.01 every 100th. **The fault was never the resolution and never the pose
attribution - it was the tick not dividing into the display period.**

**The defaults now carry it**: `[Screen] RenderWidth=2750 RenderHeight=2850`, with the 90 Hz
requirement written into the ini text beside it, because the pair is one setting and the refresh
half lives in Virtual Desktop where no ini can reach it.
`tests/golden/known-good-2750x2850-90hz.ini` is the byte copy of the machine that was judged.

### Session 14's falsification was WRONG, and the threshold was why

Session 14 measured 1.03-1.05 slots per frame, read `EVEN CADENCE`, and closed the cadence
hypothesis. The verdict was lying: its threshold was `|off| > 0.06`, so it called 1.05 - a beat
every twenty frames - clean. **The hypothesis was right and the instrument's threshold was
wrong.** Now 0.02, drawn at the measured edge (1.02 does not ghost, 1.05 does), and both
branches print the beat as a number so an "even" verdict shows the residual it forgives.

### Why it drops to 60 at 90 Hz when it never dropped below 90 at 120 Hz

Not a contradiction, and the hitch RATE did not change - normalised by run length it is
**27.6 gaps/min at 120 Hz and 28.4 at 90 Hz. Identical.**

- At **120 Hz** the tick never fit the 8.33 ms slot, so the app never tried to hit one. It
  free-ran and the compositor smeared over the mismatch. No cliff to fall off when you are
  already past the edge: the rate reads a smooth 100-120 and the ghosting is constant.
  **Smooth, and always wrong.**
- At **90 Hz** the tick sits right at the 11.11 ms period. Most frames make their slot - which
  is what removed the ghosting - but one that misses waits a whole period, so an 11.3 ms
  overrun displays for 22.2 ms (45 fps instantaneous) and a run of them averages toward 60.
  **Correct, with a cliff directly underneath.**

The stalls were always there; they are just visible now, standing out against a locked cadence
instead of disappearing into a permanently smeared one.

### What is actually causing the remaining drops, and it is not ours

**54 of the 71 gaps sat in `present-tail (xrEndFrame)`, blocking up to 101 ms.** On a Wi-Fi
streaming runtime a 101 ms block inside the submit call is the encoder or the link. Next steps,
cheapest first, all on the Virtual Desktop side: raise the bitrate or change codec, check the
link speed and channel, try a wired/dedicated AP. Only after that is ruled out is it worth
looking at our frame path again.

The other lever, if you want margin instead: buy ~1 ms of tick. At ~0.63 ms/MP a step to about
**2600x2700** (7.02 MP) predicts ~10.3 ms against the 11.11 ms period - real headroom under the
cliff, at a small sharpness cost. Untested.

### Falsified, honestly: the content-bbox gate was not the hitch cause

Session 15 predicted that gating the 3-second full-frame readback would cut the `perf: frame
gap` count by roughly the number of 3-second windows. **It did not.** Samples fell from one per
3 s to 2-3 per run; the gap rate was unchanged. The counter-evidence recorded next to the
prediction - the gaps sat in `xrEndFrame`, not the capture phase - was the correct read. The
gate stays because it removed a real ~30 MB present-thread stall for free, but it did not fix
what it was predicted to fix.

### The pose-lane instrument: validated, still unarmed

`xr: poseaudit SEAM CHECK ok - the script lane's yaw reads 20.67 deg and this file's own
converter reads 20.67 deg for the same head pose (0.00 apart)` fired in **both** runs. The sign
calibration is proven correct against live data, so a delta it prints would be a real
disagreement. **Nobody armed it** (`vrpace poseaudit on`), so the pose-attribution question is
still open - but it is no longer the ghosting's leading suspect, because the ghosting is
explained. Keep it for the judder/`ahead` work.

## SUPERSEDED (2026-09-04): the pose-lane instrument was built for a fault the cadence explained

Everything below is **built, linted, installed and unverified at runtime** - nothing has been
launched. What the next headset session does, in order:

1. **Set Virtual Desktop to 90 Hz.** This is not optional decoration, it is the arithmetic.
   Fitting the three measured ticks against megapixels (4.56 MP -> 8.75 ms, 6.71 -> 9.55,
   15.73 -> 15.7) gives **~0.64 ms per megapixel on a ~5.6 ms fixed floor**. The floor is the
   game's own CPU tick plus two presents and resolution does not touch it, so at 120 Hz the
   entire 8.33 ms budget leaves 2.7 ms of GPU for two full-frame scene draws - unreachable at
   any VR-useful size. **90 Hz (11.1 ms) is the honest target.** 2750x2850 is 7.84 MP and
   predicts a **~10.6 ms tick**; if it lands far off that, the fit is wrong and say so.
2. **Launch.** 2750x2850 is already armed in all four places (`tools\arm-res.ps1 -Status`
   shows them). The log must read, in order: `res: launch: command line extended ...
   -ResX=2750 -ResY=2850`, `res: handed the game our 2750x2850@<hz> mode`, `CreateDevice - the
   game asked for 2750x2850`, `capture: 2750x2850`, `res: HONOURED`, `xr: swapchain pair
   2750x2850`.
3. **Reach gameplay under `stereo reentry`, then `vrpace poseaudit on`, and turn the head.**

### The pose-lane instrument - what it answers and how to read it

The mod samples the head twice: the SCRIPT lane drives the game camera (the pose the pixels
are DRAWN with), and the PRESENT lane tags the projection layer (the pose the compositor
reprojects FROM). If they disagree the warp is wrong by the difference every frame, worst when
turning fastest and worse when a frame is slow - which is the reported percept exactly. Nobody
had ever measured it. Now:

```
xr: poseaudit SEAM CHECK ok - ...                      <- must appear FIRST
xr: poseaudit L tag .. R tag .. vs SCRIPT-lane .. -> delta L +x.xx R +x.xx deg
    | rendered sample is N locate(s) back, tag is lag=1 -> GENERATION GAP +N
    | one generation costs X.XX deg at this head speed | sample age .. ms ..
```

- **SEAM CHECK first.** The two lanes read yaw out of the same matrix with opposite sign
  conventions (`atan2(m02,m22)` vs `atan2(-m02,m22)`), so a naive comparison would read twice
  the yaw and look like a catastrophic fault that is purely convention. The seam negates once
  and then proves it against live data. **If that line says FAILED, every delta after it is
  meaningless - stop and report it.**
- **`GENERATION GAP 0` with a delta near 0.00 kills the hypothesis** and that is a real result,
  written down here in advance so it cannot be explained away later.
- **A steady nonzero gap names the fault in whole generations**, and `vrpace lag 0|1|2` is the
  live A/B: one of the three must null it.

**The written prediction: the gap is +1 and `vrpace lag 2` nulls it.** The tagging code assumes
"locate N feeds the tick that presents at N+1", one generation, which is why `lag` ships at 1.
But the game's own `DishonoredEngine.ini` carries **`OneFrameThreadLag=True`** - UE3's render
thread runs a frame behind the game thread, so the pixels in present N were drawn from locate
**N-2**. If `lag 2` fixes the ghosting, `OneFrameThreadLag=False` is the independent second
test: it removes the skew at the source instead of compensating for it, at a throughput cost.

### Also shipped: the 3-second stall nobody had looked at

`capture.cpp`'s content-bbox instrument (the `FULL`/`CROPPED` line) needs CPU pixels, and in
the shipping `shared` mode - whose whole purpose is that nothing goes to the CPU - each sample
is a full `GetRenderTargetData` + `LockRect` + row copy of the entire frame **on the present
thread**. That is the same round trip that costs 17-21 ms/present in `sync` mode, it ran every
3 seconds, and there was no lever. `[Capture] BboxMs` now defaults to 30000 with
`capture bbox off|<ms>` live; a size change still resamples immediately.

**Falsifiable prediction:** if this is behind the hitches, the `perf: frame gap` count should
fall by roughly the number of 3-second windows in the run (62-82 gaps over the last two runs is
close to one per window). **Counter-evidence already on record:** those gaps mostly reported
`sat in: present-tail (xrEndFrame)`, not the capture phase. If the count does not move, this
removed a real cost and was not the hitch cause - say that.

### Two more suspects died without a run

- **Motion blur.** Already off: `MotionBlur=False` and `MotionBlurPause=False`, with Arkane's
  own comment in the file - "Motion blur is unwanted". Not the ghosting.
- **A per-eye tag asymmetry.** Under `reentry` the LEFT present holds the XR frame open and the
  RIGHT completes it; `on_present_begin` returns at the top while a pair is open, so there is
  no second waitFrame and no re-locate between the eyes. **Both eyes of a pair share one locate
  generation.** The instrument prints per eye anyway so the invariant is checked, but do not go
  hunting this.

### Still not done from the tester's list

FXAA (`iType_AntiAlias` 1 -> 2) and the vsync A/B are **not** wired - the real keys and the
AppCompat requirement are recorded below and unchanged. Killcam is still not found in any ini.

## OPEN (2026-09-04): the GHOSTING - the cadence was NOT the cause, and that is now measured

**The report** (tester, on `alpha-264-ge2b84a80` at 2064x2208): "still ghosting flicker when turning
head and sometimes it hits harder than others".

### The resolution lever WORKS. Proven, because the tester doubted it and was right to

Every number moved, and the log names the mechanism at each step (`res: handed the game our
2064x2208@240 mode (slot 112)`, `CreateDevice - the game asked for 2064x2208`, `capture: 2064x2208`):

| | 2496x2688 | 2064x2208 |
|---|---|---|
| `perf: tick` | 9.2-9.9 ms | **8.6-8.9 ms** |
| submits/s (of 120 slots) | 88-115 | **114-117** |
| supply verdict | UNDER-SUBMITTING 0.73-0.96x | **MATCHED 0.95-0.97x** |
| slots per frame | 1.13 | **1.03-1.05** |
| interval sd | 1.3-3.8 ms | **0.75-1.36 ms** |
| cadence verdict | UNEVEN | **EVEN CADENCE** |
| capture lock | 0.5 ms | 0.0-0.1 ms |

### And the ghosting SURVIVED it. The cadence hypothesis is falsified

This is the point of having written the prediction down. The cadence is now locked - EVEN CADENCE,
1.03-1.05 slots per frame, sd under 1.4 ms, submits MATCHED to the slot rate - and **the doubled
edges are still there**. So the interference beat was real, was measured, was fixed, and **was not
what the tester is seeing.** Do not spend another session on pacing for this symptom.

What that leaves, cheapest first, all live commands with no relaunch:

1. **Virtual Desktop's Synchronous Spacewarp.** It manufactures intermediate frames by
   reprojecting, which is a literal ghost-frame generator, and it engages and disengages on its own
   - which is what "sometimes it hits harder than others" sounds like. Turn it OFF in the VD
   streamer settings and repeat the same head turn. **This is the first test and it is not ours.**
2. **`vrpace lag 0|1|2`** - which locate generation the layer's views are tagged with (ships at 1,
   "one back"). Infinite recorded the identical open suspect for the identical percept: "camera
   movement feels 'a bit jumpy' beyond the hitches - candidates: ... **the one-pair-stale
   content-pose attribution**". If the tag is a generation off the pose the frame was actually
   rendered from, the compositor reprojects by the wrong amount and the error changes every frame -
   doubled edges under rotation, worst when turning fastest.
3. **`vrpace ahead 0|1|2`** - the locate TIME (ships at 0). Already on the carried list as an
   unjudged judder item. The pair phase reads a steady **-43 ms** (we close ~5 display periods
   before the slot we asked for; on a Wi-Fi streaming runtime that is VDXR's pipeline depth, not a
   fault), so the reprojection is doing 40+ ms of extrapolation on every frame and the pose it
   extrapolates FROM has to be right.

### Not faults, checked so nobody re-checks them

- **The `CROPPED` capture bbox is a menu/loading artifact, not the resolution change.** Both runs
  show `100% x 52-53% (CROPPED)` early and then `100% x 100% (FULL)` once gameplay starts. Same
  shape at both sizes.
- **Not frame duplication or eye desync**: `mono/s=0 none/s=0-1`, `L/s == R/s == out/s / 2`,
  `ageL=1 ageR=0`, `aborts=0 staleEye 0` throughout gameplay.
- **Not Infinite's 30-second GC grid**: its spike class was
  `TimeBetweenPurgingPendingKillObjects=30`, killed A-B-A with 300. Our gaps have no 30 s
  periodicity - 0-13 s in bursts. Infinite's other signature (the streaming / level-visibility walk,
  triggered by view change and traversal) is the closer match and matches the head-turn trigger.
- **The hitches did not improve with resolution**: 62 gaps at 2496x2688, 82 at 2064x2208. They are a
  separate, later item from the ghosting.

### The 4K run: the lever is proven twice over, and SSW is OUT

**3840x4096 ran.** `perf: tick` **15.6-15.8 ms, 64 ticks/s** against 8.6-8.9 ms at 2064x2208 - the
lever moves the cost by 1.8x, exactly as pixels predict, so it is not inert and never was. Tester:
"4k does look way better". The capture lock also scales (0.0-0.1 ms -> 1.4-1.7 ms), which is the
readback and is ours.

**Virtual Desktop's SSW was already OFF** (tester confirmed), so suspect 1 of 3 is dead without a
run. The ghosting persists at every size tried, and the tester adds: **"it seems to get worse when
frame drops happen"**, and their own read is "some kind of eye submit desync, or the world geometry
isn't tracking the head tracking correctly".

**That read is now the leading hypothesis and it is testable.** The mod drives the game camera from
the head pose on the SCRIPT lane, and the compositor reprojects the submitted image using the pose
in the projection layer's views, located on the PRESENT lane. Those are two different samples of the
same head. If they disagree, the compositor's warp is wrong by the difference, and the error changes
every frame - which is doubled edges under rotation, and it grows when a frame is slow, which is
exactly "worse when frame drops happen". Nobody has ever measured that disagreement.

**The matched pair to measure it already exists in the code**: `head_track.cpp` records
`g_viewYawRad` (the yaw actually written to the engine) next to `g_injHmdYawSnap` (the HMD yaw it was
computed from) - its own comment says "Matched pair: this rotation was computed from THIS
g_hmdYaw". The instrument to build is: at submit, compare `g_injHmdYawSnap` for the frame that was
RENDERED against the yaw of the pose the layer is tagged with, and print the delta in degrees. It
can print the unwelcome answer - 0.0 deg means the two lanes agree and this hypothesis dies too.
`vrpace lag 0|1|2` and `vrpace ahead 0|1|2` are the live A/Bs that move it, and neither has been
judged.

### The tester's settings requests, with the REAL keys found (not guessed)

Asked for: killcam off, antialiasing FXAA, models high, maybe vsync on ("I just tried it and it was
maybe more smooth, but I'm not sure"). What the game's own config actually carries, in
`Documents\My Games\Dishonored\DishonoredGame\Config\`:

| ask | key | now | note |
|---|---|---|---|
| FXAA | `DishonoredEngine.ini [SystemSettings] iType_AntiAlias` | **1** (MLAA) | **2 = FXAA**, and the enum is documented in the file itself by the original devs (`EPpAa_None=0, EPpAa_Mlaa=1, EPpAa_Fxaa=2`) - measured, not guessed |
| vsync | same section, `UseVsync` | **False** | `True` is the ask; the tester is unsure it helped, so this one wants an A/B, not a default |
| models high | `SkeletalMeshLODBias`, `TextureForcedLODBias`, `DetailMode`, `Skeletal/StaticLODDistanceFactorMultiplier` | 0, 0, 2, 1, 1 | **already at the high end** (bias 0 = no reduction, DetailMode 2 = high). Nothing to change without inventing a value - do NOT write a guessed multiplier |
| killcam off | **NOT FOUND** | - | no killcam-shaped key in any of the 23 game inis (searched `Kill/Death/Assass/Slow/Cam` boolean keys). It may live in the save profile rather than an ini. Needs finding before it can be defaulted |

The AppCompat trap applies to all of these the same way it applies to the resolution: the bucket
AppCompat picks at startup overwrites `[SystemSettings]`, so anything written there must be written
to all four `AppCompatBucket*` sections too - `iType_AntiAlias` and `UseVsync` both already appear
in `DishonoredCompat.ini` with per-bucket values.

### Armed now: 3840x4096, purely to prove the lever to the eye

The tester asked to see the resolution do something visible, which is the right instinct and the
repo's own rule (confirm a lever moved something before believing its verdict). 3840x4096 keeps the
near-square eye aspect (0.9375 against 2064x2208's 0.9348) and is **3.4x the pixels** of the current
size, so if the lever were inert the tick would not move. Expect it to be slow - that IS the result.
Revert with `res 2064x2208f` (or `res 2496x2688f`) and relaunch.

## RESOLVED as a reading (2026-09-04): the submit stalls were NOT the game outrunning the headset

**The prediction written before the run held.** Submits landed at 88-115/s against 120 slots/s -
UNDER-SUBMITTING in 23 of 24 windows - and `endFrame mean` measured **0.08-1.99 ms**. xrEndFrame is
not blocking and is not throttling anything; the display slots are going unfilled. The reading that
opened this item ("the game produces frames faster than the headset can show them, and the runtime
absorbs the mismatch by blocking at submit") is **dead**, and it would still be alive if the period
had not been printed.

**What remains real:** 62 `perf: frame gap` lines, most still in `present-tail (xrEndFrame)` with
36-96 ms inside the submit call. Those are genuine, rare hitches, not pacing - and on a Wi-Fi
streaming runtime an 85 ms block in xrEndFrame is the encoder or the link, not our frame path. They
are worth a separate look AFTER the ghosting, because the ghosting is continuous and these are not.

**The tick, for whoever picks up "make it 120":** 9.2-9.9 ms, split almost evenly between the two
scene renders reentry needs - per pass roughly `present 1.8 ms + out/R 2.0 ms`, with our own capture
lock at 0.2-0.5 ms and endFrame at 0.0-0.6 ms. The mod is not the cost. Reaching 8.33 ms means
taking ~12 % off the game's own render, and the obvious dial is the 2496x2688 per-eye size.

## OPEN (2026-09-04): the submit stalls - 48 hitches of 44-156 ms, two thirds in xrEndFrame

**The report** (tester, 2026-09-04): "super laggy", and separately "this game is old enough that
it should run at hardlocked 120 fps, especially on a 4070 Ti Super". The headset is set to
**120 Hz**, so the budget is **8.33 ms** per frame.

**Throughput is NOT the problem, and that is the whole point of this item.** Measured on
`alpha-267-g4b9a0f3c-dirty`, Quest 3 via VDXR, 2496x2688 per eye, method reentry:

- **mean present interval 4.3-6.6 ms** (the heartbeat read GAME=138-233 fps across the run).
  That is comfortably inside the 8.33 ms budget, with 25-50 % headroom.
- **48 `perf: frame gap` lines**, gaps of **44-156 ms**. The worst is **36.3x the 4.3 ms mean** -
  at 120 Hz a 156 ms stall is 19 dropped frames in a row.

**Where the stalls sit**, from the `sat in:` field of those same 48 lines:

| phase | count |
|---|---|
| **`present-tail (xrEndFrame)`** | **31** |
| `out/idle (waiting for the game thread)` | 9 |
| `out/R (executing the frame)` | 3 |
| `game_tick` | 3 |
| the game's `Present` | 1 |
| `present-head (wait)` | 1 |

`flags:` on those lines read `reset=0 load=0 paceTimeouts=+0` almost throughout (one `+1`, one
`pairOpen=1`), so the pace lane is not timing out and the method is not re-arming. `vrpace ahead`
was at its shipped 0.

**Two thirds of the stalls are in the submit call.** So this is a pacing/submit problem, not a
rendering-cost one - which is the good news, because the fix is scheduling rather than cutting
quality.

### The gap that had to close first: the display period is now printed (session 13)

**It was not in the log.** `dvr::vr::display_period_ns()` existed in the runtime layer, was read by
the pace sync, and was PRINTED only inside the `PACE-BOUND` clause of the `perf: tick` line - so the
one run that most needed it (48 hitches, the wait at ~0, no `PACE-BOUND` line anywhere) is exactly
the run where it stayed invisible. "The game produces frames faster than the headset can show them,
and the runtime absorbs the mismatch by blocking at submit" was therefore an INFERENCE from a phase
NAME, and this project has spent whole sessions on inferences that read well and were wrong.

**What now prints** (built and lint-clean on `performance-fix`, **not yet run** - see below):

- **`stereo: rate`**, a new line on the 3 s stereo beat:
  `hmd=8.33 ms (120.0 Hz) slots/s=120.0 | presents/s=233 submits/s=116 (one xrEndFrame per pair) |
  endFrame mean=6.41 ms max=41.2 ms over 349 submits | <verdict>`.
  `submits/s` is a NEW counter: `xrEndFrame` calls from the present path, which under reentry is
  **one per PAIR** - it is the tick rate, not `out/s`. The endFrame mean and max are the submit's
  own cost, drained per window.
- **the verdict on that same line**, which is the whole point and can print the unwelcome answer:
  **OVER-SUBMITTING** (> 1.05x the slots) confirms the throttle reading and the lever becomes "stop
  producing frames nobody sees"; **MATCHED** (0.95-1.05x) means the endFrame MEAN is the pacing wait
  and is not a hitch, only its max is; **UNDER-SUBMITTING** (< 0.95x) says display slots are going
  unfilled, which falsifies the throttle reading outright and puts the cause upstream of the
  headset's cadence. A runtime that leaves `predictedDisplayPeriod` at 0 prints `hmd=UNKNOWN` and
  gets NO verdict - never read that as 0 Hz.
- **`perf: tick`** now opens with `[hmd 8.33 ms = 120.0 Hz, budget 8.33 ms/tick]` unconditionally,
  not only when pace-bound.
- **`perf: frame gap`** now ends its phase attribution with `= 19.0 display slots at 8.33 ms`. The
  "156 ms is 19 dropped frames" arithmetic in the table above was done by hand off the log; it is in
  the line now.
- **`status.json`**: `stereo.pair{displayPeriodMs, displayHz, endFrames, endFrameMeanMs,
  endFrameMaxMs}` and `perf{displayPeriodMs, displayHz}`. `game-cmd.ps1 "stereo status"` prints the
  same numbers live, without waiting for a beat.

**A prediction worth writing down before the run, because the arithmetic already argues against the
inference.** The report's own numbers are 4.3-6.6 ms mean PRESENT interval, and reentry submits one
frame per two presents - so submits/s should land at **76-116**, BELOW 120. If that holds, the line
reads UNDER-SUBMITTING and the "game outruns the headset" reading is dead: the headset would be
going hungry, not being over-fed, and the 31 present-tail stalls are genuine hitches inside
`xrEndFrame` rather than a throttle. The measurement is what settles it either way.

**Next step: a headset run on `performance-fix` with nothing else changed**, and the three lines
above out of the log. Nothing has been installed or launched for this change - the build is verified
only as compiling, linting clean and exporting the nine names.

### Not a fault, for the record

Stereo reads mono for roughly the first 6 seconds of a run (`2nd/s=0`), then latches to 103-108 and
holds - confirmed by the tester ("it is mono at the very beginning but it latches on and stays good
after a few seconds"). The `2nd/s=0` at the very END of a log is leaving gameplay, not a regression.

## FIXED (2026-09-03, headset, dev rig): all three flickers, in one chain

Branch `swapchain-one-picture-flicker`. Installed and judged as `alpha-253-g8441404f`. **The
tester's verdict after the third fix: every kind of flicker is gone.**

The three faults were nested - each fix exposed the next - and each was confirmed in the headset
and in the log before moving on.

### 1. The stale RIGHT eye (`1507bafc`)

`reentry.cpp`'s c5 invariant has two arms and they are **not equally trustworthy**. `inv=+1`
("pass 2 after pass 1") compares two draws with NO world tick between them: the step is exactly
`-ipd*scale` along the camera's right row by construction. `inv=-1` ("pass 1 after a still pass
2") is the **only arm that reasons across a world tick**, and holds solely while the player is
near still. A gently moving player - turning in place, decelerating, crouch-walk - parks the
tick's travel inside the `+-0.35*ipd` window (about `+-2.2 uu` at the measured 6.18) and the
fragile arm then names a genuine pass-2 present a pass 1. It did so **unconditionally, on a
streak of one**. One wrong `-1` writes the left swapchain twice and never writes the right.

The fragile arm now defers to the ring on a disagreement (a streak of three still earns the
override for either arm); neither arm invents a tag on an empty ring or over the `0` tag a
single gameplay draw pushes.

| | before | after |
|---|---|---|
| `STALE ? EYE` lines | 36 in 171 s | 1 in 238 s |
| one-picture pairs at sc | 14 of 39 (36 %) | 0-1 of 40 |
| `sc-target repeats` | 80 | 2 |
| `out/s` median | 117 | 212 |

`c5Held=36` counts the deferrals that did it, against `c5Agree=29210 c5Disagree=112`.

**Why no gate admitted to it**: `SceneDrawMaybeSecond` cannot skip pass 2 unlogged - `!doubleIt`
means pass 1 pushed no `-1` at all, the poison logs an `Error` at its one site and stands the
method down, and the forced skip IS the `forced=` field. Both tags were pushed every time; the
second `-1` was manufactured downstream. Every zero on the line was true.

### 2. The mono flick on the arms and weapon (`[Stereo] HoldUntagged`)

With the stale eye gone the tester saw a different flicker: "both arm models/weapons instead of
just the left handed crossbow", and it "felt slightly different". It was. `mono/s=1..4` in steady
gameplay, and a mono present is the same image in BOTH eyes, so its error scales with disparity -
near zero on distant geometry, **largest on the viewmodel at 30-50 cm**. Symmetric, hence both
arms, where the stale-eye fault was one-sided. 26 of 29 single-draw spells were the present-stall
guard; `c5Refused=72` was the rest.

`HoldUntagged=3` (ported from the parked branch) took `mono/s` 1-4 -> **0** and every other
flicker with it.

### 3. The black frame in both eyes (`8441404f`) - caused by the fix for 2

The hold immediately produced a new, subtler artifact: one black frame in both eyes, frequent.
**The whole layer assembly in `on_present_end` sits inside `if (backbuffer)`**, and
`backbuffer = frame`. A present that hands in no texture therefore reaches `xrEndFrame` with
`layerCount 0`, and a zero-layer frame gives the compositor nothing for that display slot -
black, both eyes, one frame. `HoldUntagged` made that path common: it holds a present by
returning false from the method, and `frame_hooks.cpp:181` passes the null to `on_present_end`
unconditionally.

The ported commit's own message claims "nothing is submitted; the compositor holds the previous
pair". **That is not true of this runtime**, and the commit was parked without a headset run that
would have caught it - it was taken as established behaviour instead of checked.

The guard now re-submits the layer the previous present used. Nothing is acquired or released on
such a present, so the swapchain images are untouched and the compositor genuinely re-shows the
previous pair; reprojecting it to the new display time is the runtime's job, and the
parked-session keepalive already re-submits that same snapshot. Measured over 126 s:
**`held=25 black=0`**, `STALE` 0, `sc-target repeats=0`, one-picture `sc=0`, `out/s` median 201.

### Levers and defaults

**`[Stereo] HoldUntagged=3` now SHIPS** (the user's call, 2026-09-03), joining the session-9
precedent of the headset-judged values being the defaults. It is a deliberate exception to the
default-OFF rule for render levers, made because the artifact it removes is a visible flicker on
the viewmodel and the black frame it used to expose is fixed at the runtime. `0` is the A/B and
the pre-41.1 behaviour; `stereo hold <n>` switches it live, `none/s` on the beat counts what it
held, and `zeroLayerHeld`/`zeroLayerBlack` in status.json say whether the guard covered them.

**Judged on ONE rig, for a few minutes.** A second headset that reports one-frame stalls or a
smeared weapon should try `stereo hold 1` and then `0`, and say which is better.

### The rig, and three traps that cost runs

Quest 3 via VDXR, 5120x1440@240 desktop, RTX 4070 Ti SUPER. `[Screen] RenderWidth/Height` in the
ini CANNOT affect the launch it is set on - DllMain reads only `dishonored_vr_launch.txt`, written
by the `res` seam word, so use `res 2496x2688` and never hand-edit the key. `bPauseOnLossOfFocus`
was TRUE in the game ini (now FALSE here): with it on, alt-tabbing to drive the command seam
pauses the thing being judged. VERIFICATION gotcha 17 bit 1 run in 3: state sticks at MENU in the
level, `2nd/s=0`, screen stays mono - open and close the pause menu.

**Check the build tag before reading any verdict out of a log.** The first "it's fixed!" here was
measured on a build that had been compiled but never installed - `build.ps1` had run, `install.ps1`
had not, so the game folder still held the previous DLL. The log banner names the build
(`build alpha-NNN-gHASH`) and `Get-FileHash` against `build\src\RelWithDebInfo\d3d9.dll` settles
it in one command. A `-dirty` tag means the tree had uncommitted changes at build time and the
log cannot be traced to a commit: rebuild from a clean tree before handing a log to anyone.

## Current state (2026-09-04, session 14: the throttle reading is dead, the cadence is the suspect)

**This branch (`performance-fix`) carries two instruments and one lever, all default OFF or
log-only.** No render path changed. Session 13 added the rate measurement, it was run, and it killed
the hypothesis the branch was opened for - the submit was never throttling. Session 14 added the
cadence measurement, which points at the ghosting instead, and `[Pace] SyncHz`, which ships at 0.

What shipped: the `stereo: rate` beat line (hmd period and Hz, slots/s, presents/s, submits/s, the
pair interval's mean and sd, the submit's own mean and max cost, and TWO verdicts - supply and
evenness - either of which can print the unwelcome answer); the display period unconditionally on
`perf: tick`; the gap converted to display slots on `perf: frame gap`; `[Pace] SyncHz` as the
persisted form of `vrpace sync <hz>`; and all of it in `status.json` and `stereo status`. The two
OPEN sections at the top carry the readings and what they mean.

**Built, lint-clean, exports OK, and INSTALLED as `alpha-260-g958ab57a`** (RelWithDebInfo,
hash-checked against `build\src\RelWithDebInfo\d3d9.dll`, clean tag - no `-dirty`). It replaces
`alpha-259-g865f1bcd`, so the game folder no longer carries the crouch fix: `crouch-fix` is still
only PR #14 and this branch is cut from `VR-Main`. **Not launched, not run** - in the simulator or a
headset. Treat every number it prints as unseen until a run produces one.

The two logs that were in the game folder are archived to `D:\dvr-data\logs\s13-pre-install-*.log`
before the run overwrites them (rotation is one deep and there were already two).

**The crouch height rise is FIXED and headset-judged** (previous session): the 38.16 deep-crouch
capsule write moved the pawn 20.00 uu on every crouch and the camera's rate-limited catch-up
integrated the remainder. `[PosTrack] DeepCrouch` now defaults to 0. That work is on `crouch-fix`
and is **open as PR #14, not merged**. Its derivation and the five falsified suspects are in
`docs/dishonored/ENGINE_NOTES.md` on that branch.

**The DLL in the game folder is `alpha-262-g61130855`** (session 14's build - the cadence instrument and
`[Pace] SyncHz`), replacing the session-13 `alpha-260-g958ab57a` that produced the run above. Both
are the tip of THIS branch, built
RelWithDebInfo, installed and hash-checked against `build\src\RelWithDebInfo\d3d9.dll`. It
replaced `alpha-259-g865f1bcd` (the tip of `crouch-fix`), so **the deployed build no longer carries
the crouch fix** - `performance-fix` is cut from `VR-Main` and PR #14 is still unmerged. Nothing
diagnostic is armed either way: `[Hands] CrouchAB`, `CrouchBurst` and `[PosTrack] DeepCrouch` are
all default OFF, and the tester's `dishonored_vr.ini` and the game's `DishonoredCamera.ini` were
restored from backups after the crouch investigation; no diagnostic keys remain in either.

**Two findings from that session are deliberately unbundled and unfixed**, because neither has been
judged in a headset:

1. `LocPropFind` and `CrouchPropFind` sit below `if (!g_handMesh) return` in `ApplyHandToMesh`, and
   `[Mode] GamepadOnly=1` (the shipped default) clears `g_handMesh` - so **they have never run on a
   shipped build**, and the 38.24 eye clamp, which needs `g_actorLocFound`, has never run either.
   Reviving it is a real behaviour change and needs a headset verdict of its own.
2. The config line reporting physical crouch as "armed" when `[Mode] GamepadOnly=1` has already
   vetoed it. Cost a session's hypothesis once already.

## FIXED (2026-09-04, headset, dev rig): the crouch height rise

**The report**: crouching then standing raises the player slightly, and spamming it rises far
enough to pass through the ceiling. Also, from the same run, "almost like noclip, I could float
around".

**The cause is ours**: the 38.16 deep-crouch write. Shrinking the crouched collision cylinder
(65 -> 45) under a grounded pawn moves the pawn's origin down by exactly the shrink, 20.00 uu,
because the engine keeps the feet planted - and sometimes leaves the pawn airborne. The camera
chases that with a rate-limited convergence that cannot finish before the next crouch, so the
remainder accumulates: ~20 uu of view per cycle, 2184 uu (22 m) in one run. Measured, filmed frame
by frame, and confirmed by an A-B-A A/B (+20.35 uu/cycle with the write on, -0.26 with it off).
Full derivation and the five falsified suspects in ENGINE_NOTES.

**The fix**: `[PosTrack] DeepCrouch` defaults to **0**. Judged in the headset by the tester: the
climb and the floating are both gone. The cost is that the player no longer fits under low
furniture; `DeepCrouch=1` restores the old behaviour and the bug with it. It cannot be made safe as
written without writing `Actor.Location`, which this mod deliberately never does.

## Previous state (2026-09-04, session 9: THE EYES ARE FIXED, root cause proven in the headset; the headset-judged values are the defaults)

**Merged to `native-stereo-rendering` (PR #7, 13 commits).** The per-eye render is correct on the
headset, at rate, with the tested configuration shipping as the defaults.

- **THE ROOT CAUSE, found and fixed and PROVEN**: the eye tags paired draws to presents by
  push/pop ORDER, and the order breaks wherever the game thread runs ahead of the render thread -
  a level load, a pause/resume, a re-arm - and spontaneously in gameplay every ~2 s on the
  tester's rig (131 skews in four minutes). Each break showed each eye the other's draw until the
  next one. That is the fault this project has chased since run 15 ("the eyes disagree", "90 % of
  the time, more at the beginning", "never correct after a load"). THE FIX: between a tick's two
  draws nothing moves the camera but the eye offset, so pass 2's camera sits exactly one IPD along
  the camera's right row from pass 1's; a present whose camera step reads that IS the second draw
  whatever the queue claims, and a disagreement streak realigns the queue. `[Stereo] C5Pair=1`.
  THE PROOF (headset, the user, 2026-09-04): unticking `c5 pairing` on the F10 EYES block and
  pausing brought the fault straight back (`swapped=24 of 25` pairs, the picture's parallax on the
  wrong side), ticking it back removed it (`swapped=0` for the rest of the run) - by eye and in the
  log, three times.
- **THE INSTRUMENT that found it ships**: the frame-identity trace (`core/gfx/frame_id`, `[Perf]
  FrameId=1 FrameIdEvery=8`, `frameid on|off|status|every N`, status.json `frameid{}`): one 64x64
  luma thumbnail per sampled present at the backbuffer as the capture found it, the shared slot,
  the eye texture and the swapchain image, read three presents later and never waited on, with the
  draw's camera position and right row on the same record. The `stereo: frameid` line prints, per
  left/right pair, the L-R difference per stage, the camera step and side check, the picture's own
  parallax shift and the first stage that reads as one picture. The F10 Display tab's **EYES
  block** shows the same numbers live and carries DUMP EYES, REARM 2, CAPTURE REINIT, PROJECTION
  OFF / AUTO, the c5-pairing tickbox and the trace tickbox.
- **PERFORMANCE: back at the headset's rate.** The trace read every present cost the tester's GPU
  1.5 ms per present (stage bb's `GetRenderTargetData` is a pipeline sync there) and the tick went
  13.9 -> 16.7 ms, 60/s under a 72 Hz headset; sampling one pair every 8 ticks removed it
  ("the fps is perfect now", 2026-09-04). The trace tickbox is off = no cost at all.
- **THE DEFAULTS ARE THE HEADSET-JUDGED VALUES** (a fresh ini now comes up where the testing left
  off): `[Stereo] Method=reentry Armed=1 C5Pair=1`, `[Camera] EyeField=0x330`, `[Neck] Mode=cancel`
  with the measured pivot (0.321 / 0.062 m), `[PosTrack] Scale=98 Lane=auto`, `[Tracking]
  HeightOffsetM=-0.090`, `[Screen] RenderWidth=2496 RenderHeight=2688 RenderFullscreen=1
  VirtualMode=1` (the Quest 3 through VDXR per-eye size, advertised so the game creates it - a
  fresh install used to render the game's own size and look soft), `[Device] Ex=1 Managed=shadow`,
  `[Capture] Mode=shared`, `[Perf] FrameId=1 FrameIdEvery=8`. Another headset: the F10 Display
  picker writes its size for the next launch; `res 0x0` asks for none.
- **Also fixed this session**: `dump eyes` writes a consecutive left/right pair and encodes it on a
  worker thread (the 640 ms present-thread stall used to re-arm the second draw, so every dump
  changed what it was taken to judge); the beat line's `presentTid` follows the presenting thread
  (it was latched at the first present, which at boot is the game thread's - a false lead in run
  17); pass-2 eye writes the camera seam refuses are counted (`p2write refused=`).

## Next steps (one paragraph per developer)

**The next developer session (the ghosting)**: build the POSE-LANE instrument and stop guessing.
Three suspects have now been killed by measurement - the submit throttle, the uneven cadence, and
Virtual Desktop's SSW - and the one the tester named has never been measured at all: the camera is
driven from a head sample on the SCRIPT lane while the compositor reprojects using a pose located on
the PRESENT lane, and nothing anywhere compares the two. `head_track.cpp` already keeps the matched
pair (`g_viewYawRad` beside the `g_injHmdYawSnap` it was computed from, its own comment says so);
publish that snap to the runtime and, at submit, print the delta in degrees against the yaw of the
pose the layer is tagged with. It must be able to print 0.0 and kill the hypothesis. Then
`vrpace lag 0|1|2` and `vrpace ahead 0|1|2` are the live A/Bs that move it, and neither has ever
been judged. The corroborating detail worth keeping in mind: the tester says the ghosting **grows
when frames drop**, which is what a lane-disagreement does and what a locked cadence does not.
Second job, small and separable: the game-settings profile (the real keys and what is already at
maximum are tabulated in the OPEN section - `iType_AntiAlias` 1 -> 2 is the only clear win, `UseVsync`
wants an A/B, model detail is already high, and no killcam key exists in any of the 23 game inis).
Anything written to `[SystemSettings]` must also go to all four `AppCompatBucket*`, or the bucket
AppCompat picks at startup overwrites it - the same trap the resolution picker already handles.

**The user (headset)**: installed as `alpha-264-ge2b84a80`, with **3840x4096 armed for the next
launch** (your call - you judged 4K much better looking). Know the trade: it measures 15.6-15.8 ms
per tick = 64 fps into a 120 Hz headset, against 8.6-8.9 ms at 2064x2208, so if the ghosting really
does track frame drops, 4K is the worst case for it and the sharpest picture at the same time. If
you want the middle, `res 2496x2688f` or `res 2064x2208f` in-game then relaunch. Nothing else is
waiting on you until the pose instrument exists - the three cheap A/Bs are spent. `crouch-fix`
(PR #14) is still ready for your merge decision, and note that the installed build does NOT carry it.
Still open from earlier sessions: (1) the PITCH PIVOT with `[Neck] Mode=cancel` against `off` and
`add`; (2) WORLD SCALE and eye height at `[PosTrack] Scale=98` / `HeightOffsetM=-0.090`.

## Blockers

- **The ini version rewrite wipes a tuned ini** (`config.cpp` carries three keys over): the
  session-8 and session-9 keys ship without a version bump. A key-preserving rewrite is a
  separate change.
- **WM_CLOSE leaves a stuck `Dishonored.exe`** (session 5): close a healthy game with
  `Stop-Process`; a menu quit is clean on the Quest (run 15).
- **The walk-in on the simulator is by hand**: Return x4 with 28 s waits reached the level on four
  of six launches this session; otherwise the mod's menu flag sticks (VERIFICATION gotcha 17) and
  an Escape pair clears it. Look at an `xrsim-shot` before trusting a state line.

## Session log

### 2026-09-14/15 - session 40: the HUD redo (VR-117)

**The HUD leaves the eyes.** The abandoned PR #12 redirect was rebuilt on VR-Main as three
core modules (draw class, capture with N sinks, layout) and one runtime block, with the
38.92 wrist HUD back as a hand anchor and every placement value in `[Hud]` and on the F10
HUD tab. In-game screens ride the window with the world in stereo behind them, decided
once per blocked interval by a pure ride policy over the reflected UI owner. Simulator
first: three sequences and 137 host checks, three faults found and fixed before the
headset saw it (a health blink cancelling the open-gap stand-in, the HUD quads skipped on
held presents so a riding menu blinked, a focus loss latching a thread refusal).

**The region probe falsified its own hypothesis.** The HUD draws are
DrawIndexedPrimitiveUP with SHORT2 positions, read correctly at 0.5 us per draw, but the
c0/c1 constant rows are not the 2x4 transform on this GFx build, so per-element routing is
an instrument, not a feature (VR-118).

**The first headset run judged the anchors good and reported two faults, both in the log.**
The HUD flickered between the window and the frame at about 10 Hz: the redirect armed on
the per-present eye tag and re-entry leaves 6 to 21 presents a second untagged by design,
so each untagged present put the next present's HUD into the frame (`presents=441
armed=400`). The gate now follows the projection mode (`presents=467 armed=467` on the
sim). The weapon scroll and the grip-hold loadout dropped the world flat for a second:
both open the power wheel, which was excluded from riding and parked the redirect through
the wheel-held flag. The wheel rides now (`WindowWheel=1`, `wheel-ride.xrs`). The setup on
this PC was reset to the repo's tested profile first (mod ini byte copy, game inis
`-VRBaseline` and `-Console`, 2750x2850 armed).

### 2026-09-13 - session 39: a late tag, and a note that waited on the load rules

**VR-80 was solved by measuring one event end to end.** Three instrumented runs had built a
convincing aggregate story (a drain that over-consumes and re-triggers itself). A host model
compiling the shipped pairing could not reproduce it, and the first per-present ledger with
draw ids showed the drain removing the correct tag; the real fault was a recurring late tag.
The first repair then moved the flicker to the left eye, which named the second half: the
pipelined capture delivers the previous slot, so the image needed relabelling, not only the
label. Both halves were reproduced in the host model before each headset run.

**VR-98 was a rule written for loading screens applied to a note.** The note movie was already
observed; its silence now counts as a menu's.

### 2026-09-13 - session 38: a menu is not a load

**VR-93 was solved by timing the settle before touching it.** The pre-fix resume split into
a 499 ms UI rescan holding the game thread, 15 ms of re-adoption and 343 ms of
recalibration - so the relearn and the hold were two fixes, shipped and measured apart.

**The negative control changed the design.** The identity check was built on the theory
that a respawned actor gets a new FName; the save-load run measured the same FName on the
new pawn, and the load was caught only by its new address. The game's own load event is now
a hard drop.

**Three faults turned up that are not this one**: a pre-existing garbage-collector crash
(VR-96), the ring correction going blind while `c5` reads zero (VR-97), and a sharper
signature for the after-note flicker (VR-80) whose own counterprediction then falsified the
`c5` explanation within one run.
### 2026-09-13 - session 37: head roll, and the eye the hands were given

Two faults, one fixed and one diagnosed, and three instruments that were not measuring what
they claimed.

**VR-91** was one run. The existing accounting probe could not answer it - it rejects any
sample rolled past 12 degrees - so the first build taught it to bin by roll and report
laterally, and the first headset run named the neck term as owning 98 per cent of the
residual. The fix is structural rather than a better constant: roll is removed from the FRAME
the arc is built in, so the pitch arc is untouched.

**VR-95 cost two falsified hypotheses and was solved by the marker history, not by
reasoning.** The ambiguity counter never moved; stereo pairing was clean; the probe was not
the frame-rate confound it looked like. What settled it was nine V presses: 90 flagged
presents, all one-sided, with the tag row alternating cleanly beside a classifier that
repeated an eye.

**The three instrument failures are the durable part.** A cross-check joined at the wrong
present printed a confident near-total disagreement that could not distinguish a phase error
from a sign convention - and the correct join was already written fifteen lines away in the
same file. The marker's tag reference is fitted to maximise agreement with the classifier it
judges, so its agreement figures are not independent. And a host suite had not compiled since
VR-76, so its checks had silently not run: a test that cannot build is not passing, it is
absent.

**The fix is not confirmed and is shipped off.** It removed the measured fault and introduced
a new one the counters cannot see. Ending a session with a disarmed lever and an honest open
entry is the correct state, not a failure to finish.

### 2026-09-13 - session 36: Blink aims by the controller (VR-36)

Two headset runs. The first confirmed the direction in one look - the marker followed the
controller, the landing point followed the marker, 0 ray refusals in 153 publications per
sample - and reported one fault: the distance had become unlimited, a blink into the sky
carrying about 100 feet up.

**Both causes were in the reach, and both were mine.** The run was set to `ReachMode=1` to
"leave the length alone", which is precisely the mode that replaces the length. That
discarded a rule nobody had noticed: the engine encodes Blink's reach, vertical cap
included, in the MAGNITUDE of the aim vector it hands out. And the maximum that replaced
it was being learned from the destination seam, which after the redirect reports the result
of our own vector - an input that was a function of the previous output, with no term that
could lower it. The log printed the ratchet as plain arithmetic, which is why it cost one
run and not a session.

The fixes are structural. The reach curve is clamped so every mode can only shorten what
the engine offered, which makes the class unreachable rather than fixing the instance, and
the maximum is learned only from the engine's untouched vector length. `ReachMode` ships 0.

The second run confirmed it: reach bounded at 931-1100 uu across the whole run, the +500
cap visible and honoured, no drift.

**The trace-start question was answered for free.** The engine's settled destination, taken
from the camera, sits within half a degree of the redirected ray at every sample, so the
trace starts at the camera and the parallax is the 65 to 93 uu between controller and
camera. Not visible in the headset, so no convergence correction was added for a difference
nobody can see - the instrument that could have justified one printed the unwelcome answer
and was believed.

### 2026-09-04 - session 15b: the ghosting is solved, and the verdict that hid it is fixed

Two headset runs, same build, same scene, same 2750x2850 render, only the headset's refresh
changed. 120 Hz: ghosting still reported. 90 Hz: **none reported, and no jitter**. Display
slots per frame went 1.05-1.11 -> 1.00-1.02.

**The cadence hypothesis was right all along, and session 14 killed it on a lying verdict.**
The `EVEN CADENCE` threshold was `|off| > 0.06`, so 1.03-1.05 - a beat every twenty frames -
printed as a clean bill of health, and that clean bill was read as falsification. The
instrument was correctly built and correctly read; the line between pass and fail had simply
been picked before anything was measured. Threshold is now 0.02, at the measured edge, and both
branches print the beat as a number (one frame in N, and its Hz) so an "even" verdict has to
show the residual it is forgiving.

**The tester's puzzle - why it drops to 60 at 90 Hz when it never went below 90 at 120 Hz -
has an answer, and the hitch rate is the proof.** Normalised by run length: 27.6 gaps/min at
120 Hz, 28.4 at 90 Hz. The stalls did not get worse. At 120 Hz the tick never fit the slot, so
the app free-ran and the compositor smeared over the mismatch - no cliff to fall off when you
are already past the edge, and that smearing IS the ghosting. At 90 Hz the tick sits right at
the period: frames make their slots (ghosting gone) but a miss costs a whole period, which is
22.2 ms, which averages toward 60 in a run. Smooth-and-always-wrong versus correct-with-a-cliff.

**The remaining drops are not ours.** 54 of 71 gaps sat in `present-tail (xrEndFrame)`, up to
101 ms. On a Wi-Fi streaming runtime that is the encoder or the link.

**The bbox prediction failed and is recorded as failed.** Gating the 3-second readback cut
samples from one per 3 s to 2-3 per run and changed the gap rate not at all. The
counter-evidence written down beside the prediction was the correct read. The gate stays - it
removed a real unlevered stall for free - but it did not fix what it was predicted to fix.

**The pose-lane instrument validated itself and was never armed.** `SEAM CHECK ok ... 20.67 deg
and 20.67 deg (0.00 apart)` in both runs: the sign calibration is proven against live data, so
the instrument would not have lied. Nobody ran `vrpace poseaudit on`, so the pose-attribution
question stays open - it is just no longer the ghosting's suspect.

Defaults now carry the judged values (`[Screen] 2750x2850`, with the 90 Hz half written into
the ini text beside it because it lives in Virtual Desktop), and
`tests/golden/known-good-2750x2850-90hz.ini` is the byte copy of the machine that was judged.

### 2026-09-04 - session 15: the pose lanes get an instrument, and a 3-second stall is found

Session 14 falsified the cadence hypothesis and left three suspects. Two of them died at the
desk, from files already on disk, before anything was written:

- **Virtual Desktop's SSW** - the tester confirmed it was already off.
- **Motion blur** - `MotionBlur=False` in `DishonoredEngine.ini`, with the Arkane developers'
  own comment beside it: "Motion blur is unwanted".

That left the tester's own read - "eye submit desync, or the world geometry isn't tracking the
head tracking correctly" - and it turned out the instrument for it was **half-built and
unreachable**. The pose audit already sat at the right line (immediately after the projection
views are filled, before they are attached), already wrapped to +-180, already rate-limited at
500 ms. Two things were wrong: it compared the tag against the pose the present thread had just
CONSUMED, which is fresh at submit and therefore never the sample the pixels came from - so it
could not answer the question it was named for - and **`set_pose_audit` had no caller at all**.
The `fovaudit pose on` command its comment named does not exist in this repo. It was dead code
that would have printed a confidently wrong number if anyone had reached it.

**What was built.** A locate generation counter bumped once per `xrLocateViews`; the game side
stamps every head sample with the generation it came from and publishes it, with the yaw the
camera write actually used, through a new `dvr::vr::publish_script_head` seam (needed because
`g_injHmdYawSnap` is static inside the unity TU and the runtime layer is a real module). The
audit now reports, per eye, the tagged yaw against the rendered yaw, the gap in GENERATIONS
against the active `lag`, and what one generation costs in degrees at the current head speed.
`vrpace poseaudit on|off` arms it.

**The sign trap, and why it is self-checking.** The two lanes read yaw out of the same matrix
with opposite conventions - `atan2(m02,m22)` against `atan2(-m02,m22)` - so a naive subtraction
reads about twice the yaw. That is the most convincing possible way for an instrument to lie:
a large, stable, entirely fake disagreement. The seam negates once and then PROVES it against
live data, reading the same pose back through the runtime's own converter at the first publish
and logging `SEAM CHECK ok` or `FAILED`. An instrument whose calibration is only asserted in a
comment is not evidence.

**The prediction, written before the run.** `DishonoredEngine.ini` carries
`OneFrameThreadLag=True`. The tagging code assumes one generation of skew (`lag=1`); with UE3's
render thread a frame behind the game thread it should be two. So: gap +1 at `lag 1`, nulled by
`vrpace lag 2`. If the delta reads 0.00 at `lag 1`, the hypothesis is dead and that is the
result.

**Found on the way: a full-frame CPU readback every 3 seconds, on the present thread, in the
shipping capture mode, with no lever.** The content-bbox instrument needs CPU pixels, and
`shared` mode exists precisely so that nothing goes to the CPU. Each sample is the same
`GetRenderTargetData` + `LockRect` + row copy that makes `sync` mode cost 17-21 ms/present -
about 31 MB at 2750x2850. `[Capture] BboxMs` (default 30000) and `capture bbox off|<ms>` gate
it; a size change still resamples at once. The prediction and the counter-evidence are both
recorded at the top of this file.

**Performance, answered with arithmetic instead of another run.** Fitting the three tick
measurements against megapixels gives ~0.64 ms/MP on a **~5.6 ms fixed floor**. The floor is
what makes 120 Hz unreachable - it leaves 2.7 ms of GPU for two full-frame scene draws - so the
resolution question is really a refresh-rate question. 2750x2850 at 90 Hz is the coherent
combination and is what is armed. There is no render-scale, no foveation and no per-eye
resolution anywhere in the codebase; resolution is the only pixel lever that exists.

**New tool:** `tools\arm-res.ps1` arms a size with the game not running, writing the same four
places `ResRequest` does. Arming used to cost two launches (the seam command only exists while
the game is up, and its write takes effect the launch after).

**Nothing was launched.** Everything here is built, linted, exports-checked and installed, with
the format strings audited by hand; no runtime behaviour is verified.

### 2026-09-04 - session 14: the throttle reading dies, the cadence is named

The session-13 instrument was installed and run (`alpha-260-g958ab57a`, Quest 3 / VDXR, 120 Hz) and
it did its job in both directions.

**It killed the hypothesis it was built to test.** submits/s 88-115 against 120 slots/s -
UNDER-SUBMITTING in 23 of 24 windows - with `endFrame mean` at 0.08-1.99 ms. The submit is not
blocking and never was; the display slots were going unfilled. Written prediction, held.

**It pointed at the real one.** The tester's bigger complaint on that run was ghosting - doubled
edges on world geometry when turning the head. `perf: tick` reads 9.2-9.9 ms against an 8.33 ms
slot, and `pacetrace.log`'s `TRACE pairs` reads interval mean 8.6-11.8 ms with **sd 1.3-9.6 ms** and
`waitGate 3-64 ms/s` - the game free-runs at ~1.13 display slots per frame, unevenly, so consecutive
frames are held for different numbers of slots. That is the interference beat `pace_sync_gate()` was
written for in the BioShock lineage, and its own comment predicted this shape of fault.

Shipped: the pair interval mean/sd and an `UNEVEN CADENCE` / `EVEN CADENCE` verdict with
slots-per-frame on the `stereo: rate` line (the numbers existed only in `pacetrace.log` at trace
level, which is why no one had seen them), and `[Pace] SyncHz` - the persisted form of
`vrpace sync <hz>`, shipping OFF, refusing an out-of-range value with the number it read.

**The proposed fix was wrong and was corrected in the same session.** `vrpace sync 60` locks the
cadence but 60 is too low for VR, and the user said so. Infinite's own numbers on the same runtime
say why no limiter is needed: it ran 80 pairs/s == its 80 Hz refresh with sd 0.3-1.0 ms, LOCKED, on
the same two-draw method - because its render cost fit inside its period. Ours does not (9.4 ms into
8.33), and we are also at 47 % more pixels per eye than Infinite's native. So the lever is the gap
between tick and period, from either end: `res 2064x2208f` (the Quest 3 panel's own size) or a 90 Hz
headset. Still a prediction; nobody has judged either in a headset.

Installed as `alpha-262-g61130855` (clean tag); builds, lint clean, exports OK.

### 2026-09-04 - session 13: the display period is measured, not inferred

The branch's first code. **One commit, instrument only** - no lever, no default, no render path.
`dvr::vr::display_period_ns()` had existed since session 42 of the BioShock lineage and printed in
exactly one place, inside the `PACE-BOUND` clause of `perf: tick`, which is a clause the hitching
run never triggered. So the headset's rate was absent from the one log that needed it.

Added: an `xrEndFrame` counter with its own cost (count, cumulative sum, per-window max) at the
single present-path submit site, and the display period, both published through `PairProbe`; the
`stereo: rate` beat line built on them, with a three-way verdict (OVER-SUBMITTING / MATCHED /
UNDER-SUBMITTING, plus UNKNOWN when the runtime leaves the period at 0); the period unconditionally
on `perf: tick`; the gap in display slots on `perf: frame gap`; the same numbers in `status.json`
and in `stereo status`.

**A prediction is on the record before the run** (OPEN section): the report's 4.3-6.6 ms present
interval, halved by reentry's one-submit-per-pair, puts submits at 76-116/s against 120 slots/s -
UNDER-SUBMITTING, which would falsify the throttle reading outright. The line was written so it can
say that.

**Verified as: builds, `lint: clean`, `exports OK: 9 names`, installed and hash-checked
(`alpha-260-g958ab57a`).** Not launched, not run in the simulator or a headset - every number the
new lines print is still unseen.

What was already measured before this session and should not be re-derived:

- The rig has **headroom**: mean present interval 4.3-6.6 ms against an 8.33 ms budget at 120 Hz.
  The complaint is not framerate.
- **48 hitches of 44-156 ms**, and **31 of 48 sat in `present-tail (xrEndFrame)`** - the submit
  call. `paceTimeouts` and `reset` were 0 throughout, so the pace lane is not timing out.
- **The display period is not logged**, so the obvious reading (the game outruns the headset and
  blocks at submit) is an inference. Measuring it is step one.
- Mono for the first ~6 s of a run then latching to 103-108 `2nd/s` is NORMAL and tester-confirmed;
  do not chase it.

### 2026-09-05 - session 11: the work is on a board, and the flow is written down

Branch `claude/linear-github-integration-9d1a52`, docs and templates only. No code, no build,
no render lever touched.

Three people are now finding faults and recording them in three private places, and PRs #12,
#14 and #15 each name real, measured, open defects that appear in no shared list. The
Linear workspace `vr-stereo-hub` existed but was empty.

**The board now matches the code.** Team `VR`, project **Dishonored VR Mod** with a lead, a
spec and its doc links; four release-shaped milestones (41.1, 41.2, 42.0, 42.1) kept in sync
with the GitHub Releases page, each naming the ROADMAP rungs it closes; a `Type` label group,
ten `area:` labels and five flag labels; **43 tickets** (VR-6 to VR-48) covering every open PR,
every unticked ROADMAP box, every genuinely open KNOWN_ISSUES entry, the three STATUS blockers,
and the four defects PRs #14 and #15 handed over. Merged work is recorded as **one catch-up
project update**, not as retroactive tickets. The four open PRs carry `Fixes VR-<n>` and two
were retitled to conventional-commit subjects.

**The flow is `docs/LINEAR_AND_GITHUB.md`**: statuses and what each means here, priority,
labels, the ticket template, the numbered ticket-to-release flow, the PR contract, project
updates, the release ritual, and what only the Linear UI can do. `CLAUDE.md` carries the hard
rules and the session protocol now names the ticket step. `.github/` gains a PR template, two
issue forms and `config.yml`; `CONTRIBUTING.md` is the front door.

**Two rules adopted.** From PR #15: **never quote a chat verbatim** in anything published -
commit messages, PR bodies, tickets, comments, `docs/`. The observation is evidence; the
wording never is. And: **an agent never declares a release.** It may report that a milestone is
clear and ask.

**Not done, and it needs the user.** The `Released` status does not exist yet and the PR
automation rows are unset: the Linear MCP exposes no tool for either, and both are team
settings. More importantly **the magic-word link did not fire** - Linear received the PR edits
(the diff records updated) and created no link, so `linkedIssues` is still empty on all four.
The ids and magic words are correct, so this is the GitHub integration's issue-linking side
not being enabled for the repo. Until it is, the PR-to-ticket links are the plain attachments
created by hand and no status automation will work.

### 2026-09-04 - session 12: the crouch height rise, solved

- **The answer**: the 38.16 deep-crouch capsule write moves the pawn 20.00 uu on every crouch (the
  engine keeps the feet planted, so a shorter capsule means a lower origin), and the camera's
  rate-limited catch-up never converges before the next crouch. `[PosTrack] DeepCrouch` now
  defaults to 0. Headset-judged: fixed.
- **The method lesson**: naming a mechanism and flipping its lever failed FIVE times running
  (physical crouch, the engine's uncrouch arithmetic, our eye clamp, the game's bump smoother, our
  own crouch eye-drop). What worked was filming the pawn and the camera one line per frame across
  the transition and letting the shape of the curve name the moment. When levers keep coming back
  null, stop naming suspects.
- **Two A/B traps paid for**: an interleaved A/B measures a system with memory BACKWARDS (the eased
  eye clamp redistributed the effect across the cycle boundary and the per-cycle median reported
  the sawtooth, not the climb - blocks with settling cycles fixed it); and a lever that was never
  connected reads as a clean FALSIFIED (the eye clamp had never executed at all). Confirm a lever
  moved something before believing its verdict.
- **A symptom mentioned in passing was the mechanism speaking**: "I could float around" turned out
  to be the pawn genuinely airborne, filmed rising 34 uu and falling 128 uu back to the floor.

### 2026-09-04 - session 13: the stale RIGHT eye, read out of the code, then confirmed

Branch `swapchain-one-picture-flicker`, 6 commits, one headset run at the end.

The hand-off pointed at `SceneDrawMaybeSecond`'s unlogged early returns. They are a dead end,
and ruling them out is what found the fault: `!doubleIt` means pass 1 pushed no `-1` at all,
the poison logs an `Error` at its one site and stands the method down, and the forced skip IS
the `forced=` field. So the game side pushed both tags on all 20 stale submits, every zero on
the line was true, and the second `-1` had to be manufactured downstream of it.

It was, in `reentry.cpp`'s c5 pairing block, whose two arms are not equally trustworthy. Full
reasoning in the FIXED section above and two entries in ARCHITECTURE's decision log. The
asymmetry was the tell: only the `-1` arm can misfire, and only a wrong `-1` strands the right
eye. `L=0 R=20` is that, arithmetically. **Confirmed in the headset**: 36 stale lines in 171 s
became 1 in 238 s, sc one-picture 36 % became 0-1 of 40, `sc-target repeats` 80 became 2, and
`c5Held=36` counts the deferrals that did it.

The second finding is about the instrument, and it is the more expensive one. The STALE line
carried the game side's gates and the runtime's failures but **not one counter from the method
between them**, and its owner string mapped `abortLeft` straight to "the game side skipped pass
2" - a cause it never measured. Nine logged instances, three readers, all sent to the wrong
file. Worse, the block's unconditional `tagged = true` made `method untagged presents` read 0
*because* a tag had been invented: the counter did not merely miss the fault, it denied it.

**And a process trap that cost a run**: the first "it's fixed" verdict came from a run of a
build that did not contain the fix. `build.ps1` had run, `install.ps1` had not. The log banner
and `Get-FileHash` against `build\src\RelWithDebInfo\d3d9.dll` catch it in one command, and
that check now leads the rig notes.

| Change | What |
|---|---|
| `1507bafc` | the cross-tick arm defers to the ring (streak 3 still overrides); no invented tags on an empty or 0 ring; `sameEyePushed` counted at the push site; the c5 counters and a corrected owner string on the STALE line |
| `12c23588` | ported: `[Stereo] HoldUntagged`, default 0, `stereo hold <n>` - now the lever for the residual mono flick |
| `539b9391` | ported: the `pair geom` separation-angle line, every 2 s |
| `9620d437` | ported: F2 stamps the fault marker, eyes-free |
| `00b833a8` | ported: the `res` seam writes its ini path with a real separator |

**All three fixed, each confirmed in the headset before moving on.** The chain: the c5 fix
exposed a mono flick on the arms (disparity is largest on the viewmodel, so a mono present shows
there and nowhere else); `HoldUntagged=3` removed that and exposed a black frame in both eyes;
the black frame was a zero-layer `xrEndFrame`, because the layer assembly sits inside
`if (backbuffer)` and a held present hands in no texture. Final run: `held=25 black=0`, `STALE`
0, `sc-target repeats=0`, one-picture `sc=0`, `out/s` median 201.

| Change | What |
|---|---|
| `1507bafc` | the cross-tick arm defers to the ring; no invented tags; `sameEyePushed` and the c5 counters on the STALE line, and a corrected owner string |
| `8441404f` | a zero-layer xrEndFrame re-submits the previous layer; `zeroLayerHeld`/`zeroLayerBlack` counted and logged |
| `12c23588` | ported: `[Stereo] HoldUntagged`, default 0 - the lever for the mono flick |
| `539b9391` `9620d437` `00b833a8` | ported: `pair geom`, the F2 fault marker, the `res` seam separator fix |

**Two process lessons, both paid for this session.** A parked commit's message is not evidence:
`HoldUntagged`'s claim that "the compositor holds the previous pair" was false against this
runtime and had never been run in a headset, and taking it at face value is what put the black
flicker in front of the tester. And check the build tag before reading a verdict out of a log -
the first "it's fixed" here was measured on a build that was compiled but never installed.

### 2026-09-04 - session 9: the eyes - the trace, the swap, the fix, the proof

Branch `claude/dishonored-vr-both-eyes-same-659cb5` -> PR #7, 13 commits. Runs on the dev PC
(simulator lane, RTX 4060, 2496x2688 VirtualMode, the sewers, shipped defaults; logs in
`D:\dvr-data\logs\45-run*.log`) and the user's Quest 3 through VirtualDesktopXR:

| Run | What | Result |
|---|---|---|
| 01 | the trace and the words, first build | `stereo: frameid` pairs from the arming: L-R 4.1 at bb/slot/out, floor 1.5, c5 6.17, busy 0; `reentry rearm 2` -> SINGLE x2 then DOUBLE; `capture reinit` -> REBUILT, no STALE; `dump eyes` queued + written off-thread, no gap, no LOADING; the sc stage empty (an ordering bug) |
| 02 | the sc stage, the side check | sc reads (4.7-5.0); **the side flipped across `reentry rearm 2`** and within a second of the first arming; the ring overflowed 363 times in the menu |
| 03 | the 0-tag push + the c5 pairing (first form), the A/B | side ok from the first pair; `reentry c5pair off`: the side flipped on its own twice in 25 s, `untagged 16-19` per window; on: no flips |
| 04 | the invariant as the pairing, the picture shift | side ok + shift -1 px on every pair, P1 == P2, untagged 0-1; `reentry.xrs` 11/11 |
| 05 | the drain to the next expected tag | side ok from the first pair across a `stereo mono` -> `reentry` switch and a rearm; 0 ring drops |
| 06 | the F10 EYES block | the overlay renders the readout and the buttons in the headset's own view (`xrsim-shot`) |
| 07 | **HEADSET (the user)**, the session's build | the eyes RIGHT from the load and after every button; `side ok` / `SWAPPED=0` on every pair, c5 6.11, shift negative, L-R 3-14 (one picture = 1.5); the ring skewed 131 times in ~4 min - the old swaps, absorbed; the trace read every present cost 1.5 ms GPU idle per present, tick 16.7 ms (60/s under 72 Hz) |
| 08 | **HEADSET (the user)**, the sampled trace + the A/B | "the fps is perfect now"; `c5 pairing` OFF + pause/resume -> the fault returns (`swapped=24 of 25`, then 12 of 12, the picture agreeing), ON -> `swapped=0` for the rest of the run. **The root cause is proven.** |

### 2026-09-03 - session 8: performance - the tick budget, the census, the 9Ex device, the shared capture

Branch `claude/dishonored-vr-perf-9f4b10`, 20 commits. Runs on the dev PC (simulator lane, RTX 4060,
2496x2688 VirtualMode, logs in `D:\dvr-data\logs\44-run*.log`):

| Run | What | Result |
|---|---|---|
| 01 | the tick budget, sync / deferred / off | stereo sync: tick 46 ms (21/s), capture 17-21 ms per present of which lock 9-13, GPU dma 15.5-16.8 vs 3D 4.8; deferred: 36 ms (27/s), lock 0, dma 10.4; off: 93 presents/s pace-bound; the marker 1 BeginScene per present, 0 late GPU reads; `mark` and the gap line print; `reentry.xrs` 11/11 |
| 02 | the creation census | 8060 of 8120 creations MANAGED (398 MB), READONLY texture locks 10598, no AUTOGENMIPMAP; the shadow route decided |
| 03 | `[Device] Ex=1 Managed=shadow` | `CreateDeviceEx -> 0x0`, IS 9Ex, `shared surface AVAILABLE`; 5240 twins, 65552 updates, 0 failures; the sewers intact; shared (one slot) 0.2 ms per present |
| 04 | the fenced two-slot shared capture, stereo | SharedWait=1: tick 13.3 ms (75/s), lock = the 3.6 ms fence wait, dma 0.2; SharedWait=0: 11.1 ms (90/s) PACE-BOUND; `reentry.xrs` 11/11; hammer 0 stale over 5 cycles; the frame intact |
| 05 | the final build | deferred default 27.7/s; `capture mode off` with 0 STALE lines (the no-frame fix); `focus lose 2500` / `focus regain`: `eaten=0`, 0 stale; `reentry.xrs` 11/11 |
| 14a | **HEADSET (the user)**, Ex=0, deferred | 30-33 ticks/s at 2496x2688 (dma 10.9 ms per present), 50 gap lines, no attack freeze felt |
| 14b | **HEADSET (the user)**, Ex=1 | 9Ex device up, shared AVAILABLE but the capture stayed deferred (30 ticks/s); after repeated quickloads the twin map filled with tombstones and the game crashed in D3D9 (minidump `dvr_20260903_212436.dmp`) |
| 06 | the tombstone fix, Ex=1 + shared, 3 quickloads | 2324 live, 1984 tombstones reused of 32768, 0 failures, the game alive; 90 ticks/s pace-bound |
| 15 | **HEADSET (the user)**, Ex=1 + shared | "performance is pretty good"; the eyes disagree "90 % of the time, more at the beginning": 0 STALE, 0 tag mismatches, pairs one IPD apart, 32 pause/resumes each with a 1-1.5 s flat spell |
| 07 | the read fence + the menu resume, shipped defaults | `readWaits` 14 in the run (the race was real); hammer 10 cycles 0 stale, `view live at once` x11; `reentry.xrs` 11/11 |

### 2026-09-03 - session 7: the four headset faults and the picker, on the simulator

Branch `claude/dishonored-vr-stereo-polish-449d43`, 15 commits. Runs (simulator lane, logs in
`D:\dvr-data\logs\43-run*.log`; the run-40 headset log archived as `42-run40-quest3-verdict.log`):

| Run | What | Result |
|---|---|---|
| 01 | commits 0-1d | `Method=mono applied after the game side registered`; the gate decision logs its reason; `reentry.xrs` 11/11, `stale-eye.xrs` 18/18; hammer 10 cycles PASS, ages L=1 R=0 |
| 02 | `reentry skip2 120` | strict off: sim stale 0 -> 2, mono +62, `STALE R EYE` (owner first "unknown", then the game side); strict on: stale unchanged, 37 fallbacks to mono |
| 03 | the phase + ahead | runtime clock extension on the sim; phase +58 ms mean (synthetic); `vrpace ahead 1` logs and locates; hammer 5 PASS |
| 04 | pitchtest x3 | engine neck 0.321/0.062 m (cons 0.3 uu); the arc reached c5 on top of it; `neck cancel` -> travel < 0.5 uu; the picture agrees |
| 05 | Armed + console | park/re-arm on the seam correct; the first console word overflowed the game thread's stack (the hook re-entered) |
| 06 | the guard, boot | `Method=reentry Armed=1 -> active reentry` before the first present; `setres 2560x1440f`/`1600x900w` dispatch, empty reply, no Reset: INERT |
| 07-08 | the ini route | 2560x1440 fullscreen in every ini place -> `CreateDevice 1920x1080 windowed=1`: the ini is inert |
| 09 | the command line | `-ResX=2560 -ResY=1440 -FullScreen` via 3 import slots -> `CreateDevice 2560x1440 windowed=0`, capture and swapchains followed |
| 10-11 | 2496x2688, no VirtualMode | the game asked the mode list, fell back to 2560x1440 (a harness launch had restored the mod ini; the launch file carries the token now) |
| 12 | **2496x2688 with VirtualMode** | our mode handed at slot 123; `CreateDevice 2496x2688 windowed=1`; `res: HONOURED`; hfov 108 deg; both eyes 77 % non-black in the sewers; the frame complete; readback 18-20 ms/present |
| 13a | **HEADSET (the user)**: 2560x1440 fullscreen asked | Reset 2560x1440 twice then 2508x1411 windowed=1 (the game's own fallback); the run sat in menus, stereo never armed (`state` skips); readback 5.3-5.8 ms/present |
| 13b | **HEADSET (the user)**: 2496x2688 VirtualMode | `CreateDevice 2496x2688 windowed=1`, HONOURED, "pretty sharp"; `neck cancel` right; stereo L/s=R/s=16-28, ticks 28/s, readback 13-15 ms/present; one `STALE L EYE` (age 567) at a FOCUSED regain; the desync still seen on load; judder unjudgeable |

### 2026-09-03 - session 6: S2b - the capture cost, the lanes, the root, the second draw

Branch `claude/s2b-stereo-scene-draw-a341c5`, seven commits on `VR-Main` (24b22390): the
capture cost measured and the modes, the pipelined deferred capture, positional tracking on
the camera seam, the projection claim and the FOV handoff with the state-gate fixes, the
root derivation and the second draw, the docs. Runs on the dev PC (simulator lane, logs in
`D:\dvr-data\logs\42-run*.log`):

| Run | What | Result |
|---|---|---|
| 16 | capture modes | probe: shared REFUSED; sync 5.4 ms, deferred (first form) 5.2 ms: no gain; `mono.xrs` PASS both |
| 17 | user-memory surface | REFUSED (D3DERR_INVALIDCALL), fell back to sync |
| 18 | the lock split | sync: lock 2.4-3.1 ms, copy 0.7, upload 1.5; deferred first form: the lock still waits |
| 19 | deferred pipelined | lock 0, total 2.25-2.4 ms; `mono.xrs` PASS |
| 20 | postest | camera lane HONOURED on all axes - on the attract camera (the state mislabel found in run 21) |
| 21 | projection on the mono screen | two views, sensor 137, claim readback; the pictures were the title screen, then the loading screen (DISCARDED there), then the sewers: eyetest 120/120, postest +30.0 in real gameplay |
| 22-24 | the state gate | menu/cine tracking hoisted, main-menu flag, LOADING state, the cinematic latch cleared on a new pawn: title MENU -> quad, load LOADING -> quad, level GAMEPLAY -> projection |
| 25 | 1440x1440 | the game stayed 1920x1080 with ResX/ResY=1440 in both ini places; second aspect open |
| 26 | census + scrapes | PVR from one site once per present; render thread presents; the draw chain to the HUD PostRender |
| 27 | tick chain + probes | both chains under UGameEngine::Tick; the root 0x5fc5b0 named from the bytes at 0x6330da; pe-xref confirms every edge |
| 28 | first light | pulse: 3 second draws at 218-414 us, presents +1 each; `stereo reentry`: L/s 54 R/s 53, pair c5 travel 6.17 uu, no fault; the ring cleared every few seconds |
| 29 | the ring fix + soak | L/s 52 R/s 52 mono 0, ringCleared 0, 90 s clean; `reentry.xrs` 11/11 |
| 30 | **the headset (Quest 3, VDXR, the user)** | the doubling ran (draws 54 = 2nd 54, presents 108, pair 6.08 uu) but L/s=36 R/s=54 mono/s=18: left tags dropped by the position check while walking -> both frames in both eyes; lean reversed, a second motion on pitch (the head's displacement and roll not driven under the projection layer) |
| 31 | the fixes, sim | tags never dropped: L/s 53 R/s 53 mono 0; `reentry.xrs` 11/11; the lean under projection read 13.7 uu for 30 cm (the reference had crept) |
| 32 | the stable reference | 30 cm -> +29.4 uu held for 10 s, crouch/forward signs right, postest HONOURED; CINEMATIC stuck across the load (the pawn latched before the title toggle) |
| 33 | the cinematic latch | `cine: latch cleared - leaving the main menu`, LOADING -> GAMEPLAY, L/s 53 R/s 53 mono 0 |
| 34 | HEADSET (the user) | stereo good; tilt and lean reversed in all four directions, also with `stereo projection on` |
| 35 | the lane picture test | 2 m right / 2 m up on both lanes: the camera lane MIRRORED the vp lane on both axes - the field's sign |
| 36 | sign +1 | both axes match the vp lane by picture; eyetest HONOURED 119/120 (c5 -99.2 for +100), postest HONOURED both axes, L/s 52 R/s 51 |
| 37 | roll by picture | roll write lands (incoming = wrote); +20 right-ear-down leaned the verticals RIGHT - reversed |
| 38 | roll negated | +20 leans left, -20 right; forward axis matches the vp lane; pause/resume re-pairs cleanly (L/s = R/s, mono 0) |
| 39 | the verdict logger | `gameplay verdict: FALSE (menuOpen) ... -> the head-locked quad` 30 ms ahead of the runtime's own line |
| 40 | **HEADSET (the user), the verdict** | PASS: stereo depth, tilt, lean, look, crouch all correct. Open: the arming glitch (right eye), judder on fast movement, the pitch pivot behind the camera, and the F10 resolution picker + arming tickbox |

### 2026-09-02 - session 5: the state as session 5 left it (archived)

**The render is restarted on a native D3D9 game.** The DXVK fork, the side-by-side present
pipeline, the 4032x2268 window machinery, the OpenVR backend and the mod's own OpenXR
loader/pace thread/input are removed (one commit each, so `git revert` restores one piece;
history keeps the fork under the `dxvk-*` tags). The BioShock trilogy mod's OpenXR runtime
layer is the single backend (`core/vr/openxr_runtime`, verbatim behind two D3D9 seams: the
device provider and the frame texture), the static Khronos loader is linked into `d3d9.dll`,
and SteamVR rigs go through the bundled `dvr_steamvr32.dll` shim. Stereo is a SEAM with named
methods (`core/gfx/stereo.h`: `[Stereo] Method=mono|aer|reentry`, `stereo <name>` live):
the mono screen (rung 1) works, `aer` and `reentry` are registered design stubs with their
notes. The per-eye camera seam (`game/dishonored/camera`) carries rotation (measured), FOV
(measured) and the lateral eye offset (unmeasured, with the `camera eyetest` instrument).
Version 41.0.0, `[Meta] Version=10`. ARCHITECTURE and ROADMAP (S0-S3) describe it.

**Verified on the dev PC (the game IS installed here, `D:\SteamLibrary`), simulator lane**,
build `g4fb67333` and later, 2026-09-02 evening, eight runs:

- `xrsim-selftest.ps1` PASS; `xrsim-launch.ps1 -ViaSteam` reaches `xr: instance created on
  runtime 'dvr-xrsim'`, `xr: runtime "dvr-xrsim"`, `xr: pipeline READY`, session FOCUSED,
  `xr: first frame submitted to the headset (1600x900 quad)`, frames advancing at the sim's
  90 Hz (`stereo: beat method=mono out/s=90`).
- `status.json`: `state GAMEPLAY` (the game auto-continues into the last save), `stereo.method
  mono`, `framesOut` advancing, capture bbox `100% x 100%`, 97% non-black, `camera.c5ok true`.
- `stereo aer` / `stereo reentry` refuse with their note and mono keeps running; `stereo mono`
  is a no-op; `camera status` prints.
- `xrsim-shot`: a quad layer whose SOURCE reads 97.2% non-black in BOTH views with a full bbox;
  the composite reads L 37.95% / R 37.98% (world-locked quad, run 7) then L 16.4% / R 16.3%
  (head-locked quad, run 8), no `COMPOSITOR fault` / `APP fault` line. The session-4 black
  left eye did not reproduce; the simulator now attributes it if it does.
- `dump frame` writes `capture_*.bmp` (5.7 MB) and `eye_*_mono.png`; `soak.ps1 -Minutes 3`
  exit 0 (PASS, no wedge, no dumps); the crash file carries the run headers.
- `camera eyetest 100` in gameplay: run 7 wrote nothing (the lever off = no camera revalidation;
  fixed), run 8 wrote all six candidates and measured a CONSTANT offset between the draw's c5
  and each field (+6620 uu for 0x80/0x90/0xc4, +14140 uu for 0x330/0x350/0x374 along right):
  the fields are not c5's quantity in c5's frame, so the measure was redesigned around a
  per-candidate c5 baseline (commit `cf9ec6f2`). Runs 10-11 (after the stuck process cleared
  on its own, no reboot): **camera+0x330 HONOURED 119/120** (+99.2 uu of the asked +100; it
  holds -c5 exactly, so the write is negated), the other five DISCARDED. The eye-offset write
  point is measured; `[Camera] EyeField=0x330` is the default (ENGINE_NOTES has the table).
- `mono.xrs` PASS (both eyes 12.9%, equal bboxes, no fault line) and the head-lock pair on the
  fixed simulator: the composite bbox is IDENTICAL at yaw 0 and yaw 30 (run 9).

**Found on the way** (each fixed in its own commit, all measured, none guessed):

1. The game calls `Direct3DCreate9` twice; the second `init_instance` failed with
   `XR_ERROR_LIMIT_REACHED` and the fallback chain declared VR off (guarded).
2. The handoff's trap 6 is real: a DIRECT exe launch crashes at the main menu
   (`Dishonored.exe+0x60907e` reading NULL, thread "other", right after
   `DisGFxMoviePlayerMainMenu Start`); a Steam launch survives it. `xrsim-launch.ps1 -ViaSteam`
   exists for this and is the only way to run the simulator with the game here.
3. The agent's shell on this PC VIRTUALIZES writes under the user profile: files the harness
   wrote to `%LOCALAPPDATA%\DishonoredVR` (the sim manifest, `command.txt`) existed for the
   shell and a game it launched directly, and not for a game launched through Steam (its
   listing held only game-written entries; a WMI-created `dir` agreed). `[Paths] DataDir=` in the
   ini and `DVR_DATA_DIR` for the scripts point both at `D:\dvr-data`; the simulator takes its
   state dir from the manifest's directory (VERIFICATION gotcha 14).
4. The loader's property store beats the environment: `init_instance` hands `[VR]
   XrRuntimeJson` to `xrInitializeLoaderKHR` (XR_EXT_loader_init_properties) as well, and logs
   whether the manifest is readable and its library loads.
5. The config's version rewrite dropped `[VR] XrRuntimeJson`; it now carries `XrRuntimeJson`,
   `Runtime` and `DataDir` over.
6. The fresh ini armed `FovLever=130` (the side-by-side value) and wrote it 600 times per 3 s
   into a 90-deg camera; the lever ships off on the mono screen.
7. The mono quad sat in BioShock's world-locked LOCAL space; it is head-locked now
   (`[Screen] HeadLocked=1`).
8. The SIMULATOR composited quads in the wrong place (60 px outward per eye at yaw 0, 460 px
   of swing at yaw 30): the cbuffer matrix was read column-major and the view matrix's rotation
   block was transposed. Fixed (`d43eea11`), selftest PASS, and the re-measure passed (run 9:
   identical bboxes at yaw 0 and 30). BioShock's eye legs never saw it (projection layers
   rotate rays in the shader).
10. The c5 capture only caught an upload STARTING at register 5; after the two device Resets
   a level load brings, the engine batches it into a c0 x128 block and the seam saw no c5 for
   a run (run 9). Any block covering c5 feeds it now (`dd10da09`).
9. Quitting: `console exit|quit` returns -1 (the console seam does not reach a quit); WM_CLOSE
   logs `ViewportClosed` and then the process LINGERS with one thread, unkillable (no `PreExit`,
   no `proxy unloading`), which then holds `d3d9.dll` and the simulator DLL open and makes Steam
   refuse a relaunch. This ended the session's runs; a reboot clears it. Run 6 also logged an
   access violation inside `d3d9.dll+0x87c95` (VR disabled, right after a device Reset following
   a `GetRenderTargetData` failure on a multisampled backbuffer), thread "other", three times at
   page ends 5.3 MB apart; the process survived it. Unsymbolized (that build is gone); the Reset
   + AA path is the first suspect.

**Headset: verified** (run 13, 2026-09-03): Quest 3 through VirtualDesktopXR, the game on the
head-locked screen in both eyes, head tracking and the gamepad working. The quit crashed on
that run (the 38.79 class: VD's thread through a freed d3d11 pointer after PreExit with the
session still open). The handler had sat inside the motion-aim block since 38.79 and never
ran under GamepadOnly; hoisted, it closes the session from PreExit and the third quit (run
15) was clean: `shutdown: game PreExit`, `xr: session teardown`, `instance destroyed`,
`proxy unloading`, no exception.

**Not verified**: the SteamVR shim with
Dishonored; `apply_eye_offset` driving a real per-eye render (no method asks for an eye yet);
`head_track`/`pad_bridge` as real modules (deferred, S1).

#### Session 5 next steps (superseded by the list above)

**Both**: the recipe on this PC is `tools\build.ps1; tools\install.ps1;
$env:DVR_DATA_DIR='D:\dvr-data'; tools\xrsim-launch.ps1 -ViaSteam` (the game ini carries
`[Paths] DataDir=D:\dvr-data`), foreground the window, `tools\xrsim-run.ps1 -Path
tools\xrsim\mono.xrs -Dir D:\dvr-data\xrsim`. Close the game with Stop-Process while it is
healthy, never with WM_CLOSE (blocker below). Copy `dishonored_vr.log` out before every
relaunch. The eyetest is done: the eye offset writes into camera+0x330 in negated form
(`camera::apply_eye_offset`); `camera eyetest 100` re-measures it on any build.

**Developer A (AlternateEye, S2a)**: read `core/gfx/aer.cpp`. The eye field is measured
(0x330), so the method only has to alternate `eye_for_next_frame()` and tag each present; the
seam writes the offset on the script lane. Acceptance: `stereo aer`
accepted, the beat line `L/s == R/s == out/s / 2`, `stereo.xrs`, `eye-check.ps1` legs 0-5, the
runtime's pair probe clean.

**Developer B (SequentialReentry, S2b)**: read `core/gfx/reentry.cpp`. Task one is the
scene-draw root (caller census at `ApplyHeadToViewRotation`, live stack scrape, identify the
pass by making it MOVE with the eyetest as the mover); every address to `patterns.h` with its
derivation in ENGINE_NOTES; the second call deny-by-default and SEH-guarded.

**The user**: the headset run on Quest 3 via VDXR: `tools\install.ps1`, launch through Steam
with VD streaming and VDXR active (SteamVR not running), expect the game on a head-locked
screen in both eyes, head rotation turning the view, the gamepad working; F10 for the screen
size; send `dishonored_vr.log`. Quit through the game's own menu and report whether the process
lingers.

#### Session 5 blockers (superseded)

- **WM_CLOSE leaves a stuck `Dishonored.exe`** (one thread, unkillable, holds `d3d9.dll` and
  the build's `dvr_xrsim32.dll`, Steam refuses a relaunch). Pid 13452 cleared on its own after
  about an hour, no reboot. Until the quit path is understood, close a healthy game with
  `Stop-Process`, which works.
- **The quit path**: `console exit` returns -1; WM_CLOSE leaves the process lingering with no
  `PreExit`; the runtime layer's teardown therefore never runs on a graceful close. Which
  thread is stuck (the simulator's, the runtime's, a driver's) is unknown; a debugger on the
  next occurrence, or a minidump taken by hand before killing it.
- The headset run needs the user.

### 2026-09-02 - session 5: the native-stereo foundation (41.0)

The decision (docs/ARCHITECTURE.md decision log, session 5): four headset sessions showed the
DXVK side-by-side design cannot be tuned; the game renders natively again and stereo is rebuilt
as a ladder of methods on one seam, two developers taking rungs 2 and 3. One PR
(`claude/native-stereo-foundation-77e2b6` -> `VR-Main`), 27 commits: seven removals, the
static loader, the runtime layer, the shim, the stereo seam + mono screen, the camera seam +
eyetest, the stubs, the simulator instruments, the harness, the docs, then the fixes the
first runs demanded (above, "Found on the way").

Runs on the dev PC (simulator lane; logs in `D:\dvr-data\logs\41-run*.log`):

| Run | Launch | Result |
|---|---|---|
| 1 | direct exe | instance on dvr-xrsim, then init_instance twice -> VR off; the game CRASHED at the main menu (`Dishonored.exe+0x60907e`, trap 6) |
| 2 | Steam | the ini rewrite dropped XrRuntimeJson -> VDXR (no headset), flat |
| 3 | Steam | env var set but the loader answered RUNTIME_UNAVAILABLE |
| 4 | Steam | loader property override set; manifest "path not found" (err 3) - the sandbox finding |
| 5 | Steam | the path probe: the game sees 5 entries where the shell sees 7 |
| 6 | Steam (no VR) | an AV in `d3d9.dll+0x87c95` after a Reset + RTD failure, three times, survived |
| 7 | Steam, `D:\dvr-data` | **dvr-xrsim, FOCUSED, first frame submitted, GAMEPLAY, both eyes 38% non-black, soak PASS 3 min**; eyetest NOT WRITTEN (null camera) |
| 8 | Steam | head-locked quad 16% per eye but swinging with yaw (the sim's quad math); eyetest wrote, measured the field/c5 offsets; WM_CLOSE -> the stuck process |
| 9 | Steam (after the process cleared) | `mono.xrs` PASS; head-lock pair IDENTICAL bboxes on the fixed sim; no c5 (the c0 x128 block) |
| 10 | Steam | c5 back; eyetest: 0x330 reads -c5 and moves c5 by -98.7 uu (75/76), the rest discarded |
| 11 | Steam | sign-aware seam: **0x330 HONOURED 119/120 (+99.2 uu)**, five DISCARDED; `camera eyefield 0x330` |
| 12 | Steam + Quest 3 (VDXR) | flat: a stale `[VR] XrRuntimeJson` (the sim manifest) made the loader fail; fixed to warn and ignore (`21e1cb64`) |
| 14 | Steam + Quest 3 (VDXR) | quit crashed again: the PreExit handler never ran (it lived inside the motion-aim block, off under GamepadOnly) - hoisted (`cf506ba4`) |
| 15 | Steam + Quest 3 (VDXR) | **clean quit**: `shutdown: game PreExit`, session teardown, instance destroyed, proxy unloading, no exception |
| 13 | Steam + Quest 3 (VDXR) | **THE HEADSET RUN: VirtualDesktopXR, Meta Quest 3, FOCUSED, READY, the screen in both eyes following the head, the gamepad working (user's report)**; quitting through the menu crashed 2.3 s after PreExit (EIP DEDEDEDE in d3d11.dll on VD's thread, the session still open) - teardown moved to the PreExit handler |

### 2026-09-02 - session 4e: gamepad-only, and the three rendering symptoms

Headset run at 4032x2268 requested / `capture: 3840x2160` actual. The tester reported three
things and they turn out to be one geometry. Full derivation in ENGINE_NOTES, "The three
rendering symptoms, and the one geometry that ties them".

1. **"Super pixelated, but the pause menu is huge like it's at full resolution."** Both
   halves are the same fact: SBS gives the WORLD half the frame width per eye
   (`per eye 1920x2160`) while a MONO menu frame samples the whole 3840 across the same quad.
   The menu is drawn at exactly twice the world's horizontal sampling density. That is the
   cleanest confirmation of the SBS packing anyone has produced, and it is not a bug - but it
   means the frame must be at least `2 x eyeWidth = 4992` columns for a 1:1 world. At 3840 the
   world sits at 77% of the panel.
2. **The fisheye is `FovLever`.** It does not only size the quad, it WRITES the game camera's
   FOV (`fov_lever.cpp`), so `FovLever=130` makes the game render 130 deg horizontal - the log
   agrees (`MEASURED render FOV ... = 130.0 deg`). A 130 deg rectilinear frame shown across a
   94 deg frustum stretches the edges. The author already knew: `frame_hooks.cpp` disarms the
   lever on overshoot, commented *"rather than leave the user in a fisheye"*.
3. **The black bottom border cannot be tuned away at 16:9.** Filling a 99 deg vertical
   frustum needs `lever = 2*atan(tan(v/2)*aspect)`: 128.6 at 16:9, 114.6 at 4:3, 100.5 near
   square. So the fisheye and the border are the SAME setting pulled in opposite directions,
   and at 16:9 nothing satisfies both. The tester found that empirically. A taller frame is
   not a preference, it is the only way out - which is exactly what they asked for.

**Next single change: `3840x2880` (4:3) with `FovLever` ~115.** Same per-eye width as now, so
no sharpness regression, +33% pixels, and it should visibly ease the fisheye.

**Two corrections to 4c, both mine.** (a) "Must be a real display mode" was too strong -
3840x2160 WAS honoured. The real rule is narrower: **`PinBackbuffer=1` causes the crop**; a
size the game rejects merely falls back, harmlessly, as long as the pin is off. Both effects
were present at 2850x2750, which made them look like one. (b) There is no 2560x1440 cap -
that was read from the run before the pin was turned off. 4032x2268 is still not honoured
(empty `setres` replies), so **trust `capture:`, never the requested number**.

**Applied this session:**

- **`[Mode] GamepadOnly=1`, new and default ON** (`config.cpp`). Turns off SkelControl hand
  writes, hand mesh, motion aim, motion melee, motion crouch and controller Blink aim, and
  scales no hand or weapon model. Head tracking, positional tracking, the FOV lever and the
  virtual gamepad keep running - this is NOT the `XR_SAFE` bisector, which also stops the
  head. It logs loudly and `status.json` gains `gamepadOnly` so the zeroes below it read as
  BY DESIGN rather than as failures. The author's rule that motion crouch and hands "must
  never stop working" is respected: nothing is retired, it is one key, set `GamepadOnly=0`.
- **The tester's tuned values are now the repo defaults**, in both the generated ini text and
  the `IniFloat` fallbacks, so a fresh install comes up where the headset testing left off.
- **`[PosTrack] Scale` default 50 -> 98.** This closes 40.2b: the tester tuned world scale by
  feel and landed on 98, within 2% of the 100 derived from the movement constants, arrived at
  independently and without seeing the number. That is the cross-check 40.2b was waiting for.
- Hand trims and `HandSize` reset to neutral, per the tester's "no scaling or changing the
  default hand/weapon models".

Build clean, exports 9/9 undecorated, lint clean, RelWithDebInfo installed. The fork and
`dxvk_stereo.txt` are untouched.

### 2026-09-02 - session 4d: the lever is half of the resolution setting

**The 3840x2160 run was full-frame but letterboxed** - tester: "almost sort of right again,
only problem was that the resolution was rectangular so it didn't fill my view". Both halves
of that are now explained, and one of them was my error.

**My error.** Session 4's restore set `FovLever=100`, correct for the near-square 2850x2750
it was paired with. Session 4c then changed the render to 16:9 and left the lever at 100.
The frustum-fill branch takes its vertical extent as `tan(fovDeg/2)/aspect`, so at 16:9 with
lever 100 the quad clamps to **67.7 deg inside a 99 deg frustum** - letterboxed by
construction. At lever 130 the same 16:9 render fills edge to edge with ~9% black at the
bottom. Changing the render aspect without changing the lever is NOT a one-variable change:
the pair is the variable. Table in ENGINE_NOTES, "FovLever IS the vertical fill lever".

**The requested resolution is not being honoured at all.** The mod asked for 3840x2160; the
log says `capture: 2560x1440`. With `PinBackbuffer=0` that is harmless - buffer and content
agree, no crop, which is why 4c's fix worked - but **this rig cannot render above 2560x1440**
(desktop 5120x1440 caps it). Every "4032x2268" run here is really 2560x1440. Trust the
logged `capture:` number, never the requested one.

**F10 SAVE AS DEFAULTS was finally pressed** (the thing session 3c said had never happened).
The tester's tuning is now persisted and snapshotted to
`tests/golden/f10-tuned-2026-09-02.ini` and `<game>\...\dishonored_vr.ini.f10-saved-0247`:

| key | default | tuned |
|---|---|---|
| `[Tracking] HeightOffsetM` | 0.000 | 0.040 |
| `[Screen] FillScale` | 1.00 | **0.74** |
| `[Screen] DistanceMeters` | 1.60 | 1.67 |
| `[PosTrack] Scale` | 50.0 | **100.0** |
| `CrouchToggle` | 1 | 0 |
| `XrLayer` | (absent) | proj |
| `StampFix` | (absent) | 0 |

`[PosTrack] Scale=100.0` matches the measured 100 uu/m from commit 60235b86 - the tester
converged on the measured value by feel, which is a good cross-check on that measurement.

**Applied on request: GingasVR's resolution default**, as the coherent pair -
`RenderWidth/Height=4032x2268`, `SpoofDesktopW/H=4096x2304`, **`FovLever=130`** - plus
`DishonoredEngine.ini` and all four AppCompat buckets at 4032x2268. `PinBackbuffer` stays 0
(her ini has no such key) and `MenuFillScale` stays 1.00 (the 4b fix). Every F10 value above
is preserved. Backups `.pre-gingasres` / `.f10-saved-0247`. NOT YET TESTED.

**Expect this**: the render will probably clamp to 2560x1440 again (fine, still 16:9), and at
lever 130 the quad should fill horizontally and to the top with ~9% black at the bottom -
**but `FillScale=0.74` will still present it at 74% of that**. F2 raises FillScale live in
the headset; that single knob is the difference between 74% and full. Judge the lever first,
then the fill.

### 2026-09-02 - session 4c: THE RENDER WAS NEVER 2850x2750

**This is the root cause.** The tester sent a desktop-mirror screenshot of the main menu with
the picture in the top-left of the window. The mirror blits the backbuffer's LEFT HALF
pillarboxed (`frame_hooks.cpp:415-460`), so its horizontal placement is expected - but the
picture filled only the top ~52% of the window, and that is not.

**Measured, six capture dumps, non-black bounding box:**

| requested buffer | actual content | real display mode? | verdict |
|---|---|---|---|
| 1600x900 | 1600x900 | yes | FULL |
| 2560x1440 | 2560x1440 | yes | FULL |
| 3840x2160 | 3840x2160 | yes | FULL |
| 4032x2268 (GingasVR's) | 3024x1440 | no | CROPPED |
| 2750x2850 | 2750x2200 | no | CROPPED |
| **2850x2750 (our "known good")** | **2560x1440** | no | **CROPPED** |

At 2850x2750 the game draws exactly **2560x1440 into the top-left** and leaves the rest
black. So the whole session's geometry was applied to a frame that is half empty. It
explains all three symptoms at once: tiny (content covers ~90% x 52% of the quad), top-left
(it is literally there), and the eyes not fusing (the SBS halves meet at x=1280, not the
x=1425 the split assumes, so each eye gets part of the other's view plus black).

**Why nothing caught it.** `PinBackbuffer=1` forces the DEVICE to 2850x2750 while the game
renders at the size it asked for (`CreateDevice the game asked for 2560x1440`). The mod then
spoofs `GetClientRect` to 2850x2750 and the setres path reads that spoof back, concluding
`setres: the game is already at 2850x2750 - skipping the resolution script entirely`. A
check reading our own spoof cannot fail its own hypothesis, so the engine-side resize never
ran and `capture: 2850x2750` was logged for a half-empty frame.

**`PinBackbuffer` is ours, not GingasVR's.** Her tuned ini (`.pre-2750`) has no such line.
Every ini since session 2 sets it to 1.

**This is probably the central open bug.** 4032x2268 is not a standard mode either and
cropped here too. Whether an injected mode is honoured depends on the machine's GPU, driver
and desktop mode - this rig's desktop is 5120x1440, and two of the three cropped captures
came back exactly 1440 tall. It predicts affected users have a desktop shorter than the
requested render height, and it is falsifiable by asking one for their desktop resolution.

**The Documents folder was ruled out**, on the tester's suggestion: `DishonoredEngine.ini` is
vanilla plus the intended VR lines, all four AppCompat buckets were already correct, and no
file in the Config directory contains 2560 or 1440.

**Applied for testing** (one coherent change: use a resolution the display actually offers):
`RenderWidth/Height 2850x2750 -> 3840x2160`, `SpoofDesktopW/H -> 3840x2160`,
`PinBackbuffer 1 -> 0`, plus `DishonoredEngine.ini` and all four `[AppCompatBucket1..4]` to
3840x2160. Per-eye half 1920x2160 = aspect 0.889, the same per-eye aspect as GingasVR's
4032x2268. Backups: `.pre-realmode` next to each of the three files. NOT YET TESTED.
Fallback if 3840x2160 is not honoured: 2560x1440, identical per-eye aspect, and the game
asked for it itself.

**Also confirmed this session**: the 4b MenuFillScale fix works. The 02:35 run logged 28 quad
rebuilds, all at `fill=1.00` / 100.0 x 98.0 deg, none at 0.60. The size pumping is gone.

**Next instrument to build**: nothing compares the captured frame's real content extent to
the buffer size. A non-black bounding-box check on the capture, logged once per resolution
change, turns this class of bug into one line. The setres check must also stop reading the
mod's own `GetClientRect` spoof.

### 2026-09-02 - session 4b: the world size PUMPS, and MenuFillScale is why

A headset run on the restored known-good ini (02:23, VirtualDesktopXR + Quest 3) reported
"still rendering tiny and in the top left corner". Its log names the cause outright, so this
did not need a new instrument. Full derivation in ENGINE_NOTES, "MenuFillScale pumps the
world size during GAMEPLAY".

**The number.** The quad subtends **71.1 x 69.2 deg** inside a frustum of **94.0 x 99.0 deg**
- about half its solid angle. That is "tiny", fully explained, and nothing to do with
resolution, adapter, world scale or convergence. 40 of the run's 46 quad rebuilds were at
`fill=0.60`; the run ended there.

**The mechanism.** `MenuFillScale=0.60` and the `XrFrustumFill` gate are driven by the SAME
condition (`g_menuOpen || g_inMenu || g_sbsMonoNow`), and a change in it forces a rebuild.
The menu flag flaps during gameplay (the `Req_SaveSlotInfos` save-slot polls, already
documented at `present.cpp:613-616`), so the world size pumped 100 -> 71 -> 100 -> 71 -> 100
-> 71 deg across the six seconds before the crash, all after gameplay had started. The
`sbs:` line proves it is the MENU flag and not the splice counter: its last transition is
well before the pumping began.

**Why the Index never saw it.** OpenVR has one geometry path, so `MenuFillScale` only ever
dimmed a menu. `XrFrustumFill` (38.13) added a second path for the OpenXR port without making
the transition continuous, so on Quest the same flap swaps the whole quad construction
mid-gameplay. The tester's own read - Index/SteamVR was the tuned target, OpenXR/Quest a
later port - is exactly right here.

**Applied, config only, one variable, no rebuild**: `[Screen] MenuFillScale 0.60 -> 1.00`
(backup `.pre-menufill`). The menu branch now builds the same 100.0 x 98.0 deg quad as
gameplay, so a flap cannot change the world size. Cost: menu edges crop, which is what 32.4
added the key to avoid. NOT YET TESTED.

**A falsifiable prediction, and it contradicts session 3c.** Worked from the logged frustum,
the authored quad's vertical border must be SYMMETRIC, ~21% black top and ~21% bottom, with
the world in the middle 57.6%; horizontally the left eye gets 29.8% black on the temple side
and 5.6% on the nasal side (mirrored in the right eye - the rigid-screen design). Session 3c
recorded "top ~54%, bottom half black". The next `dump eyes` settles it: symmetric borders
retire that contradiction as a misread dump; a real black bottom half falsifies this model.

**The run also CRASHED** at 02:23:49, wild instruction pointer, minidump at
`%LOCALAPPDATA%\DishonoredVR\dumps\dvr_20260902_022349.dmp`. Untriaged, separate lane.

**Good news in the same log**: the fork's projection export resolved this time -
`quad/fill: world scale is set by the MEASURED render FOV (fork dxvk_vr_proj) = 100.0 deg`.
The landscape fix (session 3b) worked; world scale is no longer an assumed constant.

### 2026-09-02 - session 4: reverted to the known-good point

No launches, no code changes. Session 3c left the rig one key away from its own
confirmed-good configuration and that key was an open, unevaluated experiment; this
restores the documented point so the next run starts from a known baseline.

**What was actually different.** Exactly one line: `FovLever=130` vs `100`. Everything
else already matched - `RenderWidth/Height=2850x2750`, `SpoofDesktopW/H=2816x2880`,
`PinBackbuffer=1`, `GameFOVDeg=100`, `FillScale=1.00`, `[PosTrack] Scale=50.0`, the game
ini at 2850x2750 on both `[SystemSettings]` and `[SystemSettingsEditor]`, and all four
`[AppCompatBucket1..4]` at 2850x2750. `setup-game-ini.ps1` did not need re-running.

**The snapshot checks out.** `tests\golden\known-good-2850x2750-lever100.ini`, the game
folder's `dishonored_vr.ini.KNOWN-GOOD` and `dishonored_vr.ini.pre-lever130` (the ini as it
actually ran when the tester reported "the eyes seem to overlap correctly and provide
depth") are all identical. The snapshot is a truthful record of the run, not a
reconstruction - worth stating, because it was written at 00:54 while the live file was
already at `FovLever=130`.

**Restored** by byte copy, verified with `cmp` against both snapshots. Prior state saved as
`dishonored_vr.ini.pre-restore-known-good`.

**`FovLever=130` is untried, not disproved - and it was the well-motivated direction.**
ENGINE_NOTES "FovLever and the render size are ONE setting" has it the other way round from
how session 3c's ordering reads: at lever **100** the clamp limit is 1.91 m against a
frustum reaching 2.20 m horizontally and 2.29 m vertically, so it fires on **all four
sides**, and `dump eyes` at lever 100 confirmed the world inset with a ~9-10% border on
every side. At lever **130** the limit is 3.43 m, outside the frustum edge, so nothing
clamps and the quad fills the eye. 130 is also GingasVR's own tuned value.

**So the restore knowingly reinstates the bordered configuration.** That is the right call -
lever 100 at 2850x2750 is the only point a tester has ever confirmed fuses with depth, and
an unevaluated experiment is not a baseline - but the border is a KNOWN artifact of this
baseline, not a new symptom, and "the render window is halfway up my vision" must be judged
against that. Lever 130 stays queued as the next one-variable change once the gameplay dump
is in hand.

**Caveat that still stands**: no F10 tuning has ever been persisted (SAVE AS DEFAULTS was
never pressed), so this ini is the only reproducible configuration that exists.

**Untouched deliberately**: the proxy, the fork, `dxvk_stereo.txt`, and the stale
`command.txt` in `%LOCALAPPDATA%\DishonoredVR\` (`command.cpp:153` discards and clears a
command file older than the process, so the next run's log should show that branch firing -
a free check of the new seam diagnostics).

**Next**: unchanged from session 3c. Launch, reach GAMEPLAY with the window focused, then
`tools\game-cmd.ps1 "dump eyes"` and read the two PNGs. The unexplained contradiction is
still the lead: the world occupied only the top ~54% of the eye render target while the
measured frustum (55 deg down against 44 deg up) says the quad should overflow vertically.
That dump was a MENU frame and needs confirming in gameplay before anything is changed.

### 2026-09-02 - session 3c: the seam went deaf, and the eye texture is half black

**START HERE.** The bug is not fixed and the last measurement is incomplete.

**The one thing to do first**: launch, get into GAMEPLAY (not a menu, window focused),
then `tools\game-cmd.ps1 "dump eyes"` and look at the two PNGs in
`%LOCALAPPDATA%\DishonoredVR\dumps\`. Everything below is waiting on that image.

**The live symptom** (tester, Quest 3 + VirtualDesktopXR): eyes will not fuse, and the
image sits high - "the render window is halfway up my vision so I only see the bottom
half of it". Earlier in the session, "everything looked tiny".

**The measurement that matters.** A `dump eyes` caught mid-session shows the world
occupying only the **top ~54% of the eye render target, bottom half pure black**, in both
eyes. That is our own D3D11 pass, before the compositor. It was a MENU frame (the dump
caught a paused game), so it needs confirming in gameplay - but the vertical placement is
the lead. The measured frustum is
`eye frustums: L[-1.376 0.839 -1.428 0.966] ex=-0.0316 | R[-0.839 1.376 -1.428 0.966]`,
slots `[left, right, down, up]`: **down-biased, 55 deg down against 44 deg up**. Worked by
hand at these settings the quad should span y -2.36..+1.62 against a frustum of
-2.29..+1.55, i.e. it should OVERFLOW the eye vertically, not sit in the top half. That
contradiction is unexplained and is the next thing to chase.

Same dump showed the two eyes holding **different content** (different horizontal extents,
different fragments of the same menu text). That is the mono/stereo UV race on menu frames
- the fork stops splicing, the frame goes mono, each eye still takes its own half. Probably
menu-only: in gameplay the splice count is 5000+ and the halves are a real stereo pair.

**The command seam went deaf and blocked the session.** `status.json` stale for 18 minutes
and `command.txt` unread, while the Present hook ran at 62 fps with 251 `[present]` lines.
Two `dump eyes` commands did nothing and left no trace. `poll()` had FIVE silent returns,
so the failure was indistinguishable from the command never being written. **Fixed and
installed**: each guard now names itself with the path, the size and GetLastError. If the
seam is still deaf next session the log will say which branch refuses.

**Theories killed this session (do not re-run them):**

- The `CopyResource` size mismatch (step 0a) - eye RTs and XR eye size matched exactly.
- The pace thread as the crash victim - the faulting thread is `(other)`, not `xr-pace`.
- The black left eye - a SIMULATOR defect, not the mod; `dump eyes` shows the left eye
  texture full. See VERIFICATION "Known simulator defects".
- Eye cant - `g_eyeRot` is declared identity and the XR path correctly leaves it alone.
- The clock/rate gate - `MaimNowMs` is fine (the 5 s `depth:` line printed 50 times against
  the 3 s heartbeat's 81). Note the log uses `GetTickCount` while the gates use QPC via
  `dvr::clock`, so an advancing log proves NOTHING about the gate clock.
- A game restart clearing the seam - it did not.

**THE KNOWN-GOOD STATE, AND HOW TO GET BACK TO IT.** This rig's best result so far -
tester: *"the eyes seem to overlap correctly and provide depth, there is no freeze"* - came
from `2850x2750` + `FovLever=100` + `Scale=50`, with the main scene splicing (5202). It is
worth more than GingasVR's own values, which come from a different machine and are the
subject of the project's central open bug ("works only on her PC").

Snapshotted byte-for-byte in two places, so it survives a wiped game folder:

- `tests\golden\known-good-2850x2750-lever100.ini` (in the repo)
- `<game>\Binaries\Win32\dishonored_vr.ini.KNOWN-GOOD`

Restore = copy either over `<game>\Binaries\Win32\dishonored_vr.ini`, then
`tools\setup-game-ini.ps1 -Resolution -Width 2850 -Height 2750` for the game ini and the
four AppCompat buckets. The keys, if you ever need to rebuild it by hand:
`[Screen] RenderWidth=2850 RenderHeight=2750 SpoofDesktopW=2816 SpoofDesktopH=2880`
`PinBackbuffer=1 GameFOVDeg=100 FovLever=100 FillScale=1.00`, `[PosTrack] Scale=50.0`.

**WARNING - the snapshot does NOT contain any F10 tuning, and neither did any run.** The
overlay's sliders are live-only until someone presses **"SAVE AS DEFAULTS"**
(`overlay.cpp`, top of the panel), which calls `OverlaySaveDefaults` and writes ~90 keys -
world scale, fill, screen distance, height offset, menu fill, wrist HUD, and the whole hand
/ graft / blink block. It was never pressed, so every F10 adjustment the tester made across
this session was lost at process exit; the only thing that persisted was the hand
calibration, which `skelcontrol.cpp` writes on its own. **Procedure from now on: tune in
F10, press SAVE AS DEFAULTS, then re-snapshot the ini.** Otherwise a good configuration
cannot be reproduced, which is exactly what happened here.

**Config as left, LIVE right now**: ~~`FovLever=130`~~ **RESTORED to the known-good
snapshot (session 4)**. The live `dishonored_vr.ini` is now byte-identical to both
`tests\golden\known-good-2850x2750-lever100.ini` and the game folder's
`dishonored_vr.ini.KNOWN-GOOD` (`cmp` clean against both), i.e. `FovLever=100`. The
`FovLever=130` experiment was applied but never evaluated - the seam went deaf before a
dump could be taken - so it was discarded rather than judged; it remains untried, not
disproved. The pre-restore state is saved as `dishonored_vr.ini.pre-restore-known-good`.
Game ini + all 4 AppCompat buckets verified still at 2850x2750 (`DishonoredEngine.ini`
lines 1081/1141, `DishonoredCompat.ini` buckets 1-4), so no `setup-game-ini.ps1` re-run
was needed.
Backups in the game folder: `.pre-2750` (GingasVR's own tuned ini), `.pre-landscape`,
`.pre-scale100`, `.pre-gingas-restore`, `.pre-rollback`, `.pre-lever130`,
`.pre-restore-known-good`.

**Installed**: RelWithDebInfo proxy only, 00:50, carrying the seam diagnostics. The fork
(20:24) and `dxvk_stereo.txt` (13:52) are untouched and must stay that way - one variable.
Re-verified session 4: the installed `d3d9.dll` is md5-identical to
`build\src\RelWithDebInfo\d3d9.dll`, and the tree is clean at `112105b7`, so source,
build and install all agree. The fork and `dxvk_stereo.txt` timestamps are unchanged.

**Do not repeat these mistakes.** Restoring GingasVR's baseline I changed render size, FOV
lever and world scale in ONE step, so "misaligned" was unattributable; her values also come
from a different machine, and the project's central open bug is that her build works only
on her PC. This rig now has its own confirmed-good point (2850x2750, splices 5202, tester:
"eyes overlap correctly and provide depth") which is worth more than her numbers. Change
one thing per run, and get the gameplay dump before changing anything at all.

### 2026-09-01 - session 3b: the render was PORTRAIT, so there was no stereo

A headset run mid-session reported "the eyes are suuuuper far off and they both appear to
be zoomed in", worse than before. Its log is the **first surviving headset log** and is
archived outside the game folder. Cause found, fix applied, not yet tested.

**Session 2's resolution fix set the render to 2750x2850, which is portrait, and the DXVK
fork refuses to splice the main scene on a portrait viewport.** `d3d9_device.cpp:4381`
sets the refusal reason `"rt-portrait"`; the per-eye splice at `:4578` runs only when that
reason is `SPLICE`. So the world was drawn **mono** across the full frame while the proxy
handed each eye a different **half** of it - unrelated views that cannot fuse, each
magnified 2x by the stretch onto the quad. Exactly the report.

**The splice counter lies about it.** Light shafts, shadows and the M8.1 quarter light pass
splice under different conditions and kept working, so `splices=85` while the main scene
never spliced once - which kept `g_sbsMonoNow` false and the half-frame UVs on. The fork's
own log shows only effects splices.

**The same gate kills the FOV measurement.** `dxvk_vr_proj`'s publish (`:5996`) is also
gated on `Width > Height`, so `g_liveFovX` was 0 all session, the frustum-fill path fell
back silently to the ini constant `GameFOVDeg=100`, and an assumed number set world scale.

**This falsifies session 2's "the eyes ARE a stereo pair".** 32.7 mean-abs-diff static /
11.5 after a head turn is exactly what two different halves of one mono frame produce. That
test could not distinguish a stereo pair from two unrelated crops, so it could never have
failed its own hypothesis.

**Applied**: `2850x2750` - the same two numbers swapped. Same pixel cost, landscape by
100 px, full-frame aspect 1.036 so the quad subtends 100 x 98 deg at `FovLever=100`.
Changed in `tools/setup-game-ini.ps1` (defaults + a header section on why landscape is
mandatory), applied to `DishonoredEngine.ini` and `DishonoredCompat.ini` via the tool (both
backed up), and to the game folder's `dishonored_vr.ini` (backup `.pre-landscape`).

**Instruments added so this cannot hide again**: a portrait capture logs an Error naming
the fork's own refusal string and the fix (`present.cpp`); the frustum-fill path now says
every 10 s whether world scale comes from the MEASURED render FOV or from the assumed ini
constant (`eye_quads.cpp`).

**Installed**: RelWithDebInfo proxy only (`install.ps1 -Release -SkipDxvk`) - the fork and
`dxvk_stereo.txt` are untouched, so the resolution is the only render-path variable. The
proxy's other changes (session 3 below) are the shutdown/pace lane and logging, which
cannot confound the zoom result.

**Not yet tested.** If the fix worked the portrait Error is absent and the eyes fuse; if
the Error appears, the resolution did not take and AppCompat is overwriting it again.

### 2026-09-01 - session 3: the crash fingerprint was misread, and why

No launches: everything here comes from artifacts already on disk plus the source. The
game is installed on this PC, but nothing was run.

**Two of session 2's conclusions are instrument bugs, not engine facts.**

1. **The exit crash is an EXECUTE fault, not a freed-memory write.**
   `ExceptionInformation[0]` is three-valued (0 read, 1 write, 8 execute/DEP) and the
   fingerprinter tested it for truth, so every execute fault has printed as "writing". The
   records prove it: `ExceptionAddress == ExceptionInformation[1] == 0xDEDEDEDE` with the
   module resolving to `?`. A data write would have left `ExceptionAddress` inside
   `d3d11.dll`. **EIP landed in freed memory: a call through a poisoned code pointer.**
2. **The faulting thread is not the pace thread.** All three records say `(other)`, which
   `thread_name()` returns only for a tid in no registered slot; `present` and `xr-pace`
   both register at entry. The faulter is a third-party worker (d3d11, driver, runtime).
   The pace thread can be the cause, but instrumenting it as the victim will find nothing.

**Two evidence channels were dead and are now fixed.**

- **`dumps\` was empty by construction.** 3 `EXCEPTION` lines, 0 `minidump` lines: proof
  that `unhandled()` never ran, because UE3's own filter/SEH frame consumes the fault
  before `SetUnhandledExceptionFilter` fires. The dump is now taken from the **vectored**
  handler (which always runs), gated on the instruction pointer resolving to no loaded
  module - fatal-only by construction, and falsifiable: an ordinary in-module fault
  produces no dump and disproves the wild-EIP reading. `dbghelp.dll` is resolved at
  `install()` time so the VEH never touches the loader lock.
- **The crash file had no run identity.** `FILE_APPEND_DATA` / `OPEN_ALWAYS` with nothing
  separating runs, and `dvr-xrsim` and VDXR produce byte-identical fingerprint text, so
  the three records cannot be attributed to a backend at all. Now one header per run
  (clock, version, build id, pid, backend + runtime name).
- **Log rotation is one deep**, so two simulator runs erased both headset logs; the
  survivors contain no `EXCEPTION` and no `PreExit`. Copy the log out before each launch.

**The author read this fault correctly and session 2 inverted it.** The 38.79 comments say
"EIP dededede" and "a call through freed memory". 38.79 acted on that by standing the
**game** thread down at `PreExit`, which was right but not the whole path - it left the
pace thread running with nobody waiting for it. Closed below.

**Two pace-lane defects fixed** (steps 0b and 0c):

- **`XR_TIMEOUT_EXPIRED` is a success code.** `XrResult` is negative for failure only, so
  `XR_FAILED()` is false for it and `!XR_FAILED(xrWaitSwapchainImage(...))` ran
  `CopyResource` into an image the compositor had not finished reading - a race with the
  runtime on the one resource the headset displays, invisible because every call returns
  success. Now `== XR_SUCCESS`. In the same block `g_xrpShown` advanced **before** the
  copies, so a frame lost to a timeout was dropped permanently instead of retried; it now
  advances only once both eyes actually received the content.
- **`XrPaceStop()` joins the pace thread**, bounded at 750 ms, replacing the bare
  `g_xrRun = 0`. On expiry the thread is left running on purpose - `TerminateThread` would
  orphan `g_xrCs` and abandon an acquired swapchain image, which is worse than the race -
  and the error line is the instrument: a fault after it means the pace lane is still the
  suspect, a fault without it means the thread was already gone and it is not. The event
  pump's inner `while` now tests `g_xrRun` so an event backlog cannot hold the loop past a
  stop request.

**Changed** (8 files, uncommitted): `src/core/util/crash.cpp` and `crash.h` (three-valued
AV decode; run header; `set_context`; shared `write_dump` with the wild-EIP gate),
`src/core/vr/openxr_backend.cpp` (names the runtime in the crash context),
`src/core/vr/openxr_pace.cpp` (the wait fix, the retry fix, `XrPaceStop`),
`src/game/dishonored/ue3/process_event.cpp` (`PreExit` joins), `src/mod/fwd.h`,
`docs/dishonored/ENGINE_NOTES.md`, `docs/STATUS.md`. Verified: Debug, RelWithDebInfo and
`-Legacy` all build, and both DLLs carry the new strings; `lint.ps1` clean; exports 9/9
undecorated. **Not run in the game, in the simulator, or in a headset.**

**Next**: step 0a - the `CopyResource` size mismatch, which is still only a code-reading
hypothesis. Then the full `xrRequestExitSession` / `xrDestroySession` shutdown.

### 2026-09-02 - session 2: instrumentation, resolution, and a real headset

**First session with the game actually installed and a real Quest + Virtual Desktop
headset on the other end.** Environment: proxy and fork both built from source with
MSVC (meson + ninja + glslang 16.5.0 standalone, no Vulkan SDK); `tools\build-dxvk.ps1`
needed two fixes to run at all on a PC with VS 2026 installed next to VS 2022.

**Fixed and verified**

- **Resolution.** `ResX` in `DishonoredEngine.ini` never held: UE3 AppCompat picks an
  `[AppCompatBucketN]` at startup and writes that bucket's ResX/ResY over
  `[SystemSettings]`. Buckets 3 and 4 ship 1600x900, and any GPU newer than the 2012
  table lands in one, so every modern machine started at 1600x900 forever. Fix: set all
  four buckets; `tools\setup-game-ini.ps1 -Resolution` now does this and defaults to
  2750x2850. Measured before/after: `CreateDevice (1600x900)` -> `CreateDevice
  (2750x2850)`, `capture: 2750x2850` on the FIRST device creation, no setres needed.
- **`setres` is a dead end.** Measured `setres 2750x2850w -> "(empty reply)"` with NO
  device Reset following. New `[Screen] PinBackbuffer=1` (default OFF) sets the size in
  the present parameters at CreateDevice instead. The 32.57 "image in the corner"
  objection is answered by the GetClientRect hook that landed later.
- **World scale.** `W` is pinned to the rendered FOV and `H = W / frameAspect`, so a
  squarer render makes a taller virtual screen than the lenses can show and the player
  sees a magnified middle. 2750x2850 at FovLever=130 subtends 100x132 deg; at 100 it is
  100x102, which matches the headset. FovLever set to 100.
- **The mono/stereo UV race.** `BuildEyeQuads` BAKES the sampling UVs, but the rebuild
  only fired on an aspect change or a menu toggle. `g_sbsMonoNow` flips during gameplay
  whenever the fork's splice count dips, so a mono frame could be sampled with stereo
  UVs and each eye got a different half of one mono image. Now a change in frame kind
  forces a rebuild, exactly like the menu flag.

**Instrumentation added** (see the new "Logging" section in `CLAUDE.md`)

- Full DXGI adapter enumeration, the LUID the runtime asks for, and the adapter read
  back OUT of the finished device with an Error-level mismatch line.
- `RESOLUTION CHANGED MID-SESSION` at Warn, `quad: ... subtends AxB deg` per rebuild
  with a Warn past 110 deg vertical, `res: the game asked for WxH`, and a `skc/gate:`
  line for the hand drive.
- The hands heartbeat now names the OWNER and reports that owner's counter.

**Corrected beliefs** (all three were believed and are wrong)

1. "The hand graft never attaches." It attaches fine: `OWNER=SkelControl writes=~406/3s`
   in gameplay. The old heartbeat tracked two counters that read 0 BY DESIGN - one a
   retired subsystem, one the legacy drive that is deliberately stood down. Three
   readers including the original author concluded "the hands are dead" from a healthy
   run.
2. "The 39.3 adapter bug needs two GPUs." DXGI enumerates the SAME RTX 4070 Ti SUPER
   twice on this PC (virtual display drivers), so the default adapter is not stable on
   single-GPU machines either. **But on the real VDXR run the runtime asked for
   adapter[0], which IS the default, so the LUID mismatch is NOT the cause of the
   symptoms on this rig.** 40.1 still fixes the class of bug and makes it visible.
3. "The eyes are not a stereo pair." Measured 32.7 mean-abs-diff when static, but 11.5
   after a head turn, which is normal parallax. Stereo works; the divergence is a
   symptom of the freeze, not the disease.

**Still broken: the freeze-then-rescale.** Reported again after all of the above. What
is ruled out: the resolution (it now stays 2750x2850), the adapter (matched), the FOV
(100), the mono/stereo UV race (fixed), and the hand drive (working). What is NOT ruled
out and is where to look next:

- The **shutdown crash is a teardown race** and may share a root with the freeze: three
  runs ended with `EXCEPTION 0xc0000005 writing 0xDEDEDEDE` inside `d3d11.dll`, two
  threads at once, immediately after `PreExit` stops the pace thread. 0xDEDEDEDE is
  freed-memory poison. The detached pace thread is touching released D3D11 objects.
- The pace thread owns every runtime call while the game thread owns capture and
  UpdateSubresource on the SAME `g_ctx11`; ID3D11DeviceContext is NOT thread-safe.
  `ID3D10Multithread` is enabled but that protects the device, not a stale pointer.
- Instrument the frame path next: log around the Reset/teardown boundary and around
  every `g_ctx11` use from the pace lane, and get a minidump analysed from
  `%LOCALAPPDATA%\DishonoredVR\dumps`.

### 2026-09-02 - session 1: development framework

Explored the 22,959-line `src/dllmain.cpp` and the BioShock trilogy mod; planned the refactor
with the user (decisions: CMake+MSVC; DXVK restored in-repo and kept as the stereo path; both
backends kept behind one pipeline; retired code to `src/legacy`; a proper logging/debugging
surface). Executed: DXVK restore (52 patch commits + the M8.4 revert; `fork-patches/` removed),
submodules and vendored OpenVR, CMake scaffold and MSVC port (naked stubs, `.def`,
`_ReturnAddress`, `ID3D10Multithread`), the unity split, Phase 2 utilities, harness copy and
adaptation, simulator build + selftest PASS, debug surface, patterns.h, legacy gating, backend
probe, docs. Found: `dxvk_vr_view` is resolved by the proxy but absent from the published
patches (the handoff confirms it is the unshipped p53 commit); the hand-skin `.mtl` path used
`\v` and `\%` escapes so materials never loaded (fixed); 165 em dashes swept. Received the
author's handoff (their build 39.4) at the end of the session: version renumbered to 40.0.0,
the 39.x fixes and the adapter hypothesis folded into ROADMAP, KNOWN_ISSUES, CODE_REVIEW,
ENGINE_NOTES and XR_HANDOFF. Verification: exports 9/9, lint clean, both legacy
configurations build, `split-source.py --check` reports only the intended changes. Branch
pushed.

### 2026-09-13: VR-70 investigation checkpoint

Reconciled the pending Linear stability evidence and created the missing
completed records. Verified current main and branched for VR-70. Reviewed
PR #12 and independently derived final camera cache getters. Added a bounded
read-only boat trace, production build/lint/exports/golden verified; standalone
simulator 60 frames passed after process-local OBS-layer opt-out. Awaiting
the tester-owned boat observation before selecting the head-motion writer.

## Session continuation (2026-09-13): boat camera ownership and rotation candidate

Archived diagnostic build 218-ge5c7653f logs before the next launch. The measured
Soiree animation path bypasses controller rotation while Walk uses it. Added a
guarded draw-scoped cache rotation overlay, coherent stereo eye orientation and
live A/B. Headset behavior remains pending; lean and stick are untested. Details
and remaining acceptance are in CINEMATIC_HEAD_TRACKING.md.

## Session continuation (2026-09-13): remove cinematic recentering on pacing gaps

Build219's32 avoidable reference resets are measured,with20 exact logged
no-present SINGLE correlations. Fixed lifetime and centered single-draw support;
34 host checks cover the regression and restoration. Earlier override before
full-animation ownership is still being measured. See CINEMATIC_HEAD_TRACKING.

## Session continuation (2026-09-13): stable gaze; cinematic pitch and bars

Build220 stable gaze confirmed,including early boat. Added cinematic-only
position publication without gameplay CANCEL neck term; normal gameplay is
unchanged. Native hide-letterbox HUD flag derived after actual game INI/script
search. Pitch acceptance first,then a separate letterbox A/B. Plan and detailed
provenance:CINEMATIC_HEAD_TRACKING.md and ENGINE_NOTES.md.

## Session continuation (2026-09-13): install native border query control

Natural pitch confirmed on221. Implemented default-off HideBorders and installed
A/B after136 x86 checks. No engine HUD fields are written; the existing GFx movie
owns visibility changes. Next question is whether the scene fills former bar
areas. User requested no further subagents this session; active work was stopped
and remaining validation performed locally. No game launch or merge.

## Session closure (2026-09-13): cinematic acceptance

Build222 border removal accepted. PR54 merge explicitly authorized; new state
reading work is VR-103,not a regression fix folded into the approved camera PR.
Detailed acceptance and archived identity:CINEMATIC_HEAD_TRACKING.md.

## Installed combined candidate230

Both changes are enabled in build vr33-hands-working-230-g47c626a5,
compile21:40:15. 30 FOV/handback checks,43 identity lifecycle checks, release
build, lint,9 exports, golden INI and60-frame standalone XR simulator pass.
Full installed INI diff contains only Cine.LockFov=1 and Anim.CinematicHandBack=1;
CRLF preserved. DLL hash matches the build. Prior DLL, full INI and both logs
archived in build/cinematic-fov/install-20260913-214151. Manifest:
build/cinematic-fov/latest-install.json. No game launch; headset test pending.

## Deferred headset queue, 2026-09-13

Testing resumes in the next session. PR56 stays draft and unmerged. Candidate234
is preserved independently under build/playtest-candidates/cinematic-234 with
DLL, exact CRLF INI and manifest. Test this candidate first: tilted Empress scene
and free head look. Mantle remains a separate untested acceptance item.
VR-106 tracks a longstanding standing-only pitched-head roll arc on a child
branch from this branch. It must not be confused with authored cinematic roll.
The agent swaps builds, compares the entire installed INI and reads/archives
both logs; the tester only launches and reports observations. One question per
launch. No game launch or merge is authorized. Both candidates await testing.

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
## Session 2026-09-14: VR-109 handoff correction

Archived verified234 playtest. Accepted comfort/FOV/free look; mono handoff and
movement-heading regressions remain open. Corrected activity accounting and
body-facing ownership in PR56, keeping PR57/58 candidates separate. Tests and
next single-question launch are in CINEMATIC_FOV_AND_HANDS.md. No merge.

## Session 2026-09-14: mono UI ownership and anchoring

Created VR-107/VR-108, reused VR-74/VR-71 and branched from the standing candidate.
Implemented reflected current-owner UI gating and upright configurable mono
placement. Historical build60 logs support the menu clamp/stale input diagnosis;
current game log is232, not a new candidate test. Host camera/UI suites and
standalone60-frame XR smoke pass. Packaging and deferred headset verification
are recorded in MONO_ANCHOR_UI_STATE.md; no merge approval.
