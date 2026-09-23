# F10 panel audit and cleanup plan (VR-196, 2026-09-22)

The F10 panel grew one control per investigation: about 200 controls across 11 tabs, with
fixes nobody should turn off sitting beside player settings, and some controls that do
nothing at all in this game. This is the per-control inventory and the plan. Decisions
marked **ASK** wait for the tester.

Codes: **B** Basic, **A** Advanced, **D** Debug, **X** remove from the panel (the ini key and
the code stay unless the row says otherwise), **M** merge into another row.

## 1. The model

* **One view selector at the top: `Basic | Advanced | Debug`.** It replaces the "developer
  tools" checkbox (`[Overlay] DevTools`). It is saved as `[Overlay] Level`, and a fresh
  install starts on Basic. Each tier shows everything in the tiers below it.
* **Basic** is what a player tunes to their body and taste.
* **Advanced** is preference detail most players never touch.
* **Debug** holds fixes that should stay on, A/B levers, instruments, readouts and dev
  experiments.
* **Every section is a collapsing header, closed by default.** The panel remembers what
  was open for the session only.
* **Every control gets a description:** one grey line under it, written for a player
  rather than a maintainer. Debug rows may use a hover tooltip instead, to stay compact.
* **No behaviour change.** Every ini key keeps its meaning and its default. Moving a control
  changes where it is drawn, not what it does.
* The helpers live in `overlay.cpp`: `OvlLevel()`, `OvlSection(name, tier)` and
  `OvlDesc(text)`. The sub-panels (sword, camera shake, aim, HUD layout, hand trim) take
  the tier as well.

## 2. The new layout

Always visible, above the tabs: **Recenter**, **Height offset**, **Save as defaults**, the
view selector, and the text-scale slider.

| Tab | Basic sections | Advanced adds | Debug adds |
|---|---|---|---|
| **Hands** | Hand size; Hand position (L / R / powers steps); Sleeve | Wrist roundness; Game arms per action (the Animations list) | Hand-eye and identity fixes; legacy SkelControl drive (if kept) |
| **Aim** | Reticle (on, hand, size, colour, distance); Reticle position (other items); Held object position | Head or controller aim per item; carried-object options | Ray source levers; control dot; anchors |
| **Controls** | Controller layout (D-pad modifier, flip, pause chord); Motion sword (on, speed, cooldown, stealth stab) | Sword detector detail; stab shape | Sword readouts |
| **Comfort** | Crouch (real crouch on, trigger height); Camera shake (remove shake); Gameplay FOV | Positional tracking; neck pivot; camera shake categories; rain and lens effects; cinematic head look options | Arm and facing levers; mirror weapons; stereo state guards |
| **HUD** | HUD on/off; notes and journal on the hand | Weapon dial; wheel side panels; menu immersion; element anchors; window; hand panels | Alpha modes; native-marker tests; freshness tests; census |
| **Display** | Resolution scale | Fullscreen, vsync; desktop mirror | Capture mode, EYES, 9Ex, profiles, benchmark, MARK |
| **Runtime** (Debug only) | - | - | The runtime layer's panel, pacing |
| **Log** (Debug only) | - | - | The log ring |
| **Diagnostics** (Debug only) | - | - | Game options read and apply, status.json |

The old **View**, **Blink**, **Animations** and **Advanced** tabs dissolve into the tabs above.

## 3. Inventory (every control, current tab -> new place)

### Top of the panel
| Control | Code | Note |
|---|---|---|
| RECENTER (F5) | B | top |
| height offset (m) | B | top |
| SAVE AS DEFAULTS | B | top. Most rows already save on change; the description says what this adds |
| IPD text | B | a small read-only line under Recenter |
| UI text scale | B | bottom, as now |

### Controls tab
| Control | Code | Note |
|---|---|---|
| D-pad modifier, Flip D-pad, X + Y pause chord | B | the six help paragraphs become one description per control |
| Motion sword: on/off, swing speed needed, swing cooldown | B | |
| hide the sword's swing trail | D | a fix (VR-171) |
| swing detector (sustain/edge) | D | edge is the tested answer |
| travel first, re-arm below, attack press, median of 3, head-relative, only with the sword, a swing presses | A | |
| stealth stab on, stab motion | B | |
| stab start, thrust speed/reach, how straight/forward | A | |
| count a thrust standing up (testing) | D | |
| sword readouts (last/peak speed, gate, fires...) | A | ASK: A or D. They are what you use to set the speed |
| Camera shake section | moves to Comfort | |

### Aim tab
| Control | Code | Note |
|---|---|---|
| Head or controller, per item (7 rows) | A | ASK: A or B |
| Throw carried objects with the left trigger | A | |
| Hold carried objects in the hand | A | |
| Held object position steps | B | |
| Held object stored values | D | |
| Held object ignores reticle changes, tuned-at readout, retune button | A | |
| Anchor the held object on the game camera | D | a fix (VR-181) |
| Keep the angle it was picked up at | A | |
| Held object drawn in the world | D | a fix (VR-181) |
| Held object turns with the hand | A | |
| Ray follows the hand trim, Ray from loaded bolt geometry | D | fixes |
| CONTROL dot | D | instrument |
| Ray / renderer status line | D | |

### Reticle (now in the HUD tab; moves to Aim)
| Control | Code | Note |
|---|---|---|
| Reticle on, Beam, Left/Right hand, distance, size, colour, White | B | |
| Other items X/Y, Centre, Tested position | B | "Tested position" becomes "Reset to default" |
| Show the reticle while this panel is open | A | |

### View tab (dissolves)
| Control | Code | Note |
|---|---|---|
| Custom gameplay FOV + slider + reset | B | Comfort |
| Cinematic head look (candidate) | A | Comfort, cinematics section. ASK: is it still a candidate or the answer? |
| Suppress animation up/down tilt, Suppress cinematic roll, Suppress cinematic FOV zoom | A | Comfort, cinematics |
| Natural head look in lean / keyholes | A | Comfort |
| Native hands while mantling, Native cinematic hands and arms | A | Hands, game arms |
| Guard menu/loading stereo and input | D | a fix |
| Stereo cinematic/dialogue states, Stereo while possessing | D | fixes |
| Hide camera rain, rain distance, lens distance, lens keep size, lens follow head, rain lens strength | A | Comfort, rain and lens effects |
| Mirror pistol/crossbow + Rebuild mirror | D | a fix |
| Hide cinematic black borders | A | Comfort, cinematics |
| Image-linked head orientation | D | a fix |
| world scale (uu/m) | A | ASK: A or D. Changing it breaks the tested scale |
| arm counter-yaw + on/off | D | VR-30 lever |
| Head-based movement | A | Comfort |
| arms: head yaw does not turn the body | D | |
| arms: strip mesh rotation (dead end, kept as A/B) | **X** | its own label calls it a dead end |
| screen distance / width (m) | A | Display, "the flat screen (menus, loading)" |

### Comfort tab
| Control | Code | Note |
|---|---|---|
| auto-refocus game when SteamVR steals it + Refocus now | A | ASK: still needed on the Quest / VD path? |
| real crouching crouches Corvo, crouch trigger | B | |
| positional head tracking | A | |
| neck pivot: upright at steep pitch, off/add/cancel, below/behind, crouched pivot | A | one collapsed section; the readout line moves to D |

### Blink tab (dev-only today)
| Control | Code | Note |
|---|---|---|
| Blink aims with your CONTROLLER | **M** | duplicate of Aim > Blink |
| ...off the same ray as the dot and your shots | D | |
| capture a frame WITH the marker | D | |
| reach mode (head / fixed / hand pitch), max reach, near reach, pitch near/far | A | ASK: keep, or X if the hand-pitch curve (VR-92) was retired |
| landing marker explanation text | **X** | history, belongs in docs |

### Animations tab
| Control | Code | Note |
|---|---|---|
| Enable selected game arms | M | the same switch as Hands > "Game arms during scripted actions"; kept once |
| Animation view left/right + reset | D | |
| Sword swing animation, Shooting animation | A | |
| Reset arm choices, Enable all actions, filter, the 40 per-action rows | A | ASK: A or D |

### Hands tab
| Control | Code | Note |
|---|---|---|
| Sleeve preset, Sleeve length | B | |
| Rounded wrist ends, Wrist roundness | A | |
| Game arms during scripted actions | A | |
| Menu hand eye: half-IPD steps | D | test |
| Predict the eye when the jump is unreadable | D | VR-95 lever, ships off |
| Keep weapon identity across a menu / a note, Skip the UI rescan | D | fixes |
| Cache startup name lookups | D | fix (VR-102) |
| Detect loaded player without crouching | D | fix |
| Fast mono for books and notes | D | fix (VR-98) |
| Animation state readout | D | |
| Correct weapon-specific lens | D | fix |
| hand / weapon size | B | |
| Hand model position (L / R / powers, steps, stored values) | B | stored values D |
| Separate left-hand position for powers | A | |
| Numpad steps follow my view | A | |
| CALIBRATE HANDS (3 s) | ASK | it calibrates the SkelControl neutral. Is it still meaningful with palette hands? |
| SkelControl dev block: DRIVE them, strength, move XYZ, space, drive from controllers, position/rotation/add to anim, hand travel, world reach, rotator writes, camera look-at strength, L/R hand control, counter head turn, experiment: remove mesh rotation, clamp, world-space placement | ASK | pre-palette R&D. Propose **X** for the panel and a Debug note; the drive keeps running from the ini |
| separate trim while crouched, FORCE crouched pose, stance source, crouch button bind / toggle / re-measure, must hold for, eye-height separation, re-learn standing height | ASK | these feed the SkelControl trims. Keep in D if still effective; X if not (to be measured) |
| STANDING / CROUCH / BLOCK offsets (L/R fwd/right/up) | ASK | same question |
| HAND ROTATION (donor graft) section | ASK | propose **X**: the palette rotation replaced it |
| FINISHER CAPTURE diagnostic | **X** | a one-time log probe, done |
| writes/s readouts | D | |

### Display tab
| Control | Code | Note |
|---|---|---|
| Resolution scale, Set resolution | B | |
| fullscreen (live), vsync (live) + readout | A | |
| Desktop benchmark, pair pacing, Reduced trial | D | |
| Disable desktop mirror, keep frozen across gaps, Reduce desktop presentation | A | ASK: the last is a "candidate": A or D |
| Sample render-thread CPU costs | D | |
| stereo armed | D | |
| Render resolution picker (next launch: size combo, W/H, fullscreen, VirtualMode, Apply, refresh modes, use the game's size) | ASK | superseded by the live scale above. Propose **X**, keeping `res` on the seam |
| Native draw CPU timing, Bridge GPU timing, Diagnostic A/B/A | D | |
| tick / gpu readouts, MARK | D | |
| capture mode | D | |
| EYES readout, DUMP EYES, REARM 2, CAPTURE REINIT, PROJECTION OFF/AUTO | D | |
| c5 pairing, Repair right-eye flicker, Repair late eye tags | D | fixes |
| frame-identity trace | D | |
| D3D9Ex device at next launch | D | |
| FOV lever (legacy) | ASK | a second FOV mechanism beside "Custom gameplay FOV". Propose **X**, keeping the ini key |

### HUD tab
| Control | Code | Note |
|---|---|---|
| HUD on its anchors | B | |
| route elements by screen region | D | |
| draw census + quad readout | D | |
| Native HUD comparison banner + restore | D | |
| Weapon wheel side panels (on, alpha, per-panel anchor/position/size, captured area) | A | captured area D |
| Menu immersion (keep direction, per-menu head look and blur) | A | |
| Recent scene uploads in head-tracked menus (test) | D | |
| Pause menu alpha, wheel crop through closing (test), recent pause uploads (test) | D | |
| Notes/books/journal alpha | A | |
| Objectives: rune/objective arrow boundary + insets, native reference, rune ownership, inner artwork, every Heart marker, awareness meters, native size | D | all "(test)" |
| HUD grouping: centre gauges ride the aim dot | A | |
| interaction labels together, route moving objectives, objectives follow screen | D | fixes |
| native objective icons / labels / upright (test) | D | |
| Interactables alpha | A | |
| Notes and journal on the hand (tilt, follow left hand, width, offsets) | B | |
| Weapon dial (alpha, on, distance, direction only, circle, entry tilt/yaw, neutral radius, width, travel, crop) | A | on/off and width in B? ASK |
| ELEMENTS table (anchor, x/y/scale per element) | A | |
| THE WINDOW (recenter, distance, width, height, offsets) | A | |
| LEFT/RIGHT HAND PANEL (orientation, x/y/z, lift, width, tilt) | A | |
| GENERAL HUD ALPHA (restore, mode, gain, floor, gamma, mix, backdrops) | D | the restore button in A |
| SCREENS ON THE ANCHORS (in-game screens ride, per-menu mask) | A | |
| hud reset (the presets) | A | |

### Runtime tab (the runtime layer's own panel)
| Control | Code | Note |
|---|---|---|
| whole panel | D | |
| VR PACING: detach, keepalive, feed, phases | D | fixes |
| VR enabled, VR camera mode | D | |
| AlternateEye stereo test, Swap eyes | D | ASK: X? AER is not a method this build ships |
| SR pair pacing, sync pair rate, strict pairs, pose look-ahead | D | |
| Cinematic auto-detect, cinematics as stereo, During cutscenes | D | |
| **Hide cutscene black bars, Full-screen effects across the view, Post effects: source must be a render target, the routing line, Cutscene subtitles in-frame, VR HUD (gameswf quad) + its stats line** | **X** | `core/vr/hud_stub` answers "nothing" to all of them in this game, so these switches do nothing. Removed behind a `41.x (Dishonored)` host guard, so the layer stays otherwise verbatim |
| Manual claimed FOV + slider | D | |
| Anchor mono screens, mask, recenter, screen distance/width | A | M with the View tab's screen distance/width |
| status lines | D | |

### Log tab and Advanced tab
| Control | Code | Note |
|---|---|---|
| Log ring (category, level, follow, copy tail, status.json) | D | |
| developer tools checkbox | **X** | replaced by the view selector |
| Apply VR defaults at startup | A | ASK: A or D |
| Read game options into the log, read automatically | D | |
| game window size line | D | |

## 4. Order of work

1. The selector, `OvlSection` and `OvlDesc`, and `[Overlay] Level` (default basic,
   migrating `DevTools=1` to debug).
2. Move each control to its new tab and section, with its tier and description, one tab
   at a time. Each tab builds and is checked on the desktop mirror.
3. Remove the **X** rows. The ini keys keep working.
4. Sub-panels: sword, camera shake, aim and reticle, HUD layout, hand trim, runtime (host
   guard only).
5. Update the default ini (the `[Overlay] Level` key), the golden ini and the docs
   (F10_MOTION_CONTROLS, this file's final disposition).
6. One headset pass per tier.

## 5. Outcome (built 2026-09-22)

The tester's answers, applied:

| # | Question | Decision |
|---|---|---|
| 1 | Descriptions | A hover tooltip on every control, at every tier |
| 2 | Sword readouts | Debug |
| 3 | Head or controller aim per item | Basic |
| 4 | The per-action game-arms list | Advanced |
| 5 | World scale | Basic |
| 6 | Weapon dial | Advanced (all of it) |
| 7 | Apply VR defaults at startup | Advanced (a Game options tab) |
| 8 | Reduce desktop presentation | Advanced; Disable desktop mirror is Basic |
| 9 | Cinematic head look | Settled; Debug, "candidate" dropped |
| 10 | Auto-refocus when SteamVR steals focus | Removed from the panel. The behaviour stays: it acts only when a SteamVR window takes focus |
| 11 | The SkelControl hand system's panel | Removed (calibrate, drive, trims, crouch stance tuning, donor graft, finisher capture). The ini keys still load |
| 12 | Old next-launch resolution picker | Removed; the live scale stays, and `res` stays on the seam |
| 13 | Legacy FOV lever | Removed from the panel; `[Camera] FovLever` still works |
| 14 | Blink reach modes | Debug (Aim tab) |
| 15 | AlternateEye test and Swap eyes | Removed from the runtime panel |

Built as: `core/ui/ovl_ui` (the level, `tip`, `section`), `core/ui/overlay_tabs.inc` (one
function per tab), and tiers and tips inside every sub-panel (sword, camera shake, reticle and
aim ray, HUD layout, hand position, runtime). Tabs: Hands, Aim, Controls, Comfort, HUD, Display,
Game options (Advanced), Runtime and Log (Debug). `[Overlay] Level=basic|advanced|debug`
replaces `DevTools` (DevTools=1 opens on debug).

### What the audit after the rewrite found and fixed

* **Controls that did not persist.** Crouch height (`[Tracking] CrouchDropM`) and flat-screen
  width (`[Screen] WidthMeters`) were read at launch and written by nothing, SAVE AS DEFAULTS
  included, so the sliders reset every launch. Real crouching, positional tracking, gameplay
  FOV, head-based movement, the neck pivot, the cinematics switches, rain and lens, world
  scale, height offset, hand size, and game hands while mantling or in cinematics were kept
  only if SAVE AS DEFAULTS was pressed. Every Basic and Advanced control now writes its key
  when it changes (sliders when released). Debug switches stay session-only unless SAVE AS
  DEFAULTS is pressed, which is what an A/B switch should do.
* **ID collisions.** The "Sleeve" header and the "Sleeve" combo, and the "Reticle" header and the
  "Reticle" checkbox, shared an ImGui ID, which makes hovering and clicking unreliable. The
  controls were renamed.
* **Coverage.** Every ini key the old panel wrote is still written. The only setters no longer
  reachable are the removed rows: strip mesh rotation, the donor graft, the HUD stub, and the old
  picker's FOV readout. Every widget carries a tip, and every Begin/End, Push/Pop and
  TreeNode/TreePop is balanced.
* **Not verified in the running game.** Nothing here has been drawn yet. The first launch should
  check that each tier shows what section 2 says, and that a change survives a relaunch.

## Follow-up VR-199 (2026-09-22)

Reticle hand selection and Beam have been removed from the panel and normalized to left/off
when configuration is applied. The header carries an always-visible click/hold shortcut
hint in the existing brass/bone theme. Reset to Defaults is beside Save as Defaults and
uses the shipped profile at next startup, with a backup and runtime/DataDir preservation.
Rain hide is now Basic > Comfort > Rain; lens placement remains Advanced. Rain hide includes
rain-named looping camera lens particle templates as well as the camera box, pending a
headset A/B. Reset and all-tier headset verification remain open.
