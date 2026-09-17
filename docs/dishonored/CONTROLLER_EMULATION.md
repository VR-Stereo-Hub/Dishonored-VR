# Controller emulation

## F10 > Controls

Changes apply and save immediately in `[Controllers]`.

| Setting | Default | Meaning |
|---|---|---|
| DpadModifier |1|0 off,1 right thumbrest,2 R3,3 left grip,4 left thumbrest |
| DpadFlip |0|0 left stick,1 right stick; F10 flip also moves a thumbrest modifier to the opposite hand |
| PauseChord |1|X+Y can substitute for the menu button |

Hold the modifier and move the selecting stick past0.65 along its dominant
axis. The game receives a held D-pad direction, released when centered or the
modifier is released. That stick does not also move/turn. The other stick is
unchanged. Thumbrest contact alone preserves sub-threshold movement, matching
BioShock1's behavior. R3/grip modifiers consume their selecting stick throughout
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
existing health-elixir hold; left-grip modifier reserves the weapon-wheel grip.
F10 explains these conflicts. Both-stick recenter remains the existing raw XR
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
