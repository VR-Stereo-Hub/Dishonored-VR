"""Export locally authored Heart additions and skin reference from Blender.

Run in Blender: --background --python this.py -- <proposal directory> <tools directory>.
All output is derived from a user-owned game asset and must remain untracked.
"""
import bpy, sys, struct, json, importlib.util
import numpy as np
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
root, tools = map(Path, sys.argv[sys.argv.index('--')+1:])
spec=importlib.util.spec_from_file_location('rig',tools/'prepare-arm-rig.py')
rig=importlib.util.module_from_spec(spec);spec.loader.exec_module(rig)
bones,points,triangles=rig.read_psk(root/'psk-reference/Startup/SkeletalMesh3/Heart.psk')
bpy.ops.wm.open_mainfile(filepath=str(root/'Heart-backside-proposal.blend'))
cap=bpy.data.objects['Sculpted posterior wall'];mesh=cap.data
orig=bpy.data.objects['Original Heart - unchanged geometry']
# The authored preview has no gripping hand. Give the runtime fingers clearance
# by reducing only the added rear bulge; the shipped boundary stays fixed.
edge_counts={}
for poly in mesh.polygons:
    ids=list(poly.vertices)
    for a,b in zip(ids,ids[1:]+ids[:1]):
        key=tuple(sorted((a,b)));edge_counts[key]=edge_counts.get(key,0)+1
rim=np.array([tuple(mesh.vertices[i].co)[:2] for e,c in edge_counts.items() if c==1 for i in e])
def clearance(p):
    d=np.linalg.norm(rim-np.array(p[:2]),axis=1).min()
    return .015*min(d/.035,1)
for v in mesh.vertices:v.co.z+=clearance(v.co)
mesh.update()
for obj in bpy.data.collections['PROPOSAL - new back geometry'].objects:
    if obj.type=='CURVE':
        for spline in obj.data.splines:
            for p in spline.points:p.co.z+=clearance(p.co)
bpy.context.view_layer.update()
refpos=np.array([p[:3] for p in points])*.01
weights=np.zeros((len(points),len(bones)))
for i,p in enumerate(points):
    for b,w in zip(p[3:7],p[7:]):
        if w>0:weights[i,b]=w
pos=np.array([tuple(v.co) for v in mesh.vertices])
nearest=np.array([np.argmin(((refpos-p)**2).sum(axis=1)) for p in pos])
dist=np.linalg.norm(pos-refpos[nearest],axis=1);fixed=np.where(dist<1e-6)[0]
assert len(fixed)==65, len(fixed)
adj=[set() for _ in pos]
for e in mesh.edges:
    a,b=e.vertices;adj[a].add(b);adj[b].add(a)
degree=max(map(len,adj));neighbor=np.zeros((len(pos),degree),int);coef=np.zeros((len(pos),degree))
for i,ns in enumerate(adj):
    for j,k in enumerate(ns):neighbor[i,j]=k;coef[i,j]=1/max(np.linalg.norm(pos[i]-pos[k]),1e-7)
coef/=coef.sum(axis=1)[:,None]
skin=weights[nearest].copy()
for _ in range(1400):
    skin=(skin[neighbor]*coef[:,:,None]).sum(axis=1);skin[fixed]=weights[nearest[fixed]]
mesh.calc_loop_triangles();faces=[tuple(t.vertices) for t in mesh.loop_triangles]
tree=BVHTree.FromPolygons([Vector(p) for p in pos],faces,all_triangles=True)
def skin_at(p):
    q,_,face,_=tree.find_nearest(Vector(p));ids=faces[face]
    a,b,c=pos[list(ids)];u=b-a;v=c-a;d=np.array(q)-a
    uv=np.linalg.lstsq(np.stack((u,v),axis=1),d,rcond=None)[0]
    w=np.maximum([1-uv.sum(),*uv],0);w/=sum(w)
    return (skin[list(ids)]*w[:,None]).sum(axis=0)
tex=bpy.data.images['Heart_D.png'];width,height=tex.size
pixels=np.array(tex.pixels[:]).reshape(height,width,4)[:,:,:3]
def nearest_uv(color):
    color=np.array(color);srgb=np.where(color<=.0031308,color*12.92,1.055*color**(1/2.4)-.055)
    # Keep details within the same weathered-flesh atlas region.
    crop=pixels[int(height*.1):int(height*.41),int(width*.245):int(width*.50)]
    y,x=np.unravel_index(np.argmin(((crop-srgb)**2).sum(axis=2)),crop.shape[:2])
    return ((x+int(width*.245)+.5)/width,(y+int(height*.1)+.5)/height)
verts=[];indices=[];counts={};deps=bpy.context.evaluated_depsgraph_get()
for obj in bpy.data.collections['PROPOSAL - new back geometry'].objects:
    ev=obj.evaluated_get(deps);m=ev.to_mesh();m.calc_loop_triangles()
    if obj==cap:
        m.calc_tangents();uvs=m.uv_layers.active.data
    else:
        base=next(n for n in obj.data.materials[0].node_tree.nodes if n.type=='BSDF_PRINCIPLED').inputs['Base Color'].default_value[:3]
        uv=nearest_uv(base);uvs=None
    unique={};before=len(verts)
    for tri in m.loop_triangles:
        out=[]
        for li in tri.loops:
            loop=m.loops[li];vi=loop.vertex_index;p=m.vertices[vi].co;n=loop.normal
            if n.length<.5:n=m.vertices[vi].normal
            if uvs:
                uv=tuple(uvs[li].uv);t=loop.tangent;sign=loop.bitangent_sign
            else:
                t=n.cross(Vector((0,1,0)))
                if t.length<.001:t=n.cross(Vector((1,0,0)))
                t.normalize();sign=1
            inf=skin[vi] if obj==cap else skin_at(p)
            bi=np.argsort(inf)[-4:][::-1];bw=inf[bi];bw/=bw.sum()
            # Model coordinates reflect Y. UV V reflection cancels the tangent handedness reflection.
            row=(p.x*100,-p.y*100,p.z*100,n.x,-n.y,n.z,t.x,-t.y,t.z,sign,uv[0],1-uv[1],*map(int,bi),*map(float,bw))
            key=struct.pack('<12f4i4f',*row)
            if key not in unique:unique[key]=len(verts);verts.append(key)
            out.append(unique[key])
        indices.extend(reversed(out))
    counts[obj.name]=len(verts)-before;ev.to_mesh_clear()
assert len(verts)<=65536 and len(indices)//3<=131072
raw=bytearray(struct.pack('<8sII',b'DVRHRT01',len(verts),len(indices)//3));raw.extend(b''.join(verts));raw.extend(struct.pack('<%dI'%len(indices),*indices))
(root/'dishonored_vr_heart_back.bin').write_bytes(raw)
raw=bytearray(struct.pack('<8sIII',b'DVRIK002',len(bones),len(points),3672))
for name,parent,x,y,z in bones:raw.extend(struct.pack('<64si3f',name,parent,x,-y,z))
for p in points:raw.extend(struct.pack('<3f4i4f',p[0],-p[1],*p[2:]))
(root/'dishonored_vr_heart_rig.bin').write_bytes(raw)
report={'vertices':len(verts),'triangles':len(indices)//3,'boundary_skin_matches':len(fixed),'objects':counts,'bones':len(bones)}
(root/'export-report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
