# grab_verify.py - check the grab animation (hands/grab_pose.h, mesh_split.cpp THE GRAB) on the
# real first-person hand.
#
#   .\tools\grab-pose-host.ps1        # builds build\grab-pose-tests\grab-pose-cli.exe
#   .\tools\blender-run.ps1 tools\blender\grab_verify.py -- --psk <Skm_Player.psk> --cli <grab-pose-cli.exe> --out grab-verify.json
#
# What it answers.
#  1. THE PARENTS. The game does not read the skeleton: it finds each finger bone's parent from
#     the mesh (which bones share a vertex, weights >= 0.05 as MsBones does) and from the curled
#     pose (grab_pose.h parent_score, through the CLI). Here the same inference runs on the
#     extracted mesh, and its answer is compared with the skeleton's real parents.
#  2. THE MOTION. The honest reference for "a natural grab" is every finger JOINT turning by its
#     share of the curl through the real hierarchy (forward kinematics), as an animator's rig
#     does. The game's loop (mirrored here line for line: bones in order of distance from the
#     wrist, each = its inferred parent's output * the screw blend of its pose against that
#     parent) is run for the same poses and per-bone timing; the real vertices are skinned with
#     both (linear blend skinning, as the shader does) and compared frame by frame: the largest
#     vertex distance and the largest gap opened at a finger joint.
#  3. THE FAILED FIRST DESIGN, kept as the control: each bone blended against the WRIST (screw,
#     and plain element-wise). It must read worse, or the instrument cannot tell them apart.
#  4. SMOOTHNESS: the fingertip speed over the timeline, and the largest step in it.
# Plus a contact sheet: the game's method (top) and the reference (bottom).
# The blend, the timing and the parent score are the PRODUCTION header through grab-pose-cli.exe.
#
# The fist is SYNTHETIC: the game's fist is a live palette and its animations cannot be extracted
# (MODEL_WORKFLOW.md). It is a large multi-joint curl about each finger's knuckle axis, the hard
# case. Output lands in the model workspace (game-derived: never committed).
import bpy, sys, os, json, math, argparse, subprocess
import numpy as np
from mathutils import Matrix, Vector, Quaternion

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ap = argparse.ArgumentParser()
ap.add_argument("--psk", required=True)
ap.add_argument("--cli", required=True)
ap.add_argument("--out", default="grab-verify.json")
ap.add_argument("--side", default="R")
ap.add_argument("--fps", type=float, default=90.0)
ap.add_argument("--timing", default="70 220 180 260 40")
ap.add_argument("--distw", type=float, default=0.02)
ap.add_argument("--norender", action="store_true")
args = ap.parse_args(argv)
ws = os.environ.get("DVR_MODEL_WS", os.getcwd())
out = args.out if os.path.isabs(args.out) else os.path.join(ws, "verification", args.out)
os.makedirs(os.path.dirname(out), exist_ok=True)
R = {"psk": os.path.abspath(args.psk), "blender": bpy.app.version_string, "problems": []}

# ---- the production maths, through the CLI ----
cli = subprocess.Popen([args.cli], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
def ask(line):
    cli.stdin.write(line + "\n"); cli.stdin.flush()
    return cli.stdout.readline().strip()
T0 = [float(x) for x in args.timing.split()]
T = list(T0)
def set_timing(shape):
    global T, rel_at
    T = [shape] + T0[1:]
    assert ask("T " + " ".join("%g" % x for x in T)) == "ok"
    rel_at = T[0] + T[1] + T[4] + T[2]
rel_at = 0.0
set_timing(T0[0])
def phase(e, rel, along):
    p, s = ask("P %.4f %.4f %.4f" % (e, rel, along)).split()
    return int(p), float(s)
def m34(M): return [M[r][c] for r in range(3) for c in range(4)]
def fmt(M): return " ".join("%.7g" % x for x in m34(M))
def from34(v):
    return Matrix(((v[0], v[1], v[2], v[3]), (v[4], v[5], v[6], v[7]), (v[8], v[9], v[10], v[11]), (0, 0, 0, 1)))
def blend(kind, s, A, B):
    return from34([float(x) for x in ask("%s %.6f %s %s" % (kind, s, fmt(A), fmt(B))).split()])
def pscore(Q, cp, cc):
    sc, res = ask("J %.4f %s %s %s" % (args.distw, fmt(Q), " ".join("%.7g" % x for x in cp), " ".join("%.7g" % x for x in cc))).split()
    return float(sc), float(res)

# ---- the mesh and skeleton ----
for o in list(bpy.data.objects): bpy.data.objects.remove(o, do_unlink=True)
bpy.ops.psk.import_file(filepath=os.path.abspath(args.psk), scale=1.0)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
mesh = next(o for o in bpy.data.objects if o.type == "MESH")
bones = arm.data.bones
S = args.side
wrist = "hand_%s_jnt" % S
fingers = ["thumb", "index", "middle", "ring", "pinky"]
chain = {f: ["%s_%d_%s_jnt" % (f, i, S) for i in range(3)] for f in fingers}
tipname = {f: "%s_end_%s_jnt" % (f, S) for f in fingers}
for f in fingers:
    for b in chain[f] + [tipname[f]]:
        if b not in bones: R["problems"].append("missing bone " + b)
if R["problems"]:
    json.dump(R, open(out, "w"), indent=1); print("DVR_GRAB problems", R["problems"]); sys.exit(1)
head = {b.name: Vector(b.head_local) for b in bones}
parent_of = {b.name: (b.parent.name if b.parent else None) for b in bones}

me = mesh.data
V = np.array([tuple(v.co) for v in me.vertices], dtype=np.float64)
gname = {g.index: g.name for g in mesh.vertex_groups}
Wall = {}
for v in me.vertices:
    for g in v.groups:
        n = gname.get(g.group)
        if n is None or g.weight <= 0: continue
        Wall.setdefault(n, np.zeros(len(V)))[v.index] = g.weight
# adjacency as MsBones builds it: some vertex weighted >= 0.05 to both
adj = set()
names = list(Wall.keys())
for i in range(len(names)):
    for j in range(i + 1, len(names)):
        if np.any((Wall[names[i]] >= 0.05) & (Wall[names[j]] >= 0.05)):
            adj.add((names[i], names[j])); adj.add((names[j], names[i]))
cen = {n: (V * Wall[n][:, None]).sum(0) / Wall[n].sum() for n in names}

# the hand's frame, as the mesh split measures it: limb axis from the forearm toward the hand
along_ax = (head[chain["middle"][0]] - head[wrist]).normalized()
across = (head[chain["pinky"][0]] - head[chain["index"][0]]).normalized()
normal = along_ax.cross(across).normalized()
wc = cen[wrist]
def along(n): return float(np.dot(cen[n] - wc, np.array(along_ax)))
# the game's finger set: this side's weighted bones at or past the wrist (OhBuildPairs' test)
side_bones = [n for n in names if n.endswith("_%s_jnt" % S) or n.endswith("_%s_Jnt" % S)]
fset = [n for n in side_bones if n != wrist and along(n) >= -2.0]
R["finger_set"] = sorted(fset, key=along)
R["along_all"] = {n: round(along(n), 2) for n in side_bones}
print("DVR_GRAB along", R["along_all"])

# palm side from the rest pose's own slight curl
bend = 0.0
for f in ["index", "middle", "ring", "pinky"]:
    p0, p1, tip = head[chain[f][0]], head[chain[f][1]], head[tipname[f]]
    v1 = (p1 - p0).normalized(); v2 = tip - p1
    bend += (v2 - v1 * v2.dot(v1)).dot(normal)
palm_sign = 1.0 if bend > 0 else -1.0
R["palm_side"] = {"rest_curl_along_normal": bend, "sign": palm_sign}

curl = {"thumb": (20, 35, 45), "index": (80, 95, 65), "middle": (85, 100, 65), "ring": (88, 100, 65), "pinky": (90, 100, 65)}
def axis_of(f):
    p0, tip = head[chain[f][0]], head[tipname[f]]
    d = (tip - p0).normalized()
    if f == "thumb": return d.cross(normal * palm_sign).normalized()
    a = (across - d * across.dot(d)).normalized()
    test = (Quaternion(a, math.radians(30)).to_matrix() @ (tip - p0)) - (tip - p0)
    return a if test.dot(normal * palm_sign) > 0 else -a
axes = {f: axis_of(f) for f in fingers}
joint_curl = {}
for f in fingers:
    for i, b in enumerate(chain[f]): joint_curl[b] = (axes[f], curl[f][i])
def rot_about(p, a, deg):
    return Matrix.Translation(p) @ Quaternion(a, math.radians(deg)).to_matrix().to_4x4() @ Matrix.Translation(-p)
# forward kinematics over the REAL hierarchy, for every bone of the finger set (end bones and the
# attachment ride their parents); the wrist stays at identity
def fk(frac):
    Sm = {wrist: Matrix.Identity(4)}
    def get(n):
        if n in Sm: return Sm[n]
        p = parent_of[n]
        P = get(p) if p in fset or p == wrist else Matrix.Identity(4)
        if n in joint_curl:
            a, deg = joint_curl[n]; Sm[n] = P @ rot_about(head[n], a, deg * frac.get(n, 0.0))
        else: Sm[n] = P.copy()
        return Sm[n]
    for n in fset: get(n)
    return Sm
flat = fk({})
fist = fk({b: 1.0 for b in joint_curl})

# ---- 1. the parents, inferred as the game will ----
inferred, report = {}, {}
def depth(n): return float(np.linalg.norm(cen[n] - wc))   # chain depth: straight-line distance from the wrist
for c in sorted(fset, key=depth):
    cands = [p for p in fset + [wrist] if p != c and depth(p) < depth(c)]
    joined = [p for p in cands if (p, c) in adj]
    pool = joined if joined else cands
    best, bs, rows = None, 1e30, []
    for p in pool:
        Q = fist[p].inverted() @ fist[c]
        sc, res = pscore(Q, cen[p], cen[c])
        rows.append((p, round(sc, 3), round(res, 3)))
        if sc < bs: bs, best = sc, p
    inferred[c] = best
    truth = parent_of[c] if parent_of[c] in fset or parent_of[c] == wrist else wrist
    report[c] = {"inferred": best, "true": truth, "ok": best == truth, "pool": "joined" if joined else "ALL (no joined bone)",
                 "scores": sorted(rows, key=lambda r: r[1])[:4]}
R["parents"] = report
R["parents_wrong"] = [c for c in report if not report[c]["ok"]]

# ---- 2. the motion ----
alongn = {}
amax = max(1e-3, max(along(n) for n in fset))
for n in fset: alongn[n] = along(n) / amax
order = sorted(fset, key=depth)                      # parents before children
dt = 1000.0 / args.fps
def timeline():
    fr, t = [], 0.0
    while t <= rel_at + T[3] + 2 * dt: fr.append(t); t += dt
    return fr
# The game's shape rule: GrabAnimShapeMs scaled by the largest joint angle between the base pose
# and flat, against 60 degrees (mesh_split.cpp GrApply).
def worst_angle(base):
    worst = 0.0
    for b in order:
        p = inferred[b]
        qa = (base[p].inverted() @ base[b]).to_quaternion(); qf = (flat[p].inverted() @ flat[b]).to_quaternion()
        worst = max(worst, math.degrees(qa.rotation_difference(qf).angle))
    return worst
def shape_for(base):
    d = worst_angle(base)
    return T0[0] * (1.0 if d >= 60 else d / 60.0), d

def game_loop(e, base, kind="B"):
    """mesh_split.cpp's loop: in order from the wrist, out[b] = out[parent] * blend(Q_from, Q_to)."""
    outm, fracs = {wrist: Matrix.Identity(4)}, {}
    for b in order:
        p = inferred[b]
        ph, s = phase(e, rel_at, alongn[b])
        Qb = lambda X: X[p].inverted() @ X[b]
        if ph == 1:   A, B = Qb(base), Qb(flat)
        elif ph == 2: A, B = Qb(flat), Qb(fist)
        elif ph == 3: A, B, s = Qb(fist), Qb(fist), 1.0
        elif ph == 4: A, B = Qb(fist), Qb(base)
        else:         A, B, s = Qb(base), Qb(base), 0.0
        outm[b] = outm[p] @ blend(kind, s, A, B)
        fracs[b] = 0.0 if ph in (0, 1, 5) else (s if ph == 2 else 1.0 if ph == 3 else 1.0 - s)
    return outm, fracs
def wrist_relative(e, base, kind):
    """the first design: every bone blended against the wrist"""
    outm = {wrist: Matrix.Identity(4)}
    for b in order:
        ph, s = phase(e, rel_at, alongn[b])
        if ph == 1:   A, B = base[b], flat[b]
        elif ph == 2: A, B = flat[b], fist[b]
        elif ph == 3: A, B, s = fist[b], fist[b], 1.0
        elif ph == 4: A, B = fist[b], base[b]
        else:         A, B, s = base[b], base[b], 0.0
        outm[b] = blend(kind, s, A, B)
    return outm

moved = [n for n in fset if n in Wall]
mw = sum(Wall[n] for n in moved)
fv = mw > 0.05
R["vertices"] = len(V); R["finger_vertices"] = int(fv.sum())
def skin(Sm):
    acc = np.zeros_like(V); wsum = np.zeros(len(V))
    for b in moved:
        M = np.array(Sm[b]); w = Wall[b]
        acc += w[:, None] * (V @ M[:3, :3].T + M[:3, 3]); wsum += w
    return acc + (1.0 - wsum)[:, None] * V
def joint_gap(Sm):
    g = 0.0
    for f in fingers:
        for i in range(1, 3):
            pb, cb = chain[f][i - 1], chain[f][i]
            if pb not in Sm or cb not in Sm: continue
            g = max(g, ((Sm[cb] @ head[cb]) - (Sm[pb] @ head[cb])).length)
    return g
tipbone = "index_2_%s_jnt" % S
def run(method):
    dev, gap, tip = [], [], []
    for e in timeline():
        ours, fr = game_loop(e, flat, "B")
        ref = fk(fr)
        if method != "game": ours = wrist_relative(e, flat, method)
        d = np.linalg.norm(skin(ours)[fv] - skin(ref)[fv], axis=1)
        dev.append(float(d.max())); gap.append(joint_gap(ours)); tip.append(list(ours[tipbone] @ head[tipname["index"]]))
    v = np.linalg.norm(np.diff(np.array(tip), axis=0), axis=1) / (dt / 1000.0)
    return {"max_vertex_dev": max(dev), "max_joint_gap": max(gap), "dev_by_frame": [round(x, 3) for x in dev],
            "peak_tip_speed": float(v.max()), "speed_step_over_peak": float(np.abs(np.diff(v)).max() / max(v.max(), 1e-6)),
            "speed_first_last": [float(v[0]), float(v[-1])], "speed_by_frame": [round(float(x), 1) for x in v]}
sh, deg = shape_for(flat); set_timing(sh)
R["open_start_shape"] = {"worst_joint_deg_from_flat": deg, "shape_ms": sh}
R["game_method"] = run("game")
R["control_wrist_screw"] = run("B")
R["control_wrist_plain"] = run("L")
idx_len = (head[tipname["index"]] - head[chain["index"][0]]).length
R["scale"] = {"index_finger_length": idx_len,
              "index_tip_travel": (fist[tipbone] @ head[tipname["index"]] - head[tipname["index"]]).length,
              "units": "game units (UE3: about a centimetre each)"}
# from a fist (the right hand on the sword): opens, closes, holds; judged by gaps and smoothness
sh2, deg2 = shape_for(fist); set_timing(sh2)
R["fist_start_shape"] = {"worst_joint_deg_from_flat": deg2, "shape_ms": sh2}
gap2, tip2 = [], []
for e in timeline():
    ours, _ = game_loop(e, fist, "B")
    gap2.append(joint_gap(ours)); tip2.append(list(ours[tipbone] @ head[tipname["index"]]))
v2 = np.linalg.norm(np.diff(np.array(tip2), axis=0), axis=1) / (dt / 1000.0)
R["from_fist"] = {"max_joint_gap": max(gap2), "peak_tip_speed": float(v2.max()),
                  "speed_step_over_peak": float(np.abs(np.diff(v2)).max() / max(v2.max(), 1e-6)),
                  "speed_by_frame": [round(float(x), 1) for x in v2]}
R["frame_ms"] = dt

# ---- the contact sheet ----
def moments_now():
    return [0.5 * T[0], T[0] + 0.25 * T[1], T[0] + 0.5 * T[1], T[0] + 0.75 * T[1], T[0] + T[1] + T[4], rel_at + 0.5 * T[3], rel_at + T[3]]
if not args.norender:
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"; scene.display.shading.color_type = "SINGLE"
    scene.display.shading.single_color = (0.80, 0.62, 0.52)
    scene.render.resolution_x = 360; scene.render.resolution_y = 360
    mesh.hide_render = True; arm.hide_render = True
    hand_idx = np.where(np.linalg.norm(V - np.array(head[wrist]), axis=1) < idx_len * 1.6 + 6)[0]
    centre = Vector(V[hand_idx].mean(0))
    cam_data = bpy.data.cameras.new("cam"); cam = bpy.data.objects.new("cam", cam_data); scene.collection.objects.link(cam)
    scene.camera = cam
    cam.location = centre + (normal * palm_sign) * 30 + across * 12 - along_ax * 4
    cam.rotation_euler = (centre - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam_data.lens = 50
    sheet_dir = os.path.join(os.path.dirname(out), "grab-verify")
    os.makedirs(sheet_dir, exist_ok=True)
    tiles = []
    rows = (("game", flat, sh), ("ref", flat, sh), ("fist-start", fist, sh2))
    R["sheet_rows"] = ["the game's method from an open hand", "the joint-by-joint reference", "the game's method from a fist"]
    R["moments_ms"] = {}
    for row, (which, base, shape) in enumerate(rows):
        set_timing(shape); moments = moments_now(); R["moments_ms"][which] = moments
        for k, e in enumerate(moments):
            ours, fr = game_loop(e, base, "B")
            P = skin(fk(fr) if which == "ref" else ours)
            md = me.copy(); md.vertices.foreach_set("co", P.astype(np.float32).ravel())
            ob = bpy.data.objects.new("pose", md); scene.collection.objects.link(ob)
            path = os.path.join(sheet_dir, "%s_%02d.png" % (which, k))
            scene.render.filepath = path
            bpy.ops.render.render(write_still=True)
            bpy.data.objects.remove(ob, do_unlink=True); bpy.data.meshes.remove(md)
            tiles.append((row, k, path))
    Wt = Ht = 360; nr = len(rows); nc = 7
    sheet = np.zeros((nr * Ht, nc * Wt, 4), dtype=np.float32)
    for row, k, path in tiles:
        img = bpy.data.images.load(path)
        px = np.array(img.pixels[:], dtype=np.float32).reshape(Ht, Wt, 4)
        sheet[(nr - 1 - row) * Ht:(nr - row) * Ht, k * Wt:(k + 1) * Wt] = px
        bpy.data.images.remove(img)
    img = bpy.data.images.new("sheet", nc * Wt, nr * Ht, alpha=True)
    img.pixels = sheet.ravel()
    R["sheet"] = os.path.join(os.path.dirname(out), "grab-verify-sheet.png")
    img.filepath_raw = R["sheet"]; img.file_format = "PNG"; img.save()

cli.stdin.close(); cli.wait()
json.dump(R, open(out, "w"), indent=1)
g, cs, cp = R["game_method"], R["control_wrist_screw"], R["control_wrist_plain"]
print("DVR_GRAB parents wrong %d of %d %s | game: dev %.3f gap %.3f step/peak %.3f | wrist screw: dev %.3f gap %.3f | "
      "wrist plain: dev %.3f gap %.3f | from fist: gap %.3f step/peak %.3f peak %.0f (shape %.0f ms) vs close peak %.0f | index length %.2f travel %.2f | palm %+d -> %s" % (
      len(R["parents_wrong"]), len(fset), R["parents_wrong"], g["max_vertex_dev"], g["max_joint_gap"], g["speed_step_over_peak"],
      cs["max_vertex_dev"], cs["max_joint_gap"], cp["max_vertex_dev"], cp["max_joint_gap"], R["from_fist"]["max_joint_gap"],
      R["from_fist"]["speed_step_over_peak"], R["from_fist"]["peak_tip_speed"], sh2, g["peak_tip_speed"], idx_len,
      R["scale"]["index_tip_travel"], palm_sign, out))
