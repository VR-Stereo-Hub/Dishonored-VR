# Known issues

Alpha software. The list from the original 38.92 release plus what the continuation knows.
The milestone in brackets is where the fix is planned (docs/ROADMAP.md).

- **Quest / OpenXR path is not converged** [D1a, D3]. "Zoomed in / can't look up-down /
  hands wrong" on some Quest + Virtual Desktop machines, never reproduced on the author's
  PC. Best current explanation: on a PC with two GPUs (a laptop, or a CPU with integrated
  graphics next to the card) the mod creates its D3D11 device on the default adapter instead
  of the one the OpenXR runtime asks for, and the shared eye textures cannot cross adapters;
  every call succeeds and nothing logs an error. The author's build 39.3 fixed it and it is
  being ported (D1a); it has not been confirmed by an affected user. Also on this path: hands
  and the wrist HUD are not drawn in the quad and cylinder presentation modes, and the
  settings overlay is mis-scaled. SteamVR-native headsets (Vive, Index) are the tuned path.
  Details: docs/dishonored/XR_HANDOFF.md.
- **Head-look parks after a missed menu close event** ("F9 fixes it") [D1a]. Windowed mode
  keeps the desktop cursor visible, so a ghost menu flag was cleared by neither automatic
  rescue. The author's 39.4 fix is being ported. Plain F9 clears it meanwhile.
- **Weapons "wiggle" after a weapon swap** [D1a]. Calibration records were keyed by component
  pointer, so a swap re-probed mid-combat with inverted signs. The author's 39.0 fix (bank by
  asset name) is being ported.
- **Hands rotate with your head** [D5]. The first-person arms are placed by a camera-space
  LookAt control in Arkane's animation tree; the mod draws its own hand models for the
  weapons and drives the engine's hand bones, but the coupling is not fully removed. The
  `[Hands] GraftHeadFollowYaw/Pitch` sliders (default 1.5) compensate.
- **The prologue cutscene is broken** [D6]: the boat arrival blocks with a Block prompt. The
  mod jumps straight to the prison (IntroSkip). Start a new game, then continue from the
  prison save.
- **Cutscene cameras are fixed** (no head-look) [D6].
- **Possession, Devouring Swarm and Windblast are still head-aimed** [D7]. Blink, the
  crossbow, the pistol and grenades are hand-aimed.
- **Some dynamic lights render inconsistently per eye; thin fast-swinging objects shimmer
  between the eyes** [D8]. The fork's per-eye light and shadow fixes cover most passes, not all.
- **Menus sometimes shrink onto your wrist** (the wrist HUD redirect catches a menu draw) [D8].
- **Vents, crouch and Blink can glitch** (collision cylinder writes) [D8].
- **`[VR] StampFix` does nothing** on builds made from this repository: the fork export it
  reads (`dxvk_vr_view`) was never in the published patch series [D3].
- **The default `dishonored_vr.ini` does not list every key** the mod reads (`[VR]`, `[Hands]`,
  `[Blink]`, `[Hud]`, `[Overlay]`...); the keys work when added by hand [D0, config table].
- **The desktop window must be 4032x2268 windowed** (run `setup_resolution.bat` or
  `setup-game-ini.ps1 -Resolution`); the mod holds that size against the window manager.
  A monitor smaller than the window is fine.
- **GOG version unsupported** (different exe; every hook address is for the Steam build).
- **Motion Blur must be off** in the game's options.
