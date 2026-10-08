"""Regression for the ActorX export boundary, using an authored synthetic PSK."""
import importlib.util
import math
from pathlib import Path
import struct
import tempfile

spec = importlib.util.spec_from_file_location('prepare_arm_rig', Path(__file__).with_name('prepare-arm-rig.py'))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

def chunk(name, fmt, rows):
    return struct.pack('<20siii', name, 0, struct.calcsize(fmt), len(rows)) + b''.join(struct.pack(fmt, *r) for r in rows)

# Known PSK hierarchy: root at (1,10,3), rotated +90 around Z. Its child
# translates (2,3,4), hence head (-2,12,7); grandchild adds local (0,2,0).
# Expected ENGINE heads are independently stated, with the export Y undone.
q = math.sqrt(.5)
bones = [(b'root', 0, 1, 0, 0, 0, q, q, 1, 10, 3, 1, 1, 1, 1),
         (b'child', 0, 1, 0, 0, 0, 0, 1, 2, 3, 4, 1, 1, 1, 1),
         (b'tip', 0, 0, 1, 0, 0, 0, 1, 0, 2, 0, 1, 1, 1, 1)]
points = [(1,10,3), (-2,12,7), (-4,12,7)]
expected = [(1,-10,3), (-2,-12,7), (-4,-12,7)]
raw = chunk(b'ACTRHEAD', '<', [])
raw += chunk(b'PNTS0000', '<3f', points)
raw += chunk(b'REFSKELT', '<64s3i11f', bones)
raw += chunk(b'RAWWEIGHTS', '<fii', [(1, i, i) for i in range(3)])
raw += chunk(b'FACE0000', '<3H2BI', [(0,1,2,0,0,1)])
with tempfile.TemporaryDirectory() as tmp:
    source, output = Path(tmp)/'synthetic.psk', Path(tmp)/'rig.bin'
    source.write_bytes(raw)
    module.prepare(source, output)
    data = output.read_bytes()
    assert struct.unpack_from('<8sIII', data) == (b'DVRIK002',3,3,1)
    for i, want in enumerate(expected):
        bone = struct.unpack_from('<64si3f', data, 20+80*i)
        vertex = struct.unpack_from('<3f4i4f', data, 20+80*3+44*i)
        assert max(abs(a-b) for a,b in zip(bone[2:], want)) < .00001, (bone, want)
        assert vertex[:3] == want
        assert vertex[3] == i and vertex[7] == 1
    assert source.read_bytes() == raw
print('PASS: independent engine-space joint and vertex expectations, nested rotation, weights, version, source preservation')
