# Research

Facts with sources. Engine and runtime facts first, prior art second, the legal posture last.
Everything here was gathered 2026-09-02 from the web and from the BioShock trilogy mod's
research; nothing was measured on the game yet (not installed on the dev PC).

## The target

- **Dishonored (2012), Arkane/Bethesda. Unreal Engine 3, licensee build 9099.** PC x86 only
  (Steam/GOG); a 64-bit build exists only in the Microsoft Store / Epic packages. Direct3D
  9.0c, shader model 3. Middleware: PhysX 2.8.4 + APEX, Wwise, Scaleform GFx (the UI), Bink
  1.9p. Exe: `Binaries\Win32\Dishonored.exe`; `binkw32.dll`, `PhysXCore.dll` beside it.
  Sources: michaeljcole UE games list; PCGamingWiki "Dishonored"; the Unreal Engine blog post
  on Dishonored (Arkane added audio propagation, a dialog editor, stealth/AI on stock UE3
  Kismet/Lightmass/navmesh).
- **Package format 801 / licensee 030**, fully supported by UELib and UE Explorer (packages +
  UnrealScript decompile). Game classes are in `DishonoredGame\CookedPCConsole\
  DishonoredGameFull.upk` (decompress first). Only `DishonoredCheatManager` and the `Dis*`
  cheat prefix (`DisSlomo`) are documented online; the class prefix for the pawn/controller/
  camera must be confirmed in UE Explorer. Sources: EliotVU/Unreal-Library compatibility
  table; UE-Explorer; Steam community cheat guides.
- **No ASLR**: the exe loads at 0x400000 (the mod's `kModBase`; the DllMain asserts it). Steam
  appid 205100 (RHCP variant 217980); last PC patch 1.4 (Aug 2013); "Definitive Edition" on
  Steam/Epic is the same PC build plus DLC. GOG "Definitive Edition" is DRM-free build 334700
  (2022) and unsupported (different exe). Sources: SteamDB; GOG DB; Steam threads.
- **Config** in `%USERPROFILE%\Documents\My Games\Dishonored\DishonoredGame\Config\`.
  `DishonoredEngine.ini`: `ResX`, `ResY`, `Fullscreen`, `bSmoothFrameRate`,
  `MinSmoothedFrameRate`, `MaxSmoothedFrameRate` (default 120; keep <= 148 or cutscene
  triggers break, input issues from ~158), `OneFrameThreadLag` (False removes one frame of
  render-thread lag: relevant for head-pose latency), `MaxAnisotropy`,
  `bForceNoStartupMovies`. Resolution is mirrored in `HKCU\Software\Arkane\Dishonored`
  `ResX`/`ResY`. Alt+Enter toggles windowed. Sources: PCGamingWiki; PCGamesN improvement
  guide; Blur Busters forum.
- **FOV**: slider 65-85; console `FOV <n>` goes further (community up to 110); bind it in
  `DishonoredInput.ini` after `m_PCBindings=(Name="Zero",Command="GBA_Shortcut_9")`. The mod
  drives FOV from inside (`FovLeverApply`) instead. Sources: PC Gamer; Destructoid.
- **Console / cheats**: `m_PCBindings=(Name="F1",Command="set Console ConsoleKey Tilde | set
  PlayerController CheatClass class'DishonoredCheatManager' | EnableCheats")` in
  `DishonoredInput.ini` (`tools\setup-game-ini.ps1 -Console`). Commands: `DisSlomo`,
  `DisSlomoFull`, `MaxPowers`, `AddPower`, `Ghost`, `Fly`, `Teleport`. The mod's `RunConsole`
  drives the engine console directly (used by the intro skip: `ce ChangeLvl_fromTower_toPrison`).
- **Modding tools**: UPK Explorer (built for Dishonored), deadYokai/dishonored-toolkit
  (unpack/patch/texture/font; needs Unreal Package Decompressor first).

## Engine facts the mod relies on (from the original author's work, to re-verify)

See `docs/dishonored/ENGINE_NOTES.md` for the full table. In short: `UObject::ProcessEvent`
at 0x00470640 is the script dispatch spine and the head-tracking write point
(`ProcessViewRotation` parms); GObjects at 0x1423630, GNames at 0x1435674; the Blink power's
aim vector is sourced at 0xbf55a3 (the shipped redirect), with the trace, destination and
first-movss sites nearby; the XInput import slots at 0xf946c0/c4; the first-person arms are
positioned by a `LookAtControl_Camera` SkelControl in the player's AnimTree (the head-coupling
root cause).

## UE3 VR technique (general, sourced)

- **UEVR is UE 4.8-5.4 only**; no UE3 support and none planned (praydog/UEVR README).
- **Mirror's Edge VR mod** (letsgosportsteam, 2026; UE3 build 3.536, D3D9) is the closest
  public writeup: `d3d9.dll` proxy, stereo by DRAW DUPLICATION (every scene draw issued twice
  into the left/right halves of one wide backbuffer), synthetic display modes via
  `EnumAdapterModes` so the whole render-target set allocates at that size, view matrix found
  at `SetVertexShaderConstantF` slot 94 register c0 in row-vector world-space form (per-eye
  offset `row3 -= o.x*row0 + o.y*row1 + o.z*row2`), FOV read from the matrix, occlusion query
  `GetData` (vtable 7) forced visible during duplication, head pose written through
  GNames/GObjects + `ProcessEvent` into `PlayerController.Rotation`/`FOVAngle`, HUD to a
  separate composition layer (planned), frame cap must divide the headset refresh. Source:
  github.com/letsgosportsteam/mirrors-edge-vr-mod ENGINE_NOTES.md. This is the "stereo without
  DXVK" path in the backlog.
- **Helix Mod 3D Vision fix for Dishonored (Oct 2012)**: patches ~600 shaders; the passes that
  break in stereo are light halos, shadows, fog, water and reflections, glass, HUD depth, the
  crosshair; mine/grenade crosshair and scope zoom unfixed. DarkStarSword's UE3 notes: the
  screen-space reconstructions break (shadows, light shafts with `TextureSpaceBlurOrigin`,
  bloom, `DNEReflectionTexture` reflections, water refraction, decal/point-light scissor rects,
  `vPos` shaders). The fork's M5/M6/M7 series is exactly this list, done inside DXVK. Sources:
  helixmod.blogspot.com/2012/10/dishonored.html; DarkStarSword/3d-fixes README; Epic forum
  "UE3 3D Vision issues".
- **UE3 camera and arms** (UDK docs): `PlayerController.GetPlayerViewPoint` ->
  `Camera.UpdateViewTarget` -> `Pawn.CalcCamera`; `ProcessViewRotation(DeltaTime, out
  ViewRotation, DeltaRot)` is called from `UpdateRotation` (the mod's hook point); first-person
  arms are the pawn's `bOnlyOwnerSee` SkeletalMeshComponent; `SkelControlLookAt`
  (`TargetLocation`, `ControlStrength`) and `SkelControlSingleBone` are the documented
  bone-driving nodes, found via `FindSkelControl`. Sources: UDK CameraTechnicalGuide,
  CharactersTechnicalGuide, UsingSkeletalControllers.
- **Prior Dishonored VR**: GingasVR's Dishonored-VR (this codebase); vorpX (Geometry 3D +
  DirectVR profile); Vireio Perception (2013, d3d9 proxy, HUD invisible). None documents
  Dishonored's camera or SkelControl internals; the original author's comments are the only
  record (ENGINE_NOTES).
- **F.E.A.R. VR** (LithTech) shows the x64-host + x86-bridge workaround for games whose
  runtime has no 32-bit path (poses and shared D3D11 handles over shared memory). Not needed
  while VDXR ships a 32-bit runtime.

## DXVK

- v3.0 (25 Jun 2026): dxbc-spirv shader compiler, Vulkan 1.4 required, D3D9 fixed function
  as ubershaders, on-demand uploads fix 32-bit D3D9 address-space crashes. v3.0.1 (5 Jul):
  secondary command buffers off on desktop GPUs, D3D9 regressions. v3.0.2 (17 Jul): the base
  of the fork. v3.1 (28 Aug 2026): incremental present, a D3D9 fog regression fix (candidate
  for a rebase). Build: meson 0.58+, MinGW-w64 10+ (official) or MSVC (upstream CI builds x86
  on Windows with `--backend vs2022`), glslang. AVX in MinGW builds is a compile-time error.
- The d3d9 frontend records draws linearly (`D3D9DeviceEx` -> `EmitCs` -> `DxvkContext::draw*`,
  no deferred contexts), so per-eye replay in `d3d9_device.cpp` is the natural splice point;
  that is what the fork does. Other VR forks: TheIronWolfModding `vr-dx9-rel` (OpenVR submit
  from DXVK, GTR2), the GTA IV VR mod (AER/TrueStereo on stock DXVK + Vulkan interop), a Black
  Ops (2010) VR project on Proton.

## OpenXR from a 32-bit process

| Runtime | 32-bit OpenXR | Note |
|---|---|---|
| SteamVR / Steam Link | no | no `steamxr_win32.json`; open requests since 2021. Use OpenVR (the mod's native path) or an OpenXR-on-OpenVR shim (the BioShock mod ships one). |
| Virtual Desktop VDXR | yes | the runtime both the Mirror's Edge and BioShock mods confirm for 32-bit D3D11 |
| Meta PC (Link / Air Link) | on paper (`oculus_openxr_32.json`) | 32-bit `xrCreateSession` crashes reported; -50 `GRAPHICS_REQUIREMENTS_CALL_MISSING` if the D3D11 requirements call is skipped |
| Windows Mixed Reality | yes ("Active - 32-bit Only") | deprecated platform |
| Pimax | reported yes | |
| Varjo, PimaxXR | no | |

Loader facts: a 32-bit app reads `HKLM\SOFTWARE\WOW6432Node\Khronos\OpenXR\1\ActiveRuntime`;
`XR_RUNTIME_JSON` overrides discovery entirely (how the simulator is selected);
`xrGetD3D11GraphicsRequirementsKHR` must precede `xrCreateSession`. Projection-layer pose:
the spec says the view pose and FOV "should almost always derive from `XrView::pose`/`fov`
from `xrLocateViews` at `predictedDisplayTime`", i.e. the pose the image was rendered with;
the runtime reprojects by the delta to the display-time pose. Oculus reportedly ignores a
submitted pose that differs from the located one; SteamVR honours it. This is the axis of the
XR_HANDOFF: the game camera already carries the head rotation, so a stamp that disagrees
with the current head cancels the motion the render applied.

## Legal / distribution posture

The mod is a fan project: no game assets, no decompiled script in the repo (`tools/uscript/`
gitignored), addresses and offsets only. Licenses: the whole repo zlib (DXVK's, kept as the
original author chose); ImGui MIT; OpenXR SDK Apache-2.0; OpenVR BSD-3 (vendored with SHA-256
provenance); the BioShock-derived tooling MIT (the user's own). UEVR is all-rights-reserved:
concepts only. REFramework MIT with attribution.
