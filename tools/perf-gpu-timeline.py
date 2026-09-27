#!/usr/bin/env python3
"""The uncap deep dive's trace reader: a DvrGpu capture (tools/wpr/dvr-gpu.wprp) as a GPU timeline.

Input is the text `xperf -i <etl> -o <txt> -a dumper` writes (pass the .etl and it runs xperf for you).
Under hardware-accelerated GPU scheduling the per-packet GPU times are not in the trace as such; this
rebuilds them per hardware queue from DxgKrnl's DmaPacket events: a submit (hHwQueue, fence, pDmaBuffer)
and a completion (hHwQueue, fence). A queue executes its packets in order, so a packet runs from
max(its submit, the previous completion on that queue) to its completion. That interval includes any
time the queue waited on a GPU sync object or was time-sliced out - it is QUEUE OCCUPANCY, an upper bound
on execution, never less than it. Queues map to process and engine node through the rundown
(HwQueue -> Context -> Device -> process id).

It prints: occupancy per process and node over the window; the union of every process on each node (the
closest this trace gets to 'GPU busy'); the game's per-pair GPU occupancy; and every GAP on the game's
busiest node queue - how long, how much of it another process occupied, and which of the proxy's phases
(DishonoredVR markers) the render thread was in when it began.

    python tools/perf-gpu-timeline.py seg07-baseline-D.etl [--game Dishonored.exe]
"""
import argparse
import bisect
import collections
import os
import re
import subprocess
import sys

XPERF = r"C:\Program Files (x86)\Windows Kits\10\Windows Performance Toolkit\xperf.exe"
PHASES = {1: 'present', 2: 'xrWaitFrame(begin)', 3: 'game tick', 4: 'method', 5: 'capture fence',
          6: 'capture read fence', 7: 'hud', 8: 'xrEndFrame', 9: 'desktop present/flush',
          10: 'scene draw (game thread)', 11: 'hud fence'}


def split_fields(line):
    """Split a dumper line on ', ' outside [] and quotes."""
    out, cur, depth, quote = [], [], 0, False
    for ch in line:
        if ch == '"':
            quote = not quote
        elif not quote and ch == '[':
            depth += 1
        elif not quote and ch == ']':
            depth -= 1
        if ch == ',' and depth == 0 and not quote:
            out.append(''.join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    out.append(''.join(cur).strip())
    return out


def pid_of(procfield):
    m = re.search(r'\(\s*(-?\d+)\)', procfield)
    return int(m.group(1)) if m else -1


def union(intervals):
    out = []
    for a, b in sorted(intervals):
        if b <= a:
            continue
        if out and a <= out[-1][1]:
            if b > out[-1][1]:
                out[-1][1] = b
        else:
            out.append([a, b])
    return out


def total(iv):
    return sum(b - a for a, b in iv)


def clip(iv, lo, hi):
    return [[max(a, lo), min(b, hi)] for a, b in iv if b > lo and a < hi]


def overlap(iv, a, b):
    """Length of [a,b] covered by the sorted disjoint list iv."""
    s = 0.0
    i = bisect.bisect_left([x[1] for x in iv], a)
    while i < len(iv) and iv[i][0] < b:
        s += max(0.0, min(b, iv[i][1]) - max(a, iv[i][0]))
        i += 1
    return s


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('trace')
    ap.add_argument('--game', default='Dishonored.exe')
    ap.add_argument('--gaps', type=int, default=12, help='how many of the largest gaps to list')
    args = ap.parse_args()
    path = args.trace
    if path.lower().endswith('.etl'):
        txt = path[:-4] + '.txt'
        if not os.path.exists(txt):
            subprocess.run([XPERF, '-i', path, '-o', txt, '-a', 'dumper'], check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        path = txt

    procname = {}
    dma_sub = collections.defaultdict(dict)    # hq -> fence -> (t, pid)
    dma_done = collections.defaultdict(dict)   # hq -> fence -> t
    qp_pid = collections.Counter()             # (hq, pid) -> submissions
    hq_parent_ctx = {}                         # ParentDxgHwQueue -> hContext
    ctx_node, ctx_dev, dev_pid = {}, {}, {}
    ctx_parent = {}                            # ParentDxgContext -> hContext
    phases = []                                # (t, tid, id, a)
    frame_starts = []
    marks = []
    t_min, t_max = None, None
    game_pids = set()

    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            s = line.lstrip()
            if not s or s[0] not in 'MDC':
                continue
            if s.startswith('Microsoft-Windows-DxgKrnl/'):
                name = s[26:s.index(',')]
                if not (name.startswith('DmaPacket/win:Info') or name.startswith('QueuePacket/win:Start')
                        or name.startswith('HwQueue/') or name.startswith('Context/') or name.startswith('Device/')):
                    continue
                fl = split_fields(s)
                if len(fl) < 10 or fl[1] == 'TimeStamp':
                    continue
                t = float(fl[1])
                pid = pid_of(fl[2])
                procname.setdefault(pid, fl[2].split('(')[0].strip())
                t_min = t if t_min is None else min(t_min, t)
                t_max = t if t_max is None else max(t_max, t)
                p = fl[9:]
                if name.startswith('DmaPacket/win:Info'):
                    if len(p) == 5:        # submit: hHwQueue, fence, pDmaBuffer, ntStatus, pendingFlip
                        dma_sub[p[0]][int(p[1])] = (t, pid)
                    elif len(p) == 2:      # completion: hHwQueue, fence
                        dma_done[p[0]][int(p[1])] = t
                elif name.startswith('QueuePacket/win:Start'):
                    qp_pid[(p[0], pid)] += 1
                elif name.startswith('HwQueue/') and len(p) >= 3:
                    hq_parent_ctx[p[2]] = p[0]
                elif name.startswith('Context/') and len(p) >= 12:
                    ctx_dev[p[9]] = p[0]
                    ctx_node[p[9]] = int(p[1])
                    ctx_parent[p[11]] = p[9]
                elif name.startswith('Device/') and len(p) >= 4:
                    dev_pid[p[3]] = int(p[0], 16) if p[0].startswith('0x') else int(p[0])
            elif s.startswith('DishonoredVR/'):
                fl = split_fields(s)
                if len(fl) < 10 or fl[1] == 'TimeStamp':
                    continue
                t = float(fl[1])
                tid = int(fl[3])
                game_pids.add(pid_of(fl[2]))
                if s.startswith('DishonoredVR/Phase/'):
                    phases.append((t, tid, int(fl[9]), int(fl[10])))
                elif s.startswith('DishonoredVR/FrameStart/'):
                    frame_starts.append(t)
                elif s.startswith('DishonoredVR/Mark/'):
                    marks.append((t, fl[9]))

    lo, hi = t_min, t_max
    span = hi - lo
    print(f'trace {os.path.basename(args.trace)}: window {span / 1000:.1f} ms of DxgKrnl events')

    # Queue -> (pid, node). Submitter pid from QueuePacket starts, node through the rundown.
    hq_pid = {}
    for (hq, pid), n in qp_pid.most_common():
        hq_pid.setdefault(hq, pid)
    for hq, subs in dma_sub.items():
        if hq not in hq_pid and subs:
            hq_pid[hq] = collections.Counter(p for _, p in subs.values()).most_common(1)[0][0]

    def node_of(hq):
        ctx = hq_parent_ctx.get(hq) or ctx_parent.get(hq)
        return ctx_node.get(ctx, -1) if ctx else -1

    # Execution intervals per queue.
    iv = {}
    for hq, subs in dma_sub.items():
        done = dma_done.get(hq, {})
        prev = None
        out = []
        for fence in sorted(subs):
            if fence not in done:
                continue
            t0 = subs[fence][0]
            t1 = done[fence]
            start = t0 if prev is None else max(t0, prev)
            if t1 > start:
                out.append([start, t1])
            prev = t1
        if out:
            iv[hq] = union(clip(out, lo, hi))

    game_pid = next((p for p, n in procname.items() if n.lower() == args.game.lower()), None)
    if game_pids:
        game_pid = next(iter(game_pids))
    print(f'game pid {game_pid}; {len(iv)} hardware queues with GPU packets; nodes from the rundown: '
          f'{len(hq_parent_ctx)} queue parents, {len(ctx_node)} contexts, {len(dev_pid)} devices')
    print()
    print(f'{"process":28s} {"node":>4s} {"queue":>20s} {"packets":>7s} {"occupied":>9s} {"% window":>8s}')
    by_proc_node = collections.defaultdict(list)
    rows = []
    for hq, ivs in iv.items():
        pid = hq_pid.get(hq, -1)
        node = node_of(hq)
        by_proc_node[(pid, node)].extend(ivs)
        rows.append((total(ivs), pid, node, hq, len(dma_sub[hq])))
    for occ, pid, node, hq, n in sorted(rows, reverse=True)[:16]:
        print(f'{procname.get(pid, "?")[:28]:28s} {node:4d} {hq[-12:]:>20s} {n:7d} {occ / 1000:8.1f}ms {100 * occ / span:7.1f}%')
    print()
    nodes = sorted({k[1] for k in by_proc_node})
    print('per node: union of every process (upper bound on busy) and the game\'s share')
    per_node_union = {}
    for node in nodes:
        allv = union([x for (p, n), v in by_proc_node.items() if n == node for x in v])
        per_node_union[node] = allv
        gv = union(by_proc_node.get((game_pid, node), []))
        others = collections.Counter()
        for (p, n), v in by_proc_node.items():
            if n == node and p != game_pid:
                others[procname.get(p, '?')] += total(union(v))
        oth = ', '.join(f'{k} {100 * v / span:.1f}%' for k, v in others.most_common(4))
        print(f'  node {node:2d}: any process {100 * total(allv) / span:5.1f}% | game {100 * total(gv) / span:5.1f}% | {oth}')

    # The game's main node: the one with most game occupancy.
    gnode = max(nodes, key=lambda n: total(union(by_proc_node.get((game_pid, n), []))))
    gv = union(by_proc_node.get((game_pid, gnode), []))
    other = union([x for (p, n), v in by_proc_node.items() if n == gnode and p != game_pid for x in v])
    presents = [t for t, tid, pid_, a in phases if pid_ == 1]
    render_tid = collections.Counter(tid for t, tid, i, a in phases if i == 1).most_common(1)
    render_tid = render_tid[0][0] if render_tid else None
    game_tid = collections.Counter(tid for t, tid, i, a in phases if i == 10).most_common(1)
    game_tid = game_tid[0][0] if game_tid else None
    npresent = len([1 for t, tid, i, a in phases if i == 1 and tid == render_tid]) // 2   # begin+end
    pairs = npresent / 2.0
    wms = span / 1000.0
    print()
    print(f'game node {gnode}: game occupied {100 * total(gv) / span:.1f}%, another process alone '
          f'{100 * (total(union(gv + other)) - total(gv)) / span:.1f}%, NOTHING {100 * (span - total(union(gv + other))) / span:.1f}%')
    if pairs:
        print(f'  {npresent} presents = {pairs:.0f} pairs in {wms:.0f} ms ({1000 * pairs / wms:.1f} pairs/s): '
              f'{wms / pairs:.2f} ms per pair, game GPU occupancy {total(gv) / 1000 / pairs:.2f} ms per pair, '
              f'other processes on the node {(total(union(gv + other)) - total(gv)) / 1000 / pairs:.2f} ms, idle '
              f'{(span - total(union(gv + other))) / 1000 / pairs:.2f} ms')

    # Render-thread phase timeline (toggle per (tid,id): begin, end, begin, ...).
    open_ = {}
    spans = collections.defaultdict(list)
    for t, tid, i, a in sorted(phases):
        k = (tid, i)
        if k in open_:
            spans[i].append((open_.pop(k), t, tid))
        else:
            open_[k] = t
    rt_spans = sorted((a, b, i) for i, v in spans.items() for a, b, tid in v if tid == render_tid and i != 1)
    pres_spans = sorted((a, b) for a, b, tid in spans.get(1, []) if tid == render_tid)
    starts = [a for a, b, i in rt_spans]

    def rt_phase(t):
        j = bisect.bisect_right(starts, t) - 1
        best = None
        while j >= 0 and j >= len(starts) - 10000:
            a, b, i = rt_spans[j]
            if a <= t <= b:
                best = PHASES.get(i, str(i)) if best is None else best
                break
            if t - a > 50000:
                break
            j -= 1
        if best:
            return best
        k = bisect.bisect_right([a for a, b in pres_spans], t) - 1
        if k >= 0 and pres_spans[k][0] <= t <= pres_spans[k][1]:
            return 'present (other)'
        return 'outside present (engine)'

    # Gaps on the game's queue.
    gaps = []
    prev = lo
    for a, b in gv:
        if a > prev:
            gaps.append((prev, a))
        prev = max(prev, b)
    if hi > prev:
        gaps.append((prev, hi))
    gaps = [(a, b) for a, b in gaps if b - a >= 20]   # >= 20 us
    gt = sum(b - a for a, b in gaps)
    print()
    print(f'GAPS on the game\'s node-{gnode} queue (>= 20 us): {len(gaps)}, {gt / 1000:.1f} ms = {100 * gt / span:.1f}% '
          f'of the window{f", {gt / 1000 / pairs:.2f} ms per pair" if pairs else ""}')
    by_phase = collections.Counter()
    by_phase_other = collections.Counter()
    hist = collections.Counter()
    for a, b in gaps:
        ph = rt_phase(a)
        by_phase[ph] += b - a
        by_phase_other[ph] += overlap(other, a, b)
        d = b - a
        hist['<0.1 ms' if d < 100 else '0.1-0.5 ms' if d < 500 else '0.5-1 ms' if d < 1000 else '1-2 ms' if d < 2000 else '>=2 ms'] += d
    print('  gap time by what the RENDER thread was in when the gap began (and how much of it another process filled):')
    for ph, v in by_phase.most_common():
        print(f'    {ph:28s} {v / 1000:7.1f} ms ({100 * v / max(gt, 1):5.1f}% of gap time; other process busy for '
              f'{100 * by_phase_other[ph] / max(v, 1):5.1f}% of it)')
    print('  gap time by gap length: ' + ', '.join(f'{k} {v / 1000:.1f} ms' for k, v in
                                                  sorted(hist.items(), key=lambda x: x[0])))
    print(f'  the {args.gaps} largest:')
    for a, b in sorted(gaps, key=lambda g: g[0] - g[1])[:args.gaps]:
        print(f'    at +{(a - lo) / 1000:8.2f} ms: {(b - a) / 1000:6.2f} ms, render thread in "{rt_phase(a)}", other '
              f'process busy {100 * overlap(other, a, b) / (b - a):5.1f}%')

    # Our phases on the render thread, per pair.
    print()
    if pairs:
        print('render thread phases (tid %s), ms per pair:' % render_tid)
        for i in sorted(spans):
            v = [b - a for a, b, tid in spans[i] if tid == (game_tid if i == 10 else render_tid)]
            if v:
                print(f'    {PHASES.get(i, str(i)):28s} {sum(v) / 1000 / pairs:6.2f}  (n={len(v)}, mean {sum(v) / len(v) / 1000:.3f} ms)')
    if marks:
        print('marks: ' + '; '.join(f'+{(t - lo) / 1000:.0f} ms {m}' for t, m in marks[:10]))


if __name__ == '__main__':
    main()
