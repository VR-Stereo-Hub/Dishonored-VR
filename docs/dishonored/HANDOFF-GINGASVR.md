<!-- The original author's handoff, written 2026-09-01 at their build 39.4 and given to
this project with the mod. Kept as received apart from the repo's hyphen rule (em and en
dashes replaced). Line numbers and paths refer to THEIR tree (one file, MinGW, G:\ drive),
not this repository; see docs/STATUS.md for how it maps onto ours. -->

# Dishonored VR - Handoff

Written 2026-09-01, at build **39.4**, by the Claude session that built most of
the 30.x-39.x line with GingasVR.

This is not a summary. It is everything that was measured, everything that was
disproved, and every trap that cost days. Read the **Traps** and **Dead ends**
sections before writing a line of code. Most of the wasted time on this project
was spent re-discovering things already in here.

---

## 1. What this is

A from-scratch VR conversion of **Dishonored (2012, Unreal Engine 3, D3D9,
32-bit)**. No source access, no SDK. Two binaries:

| binary | what it is |
|---|---|
| `d3d9.dll` | The proxy. A DLL that the game loads instead of the system D3D9. It hooks the engine at runtime, drives the headset, reads and writes UObjects, renders the overlay. **~22,800 lines in one file**, `src/dllmain.cpp`. |
| `dxvk_d3d9.dll` | A **fork of DXVK 3.0.2** (branch `lighting-m8`). Translates D3D9 to Vulkan, and is where per-eye stereo actually happens - the view-projection matrix is sheared per eye inside the fork. |

The proxy chain-loads the fork. The fork does the rendering. They talk through
exported volatile globals (`dxvk_vr_*`) and one text file (`dxvk_stereo.txt`).

Headsets: Vive/Index through **SteamVR native (OpenVR)**, Quest through
**Virtual Desktop's OpenXR runtime**. These are two different code paths and
the difference matters - see §7.

---

## 2. Build and deploy

Cross-compiled from Linux with MinGW. Single translation unit.

```bash
cd /root/dishonored-vr && ./build.sh          # -> out/d3d9.dll
```

```
i686-w64-mingw32-g++ -O2 -shared -static -static-libgcc -static-libstdc++
    -I../openvr/headers -I/root/OpenXR-SDK/include -Iimgui -Iimgui/backends
    src/dllmain.cpp imgui/*.cpp imgui/backends/imgui_impl_{win32,dx11}.cpp
    -o out/d3d9.dll -ld3dcompiler -limm32 -ladvapi32 -ldwmapi -lgdi32
    -lole32 -luuid -lwindowscodecs -Wl,--kill-at
```

`-Wl,--kill-at` matters - D3D9 exports are `__stdcall` and must not be
decorated.

Deploy target on the test machine:

```
G:\SteamLibrary\steamapps\common\Dishonored\Binaries\Win32\
```

Backups and every historical build live in `G:\back\Dishonored vr\out\`, as
`d3d9_<build>.dll` plus a `dllmain_<build>.cpp` source mirror **for every
build**. That mirror archive is the single most valuable asset in this project
 -  it is what made the 39.x rebase possible. **Keep writing it.**

**Bump `kBuildTag` on every deploy.** It is the first line of every log and
without it you cannot tell which binary produced a report.

The fork is a separate, much slower build. It has been frozen at **M8.2**
(`a6395b3a0bc73dd8cf766e24e984b968`) for a long time and should stay frozen
unless you have a specific reason. One unshipped commit exists on the branch:
`45116f2f` (**p53**) exports `dxvk_vr_view[4]`, the main camera's world forward
vector. It was built for a "verify what we rendered" idea that was never
finished. It is a good foundation if you need render-side ground truth.

---

## 3. Architecture, in the order it runs

1. **`DllMain`** - read `dishonored_vr.ini`, install IAT hooks for the
   resolution spoof (`GetSystemMetrics`, `GetMonitorInfo*`, `GetClientRect`,
   `SetWindowPos`, `MoveWindow`, `SetWindowPlacement`, `SystemParametersInfo*`).
2. **`Direct3DCreate9`** → the real fork is loaded, `CreateDevice` is
   intercepted, the game window handle is latched, the window is subclassed.
3. **VR init** - OpenVR or OpenXR, controllers, action bindings.
4. **Per frame, in `Present`** - capture the game's backbuffer, shear/copy
   into the per-eye textures, submit to the runtime, draw the overlay.
5. **Per script event, in a hooked `ProcessEvent`** - this is where nearly all
   the game-logic work happens: head rotation, hands, crouch, blink aiming,
   menus, the intro skip.

### The engine addresses everything depends on

```c
static const uintptr_t kGObjHdr     = 0x1423630;  // TArray<UObject*>{Data,Num,Max}
static const uintptr_t kGNamesData  = 0x1435674;
static const uintptr_t kProcessEvent= 0x00470640;
static const uint32_t  kNameOff     = 0x28;       // UObject::Name
static const uint32_t  kClassOff    = 0x30;       // UObject::Class
static const float     kUEPerRad    = 10430.378f; // 65536 / 2pi
```

These are for the **Steam build of Dishonored**. They are the same binary for
everyone, so they are **not** a source of per-machine variation. Do not chase
them when something works on one PC and not another.

Useful measured field offsets:

| offset | on | meaning |
|---|---|---|
| `+0x248` | PlayerController | current Pawn |
| `+0x3ac / +0x3b0 / +0x3b4` | PlayerController | FOVAngle / DesiredFOV / DefaultFOV |
| `+0x53c` | PlayerCamera | **sensor** - the FOV actually being rendered |
| `+0x254, 0x348, 0x368, 0x38c, 0x53c, 0x540, 0x564` | PlayerCamera | the FOV fields the lever writes |
| `+0xC4` | PlayerCamera | cached POV origin |
| `+0x60/0x70/0x80`, `+0x90`, `+0xcc` | SkeletalMeshComponent | world matrix rows, translation, bounds origin |

---

## 4. THREE head-motion paths - read this before touching head tracking

This is the single most confusing thing in the codebase and the source of most
of its bugs. There are **three** independent mechanisms that can move the
player's view, all enabled at once by default.

### Path 1 - head-mouse (`[Tracking] Enabled=1`)

Converts head rotation into **synthetic mouse movement**:

```c
in.mi.dx = ix; in.mi.dy = iy;
in.mi.dwFlags = MOUSEEVENTF_MOVE;
SendInput(1, &in, sizeof(INPUT));
```

Oldest and crudest. Works on anything because it just pretends to be a mouse.

**Silently requires:** the game window to be foreground
(`GetForegroundWindow() != g_gameWnd` → returns immediately), and the menu gate
to be clear. Scaling is `YawCountsPerDegree` / `PitchCountsPerDegree` (11.5).

### Path 2 - native script write (`[HeadTrack] Native=1` → `g_rotInject`)

Hooks the engine's own `ProcessViewRotation` and writes the out-rotator in
`ApplyHeadToViewRotation`. This is the correct one.

Two things about it that are easy to get wrong and have both burned people:

- **`ProcessViewRotation` reaches `ProcessEvent` ONLY through the camera
  MODIFIER chain** (every vocabulary dump shows `CameraModifier_CameraShake`),
  never through the PlayerController. Build 38.85 gated writes on the
  controller and silenced head tracking completely. Do not do that again.
- **The rotator is at `Parms+4` OR `Parms+8`** depending on which class
  declared the event. It is found by locating a plausible `DeltaTime` float
  (0.0005-0.2) and taking the rotator after it. Guessing +4 makes turning your
  head move the view up and down.

Yaw is applied as a **delta** (it must compose with stick turning). Pitch is
**absolute** - your head's pitch simply is the view pitch, nothing else authors
it. That property is what makes the 39.2 verification possible.

### Path 3 - direct fallback / "viewinject"

Writes rotation straight into the PlayerController when path 2 goes quiet.
Held off while: a cinematic is announced, the script path has never claimed
this pawn (`g_fbPvrSince == 0`), for 15 s after any pawn latch, and while
script writes are fresh (< 750 ms).

### Why this matters

**Both bugs found on the final day were in the gates BETWEEN these paths, not
in the paths themselves.** Three mechanisms means six hand-offs and every one
of them is a place to get stuck. If you ever get the chance to collapse this to
one path, do it - but only with the test machine in front of you.

---

## 5. Hands and weapons

The VR "hands" **are the weapon view models**. There is no separate arm mesh
being driven. `Skm_Player` is the body and nothing drives it. When a user says
"my arms aren't aligned", they mean the weapon view model's calibration is off.

`FpCollect()` walks the object graph out from the pawn (breadth-first, depth
< 3, offsets `0x20..0x600`, 140 visits, 224 seen, **24 candidate cap**) and
collects `SkeletalMeshComponent`s. Real view models are named `pPlayerMesh`;
you will also see `pMesh` (`Skm_Player` = the body, used as the untouched
reference), `AnimMesh`, `pArrowMesh_HighRes`.

The walk expands through anything classed `Item`/`Container`/`Inventory`/
`Power`/`Pawn`, which means **world props get in** - measured in one session:
`wash_rag`, `FeatherDuster`, `beer_pint_setup`. They are never driven, but they
reshuffle the list and eat slots out of the 24.

Calibration is an 8-phase probe (`FpCalibrateTick`) that commands each view
model and measures the engine's response: rotation signs, the parent rest
frame, the pivot, and `emap`/`einv` (written-translation → world-motion). **It
is only valid while the player holds still.**

**39.0 fix:** records are banked by **asset name** (`FpBankFind` /
`FpBankStore`), not by component pointer. A weapon swap recreates the
component, so the old pointer-keyed carry-over made every swap look brand new
and re-probed everything mid-combat. Measured proof it was wrong - same weapon,
same session, ninety seconds apart:

```
'Wpn_PlyGunElite' yawSign=+1 pitchSign=+1 parent(y=109 p=5)
'Wpn_PlyGunElite' yawSign=-1 pitchSign=-1 parent(y=150 p=4)
```

Inverted signs. That inversion is what users describe as "wiggling weapons".

---

## 6. The resolution machinery

The game is run **windowed**, rendering far larger than the desktop:

```
SpoofDesktopW/H = 4096 x 2304     lie told to GetSystemMetrics / GetMonitorInfo
RenderWidth/Height = 4032 x 2268  what the game is told its client rect is
DesktopWindowW/H = 1600 x 900     the real window, for the spectator view
```

`hkGetClientRect` returns the **render** size for the game window while the
real window stays small. That lie is load-bearing - without the size spoof the
game falls back, once as far as 800×600, and writes that to its own ini.

Consequences worth knowing:

- Windowed mode means **the desktop cursor is always visible**, so the
  cursor-based half of the menu detector is disabled and script events are the
  only menu signal. This causes real bugs (see §8).
- `setup_resolution.bat` patches only **ResX / ResY / Fullscreen** in the
  user's `Documents\My Games\Dishonored\...\DishonoredEngine.ini`. The other
  ~60 `[SystemSettings]` values are whatever that machine's hardware detection
  chose. This is unproven as a bug source but is a real per-machine difference.

---

## 7. Two headset paths, and the adapter bug (39.3)

SteamVR-native (OpenVR) and Quest-over-Virtual-Desktop (OpenXR) are separate
code paths. **Almost every unreproducible bug report has come from OpenXR
users.** That is not a coincidence and it is the first thing to check.

The most important defect found in the whole investigation, fixed in 39.3:

```c
XrGraphicsRequirementsD3D11KHR gr;
xrGetD3D11GraphicsRequirementsKHR(g_xriInst, g_xriSys, &gr);  // "spec: must call"
```

`gr.adapterLuid` is the runtime telling you **which GPU the D3D11 device must
be created on**. It was called and the result thrown away, and then:

```c
create(NULL, D3D_DRIVER_TYPE_HARDWARE, ...);   // NULL = DXGI's default adapter
```

Meanwhile both the game-frame textures and the eye render targets are created
with

```c
td.MiscFlags = D3D11_RESOURCE_MISC_SHARED;   // LEGACY shared-surface flag
```

which **cannot cross adapters** and carries no keyed mutex (there is no
`KeyedMutex` anywhere in the file). On a one-GPU machine the default adapter is
always right. On a machine with two - any laptop, any CPU with integrated
graphics next to a card - it is a coin flip, and losing it means the runtime is
handed surfaces built on the wrong GPU. **Every call still succeeds and nothing
logs an error.**

The dev machine has an RTX 4090 *and* an AMD integrated part, and wins the flip
every launch. That is the best explanation anyone found for "it only works on
his PC", and it explains it without anything about his PC being special.

39.3 enumerates adapters, matches the LUID, and creates the device there with
`D3D_DRIVER_TYPE_UNKNOWN` (**required** when passing an explicit adapter  - 
`HARDWARE` plus a non-NULL adapter is `E_INVALIDARG`). Falls back to the old
path on any failure. `dxgi.dll` is resolved via `LoadLibrary`, so the DLL gains
no new hard import.

**This was never confirmed by anyone who had the bug.** It is the single
highest-value thing to verify. Look for these lines in any user's log:

```
xr: the runtime requires D3D11 on adapter luid ........
adapter[0] luid ........ <- THE ONE THE RUNTIME ASKED FOR
D3D11 device created on adapter: <name>
xr: *** THE D3D11 DEVICE IS ON THE WRONG GPU ***
```

---

## 8. Open problems, with the best current hypothesis for each

### 8.1 "Zoomed in / can't look up-down / hands wrong" on Quest+VD - OPEN

The defining bug of the project. Affects several users, never reproduced on the
dev machine despite direct attempts.

Current best explanation, in order:

1. **The adapter bug (§7).** Untested by an affected user. Test this first.
2. **The stuck menu flag (§8.2).** Explains "F9 helps but it's not quite
   right".
3. **The engine discarding the rotator write.** Beardo's own investigation
   proved, with a purpose-built diagnostic that never fired, that the
   "several camera modifiers overwrite each other in one frame" theory is
   **wrong**, and that pitch is written correctly and continuously for 12+
   seconds while the screen does not move. His words: *"the write was never the
   broken part. Something downstream of the write still isn't reflecting it on
   screen."*

   Build 39.2 acts on that. It counts kept-vs-discarded pitch over 120-frame
   windows and, if the engine is discarding, stops claiming the camera so the
   path-3 fallback can finally engage. Before 39.2 the fallback was gated on
   *the act of writing* rather than the write having any effect, so on an
   affected machine head look was dead **and** the rescue was permanently
   suppressed. On the working machine the first window reads **120 kept / 0
   discarded**, so the signal has no false positives.

   ```
   headtrack: the engine is KEEPING our pitch write (120 kept / 0 discarded ...)
   headtrack: the engine is DISCARDING our pitch write (9 kept / 111 discarded ...)
   ```

   **Unknown:** whether path 3, once it finally engages, drives correctly on
   affected hardware. It has never run there.

**Never collected from a single affected user: GPU vendor / model / driver.**
After five falsified theories this is still the cheapest missing variable, and
§7 makes it the obvious one.

### 8.2 The stuck menu flag - FIXED in 39.4, verify it

Why people kept saying "F9 fixes it". Plain F9 still hits this:

```c
if (f9 && !g_f9Was) { g_menuOpen = false; g_cursorGhost = true; cursorVis = false; }
```

There are two automatic rescues meant to clear a ghost menu. Their conditions
were `g_menuOpen && !cursorVis` and `cursorVis && !g_menuOpen` - so
`menuOpen && cursorVis` was covered by **neither**, and that is exactly the
state a windowed game sits in, because the desktop cursor is always showing.
One missed script close-event and the head-mouse was parked for the session
with F9 the only way out.

39.4 drops the `!cursorVis` requirement. The real discriminator - which the
code already trusted - is that **the engine stops dispatching view rotation
while paused**, so if gameplay dispatches are still flowing it is a ghost
whatever the cursor is doing.

**Verify:** during a genuine pause menu, head-look must stay parked. If the
head-mouse stays live in a real menu, this change misfired.

### 8.3 The 38.78 focus fix was lost in the rebase - NOT YET RESTORED

The 39.x line is based on **38.72**. Build **38.78** fixed a confirmed bug:
the game window loses desktop focus (Virtual Desktop's overlay takes
foreground, and the player is in a headset with nobody at the desk), UE3 fires
`OnLostFocusPause`, and **both input polling and `ProcessViewRotation` stop**  - 
measured as `polls=0` and `headwrites=0` for the rest of a session while
rendering continued at 66 fps.

The fix was, in the window subclass: while a headset is being served, rewrite
deactivation messages so the game never learns it lost focus.

```c
if (g_vrKeepAlive && (g_vrReady || g_xrOn)) {
    if (msg == WM_ACTIVATEAPP && wp == FALSE)          wp = TRUE;
    else if (msg == WM_ACTIVATE && LOWORD(wp) == WA_INACTIVE)
        wp = MAKEWPARAM(WA_ACTIVE, HIWORD(wp));
    else if (msg == WM_KILLFOCUS)                      return 0;
}
```

**This is not in 39.x.** Path 1 also independently bails on
`GetForegroundWindow() != g_gameWnd`. Port it back - it was verified good, it
is contained, and its absence reintroduces a beaten bug. The full text is in
`G:\back\Dishonored vr\out\dllmain_38.78.cpp`.

### 8.4 Smaller open items

- **Main menu on the wrist.** Going from gameplay to the main menu, the menu
  gets swept onto the wrist panel and becomes unusable. The gate requires
  `CylTruthLive()` ("a possessed pawn cannot exist at the main menu"), but the
  pawn survives the transition long enough. Inherited, not a regression.
- **Intro boat.** New games skip the prologue via the game's own dev
  transition (`ce ChangeLvl_fromTower_toPrison`), because the boat arrival
  breaks in VR - you fall in the water. Real fix = make the keyhole seat-in
  work, then set `IntroSkip=0`. The skip polls every 500 ms for 90 s for the
  pawn near `(-3901, 36639, -225)` and fires `IntroSkipDelayMs` after
  **placement at the boat** - counting from the pawn latch instead produced a
  black screen in the cell.
- **Cutscene head-look** is fixed-camera. Fork export `dxvk_vr_view` (p53) is
  the intended foundation.
- **Mirror cross-queue sync**, **hands-after-quickload re-probe flakiness**,
  **VDXR frustum scale calibration**.

---

## 9. Dead ends - do NOT re-litigate

Each of these was eliminated **by measurement**, not by argument.

| theory | how it died |
|---|---|
| Missing file in the zip | Re-verified 39.3. `dbghelp.dll` shares a timestamp with `Dishonored.exe` and the PhysX DLLs - stock. `dxvk_stereo.txt` byte-identical to the package. `vr_*.json` are proxy-written each launch. `ps_*/vs_*.dxbc` are one-way dumps. |
| Monitor resolution | Perfect correlation (all affected 2560×1440, dev 1920×1080) - then the dev machine ran at 1440p and reproduced the exact `4032x1431` / aspect 2.818 signature **with no bug**. |
| Monitor arrangement | Perfect correlation (all affected had negative `SM_XVIRTUALSCREEN`) - then the dev machine moved its second monitor left, log-confirmed `real=-1920`, **no bug**. |
| Several camera modifiers overwriting in one frame | Beardo's purpose-built diagnostic never fired. This killed the entire 38.86/87/88 chain-stamping effort. |
| FOV base race as the zoom cause | Real race (a load-time sensor sample can latch 90 and render 108° instead of 130°) but **not the cause** - monodada latched the correct 75.0 and still had the bug. |
| Controller-dispatch gate (38.85) | PVR never dispatches on the controller; gating there silenced head writes entirely. |
| Fullscreen config, VD streamer version, layer mode, per-user game inis, headset-side-compositor theories | All eliminated; same break on SteamVR **and** SteamLink. |
| The user's game config being modified | Stock: `m_fDefaultFOV=75.0`, `GameFOVAngle=75`, `FOVAngle=90`. |

**Two claims that circulate in Discord from ChatGPT analyses, both false:**

- *"The FOV lever fires once and device Resets revert it."* It runs on **every**
  ProcessEvent dispatch - logs show `lever=130 writes=6039` per 3 seconds.
- *"campov: 75 → 100 → 130 shows FOV oscillating."* `campov:` is a DevTools
  scan listing several **candidate memory blocks in one pass** (one stale at
  100, live ones at 130). It is a directory, not a timeline.

---

## 10. Traps that will bite you

1. **`hkReset` LAW.** Every `POOL_DEFAULT` D3D9 resource must be released in
   `hkReset` before the device resets, and recreated after. Miss one and the
   game dies on every alt-tab or resolution change.
2. **Never log from static initialisers.** A `Logger::info` inside a static-init
   lambda in the fork stopped the DLL loading entirely - no log at all, not
   even the header. From the outside it looks exactly like a corrupt build.
3. **UObject liveness.** Before writing to any cached UObject pointer, check it
   is still in `GObjects` at the index you found it (`CamAlive()`). The
   name/class test cannot detect a freed object - freed memory keeps its old
   contents until reused. A level load without this check writes thousands of
   floats per second into a freed object.
4. **The log is overwritten on every launch.** Archive before relaunching, or
   the evidence is gone. (This cost a run during the 39.x work.)
5. **`.ini` files are CRLF.** Preserve it when editing programmatically.
6. **Launch through Steam.** A direct EXE launch crashes at the menu.
7. **Passing an explicit adapter to `D3D11CreateDevice` requires
   `D3D_DRIVER_TYPE_UNKNOWN`.** `HARDWARE` + non-NULL adapter is
   `E_INVALIDARG`.
8. **Diagnostics must pay out as they are discovered**, not batch into a report
   at the end of a window. Three separate runs were wasted because the payload
   only printed after a timer the operator did not wait for.

---

## 11. Process rules - these were enforced by the project owner and they were right

Every one of these exists because breaking it cost real time.

1. **One behavioural change per build.** Anything else and you cannot attribute
   a regression.
2. **Build on a snapshot that has been confirmed good.** Never stack onto an
   unreviewed pile. The entire 39.x rebase exists because this was violated:
   five speculative changes went out together on an unapproved base and broke
   the overlay and head tracking.
3. **When something that worked breaks, diff against the working build FIRST.**
   On the day this document was written, that diff took four minutes and would
   have prevented three wrong guesses in a row.
4. **Measure before theorising.** Read the artifacts already on disk. The
   fork's own `Dishonored_d3d9.log` contained the dual-adapter answer the whole
   time and was never opened until the last hour.
5. **Never ship a guessed constant as a measured one**, and never sound
   confident about anything not verified in-headset.
6. **The packaged `dishonored_vr.ini` must be a byte copy of the tested
   machine's ini.** No hand-edited flags in the package, ever. This was
   violated twice. The worst case: the zip shipped `ChainStamp=0` and
   `StampLive=0` while the dev machine had neither key and the code defaults
   were `1` - so **every downloader ran a different head-injection path from
   the one being tested**, for days.
7. **Motion controls for crouching and hands must never stop working.** This is
   the owner's hard line and it is a good one.
8. Do not attribute logs to machines by drive letter. Multiple testers install
   to `G:\SteamLibrary`. Check the build tag and ask.

---

## 12. Build ladder (current line)

Rebased onto 38.72 - the last build the owner personally confirmed good - with
only evidence-backed fixes re-applied.

| build | md5 | change |
|---|---|---|
| 39.0 | `759a37f08729a03d1de38dd1740898a5` | 38.72 + `Shift+F9` + calibration bank (67 changed lines, diff-verified) |
| 39.1 | `b7de2d9a56b9662b6dd380c76c1ab009` | + `IntroSkipApply` ported verbatim from 38.75 |
| 39.2 | `3b3a05dc2ca9e1b3198c332629a75657` | + closed loop on the pitch write |
| 39.3 | `888546bb59dbb395f6a6e15d582acc39` | + D3D11 device on the runtime's required adapter |
| 39.4 | `79d65412e2d2a8147d9eaf00a1f3a7e4` | + the uncovered menu quadrant |

Fork: **M8.2**, `a6395b3a0bc73dd8cf766e24e984b968`. Last shipped zip on 39.3:
`5bde0ebc5d28d638a7fc18fe8a96ed89`.

**39.4 has not been tested by anyone yet.** 39.2 was confirmed a no-op on the
dev machine (`120 kept / 0 discarded`, calibration bank firing, intro skip
correctly declining on a loaded save).

---

## 13. If you pick this up, do these first

1. Port the **38.78 focus fix** back onto 39.4 (§8.3). It is a known-good fix
   currently missing.
2. Get **39.3+** in front of one person who actually has the bug and read the
   adapter lines in their log (§7). This is the highest-value single test
   available and it needs no special setup.
3. Collect **GPU vendor / model / driver** from every affected user. Still
   never collected once.
4. Have an affected user confirm whether `Shift+F9`-era builds still show the
   F9 alternation. If they do, something other than the two fixed causes is at
   work and it is new information.

Everything else can wait.

---

## 14. Credit

GingasVR built this - the reverse engineering targets, every in-headset test,
every measurement that killed a bad theory, and the discipline that produced
the rules in §11. It is a working seated VR conversion of a 2012 UE3 game
built without source access, and that is a genuinely hard thing that he
finished.

Beardo's own investigation produced the finding in §8.1 - that the rotator
write is fine and the problem is downstream - including a diagnostic that
disproved its own hypothesis rather than quietly moving on. That is the single
most useful piece of debugging anyone contributed, and 39.2 exists because of
it.
