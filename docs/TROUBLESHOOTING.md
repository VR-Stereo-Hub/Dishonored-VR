# Troubleshooting

First: find your log. It is `dishonored_vr.log` in the game folder (next to `Dishonored.exe`,
`...\steamapps\common\Dishonored\Binaries\Win32\`). The previous run is
`dishonored_vr.prev.log`; a crash also writes `dishonored_vr_crash.txt`. Send the log with any
report; the first lines say which build you run and which OpenXR runtime answered.

**The game does not start / "d3d9.dll" error dialog.** The proxy is the 32-bit build for the
Steam version. Delete `d3d9.dll` to get the stock game back (or rename it and keep the rest).
An older release's `dxvk_d3d9.dll` next to it is ignored; delete it too.

**It flickers for the first few seconds after a level loads, like the two eyes are
alternating.** Known, and it clears itself. While the level streams in, the game cannot keep up
with the headset's refresh, so one eye gets fresh frames far more often than the other - which
is exactly what alternating eyes look like. The moment the frame rate reaches the display rate
it locks and stays locked for the session. It is usually a few seconds; once it has been seen
to last around 25. If you want to try a workaround, `vrpace strict on` on the command seam
should trade the flicker for a briefly flat (non-stereo) picture while it lasts - it is
untested, so please say whether it helps.

**Doubled edges / ghosting when you turn your head. SET YOUR HEADSET TO 90 Hz.** This is the
single most effective setting in the mod and it is not in the mod - it is in your streaming
app (Virtual Desktop's refresh rate, or your runtime's). The mod renders a frame in roughly
11 ms at the shipping size; at 90 Hz a display slot is 11.11 ms, so one frame fills exactly one
slot. At 120 Hz a slot is 8.33 ms, so frames land 1.05-1.11 slots apart, one frame in nine is
held on screen for an extra slot, and consecutive frames shown for different durations is
exactly what a doubled edge looks like when you turn. Measured on a Quest 3 over Virtual
Desktop, ghosting was still reported at 120 Hz and none was reported at 90. **A higher refresh
rate is worse here, not better.**

Check it yourself in the log - `stereo: rate` prints a line every 3 seconds:

- `EVEN CADENCE: 1.00 display slots per frame` is what you want.
- `UNEVEN CADENCE: 1.11 display slots per frame ... one frame in 9 is held an extra slot` names
  the beat and the refresh rates that would divide cleanly at your current period.

If 90 Hz still reads UNEVEN, your frames are taking longer than one slot: drop the render size
(F10 Display, or `res 2600x2700`, which takes effect at the next launch).

**The frame rate dips to 60 at 90 Hz even though it never dipped at 120 Hz.** Expected, and not
a regression. At 120 Hz the game was never hitting a display slot at all - it free-ran and the
compositor smeared over the difference, which is what produced the ghosting. At 90 Hz it hits
its slot, so a frame that runs even slightly long misses and waits a whole period, which shows
as a hard dip instead of a smooth blur. The stall rate is unchanged (measured: 27.6 per minute
at 120 Hz, 28.4 at 90 Hz); the stalls are simply visible now. If the dips bother you more than
the ghosting did, drop the render size a step to buy headroom.

**Long freezes of 50-100 ms, several times a minute.** Look for `perf: frame gap` in the log
and read its `sat in:` field. `sat in: present-tail (xrEndFrame)` means the time was spent
inside the call that hands the frame to your headset - on a wireless link that is the video
encoder or the Wi-Fi, not the game. Raise the streaming bitrate, try a different codec, move
closer to the router or use a dedicated 5/6 GHz access point, and check nothing else is
saturating the link.

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

## The game launches flat and the log says `xrCreateInstance failed: XrResult(-32)`

`-32` is `XR_ERROR_FILE_ACCESS_ERROR` and it comes from the OpenXR **loader**,
before a runtime is even chosen. That is why it takes down the native runtime
and the SteamVR shim with the same error, and why the shim's "is SteamVR
installed?" line is misleading when it appears - SteamVR is not the problem.

The usual cause is an **implicit OpenXR API layer whose library is 64-bit only**.
Dishonored is a 32-bit game, so a 64-bit layer cannot load into it, and one
unloadable layer fails instance creation for everything.

Implicit layers are opt-out by design: nothing you configured asks for them.
They are registered under

    HKCU\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit
    HKLM\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit
    HKLM\SOFTWARE\WOW6432Node\Khronos\OpenXR\1\ApiLayers\Implicit

with a value of **0 meaning enabled** and non-zero meaning disabled. `HKCU` is
not subject to WOW64 redirection, so a layer registered there reaches a 32-bit
process even when 64-bit software installed it.

**The mod handles this itself.** On startup it reads those keys, checks each
enabled layer's library for its PE machine type, and for any layer that cannot
load here it sets the `disable_environment` variable that layer's own manifest
declares - in this process only. It never writes the registry, so the layer
keeps working for your 64-bit applications. The `apilayer:` lines in the log say
what was found and what was switched off.

Two cases it cannot fix on its own, both named in the log:

- a layer whose manifest declares no `disable_environment`: there is no
  supported way to opt one process out of it, so it has to be disabled in the
  registry (set its value to 1) or uninstalled;
- a layer whose manifest will not read at all.

Known example: OBS mirror-capture layers such as OpenXR-Layer-OBSMirror install
a 64-bit-only library under `HKCU` and will stop this game reaching VR until
they are disabled.
