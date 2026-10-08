"""Remap the local Heart backing to pale native flesh; no installation.

Run in Blender with -- <local proposal directory>, then heart-back-export.py.
Game-derived blend and exported model files remain local and untracked.
"""
import bpy,sys
from pathlib import Path
r=Path(sys.argv[sys.argv.index('--')+1])
bpy.ops.wm.open_mainfile(filepath=str(r/'Heart-backside-proposal.blend'))
cap=bpy.data.objects['Sculpted posterior wall'];m=cap.data
original=bpy.data.objects['Original Heart - unchanged geometry']
# Use a continuous pale flesh region of the native atlas. This is exported UV
# data, not a Blender-only tint that would disappear in the game shader.
for loop in m.loops:
    x,y,z=m.vertices[loop.vertex_index].co
    m.uv_layers.active.data[loop.index].uv=(.35+(x+.055)/.14*.055,.91+(y+.07)/.23*.04)
mat=cap.data.materials[0];nodes=mat.node_tree.nodes;links=mat.node_tree.links
bs=next(n for n in nodes if n.type=='BSDF_PRINCIPLED')
im=next(n for n in nodes if n.type=='TEX_IMAGE' and n.image.name=='Heart_D.png')
for l in list(links):
    if l.to_socket==bs.inputs['Base Color']:links.remove(l)
links.new(im.outputs['Color'],bs.inputs['Base Color'])
cap['native_seam_normals']=True
for image in bpy.data.images:
    if image.source=='FILE':image.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(r/'Heart-backside-proposal.blend'))
