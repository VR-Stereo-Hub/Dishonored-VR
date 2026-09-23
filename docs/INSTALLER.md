# The installer: DishonoredVR-Setup.exe (VR-198)

One exe a player downloads from the Releases page and runs. It finds the game, installs
the mod beside it, sets the game up, asks the two questions that matter (which headset,
how many pixels) and leaves everything else to the F10 panel. Run again later it turns
into a small control panel: update, change those two settings, disable VR, collect a
support bundle, uninstall.

It is a native 32-bit Windows program (`src/tools/installer/`), drawn with Dear ImGui in
the F10 panel's own theme (`src/core/ui/ovl_ui.cpp`, VR-197): ink, bone, brass and
oxblood, Constantia over Segoe UI, the brass rule with the diamond. The mod's files are
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
5. **Deletes `dxvk_d3d9.dll` and `dxvk_stereo.txt`** if a release before 41.0 left them.
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
   line endings untouched. Exactly five keys, nothing else:

   | Choice | Keys | Values |
   |---|---|---|
   | Quest via Virtual Desktop | `[VR] Runtime`, `XrRuntimeJson` | `native`, the path of `virtualdesktop-openxr-32.json` |
   | SteamVR headset | `[VR] Runtime`, `XrRuntimeJson` | `steamvr`, empty (the bundled shim; start SteamVR first) |
   | Let the mod choose | `[VR] Runtime`, `XrRuntimeJson` | `auto`, empty |
   | Render quality | `[Screen] RenderWidth`, `RenderHeight` | Performance 2382x2468 (75 %), Balanced 2750x2850 (100 %, the tested size), Quality 3012x3122 (120 %), or the Advanced slider's own size |
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

**Done**: the step list, the three things to remember (launch from Steam; F5 recenters
and F10 opens the panel; Motion Blur off in the game's own options, which live in the
Steam profile and cannot be written here), the waiting row when the game has not run,
`Launch Dishonored` (`steam://rungameid/205100`) and `Open game folder`.

**Manage** (an install record, or our `d3d9.dll` with a trace beside it): installed
version and build against the one this exe carries, the game folder, VR enabled or
disabled, the ini's runtime and size, then `Update` (oxblood when the bytes differ,
`Reinstall this build` when they do not; the ini is untouched), `Change settings` (the
Set up choices again, preselected from the ini; writes the five keys and re-applies the
game baseline, which the game's own video options reset), `Disable VR` / `Enable VR`
(`disable_vr.txt` beside the game: the mod loads and does nothing), `Collect support
bundle` (unpacks `collect-support.ps1` to `%TEMP%\DishonoredVR-Setup\` and runs it with
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
installer was started as administrator, `Launch Dishonored` goes through the desktop
shell's own automation object so Steam gets the user's token; when that route is missing
the screen says to launch from Steam instead.

## Every word it takes

```
DishonoredVR-Setup.exe                               the window
DishonoredVR-Setup.exe --game-dir <dir>              ... against that game folder
DishonoredVR-Setup.exe --config-dir <dir>            ... with that game-config folder
DishonoredVR-Setup.exe --apply --op <op> --game-dir <dir> [--config-dir <dir>]
        [--runtime vdxr|steamvr|auto] [--quality performance|balanced|quality|custom]
        [--percent <n> | --size <W>x<H>] [--vdxr-json <path>] [--delete-ini]
                                                     unattended; prints the steps, exit 0 / 2 failed / 3 access denied
        <op> = install | update | change | baseline | disable | enable | uninstall
DishonoredVR-Setup.exe --render <state>|all <out.bmp>|<dir> [--scale <f>]
                                                     draw a screen with no window (tools\installer-render.ps1)
DishonoredVR-Setup.exe --elevated-apply ... --result <file>
                                                     what the window runs under UAC; not for hand use
```

The log is `%LOCALAPPDATA%\DishonoredVR\dishonored_vr_setup.log` (previous run in
`.prev.log`, `DVR_DATA_DIR` moves it), the folder the mod keeps its harness files in. It
carries the detection line (game folder and how it was found, running, writable, config
folder, elevation, `d3dcompiler_47`, VDXR, SteamVR, the active runtime, the GPU and its
budget, installed and embedded `d3d9.dll` hashes, the legacy marker) and every step.

## Building and packaging

`tools\build.ps1 -Release` builds it as part of ALL (`cmake --build build --config
RelWithDebInfo --target dvr_setup` builds just it) into `build\src\RelWithDebInfo\
DishonoredVR-Setup.exe`, beside the DLLs. `src/tools/installer/CMakeLists.txt` stages the
payload **per config** into `build\installer-payload\<config>\` with `$<TARGET_FILE:...>`
and plain copies, and copies `payload.rc` itself in the same step so rc.exe recompiles
whenever any payload input changes (the Visual Studio generator ignores `OBJECT_DEPENDS`
for resources). The `.rc` names files only; the per-config folder reaches rc.exe through
the include path. A shared staging folder would let a Debug build leave Debug DLLs for the
next Release installer to ship, the VR-160 lesson in a new place. `openvr_api.dll`'s hash
is checked against `PROVENANCE.txt` at configure time. The window title and a banner name
a Debug or legacy build. `d3dcompiler_47.dll` is delay-loaded (imgui_impl_dx11 imports
it) so an installer on a machine that lacks it can say so instead of dying in the loader.

`tools\package.ps1` copies the exe to `dist\DishonoredVR-Setup-v<version>.exe` beside the
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
