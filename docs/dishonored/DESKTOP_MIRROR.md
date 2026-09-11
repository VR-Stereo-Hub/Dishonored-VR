# The desktop mirror, and the eye policy the game window never had

Reference for `VR-53` (the alternating game window) and `VR-54` (the pause-menu
session loss). The hypothesis graveyard that led here is
`docs/dishonored/BRIEF-eye-flicker.md`, and it is worth reading before
proposing anything new in this area: four hypotheses were argued and killed
before the actual cause was found, three of them from counters that could not
see the fault.

Modules: `src/core/gfx/desktop_eye.cpp` / `.h`, with call sites in
`src/core/vr/openxr_runtime.cpp`.

---

## 1. The fault, stated exactly

`hkPresent` calls the game's original `Present` for **every eye draw**. Under
sequential stereo that is two presents per tick, one per eye. `mirror_present()`
in the runtime layer was supposed to be what decided which of those the desktop
window shows - and it **had never implemented the D3D9 copy**. Its own comment
said so. It incremented counters and returned.

So the game window showed, frame by frame:

```
L(k), R(k), L(k+1), R(k+1), ...
```

while the headset received correct pairs the whole time. A recording of the
window alternates between two camera positions one IPD apart, which is exactly
what alternate-eye rendering looks like on a flat screen.

**The headset was never doing alternate-eye rendering.** The desktop simply had
no eye policy at all. That distinction is the whole finding: the diagnosis had
been aimed at the headset path for several sessions.

## 2. The pin and the delivered-eye correction (VR-76)

The original implementation snapshots on a delivered left tag and re-blits on
right. That tag identifies the headset texture. With deferred capture or shared
capture at `SharedWait=0`, it describes the previous present, while the pin reads
the current D3D9 backbuffer. A steady stream therefore pins RIGHT, and a zero
delivered tag after a single-draw tick lets a raw LEFT frame reach the window.
Sync and shared at `SharedWait=1` have current-frame delivery.

`[VR] DesktopEyeSource=draw` instead snapshots a resolved current left draw and
re-blits on right or up to three consecutive unknown draws. The fourth unknown
invalidates the pin and releases menus/loads. A new surface, a source toggle or
a failed snapshot requires a successful left snapshot before re-blitting. The
first right frame after such a boundary can pass through; it is counted as warmup.

The tree default is `tag` pending user testing. Live A/B is `desktopeye draw|tag`;
`desktopeye status` reports the resolved policy. Save As Defaults persists it.
The existing `vrmirror on|off` gates the runtime hook separately. The old
`[VR] DesktopEye` key was documented but never parsed; it is not a working ini
switch. `desktopeye on|off` controls the host copy module live.

**The runtime layer owns WHEN; the desktop module owns HOW.** Each present
reaches one of three post-capture hook sites: no XR frame open, first eye held
open for pairing, or normal completion. All permit a delivered 0. The early
left-only call was removed because draw mode can re-blit even when the delivered
tag is left. Every hook precedes `composite_hud`, which is currently a no-op.
The intervening runtime capture work operates on D3D11 textures, not the current
D3D9 backbuffer, so the later snapshot still reads the intended game pixels.

The surface is `D3DPOOL_DEFAULT`, released before the game's Reset. Held-image
validity belongs to that surface, not to lifetime snapshot counts. Reset, resize,
device changes and source switches invalidate it.

Implementation details, review corrections, hashes, validation and pending tests:
[VR-76 handoff](VR-76-CODEX-HANDOFF.md). This is an installed candidate fix with
no headset verdict yet.

## 3. The pause-menu session loss (VR-54)

Found in the same review, in the same file, and fixed in the same change
because they are one reading of the frame path.

On a **hold-only present**, `proj` / `projViews` / `quad` are the EMPTY locals.
The copies actually submitted are `holdProj` / `holdViews` / `holdQuad`. The
hold sets `layerCount = 1`, and the snapshot bank keyed off `layerCount` - so
it overwrote a good snapshot with zeroed structures and left it **marked
valid**.

The next hold then submitted null handles and a zero view count, `xrEndFrame`
answered `XR_ERROR_HANDLE_INVALID`, and the session stood down. Both logs from
2026-09-06 show exactly that sequence at the pause menu.

The bank is now keyed on `builtNewLayer` - whether a layer was actually built
this frame - rather than on a count that a hold also sets.

### What this does NOT fix, and why it cannot

A saved layer holds swapchain **handles**, not pixels, and OpenXR composites
the most recently RELEASED image from a swapchain. So a held structure does not
preserve the picture that was in it; preserving a completed PAIR needs retained
images, not a retained structure. The hold is correct as a hold. It is not a
freeze-frame and cannot be made into one without holding images. Tracked
separately.

## 4. Retracted here: the "30 % of ticks double" reading

The stand-down guard built on that reading is **REMOVED, not tuned**, and this
is recorded because the reasoning error is more useful than the fix.

The window that reading came from straddled a pause menu, an `xrEndFrame`
failure, and session teardown. The windows either side of it read
`draws/s=78 2nd/s=78`, `86/86`, `87/87`, `81/81`. The renderer doubles
essentially every gameplay tick. The refusals inside the bad window were "no XR
session" - counted *after* the session was already gone, for the reason in
section 3.

A guard built on that premise would have disarmed a healthy renderer every time
a session dropped, which is a fault that looks exactly like the one it was
built to prevent. **A counter is not evidence until you know its population**,
and the population here was "frames during a session teardown".

The `camera/eyetrace` line is corrected in the same change: its ring is
appended in `note_render_pos`, so it samples **constant uploads**, not
presents. A full eye-to-eye span across that ring is what healthy sequential
stereo produces. The line previously said otherwise and that was wrong.

## 5. The API layer guard (shipped on the same branch)

Unrelated fault, same lane. The game is 32-bit. An implicit OpenXR API layer
installed for the **64-bit** runtime is still enumerated for this process, the
loader tries to load a DLL of the wrong bitness, and `xrCreateInstance` fails
outright - no session, no VR, and an error that names the offending layer only
if you already know to look.

The guard disables layers that cannot apply to this process before the instance
is created, **names each one it turned off and why**, and says the cost of
running out loud rather than being invisible. `[VR] DisableBadApiLayers`,
default ON, because suppressing somebody else's layer has to be reversible from
an ini when it guesses wrong.

## 6. Reading the log

**`desktopeye:`**, every 15 s, reports actual window deltas: current L/R/0,
delivered tag same/opposite/zero, single gameplay ticks, old-policy shadow
changes/raw leaks, successful copies, shown-right/unknown, warmup and failures.
Equal snapshot/blit counts cannot prove that the held eye is the correct one.
The shadow assumes successful copies; actual shown-eye values are inferred from
copy provenance, not measured pixels. See the handoff for the field definitions.
**`stereo: beat`** carries `L/s` and `R/s`, which **read 0 by design on the
mono screen** - the line says so on the line, because two heartbeat counters
reading zero by design were read as "the hands are dead" by three separate
readers including the original author.

**`XR_ERROR_HANDLE_INVALID` from `xrEndFrame`** is the VR-54 signature. It
should no longer appear at a pause menu.

## 7. What still needs a headset

Everything below. None of it is verified.

- The game window shows **one view, no alternation**, while the headset keeps
  correct stereo depth.
- Draw source active, real bursts and old-policy raw leaks observed, no unexpected right output or copy failures.
- Open and close the pause menu several times: no `XR_ERROR_HANDLE_INVALID`, no
  session teardown.

## 8. Not addressed here

Performance. Roughly 78 complete pairs per second against a 90 Hz headset,
about 9.7 ms of D3D9 GPU span per tick, 15.7 megapixels per pair at 2750x2850.
That is the next subject and it is not touched by anything in this document.
