"""Print the perf: tick series (seconds since the first window : tick ms, * = RENDER THREAD STARVED, idle of P1)."""
import re, sys

path, skip = sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 0
rows = []
for i, l in enumerate(open(path, errors="replace")):
    if i < skip:
        continue
    m = re.search(r"\[\s*(\d+)\].*perf: tick ([\d.]+) ms", l)
    if not m:
        continue
    idle = re.search(r"P1\[-1\].*?\(idle ([\d.]+) R ([\d.]+)\)", l)
    rows.append((int(m.group(1)), float(m.group(2)), "STARVED" in l, float(idle.group(1)) if idle else -1.0))
if rows:
    t0 = rows[0][0]
    print(" ".join("%d:%.1f%s(i%.1f)" % ((t - t0) // 1000, v, "*" if s else "", idle) for t, v, s, idle in rows))
