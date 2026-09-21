"""What does the log print in the slow half of the 12 s cycle that it does not print in the fast half?
Buckets every line after `skip` by the perf window it falls in (3 s windows closed by a perf: tick line), labels
each window slow (tick >= thr) or fast, and prints message prefixes ranked by (slow count - fast count)."""
import re, sys, collections

path, skip = sys.argv[1], int(sys.argv[2])
thr = float(sys.argv[3]) if len(sys.argv) > 3 else 21.0
ts_re = re.compile(r"^\[\s*(\d+)\] \[\w\] \[(\w+)\] (.*)$")
norm = re.compile(r"[-+]?\d+(\.\d+)?|0x[0-9A-Fa-f]+|[0-9A-F]{8}")

win_lines, windows = [], []
gaps = []
for i, l in enumerate(open(path, errors="replace")):
    if i < skip:
        continue
    m = ts_re.match(l.rstrip())
    if not m:
        continue
    msg = m.group(3)
    if "perf: frame gap" in msg:
        gaps.append((int(m.group(1)), msg[:230]))
    t = re.search(r"perf: tick ([\d.]+) ms", msg)
    if t:
        windows.append((float(t.group(1)), win_lines))
        win_lines = []
        continue
    key = m.group(2) + " | " + norm.sub("#", msg)[:70]
    win_lines.append(key)

slow = collections.Counter()
fast = collections.Counter()
ns = nf = 0
for tick, lines in windows:
    if tick >= thr:
        ns += 1
        slow.update(lines)
    elif tick <= thr - 2.0:
        nf += 1
        fast.update(lines)
print("windows: %d slow (tick >= %.1f), %d fast (tick <= %.1f)" % (ns, thr, nf, thr - 2.0))
keys = set(slow) | set(fast)
rank = sorted(keys, key=lambda k: -(slow[k] / max(ns, 1) - fast[k] / max(nf, 1)))
print("per-window line rate, slow vs fast, top by difference:")
for k in rank[:22]:
    print("  %6.1f  %6.1f   %s" % (slow[k] / max(ns, 1), fast[k] / max(nf, 1), k))
print("frame gaps in the slice: %d" % len(gaps))
for t, g in gaps[:12]:
    print("  [%d] %s" % (t, g))
