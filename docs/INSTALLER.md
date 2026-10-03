## 2026-10-03: Display upscaler settings and ReShade compatibility

Display now offers Off, NVIDIA DLSS and AMD FSR; Native AA/DLAA, Ultra Quality,
Quality, Balanced, Performance and Ultra Performance; and the same six DLSS presets
as F10. `core/gfx/upscaler_options.h` is the shared model/preset table. FSR does not
use NVIDIA preset numbers; available FSR versions remain runtime-discovered in F10.

Only explicit edits write DLAA/Upscaler, DlssQuality or DlssModel/DlssPreset. Unknown
custom values display as kept. CLI/elevated-worker flags are `--upscaler 0|1|2`,
`--upscaler-quality 0..5` (persisted INI order: Ultra Quality is 5), and
`--upscaler-preset 0..5` (shared F10 model-choice index).

F10 stores the reduced render dimensions in Screen and full resolution in
Clarity/DlssOutputWidth/Height. Detection preselects the full output. Applying an
unchanged output preserves the reduced render, and disabling upscaling restores
full resolution and clears the output latch. This avoids double downscaling and
prevents a later runtime resize from undoing a launcher resolution selection.

ReShade support no longer means an exact DLL hash match. The installed proxy must
contain the released manual-runtime-ready marker for install/enable; this covers
1.0.3 even when the launcher carries a newer build. Disable/removal require the
runtime and settings file, not support recognition. Running-game guards, settings
backup and retained shader/preset files are unchanged. Native UI and scratch tests
exercise both a different compatible proxy and an unrecognized proxy.

## 2026-10-03 maintenance verification

A support bundle identified launcher 1.0.3 with actual game `BUILD 38.74`, the old
DXVK SBS path and no current SteamVR shim. Changing settings had not installed the
new binaries. Treat the game log banner and actual DLL hash as authoritative;
launcher version alone is not the installed mod version. The repair ZIP uses the
existing 1.0.3 release DLLs; remote headset confirmation remains pending.

Installation and update now remove the exact retired runtime artifacts before
writing the new payload, retaining recovery copies. Do not remove stock
`dbghelp.dll`, third-party `dxgi.dll`, arbitrary JSON, user shader files or support
logs. Tests seed these unrelated files and assert their contents survive.

The download-progress modal previously combined `AlwaysAutoResize` with text wrapped
to the available width and no initial width. It collapsed to a narrow column. It
now sets a 520 logical-pixel width capped to the viewport minus margins each frame,
auto-sizes its height and centers on the working viewport. `update-downloading` is
a render fixture. UI checks verify readable dimensions, no vertical scrolling,
viewport containment and closure on completion/failure at 100%, 150% and minimum
supported window size. Native PNGs were visually inspected at both DPI scales.

Validation: installer host 86/86, updater host 57/57, native UI 190/190, full scratch
installer lifecycle including cleanup lock/rollback cases, executable handoff smoke,
and lint. No game launch is needed for these launcher changes.

## Candidate additions: texture packs and ReShade (2026-10-02)

Setup/Change settings > Texture packs offers **Reduce texture address space use**.
Omitting this choice preserves the existing mode. On writes `[Device] Ex=1`, `Managed=paged`,
`ShadowSurfaces=1`, `ShadowFullCopy=1`; Off selects `Managed=shadow` and keeps the compatible
surface/full-copy settings. CLI: `--texture-memory on|off`. The elevated worker receives the
same choice. These apply next launch; Windows pagefile size is never edited. See
[dishonored/PERFORMANCE.md](dishonored/PERFORMANCE.md) for measured costs and limits.

Mods > **Install ReShade 6.8** downloads the official full add-on runtime on request after
this launcher's mod build is installed. CLI operation: `--apply --op reshade`. The worker
verifies the pinned official setup SHA256, reads its ZIP payload without executing it,
validates PE32 x86, and installs only `ReShade32.dll`. Existing ReShade runtime is backed up;
`d3d9.dll`, `ReShade.ini` and presets stay intact. Download failure or hash mismatch refuses
the install. No ReShade runtime is embedded in the launcher or release ZIP.

The mod's `DishonoredVR_ReShade.addon32` is built and embedded with every launcher, installed
by install/update and included in rollback snapshots; it is inactive without ReShade.
Mod uninstall removes this bridge and preserves ReShade and user presets. Shader packages
remain a separate installation from the [official ReShade site](https://reshade.me/).
F10 > ReShade uses the existing panel position and controller input for presets, effects,
techniques and shader parameters. Turn off Performance mode to edit parameters. Startup
is off by default (`[ReShade] Enabled=0`). The tested manual runtime (`ManualRuntime=1`)
renders effects before VR capture and preserves desktop mirror suppression. Builds 264
and 266 are headset-accepted for appearance/startup and panel controls respectively.
Mods provides Turn ReShade on/off and Uninstall ReShade runtime. These are next-launch
operations, refuse while the game runs and require this launcher's installed VR build.
They back up the complete VR INI; removal moves ReShade32.dll to a unique backup and
preserves ReShade.ini, shaders and presets. CLI operations: `reshade-on`, `reshade-off`,
`reshade-remove`. HD Texture Pack 2.0 and the Carinth preset link to their author pages;
HD installation remains manual, following TFC Installer instructions and retaining backups.

## Native sidebar and readiness audit (2026-10-03)

Overview exposes Collect logs directly. Settings has Display and Controls tabs; runtime
and Stereo/AFW use dropdowns, render quality and 50-450% resolution remain visible,
and the full action/source mapping editor is expandable. `--stereo stereo|afw` writes
only an explicit selection. Omitted selections keep existing modes, including custom
ones. Play remains at the bottom right on every page. Unsaved settings survive navigation;
Play asks whether to return to settings or use saved values. Settings Apply never replaces
an installed DLL. Browsing a different installation resets its draft.

Mods, Bindings, Updates and Help/about use the same native sidebar and artwork. The
required headset selector remains modal. Running-game or unknown process checks refuse
settings writes; active operations and downloads block conflicting navigation. Collect
logs remains a read-only local operation, including while the game runs. Its bounded ZIP
includes up to ten session logs and stays under the existing 24 MB budget; nothing uploads.

Changing settings preserves the installed DLL's actual hash and recorded build identity,
rather than recording the launcher's bundled version as installed. Unknown identities
remain unknown. Build ordering now prepares the 64-bit DLAA/DLSS helper before configuring
and embedding the launcher payload, including available pinned NVIDIA/AMD runtime files.

Verification includes 175 real-widget interaction checks (`tools/installer-ui-host.ps1`),
100%/150% native renders, full-INI smoke, running-game refusal, exact helper hashes,
ReShade on/off/removal backups, installed identity preservation and partial-update rollback.
Two old smoke assumptions were corrected: ReShade comments can precede Enabled, and
multiple intentional INI backups can coexist. No game is launched by these tests.

Verification: `tools/installer-host.ps1`, `tools/installer-smoke.ps1`,
`tools/reshade-install-tests.ps1 -DownloadFile <official pinned setup>` and launcher render
fixtures. Tests never execute the downloaded setup or start the game.

# The launcher: DishonoredVR-Launcher-v1.0.1.exe (VR-198)

One exe to install, configure and launch Dishonored VR. Setup offers runtime,
render quality, desktop mirror, physical crouching, close rain overlay and movement
direction. Controller shortcuts and exact resolution remain expandable. Returning
players can launch through Steam, change settings, collect logs, create shortcuts,
update, disable VR or uninstall.

The **Bindings** page embeds the owner-supplied Quest 3 reference. Fit/zoom and
scrolling keep it readable; maximize the window for more room. Current shortcut
preferences appear above the image because custom settings can differ.

It is a native 32-bit Windows program (`src/tools/installer/`), drawn with Dear ImGui using the selected
sidebar design: full-width Dishonored artwork, parchment navigation, Perpetua headings,
Display/Controls settings and a fixed footer. Shared F10 controls retain their own theme. The mod's files are
embedded in the exe as resources, so there is nothing to unzip and nothing else to
download. The zip still ships for people who prefer to copy files by hand.

## What it does, in order

The Install button runs these steps and shows each one on the Done screen with what it
found. A failed step stops the list; nothing below it runs.

1. **Refuses while `Dishonored.exe` runs.** Its `d3d9.dll` is mapped and cannot be
   replaced. A process list that cannot be read counts as running: a check that cannot
   fail its own hypothesis is not a check.
2. **Warns when `d3dcompiler_47.dll` is missing** from Windows (the SysWOW64 folder). The
   mod needs it to reach the headset and the log would say `no D3D11 blit`; the install
   goes on, the warning names the fix (Windows Update or the DirectX End-User Runtime).
3. **Backs up a foreign `d3d9.dll`** once, to `d3d9.dll.dvr-backup`, only when nothing
   beside it says it is this mod's (no `dishonored_vr.ini`, no `dishonored_vr.log`, no
   `dxvk_d3d9.dll`). Uninstall puts it back. The same rule as `tools/install.ps1`.
4. **Writes `d3d9.dll`, `dvr_steamvr32.dll` and `openvr_api.dll`** from the embedded
   copies. Every write is atomic (a `.tmp` beside the target, then a replace), so a
   half-written DLL can never be the one the game loads.
5. **Cleans the known pre-41.0 artifacts before writing replacement payloads:**
   `dxvk_d3d9.dll`, `dxvk_stereo.txt`, `vr_actions.json`, `vr_bindings_knuckles.json`,
   `vr_bindings_touch.json` and `vr_bindings_native.json`. Exact names come from
   original proxy source `824e08d8b`; no wildcard removal is used. Copies go to a
   timestamped `dvr-legacy-backup-*` folder. Unexpected directories/reparse points
   and inspection, backup or removal failures stop the operation. A partial cleanup
   restores already removed members; a later update failure restores all six through
   the update transaction. Current runtime DLLs are replaced by the payload, and the
   existing INI migration/preservation policy still applies.
6. **Writes `dishonored_vr.ini`** as a byte copy of `release/dishonored_vr.ini`, the ini
   this build was tuned and tested with (HANDOFF rule 6), but only when there is none.
   An existing ini at the build's `[Meta] Version` is kept: the F10 settings in it
   survive. An existing ini from an OLDER build is refreshed here and now, with the old
   file kept beside it as `dishonored_vr.ini.<stamp>.dvr-backup`: the mod would replace
   it at the next launch anyway (`config.cpp`, `kConfigVersion`), keeping only `[VR]
   Runtime`, `XrRuntimeJson` and a non-empty `DataDir`, so a render size written into it
   would be lost and an empty `DataDir` would come back as the dev PC's drive. Measured
   on the dev PC on 2026-09-23 with an ini at version 13: the mod's refresh discarded the
   installer's 2064x2208 and wrote `D:\dvr-data`. Doing the refresh in the installer
   keeps every choice the player just made; `Update` carries the old file's runtime and
   size across too. The version the refresh targets is read from the embedded ini's own
   `[Meta] Version`, never a second constant.
7. **Applies the choices** with the private-profile API, one key at a time, comments and
   line endings untouched. Five baseline choice keys, plus optional preferences:

   | Choice | Keys | Values |
   |---|---|---|
   | Quest via Virtual Desktop | `[VR] Runtime`, `XrRuntimeJson` | `native`, the path of `virtualdesktop-openxr-32.json` |
   | SteamVR headset | `[VR] Runtime`, `XrRuntimeJson` | `steamvr`, empty (the bundled shim; start SteamVR first) |
   | Let the mod choose | `[VR] Runtime`, `XrRuntimeJson` | `auto`, empty |
   | Render quality | `[Screen] RenderWidth`, `RenderHeight` | Performance 2382x2468 (75 %), Balanced 2750x2850 (100 %, the tested size), Quality 3012x3122 (120 %), or the Advanced slider's own size |
   | Desktop mirror | [VR] DesktopMirrorOff | inverse of the checkbox; honored on every runtime |
   | Physical crouching | [Tracking] PhysicalCrouch | 0 or 1 |
   | Hide close rain overlay | [Rain] Hide | 0 or 1; sky rain and splashes stay |
   | Walk in the direction you look | [Camera] HeadBasedMovement | 0 or 1 |
   | Controller shortcuts | [Controllers] DpadModifier, DpadFlip, PauseChord | same values and flip policy as F10 |
   | (always) | `[Paths] DataDir` | empty, meaning `%LOCALAPPDATA%\DishonoredVR` |

   The runtime mapping is `tools/vr-runtime.ps1`'s. The sizes use the F10 Display
   picker's arithmetic (both axes by the square root of the pixel share, rounded half up),
   so 110 % is 2884x2989 and 120 % is 3012x3122 as `PERFORMANCE.md` records. Virtual
   Desktop chosen but its manifest missing is a warning and `auto`, never a broken pin.
   `DataDir` is emptied only when it is empty already or names the dev PC's `D:\dvr-data`
   (a value a build once shipped); a folder set by hand is kept. `WritePrivateProfileString`
   with a null value would delete the key, and a missing key falls back to that same dev
   path in `config.cpp`, which is why the value is written as an empty string and never
   removed.
8. **Applies the four game-ini values** the mod depends on, exactly
   `tools/setup-game-ini.ps1 -VRBaseline`: `DishonoredEngine.ini` `[Engine.Engine]
   bSmoothFrameRate=FALSE`, `[SystemSettings] DepthOfField=False`, `[SystemSettings]
   UseVsync=False`; `DishonoredInput.ini` `[Engine.PlayerInput] bEnableMouseSmoothing=FALSE`.
   Section-scoped (the same key lives in `[SystemSettingsMobile]` too), value compared
   case-sensitively (`FALSE` is not `False` to the engine), a missing key appended at the
   end of its section, a missing section a refusal with nothing written, CRLF and any BOM
   preserved byte for byte. Each file is backed up once per run as
   `<file>.<yyyyMMdd-HHmmss>.dvr-backup` before its first change, and only when a change
   is needed. The game's own config folder is `Documents\My Games\Dishonored\DishonoredGame\
   Config`, resolved through the known-folder API so a OneDrive-moved Documents works.
   **Resolution is never written to the game's inis** (`GAME_CONFIG_MAP.md`, "Setting
   these on install"): the mod drives the size from its own `[Screen]` keys.
   When the folder does not exist the game has never run: the step reads "waiting for the
   game's first run", and the Done screen polls every two seconds while it is open and
   applies the four values by itself once the folder appears and the game has exited. An
   `Apply now` button does the same on demand.
9. **Writes `dishonored_vr_install.json`** beside the game: the installer's version and
   build id, the build config, the `d3d9.dll` sha256, the UTC time, the choices and whether
   it ran elevated. `d3d9.dll` carries no version resource, so the hash is the identity,
   the same one `tools/archive-symbols.ps1` keys the symbol archive on.

Nothing else is copied into the game folder: no scripts, no docs, no `.cmd` files. The
support collector is unpacked to `%TEMP%` when its button is pressed.

## The screens

**Set up** (no mod installed yet). One screen: the game folder, found through Steam
(`HKCU\Software\Valve\Steam` SteamPath, every `path` in `libraryfolders.vdf`, the library
holding `appmanifest_205100.acf`), or `DVR_GAME_DIR`, or the `Change...` folder picker,
which accepts the game root, `Binaries` or `Binaries\Win32`. Then the two choice rows:

- **Headset**: Virtual Desktop is preselected when `%ProgramW6432%\Virtual Desktop
  Streamer\OpenXR\virtualdesktop-openxr-32.json` exists (a 32-bit process must ask for
  `ProgramW6432`; `%ProgramFiles%` is `Program Files (x86)` there), SteamVR when
  `steamapps\common\SteamVR` is in a library, else "let the mod choose". The status line
  names the 32-bit OpenXR runtime Windows has registered (`HKLM\SOFTWARE\Khronos\OpenXR\1`
  `ActiveRuntime`, read through the WOW64 view because that is the view the game reads).
- **Render quality**: Performance is preselected when DXGI's local memory budget is under
  9 GB (an 8 GB card measured 45-55 pairs/s at 120 % in the headset, VR-160), Balanced
  otherwise, Quality never. The budget comes from `IDXGIAdapter3::QueryVideoMemoryInfo`
  because `DXGI_ADAPTER_DESC::DedicatedVideoMemory` is a `SIZE_T` and reads 4 GB for every
  card in a 32-bit process. The status line names the GPU and says "set the headset to
  90 Hz", the single most effective setting in the mod, which is not in the mod.
- **Advanced** (collapsed): the F10 Display tab's "Total pixels (%)" slider, 50 to 200.

`Install` is the one oxblood button. When the game folder is not writable (Steam under
`Program Files (x86)` on a locked-down account) it reads `Install (administrator)`.

**Done**: the step list, launch guidance (Steam/GOG directly or the launch button;
F5 recenters and L3 + R3 opens settings), the waiting row when the game has not run,
`Launch via Steam` (`steam://rungameid/205100`) and `Open game folder`.

**Manage** (an install record, or our `d3d9.dll` with a trace beside it): installed
version and build against the one this exe carries, the game folder, VR enabled or
disabled, the ini's runtime and size, then `Update` (oxblood when the bytes differ,
`Reinstall this build` when they do not; the ini is untouched), `Change settings` (the
Set up choices again, preselected from the ini; writes the selected keys and re-applies the
game baseline, which the game's own video options reset), `Disable VR` / `Enable VR`
(`disable_vr.txt` beside the game: the mod loads and does nothing), `Collect logs` (unpacks `collect-support.ps1` to `%TEMP%\DishonoredVR-Launcher\` and runs it with
`-GameDir`; the zip lands in `Desktop\DishonoredVR Support` and the folder opens on it),
`Uninstall` (an inline confirmation with "also delete dishonored_vr.ini", off by default;
mirrors `tools/uninstall.ps1`: removes `d3d9.dll` only when it is ours, the shim, Valve's
loader, the dxvk leftovers, the kill switch and the record, restores the backup, leaves
the game's own inis with their `.dvr-backup` copies in place).

## Elevation

The installer runs as the invoker (its manifest says so; without a declared level a
32-bit exe gets UAC file virtualisation, its writes under `Program Files (x86)` are
redirected to `VirtualStore` and report success). Before anything is written it probes the
game folder and the config folder with a create-new, delete-on-close file. A denied probe
turns the Install button into `Install (administrator)`; pressing it relaunches this exe
elevated as a **headless worker** (`--elevated-apply`, every resolved path and choice on
its command line, the step results back through a `--result` file), and the window shows
the results. The window itself never elevates: the folder picker and the Steam launch
stay on the player's own token (a game launched from an elevated Steam cannot talk to the
normal one), and under an over-the-shoulder elevation the admin's Documents and registry
are the wrong ones, which is why the child re-resolves nothing. Elevation is attempted at
most once; a second denial is reported as what it usually is, Controlled Folder Access or
an antivirus on `Documents\My Games`, which elevation does not bypass. If the whole
installer was started as administrator, `Launch via Steam` goes through the desktop
shell's own automation object so Steam gets the user's token; when that route is missing
the screen says to launch from Steam instead.

## Every word it takes

```
DishonoredVR-Launcher-v1.0.1.exe                               the window
DishonoredVR-Launcher-v1.0.1.exe --game-dir <dir>              ... against that game folder
DishonoredVR-Launcher-v1.0.1.exe --config-dir <dir>            ... with that game-config folder
DishonoredVR-Launcher-v1.0.1.exe --apply --op <op> --game-dir <dir> [--config-dir <dir>]
        [--runtime vdxr|steamvr|auto] [--quality performance|balanced|quality|ultra|custom]
        [--percent <n> | --size <W>x<H>] [--vdxr-json <path>] [--delete-ini]
                                                     unattended; prints the steps, exit 0 / 2 failed / 3 access denied
        <op> = install | update | change | baseline | disable | enable | uninstall
DishonoredVR-Launcher-v1.0.1.exe --render <state>|all <out.bmp>|<dir> [--scale <f>]
                                                     draw a screen with no window (tools\installer-render.ps1)
DishonoredVR-Launcher-v1.0.1.exe --elevated-apply ... --result <file>
                                                     what the window runs under UAC; not for hand use
```

The log is `%LOCALAPPDATA%\DishonoredVR\dishonored_vr_launcher.log` (previous run in
`.prev.log`, `DVR_DATA_DIR` moves it), the folder the mod keeps its harness files in. It
carries the detection line (game folder and how it was found, running, writable, config
folder, elevation, `d3dcompiler_47`, VDXR, SteamVR, the active runtime, the GPU and its
budget, installed and embedded `d3d9.dll` hashes, the legacy marker) and every step.

## Building and packaging

`tools\build.ps1 -Release` builds it as part of ALL (`cmake --build build --config
RelWithDebInfo --target dvr_setup` builds just it) into `build\src\RelWithDebInfo\
DishonoredVR-Launcher-v1.0.1.exe`, beside the DLLs. `src/tools/installer/CMakeLists.txt` stages the
payload **per config** into `build\installer-payload\<config>\` with `$<TARGET_FILE:...>`
and plain copies, and copies `payload.rc` itself in the same step so rc.exe recompiles
whenever any payload input changes (the Visual Studio generator ignores `OBJECT_DEPENDS`
for resources). The `.rc` names files only; the per-config folder reaches rc.exe through
the include path. A shared staging folder would let a Debug build leave Debug DLLs for the
next Release installer to ship, the VR-160 lesson in a new place. `openvr_api.dll`'s hash
is checked against `PROVENANCE.txt` at configure time. The window title and a banner name
a Debug or legacy build. `d3dcompiler_47.dll` is delay-loaded (imgui_impl_dx11 imports
it) so an installer on a machine that lacks it can say so instead of dying in the loader.

`tools\package.ps1` copies the exe to `dist\DishonoredVR-Launcher-v<version>.exe` beside the
zip and refuses when the staged `d3d9.dll` fails `Test-DvrLegacyDll` or differs from the
one in the zip. The GitHub release carries both files.

## Verifying it

| Intent | Tool | How to read it |
|---|---|---|
| Does every screen look right? | `.\tools\installer-render.ps1` | `build\installer-preview\<state>.png` for every named state (`src/tools/installer/model/fake_states.cpp`), at 1.0 and 1.5 scale. Compare with the F10 panel's theme render (`tools\ovl-theme-preview.ps1`). |
| Do the helpers do what the scripts do? | `.\tools\installer-host.ps1` | `tools\installer-tests.cpp`: the game-ini editor on synthesised files (CRLF, LF, no final newline, UTF-8 BOM, UTF-16, the duplicated key, append, refuse), the picker sizes, the record, sha256, argument quoting, the private-profile writes on disk. Fixtures are synthesised; no game ini is ever committed. |
| Does an install do the right thing end to end? | `.\tools\installer-smoke.ps1` | `%TEMP%\dvr-installer-smoke\`: a stand-in `Dishonored.exe` (a renamed `ping.exe`, so "refused while running" is real), a foreign `d3d9.dll`, synthesised game inis. Asserts the DLL hashes, that the ini differs from the shipped copy in exactly the five keys, CRLF and no BOM, the four game-ini lines and nothing else, the backups, idempotence, a hand-edited ini kept, VDXR pinning, disable/enable, uninstall restoring the backup and keeping the ini. |
| Did the game honour what was written? | one Steam launch, `dishonored_vr.log` | `config: [Screen] Render WxH`, `config: [Paths] DataDir -> ...`, `xr: XR_RUNTIME_JSON set from [VR] XrRuntimeJson` (VDXR) or the runtime line the `xr:` lines name. A verified write is not an honoured one. |

## Traps this paid for while being built

- `windows.h` maps `ShellExecute` to `ShellExecuteW`, which renames `IShellDispatch2::
  ShellExecute` if the macro is live while `shldisp.h` is parsed. The CMake build defines
  `WIN32_LEAN_AND_MEAN` and the host test build must too, or the two compile differently.
- ImGui's `SameLine(x)` places an item on the PREVIOUS line. A footer laid out
  right-to-left with it drew the primary button under the Cancel button; the footer now
  starts a new line for the first button of a frame and uses `SetCursorPosX`.
- A CHECK macro cannot take `Size{ w, h }` without an extra pair of parentheses; the
  comma splits the argument.
- The staging copy of `payload.rc` must be a plain copy. `copy_if_different` keeps the old
  timestamp and rc.exe does not recompile when only a DLL changed.
- The dev PC's `[Paths] DataDir=D:\dvr-data` is set on purpose there, with `DVR_DATA_DIR`
  matching it in the shell profile. The installer's `change` writes it empty, which is
  right for a player and wrong for that machine; put it back after a test there.

## Deliberately not here

- IPD and snap turn: no ini key exists for either (IPD comes from the runtime's view
  poses; turning is the right stick).
- A left-handed option: six keys plus the mirrored HUD hand panels, nothing in the repo
  does it yet.
- An online update check: the Manage screen shows both versions and links the Releases page.
- GOG (a different exe, unsupported) and Epic auto-detection (Browse covers it).
- The compiled fallback of `D:\dvr-data` for a MISSING `[Paths] DataDir` (`config.cpp`)
  and the version refresh that re-writes it: a mod-side fix, its own ticket.

## Launcher continuation (2026-09-23)

The executable, packaging, version metadata and visible title use
DishonoredVR-Launcher. Internal source/tool paths retain installer names for
compatibility. The returning-player screen has Launch via Steam, Bindings, Collect
logs, Open log, Desktop shortcut and Start menu shortcut buttons. Steam launch
uses the desktop user's unelevated shell and app 205100, with a fresh process check.
There is no automatic game launch.

Shortcut buttons atomically copy the launcher to
%LOCALAPPDATA%/DishonoredVR/Launcher/DishonoredVR-Launcher-v1.0.1.exe, then create a per-user
IShellLink in the Windows Desktop/Programs known folder. The selected game path
is quoted in the shortcut. Nothing is created until clicked. Uninstalling the
mod keeps these independent launcher entry points.

Preferences load from the installed ini. Omitted headless preference flags keep
their values. DLL updates keep a current-version ini byte-for-byte. Old-version
refresh still follows the existing backed-up migration policy. New optional CLI
flags --mirror, --physical-crouch, --hide-rain-overlay,
--dpad-flip and --pause-chord take on/off. --dpad-modifier accepts 0, 1, 2 or 4.
Invalid flags fail before writes. The elevated GUI helper carries the same choices.

Collect logs adds current/previous launcher logs, old setup log names, actual
local/game shim log paths, generated SteamVR JSON, install record, game inis,
VirtualStore shadows, runtime registration, GPU driver, CPU, RAM and OS build.
The manifest keeps sources, timestamps and hashes distinct. Nothing is uploaded.

The shared F10 pill helper accepts an optional width; its default remains unchanged.
Launcher choices fill their row. Settings scroll independently of fixed actions.
Validation includes the optimized build, full-ini scratch smoke comparisons, all
six preference mappings, omitted-flag preservation, invalid-input refusal,
shell-link target/argument roundtrip and support ZIP collection. Offscreen fixtures
cover Bindings fit/zoom, SteamVR mirror choice and every screen at 100/150% DPI.

See [BRVR launcher research](BRVR_LAUNCHER_RESEARCH.md) for the source review and
controller-port work that remains separate.


## Public 1.0.0 launcher polish (2026-09-23)

Fresh setup selects Let the mod choose (Auto) and Balanced regardless of detected
runtime or GPU. Existing INI runtime, exact resolution and preferences remain
selected. Head-based walking is no longer a launcher preference; the underlying
setting is preserved. Recommended refresh: 120 Hz, or 144 Hz with Virtual Desktop
Beta, per the project owner's current guidance; historical performance records
are not evidence against this newer recommendation.

About is reachable from setup, completion and management. It carries version
1.0.0, GitHub releases, the three owner-approved credits/profile links and the
owner-approved optional Ko-fi support text. The supplied emblem is embedded as
seven icon sizes for Explorer, the title bar and shortcuts. No release is
published by setting the version.

Manage has an off-by-default Overwrite INI and F10 settings on update checkbox.
Its selection persists in %LOCALAPPDATA%/DishonoredVR/launcher.ini under
[Updates] OverwriteSettings. The elevated worker receives --overwrite-settings.
When enabled, Update/Reinstall backs up the full existing INI to a unique
.dvr-backup, writes this build's defaults, selects Auto/Balanced and clears the
developer data path. When off, a current-version INI remains byte-identical.
The pre-existing old-schema migration remains backed up and may reset tuning.
No automatic update check/download is implemented; About links to releases.

The F10 Layout tab embeds the supplied controller picture. Full view expands it over the panel; Back restores the tabs. Fit, Zoom -/+, four
pan buttons, scrollbars and dragging expose the whole image at up to 8x zoom.
The tab is available in every tier. The same production viewer is rendered by
tools/ovl-theme-preview.ps1 -Layout [-Zoom], including actual pointer-event
checks for zoom, both-axis panning and Fit resetting the view.

Collector regression: the real 32-bit launcher previously spawned PowerShell
with an inherited module path that did not expose Get-FileHash. The earlier
standalone script test did not exercise this path. Collection now uses the full
system PowerShell path, restores its built-in module search path and hashes with
.NET SHA256 without a cmdlet dependency. A binary read failure goes in the
manifest instead of aborting other evidence. Failed output is saved in full and
the first meaningful error is shown. GUI and --collect-logs share one helper:

    DishonoredVR-Launcher-v1.0.1.exe --collect-logs --game-dir <Win32> --support-out <folder> --result <textfile>

This headless check writes a local ZIP without opening Explorer or launching the
game. The real-folder regression collected 19 evidence files, four binary hashes
and machine details with zero manifest errors. Nothing is uploaded.

## 1.0.1 support collection and history (VR-215)

The 1.0.0 error 3 happened before PowerShell: the launcher used single-level
CreateDirectory for TEMP/DishonoredVR-Launcher/support-<id>, and the parent did
not exist on fresh profiles. The helper creates missing parents, falls back to
LocalAppData/DishonoredVR/SupportCollector if TEMP is unusable, and reports the
failing path if neither works. Collection flushes its launcher log first. The
system PowerShell process emits UTF-8 so non-ASCII output paths remain usable.
Default output is Desktop/DishonoredVR Support, with LocalAppData and temp
fallbacks for unavailable redirected desktops; explicit output errors stay visible.

Both logger users retain ten sessions: .log, .prev.log, .prev2.log through
.prev9.log. Old one-deep histories migrate without losing the previous log.
Rotation happens only at initialization, never in the frame loop. If an archive
move fails, the logger appends rather than truncating the current evidence and
records why. Separate source paths retain separate names in the support ZIP.

The ZIP is capped at 24,000,000 bytes. Small context gets a bounded reservation;
game logs are tried newest to oldest, then other diagnostics. Each file snapshot
is compressed independently with a streaming byte cap, so no guessed compression
ratio or unbounded raw staging copy decides what fits. Current oversized text
logs retain a header/tail excerpt; older oversized logs can be omitted so smaller
older evidence still fits. Manifest records source/copy sizes, exact ZIP-content
hashes, timestamps, omissions and read errors. Full small files remain exact.
The final archive is measured before publication; an oversized ZIP is refused.
Collection never uploads. Dumps remain explicit opt-in and share the size budget.

Tests: tools/log-history-host.ps1, support-collector-tests.ps1,
support-budget-tests.ps1 (run under Windows PowerShell 5.1), and
support-launcher-tests.ps1. The last uses the actual 32-bit launcher, a fresh temp
profile, a Unicode path, ten fixture sessions and an unusable TEMP. Optional
-OldLauncher reproduces the released error before exercising the fix.

## Snap turning (VR-219)

Play preferences carries a Snap turning checkbox next to Hide close rain overlay. It
writes `[Turning] SnapTurn` (0 = the game's smooth turn, the default; 1 = fixed steps
from the right stick) through the same preference path as the other checkboxes, and
`--snap-turn on|off` does the same headless. An omitted flag, like the others, leaves
the stored value alone. The step size and the stick thresholds stay in F10 >
Controls > Turning, where the Snap turn checkbox is also in the Basic view.
## Headset selection (VR-223)

The launcher asks which headset the player has before anything else. With none
recorded, a modal picker opens over whichever screen starts (Setup or Manage)
and has no close control: Continue stays disabled until a headset is picked, or
a name is typed for Something else. The update popup waits behind it.
Afterwards the headset shows in the Headset section of Setup and in Current
Settings on Manage, each with a Change button that reopens the picker with a
Cancel.

It is recorded for diagnostics, and for one setting: Valve Index and Bigscreen
Beyond 1 / 2 turn on the mod's Index controller tuning (VR-224, below). No other
pick changes anything; the runtime pills are still what the mod acts on. The list and its order are the BioShock
Remastered VR mod's Setup.bat question (Quest 3/3S, Quest Pro, Quest 2, Quest
1, Rift S/CV1, Index, Vive/Vive Pro, Vive Pro 2, Vive XR Elite, Beyond, Pimax
Crystal/Light, Pimax 5K/8K, Reverb G2/WMR, Varjo, Pico 4, Somnium VR1, PSVR2,
Something else), so reports from the two mods group the same way. That mod also
flips per-headset controller defaults from the answer; this one does so only for
the Index tuning.

Where it lives: %LOCALAPPDATA%/DishonoredVR/launcher.ini [Headset] Model, the
label itself (a typed name is printable ASCII, 48 characters at most). It is
kept out of dishonored_vr.ini on purpose: the picker has to work before the mod
is installed, and a config version bump rewrites the mod's ini.

Where it shows up:
- the launcher log: `launcher: headset: user reported '<name>'` at every start,
  `none recorded - asking` on a first run, and `user changed 'a' -> 'b'`
- the mod log, near the top: `config: headset (user reported in the launcher): <name>`,
  read from the same file. The runtime's own system name is logged separately
  at session start; the two can legitimately differ (a Quest through SteamVR)
- the support bundle manifest: `headset`

Offscreen states: `headset-required`, `headset-other`, `headset-change`. Every
other fake state has a headset set so the picker does not cover it.

## Index controller tuning from the headset choice (VR-224)

Ported from a community fork's `index-controller-offsets` branch (its two commits
are kept with their author) and gated here. `[Controllers] IndexTuning` in
dishonored_vr.ini: -1 (default) = on when launcher.ini [Headset] Model is
`Valve Index`, `Bigscreen Beyond 1 / 2` or `Vive Pro 2` (played on Index
controllers; Vive wands are not supported), 0 = off, 1 = on. The mod resolves it in
LoadConfig and logs the verdict with its owner
(`config: [Controllers] IndexTuning=-1 -> ON (owner: launcher headset is an
Index-controller headset)`), then hands it to the SteamVR shim in the process
environment as `DVR_INDEX_TUNING` (the shim is loaded later, by the OpenXR
loader, and logs `input: Index tuning ON|off` in ovrshim.log).

What it turns on:

| Part | Where | Needs |
|---|---|---|
| Grip/aim rebuilt from the raw pose and SteamVR's `tip`, so an Index hand reports the Quest frames every weapon offset was tuned on | shim | a `knuckles` controller, SteamVR |
| Hold trim: pitch -21, left yaw +11, wrist roll L -20 / R +30, about the grip point, grip and aim together | shim | same |
| Right-hand sword trim: turn 30 right, tip back 8 (grip only) | shim | same |
| Knuckles binding: grip from the force sensor, not trigger-mode pull | shim (bindings_knuckles.json, rewritten every connect) | same |
| Empty left hand (powers, Blink, the Heart) rolled 60 about the forearm, turned, wrist up 25 | mod, palm draw | nothing else |
| Its aim ray lifted 23 deg to match (the pistol and crossbow keep theirs) | mod | nothing else |

Off, every one of these is exactly what shipped before: the shim reports
handgrip/tip as bound, the binding is trigger-mode pull, and the left hand and
its ray are untouched. The numbers were tuned in one headset on one Index rig and
are not yet confirmed on a second; the trims trade a gripped hand reading slightly
low for an open hand reading level (the fork's own note).

## Ultra quality and the 300% ceiling (VR-282)

Render quality has a fourth pill, Ultra quality, at 150% of the tested pixel count
(3368x3491 per eye), a step above Quality (120%). `--quality ultra` selects it headless,
and an ini already at that size reads back as Ultra. The Advanced slider, like the F10
Display slider, now reaches 300% (4763x4936 per eye; the live resize accepts up to 16384
per side). Nothing above Balanced was judged on more than one card; Ultra is for cards with
clear headroom at Quality.

## Proposed Mods section and launcher layouts (2026-10-02)

Three interactive design previews were prepared: Sidebar, Compact tabs, and Split
workspace. Each keeps primary action buttons adjacent, separates Settings/Mods/Bindings/
Support, and exposes AFW/Stereo as a Settings dropdown. Split workspace also shows it
beside Mods. These are previews only; no production layout or download manager is shipped.
The proposed lifecycle is Download -> downloaded Off -> On/Off with Uninstall available.
Turning Off retains the cache; Uninstall restores owned originals and removes managed data.
All game-file changes must refuse while the game is running and be reversible.

The requested catalog is [HD Texture Pack 2.0](https://www.nexusmods.com/dishonored/mods/51)
and the current ReShade preset variant of
[ENB or ReShade with SweetFX](https://www.nexusmods.com/dishonored/mods/5), plus the existing
optional ReShade runtime. HD 2.0 lists a 4.8 GB download and requires TFC Installer to patch
game assets; its documented removal restores backups. A reliable toggle needs a tested TFC
adapter and enough space to retain originals and prepared modded data. It is not a DLL rename.
The preset variant requires Standard Effects, SweetFX and prod80 effects. Do not copy old
ENB/SweetFX proxy DLLs over the VR proxy. Download from the authors' sources, not a mirror.

The linked [NexusModsModDownloader](https://github.com/Wedsels/NexusModsModDownloader)
uses Python, Playwright/Firefox and stealth automation, copies Firefox profile cookies and
expects an API key. No license file is visible in the repository root reviewed. It is not
selected for bundling. Prefer native API integration with an Import archive fallback.
[Nexus's documented download API](https://github.com/Nexus-Mods/node-nexus-api/blob/master/docs/classes/_nexus_.nexus.md#getdownloadurls)
requires a website-generated nxm key for non-Premium accounts; Premium can obtain direct
links. Do not promise unattended free-account downloads. Public release of API integration
requires [application registration](https://help.nexusmods.com/article/114-api-acceptable-use-policy).
No account credentials were requested or copied during this investigation.

Runtime installation found Get-FileHash discovery can fail in the launcher's inherited
PowerShell environment. The helper uses .NET SHA256 directly and tests a deliberately
unavailable hash cmdlet. Verification failure still leaves game files unchanged.
## Full sidebar design and installed preset validation (2026-10-03)

The user selected Sidebar. The complete interactive preview now uses the existing
backdrop, parchment strip and binding art, serif typography, slate controls and oxblood
primary action. **Play stays bottom right**. Overview holds installation/path and shortcut
actions; Settings separates Display and Controls; Mods owns enable/remove; Bindings retains
the guide; Updates owns release checking, overwrite policy, reinstall and history; Help/about
retains log collection, credits and Ko-fi. The actual launcher and updater code are unchanged.
Display includes headset model, runtime, AFW/Stereo, quality presets, the 50-450% pixel slider,
mirror, rain and texture compatibility. Controls includes crouch, snap turn, modifier, stick
swap and pause chord. Other in-game-only controls remain in F10/L3+R3. Preview navigation,
Apply, mode selection, TFC preparation and update modal interactions pass; 320/360/560px
layouts fit. The native updater's 57 host checks pass, including invalid size/hash/version,
replacement refusal/preservation, successful replacement and Steam/GOG discovery.

TFC: the local HD 2.0 pack has GameProfile.xml at its root and mapping/TFC data below
TexturePack. TFC 2.5.4 exposes GamePath/TFCPath/UpdateSettings in its .NET configuration;
that is evidence for investigating prefilled setup, not proof of an unattended API. The
documented flow is selecting folders then Update All. The proposed launcher will import
and validate the archive, manage its extracted location, check backup capacity, prepare
the paths and guide the final TFC step. Completion must be verified before showing On.
Off/Uninstall must restore verified original packages; it cannot be a simple DLL toggle.
HD textures were not installed during the ReShade test setup. No donor binary was bundled.

The user supplied the current Nexus mod-5 archive in Downloads. Only its sole preset entry
was extracted, not a legacy ENB proxy. Archive SHA256:
EC0EC351918B1C1FD0965B8DBA00ED06843FE4E06B10872859708EC98DAD5613.
Installed DishonoredCarinthPresetv3.ini SHA256:
BF54F4CA10C3748D9FF4585B3B764410E7ED73D968270C02183D2349E2905D6D.
Active techniques are SMAA, LiftGammaGain, LumaSharpen, Vibrance, Curves, HDR (FakeHDR.fx),
and prod80_04_ContrastBrightnessSaturation. Other TechniqueSorting names are inactive.

Official packages, pinned before extraction, retaining source/license files:

| Package | Commit | Download SHA256 |
|---|---|---|
| [Standard Effects](https://github.com/crosire/reshade-shaders/tree/slim) | fd0022170615ce0d8162d219bff07232fa6dd84f | a3b110ba5118f3b944d74f0b0746c21280071d389ed98d615bf3c4b3a1778586 |
| [SweetFX](https://github.com/CeeJayDK/SweetFX) | 93ddf39b357f5da534ed6d34ba4ec8cc7dcfa361 | e1e1d6515d29c65fcf115c9692a1c5f91ffb8d9734e6871bcf67ac48588dbbce |
| [prod80](https://github.com/prod80/prod80-ReShade-Repository) | 1c2ed5b093b03c558bfa6aea45c2087052e99554 | 15b251a3f99901dda81072c3cb8ffa1eb2144dee5399d459a5dde7a50bbd6132 |

Packages live separately in Win32/dvr-reshade-shaders, with explicit effect/texture search
paths in the newly created ReShade.ini. PresetPath selects Carinth; PerformanceMode=1 and
SkipLoadingDisabledEffects=1. Scroll Lock (145) toggles effects; Home (36) opens the overlay.
There was no prior ReShade.ini/preset/package folder to overwrite. Before copying, verified
the accepted mod/runtime hashes and that Dishonored was not running; archived DLL, full
INI, bridge, runtime and both game logs. After copying, full VR INI byte comparison and
CRLF verification pass, and the VR DLL is unchanged. Main build/texture-reshade-candidate/
preset contains sources.json, installed-profile.json, validation scripts and before-profile
backup. Shader/include and texture dependencies are checked locally. Actual compilation,
headset effect capture and frame-time cost await the user's launch.

## Launcher copy and button mapping follow-up (2026-10-03)

The actual native settings screen now exposes all 11 F10 actions and the same 12 physical
source choices, stick swap, reset-to-default mappings, and duplicate-source feedback.
The shared controller_binds header is authoritative. Detection reads existing keys without
marking them dirty; apply writes only changed actions and SwapSticks. Reset explicitly
sets all known actions to defaults while retaining unknown keys. `--bind-<Action> <Source>`
and `--bind-swap-sticks on|off` carry selections into headless/elevated apply. Invalid
sources are refused before mutation. Button press capture remains in F10 during gameplay.

Both current developers have the same role description. The donation paragraph and link
label follow the user's requested product copy. Rain overlay is a positive UI checkbox;
its ON default still writes/reads [Rain] Hide=0. The existing --hide-rain-overlay CLI keeps
its original meaning. A missing saved headset still requires selection at startup.

The complete sidebar preview now places a collapsible mapping editor under Controls.
Texture setup says to choose the downloaded pack, let the launcher unpack/find the game,
then click Update All in the window that opens. That automated preparation remains a
proposal; the final TFC step and verified restore are still required before enable/off
management can ship. No texture pack was installed for this ReShade experiment.

## ReShade that starts, a preset drop zone, and a Mods audit (2026-10-03)

A remote 1.0.3 player installed ReShade from the Mods screen, turned it on, and it never
ran: every launch logged `reshade: load failed error=1114`, no shader folder ever appeared,
and F10 kept saying a restart would load it. The launcher had installed `ReShade32.dll` and
nothing else. ReShade's own DllMain refuses to load (error 1114) when it is not loaded under
a proxy name and no `ReShade.ini` exists beside the exe (upstream `source/dll_main.cpp`, the
"not enabled for" check). The setup that worked on the dev PC had been laid down by hand
(the section above) and carried its own `ReShade.ini`, so the launcher path was never tried
on a clean folder. Reproduced with the real 6.8.0 DLL in a 32-bit test program: no
`ReShade.ini` gives 1114; the launcher's `ReShade.ini`, or `RESHADE_DISABLE_LOADING_CHECK`,
loads it.

What Install ReShade does now (`tools/install-reshade.ps1`):

- the pinned runtime as before; an identical runtime is no longer replaced, so a repeat
  install adds no backup;
- the three packages of the table above, from the same pinned commits and hashes (checked
  live on 2026-10-03), each into `Win32\dvr-reshade-shaders\<name>`, staged and moved into
  place, never over an existing folder;
- `dvr-reshade-shaders\custom\Shaders` and `\Textures` for the player's own files, searched
  recursively (`\**`, ReShade `runtime.cpp`);
- `ReShade.ini` with those search paths, PerformanceMode=1, SkipLoadingDisabledEffects=1,
  Scroll Lock / Home, and PresetPath only when the Carinth preset is already present - and
  only when no `ReShade.ini` exists. A player's own file is never rewritten.

A package that fails its hash or download is reported and the rest still installs; the
operation then reads as incomplete and Repair ReShade retries only what is missing.

The proxy (`core/gfx/reshade_runtime.cpp`) now sets `RESHADE_DISABLE_LOADING_CHECK` for the
duration of its own `LoadLibrary` - `[ReShade] Enabled=1` is the opt-in that check exists to
require - and a failed load records why: another ReShade already in the process (it names
the module), a refusal with whether `ReShade.ini` is present, or a missing dependency. F10 >
ReShade shows that reason instead of "Restart Dishonored to load", which was the message a
player saw on every launch while nothing a restart could change was wrong.

**The drop zone.** The ReShade section has a box: drag a preset `.ini`, a preset download
(`.zip`) or its folder anywhere onto the window, or use Choose files. `tools/import-reshade-
preset.ps1` copies only preset `.ini` files (a `Techniques=` line) beside the exe, `.fx`/`.fxh`
below the archive's own `Shaders` folder into `custom\Shaders`, and images from a `Textures`
folder into `custom\Textures`. It refuses every program file by extension, so the old ReShade
proxy `d3d9.dll` that preset downloads carry can never replace the VR mod, and says so. A
download's own `ReShade.ini` is refused. A replaced file is kept as `.dvr-backup`. It names
the shaders an imported preset uses that no installed package provides, and selects the
preset when `ReShade.ini` names none yet. The import never elevates (an elevated child cannot
be handed dropped paths); the window accepts drops from a non-elevated Explorer either way.

**The Mods audit.** Rendered every state (`tools\installer-render.ps1 -State mods-*`):

| Found | Now |
|---|---|
| A runtime with no `ReShade.ini` read "Installed and enabled" | "Incomplete: ... Click Repair ReShade", the button becomes Repair (primary) |
| Nothing said whether ReShade ran | `Last game launch: ReShade ran / was off / did not start (reason)`, read from the head of the game log |
| The install message pointed at F10 to enable | Points at Turn ReShade on, the button beside it |
| "Carinth shaders already installed" shown with no ReShade | Install / Repair ReShade first |
| Copy said to download the preset and keep d3d9.dll | The drop zone does the copying and refuses DLLs |
| A result's pipe-separated detail was one run-on line | One line per part |
| A ReShade result returned to Overview | Returns to Mods |
| A 180 s limit for ~25 MB of downloads | 600 s for the install |

Tests: `tools\reshade-install-tests.ps1 -DownloadFile <setup exe> -PackageDir <folder with
the three zips>` (existing preservation cases, fresh install, repeat install, Carinth
selection, tampered package) and `tools\reshade-import-tests.ps1` (zip and folder drops,
DLL and foreign ReShade.ini refusal, missing shaders, backups, empty drop) both pass. The
built launcher's `--apply --op reshade` installed everything from the live sources into a
fixture folder. `tools\reshade-manual-host.ps1` (production runtime on a D3D9Ex device)
passes 759 checks. Not yet run: a game launch with the new layout.
