"""Run in Blender against a local imported reference .blend, never the game.

blender --background Reference.blend --python blender-arm-ik-sweep.py --
  --sweep sweep.json --reference reference.bin --out output_directory
The matrices come from arm-ik-sweep.cpp, which calls the production solver.
All outputs contain local game-derived geometry and must stay untracked.
"""
import argparse
import json
import math
import struct
import sys
from pathlib import Path
import bpy
import numpy as np
from mathutils import Matrix, Vector

parser = argparse.ArgumentParser()
parser.add_argument('--sweep', type=Path, required=True)
parser.add_argument('--reference', type=Path, required=True)
parser.add_argument('--out', type=Path, required=True)
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:])
args.sweep = args.sweep.resolve()
args.reference = args.reference.resolve()
args.out = args.out.resolve()
args.out.mkdir(parents=True, exist_ok=True)
sweep = json.loads(args.sweep.read_text())
source = bpy.data.objects.get('Original_Skm_Player_Mesh')
rig = bpy.data.objects.get('Original_Skm_Player_Rig')
if source is None or rig is None:
    raise RuntimeError('Expected the preserved original reference mesh and rig')

# Independently verify the prepared joint locations against the PSK import.
raw = args.reference.read_bytes()
magic, bone_count, _, _ = struct.unpack_from('<8sIII', raw)
assert magic == b'DVRIK001'
head_errors = []
for i in range(bone_count):
    name, _, x, y, z = struct.unpack_from('<64si3f', raw, 20+i*80)
    name = name.split(b'\0', 1)[0].decode()
    head_errors.append((rig.data.bones[name].head_local-Vector((x, y, z))).length)
assert max(head_errors) < .002, f'Reference joint convention mismatch: {max(head_errors)}'

for collection in bpy.data.collections:
    collection.hide_render = True
    collection.hide_viewport = True
collection = bpy.data.collections.new('IK Pose Sweep')
bpy.context.scene.collection.children.link(collection)
mesh = source.copy()
mesh.data = source.data.copy()
mesh.name = 'IK Arms - production solver'
mesh.parent = None
mesh.matrix_world = Matrix.Identity(4)
mesh.scale = (.01, .01, .01)
mesh.modifiers.clear()
mesh.animation_data_clear()
if mesh.data.shape_keys:
    mesh.shape_key_clear()
collection.objects.link(mesh)
working = bpy.data.objects.get('Working_Skm_Player_Mesh')
if working and working.data.materials:
    mesh.data.materials.clear()
    for mat in working.data.materials:
        mesh.data.materials.append(mat)

names = {name: i for i, name in enumerate(sweep['bones'])}
vertices = np.array([v.co[:] for v in source.data.vertices], dtype=np.float64)
weights = np.zeros((len(vertices), 4), dtype=np.float64)
bones = np.zeros((len(vertices), 4), dtype=np.int32)
for vertex in source.data.vertices:
    assert len(vertex.groups) <= 4
    for j, group in enumerate(vertex.groups):
        bones[vertex.index, j] = names[source.vertex_groups[group.group].name]
        weights[vertex.index, j] = group.weight
assert np.max(np.abs(weights.sum(axis=1)-1)) < .001
mesh.data.calc_loop_triangles()
triangles = np.array([t.vertices[:] for t in mesh.data.loop_triangles])

def areas(points):
    return np.linalg.norm(np.cross(points[triangles[:, 1]]-points[triangles[:, 0]],
                                   points[triangles[:, 2]]-points[triangles[:, 0]]), axis=1)*.5

reference_area = areas(vertices)
significant = reference_area > .01
mesh.shape_key_add(name='Basis')
reports = []
for frame in sweep['frames']:
    palette = np.asarray(frame['skin'], dtype=np.float64).reshape((-1, 3, 4))
    posed = np.zeros_like(vertices)
    for j in range(4):
        selected = palette[bones[:, j]]
        posed += weights[:, j, None]*(np.einsum('nij,nj->ni', selected[:, :, :3], vertices)+selected[:, :, 3])
    assert np.isfinite(posed).all(), f'Non-finite skin at frame {frame["frame"]}'
    ratio = areas(posed)[significant]/reference_area[significant]
    report = {'frame': frame['frame'], 'label': frame['label'],
              'minAreaRatio': float(ratio.min()), 'maxAreaRatio': float(ratio.max()),
              'collapsedTrianglesBelowOnePercent': int((ratio < .01).sum()),
              'stretchedTrianglesAboveTenTimes': int((ratio > 10).sum())}
    reports.append(report)
    key = mesh.shape_key_add(name=f'{frame["frame"]:03d} {frame["label"]}')
    key.data.foreach_set('co', posed.astype(np.float32).ravel())
    for at, value in [(frame['frame']-1, 0), (frame['frame'], 1), (frame['frame']+1, 0)]:
        key.value = value
        key.keyframe_insert(data_path='value', frame=at)
    key.value = 0

def material(name, color, roughness=.7):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*color, 1)
    bsdf.inputs['Roughness'].default_value = roughness
    return mat

def move_to_collection(obj):
    for owner in list(obj.users_collection):
        owner.objects.unlink(obj)
    collection.objects.link(obj)

torso_mat = material('Body position guide', (.085, .10, .12))
marker_mat = material('Wrist targets', (.04, .65, .35))
shoulder_mat = material('Solved shoulders', (.95, .35, .06))
bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, location=(0, 1.24, -.12))
torso = bpy.context.object
torso.name = 'Body guide (not game geometry)'
torso.scale = (.18, .30, .09)
torso.data.materials.append(torso_mat)
move_to_collection(torso)
for side in range(2):
    for joint, mat in [(0, shoulder_mat), (2, marker_mat)]:
        bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=8, radius=.008)
        obj = bpy.context.object
        obj.name = f'{"Right" if side else "Left"} {"shoulder" if joint == 0 else "wrist target"}'
        obj.data.materials.append(mat)
        move_to_collection(obj)
        for frame in sweep['frames']:
            obj.location = Vector(frame['joints'][side][joint])*.01
            obj.keyframe_insert(data_path='location', frame=frame['frame'])

scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.samples = 16
scene.cycles.use_denoising = True
scene.render.resolution_x = 640
scene.render.resolution_y = 560
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.world.color = (.12, .12, .12)
scene.view_settings.view_transform = 'AgX'
camera_data = bpy.data.cameras.new('IK inspection camera')
camera = bpy.data.objects.new('IK inspection camera', camera_data)
collection.objects.link(camera)
camera.location = (2.15, 2.0, 2.65)
target = Vector((0, 1.43, .12))
direction = (target-camera.location).normalized()
right = direction.cross(Vector((0, 1, 0))).normalized()
up = right.cross(direction).normalized()
camera.rotation_euler = Matrix(((right.x, up.x, -direction.x),
                               (right.y, up.y, -direction.y),
                               (right.z, up.z, -direction.z))).to_euler()
camera_data.type = 'ORTHO'
camera_data.ortho_scale = 1.95
scene.camera = camera
for name, pos, power, size in [('Key', (1.4, 2.6, 2.4), 650, 2), ('Fill', (-1.5, 1.8, 1.3), 450, 2), ('Rim', (0, 2.3, -1.7), 550, 1.5)]:
    light_data = bpy.data.lights.new(name, 'AREA')
    light_data.energy = power
    light_data.shape = 'DISK'
    light_data.size = size
    light = bpy.data.objects.new(name, light_data)
    collection.objects.link(light)
    light.location = pos
    light.rotation_euler = (target-light.location).to_track_quat('-Z', 'Y').to_euler()
scene.frame_start = 1
scene.frame_end = len(sweep['frames'])
scene.render.fps = 30
scene.frame_set(20)
mesh.select_set(True)
bpy.context.view_layer.objects.active = mesh
scene['IK verification'] = 'Skin matrices exported from the exact production C++ solver. Orange: solved shoulders. Green: final wrist targets.'
scene['IK source'] = str(args.sweep)
bpy.ops.wm.save_as_mainfile(filepath=str(args.out/'Arm-IK-Pose-Sweep.blend'))
summary = {'jointImportMaxError': max(head_errors), 'productionSolver': sweep['validation'],
           'meshFrames': reports, 'rendered': []}
selected = [10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 130, 150, 160, 200, 210, 240, 250, 260]
for frame in selected:
    scene.frame_set(frame)
    path = args.out/f'pose-{frame:03d}.png'
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    summary['rendered'].append({'frame': frame, 'label': sweep['frames'][frame-1]['label'], 'path': str(path)})
(args.out/'verification.json').write_text(json.dumps(summary, indent=2))
print('IK_BLENDER_VALIDATION', json.dumps({'frames': len(reports), 'jointImportMaxError': max(head_errors),
      'maxAreaRatio': max(r['maxAreaRatio'] for r in reports), 'renders': len(selected)}))
