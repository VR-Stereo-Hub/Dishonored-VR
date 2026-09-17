# Controller emulation

## F10 > Controls

Changes apply and save immediately in `[Controllers]`.

| Setting | Default | Meaning |
|---|---|---|
| DpadModifier |1|0 off,1 right thumbrest,2 R3,4 left thumbrest; old3 becomes off |
| DpadFlip |0|0 left stick,1 right stick; F10 flip also moves a thumbrest modifier to the opposite hand; choosing a thumbrest always selects the other stick |
| PauseChord |1|X+Y can substitute for the menu button |

Hold the modifier and move the selecting stick past0.65 along its dominant
axis. The game receives a held D-pad direction, released when centered or the
modifier is released. That stick does not also move/turn. The other stick is
unchanged. Thumbrest contact alone preserves sub-threshold movement, matching
BioShock1's behavior. R3 modifier consumes its selecting stick throughout
the hold. Modifier off restores ordinary sticks.

Tap menu for pause. Modifier plus menu immediately holds the journal button.
Holding menu alone for500ms also opens the journal; a short tap emits Start for
150ms after release. Once journal owns a gesture, releasing the modifier before
menu cannot cause a pause tap. X+Y uses the same menu policy and consumes both
face buttons until both release. A single button pressed before its chord
partner may reach the game first, as in the BioShock source; there is no added
single-button input delay. Turn off the chord to forward both buttons normally.

Y forwards the game's native Y binding, lean/adrenaline in standard layouts.
The game still owns alternate controller layouts. R3 modifier reserves the
existing health-elixir hold. Both grips always retain their gameplay actions.
F10 explains the R3 conflict. Both-stick recenter remains the existing raw XR
chord. Index/other controllers without thumbrest input can choose R3. Vive wands
without face buttons need a reachable menu binding; X+Y does not create absent
hardware buttons. SteamVR binding suggestions and shim manifests are unchanged.

## Derivation and scope (2026-09-17)

Read the local BioShock trilogy `src/core/vr/openxr_input.cpp` s63 DpadMod,
DpadFlip, dominant-axis select and modifier-menu policy, plus its F10
`src/core/input/xinput_bridge.cpp` controls. Adapted the policy in a small pure
composer, keeping Dishonored's own gameplay-specific pad bridge. Do not copy
BioShock pad bit meanings or its ammo pulse behavior into Dishonored.

Local shipped DefaultInput.ini and saved DishonoredInput.ini establish:
Y -> GBA_Lean_Gamepad; Back -> GBA_Journal; Start -> Dis_OpenPauseMenu;
D-pad Up/Down/Left/Right -> shortcut indices0/1/2/3, Select on press and Use on
release. Previously pad_bridge explicitly sent Start for `in.y || in.menu`.
No game config was edited and no game-derived files are committed.

Runtime publishes raw X/Y/menu and both thumbrest actions. Composer owns menu
latches/timers on the present thread, before sprint/health/wheel processing.
Settings publish as one atomic tuple. Inactivity/settings changes clear pending
menu state; old taps do not complete after focus returns. Suppression is applied
before wheel/reading input and at final pad boundary, with room-scale excluded
when the movement stick is consumed. Hand wheel selection remains independent.
Removed unused BioShock composer constants/state from the raw XR source; actual
behavior resides in the tested Dishonored composer.

Validation: tools/controller-emulation-host.ps1 executes221 checks against the
production composer and extracted production face mapping. Covers all modes,
sides, four directions, diagonal selection, held/release behavior, unrelated
stick preservation, menu release orders, chord enable/disable, focus reset,
settings reset, grip hysteresis and native Y. Production INI writer, package and
golden are byte-identical. First test compile lacked initializer_list include;
fixed. First release compile found UINT-to-int list-initialization narrowing in
the config read; added explicit conversion. Neither failed build was installed.
No game/simulator launched. Headset behavior remains pending on this branch.

Installed vr33-hands-working-417-gdc6751a0f from clean sourcedc6751a0f.
Release/9 exports/lint/221 controller checks and default-profile byte parity pass.
Both logs, previous DLL and full INI archived at
build/playtest-candidates/installs/20260917-092418-488098.
Entire installed INI differs only by three new Controllers keys:
DpadModifier=1,DpadFlip=0,PauseChord=1. Removing those lines reproduces
prior INI byte-for-byte; CRLF and installed hashes independently verified.
No game/simulator launch. Controller source remains local, no new PR/merge.

## Cross-hand correction and powers scroll after417 (2026-09-17)

Verified417 log shows both thumbrests present. Left-rest modifier4 was used with
DpadFlip0 (left stick) and detected held without a D-pad direction. One thumb
cannot touch its rest and move that same stick; warning text alone was not
sufficient. Normalize the pairing at config/composer/UI boundaries. Right rest
means left stick, left rest means right stick. Explicit R3 still permits either.
The final installed profile remains right rest/left stick from the latest save.

Grip mode is removed. Numeric3 now means off, not a silently reassigned button;
left rest remains4 so existing choices survive. Both grips pass through intact.

Y remains the native lean/adrenaline button. In active gameplay with no wheel,
menu, cinematic or F10 overlay, Y routes the physical right axes to the game's
left lean axes and zeros right axes so it does not also turn. Left-stick motion
and room-scale synthesis cannot override this final mapping. It takes priority
over D-pad but not over X+Y menu consumption. Native action determines lean vs
adrenaline behavior. The game's response to that axis mapping is not yet headset
verified; no new native field writes, addresses or game configuration edits.

Powers/journal scrolling was blocked by ordinary menu shaping setting RX/RY0.
Restore continuous right Y after that shaping for recognized native menus,
excluding wheel and respecting D-pad right-stick consumption. Horizontal menu
axes remain unchanged to avoid restoring the old double-step behavior. Log
pad/axes includes context, physical right stick and final left/right axes.

196 standalone checks pass, covering supported modifiers, opposite pairing,
legacy grip migration, native face mapping, Y routing priorities and final menu
scroll policy. Count is lower than417 because obsolete grip combinations were
removed. Default writer/package/golden parity passes. Headset test pending.

Installed vr33-hands-working-419-g1f10404e6 from clean source1f10404e6.
Release/9 exports/lint pass;196 controller checks and default-profile parity pass.
Both logs/full prior DLL/INI archived in
build/playtest-candidates/installs/20260917-104459-011878. Entire installed INI
byte-identical to latest save; zero settings changes. DLL/INI hashes and CRLF
independently verified. Headset result pending; source local, no new PR/merge.
