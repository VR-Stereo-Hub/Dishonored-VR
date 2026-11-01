# Controller bind remapping

The player chooses which VR controller input performs each game action: F10 > Controls >
**Button mapping** (a list per action, a "Press to set" capture from the controllers, a conflict
warning, "Swap sticks", "Reset to default layout"), the ini section `[ControllerBinds]`, or the
seam word `binds`. Every change is live and saved at once.

Code: `src/core/input/controller_binds.h` (header only, host-tested), applied in
`src/core/input/pad_bridge.cpp` (`UpdateVirtualPad`) and `src/game/dishonored/ue3/ui_surface.cpp`
(the wheel's grip check). Config: `config.cpp` (`BindsSet`, and the load beside `[Controllers]`).
F10: `overlay_tabs.inc` (`OvlBindsSection`). Host test: `tools\controller-binds-host.ps1`.

## 1. How it works: the snapshot is remapped, not the pad bits

The runtime layer publishes the PHYSICAL controller state (`dvr::vr::InputSnapshot`: `a b x y`,
stick clicks, menu, grips, triggers, sticks, thumbrests). `dvr::binds::apply()` turns it into a
LOGICAL snapshot of the same type, where each field means an action: `a` is "the source bound to
Jump", `gripL` is "the power wheel's source". The pad bridge reads only the logical snapshot from
the point the layout is applied, so everything downstream follows a remap without knowing about it:

| Action (ini key) | Snapshot field | Shipped source | Game input | Mod systems that follow it |
|---|---|---|---|---|
| `Jump` | `a` | A | A | |
| `Stealth` | `b` | B | B | slide assist (B at full run); physical crouch's "button owns the stance" |
| `Interact` | `x` | X | X | X+Y pause chord |
| `Lean` | `y` | Y | Y | lean with the opposite stick; X+Y pause chord |
| `Sprint` | `clkL` | LeftStickClick | L3 | `SprintBit` |
| `Health` | `clkR` | RightStickClick | (hold) | `HealthElixirTick` |
| `PauseJournal` | `menu` | Menu | Start / Back | the composer's tap / hold |
| `PowerWheel` | `gripL` | LeftGrip | LB | `g_wheelHeld` (aim and crouch gates), the UI surface's wheel release |
| `Choke` | `gripR` | RightGrip | RB | |
| `Power` | `trigL` | LeftTrigger | LT | carry/throw swap, motion aim, fire tracer |
| `Attack` | `trigR` | RightTrigger | RT | the motion sword's real-trigger note, drop takedowns, carry/throw, motion aim |

Sources: `A B X Y LeftStickClick RightStickClick Menu LeftGrip RightGrip LeftTrigger RightTrigger
None` (short forms `L3 R3 LT RT LG RG` also parse; case does not matter). `SwapSticks=1` moves the
move stick to the right and the turn stick to the left; the D-pad, lean, the wheel and snap turn
follow it because they read the logical sticks.

- A button action on a grip or trigger presses past half travel. A grip or trigger action on a
  button reads 1 or 0; the grips' 0.9 / 0.7 hysteresis in the pad bridge still applies.
- Two actions may share a source (F10 shows it in orange, the log warns). `None` never presses.
- **With every action on its shipped source and the sticks unswapped, `apply()` returns its input
  unchanged.** The host test checks that over all 16384 combinations of buttons, rests, grips and
  triggers, and that every downstream consumer therefore sees exactly today's values.

## 2. What is deliberately not remapped

- **The both-stick-clicks chord** (tap = F10 panel, hold = recenter). It is resolved in the runtime
  layer before the snapshot is published, so it stays physical and cannot be lost by a remap.
- **The thumbrests.** They are the D-pad modifier's gesture and already have their own setting.
- **The R3 D-pad modifier** always reads the physical right stick click. It takes Health's slot as
  before; an action bound to the right stick click while R3 is the modifier is warned about.
- **The zoom (2026-10-07)** is not an action in the layout: the game's zoom button is the right stick
  click, which the modifier and the elixir hold both consume, so nothing ever sent it. A TAP of the
  physical right stick click (released before `[Input] HealthElixirHoldMs`, no D-pad pick, not the
  both-sticks chord) pulses the pad's right thumb button for 120 ms, the game's own toggle
  (`[Controllers] ZoomTap`, `zoomtap on|off`). So does the right trigger with the right controller
  held at the right eye, within `[Controllers] ZoomGestureRadiusCm` (25) of the head pose moved half
  the IPD plus `ZoomGestureRightCm` (8, the temple) along its right axis; the press is swallowed from
  the trigger's first movement, so the game never sees it as an attack or an unholster (`[Controllers] ZoomGesture`,
  `zoomgesture on|off`, `zoomgesture radius <cm>`). Both log `zoom:` with the reason. F10 Controls.
- **The F10 pointer** reads the physical snapshot: the pointing hand's trigger clicks, its stick
  scrolls. With a custom layout, that trigger is muted for every action while the panel is up.
- **SteamVR's own bindings.** On the SteamVR shim, SteamVR's controller binding UI is applied first;
  this layer remaps whatever arrives.
- **The hand swap (2026-10-20, `[Hands] SwapHands`, HAND_SWAP.md)** happens BEFORE this layer, in
  the action layer: with it on, the snapshot's `trigL/gripL` are the right controller's and
  `trigR/gripR` the left's, so the shipped layout attacks with the left trigger and fires with the
  right, and a source named `LeftTrigger` means the trigger of the controller holding the game's
  left hand. Sticks, face buttons, stick clicks and thumbrests are never swapped. The F10 pointer
  (`[Overlay] PointerHand`) names a controller and converts back, so its trigger mute follows it.

- **Menus use the shipped layout (2026-10-29, `[Controllers] MenuDefaultButtons=1`, `binds menus
  on|off`, F10 Controls > Button mapping > "Menus use the standard buttons").** A custom layout used
  to follow the player into the menus, where every prompt names the pad's own buttons: with Lean
  unbound nothing produced pad Y and the journal's "Show history" (`OnToggleObjectivesListHistory`)
  could not be pressed (a headset run: 0 Y presses in four journal visits). In the pause menu, the
  journal and other menus (not the power wheel, which the bound grip opens, and not the F10 panel)
  the shipped layout applies; `input/binds: a menu is open ...` and `... back to the custom layout`
  mark the switch. An input still held across the switch (the X held to open the journal, the B
  that closes it) is ignored until it is let go, so it neither presses the menu's X nor crouches on
  the way out. Simulator: LB and RB switched tabs, Y toggled the history, LT the sub-tab, B closed
  the journal and resumed from pause.
- **The wheel is never a menu to that switch (2026-10-31).** Its own screen raises the pad's MENU
  mode (`g_inMenu`), which the first version did not exclude: the layout switched one frame after
  the wheel opened, the grip holding it was masked as held across, and the wheel shut. Now, while the
  wheel is held or up and for 250 ms after, nothing it raises counts as a menu (TRAPS 2026-10-31).
- **The wheel hold is gameplay only (2026-10-29).** The power wheel's grip hold also swallowed every
  quick LB press in menus (the journal's previous tab); in a menu the grip is LB at once.
- **The Choke action chokes the guard you are behind (2026-11-01, `choke_assist.cpp`, `[Choke]`).**
  The game chokes on a HELD Block only while its interaction focus is an unaware guard seen from
  behind (ENGINE_NOTES 2026-11-01), and that focus follows the hand ray, so the grip used to block.
  Now, every frame, the nearest living guard within `ReachCm` (150), with you within `BehindDeg` (90)
  of its back and it within `FrontDeg` (60) of your view, is the candidate (`WorldInfo.PawnList`).
  The Choke action pressed with a candidate holds RB back, turns the interaction trace from the camera
  onto that guard (ahead of the pickup's own steering), and presses RB the moment the game focuses
  it (5-17 ms measured); once the game is choking, RB stays down by itself until the choke's ending
  (Win, Lose, Cancel; at most 8 s), so a quick squeeze completes it. No focus within `FocusWaitMs`
  (250): a held grip blocks as it always did. No candidate: the grip blocks at once, unchanged, so a
  front fight keeps its instant block. When the Choke grip ALSO has something to take (a key on that
  guard's belt was the pointed target through a whole headset approach), it has two jobs and the
  game's own choke being a hold decides between them: nothing is pressed on the squeeze, a release
  within `HoldToChokeMs` (250) taps Interact and takes the thing, and a grip still down at 250 ms
  goes to Block as the choke (`dvr::choke::TapHold`; `pickup: ... undecided`, `... a tap: the take`,
  `... held 250 ms - the choke`). The cost is that choke starting 250 ms later, only when both jobs
  are on offer; the other grip takes things as always. A swing of the ranged hand at the sword's speed does the same
  (PHYSICAL_SWING, `[Choke] RangedSwing`). Pure core `choke_core.h`, host `tools\choke-host.ps1`
  (254 checks). Simulator, the player's save: grip `HONOURED after 31 ms` -> `WON ... after 2172 ms`
  (RB held for the choke alone 1688 ms after a 400 ms squeeze); ranged swing 4.46 m/s -> `WON` after
  2171 ms; a swing with nobody there `nothing pressed`; a grip with nobody there blocked at once.
  Words: `choke status|near|assist on|off|swing on|off|reach|behind|front|wait`; F10 Controls >
  "Choke (non-lethal)".

## 3. Ini, seam and log

```ini
[ControllerBinds]
Jump=B
Stealth=A
SwapSticks=0
```

An absent key keeps its action on the shipped source, so an ini without the section is the shipped
layout and nothing is added to the default ini. "Reset to default layout" (and `binds reset`)
DELETES the section. An unknown value logs a `WARN` and keeps the shipped source.

Seam: `binds status`, `binds reset`, `binds swap on|off`, `binds <Action> <Source>`
(`binds Jump RightTrigger`).

Log: one line at load, on every change and on `binds status`:
`input/binds (config): Jump=A Stealth=B ... SwapSticks=0 | the shipped layout, the snapshot passes
through untouched`, or `... Jump=B* ... | CUSTOM (* = moved from the shipped source)`, followed by one
`WARN ... share ...` line per conflict. Press-to-bind logs its start, the result
(`Jump = RightTrigger (was A) by press-to-bind`) or its timeout.

## 4. Press to set

"Press to set" waits until every controller input is released (the click that started it is still
down), then takes the first input pressed past 0.6 (a button, a grip, a trigger or a stick click),
within 6 s. While it waits the game receives no buttons, grips or triggers, so the captured press
does not also jump or fire; that mute is a deadline (`captureUntilMs`), so closing the panel or
switching tabs mid-capture cannot leave the pad silent. The mute holds until the captured press is
released.

## 5. Verification

- Host: `tools\controller-binds-host.ps1` (32948 checks: the shipped layout is an identity for
  every combination, pack/parse round-trips, a swap moves the action, analog/button conversion,
  unbound, stick swap, the pointer mute, press-to-bind edges, conflicts).
  `tools\controller-emulation-host.ps1` still reads the production face-button block.
- Not run: the game, the simulator or the headset. A simulator check would drive `press a` /
  `grip r 1.0` through `tools\xrsim-cmd.ps1` and read the `pad: xbtn=` line before and after
  `binds Jump B`.
- Headset check: the everyday actions on the shipped layout (nothing should change), one remap
  and back through F10 including "Press to set", physical crouch, and a sword swing.

## Physical pickup takes a grip press when loot is in reach

`[Aim] PhysicalPickup=1` (F10 > Interact, seam `pickup on|off`, `pickup reach <cm>`): while a
lootable item is within `PhysicalPickupReachCm` (30) of a hand and the game has focused it, a
press of that hand's PHYSICAL grip is taken before the remap. The grip reads 0 for the rest of
that press, so the action bound to it (the power wheel or Choke in the shipped layout) does not
fire, and the logical Interact is held for 130 ms instead. A grip that was already held when
the hand arrived is not taken; the Interact button itself is untouched. Log: `pickup: LEFT|RIGHT
grip pressed ... grip swallowed, Interact pressed`. Books and notes use
`PhysicalPickupBookReachCm` (45), and a page opened this way attaches to the hand that opened it.
With `[Aim] PhysicalDoors=1` (F10 > Interact, `pickup doors on|off`) the same grip opens or closes a
door within `PhysicalDoorReachCm` (20) of the hand. `[Aim] PhysicalCarry=1` (`pickup carry
on|off`) does the same for things carried and thrown, and `[Aim] PhysicalUsables=1` (`pickup
usables on|off`) for levers, switches, valves, chains and placed traps, both at the door reach
from the object's collision box. On a usable the grip HOLDS Interact for as long as it stays
down on the same focused target (130 ms at least), and lets go the moment the target is lost,
because Interact held with nothing focused sheathes the weapon. People are never a target, so
the grip's own Choke is untouched. While an object is carried the grip is never taken (it
throws or drops as bound); while a body is carried only the free hand can interact. The
Interact button works on every one of these exactly as before. The plan for the rest (the
weapon hidden and the hand open while eligible) is PLAN-physical-interaction.md.

## Pointing grab, the wheel hold and the swap gesture (2026-10-21)

**Pointing grab (`[Aim] PointGrab=1`, `PointGrabRangeCm=250`).** Both hands carry a dot: the main
hand's is the aim ray as before, and the other (sword) hand's is the weapon hand's dot carried
across: the main ray's final direction expressed in the main grip's frame, reflected across x and
placed on the other grip, so both dots leave their hands at the same angle (`aim_ray.cpp`
`fire_frame_hand`, `xrmath::mirror_dir_between`, published only while PointGrab asks for it). The
first build turned the other controller's AIM pose by the -48 deg other-items offset, which is tuned
against the model axis, and the sword hand's dot pointed nearly straight down in the headset.
`[Aim] PointGrabReticles` (`both`, `weapon`, `hand`; F10 Aim > "Reticles shown"; seam `pickup point
reticles both|weapon|hand`) picks which dot is always up. Since 2026-10-29 both hands point whatever
it says (a headset run with `weapon` had 35 pointed grabs, all on the gun hand, while the sword
hand reached 60 cm), and a hidden dot comes back while its hand rests on something its grip would
take (`dvr::aim::set_point_offer` from the pickup gate). With `hand` the weapon hand's ray still
aims the guns, Blink and throws.

**Throwing from the sword hand (2026-10-29).** A carry in the right hand is thrown by the game's
attack, and a layout with `Attack=None` (motion-sword players) left that hand no way to throw.
While the right hand carries, its own physical trigger is the throw whatever Attack is bound to,
unless another action is on the right trigger (`carry/aim: the right hand carries and Attack is on
...`); a motion-sword swing never throws a carry from either hand. Simulator: a right-grip pickup,
the right trigger threw it along the hand (0.8 deg off the hand ray).
A listed interactable (any of the kinds above, people never) that a hand's ray meets within the
range is that hand's pointed candidate: the ray is tested against the collision box grown by 3 cm
plus 2.5 degrees of the distance, or a 10 cm ball for a thing without a box. A thing in palm reach
still wins over a pointed one. From there the path is the physical pickup's: the interaction trace
runs from that palm to the target, and the hand's grip is swallowed and becomes Interact while the
game focuses it. The main hand also offers its grip as Interact on anything else the game focuses
on its dot (not a pawn, checked by class chain), so every interaction is reachable, not only the
listed kinds. So whenever a dot rests on something interactable, the grip is the grab, and its
bound action (Choke or the power wheel in the shipped layout) does not fire for that press. The
open-hand pose is kept for things in reach; a distant grab does not open the hand. F10 Aim >
Reach and grab > "Point either hand at things to grab them" and "Pointing range (cm)"; seam
`pickup point on|off`, `pickup point range <cm>` (50 to 400). Log: `pickup: target ... POINTED AT
by that hand's dot (PointGrab)` and the beat's `pickup: PointGrab ON - range ..., rays L ok R ok`.
A pointed target the game does not focus within a quarter second (beyond the engine's own
interaction reach, or behind something) is set aside for 3 s (1 s for a reached one), so the next
thing on the dot can win; the release line says which.

**Every interactable (2026-10-21).** Three gaps closed after a headset run:
- *Interact follows the grip.* It used to be a fixed 130 ms tap except on usables; it is now held
  for as long as the grip is (130 ms at least) on every target, and released when the target is
  lost for more than 150 ms. The game reads the duration: a tap opens a door, a hold of about a
  third of a second peeks through its keyhole (two 130 ms grip taps at a keyhole door did nothing,
  the held Interact button peeked 330 ms in). While peeking, either grip taps Interact, which leaves
  the peek; a tap only, so a held grip cannot start the next one.
- *People and bodies.* The grip refused every pawn. A focused BODY (`Pawn.Health` 0 or less, or no
  `Pawn.Controller`) is now taken on either grip (search, carry); a living person (talk, pickpocket)
  only on a grip not bound to Choke, so a choke from behind stays a choke. `pickup: the engine
  focuses ... - a BODY | a living person (Health, controller)` says which, once per focus. If an
  unconscious body reads as living, the line shows it and the left grip still works.
- The beat counts `grips offered on a focused person or body`; `pickup pawns` lists the nearest
  NPCs with the readings the body test uses.
- Simulator, in the game (2026-10-21): a held grip peeked through a keyhole and a tap of the other
  grip left it; the Choke grip carried a dead NPC on a long squeeze; on a living civilian the left
  grip was Interact and the right grip stayed the game's own (it blocked). ENGINE_NOTES 2026-10-21.

**The wheel hold (`[Controllers] WheelHoldToOpen=1`, `WheelHoldMs=1000`).** The power wheel's
grip opens the wheel only once it has been held `WheelHoldMs`; a shorter squeeze never reaches the
pad, so grabbing something cannot open the wheel. 0 opens it on the press, as before. F10 Controls >
"Hold the grip to open the tool wheel" and "Hold time (ms)"; seam `wheelhold on|off|<ms>`. Log:
`wheel: grip held N ms - the wheel opens` or `wheel: grip released after N ms, under the ... hold -
the wheel stays shut`.

**The swap gesture (`[Controllers] SwapHandsGesture=right`).** A double click of the physical right
stick toggles `[Hands] SwapHands` (HAND_SWAP.md section 1); `left` moves it to the left stick and
`off` removes it. The right stick's single tap still zooms, 350 ms later.
