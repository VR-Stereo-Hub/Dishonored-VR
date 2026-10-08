"""Measure forearm radius in a production-solver roll sweep (local assets only).
Run with a Python that has numpy, including Blender's bundled Python.
"""
import argparse
import json
from pathlib import Path
import struct
import numpy as np

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('reference',type=Path)
parser.add_argument('sweep',type=Path)
parser.add_argument('--minimum',type=float,default=.85)
parser.add_argument('--output',type=Path)
args=parser.parse_args()
raw=args.reference.read_bytes()
magic,n,nv,nt=struct.unpack_from('<8sIII',raw)
assert magic==b'DVRIK002'
b=[struct.unpack_from('<64si3f',raw,20+i*80) for i in range(n)]
names=[x[0].split(b'\0')[0].decode() for x in b]
heads=np.array([x[2:] for x in b])
v=[struct.unpack_from('<3f4i4f',raw,20+n*80+i*44) for i in range(nv)]
points=np.array([x[:3] for x in v]); indices=np.maximum(0,np.array([x[3:7] for x in v])); weights=np.array([x[7:] for x in v])
sweep=json.loads(args.sweep.read_text())
records=[]
for side,suffix in enumerate(['L','R']):
    lower=names.index('lower_arm_'+suffix+'_jnt'); wrist=names.index('hand_'+suffix+'_jnt')
    e,w=heads[[lower,wrist]];axis=w-e;ref_length=np.linalg.norm(axis);axis/=ref_length
    offset=points-e;fraction=(offset@axis)/ref_length
    radius=np.linalg.norm(offset-np.outer(offset@axis,axis),axis=1)
    # Keep the shaft, avoiding the elbow cap, wrist joint, fingers and the other
    # arm. Bone ancestry defines membership; do not infer sides from mesh axes.
    owned=[]
    hand=[]
    for bone in range(n):
        at=bone
        for _ in range(n):
            if at==wrist:hand.append(bone);break
            if at<0:break
            at=b[at][1]
        at=bone
        for _ in range(n):
            if at==lower:owned.append(bone);break
            if at<0:break
            at=b[at][1]
    arm_weight=(weights*np.isin(indices,owned)).sum(axis=1)
    # Native hand influence intentionally bends at the wrist. Measure that
    # cuff separately; shaft volume must not be mistaken for wrist flexion.
    hand_weight=(weights*np.isin(indices,hand)).sum(axis=1)
    cuff=(fraction>.1)&(fraction<.98)&(radius>.8)&(radius<10)&(arm_weight>.99)&(hand_weight>0)
    mask=(fraction>.1)&(fraction<.98)&(radius>.8)&(radius<10)&(arm_weight>.99)&(hand_weight==0)
    assert mask.sum()>20, 'No meaningful shaft population'
    for frame in sweep['frames']:
        if not frame['label'].startswith('Roll '):continue
        matrices=np.asarray(frame['skin']).reshape(-1,3,4)
        posed=sum(weights[:,j,None]*(np.einsum('nij,nj->ni',matrices[indices[:,j],:,:3],points)+matrices[indices[:,j],:,3]) for j in range(4))
        _,E,W=np.array(frame['joints'][side]);axis=W-E;axis/=np.linalg.norm(axis)
        offset=posed-E;actual=np.linalg.norm(offset-np.outer(offset@axis,axis),axis=1)
        # Hand scale is the same uniform radial scale carried by the arm.
        scale=np.linalg.norm(matrices[wrist,:,:3][:,0])
        ratio=actual[mask]/(radius[mask]*scale)
        records.append({'frame':frame['frame'],'side':suffix,'label':frame['label'],'vertices':int(mask.sum()),
                        'minRadiusRatio':float(ratio.min()),'p10RadiusRatio':float(np.quantile(ratio,.1)),
                        'medianRadiusRatio':float(np.median(ratio)),
                        'cuffMinRadiusRatio':float(np.min(actual[cuff]/(radius[cuff]*scale)))})
assert records
summary={'minimumRadiusRatio':min(x['minRadiusRatio'] for x in records),'threshold':args.minimum,'samples':records}
if args.output:args.output.write_text(json.dumps(summary,indent=2))
worst=min(records,key=lambda x:x['minRadiusRatio'])
print(json.dumps({'minimumRadiusRatio':summary['minimumRadiusRatio'],'worst':worst,'passed':summary['minimumRadiusRatio']>=args.minimum}))
raise SystemExit(0 if summary['minimumRadiusRatio']>=args.minimum else 1)
