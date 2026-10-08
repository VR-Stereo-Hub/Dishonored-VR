# cut_preview.py - apply the body_cut.cpp classifier to a UModel PSK in Blender and SHOW the result:
# the arm vertices within --radius of the arm chain (upper_arm -> lower_arm -> hand -> fingers), more
# than --start from the shoulder joint, are removed triangle by triangle (a triangle goes when at
# least --min of its three vertices are arm), and the mesh is rendered front and side, before and
# after, to PNGs. The chain comes from the imported armature's bone heads (the add-on composes the
# reference pose), so the runtime's own quaternion composition is checked against it by the counts.
# Run through tools\blender-run.ps1:
#
#   .\tools\blender-run.ps1 tools\blender\cut_preview.py -- --psk <mesh.psk> --out <folder> [--radius 12] [--start 11] [--min 2]
#
# Output is about a game asset: it stays in the model workspace, never the repo.
import bpy, bmesh, sys, os, json, math, argparse
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ap = argparse.ArgumentParser()
ap.add_argument("--psk", required=True); ap.add_argument("--out", required=True)
ap.add_argument("--radius", type=float, default=12.0); ap.add_argument("--start", type=float, default=11.0)
ap.add_argument("--min", type=int, default=2)
args = ap.parse_args(argv)
ws = os.environ.get("DVR_MODEL_WS", os.getcwd())
out = args.out if os.path.isabs(args.out) else os.path.join(ws, "verification", args.out)
os.makedirs(out, exist_ok=True)
R = {"psk": args.psk, "radius": args.radius, "start": args.start, "min": args.min}

bpy.ops.wm.read_homefile(use_empty=True)
for o in list(bpy.data.objects): bpy.data.objects.remove(o, do_unlink=True)
res = bpy.ops.psk.import_file(filepath=os.path.abspath(args.psk), scale=1.0)
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
arms = [o for o in bpy.data.objects if o.type == "ARMATURE"]
if not meshes or not arms:
    R["error"] = "import gave %d meshes and %d armatures" % (len(meshes), len(arms))
    print(json.dumps(R, indent=1)); sys.exit(1)
mesh, arm = meshes[0], arms[0]
R["vertices"] = len(mesh.data.vertices); R["triangles"] = len(mesh.data.polygons)

def head(name):
    b = arm.data.bones.get(name)
    return (arm.matrix_world @ b.head_local) if b else None

chains = []
for side in ("L", "R"):
    j = [head("upper_arm_%s_jnt" % side), head("lower_arm_%s_jnt" % side), head("hand_%s_jnt" % side)]
    if any(v is None for v in j):
        R["error"] = "missing arm bones for side %s" % side; print(json.dumps(R, indent=1)); sys.exit(1)
    d = (j[2] - j[1]); d = d / d.length if d.length > 1e-6 else Vector((0, 0, 1))
    j.append(j[2] + d * 20.0)
    chains.append(j)
R["chains"] = [[list(map(lambda x: round(x, 1), v)) for v in c] for c in chains]

def dist_seg(p, a, b):
    ab = b - a; ap = p - a; l2 = ab.length_squared
    t = 0.0 if l2 < 1e-9 else max(0.0, min(1.0, ap.dot(ab) / l2))
    return (a + ab * t - p).length, t

def is_arm(p):
    for c in chains:
        for s in range(3):
            d, t = dist_seg(p, c[s], c[s + 1])
            if d > args.radius: continue
            if s == 0 and t * (c[1] - c[0]).length < args.start: continue
            return True
    return False

mw = mesh.matrix_world
armv = [is_arm(mw @ v.co) for v in mesh.data.vertices]
R["arm_vertices"] = sum(armv)
# which vertex groups (bones) the arm vertices and the KEPT near-shoulder vertices belong to
vg = {g.index: g.name for g in mesh.vertex_groups}
def dominant(v):
    best = None
    for g in v.groups:
        if best is None or g.weight > best[0]: best = (g.weight, vg.get(g.group, "?"))
    return best[1] if best else "?"
by_bone = {}
for v, a in zip(mesh.data.vertices, armv):
    if a: by_bone[dominant(v)] = by_bone.get(dominant(v), 0) + 1
R["arm_vertices_by_dominant_bone"] = dict(sorted(by_bone.items(), key=lambda kv: -kv[1]))
leak = {}
for v, a in zip(mesh.data.vertices, armv):
    nm = dominant(v)
    if a and not any(k in nm for k in ("arm", "hand", "index", "middle", "ring", "pinky", "thumb")):
        leak[nm] = leak.get(nm, 0) + 1
R["arm_vertices_whose_dominant_bone_is_not_an_arm_bone"] = leak
stub = {}
for v, a in zip(mesh.data.vertices, armv):
    nm = dominant(v)
    if not a and ("upper_arm" in nm or "lower_arm" in nm or "hand_" in nm):
        stub[nm] = stub.get(nm, 0) + 1
R["kept_vertices_dominated_by_arm_bones_(the_stub)"] = stub

# render helper
def render(tag, obj):
    scn = bpy.context.scene
    scn.render.engine = "BLENDER_WORKBENCH"
    scn.display.shading.light = "STUDIO"; scn.display.shading.color_type = "SINGLE"
    scn.render.resolution_x, scn.render.resolution_y = 900, 1100
    scn.render.film_transparent = False
    bb = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    lo = Vector((min(v.x for v in bb), min(v.y for v in bb), min(v.z for v in bb)))
    hi = Vector((max(v.x for v in bb), max(v.y for v in bb), max(v.z for v in bb)))
    ctr = (lo + hi) / 2; size = max((hi - lo).x, (hi - lo).y, (hi - lo).z)
    # The PSK import is Y-up (the add-on keeps UModel's frame): a camera looks down its own -Z,
    # so "front" sits on +Z looking back, "side" on +X turned 90 deg about Y, "back" on -Z.
    for view, offset, rot in (("front", Vector((0, 0, size * 2.2)), (0, 0, 0)),
                              ("side", Vector((size * 2.2, 0, 0)), (0, math.radians(90), 0)),
                              ("back", Vector((0, 0, -size * 2.2)), (0, math.radians(180), 0))):
        cam_data = bpy.data.cameras.new("Cam"); cam_data.type = "ORTHO"; cam_data.ortho_scale = size * 1.15
        cam = bpy.data.objects.new("Cam", cam_data); bpy.context.collection.objects.link(cam)
        cam.location = ctr + offset; cam.rotation_euler = rot; scn.camera = cam
        scn.render.filepath = os.path.join(out, "%s_%s.png" % (tag, view))
        bpy.ops.render.render(write_still=True)
        bpy.data.objects.remove(cam, do_unlink=True)
    R.setdefault("renders", []).append(os.path.join(out, tag + "_*.png"))

render("before", mesh)

# the cut: drop triangles with >= min arm vertices
bm = bmesh.new(); bm.from_mesh(mesh.data); bm.verts.ensure_lookup_table()
drop = [f for f in bm.faces if sum(1 for v in f.verts if armv[v.index]) >= args.min]
R["triangles_dropped"] = len(drop); R["triangles_kept"] = len(bm.faces) - len(drop)
bmesh.ops.delete(bm, geom=drop, context="FACES")
bm.to_mesh(mesh.data); bm.free(); mesh.data.update()
render("after", mesh)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, "cut_preview.blend"))
with open(os.path.join(out, "cut_preview.json"), "w") as fh: json.dump(R, fh, indent=1)
print(json.dumps(R, indent=1))
