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
- **The F10 pointer** reads the physical snapshot: the pointing hand's trigger clicks, its stick
  scrolls. With a custom layout, that trigger is muted for every action while the panel is up.
- **SteamVR's own bindings.** On the SteamVR shim, SteamVR's controller binding UI is applied first;
  this layer remaps whatever arrives.

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
