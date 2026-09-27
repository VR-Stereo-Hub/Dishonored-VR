# Next session: controller bind remapping

Read CLAUDE.md, the newest three sections of docs/STATUS.md, this brief, and `docs/dishonored/F10_AUDIT.md`
(how F10 tiers and saves work). Branch `claude/controller-remap` (off staging after #143; this brief is its
first commit). No Linear ticket could be created (the workspace hit its free issue limit): ask the maintainer
to free a slot or name an existing ticket before the PR, or open the PR with `Ref` to the nearest one.

## The goal

Let the player choose which physical VR controller input performs each game action, from the F10 panel and
the ini, live, with a reset to the shipped layout. Today every binding is hard-wired in code.

## How input flows today (read these, do not re-derive)

1. **The runtime layer** (`src/core/vr/openxr_input.cpp`, ~700 lines) owns the OpenXR action set and the
   suggested bindings per interaction profile (Touch / Quest, Index, Vive, WMR, simple). It publishes one
   `dvr::vr::InputSnapshot` per poll (`src/core/vr/input_snapshot.h`): `mv[2]` left stick, `lk[2]` right
   stick, `trigL/trigR`, `gripL/gripR`, `a b x y`, `clkL clkR` (stick clicks), `menu` (left menu button),
   `restL restR` (capacitive thumbrests), `active`. Keep this layer physical: the remap belongs above it.
   The SteamVR shim (`dvr_steamvr32.dll`) feeds the same snapshot; SteamVR's own binding UI can also move
   buttons there and always wins (the trilogy mod's release notes say so) - note it in the F10 tip.
2. **The composer** (`src/core/input/controller_emulation.h`, `dvr::controller::Composer`): the D-pad
   modifier (right thumbrest / R3 / left thumbrest / off, `[Controllers] DpadModifier`), D-pad on the left
   or right stick (`DpadFlip`), the X+Y pause chord (`PauseChord`), menu tap = pause (Start), menu hold or
   modifier+menu = journal (Back), Y + opposite stick = native lean.
3. **The pad bridge** (`src/core/input/pad_bridge.cpp`, `UpdateVirtualPad`, ~line 87) turns the snapshot into
   the XInput state the game reads through the proxy's `XInputGetState` hook. The hard-wired layout:

   | Physical | Game (XInput) | Notes |
   |---|---|---|
   | left grip (0.9 on / 0.7 off) | LB: power wheel | also `g_wheelHeld`, which gates aiming and crouch pulses |
   | right grip (same hysteresis) | RB: choke / block | the motion sword (`dvr::swing`) also drives RB |
   | A | A: jump | |
   | B | B: stealth (crouch toggle) | slide assist rewrites B at full run; physical crouch PULSES B |
   | X | X: interact | |
   | Y | Y: lean / adrenaline | Y + opposite stick = lean axes (composer) |
   | left stick click | L3: sprint | `SprintBit` |
   | right stick click | health elixir hold | `HealthElixirTick`, not a pad bit |
   | triggers | LT / RT | attack / power; carry and swing logic read them |
   | both stick clicks | recenter (hold) / F10 panel (tap) | runtime-layer chords, not game input |
   | menu | Start / Back (tap / hold) | composer |

   Several mod systems key on the PHYSICAL input, not the game action: physical crouch (B pulses),
   slide assist (B at full run), the swing detector (RB), carry/throw (triggers), the wheel (left grip),
   the health hold (right click), the recenter and panel chords. A remap must move these with their
   action, or they will fire on the wrong button.
4. **F10**: Controls tab, Basic section "Controller layout" (`src/core/ui/overlay_tabs.inc`, ~line 506):
   D-pad modifier combo, flip, pause chord, and a text line describing the menu button. Ini `[Controllers]`
   `Enabled Haptics Deadzone DpadModifier DpadFlip PauseChord` (config.cpp ~2059, saved ~3770).
5. **The game's own binds** live in `DishonoredInput.ini` (`docs/dishonored/GAME_CONFIG_MAP.md`). Remapping
   there is NOT the route: the game rewrites the file at exit (the BioShock mod measured the same), and the
   mod's systems above need to know the mapping anyway.

## Prior art

The BioShock trilogy mod changed its fixed layout once (A jump, B use, X reload, Y heal) but never shipped
user remapping (`C:\dev\bioshock-trilogy-vr\docs\STATUS.md` ~8480, "remap build"; rebinds parked to M9).
BRVR's `docs/modules/input.md` covers its bindings per profile and notes that users without a numpad could
not rebind anything. So the design is ours; port nothing blind.

## Suggested design (argue with it before building)

- A logical ACTION layer between the snapshot and the XInput composition: `Jump, Stealth, Interact, Lean,
  PowerWheel, Choke, Sprint, Health, Attack (RT), Power (LT), Pause/Journal, DpadModifier` - each bound to
  one physical SOURCE (a button, a grip, a trigger, a stick click, a thumbrest) or `none`.
- Every mod system that reads a physical input today reads the action instead (the list in 3). One place
  resolves `action -> source -> value`, with the grip/trigger hysteresis kept per source.
- Ini `[ControllerBinds] Jump=A Stealth=B ...`; a missing section = today's layout exactly (no config bump,
  no rewrite of anyone's ini). Default OFF in the sense that nothing changes until a bind is edited.
- F10 Controls: a "Button mapping" section (Basic: the everyday actions; Advanced: the rest) with, per
  action, a combo of sources AND a "press a button" capture driven from the controllers (the F10 motion
  controls already route controller input to the panel: `docs/dishonored/F10_MOTION_CONTROLS.md`), conflict
  warning (two actions on one source), "Reset to default layout", save on change.
- Left-handed swap (mirror left/right sources) as one checkbox is the cheapest big win; ask whether wanted.
- Log every bind at startup and every change (`input: bind Jump = A (was ...)`) so a tester's log shows the
  layout that was live.

## Verification

- The simulator presses every button (`tools\xrsim-cmd.ps1`, e.g. `press a`, `grip r 1.0`; the catalog is
  `docs/VERIFICATION.md`); the seam's `vrinput status` prints the virtual pad; assert XInput bits per action
  before and after a remap. Build a small `tools\xrsim\remap.xrs` sequence that checks the default layout
  bit for bit, then a remapped one, so the default can never drift silently.
- Headset: the maintainer checks the everyday actions and one remap, plus physical crouch and the sword.

## Rules for this work

- Default layout byte-for-byte identical to today when no bind is set (the regression that matters).
- Motion controls for crouching and hands must never stop working (the original author's rule).
- Every change in one branch; commits 2-10; PR against staging; never merge without the maintainer saying so.
- Simulator launches allowed for questions the simulator can answer; restore the ini after each run; the
  `dlss`-style seam words write the ini, so back it up first.

## Installed state at handoff

Build from `claude/fsr-upscaler` (#144, FSR 4.1.1 / 3.1.5 beside DLSS, not merged), the maintainer's own INI
(DLSS DLAA on). Staging has #143 (DLSS in F10 Basic, MLAA off with DLSS, the uncap verdict).

## Copyable starting prompt

Continue in C:\dev\Dishonored-VR\build\worktrees\fsr on branch claude/controller-remap. Read CLAUDE.md, the
newest three sections of docs/STATUS.md, docs/dishonored/NEXT_SESSION.md and docs/dishonored/F10_AUDIT.md.
Goal: controller bind remapping - let the player choose which VR controller input performs each game action,
from F10 (with press-to-bind from the controllers, conflict warnings and a reset) and the ini, live. Start by
reading the input path the brief names (openxr_input.cpp, input_snapshot.h, controller_emulation.h,
pad_bridge.cpp, the F10 Controller layout section) and list every place a mod system reads a physical input;
propose the action layer before writing it. The default layout must stay bit-for-bit identical when nothing
is remapped (prove it in the simulator). Simulator launches are allowed; restore the ini after each run.
Open a PR against staging; do not merge.
