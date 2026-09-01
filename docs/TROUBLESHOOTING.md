# Troubleshooting

First: find your log. It is `dishonored_vr.log` in the game folder (next to `Dishonored.exe`,
`...\steamapps\common\Dishonored\Binaries\Win32\`). The previous run is
`dishonored_vr.prev.log`; a crash also writes `dishonored_vr_crash.txt`. Send the log with any
report; the first lines say which build you run, which backend it picked and why.

**The game does not start / "d3d9.dll" error dialog.** The proxy is the 32-bit build for the
Steam version. Check `dxvk_d3d9.dll` and `openvr_api.dll` sit next to it. Delete
`d3d9.dll` to get the stock game back (or rename it and keep the rest).

**Flat game, no VR.** The log's `config: VR backend ...` line says what the mod chose:
- `AUTO -> OPENVR` on a Quest: Virtual Desktop was not streaming when the game started, or
  VDXR is not the active OpenXR runtime. Start VD streaming first, or set `[VR] Backend=openxr`
  in `dishonored_vr.ini`.
- `probe: no 32-bit OpenXR runtime could be negotiated`: SteamVR registers no 32-bit OpenXR
  runtime, that is expected on a SteamVR rig (the OpenVR path is used). On a Quest the
  32-bit VDXR runtime must be installed (Virtual Desktop Streamer does that).
- `xr: no HMD yet (headset off / VD not streaming?)`: the headset is not streaming; the mod
  keeps retrying.
- `disable_vr.txt` exists next to the exe: the kill switch is on.

**Wrong window size / zoomed view.** `ResX=4032 ResY=2268 Fullscreen=False` must be in
`Documents\My Games\Dishonored\DishonoredGame\Config\DishonoredEngine.ini`
(`setup_resolution.bat` does it). The log's `res:` lines show what the game asked for and
what it got; a `Reset to 3854x1071`-style line means the window manager clamped the window
to the monitor before the mod's hooks were in place.

**Quest: image warps when I turn my head, or the view locks after a load.** Known issue of the
OpenXR path (docs/KNOWN_ISSUES.md). Things to try, one at a time, and report which helped:
`[VR] StampLive=1` (default), `[VR] XrPoseDelay=0..3`, `[VR] XrLayer=proj|cyl|quad`,
`[VR] FpsCap=72` (or 45 at 90 Hz), SSW off in Virtual Desktop, 72 Hz.

**Head tracking stops after a save load.** F9 forces gameplay mode (re-arms the script hook);
F5 recenters. The `[HeadTrack]` section has the fallbacks.

**Hands are wrong / in my face.** END recalibrates the hands (hold your hands where the game's
are). HOME toggles the hand drive. `[Hands] GraftHeadFollowYaw/Pitch` tune the residual
head coupling.

**Stutter.** Motion Blur off. On Quest, `[VR] FpsCap` pins the game to the display rate (72)
or half of it (45 at 90 Hz) for an even cadence. The `heartbeat:` log lines show game fps vs
submits.

**Reset everything.** Delete `dishonored_vr.ini`; the mod writes a fresh one.

**Reporting.** Attach `dishonored_vr.log` (and `dishonored_vr_crash.txt` if present), say
headset, runtime (SteamVR / Virtual Desktop / Link), GPU, and what you did. Developers:
`tools\log-parse.ps1` summarises a log; `tools\status-dump.ps1` reads the live state.
