"""Line up a net-ping-watch.ps1 trace against the mod log's xrEndFrame stalls.

The question (FLICKER_REFERENCE 2026-10-05, "xrEndFrame blocks 5-6 display periods every
5.66 s"): is the stall on the link (the headset stops answering at the same moments) or on
the PC (the link is quiet while the submit blocks)?

    python tools/ping-vs-log.py <pingwatch-*.csv> <dishonored_vr.log> [--window 150]

Both files use one clock: the csv's `tick` is [Environment]::TickCount64 and the log's
`[  NNNNNNNN]` column is GetTickCount, the same counter truncated to 32 bits, so ticks are
compared modulo 2^32. Reads only; prints only.

What it prints, and what each answer means:
  - the stalls inside the ping trace's span (`perf: frame gap NNms ... present-tail
    (xrEndFrame) ... endFrame X`), their period, and the ping spikes (RTT >= 3x the median
    or lost) with theirs;
  - COINCIDENCE: the share of stalls with a spike within +-window ms, against a NULL
    CONTROL - the same count with the spike train shifted by random offsets. A rate far
    above the control = the link (verdict LINK); a rate at the control = the PC side
    (verdict PC SIDE); too few stalls = no verdict. The control is what lets this
    instrument print the unwelcome answer: a dense spike train coincides with anything.
"""
import argparse, bisect, csv, random, re, statistics, sys

M32 = 1 << 32
GAP = re.compile(r"^\[\s*(\d+)\].*perf: frame gap (\d+)ms .*sat in: present-tail \(xrEndFrame\).*?endFrame ([\d.]+)")


def load_ping(path):
    rows = []
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            rows.append((int(r["tick"]) % M32, float(r["rttMs"])))
    return rows


def load_gaps(path):
    gaps = []
    runtime = "?"
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if runtime == "?" and "instance created on runtime" in line:
                m = re.search(r"instance created on runtime '([^']+)'", line)
                runtime = m.group(1) if m else "?"
            m = GAP.match(line)
            if m:
                gaps.append((int(m.group(1)), int(m.group(2)), float(m.group(3))))
    return gaps, runtime


def spikes_of(ping):
    ok = sorted(r for _, r in ping if r >= 0)
    if not ok:
        return [], 0.0, 0.0
    med = ok[len(ok) // 2]
    thr = max(3.0 * med, med + 5.0)
    raw = [t for t, r in ping if r < 0 or r >= thr]
    ev, last = [], None
    for t in raw:                                   # one event per 200 ms burst
        if last is None or t - last > 200:
            ev.append(t)
        last = t
    return ev, med, thr


def hits(stalls, spikes, window):
    n = 0
    for t in stalls:
        i = bisect.bisect_left(spikes, t - window)
        if i < len(spikes) and spikes[i] <= t + window:
            n += 1
    return n


def period(ts):
    d = [(b - a) / 1000.0 for a, b in zip(ts, ts[1:])]
    if not d:
        return "n/a"
    return "median %.3f s (min %.3f, max %.3f, n=%d)" % (statistics.median(d), min(d), max(d), len(d))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv"); ap.add_argument("log")
    ap.add_argument("--window", type=int, default=150, help="ms either side of a stall")
    a = ap.parse_args()

    ping = load_ping(a.csv)
    if not ping:
        sys.exit("empty ping trace")
    t0, t1 = ping[0][0], ping[-1][0]
    gaps, runtime = load_gaps(a.log)
    stalls = [g for g in gaps if t0 <= g[0] <= t1]
    spikes, med, thr = spikes_of(ping)
    lost = sum(1 for _, r in ping if r < 0)

    print("log runtime: %s   (a simulator log cannot answer this question)" % runtime)
    print("ping: %d samples over %.1f s, median %.1f ms, spike threshold %.1f ms, lost %d, %d spike events, period %s"
          % (len(ping), (t1 - t0) / 1000.0, med, thr, lost, len(spikes), period(spikes)))
    print("stalls inside the trace: %d (of %d in the log), endFrame median %s ms, period %s"
          % (len(stalls), len(gaps), "%.1f" % statistics.median([s[2] for s in stalls]) if stalls else "n/a",
             period([s[0] for s in stalls])))
    if len(stalls) < 5:
        print("VERDICT: none - fewer than 5 stalls inside the trace (did the trace run during play?)")
        return

    st = [s[0] for s in stalls]
    h = hits(st, spikes, a.window)
    rate = h / len(st)
    rng = random.Random(1)
    span = t1 - t0
    ctrl = []
    for _ in range(200):
        off = rng.randint(2000, max(2001, span - 2000))
        shifted = sorted(t0 + ((s - t0 + off) % span) for s in spikes)
        ctrl.append(hits(st, shifted, a.window) / len(st))
    c_mean = statistics.mean(ctrl)
    c_p95 = sorted(ctrl)[int(0.95 * len(ctrl))]
    print("COINCIDENCE: %d of %d stalls (%.0f%%) have a ping spike within +-%d ms; null control (spikes shifted) "
          "mean %.0f%%, p95 %.0f%%" % (h, len(st), 100 * rate, a.window, 100 * c_mean, 100 * c_p95))
    for t, gap, ef in stalls[:12]:
        i = bisect.bisect_left(spikes, t - 1000)
        near = [s - t for s in spikes[i:i + 4] if abs(s - t) <= 1000]
        print("  stall at %d: gap %d ms, endFrame %.1f ms, nearest spikes %s ms" % (t, gap, ef, near or "none within 1 s"))
    if rate >= 0.6 and rate > c_p95 + 0.2:
        print("VERDICT: LINK - the headset stops answering when the submit blocks. Next: the Wi-Fi side "
              "(router channel/band, the headset's power save, another device's periodic scan).")
    elif rate <= c_p95:
        print("VERDICT: PC SIDE - the link answers normally through the stalls. Next: the streamer's "
              "performance overlay at those ticks, a run at lower bitrate, a run at 90 or 120 Hz.")
    else:
        print("VERDICT: MIXED - above chance but not most stalls. Look at the listed stalls one by one.")


if __name__ == "__main__":
    main()
