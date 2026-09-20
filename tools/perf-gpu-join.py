"""Join 1 s GPU 3D-engine samples (tickcount,percent) to the log's 3 s perf windows."""
import re, sys

log, csv = sys.argv[1], sys.argv[2]
gpu = []
for l in open(csv):
    a, b = l.strip().split(",")[:2]
    gpu.append((int(a), float(b)))
wins = []
for l in open(log, errors="replace"):
    m = re.search(r"\[\s*(\d+)\].*perf: (tick|present) ([\d.]+) ms \(([\d.]+)/s", l)
    if m:
        idle = re.search(r"\(idle ([\d.]+) R ([\d.]+)\)", l)
        lock = re.findall(r"lock ([\d.]+)", l)
        wins.append((int(m.group(1)), m.group(2), float(m.group(3)), float(m.group(4)),
                     float(idle.group(1)) if idle else -1, sum(float(x) for x in lock)))
lo, hi = gpu[0][0], gpu[-1][0]
print("window_end  kind   ms    rate  P1idle  lock/tick  gpu3D%(mean of the 3 s)")
for t, kind, ms, rate, idle, lock in wins:
    if t < lo or t - 3000 > hi:
        continue
    vals = [v for (g, v) in gpu if t - 3000 <= g <= t]
    if vals:
        print("%10d  %-7s %5.1f %5.1f  %5.1f   %5.1f      %5.1f  (n=%d)" % (t, kind, ms, rate, idle, lock,
                                                                          sum(vals) / len(vals), len(vals)))
