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
