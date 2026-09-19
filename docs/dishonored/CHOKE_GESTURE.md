# Physical choke (VR-145)

## The request

A choke by gesture: the right hand brought QUICKLY up to the left shoulder
starts it, and it lasts only while the hand is held close to that shoulder.

## Run503 correction and current gate (2026-09-19)

The prior output claim below is only valid for pad binding set 1. Run503 held RB
without entering StatePlayerMasterChoke; set 2 maps RB to attack. The first
candidate from CODEX_PLAN_VITALS_CHOKE.md changes only vitals ownership and
diagnostics. The existing 9 choke host checks pass, but they cannot establish
the active game binding or successful choking. Output research and three-try
calibration follow as a separate candidate; do not interpret RB held as success.

## What the game offers

The choke is not its own button. `DishonoredInput.ini` binds
`GBA_Block: Button m_bBlockButton | Button m_bChokeButton`, and the mod's pad
already sends it as RB from the right grip (`pad_bridge.cpp`, "right grip =
choke"). Held behind an unaware target, the game chokes; elsewhere RB blocks.
The choke's own state is `StatePlayerMasterChoke`, and releasing RB ends it.
So the gesture only has to HOLD RB for exactly as long as the choke should
last. No engine write, no new hook.

## The design

- The shoulder is modelled from the head pose: yaw-only (a nod or a tilt must
  not swing it), 0.17 m left, 0.22 m down, 0.04 m back. Arm length and posture
  differ, so all three are sliders.
- Start: the right controller comes within `EnterM` (0.14 m) of that point, and
  it moved at `MinSpeed` (0.9 m/s) or faster within the last `WindowMs`
  (400 ms). The speed gate is what makes it a deliberate move and not a hand
  resting near the chest.
- Hold: RB stays down while the hand is within `ExitM` (0.22 m). The wider exit
  distance is hysteresis, so a small wobble does not drop the choke.
- Release: the hand leaves the zone, or a menu, the wheel, the F10 overlay or
  lost tracking. A release never holds RB on into a menu.
- Output: ORed into the pad's RB. The grip still chokes as before.

## Levers (`[Choke]`, F10 Controls, saved on change)

| Key | Default | Meaning |
|---|---|---|
| `Gesture` | 0 | the gesture on/off (a new lever: off in the shipped ini) |
| `EnterM` / `ExitM` | 0.14 / 0.22 | start within / release beyond, metres from the shoulder point |
| `MinSpeed` / `WindowMs` | 0.9 / 400 | the quick move: peak hand speed within the window before entering |
| `ShoulderLeftM` / `DownM` / `BackM` | 0.17 / 0.22 / 0.04 | the shoulder point from the head |

F10 shows the live hand distance, speed and HELD/off, so the zone can be tuned
by watching the numbers.

## Logging

`choke: START` (distance, window) and `choke: RELEASE` (why: the hand left, or
a menu/wheel/overlay/tracking) at every edge; an idle line at Debug on the pad
lane. Host tests: `tools/choke-gesture-host.ps1` (shoulder placement under yaw
and nod, a slow drift never starts, a quick move starts, hysteresis holds,
leaving releases, not-allowed and off never start).

## Not in this step

No check that a chokeable target is actually in front: the game decides that
from RB, exactly as with the grip. If the gesture fires RB as a block where the
player did not mean one, the next step is to gate it on the game's own "choke"
prompt.
