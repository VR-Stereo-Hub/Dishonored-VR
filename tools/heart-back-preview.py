"""Build a local-only Heart back proposal from a user-owned glTF export.

Run inside Blender. Outputs are game-derived review artifacts, never Git content.
This script does not install assets or write to the game.
"""
import bpy, bmesh, json, math, random, sys
import numpy as np
from mathutils import Vector, Matrix
from mathutils.geometry import delaunay_2d_cdt
from mathutils.bvhtree import BVHTree
from pathlib import Path
ROOT = Path(sys.argv[sys.argv.index('--') + 1])
random.seed(42)
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'heart-original-inspection.blend'))
scene=bpy.context.scene
heart=next(o for o in scene.objects if o.type=='MESH' and o.name.startswith('Heart'))
heart.name='Original Heart - unchanged geometry'
added=bpy.data.collections.new('PROPOSAL - new back geometry');scene.collection.children.link(added)
def add_obj(name,data):
    o=bpy.data.objects.new(name,data);added.objects.link(o);return o
bm=bmesh.new();bm.from_mesh(heart.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
todo={e for e in bm.edges if e.is_boundary};groups=[]
while todo:
    e=todo.pop();group={e};stack=list(e.verts)
    while stack:
        v=stack.pop()
        for e in v.link_edges:
            if e in todo:todo.remove(e);group.add(e);stack.extend(e.verts)
    groups.append(group)
boundary=max(groups,key=len);start=min({v for e in boundary for v in e.verts},key=lambda v:v.co.y)
ring=[start];prev=None;cur=start
while True:
    nxt=next(v for e in cur.link_edges if e in boundary for v in e.verts if v!=cur and v!=prev)
    if nxt==start:break
    ring.append(nxt);prev,cur=cur,nxt
coords=np.array([tuple(v.co) for v in ring],dtype=np.float64)
uvlayer=bm.loops.layers.uv.active
uvs=np.array([tuple(v.link_loops[0][uvlayer].uv) for v in ring])
tex=bpy.data.images.get('Heart_D.png');w,h=tex.size
pixels=np.empty(w*h*4,dtype=np.float32);tex.pixels.foreach_get(pixels);pixels=pixels.reshape((h,w,4))
colors=pixels[np.clip((uvs[:,1]*h).astype(int),0,h-1),np.clip((uvs[:,0]*w).astype(int),0,w-1),:3]
colors=np.where(colors<=.04045,colors/12.92,((colors+.055)/1.055)**2.4)
# CDT respects the concave vessel outline. The original boundary is kept exact.
xy=[Vector(v[:2]) for v in coords]
for iy,y in enumerate(np.arange(coords[:,1].min()+.002,coords[:,1].max(),.003)):
    for x in np.arange(coords[:,0].min()+.002,coords[:,0].max(),.003):xy.append(Vector((x+(.0015 if iy%2 else 0),y)))
face=list(range(len(coords)))
if sum(coords[i,0]*coords[(i+1)%len(coords),1]-coords[(i+1)%len(coords),0]*coords[i,1] for i in face)<0:face.reverse()
v2,edges,faces,orig,_,_=delaunay_2d_cdt(xy,[],[face],1,1e-7,True)
used=sorted({v for f in faces for v in f});remap={v:i for i,v in enumerate(used)}
v2=[v2[v] for v in used];orig=[orig[v] for v in used];faces=[[remap[v] for v in f] for f in faces]
pos=np.array([tuple(v) for v in v2]);n=len(pos)
fixed={i:next(j for j in ids if j<len(coords)) for i,ids in enumerate(orig) if any(j<len(coords) for j in ids)}
adj=[set() for _ in range(n)]
for f in faces:
    for i in range(len(f)):a,b=f[i],f[(i+1)%len(f)];adj[a].add(b);adj[b].add(a)
maxdeg=max(map(len,adj));nbr=np.zeros((n,maxdeg),dtype=int);wt=np.zeros((n,maxdeg))
for i,ns in enumerate(adj):
    for j,k in enumerate(ns):nbr[i,j]=k;wt[i,j]=1/max(np.linalg.norm(pos[k]-pos[i]),1e-6)
wt/=wt.sum(axis=1)[:,None]
dist=np.full(n,1e9)
for i in range(len(coords)):
    a,b=coords[i,:2],coords[(i+1)%len(coords),:2];ab=b-a
    t=np.clip(((pos-a)*ab).sum(axis=1)/(ab@ab),0,1)
    dist=np.minimum(dist,np.linalg.norm(pos-(a+t[:,None]*ab),axis=1))
z=np.full(n,coords[:,2].mean());fixed_ids=np.array(list(fixed));fixed_src=np.array(list(fixed.values()))
for _ in range(1800):
    z=(z[nbr]*wt).sum(axis=1);z[fixed_ids]=coords[fixed_src,2]
# Rounded posterior wall with a shallow anatomical groove and asymmetric lobes.
bulge=.052*np.power(np.clip(dist/.055,0,1),.62)
x,y=pos[:,0],pos[:,1]
groove=.0032*np.exp(-((x-(.004+.085*(y-.04)))/.009)**2)*np.clip(dist/.016,0,1)
z-=bulge;z+=groove
pos[fixed_ids]=coords[fixed_src,:2];z[fixed_ids]=coords[fixed_src,2]
verts=[(float(p[0]),float(p[1]),float(zz)) for p,zz in zip(pos,z)]
mesh=bpy.data.meshes.new('Posterior closure - constrained to shipped edge');mesh.from_pydata(verts,[],[list(reversed(f)) for f in faces]);mesh.update()
cap=add_obj('Sculpted posterior wall',mesh)
for poly in mesh.polygons:poly.use_smooth=True
uv=mesh.uv_layers.new(name='Posterior flesh patch')
for poly in mesh.polygons:
    for li in poly.loop_indices:
        p=pos[mesh.loops[li].vertex_index]
        uv.data[li].uv=(.245+(p[0]+.055)/.14*.255,.105+(p[1]+.07)/.23*.30)
# Boundary color is interpolated only near the edge; the center uses flesh texture.
edgeattr=mesh.color_attributes.new(name='Edge color',type='FLOAT_COLOR',domain='POINT')
maskattr=mesh.attributes.new(name='Edge blend',type='FLOAT',domain='POINT')
for i,p in enumerate(pos):
    ds=np.linalg.norm(coords[:,:2]-p,axis=1);ids=np.argsort(ds)[:3];ww=1/np.maximum(ds[ids],.0002)**3;ww/=ww.sum()
    c=(colors[ids]*ww[:,None]).sum(axis=0)
    edgeattr.data[i].color=(*map(float,c),1);maskattr.data[i].value=float(math.exp(-dist[i]/.007))
mat=bpy.data.materials.new('Posterior flesh - source palette');mat.use_nodes=True
nodes=mat.node_tree.nodes;nodes.clear();links=mat.node_tree.links
out=nodes.new('ShaderNodeOutputMaterial');bs=nodes.new('ShaderNodeBsdfPrincipled');links.new(bs.outputs[0],out.inputs[0]);bs.inputs['Roughness'].default_value=.52
im=nodes.new('ShaderNodeTexImage');im.image=tex
col=nodes.new('ShaderNodeVertexColor');col.layer_name='Edge color'
mask=nodes.new('ShaderNodeAttribute');mask.attribute_name='Edge blend'
mix=nodes.new('ShaderNodeMixRGB');links.new(mask.outputs['Fac'],mix.inputs[0]);links.new(im.outputs['Color'],mix.inputs[1]);links.new(col.outputs['Color'],mix.inputs[2]);links.new(mix.outputs[0],bs.inputs['Base Color'])
noise=nodes.new('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=125;noise.inputs['Detail'].default_value=3;noise.inputs['Roughness'].default_value=.8
bump=nodes.new('ShaderNodeBump');bump.inputs['Strength'].default_value=.15;bump.inputs['Distance'].default_value=.0003;links.new(noise.outputs['Fac'],bump.inputs['Height']);links.new(bump.outputs['Normal'],bs.inputs['Normal']);mesh.materials.append(mat)
capnormal=nodes.new('ShaderNodeTexImage');capnormal.image=bpy.data.images.load(str(ROOT/'originals/Startup/Texture2D/Heart_N.png'));capnormal.image.colorspace_settings.name='Non-Color'
capnm=nodes.new('ShaderNodeNormalMap');capnm.inputs['Strength'].default_value=.65;links.new(capnormal.outputs['Color'],capnm.inputs['Color']);links.new(capnm.outputs['Normal'],bump.inputs['Normal'])
# New details sit on the actual new surface, with tapered anatomical veins.
tree=BVHTree.FromPolygons([Vector(v) for v in verts],faces,all_triangles=True)
def surface(x,y,lift=0):
    hit=tree.ray_cast(Vector((x,y,-.3)),Vector((0,0,1)))
    return Vector((x,y,hit[0].z-lift)) if hit[0] else None
def solid(name,c,rough=.5,metal=0):
    m=bpy.data.materials.new(name);m.use_nodes=True;b=m.node_tree.nodes.get('Principled BSDF');b.inputs['Base Color'].default_value=(*c,1);b.inputs['Roughness'].default_value=rough;b.inputs['Metallic'].default_value=metal;return m
veinmat=solid('Subtle posterior vessels',(.086,.038,.036),.53)
wiremat=solid('Aged dark retaining wire',(.045,.043,.04),.35,.75)
scarmat=solid('Old healed suture',(.19,.087,.061),.63)
def tube(name,points,radius,material,radii=None):
    cu=bpy.data.curves.new(name,'CURVE');cu.dimensions='3D';cu.resolution_u=10;cu.bevel_depth=radius;cu.bevel_resolution=3
    sp=cu.splines.new('POLY');sp.points.add(len(points)-1)
    for i,(bp,p) in enumerate(zip(sp.points,points)):
        bp.co=(*p,1);bp.radius=radii[i] if radii else 1
    cu.materials.append(material);return add_obj(name,cu)
def track(name,path,radius,material,rs=None,lift=.0003):
    path=[Vector(p) for p in path];points=[];radii=[]
    for i in range(len(path)-1):
        a,b,c,d=path[max(0,i-1)],path[i],path[i+1],path[min(len(path)-1,i+2)]
        for t in np.linspace(0,1,18,endpoint=(i==len(path)-2)):
            p=.5*((2*b)+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t)
            r=(rs[i]*(1-t)+rs[i+1]*t) if rs else 1
            hit=surface(p.x,p.y,lift*r)
            if hit is not None:points.append(hit);radii.append(r)
    if len(points)>1:return tube(name,points,radius,material,radii)
track('Posterior coronary vessel',[(.009,.119),(.012,.1),(.008,.078),(.007,.051),(-.001,.024),(.004,-.004),(.003,-.036)],.0013,veinmat,[.06,1,1,.88,.7,.4,.01],lift=-.00072)
track('Coronary branch left',[(.008,.078),(-.008,.072),(-.022,.056),(-.038,.043)],.0009,veinmat,[.9,.85,.5,.01],lift=-.00055)
track('Coronary branch right',[(.007,.051),(.026,.037),(.043,.02),(.048,.009)],.00075,veinmat,[.85,.7,.3,.01],lift=-.00046)
track('Lower vein',[(-.001,.024),(-.013,.018),(-.022,.001),(-.024,-.009)],.0006,veinmat,[.9,.8,.4,.01],lift=-.00036)
track('Upper small vein',[(.012,.1),(.027,.104),(.04,.096),(.048,.088)],.0007,veinmat,[1,.7,.5,.01],lift=-.00044)
# Two crossing retainers continue the front's utilitarian wire language.
track('Back retaining wire diagonal', [tuple(coords[12,:2]),(-.035,.035),(-.009,.012),(.01,-.015),tuple(coords[58,:2])],.0008,wiremat,lift=.0008)
track('Back retaining wire transverse',[tuple(coords[3,:2]),(-.017,.005),(.005,.035),(.034,.06),tuple(coords[47,:2])],.00075,wiremat,lift=.001)
scarpath=[(-.024,.065),(-.03,.044),(-.022,.028),(-.018,.002)]
track('Healed service incision',scarpath,.0006,scarmat,lift=-.0002)
for i in range(7):
    t=i/6;y=.057-t*.05;x=-.028+max(0,t-.3)*.013
    track('Suture stitch %02d'%i,[(x-.003,y-.001),(x,y+.0005),(x+.003,y+.002)],.00028,wiremat,lift=.0008)
# Preview lens is transparent enough to reveal the shipped mechanism behind it.
lens=next(m for m in heart.data.materials if 'lens' in m.name.lower())
bs0=next(n for n in lens.node_tree.nodes if n.type=='BSDF_PRINCIPLED');bs0.inputs['Base Color'].default_value=(.12,.14,.12,1)
for link in list(lens.node_tree.links):
    if link.to_socket==bs0.inputs['Base Color']:lens.node_tree.links.remove(link)
bs0.inputs['Transmission Weight'].default_value=.92;bs0.inputs['Roughness'].default_value=.16;bs0.inputs['IOR'].default_value=1.35
# Reuse the shipped normal map on the original front for inspection fidelity.
front=heart.data.materials[0];p=next(n for n in front.node_tree.nodes if n.type=='BSDF_PRINCIPLED');p.inputs['Roughness'].default_value=.52
normal=front.node_tree.nodes.new('ShaderNodeTexImage');normal.image=bpy.data.images.load(str(ROOT/'originals/Startup/Texture2D/Heart_N.png'));normal.image.colorspace_settings.name='Non-Color'
nmap=front.node_tree.nodes.new('ShaderNodeNormalMap');nmap.inputs['Strength'].default_value=.65;front.node_tree.links.new(normal.outputs['Color'],nmap.inputs['Color']);front.node_tree.links.new(nmap.outputs['Normal'],p.inputs['Normal'])
for light in [o for o in scene.objects if o.type=='LIGHT']:
    bpy.data.objects.remove(light,do_unlink=True)
center=Vector((.003,.054,.01))
def light(name,loc,power,size,color):
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size;data.color=color;o=bpy.data.objects.new(name,data);scene.collection.objects.link(o);o.location=center+Vector(loc);o.rotation_euler=(center-o.location).to_track_quat('-Z','Y').to_euler()
light('Warm key',(-.3,.25,-.45),9,.3,(1,.88,.78));light('Soft fill',(.35,.1,-.2),6,.3,(.72,.84,1));light('Upper rim',(.08,.3,.3),12,.25,(1,.92,.82))
scene.world.color=(.075,.085,.1)
scene.render.resolution_x=900;scene.render.resolution_y=1050;scene.cycles.samples=48
scene.render.image_settings.file_format='PNG';scene.render.film_transparent=False
scene.camera.data.type='ORTHO';scene.camera.data.ortho_scale=.29
def aim_camera(direction):
    scene.camera.location=center+Vector(direction).normalized()*.7
    f=(center-scene.camera.location).normalized();right=f.cross(Vector((0,1,0))).normalized();up=right.cross(f).normalized()
    scene.camera.rotation_euler=Matrix((right,up,-f)).transposed().to_euler()
for im in bpy.data.images:
    if im.source=='FILE':im.pack()
for label,direction in [('back',(0,0,-1)),('three-quarter',(.68,.1,-1)),('front',(0,0,1))]:
    aim_camera(direction)
    for state in ['before','after']:
        added.hide_render=state=='before';scene.render.filepath=str(ROOT/(state+'-'+label+'.png'));bpy.ops.render.render(write_still=True)
added.hide_render=False
aim_camera((.35,.1,-1))
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            area.spaces.active.region_3d.view_location=center;area.spaces.active.region_3d.view_distance=.45
            area.spaces.active.region_3d.view_rotation=scene.camera.rotation_euler.to_quaternion();area.spaces.active.shading.type='MATERIAL'
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Heart-backside-proposal.blend'))
report={'original_vertices':len(heart.data.vertices),'original_triangles':len(heart.data.polygons),'main_open_boundary_edges':len(coords),'new_shell_vertices':len(verts),'new_shell_triangles':len(faces),'preserved_rim_vertices':len(fixed),'max_rim_error':float(max(np.linalg.norm(np.array(verts[i])-coords[j]) for i,j in fixed.items())),'new_objects':[o.name for o in added.objects],'scope':'Local static visual proposal. No runtime integration, animation or engine validation.'}
(ROOT/'proposal-report.json').write_text(json.dumps(report,indent=2));print('PROPOSAL_REPORT',json.dumps(report))
