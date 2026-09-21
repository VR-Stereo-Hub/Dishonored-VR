# Review plan: VR-161 (game option settings) and VR-165 (chain camera arc)

Prepared 2026-09-20 for external review. Branch `claude/vr-164-pause-hands`,
19 commits off `VR-Main`. Everything below is read-only instrumentation plus one
opt-in write; nothing here has been merged.

The two items are unrelated in mechanism and are reviewed separately.

---

## Part 1 - VR-161: setting the game's own option settings

### Where the settings live (established)

Not in the game's 21 ini files. They live in the Steam Cloud profile blob
(`userdata/<id>/205100/remote/OPTIONS.sav`, bit-packed UE3
`OnlineProfileSettings`), and are held at runtime in the `ProfileSettings`
array on a live `ArkProfileSettings` object.

`DishonoredEngine.ini [SystemSettings] bAllowLightShafts=True` while the menu
and the profile both read 0. **The ini is a stale mirror.** This was the
question that started the work and it is now closed.

### What was eliminated, and how

Four hypotheses were tested and killed in order. Each was killed by a
measurement, not by argument:

| hypothesis | how it died |
|---|---|
| wrong object | ranking prints all 5 candidates; picks `ArkProfileSettings`; ids still refused |
| profile not loaded | array went 0 entries -> **115 entries**; ids still refused |
| bad parms block | irrelevant - the array is readable without the accessors |
| the accessors work | `GetProfileSettingName` / `GetProfileSettingValueInt` through ProcessEvent have refused on **every** run, reason unknown |

### The stride is measured, not guessed

Dumping the array head six dwords to a line returned a clean ascending id
column with no gaps:

```
[30] 00000002 0000001d 00000001 0000003b 00000000 00000000   id 29
[36] 00000002 0000001e 00000001 00000037 00000000 00000000   id 30
[42] 00000002 0000001f 00000001 00000025 00000000 00000000   id 31
[48] 00000002 00000020 00000001 00000028 00000000 00000000   id 32
```

`Owner` switches from 1 (Live-managed ids 1, 2, 12, 13, 16) to 2 (the game's
own); ids 29..64 are the `PSI_GBA_*` bindings and carry key codes as values,
which is why 59 and 92 appear there. This is
`OnlineProfileSetting { Owner, PropertyId, Type, Value1, Value2, AdvertisementType }`
at 24 bytes.

`GoVerifyStride` re-checks on every read: ids must ascend and stay inside the
PSI range, or the log declares the values noise. **Reviewer check:** the
threshold is `ascending * 10 >= entries * 8`. Measured runs give 111/115
ascending, because the id sequence restarts when `Owner` changes 1 -> 2. That
is expected, but the constant is a judgement call and should be checked.

### All fourteen read correctly

Every one already matches the project's target on the dev machine, which is why
the write below had nothing visible to change:

```
id 105 Gameplay_KillCamMode            VALUE 0  want 0
id 108 Gameplay_HeadBobAmount          VALUE 0  want 0    (type 5 - a FLOAT, not an int)
id 109 Gameplay_CameraRelativeClimbing VALUE 0  want 0
id  99 HUD_CrosshairStyle              VALUE 0  want 0
id  81 Gamepad_bAutoAim                VALUE 0  want 0
id  83 Gamepad_bFriction               VALUE 0  want 0
id 116 GraphicsPC_bFullScreen          VALUE 1  want 1
id 117 GraphicsPC_bVSync               VALUE 0  want 0
id 120 GraphicsPC_ModelDetails         VALUE 1  want 1
id 121 GraphicsPC_LightShaftEnable     VALUE 0  want 0
id 122 GraphicsPC_AntiAliasingMode     VALUE 1  want 1
id 123 GraphicsPC_RatShadows           VALUE 0  want 0
```

`HeadBobAmount` is **type 5** where the rest are type 1. It is a float and must
not be written as an int. Nothing writes it yet.

### The write: what happened

`[Diagnostics] GameOptsWrite=121=1,123=1` (ships empty; set by hand for this
test) wrote light shafts and rat shadows on.

- The array took both: `before=0 asked=1 readback=1`.
- **The options MENU showed the new values.** So the array is the store the
  menu reads, and the write is honoured at least that far.
- Visually inconclusive: no rats were present and light shafts could not be
  told apart. **Not evidence either way.**
- The `system` column (`scale get bAllowLightShafts`) returns nothing on this
  build - `scale get` appears to print to the console rather than return a
  value through `ConsoleCommand`, so that column has never worked and should
  not be read as "the renderer did not notice".

### Proposed next step, for review

The scripts give a better route than writing the array and hoping:

```
DisGFxMoviePlayerMenuBase:
  native function OnSettingChange(int _SettingID, float _fValue);  thunk 0x009F7B80
  native function OnApplyVideoSettings();                          thunk 0x009F93A0
  native function OnLeaveOptions();                                thunk 0x009F7640
```

`OnSettingChange` is the call the options screen itself makes. Driving it
should get the game's own validation, apply and persistence rather than us
writing a dword and guessing what reads it.

**Open questions a reviewer should weigh:**

1. `OnSettingChange` takes a **float** value and a `_SettingID`. It is NOT
   established that `_SettingID` is the PSI enum - the menu's own
   `m_SettingsCategoryList` was found (12 categories at `+0x1e0`,
   `m_Settings +0x18`, `m_SettingID +0x0`) but walking it returned `n=0`
   for category 0, so the menu's id scheme is still unread. If the schemes
   differ, calling `OnSettingChange` with a PSI id writes the wrong setting.
2. These are natives on a **movie player**, so they likely require the options
   screen to exist. Calling them outside the menu may be a no-op or worse.
3. A verified write is still not an honoured one. The graphics settings have
   consumers that read at startup; `OnApplyVideoSettings` is the candidate for
   forcing that, and has not been called.
4. `SaveProfile()` (an exec on the player controller) persists to `OPTIONS.sav`.
   **Nothing should call it until the rest is proven** - it writes a player's
   real settings file.

**Recommended order:** read the menu's own ids first (fix the category walk),
confirm whether they match the PSI enum, and only then call `OnSettingChange`.
Do not call `SaveProfile` in this phase.

---

## Part 2 - VR-165: the chain camera arc

### Symptom

After releasing a chain (reproducible by ungrabbing with X rather than jumping
off), the player feels lifted and tilted; looking up sends the view into the
sky and looking down swings it wildly. The player can still run and jump.
Pause pauses it; Blink and mantling clear it; the weapon wheel cleared it once.

### What is eliminated, and how

Everything the mod owns:

| suspect | measurement |
|---|---|
| our position offset | median 8.0 uu, **max 62.3** against a multi-hundred-uu arc |
| our rotation | head pitch 165.6 deg vs camera 162.6 - **ratio 0.98**, tracked 1:1 |
| the neck model | pivot constant `0.321/0.062` over 454 samples; ceiling ~65 uu |
| camera modifier stack | one entry, `CameraModifier_CameraShake`, alpha 0.000, 120+ tables |
| the game re-grabbing | climbs are auditable and every one was a real button press |
| a camera object/mode swap | object `18511E00`, class `DishonoredPlayerCamera`, **never changes** |
| locomotion confounding the arc | player velocity **0.0** in all six arc windows |

### The measurement that names it

Camera POV minus pawn location - the eye offset, which is the lever head
rotation swings:

```
healthy:   44.1   55.3   71.3   77.5   80.5   83.4 uu
bugged:   127.6  135.5  158.7  243.8  252.7 uu
```

Healthy clusters at **44-83 uu**, which is correct for first-person eye height
on an 87.5 uu collision cylinder. Bugged reaches **250 uu**, and the components
show why: healthy is almost pure Z (`4.7/10.0/70.4`), bugged is large in all
three (`143.3/-85.5/189.7`).

So the camera is displaced roughly 1.8 m up and to the side of the pawn, and
head pitch rotates that long offset through the arc the player feels. This
matches every subjective report: taller, lifted, tilted, swinging.

**Reviewer caution on the radius column.** The `implied radius` figures in the
log range from 88 to 12478 uu/rad and are NOT trustworthy: the calculation
divides position span by pitch span, so a window with little pitch produces a
huge quotient. The guard is a 20 degree minimum, which is too weak. **The eye
offset numbers above are the reliable measurement; the radius column should be
tightened or removed.**

### Retracted along the way

Two frequency figures reached a ticket before being withdrawn:

- **~8 Hz**, from `cachePos` logged every ~109 ms - a 9 Hz sampler cannot
  resolve 8 Hz. Aliasing.
- **~110 Hz**, from a counter sampling on the ProcessEvent lane - it was
  measuring its own dispatch rate.

Both are recorded as retractions in `docs/dishonored/FLICKER_REFERENCE.md`. A
third instrument (the climb test) printed "ON A PRESS - 0 ms ago" for every
climb, which is what it would print if the pad mask never cleared; it was
rebuilt to report the mask and refuse to decide when the pad has not been idle.

### Proposed next step, for review

The offset is the lever. The open question is **what inflates it**, and that has
not been measured. Candidates, none confirmed:

1. The engine's own eye-height / camera-offset calculation entering a state the
   chain release leaves behind.
2. A camera offset the climb applies and does not clear on the X-release path
   (jumping off does clear it).
3. Something of ours feeding the game's camera that has not been isolated -
   considered unlikely given the position offset caps at 62 uu, but not
   formally excluded for every write path.

**Recommended:** log the engine's own eye-height/offset source per frame during
the bug, rather than the resulting POV. The POV is the symptom; the field that
produces it is the cause, and it has not been located in memory yet.

**Explicitly not recommended:** clamping the POV or subtracting the excess
offset. That would mask a game-state bug with a camera hack, and this session
has already shipped two levers (live fullscreen, live vsync) that were written
on plausible reasoning and produced a resolution collapse and a persisted wrong
ini value.

---

## What a reviewer should push back on

1. **The stride ascending threshold** (80%) - a judgement call, currently
   satisfied at 111/115 for a legitimate reason.
2. **The radius column in swing/arc** - known unreliable, still printed.
3. **Whether the menu id scheme matches the PSI enum** - assumed similar,
   never verified, and the whole `OnSettingChange` plan depends on it.
4. **Writing floats** - `HeadBobAmount` is type 5 and the current write path
   stores an int.
5. **Instrument discipline generally** - four instruments in this session
   produced confident answers that were artifacts. The pattern each time was a
   test that could not print the unwelcome answer. Reviewers should check every
   new probe for that property specifically.
