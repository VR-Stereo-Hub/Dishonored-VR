"""Median / p90 of the perf: tick fields over a log slice. Usage: tickstats.py <log> [skipLines] [label]"""
import re, sys, statistics as st

path = sys.argv[1]
skip = int(sys.argv[2]) if len(sys.argv) > 2 else 0
label = sys.argv[3] if len(sys.argv) > 3 else path
maxw = int(sys.argv[4]) if len(sys.argv) > 4 else 10**9

tick_re = re.compile(r"perf: tick ([\d.]+) ms \(([\d.]+)/s, (\d+) presents/s\)")
p_re = re.compile(
    r"P(\d)\[[-+]1\] n=\d+ in ([\d.]+) \(pre ([\d.]+) begin ([\d.]+) (?:\[wait ([\d.]+)\] )?tick ([\d.]+) method ([\d.]+) "
    r"\[cap ([\d.]+): lock ([\d.]+)[^\]]*\] end ([\d.]+) (?:\[acq [\d.]+ xrCopy [\d.]+ endFrame ([\d.]+)\] )?present ([\d.]+)\) "
    r"\+ out ([\d.]+) \(idle ([\d.]+) R ([\d.]+)\)")
gpu_re = re.compile(r"perf: gpu/present render-to-entry=([\d.]+) ms.*per tick span=([\d.]+) dma=([\d.]+) idle=([\d.]+)")
beat_re = re.compile(r"stereo: beat method=(\w+) out/s=(\d+) L/s=(\d+) R/s=(\d+)")

rows, gpus, beats, banner = [], [], [], ""
with open(path, "r", errors="replace") as f:
    for i, line in enumerate(f):
        if i == 0:
            banner = line.strip()
        if i < skip:
            continue
        m = tick_re.search(line)
        if m and len(rows) >= maxw:
            break
        if m:
            tick = float(m.group(1))
            if tick >= 60.0:
                continue
            d = {"tick": tick, "rate": float(m.group(2))}
            for pm in p_re.finditer(line):
                k = "P" + pm.group(1)
                names = ["in", "pre", "begin", "wait", "tickf", "method", "cap", "lock", "end", "endFrame", "present",
                         "out", "idle", "R"]
                for n, v in zip(names, pm.groups()[1:]):
                    d[k + "." + n] = float(v) if v is not None else 0.0
            d["starved"] = 1.0 if "STARVED" in line else 0.0
            d["pacebound"] = 1.0 if "PACE-BOUND" in line else 0.0
            rows.append(d)
            continue
        g = gpu_re.search(line)
        if g:
            gpus.append([float(x) for x in g.groups()])
            continue
        b = beat_re.search(line)
        if b and int(b.group(3)) > 0:
            beats.append(int(b.group(3)))


def med(xs):
    return st.median(xs) if xs else float("nan")


def p90(xs):
    if not xs:
        return float("nan")
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(round(0.9 * (len(xs) - 1))))]


print("== %s" % label)
print("   " + banner[:200])
print("   windows=%d  tick median %.1f / p90 %.1f ms  rate median %.1f/s  L/s median %.0f  starved %d  pace-bound %d" % (
    len(rows), med([r["tick"] for r in rows]), p90([r["tick"] for r in rows]), med([r["rate"] for r in rows]),
    med(beats), sum(r["starved"] for r in rows), sum(r["pacebound"] for r in rows)))
for k in ("P1", "P2"):
    fields = ["in", "pre", "begin", "tickf", "method", "lock", "end", "present", "out", "idle", "R"]
    vals = ["%s %.1f" % (f, med([r.get(k + "." + f, 0.0) for r in rows])) for f in fields]
    print("   %s median: %s" % (k, "  ".join(vals)))
if gpus:
    print("   gpu median: render-to-entry %.1f  span/tick %.1f  dma %.1f  idle/tick %.1f  (n=%d)" % (
        med([g[0] for g in gpus]), med([g[1] for g in gpus]), med([g[2] for g in gpus]), med([g[3] for g in gpus]),
        len(gpus)))
