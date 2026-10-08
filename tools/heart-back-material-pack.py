"""Package approved local textures and native content references; no install."""
from pathlib import Path
import struct,json,hashlib,sys
import numpy as np
from PIL import Image
r=Path(sys.argv[1])
channels=['D','N','S','SP','E'];out=bytearray(struct.pack('<8sI',b'DVRHMT01',len(channels)))
report=[]
def fnv(data):
    h=14695981039346656037
    for b in data:h=((h^int(b))*1099511628211)&0xffffffffffffffff
    return h
for channel in channels:
    path=r/'native-dds/Startup/Texture2D'/('Heart_'+channel+'.dds');raw=path.read_bytes()
    assert raw[:4]==b'DDS ' and raw[84:88]==b'DXT1'
    h,w=struct.unpack_from('<II',raw,12);ref=Image.open(path).convert('RGB');assert ref.size==(w,h)
    p=np.asarray(ref,dtype=float);mean=p.mean(axis=(0,1));score=((p-mean)**2).sum(axis=2)
    # Regular coverage plus high-information regions prevents a mostly blank
    # emissive texture from being identified by its black background alone.
    probes=[(int((x+.5)*w/4),int((y+.5)*h/8)) for y in range(8) for x in range(4)]
    features=[]
    for y in range(8):
        for x in range(8):
            tile=score[y*h//8:(y+1)*h//8,x*w//8:(x+1)*w//8]
            iy,ix=np.unravel_index(tile.argmax(),tile.shape)
            features.append((float(tile[iy,ix]),x*w//8+ix,y*h//8+iy))
    probes.extend((x,y) for _,x,y in sorted(features,reverse=True)[:32]);assert len(probes)==64
    coords=np.array([((x*65536+32768)//w,(y*65536+32768)//h) for x,y in probes],dtype='<u2')
    references=[];dim=w
    while dim>=64:
        mip_raw=(r/'native-mip-fixtures'/(channel+'-'+str(dim)+'.dds')).read_bytes()
        image=np.asarray(Image.open(r/'native-mip-fixtures'/(channel+'-'+str(dim)+'.dds')).convert('RGB'));xy=coords.astype(np.uint32)*dim//65536
        rgb=image[xy[:,1],xy[:,0]].tobytes()
        blocks=b''.join(mip_raw[128+((int(y)//4)*(dim//4)+int(x)//4)*8:128+((int(y)//4)*(dim//4)+int(x)//4)*8+8] for x,y in xy)
        digest=fnv(blocks)
        references.append(struct.pack('<IIQ',dim,dim,digest)+rgb);dim//=2
    image=Image.open(r/('Heart_back_matched_'+channel+'.png')).convert('RGB')
    target=1024 if channel in ['D','N'] else 512
    image=image.resize((target,target),Image.Resampling.BOX);levels=[];dim=target
    while True:
        pixel=np.array(image,dtype=np.uint8)
        if channel=='N':
            v=pixel.astype(float)/127.5-1;v/=np.maximum(np.linalg.norm(v,axis=2,keepdims=True),1e-9)
            pixel=np.clip(np.rint((v+1)*127.5),0,255).astype(np.uint8)
        bgra=np.full((dim,dim,4),255,dtype=np.uint8);bgra[:,:,:3]=pixel[:,:,::-1]
        levels.append(struct.pack('<II',dim,dim)+bgra.tobytes())
        if dim==1:break
        dim//=2;image=image.resize((dim,dim),Image.Resampling.BOX)
    out+=struct.pack('<II',len(references),len(levels))+coords.tobytes()+b''.join(references)+b''.join(levels)
    report.append({'channel':channel,'source_size':w,'reference_levels':len(references),'replacement_size':target,'mips':len(levels)})
(r/'dishonored_vr_heart_material.bin').write_bytes(out)
(r/'material-export.json').write_text(json.dumps({'textures':report,'bytes':len(out),'sha256':hashlib.sha256(out).hexdigest()},indent=2))
print(json.dumps(report),len(out),'bytes')
