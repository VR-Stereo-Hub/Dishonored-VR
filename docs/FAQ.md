# Dishonored VR - Frequently Asked Questions

Answers for version **1.0.4**. The questions are ordered by how often they came up in the
community thread between the 1.0.0 release and 1.0.4, most asked first. Where something was
a problem in an early release and has since been fixed, the answer says which version fixed
it, so update before anything else.

Download: [GitHub releases](https://github.com/VR-Stereo-Hub/Dishonored-VR/releases) |
Full guide: [README](../README.md) | What changed: [release notes](RELEASE_NOTES.md)

## The short version

Most problems in the thread came down to one of these five answers:

1. **Use the Steam or GOG version.** Epic and Game Pass are a different, 64-bit game and will not work.
2. **Set your headset to 120 Hz or 144 Hz.** The mod runs worse at lower rates, not better.
3. **Turn on DLAA or DLSS for a clean image.** They remove jagged edges. They do not add frames in this game.
4. **Update with the launcher.** Shrinking view, HUD off the edges, one-eye flicker and the spyglass were all fixed in later releases.
5. **Something still wrong? Press `Collect logs` in the launcher** and send the zip with a short description. A log is the only way a fault on your PC can be fixed.

## Contents

- [Getting started](#getting-started)
- [Picture quality and performance](#picture-quality-and-performance)
- [Display problems](#display-problems)
- [Combat and controls](#combat-and-controls)
- [Other mods and add-ons](#other-mods-and-add-ons)
- [Everything else](#everything-else)

---

## Getting started

### Which version of Dishonored do I need?

**Steam or GOG.** Both the standard and the Definitive / Game of the Year edition work.

**Epic Games Store and PC Game Pass do not work, and are not planned.** Those stores sell a
64-bit build of the game, while Steam and GOG sell the original 32-bit one. The mod is built
around the 32-bit game, so supporting the 64-bit one would mean porting the whole mod. If
you only own it on Epic, the game is often on sale for a few dollars on Steam, GOG and key
sites.

GOG has been supported since 1.0.1: the launcher finds a GOG install and launches it
through Galaxy.

### How do I install it?

1. Download `DishonoredVR-Launcher-v1.0.4.exe` from the
   [releases page](https://github.com/VR-Stereo-Hub/Dishonored-VR/releases).
2. Run it. It finds the game on its own; if not, point it at the game folder.
3. Pick your headset and render quality. The defaults are a good start.
4. Press **Install**, then **Launch via Steam** (or Galaxy for GOG).

You only need the launcher. The zip on the release page is for people who prefer to copy
files by hand, and the README explains that route. If Windows Defender deletes the download,
it is a false positive on an unsigned exe; restore it from Defender's protection history.

### Which headsets work?

| Headset | How to run it |
|---|---|
| Quest 2 / 3 / 3S / Pro | **Virtual Desktop with VDXR** as its OpenXR runtime. This is the most tested path. Pick "Quest via Virtual Desktop" in the launcher. |
| Index, Vive, Pimax, WMR, Bigscreen Beyond | Start SteamVR first, pick "SteamVR headset" in the launcher. The mod uses its own SteamVR bridge. |
| Quest over Link / Air Link / Steam Link | Works through SteamVR ("SteamVR headset"). Virtual Desktop is smoother. |
| Pico, PSVR2, Play for Dream | Reported working through SteamVR or Virtual Desktop. Not officially tested. |
| Steam Frame | Untested. |

Recommended refresh rate: **120 Hz**, or **144 Hz** with the Virtual Desktop beta.

### The launcher is set to Virtual Desktop but SteamVR opens (or the other way round)

"Let the mod choose" uses whichever 32-bit OpenXR runtime Windows has registered, which is
often SteamVR. Open the launcher, choose your headset explicitly, and press **Apply**. 1.0.4
also fixed SteamVR being ignored when the game was run as administrator.

### The game opens flat on my monitor and nothing appears in the headset

- Make sure you installed through the launcher and the install finished. A half-finished
  manual install is the most common cause.
- Start SteamVR (or connect Virtual Desktop) **before** launching the game.
- Launch through Steam or GOG Galaxy, not by double-clicking `Dishonored.exe`.
- If it still runs flat, press **Collect logs** and send the zip. The log says why VR did not start.

### Does it work on Linux / Steam Deck / SteamOS?

No. Linux is not supported, including through Proton with WiVRn.

### Can I launch from a desktop shortcut or `Dishonored.exe` directly?

Launch through Steam (or Galaxy for GOG). Starting the exe directly has crashed at the main
menu since the original mod. The launcher's **Launch via Steam** button and its desktop
shortcut both go through Steam.

### How do I update?

Open the launcher and press **Update**. From 1.0.1 on the launcher checks for new releases
itself. "Overwrite INI" (on by default) replaces your settings with the new tested defaults
and keeps a backup of your old file; untick it to keep your own settings. To go back to an
older version, run that version's launcher.

### How do I play flat again, or remove the mod?

In the launcher, **Disable VR** keeps the mod installed but lets the game run flat; **Enable
VR** turns it back on. **Uninstall** removes it and restores any `d3d9.dll` it replaced.

### Do the DLCs work (Knife of Dunwall, Brigmore Witches)?

They run and have been played through on GOG without trouble, but the mod was not designed
around them. Daud's powers are close to Corvo's, so most things work. Report anything broken
with a log.

---

## Picture quality and performance

### The image looks jagged, shimmery or blurry. How do I make it sharper?

This is the most asked question about the picture. Dishonored's own anti-aliasing is weak,
and its fine detail shimmers in a headset. Since 1.0.2 the mod has real fixes:

1. **Turn on DLAA or DLSS** (NVIDIA) or **FSR** (AMD): F10 > Display > *Upscaling and
   anti-aliasing*, or in the launcher next to resolution (1.0.4). DLAA removes aliasing almost
   completely. DLSS Quality or Ultra Quality is lighter and still very clean.
2. **Add sharpening.** CAS sharpening is in F10 > Display > *Sharpness, texture filtering and
   other anti-aliasing*. ReShade's sharpening (1.0.3) also works well.
3. **Raise the resolution** if your GPU has room. The launcher goes up to Ultra (150%) and F10
   up to 300%. DLAA at a moderate resolution usually looks better than a huge resolution
   without it.

Lowering the field of view makes the image look sharper too, but it narrows your view, so
it is a last resort.

### Why doesn't DLSS give me more FPS?

In this game DLSS and FSR are **anti-aliasing, not a frame-rate boost.** Dishonored is a 2012
game that is light on a modern GPU, so drawing fewer pixels saves very little. On top of
that, DLSS cannot run inside a 32-bit DirectX 9 game, so the mod runs it in a separate helper
program and passes every frame across, which costs time. Expect a cleaner picture at a
similar or slightly lower frame rate. If DLSS makes you slower, lower the resolution or pair
it with AFW (below).

### What refresh rate should I use?

**120 Hz, or 144 Hz with the Virtual Desktop beta.** It sounds backwards, but the game runs
*worse* at a lower cap: a PC that holds 100 fps at 120 Hz can drop under 72 when set to
72 Hz. The game is not locked to 60 internally. If you get stutter at 72 Hz with Virtual
Desktop's SSW, try 120 Hz with SSW off.

### What settings do the developers use?

On an RTX 4070 Ti Super with a Ryzen 5600X: 100% resolution, **AFW** rendering, **DLSS Ultra
Quality, preset K**, plus sharpening. That gives roughly 110-130 fps at 144 Hz. Start there and
adjust for your PC. The game leans on the CPU more than the GPU, and it has run smoothly on
cards as old as a GTX 1650 at the Performance setting.

### What is AFW, and should I use it?

**Alternate Frame Warping** (1.0.3) renders one eye per frame and rebuilds the other eye from
depth, so the game does about half the work. On the test PC it went from about 110 to a full
144 fps, and DLAA from 50-70 to about 120.

- Turn it on in the launcher (*Rendering method*), or live in F10 > Display > *Rendering
  method: Stereo or AFW*.
- **Always use it with DLSS or DLAA.** Without them it looks noticeably worse.
- It is still marked experimental. Known artefacts: flicker on held objects, a ghost when one
  eye sees a surface the other cannot (very close objects), and a one-frame lag between the
  eyes while the game moves you, such as on a boat. 1.0.4 made it smoother.
- If it bothers you, go back to **Stereo**, which is still the default.

### My frame rate is terrible (single digits) or one eye stutters

- Check the refresh rate first (above).
- One eye stuttering on slower GPUs started in 1.0.2 and was **fixed in 1.0.4**.
- A big slowdown on some PCs was fixed in 1.0.2.
- Still slow? Press **Collect logs** and send the zip. Several slowdowns in the thread were
  fixed from exactly those logs.

### Can I see the game on my monitor?

The desktop window is **off by default** because it costs performance. Turn on *Desktop
mirror* in the launcher or in F10 > Display > *Desktop window*. If SteamVR crashes at startup,
turning the mirror on is also the fix for that.

### How do I record a video?

OpenXR capture layers such as OpenXR-OBSMirror are 64-bit and cannot see this 32-bit game.
Record SteamVR's *Display VR View*, Virtual Desktop's recorder, or the Quest's own recording.
Note that a headset recording crops the edges, so HUD items near the edge may not show.

---

## Display problems

### My view shrinks into a small box, or I get black bars

This was the most reported bug in 1.0.0 and it is **fixed**: 1.0.1 fixed the gradual shrink,
1.0.2 fixed it after dying or leaving a menu, and 1.0.4 fixed it when a conversation opens a
shop. Update first.

If you still see black bars top and bottom, open F10 > Display > *Resolution*, pick 100% and
press **Apply**. The game was rendering at the monitor's shape instead of the headset's, and
re-applying fixes it. A save and reload also clears a one-off occurrence.

### HUD text and pop-ups sit at the edge of my view and I can't read them

Fixed across 1.0.2 and 1.0.4: the HUD was reworked so it cannot come loose, and its default
position is more central. It is worst for glasses wearers and anyone with a narrow view.

To adjust it yourself: F10 > **HUD**. Every element can be off, in a window in front of you,
fixed in the world, or on either hand. Health and mana live on your wrists (raise your hand
to read them), and potion counts and the D-pad shortcuts show when you hold the left grip.

### Things pop in and out in one eye, or the stereo breaks when I look away during a cutscene

Both were in 1.0.0 and both were **fixed in 1.0.2**. The game was hiding objects it thought
were out of sight for one eye only; each eye now decides for itself. If you see an enemy
briefly as a dark silhouette when your sword covers them in one eye, set F10 > Display >
*Object culling* to Off (costs a little performance).

### I see flicker

There is no single flicker bug. Many separate kinds were found and fixed during development,
and 1.0.4 fixed the last known one-eye sideways flicker. If you still see it:

- With **AFW** on, some flicker is a known trade-off (see AFW above). Switch to Stereo to compare.
- Otherwise, press the **V** key on the keyboard the moment it happens. It drops a marker
  into the log. Then **Collect logs** and say whether the flicker was on the hands and weapon,
  the world, or both, which eye, and which direction it jumped.

---

## Combat and controls

### My sword swings don't hit anything

The motion sword decides *when* you attack; the game still decides *where*, along your aim.
If you play standing with your left hand hanging at your side, the aim points at the floor
and the swing goes there. Keep your left hand roughly pointed forward, or switch melee to
head aim in F10 > **Aim** > *Aim with the controller or your head, per weapon*. Swing speed is
adjustable in F10 > Controls.

### Will there be real collision-based sword fighting?

Planned, not in 1.0.4. The goal is a sword that hits with the blade model itself, like the
wrench in the BioShock VR mod. Development has moved back to the BioShock trilogy for now.

### How do I do drop takedowns and stealth kills?

- **Stealth kill:** come up behind an unaware enemy and stab forward.
- **Drop takedown:** drop onto an enemy from above and stab downward, like stabbing a table.
  Crouching first makes it easier to land. The mod holds your attack until the game has found
  the enemy below, so the takedown plays instead of a plain slash.
- **Choke:** behind an enemy, hold the right grip.

### Can I snap turn?

Yes, since 1.0.2. Off by default: F10 > Controls > *Turning*, or in the launcher. The step
size is adjustable (45 degrees by default).

### Can I remap the buttons? I want X and Y swapped, or jump on the stick

Yes. F10 > Controls > *Button mapping* (1.0.2), or the launcher's *Button mapping* section
before you start the game (1.0.3). SteamVR's own binding editor also works for SteamVR
headsets.

### Menus or the weapon favourites keep popping up, or I stop walking for no reason

That is the **right thumbrest**. Touching it turns the left stick into the item-shortcut D-pad,
so a resting thumb can open shortcuts or stop you moving. Pico and Index controllers have
thumbrest sensors too. Change the shortcut button (to the right stick click, the left
thumbrest, or off) in the launcher's *Controller shortcuts* or F10 > Controls.

### My Index controller grips are far too sensitive

Fixed in 1.0.2: the mod has Index tuning with a force-sensor grip. It turns on when you pick
Valve Index, Bigscreen Beyond or Vive Pro 2 as your headset in the launcher. You can also
raise the grip force in SteamVR's binding editor (set the grip to a force or "grab" input and
increase the threshold).

### I keep sliding when I try to stand up

Fixed in 1.0.2: *Slide assist* is off by default. Sliding now works the way the game does it:
sprint (click the left stick), then press B.

### Picking up coins and loot is fiddly / chokes need precise pointing

1.0.4 added **physical grabbing**: reach out and squeeze the grip to take loot, read books,
open doors, pull levers and pick things up. Each hand grabs for itself. Settings are in F10 >
**Interact**. The Interact button still works.

If you prefer to look rather than point, set interactions to head aim in F10 > Aim.

### How do I use the spyglass (mask zoom)?

Fixed in 1.0.4: hold the right controller up to your right eye and pull the trigger, or tap
the right stick click.

### How do I drink a health or mana potion?

Hold the right stick click for about half a second to drink a health elixir. Or hold the
left grip to open the wheel, where the potion counts show in the corner: B drinks health and
X drinks mana. The game's own low-health prompt works too.

### I'm left-handed / I want to play with a gamepad

There is no full left-handed mode yet. The launcher's *Button mapping* section has **Swap
move and turn sticks**, and buttons can be remapped. Playing with a real Xbox controller is
not an official option. Setting `[Controllers] Enabled=0` in `dishonored_vr.ini` stops the
mod's controller emulation so a real pad can reach the game, but that setup is untested.

### How do I open the settings menu in the headset?

Yes. Tap **both stick clicks** together to open the F10 panel in the headset. Point with your
right hand and click with the trigger. Basic mode (the default) shows only the common
settings; Advanced and Debug add the rest. Hold both stick clicks for half a second to
recenter.

---

## Other mods and add-ons

### Does ReShade work?

Yes, since 1.0.3, and 1.0.4 fixed the remaining start-up problems.

1. In the launcher, open the **Mods** section and install **ReShade** (6.8). It sets
   everything up; do not install ReShade over the mod's `d3d9.dll` yourself.
2. Enable it in F10 > **ReShade**, which also picks the preset and adjusts its effects.
3. **Scroll Lock** turns effects on and off in game.

Presets that need depth require `[ReShade] ManualRuntime=0` in `dishonored_vr.ini` and a
restart.

### Do HD texture packs work? The game crashes with one

The crash is the 32-bit game running out of its 4 GB of address space, not a bug in the pack.
Since 1.0.3 there is a fix: tick **Texture pack compatibility** in the launcher (or F10 >
Display > *Texture pack memory*) and restart. The launcher's Mods section links to HD Texture
Pack 2.0 and its install steps. Very high resolutions plus a 4K pack can still be too much;
lower one of them if it crashes.

If the game crashes before the main menu after installing other mods, clearing the game's
shader cache and retrying has helped.

### Do other Nexus mods work?

Texture-only mods generally work. Mods that replace the player's arm or body meshes may not,
because the mod cuts and rebuilds the arms. *High Quality Player Arms* has been reported
working. The *CorvoBody* full-body mod crashes in VR for now.

### Does it support bHaptics?

Not yet. It has been requested several times and is on the wish list.

---

## Everything else

### Something went wrong. How do I report it?

This is the answer the maintainers gave most often in the thread, by far.

1. Reproduce the problem, then **don't relaunch the game yet**.
2. Open the launcher and press **Collect logs**. It saves a zip to a `DishonoredVR Support`
   folder on your desktop. The mod keeps the last ten sessions, and the zip stays under
   Discord's upload limit.
3. Post the zip on the [issue tracker](https://github.com/VR-Stereo-Hub/Dishonored-VR/issues)
   or in the Discord thread, with what you were doing, roughly when, your headset and your GPU.

A log turns "it doesn't work" into a fix. Most of the 1.0.1 to 1.0.4 fixes came from logs
players sent in.

### The motion during takedowns makes me queasy

The game's camera shake and head bob are off by default, and each can be turned back on in
F10 > Comfort. During takedowns the game itself moves your body, which the mod cannot fully
stop. F10 > Hands can hide the arms during takedowns.

### Can I see my own body or shadow, or use holsters?

Not in 1.0.4. 1.0.4 added full arms that follow your controllers (F10 > **IK** to fit them to
your body, or turn them off for floating hands). A full body, a player shadow and holsters are
requests for the future.

### Is the mod finished?

1.0.4 is the last planned feature update for now. There may be a hotfix for serious problems,
so keep sending logs. Development has moved to the BioShock trilogy VR mod, which will get
the features built here, and to a new project.

### What about Dishonored 2?

Dishonored 2 runs on a different engine and is not part of this mod. A separate community
project is working on it; look for its own thread.

### Can I support the developers?

The mod is free and always will be. If you want to, you can donate on
[Ko-fi](https://ko-fi.com/pizzzaparker). It goes into development of this mod and the ones
after it.
