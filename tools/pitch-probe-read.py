#!/usr/bin/env python3
"""pitch-probe-read.py - VR-173 prerequisite 1: does the ENGINE's view pitch follow the head
through a sword attack?

The sibling BioShock mod wrote the rendered pitch from the head and discarded the engine's
own value, so the engine's pitch froze (measured there at -88.9 degrees) and melee, which
the engine aims from its own view, landed on the floor. Nothing on screen showed it. This
reads the rows `camshake capture <s> <tag>` already records, one per game tick, from the
fresh branch of the head write BEFORE the write:

  inP    the pitch the engine handed the event this tick
  prevP  the pitch the mod wrote on the previous tick (the head's pitch, clamped)
  ctrlP  the player controller's own Actor.Rotation pitch, read at the same moment
  povP   the camera's point-of-view pitch
  state  master|upper animation state

and answers three questions per capture, over all rows and over the attack rows alone:

  retention   inP  - prevP   did the engine keep what the head wrote?
  controller  ctrlP - prevP  does the controller's own rotation hold the head's pitch?
  camera      povP - prevP   does the camera's?

A frozen engine pitch reads as a retention or controller error that GROWS with the head's
pitch, so a capture with the head level cannot fail and is not evidence by itself: run it
with the head pitched as well (tools/xrsim/pitch-probe.xrs does).

Usage:  python tools/pitch-probe-read.py <camshake-*.csv> [more.csv ...] [--limit 1.0]

The CSV is the game's own state and is never committed; this tool and its verdict are.
"""
import csv
import statistics
import sys

UNITS_PER_DEG = 65536.0 / 360.0


def wrap_deg(units):
    d = (units / UNITS_PER_DEG + 180.0) % 360.0 - 180.0
    return d


def quantile(values, p):
    v = sorted(values)
    return v[min(len(v) - 1, int(p * len(v)))]


def describe(name, values, limit):
    if not values:
        return "  %-10s no rows" % name, None
    mags = [abs(x) for x in values]
    over = sum(1 for x in mags if x > limit)
    p90 = quantile(mags, 0.9)
    line = ("  %-10s n=%d median %.3f p90 %.3f max %.3f deg, %d row(s) over %.1f deg, signed mean %+.3f"
            % (name, len(values), statistics.median(mags), p90, max(mags), over, limit, statistics.mean(values)))
    return line, p90


def read(path, limit):
    rows = []
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            if r.get("havePrev") != "1":
                continue          # no previous write to compare with: the first tick, or after a refusal
            rows.append(r)
    print("%s: %d row(s) with a previous write" % (path, len(rows)))
    if not rows:
        print("  NOTHING TO JUDGE: the capture holds no tick that followed a head write")
        return None
    head = [wrap_deg(int(r["prevP"])) for r in rows]
    print("  head pitch written: %.1f to %.1f deg (a span near 0 means a level head, which cannot show a frozen pitch)"
          % (min(head), max(head)))
    states = {}
    for r in rows:
        states.setdefault(r["state"], []).append(r)
    print("  states seen: " + ", ".join("%s (%d)" % (k, len(v)) for k, v in sorted(states.items(), key=lambda kv: -len(kv[1]))))
    attack = [r for r in rows if "MeleeAttack" in r["state"] or "Fatality" in r["state"] or "Assassinate" in r["state"]]
    # What the game adds on top of the controller's rotation (a weapon's kick, an attack's
    # own camera motion) lives in the camera and nowhere else. The control leg reads here.
    added = [wrap_deg(int(r["povP"]) - int(r["ctrlP"])) for r in rows]
    print("  camera minus controller, all rows: %.3f deg peak to peak (the game's own added pitch; the control leg's pistol kick shows HERE)"
          % (max(added) - min(added)))
    verdict = None
    for label, sel in (("ALL ROWS", rows), ("ATTACK ROWS", attack)):
        print(" %s (%d)" % (label, len(sel)))
        if not sel:
            print("  none: the capture never saw an attack state, so it says nothing about one")
            continue
        worst = 0.0
        for name, col in (("retention", "inP"), ("controller", "ctrlP"), ("camera", "povP")):
            vals = [wrap_deg(int(r[col]) - int(r["prevP"])) for r in sel]
            # A controller or camera column that is all zero while the head is pitched is a
            # field that was never read, not an engine that agrees.
            unread = all(int(r[col]) == 0 for r in sel)
            line, p90 = describe(name, vals, limit)
            print(line + ("  <- column is 0 on every row: NOT READ, not evidence" if unread else ""))
            if p90 is not None and not unread and name != "camera":
                worst = max(worst, p90)
        if label == "ATTACK ROWS":
            verdict = worst
    if verdict is None:
        print(" VERDICT: NONE - no attack rows")
    elif verdict <= limit:
        print(" VERDICT: TRACKS - through the attack the engine's pitch stays within %.1f deg of the head's (p90 %.3f)" % (limit, verdict))
    else:
        print(" VERDICT: DIVERGES - through the attack the engine's pitch is off the head's by p90 %.3f deg (limit %.1f)" % (verdict, limit))
    return verdict


def main(argv):
    limit = 1.0
    paths = []
    i = 0
    while i < len(argv):
        if argv[i] == "--limit" and i + 1 < len(argv):
            limit = float(argv[i + 1]); i += 2
        else:
            paths.append(argv[i]); i += 1
    if not paths:
        print(__doc__)
        return 2
    bad = 0
    for p in paths:
        v = read(p, limit)
        if v is None or v > limit:
            bad += 1
        print()
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
