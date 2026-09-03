# Troubleshooting

First: find your log. It is `dishonored_vr.log` in the game folder (next to `Dishonored.exe`,
`...\steamapps\common\Dishonored\Binaries\Win32\`). The previous run is
`dishonored_vr.prev.log`; a crash also writes `dishonored_vr_crash.txt`. Send the log with any
report; the first lines say which build you run and which OpenXR runtime answered.

**The game does not start / "d3d9.dll" error dialog.** The proxy is the 32-bit build for the
Steam version. Delete `d3d9.dll` to get the stock game back (or rename it and keep the rest).
An older release's `dxvk_d3d9.dll` next to it is ignored; delete it too.

**Flat game, no VR.** Read the `xr:` lines near the top of the log:
- `xr: instance ... runtime '<name>'` says which OpenXR runtime answered. A Quest through
  Virtual Desktop should say VDXR; a SteamVR rig should say `DishonoredVR SteamVR shim`.
- No runtime at all: on a Quest, start Virtual Desktop streaming FIRST and make sure VDXR is
  the active OpenXR runtime (the VD Streamer sets it). On a SteamVR rig, `dvr_steamvr32.dll`
  and `openvr_api.dll` must sit next to `d3d9.dll` (the installer puts them there) and
  SteamVR must be running; the shim writes its own log to
  `%LOCALAPPDATA%\DishonoredVR\ovrshim.log`.
- `xr: session created ... waiting for READY` and nothing after: the headset is not streaming
  or not worn; the mod keeps retrying.
- `xr: pipeline READY` then `stereo: beat method=mono ... none/s=60`: the runtime is fine but
  no frame reaches it. Look for `capture:` and `mono:` lines: `backbuffer format ... not
  handled` or `GetRenderTargetData failed` (multisampled backbuffer: turn the game's AA off)
  name the cause.
- `disable_vr.txt` exists next to the exe: the kill switch is on.

**The screen is too close, too big, or off-centre.** F10 > `screen distance (m)` and `screen
width (m)` (`[Screen] DistanceMeters`, `WidthMeters` in the ini). F5 recenters.

**The world looks cropped or stretched.** The log's `capture: WxH content bbox ... (FULL)`
line says whether the game draws its whole window; `CROPPED` with a box smaller than the
window means the game rendered into a corner or a band (a resolution the game's video options
do not really support). Pick a standard size in the game's video options.

**Head tracking stops after a save load.** F9 forces gameplay mode (re-arms the script hook);
F5 recenters. The `[HeadTrack]` section has the fallbacks.

**Stutter.** Motion Blur off. `[VR] FpsCap` pins the game to the display rate (72) or half of
it (45 at 90 Hz) for an even cadence. The `heartbeat:` lines show game fps vs headset
submits; the per-frame capture costs more at a bigger window, so try 1920x1080.

**A crash when quitting.** Send `dishonored_vr.log` and `dishonored_vr_crash.txt`; the
session teardown at exit is best-effort (docs/KNOWN_ISSUES.md).

**Reset everything.** Delete `dishonored_vr.ini`; the mod writes a fresh one.

**Reporting.** Attach `dishonored_vr.log` (and `dishonored_vr_crash.txt` if present), say
headset, runtime (Virtual Desktop / SteamVR / Link), **GPU vendor, model and driver version,
and whether the PC has a second GPU** (integrated graphics counts; the log's `adapter:` lines
say which one the mod used), and what you did. Developers: `tools\log-parse.ps1` summarises
a log; `tools\status-dump.ps1` reads the live state; `tools\game-cmd.ps1 "stereo status"`
and `"camera status"` print the two seams.

**The world is soft or blurry in stereo.** Sharpness is set by how many pixels per degree the
frame carries at the centre, and a 16:9 frame must claim 137 deg to cover the eye, so most of
it lands outside your view. Pick the runtime's per-eye size on the F10 Display tab (the
default entry), tick `VirtualMode` if the log says the size is not a display mode, Apply, and
relaunch: the log must then say `res: HONOURED - the game renders 2496x2688` (or your
headset's size). If it says NOT HONOURED, read the `res:` lines above it: `launch: command
line extended` must be there (the ask travels in `dishonored_vr_launch.txt` next to the exe;
`res 0x0` clears it). At that size set `[Capture] Mode=deferred` - the shipped `sync` readback
costs ~18 ms per present there and halves your frame rate.

**One eye stops updating after a pause.** Press F10, Runtime tab, tick `Strict pairs`, and
send the log: the `STALE R EYE` line in it names which side dropped the sibling frame.

**Looking up or down feels like the whole body pitches.** F10 Comfort, `neck (pitch pivot)`:
press `cancel`. The engine moves its camera on its own neck arc (measured 32 cm below the
eyes); the cancel keeps the eye where the headset says it is.

