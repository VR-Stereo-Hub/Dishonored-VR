import bpy, bmesh, json, math, sys
from mathutils import Vector
from pathlib import Path
ROOT = Path(sys.argv[sys.argv.index('--') + 1])
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.gltf(filepath=str(ROOT/'originals/Startup/SkeletalMesh3/Heart.gltf'))
heart = next(o for o in bpy.context.scene.objects if o.type=='MESH' and o.name.startswith('Heart'))
for o in list(bpy.context.scene.objects):
    if o.type=='MESH' and o!=heart: bpy.data.objects.remove(o,do_unlink=True)
texdir=ROOT/'originals/Startup/Texture2D'
for mat in heart.data.materials:
    mat.use_nodes=True
    nodes=mat.node_tree.nodes; nodes.clear(); links=mat.node_tree.links
    out=nodes.new('ShaderNodeOutputMaterial'); p=nodes.new('ShaderNodeBsdfPrincipled')
    tex=nodes.new('ShaderNodeTexImage')
    tex.image=bpy.data.images.load(str(texdir/('HeartLens_D.png' if 'lens' in mat.name else 'Heart_D.png')))
    links.new(tex.outputs['Color'],p.inputs['Base Color'])
    p.inputs['Roughness'].default_value=.55
    # Explicit back-face transparency matches a one-sided game material.
    geom=nodes.new('ShaderNodeNewGeometry'); tr=nodes.new('ShaderNodeBsdfTransparent'); mix=nodes.new('ShaderNodeMixShader')
    links.new(geom.outputs['Backfacing'],mix.inputs[0]); links.new(p.outputs[0],mix.inputs[1]); links.new(tr.outputs[0],mix.inputs[2]); links.new(mix.outputs[0],out.inputs['Surface'])
    mat.use_backface_culling=True
scene=bpy.context.scene
scene.render.engine='CYCLES'; scene.cycles.samples=24; scene.cycles.use_denoising=True
scene.render.resolution_x=600;scene.render.resolution_y=600;scene.render.resolution_percentage=100
scene.world.color=(.24,.24,.24)
scene.view_settings.view_transform='AgX'
center=sum((heart.matrix_world@Vector(p) for p in heart.bound_box),Vector())/8
def area(name,loc,power,size):
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size
    o=bpy.data.objects.new(name,data);scene.collection.objects.link(o);o.location=center+Vector(loc);o.rotation_euler=(center-o.location).to_track_quat('-Z','Y').to_euler()
area('Key',(.4,-.5,.6),32,.5);area('Fill',(-.4,.3,.4),22,.5);area('Bottom',(0,.4,-.5),16,.4)
data=bpy.data.cameras.new('Inspection');cam=bpy.data.objects.new('Inspection',data);scene.collection.objects.link(cam);scene.camera=cam;data.type='ORTHO';data.ortho_scale=.32
bm=bmesh.new();bm.from_mesh(heart.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.000001);bm.verts.ensure_lookup_table();bm.edges.ensure_lookup_table()
bound=[e for e in bm.edges if e.is_boundary];todo=set(bound);loops=[]
while todo:
    first=todo.pop();group={first};vs=set(first.verts);stack=list(first.verts)
    while stack:
        v=stack.pop()
        for e in v.link_edges:
            if e in todo:
                todo.remove(e);group.add(e)
                for v2 in e.verts:
                    if v2 not in vs:vs.add(v2);stack.append(v2)
    loops.append({'edges':len(group),'center':list(sum((v.co for v in vs),Vector())/len(vs)),'verts':[list(v.co) for v in vs]})
report={'bounds':[list(heart.matrix_world@Vector(v)) for v in heart.bound_box],'matrix':[list(r) for r in heart.matrix_world],'vertices':len(heart.data.vertices),'polygons':len(heart.data.polygons),'welded_vertices':len(bm.verts),'boundary_edges':len(bound),'loops':sorted(loops,key=lambda l:-l['edges'])}
(ROOT/'mesh-inspection.json').write_text(json.dumps(report,indent=2));bm.free()
print('INSPECTION',json.dumps({k:v for k,v in report.items() if k!='loops'}));print('BOUNDARIES',[(l['edges'],l['center']) for l in report['loops']])
for label,vec in [('px',(1,0,0)),('nx',(-1,0,0)),('py',(0,1,0)),('ny',(0,-1,0)),('pz',(0,0,1)),('nz',(0,0,-1))]:
    cam.location=center+Vector(vec)*.6;cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(ROOT/('original-'+label+'.png'));bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'heart-original-inspection.blend'))
