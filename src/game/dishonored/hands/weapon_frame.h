// Pure weapon placement operations, shared by the proxy and frame_test.
#pragma once
#include "hand_frame.h"
#include <float.h>

namespace dvr { namespace wf {
using namespace hf;

// Geometry ownership includes the range: buffers can contain several meshes.
// A shader is part of a pass, not evidence that two passes share a transform.
struct Geometry {
    const void *vb, *ib;
    unsigned stride, offset, type, minIndex, start, vertices, primitives;
    int base;
};
static inline bool same_geometry(const Geometry& a, const Geometry& b)
{
    return a.vb && a.ib && a.vb == b.vb && a.ib == b.ib &&
        a.stride == b.stride && a.offset == b.offset && a.type == b.type &&
        a.minIndex == b.minIndex && a.start == b.start && a.vertices == b.vertices &&
        a.primitives == b.primitives && a.base == b.base;
}

static inline bool same_view(unsigned sourcePresent, int sourceEye,
                             unsigned present, int eye)
{
    return (eye == -1 || eye == 1) && sourceEye == eye && sourcePresent == present;
}

static inline bool color_view(const void* handTarget, const void* drawTarget,
                              bool sameViewport, unsigned colorMask)
{
    return handTarget && handTarget == drawTarget && sameViewport && colorMask != 0;
}

static inline bool inverse(const Xform& a, Xform* out)
{
    const float* m = a.r.m;
    const float d = det3(a.r);
    if (!_finite(d) || fabsf(d) < 1e-8f) return false;
    Mat3 r = {{m[4]*m[8]-m[5]*m[7], m[2]*m[7]-m[1]*m[8], m[1]*m[5]-m[2]*m[4],
               m[5]*m[6]-m[3]*m[8], m[0]*m[8]-m[2]*m[6], m[2]*m[3]-m[0]*m[5],
               m[3]*m[7]-m[4]*m[6], m[1]*m[6]-m[0]*m[7], m[0]*m[4]-m[1]*m[3]}};
    for (int i = 0; i < 9; ++i) {
        r.m[i] /= d;
        if (!_finite(r.m[i])) return false;
    }
    out->r = r;
    mulv3(r, a.t, out->t);
    for (int i = 0; i < 3; ++i) {
        out->t[i] = -out->t[i];
        if (!_finite(out->t[i])) return false;
    }
    return true;
}

static inline bool bridge(const Xform& nativeRef, const Xform& drawRef, Xform* k)
{
    Xform inv;
    if (!inverse(nativeRef, &inv)) return false;
    *k = xform_mul(drawRef, inv);
    return true;
}

struct Candidate { Xform predicted; int hand, assembly; };
struct Result { int best; bool ambiguous; float angle, position, scale, score; };

// Compare every eligible member across BOTH hands. A tied assembly may share
// a correction, but equal transforms in different hands must refuse even at 0.
static inline Result match(const Xform& draw, const Candidate* c, int n,
                           float angleTol, float posTol, float margin)
{
    Result out = {-1, false, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX};
    Result nearest = out;
    float scores[64];
    if (n > 64 || !(angleTol > 0) || !(posTol > 0)) return out;
    for (int i = 0; i < n; ++i) {
        scores[i] = FLT_MAX;
        Mat3 a = draw.r, b = c[i].predicted.r;
        float scale = 0, pos = 0;
        bool valid = true;
        for (int j = 0; j < 3; ++j) {
            float an = 0, bn = 0;
            for (int r = 0; r < 3; ++r) {
                an += a.m[r*3+j]*a.m[r*3+j];
                bn += b.m[r*3+j]*b.m[r*3+j];
            }
            an = sqrtf(an); bn = sqrtf(bn);
            if (!(an > 1e-5f && bn > 1e-5f)) { valid = false; break; }
            const float ds = fabsf(an-bn) / an;
            if (ds > scale) scale = ds;
            for (int r = 0; r < 3; ++r) { a.m[r*3+j] /= an; b.m[r*3+j] /= bn; }
            const float dp = draw.t[j] - c[i].predicted.t[j];
            pos += dp*dp;
        }
        if (!valid || !basis_is_orthonormal(a, .02f) ||
            !basis_is_orthonormal(b, .02f) || det3(a)*det3(b) < 0) continue;
        const float angle = rotation_diff_deg(a,b);
        pos = sqrtf(pos);
        const float score = angle/angleTol + pos/posTol + scale/.005f;
        if (!_finite(score)) continue;
        if (score < nearest.score) nearest = {i, false, angle, pos, scale, score};
        if (angle <= angleTol && pos <= posTol && scale <= .005f) {
            scores[i] = score;
            if (score < out.score) out = {i, false, angle, pos, scale, score};
        }
    }
    if (out.best < 0) { nearest.best = -1; return nearest; }
    for (int i = 0; i < n; ++i) {
        if (i == out.best || scores[i] == FLT_MAX) continue;
        if (scores[i] <= out.score*margin + 1e-4f &&
            (c[i].hand != c[out.best].hand || !c[i].assembly ||
             c[i].assembly != c[out.best].assembly)) out.ambiguous = true;
    }
    return out;
}

static inline bool palette_range(int start, int count, int vp, int local)
{
    if (start < 0 || count <= 0 || count % 3 || start > 256-count) return false;
    return !(vp >= 0 && start < vp+4 && vp < start+count) &&
           !(start < local+4 && local < start+count);
}
} }
