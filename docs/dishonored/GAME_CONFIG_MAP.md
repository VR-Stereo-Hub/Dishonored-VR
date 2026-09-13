# The game's own config folder: what is in it and what it is good for

`%USERPROFILE%\Documents\My Games\Dishonored\DishonoredGame\Config\`

Surveyed 2026-09-12. Twenty-one `.ini` files the engine writes on first run, plus
whatever `.dvr-backup` copies our tools have left beside them. This is a ROUTING
MAP, not a copy: it says which file governs what and names the keys worth
knowing, so a question like "is there already a setting for this" costs a grep
rather than a session. **Values here are the engine's own defaults as shipped;
none of these files are ours and none of them are in the repo.**

The mod's own settings are a different file entirely - `dishonored_vr.ini` next
to the exe. Nothing in this folder is read by the mod.

## Read this first

**The game REWRITES these whenever its own video/options menus are used.** Any
value here is a value that can drift back. `tools\setup-game-ini.ps1 -VRBaseline`
applies the four the mod depends on and is safe to re-run; everything else in
here is reference, not something we maintain.

**They are CRLF.** Editing them with a `^...$` regex in Python silently eats the
`\r` and leaves lone-LF corruption. Split on `\r\n`, edit by index, assert the
old value, verify the line count and a lone-LF count of zero afterwards. The
PowerShell route (`ReadAllLines` / `WriteAllLines`) preserves CRLF on its own.

**The same key exists in more than one section.** `DepthOfField` and `Fullscreen`
are in both `[SystemSettings]` and `[SystemSettingsMobile]`; `UseVsync` is in
those plus all four `[AppCompatBucketN]` sections. A file-wide match edits the
wrong line and reports success. Always match section-first.

## The files

| File | Sections / keys | What it governs |
|---|---|---|
| `DishonoredEngine.ini` | 72 / 1404 | **The big one.** Renderer, `[SystemSettings]`, frame smoothing, vsync, post effects, log suppression, startup packages |
| `DishonoredInput.ini` | 12 / 881 | Every binding, `[Engine.PlayerInput]`, the console, the autocomplete list of debug commands |
| `DishonoredCompat.ini` | 10 / 536 | Per-GPU-vendor and four `[AppCompatBucketN]` quality presets. The buckets each carry their own `UseVsync` and resolution |
| `DishonoredWeapon.ini` | 70 / 364 | Damage types, melee context, per-weapon tuning |
| `DishonoredUI.ini` | 20 / 347 | Scaleform movie players: HUD, main menu, journal, note, power wheel. **The crosshair table lives here** |
| `DishonoredStat.ini` | 30 / 349 | Stat collection groups. Profiling only |
| `DishonoredGame.ini` | 22 / 306 | `[Engine.PlayerController]`, `[Engine.HUD]`, debug draw flags, game info |
| `DishonoredConversation.ini` | 7 / 251 | Dialogue system and its commandlets |
| `DishonoredLightmass.ini` | 18 / 188 | Offline lighting build. Editor-only, irrelevant at runtime |
| `DishonoredNPC.ini` | 36 / 108 | NPC pawn, controller and every NPC state |
| `DishonoredCamera.ini` | 21 / 75 | **Camera modifiers**: lean, dodge, shake, recoil, bump smoothing, arm follow/offset disables, default FOV |
| `DishonoredAI.ini` | 5 / 68 | Global AI, noise, search crumbs, combat manager |
| `DishonoredPlayerState.ini` | 18 / 60 | Player states: climb, fall, swim, mantle, lean, melee, jump. Per-state FOV and post-process |
| `DishonoredPower.ini` | 9 / 45 | Blink, Bend Time, Wind Blast, the soul components |
| `DishonoredPlayer.ini` | 6 / 34 | Player pawn, player controller, powers component, threat perception |
| `DishonoredContactSystem.ini` | 17 / 31 | Material impact types (stone, wood, glass, ...) |
| `DishonoredDLC07.ini` | 4 / 11 | Brigmore Witches game info |
| `DishonoredDLC06.ini` | 2 / 9 | Knife of Dunwall game info |
| `DishonoredItem.ini` | 3 / 8 | Movable component, inventory |
| `DishonoredPawn.ini` | 2 / 4 | Base pawn |
| `DishonoredLevel.ini`, `DishonoredAILocomotion.ini`, `DishonoredWeaponRanged.ini` | 2 each | One or two keys each: alarm manager, foot placement, pistol |

## Where to look for a thing

| Question | File and key |
|---|---|
| Frame pacing fighting the headset | `Engine`: `bSmoothFrameRate`, `MinSmoothedFrameRate`, `MaxSmoothedFrameRate` |
| Vsync | `Engine` `[SystemSettings] UseVsync`, AND all four `Compat` buckets. The mod overrides both with `[Perf] ForceNoVSync` |
| A post effect ruining stereo | `Engine` `[SystemSettings]`: `DepthOfField`, `MotionBlur`, `MotionBlurPause` |
| Default field of view | `Camera` `m_fDefaultFOV=75`, `m_fDefaultFOVBlendSpeed`; `Engine` `FOVAngle=90`, `GameFOVAngle=75` |
| FOV changing under the player | `PlayerState`: `m_fCrouchedFOV`, `m_fPowerJumpFOV` and their blend speeds |
| Camera doing something we did not ask for | `Camera`: the `DishonoredCamera_*` modifier sections - `Lean`, `Dodge`, `Shake`, `Recoil`, `HitReact`, `PhysicalReact`, `BumpSmoother`, and the `DisableArmFollow` / `DisableArmOffset` pair |
| Mouse smoothing lagging head tracking | `Input` `[Engine.PlayerInput] bEnableMouseSmoothing` |
| How far the player can interact | `Game` `[Engine.PlayerController] InteractDistance=512` |
| The crosshair | `UI` `[DishonoredGame.DisGFxMoviePlayerHUD] m_CrosshairSettings[N]`, one row per weapon/power, each with `m_Name` and `m_bShowDot` |
| Whether the HUD draws at all | `Game` `[Engine.HUD] bShowHUD`, `HudCanvasScale` |
| Blink tuning | `Power` `[DishonoredGame.DishonoredActivePowerComponent_Blink]` |
| Gravity, physics substeps | `Game` `[Engine.WorldInfo] DefaultGravityZ=-1500`, `MaxPhysicsSubsteps` |

## Free instruments

The game ships debug facilities that cost nothing to turn on and can answer
questions a capture would otherwise be needed for.

| Instrument | Where | What it shows |
|---|---|---|
| `m_bDrawInteractableDebugBox` | `Game`, `[DishonoredGame.DishonoredHUD]`, default False | Draws a box on what the game considers interactable. A `config bool`, so the ini sets it |
| `ToggleUsableHighlight` | console | Highlights usable objects. Confirmed present as a native cheat; it flips bit `0x400` of the cheat manager's flag word |
| `ShowDebug Interaction` | console | The interaction debug page. `DebugEnable` / `DebugDisable` / `DebugOnly Interaction` also exist |
| `m_DebugColors` | `Game` | The colour table the debug draws use, including an `Interaction` entry |
| `Suppress=` lines | `Engine` `[Core.System]` | What the engine is NOT logging, e.g. `DevCrosshair`, `DevUIFocus` |

The console needs `tools\setup-game-ini.ps1 -Console` first (F1, then `~`).

## Setting these on install

`-VRBaseline` applies the four values the mod actually depends on and explains
each one on the line. **The rest of this folder is deliberately not touched.**
Two reasons, both learned here: the game rewrites these files from its own menus
so anything we set is provisional, and a mod that silently rewrites a player's
graphics settings is indistinguishable from a mod that broke them. Anything new
that joins the baseline should arrive with a measured reason, the way those four
did.

Resolution is NOT in the baseline and must not be: the mod drives the render size
from `[Screen]` in its own ini, and an older release that wrote `ResX`/`ResY` into
`Engine` and all four `Compat` buckets is exactly the trap that took a session to
unpick. See TRAPS.
