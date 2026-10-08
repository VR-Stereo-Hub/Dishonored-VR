# psk_arm_census.py - read a UModel PSK and census its skinning: bones (name, parent, ref pose
# composed to mesh space), per-bone vertex counts by dominant weight, and the distance of the
# vertices dominated by a named bone chain from the chain's ref-pose polyline. It exists to design
# a runtime classifier ("a vertex within R of the arm segments is arm geometry") against the real
# asset before any C++ is written. Output is about a game asset: keep it in the model workspace.
#
#   py -3 tools\psk_arm_census.py <mesh.psk> --chain upper_arm_L_jnt lower_arm_L_jnt hand_L_jnt [--bones]
import sys, struct, argparse, math

def read_psk(path):
    d = open(path, 'rb').read(); off = 0; chunks = {}
    while off + 32 <= len(d):
        cid = d[off:off+20].split(b'\0')[0].decode(); typ, size, count = struct.unpack_from('<iii', d, off + 20)
        off += 32; chunks[cid] = (size, count, d[off:off + size * count]); off += size * count
    return chunks

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('psk'); ap.add_argument('--chain', nargs='+', default=[])
    ap.add_argument('--bones', action='store_true'); a = ap.parse_args()
    c = read_psk(a.psk)
    sz, n, b = c['PNTS0000']; pts = [struct.unpack_from('<fff', b, i * 12) for i in range(n)]
    sz, n, b = c['VTXW0000']; wedges = [struct.unpack_from('<HHffBBH', b, i * sz)[0] for i in range(n)]   # point index per wedge
    sz, n, b = c['FACE0000']
    faces = []
    for i in range(n):
        if sz == 12: w0, w1, w2, mat, aux, sg = struct.unpack_from('<HHHBBI', b, i * sz)
        else: w0, w1, w2, mat, aux, sg = struct.unpack_from('<IIIBBI', b, i * sz)
        faces.append((w0, w1, w2, mat))
    sz, n, b = c['MATT0000']; mats = [b[i*sz:i*sz+64].split(b'\0')[0].decode(errors='replace') for i in range(n)]
    sz, n, b = c['REFSKELT']; bones = []
    for i in range(n):
        name = b[i*sz:i*sz+64].split(b'\0')[0].decode(errors='replace')
        flags, nchild, parent = struct.unpack_from('<iii', b, i*sz + 64)
        qx, qy, qz, qw, px, py, pz, length, xs, ys, zs = struct.unpack_from('<11f', b, i*sz + 76)
        bones.append(dict(name=name, parent=parent, q=(qx, qy, qz, qw), p=(px, py, pz)))
    sz, n, b = c['RAWWEIGHTS']; weights = [struct.unpack_from('<fii', b, i * 12) for i in range(n)]   # weight, point, bone
    print("points %d wedges %d faces %d materials %s bones %d weights %d" % (len(pts), len(wedges), len(faces), mats, len(bones), len(weights)))
    # compose the ref pose to mesh space (PSK convention: root quat used as is, children conjugated)
    def qmul(a, b):
        ax, ay, az, aw = a; bx, by, bz, bw = b
        return (aw*bx + ax*bw + ay*bz - az*by, aw*by - ax*bz + ay*bw + az*bx, aw*bz + ax*by - ay*bx + az*bw, aw*bw - ax*bx - ay*by - az*bz)
    def qrot(q, v):
        x, y, z, w = q; vx, vy, vz = v
        qv = (x, y, z); t = (2*(y*vz - z*vy), 2*(z*vx - x*vz), 2*(x*vy - y*vx))
        return (vx + w*t[0] + (y*t[2] - z*t[1]), vy + w*t[1] + (z*t[0] - x*t[2]), vz + w*t[2] + (x*t[1] - y*t[0]))
    world = []
    for i, bn in enumerate(bones):
        q = bn['q']
        if i == 0: wq, wp = q, bn['p']
        else:
            pq, pp = world[bn['parent']]
            qc = (-q[0], -q[1], -q[2], q[3])      # PSK stores child orientations conjugated
            r = qrot(pq, bn['p']); wp = (pp[0]+r[0], pp[1]+r[1], pp[2]+r[2]); wq = qmul(pq, qc)
        world.append((wq, wp))
    # dominant bone per point
    dom = {}
    for w, p, bi in weights:
        if w > dom.get(p, (0, -1))[0]: dom[p] = (w, bi)
    counts = {}
    for p, (w, bi) in dom.items(): counts[bi] = counts.get(bi, 0) + 1
    if a.bones:
        for i, bn in enumerate(bones):
            wp = world[i][1]; print("  bone %3d %-24s parent %3d  mesh-space (%7.1f %7.1f %7.1f)  dominant verts %d" % (i, bn['name'], bn['parent'], wp[0], wp[1], wp[2], counts.get(i, 0)))
    # vertices to the chain polyline
    if a.chain:
        idx = [next(i for i, bn in enumerate(bones) if bn['name'] == nm) for nm in a.chain]
        poly = [world[i][1] for i in idx]
        # extend past the last joint along the last segment (fingers)
        lx, ly, lz = poly[-1]; px, py, pz = poly[-2]; dx, dy, dz = lx-px, ly-py, lz-pz; L = math.sqrt(dx*dx+dy*dy+dz*dz) or 1
        poly.append((lx + dx/L*20, ly + dy/L*20, lz + dz/L*20))
        def dseg(p, a_, b_):
            ax, ay, az = a_; bx, by, bz = b_; px_, py_, pz_ = p
            abx, aby, abz = bx-ax, by-ay, bz-az; t = ((px_-ax)*abx + (py_-ay)*aby + (pz_-az)*abz) / max(1e-6, abx*abx+aby*aby+abz*abz)
            t = max(0, min(1, t)); cx, cy, cz = ax+abx*t, ay+aby*t, az+abz*t
            return math.sqrt((px_-cx)**2 + (py_-cy)**2 + (pz_-cz)**2), t
        def dpoly(p):
            best = (1e9, -1, 0)
            for s in range(len(poly)-1):
                dd, t = dseg(p, poly[s], poly[s+1])
                if dd < best[0]: best = (dd, s, t)
            return best
        # which bones count as "the chain": the named ones and every descendant of the first
        chain_set = set(idx)
        changed = True
        while changed:
            changed = False
            for i, bn in enumerate(bones):
                if i not in chain_set and bn['parent'] in chain_set: chain_set.add(i); changed = True
        print("chain bones:", [bones[i]['name'] for i in sorted(chain_set)])
        print("chain joints mesh-space:", ["(%.1f %.1f %.1f)" % w for w in poly[:-1]])
        arm_d, other_d = [], []
        for p, (w, bi) in dom.items():
            dd, s, t = dpoly(pts[p])
            (arm_d if bi in chain_set else other_d).append((dd, s, t))
        arm_d.sort(); other_d.sort()
        def pct(v, q): return v[min(len(v)-1, int(q*len(v)))][0] if v else -1
        print("chain-dominated verts %d: distance to polyline p50 %.1f p90 %.1f p99 %.1f max %.1f" % (len(arm_d), pct(arm_d,.5), pct(arm_d,.9), pct(arm_d,.99), arm_d[-1][0] if arm_d else -1))
        near = [x for x in other_d if x[0] < 20]
        print("non-chain verts within 20 of the polyline: %d (segment 0 = shoulder..elbow): by segment %s" % (len(near), {s: sum(1 for x in near if x[1]==s) for s in range(len(poly)-1)}))
        for R in (10, 12, 14, 16, 18):
            inc = sum(1 for x in arm_d if x[0] <= R); leak = sum(1 for x in other_d if x[0] <= R)
            print("  R=%2d: chain verts inside %d/%d, non-chain verts inside %d" % (R, inc, len(arm_d), leak))
        # where along segment 0 do the non-chain leaks sit (t 0 = shoulder joint)
        leaks0 = sorted(x[2] for x in other_d if x[0] <= 14 and x[1] == 0)
        if leaks0: print("  non-chain leaks within 14 on the upper arm: t from %.2f to %.2f (n=%d)" % (leaks0[0], leaks0[-1], len(leaks0)))

main()
