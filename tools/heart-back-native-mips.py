"""Recover exact texture mips using UModel on an isolated package copy.

Only local copies are edited. Reordering external mip descriptors makes
UModel export that native mip, without writing a new decompressor.
"""
import re,struct,subprocess,sys
from pathlib import Path
r,game,umodel=map(Path,sys.argv[1:])
package=(r/'packages/Startup.upk').read_bytes();scratch=r/'packages/StartupMip.upk'
listing=subprocess.run([str(umodel),'-list','-path='+str(game),'Startup'],capture_output=True,text=True,check=True).stdout
objects={m[3]:(int(m[1],16),int(m[2],16)) for m in re.findall(r'^\s*(\d+)\s+([A-Fa-f0-9]+)\s+([A-Fa-f0-9]+)\s+Texture2D\s+Heart_(D|N|S|SP|E)\s*$',listing,re.M)}
assert len(objects)==5
out=r/'native-mip-fixtures';out.mkdir(exist_ok=True)
for name,(offset,length) in objects.items():
    raw=package[offset:offset+length];top=(r/'native-dds/Startup/Texture2D'/('Heart_'+name+'.dds')).read_bytes();height,width=struct.unpack_from('<II',top,12)
    pattern=struct.pack('<II',width,height);at=raw.find(pattern);assert at>=16
    first=at-16;flags,count,stored,bulk=struct.unpack_from('<4I',raw,first)
    assert flags==17 and count==width*height//2
    pos=first
    while True:
        flags,count,stored,bulk=struct.unpack_from('<4I',raw,pos);payload=pos+16;end=payload+(0 if flags&1 else stored)
        w,h=struct.unpack_from('<II',raw,end)
        assert w==h and w<=width and count==max(4,w)*max(4,h)//2
        if w<64:break
        if w==width:dds=top
        elif flags&1:
            assert flags==17 and end-pos==16
            copy=bytearray(package);a=offset+first;b=offset+pos
            copy[a:a+24],copy[b:b+24]=copy[b:b+24],copy[a:a+24]
            scratch.write_bytes(copy)
            cmd=[str(umodel),'-export','-dds','-path='+str(game),'-out='+str(r/'mip-extraction'),str(scratch),'Heart_'+name]
            result=subprocess.run(cmd,capture_output=True,text=True)
            if result.returncode:raise RuntimeError(result.stdout+result.stderr)
            dds=(r/'mip-extraction/StartupMip/Texture2D'/('Heart_'+name+'.dds')).read_bytes()
        else:
            assert flags==0 and stored==count
            header=bytearray(top[:128]);struct.pack_into('<III',header,12,h,w,count)
            dds=bytes(header)+raw[payload:payload+stored]
        assert struct.unpack_from('<II',dds,12)==(h,w) and len(dds)==128+w*h//2
        (out/(name+'-'+str(w)+'.dds')).write_bytes(dds)
        print(name,w,'exact native BC1',flush=True)
        pos=end+8
scratch.write_bytes(package)
