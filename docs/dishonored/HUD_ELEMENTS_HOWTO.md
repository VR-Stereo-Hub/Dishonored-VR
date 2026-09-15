# How to measure and add a HUD element

For anyone who wants to give one more piece of the game's HUD its own anchor (the
equipment icons, subtitles, the pickup toast, a tutorial window, the detection arrows,
the skip gauge, dark vision, or something nobody has named yet). Read
`HUD_ANCHORS.md` sections 1 and 2 first for what the pieces are; this file is the
recipe. Nothing here needs a headset: the simulator or a flat run of the game is enough,
because the measurement is a screen rectangle.

## 0. How an element is identified (the one idea)

The mod cannot see what a HUD draw IS. For every piece the game paints it sees only WHERE
on the screen it was painted: a rectangle in normalised backbuffer coordinates, x and y
from 0 to 1, y down, read from the draw's vertices through the vertex shader's own
transform (ENGINE_NOTES, "How the Scaleform HUD identifies its elements").

The layout is a table of rows (`src/core/gfx/hud_layout.cpp`, `kRows`). A row claims a
draw when the draw's CENTRE lies inside the row's rectangle. A draw no row claims goes to
the `default` row. So "adding an element" means: find the rectangle its draws land in,
write that rectangle on its row. The in-game screens (pause, note, journal, wheel, store,
mission stats) are the exception: they are claimed by the UI owner context while they
ride, whatever their draws' rectangles.

Three rows ship with a rectangle: `vitals`, `reticle`, `prompt`. Eight rows ship without
one: `equipment`, `subtitles`, `objective`, `toast`, `tutorial`, `detection`,
`skipgauge`, `darkvision`. Those eight exist so a measurement is one ini line away.

## 1. Measure it in the game (ten minutes)

1. Get the element ON SCREEN. Play to a place where it draws (pick something up for the
   toast, stand in a conversation for subtitles, get spotted for the detection arrows).
   The simulator's sewer level shows only the vitals, the reticle, the prompt and an
   objective marker; the rest need a real level, which is why they are unmeasured.
2. Turn the census and the probe on while it is showing:

       tools\game-cmd.ps1 "draws on"
       tools\game-cmd.ps1 "hud regions on"

   (`hud regions` is on by default in the shipped ini; `draws on` is the measuring lever
   and costs a lookup per draw, so turn it off afterwards.)
3. Wait three seconds, then ask for the table:

       tools\game-cmd.ps1 "draws regions"

4. Read `dishonored_vr.log` (`tools\tail-log.ps1 -Grep "draws/cluster"`). Every line is one
   element candidate, by frequency:

       draws/cluster:   b=daba5b5f rect=[0.524,0.481 - 0.774,0.602] (0.250 x 0.121, centre 0.649,0.541) n=1.0/present in 381 of 382 presents tex=none -> default

   `rect` is the union of the draws that cluster there, `n` how many draws per present,
   `in N of M presents` how steadily it draws, `tex` whether it is textured (glyphs and
   icons) or a plain fill (bars, plates), and the arrow says which row claims it today.
   The element you want is the cluster (or group of clusters) that appears when the
   element appears and goes when it goes. Toggle the element in the game and take the
   table twice if you are not sure: the cluster that comes and goes is yours.
5. Write the claiming rectangle: the union of the element's clusters with a small margin
   (two to three percent) so a bar that fills or a label that grows stays inside, and
   make sure no OTHER element's centre falls in it. Then name it live:

       tools\game-cmd.ps1 "hud region toast 0.30,0.85,0.70,0.95"

   The row starts claiming at once and the rectangle is written to `[Hud] Region.toast`
   in the ini. Check it took: `tools\game-cmd.ps1 "hud list"` shows the row with its
   region and its `seen` count climbing; `draws regions` now shows the clusters with
   `-> toast`. Then set its anchor on the F10 HUD tab or with
   `tools\game-cmd.ps1 "hud anchor toast handR"` and look.

That is the whole measurement. Turn the census off (`draws off`) when you are done.

## 2. Bake it into the shipped defaults (one commit)

A rectangle in your ini helps you; putting it in the code helps everyone. One row, four
places, one host check:

1. `src/core/gfx/hud_layout.cpp`, `kRows`: replace the row's `{0, 0, 0, 0}` with the
   rectangle and say in `what` where it was measured (level, date, build):

       { "toast", -1, {0.300f, 0.850f, 0.700f, 0.950f}, false, AnchorWindow, "the pickup toast (measured on <level>, <date>)" },

2. `src/core/config/config.cpp`, `WriteDefaultIni`, the `[Hud]` block: add the key
   next to the other `Region.*` lines and extend the comment above them:

       "Region.toast=0.300,0.850,0.700,0.950\n"

3. Regenerate the golden and the packaged ini, and check they match:

       python tools\ini-golden.py
       copy tests\golden\dishonored_vr.ini release\dishonored_vr.ini      (the packaged ini is CRLF; see HUD_ANCHORS section 6 for the one-liner)
       python tools\ini-golden.py --check release\dishonored_vr.ini

4. `tools/hud-route-tests.cpp`: add the row to the test table and one `check(...)` with a
   rectangle you saw in the log routing to it, and one that must NOT (a neighbour). Run
   `tools\hud-route-host.ps1`.
5. Record it: the rectangle and the cluster lines in `HUD_ANCHORS.md` section 7 (numbers
   only, never a chat quote), the element's row in the table in ENGINE_NOTES "How the
   Scaleform HUD identifies its elements", and a line in `RELEASE_NOTES.md`.

Commit message shape: `feat: name the pickup toast's HUD region`.

## 3. Adding a row that does not exist yet

If the log shows a cluster that fits none of the nineteen rows, add a row:

1. `src/core/gfx/hud_layout.h`: add the name to `enum Element` BEFORE `ElVignette` for a
   gameplay element (the screens stay at the end), and keep `ElCount` last.
2. `src/core/gfx/hud_layout.cpp`, `kRows`: add the row at the same position (the enum
   order and the table order must agree; the enum index is the row index).
3. `src/core/config/config.cpp`, `WriteDefaultIni`: add `Element.<name>=window` beside the
   others (and `Region.<name>=...` if measured); regenerate the inis as above.
4. That is all the code: the F10 tab, `hud list`, `hud anchor`, `hud place`, the ini
   read and write, the status.json entries and the routing all come from the table.

A new SCREEN (something the UI owner reports as a context the table lacks) is the same
with `context` set to its `dvr::mono::Context` value instead of a rectangle; the ride
predicate in `ui_surface.cpp` and the opt-in mask in `hud_layout.cpp`
(`kMenuContextNames` / `kMenuContextBits`) need the new context too.

## 4. What a rectangle cannot do, and what to do instead

- **An element that moves with the world** (the objective marker, a waypoint) has no
  fixed rectangle. Claiming it needs a different identity: the Scaleform display object
  behind the draw (its instance name), which this build does not read. That is a
  research ticket (the handoff's route 2), not a region.
- **Two elements that overlap** cannot be split by a centre rule. The health and mana
  bars interleave in x on this build (fills centred 0.076 and 0.098, frames 0.077 and
  0.103, one background centred 0.081), which is why they ship as one `vitals` row.
  Check the cluster centres before promising a split.
- **The rows are visited in table order** and the first claiming row wins, so two rows
  whose rectangles overlap resolve by position in `kRows`. Keep rectangles disjoint.
- **The vignette rule runs first**: a draw wider AND taller than 60 % of the screen is
  `vignette` before any rectangle is tried. A full-screen fade never lands in your row.
- **Clusters are not collected while a screen rides** (the pause menu's glyphs alone
  overflowed the table), and clusters are quantised to 1/40 of the screen, so a slider
  that moves smoothly appears as several adjacent clusters: take their union.
- **A riding screen claims EVERYTHING** drawn while it is up, gameplay HUD included, by
  context. Measure gameplay elements with no screen open.

## 5. The budget

Each anchor in use costs one sink (a private target and a copy per present: 7.5 MB at the
shipped size) for its cropped elements and, if any rect-less row rides it, one more for
the catch-all. Each cropped element costs one quad layer sized to its crop, and the
headset runtime accepts 16 layers in total (the projection, the aim dot and the hand
visuals count too; the log's `xr: HUD quads N of M submitted (... layer budget K)` says
when the cap hides one). `kMaxSinks` is 12 and `kMaxHudQuads` 32; both are room, not
targets.

## 6. Reading the log

- `hud/layout: sink N = <anchor>/<crop|all>` and `... released`: which sinks exist and why.
- `hud/layout: routed this window: vitals=8900 reticle=445 default=120 | ...`: where the
  draws went in the last three seconds. A non-zero `default` outside a menu means an
  unnamed element is on screen: that is the signal to measure.
- `hud/list:` (from `hud list`): every row, its anchor, its region or "UNMEASURED", the
  draws it has seen this session.
- `draws/regions: ... 0 refused`: the probe read every draw; a `refused` count names why
  in the per-bucket lines (`no transform map` means a vertex shader the parser could not
  read: run `draws vsdump` and look at the disassembly it writes under the data dir).
- `xr: HUD quad[slot] live (<anchor> anchor, element E, crop x,y wxh, W x H m, D m from
  the eyes, subtends A deg)`: what the headset was actually given.
