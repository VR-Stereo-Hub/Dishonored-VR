"""Export the approved skinned Blender backing without regenerating its surface."""
import bpy,struct,json,sys
from pathlib import Path
from mathutils import Vector
r,legacy,rig=map(Path,sys.argv[sys.argv.index('--')+1:])
bpy.ops.wm.open_mainfile(filepath=str(r/'Heart-natural-seam.blend'))
obj=bpy.data.objects['Installed Heart back - skinning verification'];mesh=obj.data
source=legacy.read_bytes()
_,nv,nt=struct.unpack_from('<8sII',source);assert len(mesh.vertices)==nv
old=[struct.unpack_from('<12f4i4f',source,16+i*80) for i in range(nv)]
mesh.calc_tangents();mesh.calc_loop_triangles()
vertices=[];indices=[];unique={};cap=0;seen_details=False
for tri in mesh.loop_triangles:
    iscap=mesh.polygons[tri.polygon_index].material_index==1
    if iscap:assert not seen_details;cap+=1
    else:seen_details=True
    ix=[]
    for li in tri.loops:
        loop=mesh.loops[li];vi=loop.vertex_index;p=mesh.vertices[vi].co;n=loop.normal;t=loop.tangent;sign=loop.bitangent_sign
        if t.length<.5:
            t=Vector((old[vi][6],-old[vi][7],old[vi][8]));t=(t-n*t.dot(n)).normalized();sign=old[vi][9]
        uv=mesh.uv_layers.active.data[li].uv
        assert max(abs(a-b) for a,b in zip((p.x*100,-p.y*100,p.z*100),old[vi][:3]))<.00001
        row=(p.x*100,-p.y*100,p.z*100,n.x,-n.y,n.z,t.x,-t.y,t.z,sign,uv.x,1-uv.y,*old[vi][12:])
        if not iscap:row=old[vi]
        packed=struct.pack('<12f4i4f',*row)
        if packed not in unique:unique[packed]=len(vertices);vertices.append(packed)
        ix.append(unique[packed])
    indices.extend(reversed(ix))
assert cap==4279 and len(indices)//3==nt
data=struct.pack('<8sIII',b'DVRHRT02',len(vertices),nt,cap)+b''.join(vertices)+struct.pack('<%dI'%len(indices),*indices)
(r/'dishonored_vr_heart_back.bin').write_bytes(data)
(r/'dishonored_vr_heart_rig.bin').write_bytes(rig.read_bytes())
(r/'runtime-export.json').write_text(json.dumps({'vertices':len(vertices),'triangles':nt,'cap_triangles':cap,'source':'approved Blender mesh; geometry and weights preserved'},indent=2))
print('EXPORTED',len(vertices),nt,cap)

