"""Prepare local IK reference data from a locally extracted ActorX PSK.

No game assets are distributed by this tool. Its binary output is game-derived; the
one shipped copy lives in assets/vr/ (the owner's decision, 2026-10-05; see CLAUDE.md)
and is embedded in the proxy. Usage: python prepare-arm-rig.py mesh.psk output.bin
"""
import argparse
import math
import struct
from pathlib import Path


def multiply(a, b):
    x, y, z, w = a
    X, Y, Z, W = b
    return (w*X+x*W+y*Z-z*Y, w*Y-x*Z+y*W+z*X,
            w*Z+x*Y-y*X+z*W, w*W-x*X-y*Y-z*Z)


def rotate(q, v):
    return multiply(multiply(q, (*v, 0)), (-q[0], -q[1], -q[2], q[3]))[:3]


def read_psk(path):
    data = path.read_bytes()
    offset, chunks = 0, {}
    while offset < len(data):
        if offset + 32 > len(data):
            raise ValueError('Truncated PSK chunk header')
        name, _, size, count = struct.unpack_from('<20siii', data, offset)
        offset += 32
        if size < 0 or count < 0 or offset + size * count > len(data):
            raise ValueError('Invalid PSK chunk size')
        chunks[name.rstrip(b'\0')] = (size, count, data[offset:offset+size*count])
        offset += size * count

    def records(name, fmt):
        size, count, raw = chunks[name]
        if size != struct.calcsize(fmt):
            raise ValueError(f'Unsupported {name!r} stride {size}')
        return list(struct.iter_unpack(fmt, raw))

    points = records(b'PNTS0000', '<3f')
    bones = records(b'REFSKELT', '<64s3i11f')
    weights = records(b'RAWWEIGHTS', '<fii')
    if not 1 <= len(bones) <= 128 or not 1 <= len(points) <= 8192:
        raise ValueError('Rig exceeds bounded runtime limits')
    poses, visiting = {}, set()

    def pose(i):
        if i in poses:
            return poses[i]
        if i in visiting:
            raise ValueError('Cyclic skeleton')
        visiting.add(i)
        b = bones[i]
        parent, q, p = b[3], b[4:8], b[8:11]
        norm = math.sqrt(sum(v*v for v in q))
        if not .99 < norm < 1.01:
            raise ValueError('Invalid reference quaternion')
        q = tuple(v/norm for v in q)
        if i == 0:
            parent = -1
        else:
            if not 0 <= parent < len(bones) or parent == i:
                raise ValueError('Invalid bone parent')
            pq, pp = pose(parent)
            p = tuple(a+b for a, b in zip(pp, rotate(pq, p)))
            q = multiply(pq, (-q[0], -q[1], -q[2], q[3]))
        visiting.remove(i)
        poses[i] = q, p
        return q, p

    influences = [[] for _ in points]
    for weight, vertex, bone in weights:
        if not 0 <= vertex < len(points) or not 0 <= bone < len(bones):
            raise ValueError('Invalid skin index')
        if not math.isfinite(weight) or weight < 0:
            raise ValueError('Invalid skin weight')
        if weight:
            influences[vertex].append((bone, weight))
    output_bones = []
    for i, bone in enumerate(bones):
        name = bone[0].split(b'\0', 1)[0]
        if not name or len(name) >= 64:
            raise ValueError('Invalid bone name')
        _, head = pose(i)
        output_bones.append((name, -1 if i == 0 else bone[3], *head))
    output_points = []
    for p, inf in zip(points, influences):
        if not all(math.isfinite(v) for v in p) or not 1 <= len(inf) <= 4:
            raise ValueError('Unsupported vertex or influence count')
        if abs(sum(w for _, w in inf)-1) > .001:
            raise ValueError('Unnormalized reference weights')
        inf += [(-1, 0)] * (4-len(inf))
        output_points.append((*p, *(b for b, _ in inf), *(w for _, w in inf)))
    faces = chunks.get(b'FACE0000', chunks.get(b'FACE3200'))
    if faces is None:
        raise ValueError('Missing face count')
    return output_bones, output_points, faces[1]


def prepare(source, destination):
    bones, points, triangles = read_psk(source)
    # UModel ExportPsk.cpp MIRROR_MESH reflects Y for both positions and the
    # reference skeleton. Undo that export boundary after composing joint heads.
    # DVRIK002 explicitly stores engine mesh coordinates, unlike version 1.
    bones = [(name, parent, x, -y, z) for name, parent, x, y, z in bones]
    points = [(p[0], -p[1], *p[2:]) for p in points]
    raw = bytearray(struct.pack('<8sIII', b'DVRIK002', len(bones), len(points), triangles))
    for bone in bones:
        raw.extend(struct.pack('<64si3f', *bone))
    for point in points:
        raw.extend(struct.pack('<3f4i4f', *point))
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(raw)
    print(f'Local IK reference: {len(bones)} bones, {len(points)} points, {triangles} triangles; {len(raw)} bytes')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    prepare(args.source, args.destination)
