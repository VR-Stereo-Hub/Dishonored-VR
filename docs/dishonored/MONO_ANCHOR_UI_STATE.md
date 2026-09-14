## Current state: movie completion build254 installed

Installed254-g43551c09 (Sep14 08:50:11), full INI unchanged, hashes/CRLF verified.
Both logs archived at build/playtest-candidates/installs/20260914-085129-389882.
Build252 still failed: overlay-enabled state remains set during gameplay.
254 observes the manual-reset completion event used by Engine.WaitMovie.
31 anchor/loading/completion checks and17 handoff checks pass; release,
9 exports, lint and golden INI pass. Main-menu anchoring is preserved.
Next test: wait5 seconds at loading Continue, dismiss, expect mono to become
stereo only after dismissal. Logs distinguish overlayEnabled and finished.
Never launch game, no subagents, all three PRs remain unmerged.

## Current candidate: loading presentation release252

Installed vr33-hands-working-252-g2d62aca2, Sep14 08:35:52. Verified DLL hash
543d53fb56d6d413855ed7b84b852687c98b6eaa5222b9c6a975fd6f542c58dd.
Full installed INI unchanged (empty settings and text diff), CRLF verified.
Both logs archived at build/playtest-candidates/installs/20260914-083643-968595.
24 anchor/loading checks and17 cinematic handoff checks pass, release build,
9 exports, lint and INI golden pass. Runtime anchoring itself is unchanged;
prior standalone XR smoke remains applicable. No game launch by the agent.

Build250 confirms main-menu anchoring but fails gameplay stereo release.
The persistent native movie service was incorrectly treated as visible loading.
Build252 uses the byte-verified native presentation field and logs ui/loading
inputs every2 seconds. Detailed derivation and failure evidence are below.

Next launch question: load a save, wait at Continue for5 seconds, then continue;
does the loading panel stay mono until dismissed and then switch to full stereo
in gameplay? Early stereo means the active-state policy releases too soon;
remaining mono means another state still blocks. Agent reads/archives logs.
PR58 stays draft; all three PRs remain unmerged. No subagents.

## Installed final-branch candidate, 2026-09-14

Installed vr33-hands-working-250-g78abb4fa, compiled Sep14 08:17:32, source78abb4fa.
Includes accepted PR56 and PR57; both remain ready and unmerged. PR58 is draft.
DLL SHA256 661340a3f49de8330db4fa3fc1a8cf79b4aaa6f550d930027e72776862b641f5.
INI SHA256 2128528963d3de52ee04fbcd75588d581f2f0f568b5b367c728cb128b8bbcb65.
Complete INI diff: only the12 mono-anchor/category/UI-guard keys added at1.
Accepted cinematic, head-based movement and standing-roll settings preserved.
Both logs and prior DLL/INI archived at
build/playtest-candidates/installs/20260914-081856-236038. CRLF/hashes verified.

Passed19 anchor/loading,12 standing,15 facing,17 handoff,39 cinematic math,
13 camera-scope and38 FOV/handback checks; release,9 exports, lint, INI golden.
Standalone XR smoke:60 frames, FOCUSED,0 errors. No game launch by the agent.

One question for the next launch: after20 seconds on the main menu, looking
around and navigating with the stick, does the menu remain in one fixed mono
panel with working navigation? Expected: panel stays in place as the head moves,
no stereo/underground jump, and menu selections still work. Failure needs the
ui/surface and anchor records. Loading/Continue gets its own later test.
VR-87 height ceiling and VR-109 character-oriented drift remain open separately.

## Updated cumulative candidate, 2026-09-14

PR56 and PR57 are headset-accepted and ready for review, still unmerged.
This branch now includes standing parent b303c5f4, including head-based movement
and the accepted cinematic mono handoff. UI blocking does not refresh cinematic
activity. Pad routing also respects current UI ownership for menu stepping,
room-scale input, cinematic parking and stale-cursor mouse nudges.
The latter must not steal focus from a live menu while its old event flag is off.
First test remains the20-second anchored main-menu/navigation test below.
Loading/Continue is a separate later launch. Build240 is superseded.

# Mono anchoring and current UI ownership

## Scope and status

2026-09-14. VR-107 anchors mono presentation; VR-108 covers loading-screen
stereo leakage. Existing VR-74 covers the underground main-menu camera and
VR-71 covers stale-menu input recovery. All remain In Progress, awaiting
headset testing. Branch codex/vr-107-mono-anchors-and-ui-state starts at
d27fdf6c on codex/vr-106-standing-pitch-roll-arc; it includes both earlier
untested candidates. No merge authorized.

## Observed failure and evidence

Reported: main menu changes to stereo after several seconds, camera falls far
below the menu scene, and controller navigation can stop. Loading screens
briefly switch to stereo before Continue. Mono panels currently follow the head.
The flicker-reference surface is the whole view crossing mono/projection, not
one-eye capture starvation or weapon ghosting.

Historical local boat-run-2026-09-10-1929.log identifies build60-g417bfad9,
2026-09-10 17:25:41. It records a -4153.935 uu clamp-rebase offset and a stale
menu flag cleared after1503ms with a live view dispatch. This supports the
background-pawn/clamp and menu-input hypotheses; it does not prove the new fix.
The current game log identifies build232-g4ec4f457, 2026-09-13 22:10:59.
No log from builds234,237 or this new candidate exists yet.

## Script-derived ownership

Decompiled declarations, not native function bodies, name the following route:
DishonoredEngine -> Engine.GamePlayers[0] -> Player.Actor -> Actor.WorldInfo ->
WorldInfo.Game -> DishonoredGameInfo.m_pGlobalUIManager. Its movie holders
identify the current main menu, pause, note, journal, wheel, store, mission
stats and optional DLC screens. Notes use m_bNoteVisible and the wheel uses
m_bWheelIsOpen; HUD cinematic/choice/skip overlays are excluded.
An open main-menu movie uses m_Screen (None0, Title1, Async2, Main3); no pawn or
camera-upload activity is evidence that this screen has closed.

DishonoredEngine.m_pBinkOverlayManager exposes m_bLoadingStarted,
m_bShowMapNameAndHints and the opaque m_pBinkMovie. Save/load mode2 and engine
Loading/Connecting/Precaching prime a loading lease. Loading-start, or a live
movie with map/hints presentation, can also acquire it, including a completed
load first observed at Continue. A live movie retains the lease after the load
mode clears. An unreadable state cannot release it. A save notification alone
does not acquire it. The native lifetime and flags must still be verified in-game.
No new offsets or engine writes: all fields resolve by declaring class/name.

The engine root retains a full FName/class/GObjects-slot identity. Descendants
are read afresh from current owners, never trusted solely because an address
matches. Discovery scans at most1024 slots per16ms script tick, independent of
motion hands. Reads poll at50ms; live membership refreshes at most once a second.
Unknown roots/layouts hold mono and suppress gameplay input, with a live kill
switch, rather than timing out into background gameplay. Missing layouts retry
on a bounded5s cadence and report unavailable. This conservative refusal can
hold mono if a root or property differs on a build; logs must decide that case.

The shared guard vetoes scene re-entry, projection, strict gameplay input,
head-mouse, cinematic camera scopes and pawn-height clamping. It prevents the
stale-menu/cursor heuristics from clearing an observed screen. Runtime veto is
independent of cinematic hysteresis. If texture delivery pauses, a held
projection subimage is presented as a mono quad while UI owns presentation.

## Controls

Both new levers ship off; the candidate enables them together.

| Key | Default | Meaning / live control |
|---|---|---|
| Screen.AnchorMono | 0 | Seed an upright LOCAL-space panel at the current head position plus DistanceMeters forward; runtime View checkbox, monoanchor on/off |
| Screen.AnchorOther/MainMenu/Loading/Pause/Note/Journal/Wheel/Store/MissionStats/Cinematic | 1 each | Independent anchoring exceptions; 0 follows the head. Runtime View category checkboxes |
| Menu.SurfaceGuard | 0 | Current UI ownership veto; F10 camera control, uiguard on/off |

Runtime View has Recenter mono screen; monoanchor recenter does the same.
The agent can invoke the command seam; the tester need not run commands.
Config Save persists master/category/guard switches. Existing distance/width
settings size the panel. Recenter applies a changed distance to an active anchor.
A contiguous mono interval keeps one anchor across menu/category transitions;
returning to actual stereo resets it. Session/reference-space resets also
invalidate it. Invalid/untracked poses cannot seed an origin guess; the screen
follows the head until a valid horizontal heading is available. Category
exceptions affect placement only, never stereo eligibility. Category detection
runs even when the guard is off and anchoring alone is enabled.

## Validation and candidate queue

Host tests cover anchor yaw/position/invalid poses and loading-start/Continue/
unknown/save-notification lease transitions. Existing cinematic geometry/scope,
FOV/handback and standing-arc suites remain required. Full x86 release build,
exports, lint, default-INI comparison and standalone XR simulator smoke are
required before the candidate is offered. None establishes perceptual acceptance.

Keep build234 installed for the first deferred cinematic test. Preserve build237
for the next standing-arc test. This cumulative UI candidate comes third; the
agent installs each with tools/install-playtest-candidate.py, verifies hashes,
archives current and previous logs, checks the entire INI diff and CRLF, and
matches the subsequent log banner/compile time before interpretation.

For this candidate, launch1 asks only: after20 seconds on the main menu while
looking around and navigating with the stick, does the menu stay on one anchored
mono panel with working navigation? Pass supports main-menu ownership, anchor
stability and input routing; drift, stereo, underground movement or lost input
requires the ui/surface and xr anchor records before a new build.

A later launch asks only: when loading a save and waiting at Continue, does the
screen remain anchored mono until Continue is selected, then enter stereo?
Pass supports the movie-lifetime lease; an early projection or a persistent mono
screen after Continue falsifies it. Do not combine questions or request another
launch before archiving both logs. Save notifications and pause/resume are later
negative controls, one question per launch. No game launch by the agent.

## Preserved build240 and completed checks

Clean source c18a67777e1ae3c7979f9e195864054765b32678 produced
vr33-hands-working-240-gc18a6777 at00:06:55 on2026-09-14.
Bundle: build/playtest-candidates/mono-ui-240.
DLL SHA256: 62f3fa1c8b4b9a9190c72bc8cdba1a4cfc9a991d4d89de63fdc006e2eb2e4b8b.
INI SHA256: 6b8b3523dcf1765561bdc8f4d159e45ee205bdb42845b8e356b071b01670f1ba.

Passed:19 anchor/loading checks,39 cinematic math/ownership checks,
13 scoped-write checks,38 FOV/handback checks,12 standing positional checks,
x86 release build,9 proxy exports, lint, generated INI golden comparison.
Standalone xr_hello32 simulator ran60 frames, reached FOCUSED,0 errors.
The simulator smoke does not exercise Dishonored's native UI ownership.
The existing DVR_CAT macro redefinition warning remains unchanged.

Installed and hash-verified240, then restored/hash-verified234 for the first test.
Full before/after settings and text diffs plus both logs are archived under
build/playtest-candidates/installs/20260914-000734-181940 (240 install) and
20260914-000747-348380 (234 restore). CRLF verified both times.
Relative to234,240 adds the twelve anchor/guard keys plus inherited
Neck.UprightPitchArc=1 and PosTrack.ZAccount=1; no unrelated setting changed.
Relative to237, only the twelve anchor/guard keys were added.
No game launch and no headset acceptance. All tickets remain open.

## 2026-09-14: VR-108 persistent movie service is not presentation

Verified build250-g78abb4fa (Sep14 08:17:32), matching installed DLL hash.
Logs/INI archived at build/mono-ui-test/playtest-20260914-082607. Main-menu
anchoring/navigation is reported successful, but gameplay remains mono while
weapons track. ui/surface enters Loading at5282609 and never releases before
exit5313921. This falsifies the service-pointer lifetime lease.

Decompiled DisBinkOverlayManager declares m_pBinkMovie as native Pointer.
Verified ue3-natives derives metadata01350cf0, constructor00bb5000 and vtable
0115d130. Native initialization00b9e4ee copies global movie service0145be80
into the overlay movie field and registers the overlay through service slot24.
It is a persistent service, not an active movie allocation. Draw gate00bb9880
calls service slot1c and draws overlay text only for nonzero result.
Global initialization009dd3ef selects0093d470 (real service) or004ebb60 (null).
Real constructor0093d130 sets vtable010a4610; slot1c is00932cc0, a pure
32-bit read at service+130 then return (8b8130010000c3). Null vtable00fcde60
slot1c is00722980, returning0 (33c0c3). Real active field is initialized0,
set1 at00932c57/009337ad and cleared at00932c31/00935ea4.

Candidate reads that presentation field without calling native code. Requires
expected vtable, exact getter address/bytes, readable field and boolean range;
revalidates current live overlay pointer and service vtable after reading.
The service is not a UObject; no IsLiveObject claim is made for that allocation.
Overlay/engine reads retain current UObject liveness checks. No engine writes.
All new native addresses/offsets are centralized in patterns.h.

Loading mode/transition can acquire before presentation starts. Continue keeps
the lease while presentation is active. Idle service releases despite stale
started/hints metadata. Unknown layout does not release. Periodic ui/loading
prints each input, service pointer and presentation state, including failure.
24 host anchor/lease cases pass; native timing and headset acceptance pending.

## 2026-09-14: build252 rejected; follow script WaitMovie completion

Verified252-g2d62aca2, Sep14 08:35:52; matching installed DLL. Both logs/INI
archived at build/mono-ui-test/playtest-20260914-084045. Main menu starts with
service field+130 already1; after loading it remains1 through gameplay and
pause/unpause. At6156625 mode0 transition0 started0 hints1 yet lease1.
The claim that+130 proves visible presentation is retracted. It is overlay
selection/enabling state and remains set beyond movie playback. Do not reuse.

Additional decompiled declarations: Engine.WaitMovie, StopMovie with delayed
stop until game rendered; GamePlayerController.ShowLoadingMovie,
KeepPlayingLoadingMovie and ClientStopMovie. Native Engine.WaitMovie005e2600
calls service slot34 ->004dbc30, which waits on service+1c event via event slot14.
The service's movie-status queries004eb190/004eb2a0 also sample that event with
zero timeout and return complementary finished/unfinished results. This is a
completion signal, not hand tracking or an arbitrary button press.

Constructor00500cfe creates the event with manual-reset1 and initially false,
stores at+1c, then signals at00500d62 for initial idle. Factory00420500 creates
event vtable00fb98a8, Win32 HANDLE at+4. Its slot14 ->00416670 invokes imported
WaitForSingleObject with the caller timeout. Factory initialization uses
CreateEventW at IAT00f941d8; SetEvent/ResetEvent are00f941dc/00f941e0.
Code/slot/create bytes are verified before reading the current event handle.
A SYNCHRONIZE-only duplicate is observed at zero timeout then closed. Manual
reset means observing cannot consume completion. Current owner/service/event/
handle identity is checked; unknown/failed reads retain mono. No engine writes,
no native function calls, and no retaining handles across polls.

31 host checks include actual manual-reset event pending, completion, repeated
observation without consumption, second-load reset and invalid handle refusal.
The prior overlay-enabled value remains diagnostic only. Movie completion now
controls the existing loading lease. Headset timing is still unconfirmed.

## 2026-09-14: build254 loading accepted; three follow-ups

Verified254-g43551c09, compiled08:50:11, installed DLL hash matched. Both logs
and INI archived at build/mono-ui-test/playtest-20260914-085908. Loading now
releases into stereo. Mantle and block-counter native hand positioning are
headset-confirmed. Two aerial attempts produced ordinary slashes, not a
finisher with misplaced hands; VR-111 tracks native drop eligibility.

The initial load released UI at6962359. Stereo became eligible6963890, fell
back6966234, recovered6967296. During the fallback pawn/FSM remained valid,
menu0 and sceneFresh1/c5age0, but view0. Successful head writes had stopped;
the log includes rejected DeltaTime0.4. The legacy view heuristic, not the
movie lease, caused this approximately1.06s mono interruption. The candidate
allows current scene uploads to cover that loss only with the UI guard enabled
and clear, a live pawn, and a fresh Walk state. Menu/unknown UI still veto;
input permissions remain separate. Thirty host policy checks pass.

VR-111 diagnostic: Anim.DropWatch=1 reads the live player-owned native
DisItemContext_DropAssassinate decision, status, target liveness, cache tick,
and pawn velocity. Discovery is bounded; no engine writes or forced attacks.
No takedown fix is claimed. Next launch question: does one aerial takedown
still become an ordinary slash? The log will distinguish no-drop, too-high,
and do-now native decisions. User launches; agent reads and archives logs.

Whole-view both-eye flicker after crouched note closes remains VR-99. Existing
LateTagRepair was on; see FLICKER_REFERENCE for counters. No speculative
pairing change in this candidate. PR56/57/58 remain unmerged.

Installed candidate: vr33-hands-working-256-gd98bcf36, compile09:15:20. DLL SHA256
828037f579b7e8d6da7ae2f5c479238eb882e2391ea8f430c0a732d3604b0883. Full INI adds only DropWatch=1;
CRLF and hashes verified. Release/exports/lint/golden checks pass.

## 2026-09-14: build256 accepted transitions; crossbow loss VR-112

Verified256-gd98bcf36 compiled09:15:20, installed DLL SHA256
828037f579b7e8d6da7ae2f5c479238eb882e2391ea8f430c0a732d3604b0883.
Both logs and INI archived build/mono-ui-test/playtest-20260914-093057.
Tester reports mono transitions and takedown animations successful. A native
drop is measured at8732781: context status2/type2, live target, masterAssassinate
and GAME ownership; status3/Walk at8734687. The prior trigger failure did not
reproduce there. No combat eligibility change was made in256, so do not claim
the diagnostic fixed it. Crouched note flicker occurred briefly and recovered
quickly, still not completely eliminated.

After boat/cinematic travel and a level load to the hub, drawing previously
sheathed weapons tracks the sword only. Pause/resume does not repair it.
Current crossbow component is discovered from equipped inventory and readable.
Sword contract accepted9058765. Late run has one contract (sword) and no
crossbow contract. At9094687 crossbow nearest prediction differs0.0969deg,
1.2489uu and0.04685 relative scale with a2.1ms snapshot; rejection threshold
is0.005 scale. Repeated windows show the same approximately4.7 percent scale
error. This is not the old absent-candidate failure and not evidence of a
left controller pose failure. Route: FLICKER_REFERENCE section1 weapon
reattachment after load, not stereo eye flicker.

VR-112 plan: AttachScaleTrace (default0, read-only) captures near crossbow and
sword draw, predicted, native component, bridge, native reference and hand-draw
matrices/norms with frame, snapshot generation and component identities.
Bounded at one six-line group per hand per2s, snapshots only on the render lane.
No matching thresholds, ownership or engine memory writes changed. Compare
healthy versus failed groups to distinguish uniform scale, projection-dependent
axis scaling, and wrong/stale reference. Counterprediction: if matrices match
within the original scale tolerance at failure, scale is not the cause and
inspect the other matching/ambiguity guards. Do not widen scale tolerance or
reuse pre-load GPU/object identity to hide the failure.

Next launch: repeat the travel to the hub, draw crossbow, and check whether it
follows the left controller. Failure provides the transform breakdown; success
establishes an intermittent result rather than a diagnostic-induced fix.
One question per launch; agent installs and reads logs, never launches game.
All three PRs remain unmerged. Current candidate is diagnostic for VR-112.

Installed258-g4a78a745, compile09:37:23. DLL SHA256
b59b0c594f19f0309289621cf4eb77628954ed63fc29ec22b763e55b56fc5025.
Full INI diff adds Hands.AttachScaleTrace=1 only; hashes/CRLF verified.
Release,9 exports,lint and golden INI pass; matching code remains unchanged.
