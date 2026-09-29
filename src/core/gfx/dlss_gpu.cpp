// core/gfx/dlss_gpu.cpp - see dlss_gpu.h. No mod dependencies.
#include "core/gfx/dlss_gpu.h"

#include <windows.h>
#include <d3d11.h>
#include <d3dcommon.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace dvr::dlss {
namespace {

typedef HRESULT (WINAPI *PFN_D3DCompile)(LPCVOID, SIZE_T, LPCSTR, const void*, void*, LPCSTR,
                                         LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);
const UINT kOptimizationLevel3 = 1u << 15;
const int kAuditGrid = 64;

// The reprojection is clarity_gpu's Reproject, minus its screen-bounds rejection.
const char* kSrc =
    "struct VSOut { float4 pos : SV_Position; };\n"
    "VSOut vsmain(uint id : SV_VertexID) {\n"
    "    VSOut o;\n"
    "    float2 uv = float2((id << 1) & 2, id & 2);\n"
    "    o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);\n"
    "    return o;\n"
    "}\n"
    "cbuffer CB : register(b0) {\n"
    "    float4 gRowF;   // prev-from-cur row 0\n"
    "    float4 gRowR;   // row 1, tanH\n"
    "    float4 gRowU;   // row 2, tanV\n"
    "    float4 gT;      // translation (previous axes, uu), depth scale\n"
    "    float4 gSize;   // w, h, depth w, depth h\n"
    "    float4 gFlags;  // prev tanH, prev tanV, use depth, history valid\n"
    "    float4 gMask;   // lo, hi, previous colour valid, audit grid\n"
    "    float4 gBody;   // body depth (depth units; nearer = arms/weapon: rotation only), use VP\n"
    "    float4 gCa, gCb, gCw;   // current clip x, y, w as linear forms of the camera-relative point (xyz) + constant (w)\n"
    "    float4 gPa, gPb, gPw;   // the previous image's\n"
    "    float4 gD;              // current minus previous camera, world uu\n"
    "};\n"
    "Texture2D tDepth : register(t0);\n"
    "struct PSOut { float depth : SV_Target0; float2 mv : SV_Target1; };\n"
    "PSOut psmain(VSOut i) {\n"
    "    PSOut o;\n"
    "    float2 uv = i.pos.xy / gSize.xy;\n"
    "    float z = 0;\n"
    "    if (gFlags.z > 0.5) {\n"
    "        z = tDepth.Load(int3(clamp(int2(uv * gSize.zw), int2(0,0), int2(gSize.zw)-1), 0)).a;\n"
    "        if (!(z > 0) || !isfinite(z)) z = 0;\n"
    "    }\n"
    "    bool scene = z > 0 && z < 1000;\n"
    "    o.depth = scene ? 1.0 / (1.0 + z) : 0.0;\n"
    "    o.mv = 0;\n"
    "    if (gFlags.w < 0.5) return o;\n"
    "    if (gBody.y > 0.5) {\n"
    "        // Solve the camera-relative point from this pixel's NDC and its view depth w, through\n"
    "        // the matrix the game drew with, then project it through the previous image's.\n"
    "        float nx = uv.x * 2 - 1, ny = 1 - uv.y * 2;\n"
    "        float w = scene ? z * gT.w : 1.0e6;\n"
    "        float3 r0 = gCa.xyz - nx * gCw.xyz, r1 = gCb.xyz - ny * gCw.xyz, r2 = gCw.xyz;\n"
    "        float3 rhs = float3(nx * gCw.w - gCa.w, ny * gCw.w - gCb.w, w - gCw.w);\n"
    "        float det = dot(r0, cross(r1, r2));\n"
    "        if (abs(det) < 1e-12) return o;\n"
    "        float3 P = float3(dot(rhs, float3(cross(r1, r2).x, cross(r2, r0).x, cross(r0, r1).x)),\n"
    "                          dot(rhs, float3(cross(r1, r2).y, cross(r2, r0).y, cross(r0, r1).y)),\n"
    "                          dot(rhs, float3(cross(r1, r2).z, cross(r2, r0).z, cross(r0, r1).z))) / det;\n"
    "        if (scene && z >= gBody.x) P += gD.xyz;\n"
    "        float pw = dot(gPw.xyz, P) + gPw.w;\n"
    "        if (!(pw > 1e-3) || !all(isfinite(P))) return o;\n"
    "        float px = (dot(gPa.xyz, P) + gPa.w) / pw, py = (dot(gPb.xyz, P) + gPb.w) / pw;\n"
    "        o.mv = float2(0.5 + 0.5 * px, 0.5 - 0.5 * py) - uv;\n"
    "        return o;\n"
    "    }\n"
    "    float3 ray = float3(1, (uv.x*2-1)*gRowR.w, (1-uv.y*2)*gRowU.w);\n"
    "    float3 q = float3(dot(gRowF.xyz,ray), dot(gRowR.xyz,ray), dot(gRowU.xyz,ray));\n"
    "    if (scene && z >= gBody.x) q += gT.xyz / (z*gT.w);\n"
    "    if (!(q.x > 1e-4) || !all(isfinite(q))) return o;\n"
    "    float2 puv = float2(0.5+0.5*q.y/(q.x*gFlags.x), 0.5-0.5*q.z/(q.x*gFlags.y));\n"
    "    o.mv = puv - uv;\n"
    "    return o;\n"
    "}\n"
    // The mask and the audit: t0 current colour, t1 previous colour, t2 motion, t3 guide
    // depth, t4 the mask itself (audit only).
    "Texture2D tCur : register(t0);\n"
    "Texture2D tPrev : register(t1);\n"
    "Texture2D tMv : register(t2);\n"
    "Texture2D tGd : register(t3);\n"
    "Texture2D tBias : register(t4);\n"
    "SamplerState sLinear : register(s0);\n"
    "float Luma(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }\n"
    "float MaskAt(int2 p) {\n"
    "    if (gMask.z < 0.5) return 0;\n"
    "    int2 last = int2(gSize.xy) - 1;\n"
    "    float3 c = tCur.Load(int3(p, 0)).rgb, mn = c, mx = c;\n"
    "    [unroll] for (int dy = -1; dy <= 1; ++dy)\n"
    "    [unroll] for (int dx = -1; dx <= 1; ++dx) {\n"
    "        float3 s = tCur.Load(int3(clamp(p + int2(dx, dy), int2(0,0), last), 0)).rgb;\n"
    "        mn = min(mn, s); mx = max(mx, s);\n"
    "    }\n"
    "    float2 uv = (float2(p) + 0.5) / gSize.xy;\n"
    "    float2 puv = uv + tMv.Load(int3(p, 0)).xy;\n"
    "    if (any(puv < 0) || any(puv > 1)) return 0;\n"
    "    float3 h = tPrev.SampleLevel(sLinear, puv, 0).rgb;\n"
    "    float3 ex = max(mn - h, h - mx);\n"
    "    return smoothstep(gMask.x, gMask.y, max(ex.r, max(ex.g, ex.b)));\n"
    "}\n"
    // VR-39: the player's arms and weapon (the foreground pass's texels: scene depth changed after it began, t5
    // against t6) are always "trust the current colour" - the vectors are the camera's only, so their history is
    // wrong whenever the hands move. gBody.z: the foreground part on; gBody.w: the colour part on.
    "Texture2D tScene : register(t5);\n"
    "Texture2D tPreFg : register(t6);\n"
    "float ps_mask(VSOut i) : SV_Target {\n"
    "    float m = gBody.w > 0.5 ? MaskAt(int2(i.pos.xy)) : 0.0;\n"
    "    if (gBody.z > 0.5) {\n"
    "        int2 dp = clamp(int2(i.pos.xy / gSize.xy * gSize.zw), int2(0, 0), int2(gSize.zw) - 1);\n"
    "        float a = tScene.Load(int3(dp, 0)).a, p = tPreFg.Load(int3(dp, 0)).a;\n"
    "        if (a > 0.0 && abs(a - p) > 1e-4 * max(a, 1e-3)) m = 1.0;\n"
    "    }\n"
    "    return m;\n"
    "}\n"
    "float4 ps_audit(VSOut i) : SV_Target {\n"
    "    int2 p = int2((floor(i.pos.xy) + 0.5) / gMask.w * gSize.xy);\n"
    "    float2 uv = (float2(p) + 0.5) / gSize.xy;\n"
    "    float2 puv = uv + tMv.Load(int3(p, 0)).xy;\n"
    "    float lc = Luma(tCur.Load(int3(p, 0)).rgb);\n"
    "    float d = tGd.Load(int3(p, 0)).r;\n"
    "    float z = d > 0 ? 1.0 / d - 1.0 : -1.0;\n"
    "    float zeroErr = abs(lc - Luma(tPrev.Load(int3(p, 0)).rgb));\n"
    "    float vecErr = (any(puv < 0) || any(puv > 1)) ? -1.0 : abs(lc - Luma(tPrev.SampleLevel(sLinear, puv, 0).rgb));\n"
    "    return float4(vecErr, zeroErr, z, tBias.Load(int3(p, 0)).r);\n"
    "}\n"
    "float LumaCur(int2 q) { return Luma(tCur.Load(int3(clamp(q, int2(0,0), int2(gSize.xy)-1), 0)).rgb); }\n"
    "float LumaPrev(float2 px) { return Luma(tPrev.SampleLevel(sLinear, px / gSize.xy, 0).rgb); }\n"
    "struct FlowOut { float4 a : SV_Target0; float4 b : SV_Target1; };\n"
    "FlowOut ps_flow(VSOut i) {\n"
    "    FlowOut o; o.a = 0; o.b = 0;\n"
    "    int2 p = int2((floor(i.pos.xy) + 0.5) / gMask.w * gSize.xy);\n"
    "    float2 mv = tMv.Load(int3(p, 0)).xy * gSize.xy;\n"
    "    float2 pred = float2(p) + 0.5 + mv;\n"
    "    float d = tGd.Load(int3(p, 0)).r;\n"
    "    o.b.y = d > 0 ? 1.0 / d - 1.0 : -1.0;\n"
    "    if (gMask.z < 0.5 || any(pred < 8) || any(pred > gSize.xy - 8)) return o;\n"
    "    float c[25]; float cmin = 1, cmax = 0;\n"
    "    [unroll] for (int k = 0; k < 25; ++k) { c[k] = LumaCur(p + int2(k % 5 - 2, k / 5 - 2)); cmin = min(cmin, c[k]); cmax = max(cmax, c[k]); }\n"
    "    float sad[81]; float best = 1e9, total = 0; int bi = 40;\n"
    "    [loop] for (int s = 0; s < 81; ++s) {\n"
    "        float2 off = float2(s % 9 - 4, s / 9 - 4);\n"
    "        float acc = 0;\n"
    "        [unroll] for (int k = 0; k < 25; ++k) acc += abs(c[k] - LumaPrev(pred + off + float2(k % 5 - 2, k / 5 - 2)));\n"
    "        sad[s] = acc / 25; total += sad[s];\n"
    "        if (sad[s] < best) { best = sad[s]; bi = s; }\n"
    "    }\n"
    "    int bx = bi % 9, by = bi / 9;\n"
    "    float2 e = float2(bx - 4, by - 4);\n"
    "    if (bx > 0 && bx < 8) { float l = sad[bi-1], r = sad[bi+1], den = l - 2*best + r; if (den > 1e-6) e.x += 0.5 * (l - r) / den; }\n"
    "    if (by > 0 && by < 8) { float u = sad[bi-9], w = sad[bi+9], den = u - 2*best + w; if (den > 1e-6) e.y += 0.5 * (u - w) / den; }\n"
    "    float mean = total / 81;\n"
    "    bool conf = cmax - cmin > 0.06 && best < 0.5 * mean && bx > 0 && bx < 8 && by > 0 && by < 8;\n"
    "    o.a = float4(e, mv);\n"
    "    o.b.x = conf ? 1 : 0; o.b.z = best; o.b.w = mean;\n"
    "    return o;\n"
    "}\n"
    // VR-39: OBJECT MOTION. The vectors above are the camera's: right for the static world, wrong for anything
    // that moves on its own (characters) or with the camera (a boat or carriage the player rides). Per 8x8 tile of
    // this eye's image, a block match against the eye's previous image picks the best of: the camera vector, zero
    // screen motion (riding along), the camera's rotation alone (a vehicle turning), last frame's tile vector and
    // its four neighbours, then a coarse search around the best and a fine one. A tile whose best beats the camera
    // vector clearly is marked; then per pixel, the camera vector or a marked neighbour tile's vector, whichever
    // matches a 3x3 patch best (the camera wins ties, so the static world keeps its exact vectors). Vectors are
    // displacements current -> previous in render pixels, jitter-free: the previous image is sampled at
    // + gOm.zw, the jitter's own shift (current minus previous sample offset, as the flow check subtracts it).
    // t0 current colour, t1 previous colour, t2 camera motion (uv), t3 guide depth, t4 previous tiles / tiles.
    "cbuffer OM : register(b1) { float4 gOm; float4 gOm2; };\n"   // tiles w, h, jitter shift x, y | ratio, min gain, min contrast, temporal
    "Texture2D tTiles : register(t4);\n"
    "float BlockSad(int2 o, float2 v, int stride) {\n"
    // A block whose match would lie outside the previous image cannot be compared: never a winner.
    "    float2 a = float2(o) + v + gOm.zw;\n"
    "    if (any(a < 1.0) || any(a + 9.0 > gSize.xy)) return 1e9;\n"
    "    float acc = 0; float n = 0;\n"
    "    [loop] for (int y = 0; y < 8; y += stride)\n"
    "    [loop] for (int x = 0; x < 8; x += stride) {\n"
    "        int2 q = o + int2(x, y);\n"
    "        acc += abs(LumaCur(q) - LumaPrev(float2(q) + 0.5 + v + gOm.zw)); n += 1;\n"
    "    }\n"
    "    return acc / n;\n"
    "}\n"
    // The camera's vector at a pixel without its translation: what a point riding with the camera does.
    "float2 RotOnlyPx(float2 px) {\n"
    "    float2 uv = px / gSize.xy;\n"
    "    if (gBody.y > 0.5) {\n"
    "        float nx = uv.x * 2 - 1, ny = 1 - uv.y * 2;\n"
    "        float d = tGd.Load(int3(clamp(int2(px), int2(0,0), int2(gSize.xy)-1), 0)).r;\n"
    "        float z = d > 0 ? 1.0 / d - 1.0 : 0.0;\n"
    "        float w = z > 0 ? z * gT.w : 1.0e6;\n"
    "        float3 r0 = gCa.xyz - nx * gCw.xyz, r1 = gCb.xyz - ny * gCw.xyz, r2 = gCw.xyz;\n"
    "        float3 rhs = float3(nx * gCw.w - gCa.w, ny * gCw.w - gCb.w, w - gCw.w);\n"
    "        float det = dot(r0, cross(r1, r2));\n"
    "        if (abs(det) < 1e-12) return float2(0, 0);\n"
    "        float3 P = float3(dot(rhs, float3(cross(r1, r2).x, cross(r2, r0).x, cross(r0, r1).x)),\n"
    "                          dot(rhs, float3(cross(r1, r2).y, cross(r2, r0).y, cross(r0, r1).y)),\n"
    "                          dot(rhs, float3(cross(r1, r2).z, cross(r2, r0).z, cross(r0, r1).z))) / det;\n"
    "        float pw = dot(gPw.xyz, P) + gPw.w;\n"
    "        if (!(pw > 1e-3)) return float2(0, 0);\n"
    "        float px2 = (dot(gPa.xyz, P) + gPa.w) / pw, py2 = (dot(gPb.xyz, P) + gPb.w) / pw;\n"
    "        return (float2(0.5 + 0.5 * px2, 0.5 - 0.5 * py2) - uv) * gSize.xy;\n"
    "    }\n"
    "    float3 ray = float3(1, (uv.x*2-1)*gRowR.w, (1-uv.y*2)*gRowU.w);\n"
    "    float3 q = float3(dot(gRowF.xyz,ray), dot(gRowR.xyz,ray), dot(gRowU.xyz,ray));\n"
    "    if (!(q.x > 1e-4)) return float2(0, 0);\n"
    "    return (float2(0.5+0.5*q.y/(q.x*gFlags.x), 0.5-0.5*q.z/(q.x*gFlags.y)) - uv) * gSize.xy;\n"
    "}\n"
    // Two dispatches. cs_objpre, one thread per tile: the contrast test and the camera match, and the exact early
    // exit (a tile needs camSad - best > min gain to be overridden, so a camera match within the min gain can never
    // lose); the tiles left are appended to a list. cs_objsearch, one 64-thread group per listed tile (an indirect
    // dispatch): each thread owns one pixel of the 8x8 block, a candidate's SAD is a group sum, and the coarse search
    // spreads its 162 positions over the threads. The search has no branch around a barrier (the compiler refuses
    // one, and a group must never diverge there), so every candidate is evaluated and kept or dropped by value.
    // Measured on the host: one thread per tile doing the whole search serially was latency-bound, 1.7 ms per eye
    // image with one moving character; with no early exit at all, 18 ms.
    "RWTexture2D<float4> uTiles : register(u0);\n"
    "AppendStructuredBuffer<uint> uList : register(u1);\n"
    "StructuredBuffer<uint> tList : register(t5);\n"
    "[numthreads(8, 8, 1)]\n"
    "void cs_objpre(uint3 id : SV_DispatchThreadID) {\n"
    "    int2 t = int2(id.xy);\n"
    "    if (any(t >= int2(gOm.xy))) return;\n"
    "    int2 o = t * 8;\n"
    "    int2 ci = min(o + 4, int2(gSize.xy) - 1);\n"
    "    float2 cam = tMv.Load(int3(ci, 0)).xy * gSize.xy;\n"
    "    if (any(o + 8 > int2(gSize.xy))) { uTiles[t] = float4(cam, 0, 0); return; }\n"
    "    float mn = 1, mx = 0;\n"
    "    [unroll] for (int k = 0; k < 16; ++k) { float l = LumaCur(o + int2(k % 4, k / 4) * 2); mn = min(mn, l); mx = max(mx, l); }\n"
    "    if (mx - mn < gOm2.z) { uTiles[t] = float4(cam, 0, 0); return; }\n"
    "    float camSad = BlockSad(o, cam, 1);\n"
    // Outside the previous image (the camera says it came from off-screen), or already matched: final.
    "    uTiles[t] = float4(cam, 0, camSad);\n"
    "    if (camSad > 1e8 || camSad <= gOm2.y) return;\n"
    "    uList.Append(uint(t.y) * uint(gOm.x) + uint(t.x));\n"
    "}\n"
    "groupshared float gsRed[64];\n"
    "groupshared uint gsIdx[64];\n"
    "float GroupSum(float v, uint gi) {\n"
    "    GroupMemoryBarrierWithGroupSync();\n"
    "    gsRed[gi] = v;\n"
    "    GroupMemoryBarrierWithGroupSync();\n"
    "    [unroll] for (uint s = 32; s > 0; s >>= 1) { if (gi < s) gsRed[gi] += gsRed[gi + s]; GroupMemoryBarrierWithGroupSync(); }\n"
    "    return gsRed[0];\n"
    "}\n"
    "bool Inside(int2 o, float2 v) { float2 a = float2(o) + v + gOm.zw; return all(a >= 1.0) && all(a + 9.0 <= gSize.xy); }\n"
    // The whole block's mean |difference| at displacement v (the same for every thread; 1e9 outside the image).
    "float Eval(int2 o, int2 q, float lc, float2 v, uint gi) {\n"
    "    bool ok = Inside(o, v);\n"
    "    float d = ok ? abs(lc - LumaPrev(float2(q) + 0.5 + v + gOm.zw)) : 0.0;\n"
    "    float s = GroupSum(d, gi) / 64.0;\n"
    "    return ok ? s : 1e9;\n"
    "}\n"
    "void TryG(int2 o, int2 q, float lc, float2 v, uint gi, inout float2 best, inout float bs) {\n"
    "    float s = Eval(o, q, lc, v, gi); if (s < bs) { bs = s; best = v; }\n"
    "}\n"
    "[numthreads(64, 1, 1)]\n"
    "void cs_objsearch(uint3 gid : SV_GroupID, uint gi : SV_GroupIndex) {\n"
    "    uint ti = tList[gid.x];\n"
    "    int2 t = int2(ti % uint(gOm.x), ti / uint(gOm.x)), o = t * 8;\n"
    "    int2 q = o + int2(gi % 8, gi / 8);\n"
    "    float2 c = float2(o) + 4.0;\n"
    "    float2 cam = tMv.Load(int3(min(o + 4, int2(gSize.xy) - 1), 0)).xy * gSize.xy;\n"
    "    float lc = LumaCur(q);\n"
    "    float camSad = Eval(o, q, lc, cam, gi);\n"
    "    float2 best = cam; float bs = camSad;\n"
    "    TryG(o, q, lc, float2(0, 0), gi, best, bs);\n"
    "    TryG(o, q, lc, RotOnlyPx(c), gi, best, bs);\n"
    "    int2 lim = int2(gOm.xy) - 1;\n"
    "    [unroll] for (int k = 0; k < 5; ++k) {\n"
    "        int2 n = clamp(t + (k == 0 ? int2(0,0) : k == 1 ? int2(1,0) : k == 2 ? int2(-1,0) : k == 3 ? int2(0,1) : int2(0,-1)), int2(0,0), lim);\n"
    "        float4 pv = tTiles.Load(int3(n, 0));\n"
    "        TryG(o, q, lc, (gOm2.w > 0.5 && pv.z > 0.5) ? pv.xy : cam, gi, best, bs);\n"
    "    }\n"
    // Coarse: a 9x9 grid of 2-pixel steps (+-8) around the best AND around zero motion (the best of wrong candidates
    // can sit far from the truth: the host test's character), each on a quarter of the block (4-pixel steps miss fine
    // texture: the test's 3-pixel features). 162 positions, up to three per thread, then the group's minimum.
    "    float ms = 1e9; uint mi = 0xffffffff;\n"
    "    [unroll] for (uint k2 = 0; k2 < 3; ++k2) {\n"
    "        uint idx = gi + 64 * k2;\n"
    "        uint g = idx % 81;\n"
    "        float2 v = (idx < 81 ? best : float2(0, 0)) + float2(int(g % 9) - 4, int(g / 9) - 4) * 2.0;\n"
    "        float acc = 0;\n"
    "        [unroll] for (int j = 0; j < 16; ++j) {\n"
    "            int2 p2 = o + int2(j % 4, j / 4) * 2;\n"
    "            acc += abs(LumaCur(p2) - LumaPrev(float2(p2) + 0.5 + v + gOm.zw));\n"
    "        }\n"
    "        bool use = idx < 162 && Inside(o, v);\n"
    "        if (use && acc < ms) { ms = acc; mi = idx; }\n"
    "    }\n"
    "    GroupMemoryBarrierWithGroupSync();\n"
    "    gsRed[gi] = ms; gsIdx[gi] = mi;\n"
    "    GroupMemoryBarrierWithGroupSync();\n"
    "    [unroll] for (uint s = 32; s > 0; s >>= 1) {\n"
    "        if (gi < s && gsRed[gi + s] < gsRed[gi]) { gsRed[gi] = gsRed[gi + s]; gsIdx[gi] = gsIdx[gi + s]; }\n"
    "        GroupMemoryBarrierWithGroupSync();\n"
    "    }\n"
    "    uint wi = gsIdx[0];\n"
    "    uint wg = wi % 81;\n"
    "    float2 cv = wi == 0xffffffff ? best : (wi < 81 ? best : float2(0, 0)) + float2(int(wg % 9) - 4, int(wg / 9) - 4) * 2.0;\n"
    "    TryG(o, q, lc, cv, gi, best, bs);\n"
    // Fine: two rounds of +-1 on the whole block.
    "    [unroll] for (int r = 0; r < 2; ++r) {\n"
    "        float2 b0 = best;\n"
    "        [unroll] for (int s2 = 0; s2 < 9; ++s2) { if (s2 == 4) continue; TryG(o, q, lc, b0 + float2(s2 % 3 - 1, s2 / 3 - 1), gi, best, bs); }\n"
    "    }\n"
    // Sub-pixel: a parabola through the SAD at -1, 0, +1 per axis (clamped to half a pixel).
    "    float2 b1 = best;\n"
    "    float el = Eval(o, q, lc, b1 - float2(1, 0), gi), er = Eval(o, q, lc, b1 + float2(1, 0), gi);\n"
    "    float eu = Eval(o, q, lc, b1 - float2(0, 1), gi), ed = Eval(o, q, lc, b1 + float2(0, 1), gi);\n"
    "    float dx = el - 2 * bs + er, dy = eu - 2 * bs + ed;\n"
    "    float2 f = float2(dx > 1e-6 && el < 1e8 && er < 1e8 ? clamp(0.5 * (el - er) / dx, -0.5, 0.5) : 0.0,\n"
    "                      dy > 1e-6 && eu < 1e8 && ed < 1e8 ? clamp(0.5 * (eu - ed) / dy, -0.5, 0.5) : 0.0);\n"
    "    TryG(o, q, lc, b1 + f, gi, best, bs);\n"
    "    bool win = bs < camSad * gOm2.x && camSad - bs > gOm2.y;\n"
    "    if (gi == 0) uTiles[t] = float4(win ? best : cam, win ? 1 : 0, bs);\n"
    "}\n"
    "float PatchSad(float c[9], int2 p, float2 v) {\n"
    "    float acc = 0;\n"
    "    [unroll] for (int k = 0; k < 9; ++k) acc += abs(c[k] - LumaPrev(float2(p + int2(k % 3 - 1, k / 3 - 1)) + 0.5 + v + gOm.zw));\n"
    "    return acc;\n"
    "}\n"
    // Nearly every pixel has no marked tile around it: that answer costs nine tile loads and nothing else. The loop
    // below is unrolled so the tile array stays in registers.
    "float2 ps_objfix(VSOut i) : SV_Target {\n"
    "    int2 p = int2(i.pos.xy);\n"
    "    float2 cam = tMv.Load(int3(p, 0)).xy;\n"
    "    int2 t = p / 8, lim = int2(gOm.xy) - 1;\n"
    "    float4 tv[9]; bool any = false;\n"
    "    [unroll] for (int k = 0; k < 9; ++k) {\n"
    "        tv[k] = tTiles.Load(int3(clamp(t + int2(k % 3 - 1, k / 3 - 1), int2(0,0), lim), 0));\n"
    "        any = any || tv[k].z > 0.5;\n"
    "    }\n"
    "    [branch] if (!any) return cam;\n"
    "    float c[9];\n"
    "    [unroll] for (int j = 0; j < 9; ++j) c[j] = LumaCur(p + int2(j % 3 - 1, j / 3 - 1));\n"
    "    float2 camPx = cam * gSize.xy, best = camPx;\n"
    "    float bs = PatchSad(c, p, camPx) * 0.7 + 1e-3;\n"
    "    [unroll] for (int k2 = 0; k2 < 9; ++k2) {\n"
    "        [branch] if (tv[k2].z > 0.5) { float s = PatchSad(c, p, tv[k2].xy); if (s < bs) { bs = s; best = tv[k2].xy; } }\n"
    "    }\n"
    "    return best / gSize.xy;\n"
    "}\n";

void say(char* why, size_t cap, const char* fmt, ...) {
    if (!why || !cap) return;
    va_list a; va_start(a, fmt);
    _vsnprintf_s(why, cap, _TRUNCATE, fmt, a);
    va_end(a);
}
template <class T> void rel(T*& p) { if (p) { p->Release(); p = nullptr; } }

float g_cb[60] = {};

} // namespace

bool GuideGpu::init(ID3D11Device* dev, char* why, size_t cap) {
    if (ready_) return true;
    struct CompilerModule { HMODULE h; ~CompilerModule() { if (h) FreeLibrary(h); } } module{LoadLibraryA("d3dcompiler_47.dll")};
    PFN_D3DCompile compile = module.h ? (PFN_D3DCompile)GetProcAddress(module.h, "D3DCompile") : nullptr;
    if (!compile) { say(why, cap, "d3dcompiler_47.dll missing"); return false; }
    auto build = [&](const char* entry, const char* target, void** out, bool vs) -> bool {
        ID3DBlob *blob = nullptr, *err = nullptr;
        if (FAILED(compile(kSrc, strlen(kSrc), "dlss_guides", nullptr, nullptr, entry, target, kOptimizationLevel3, 0, &blob, &err))) {
            say(why, cap, "%s compile failed: %s", entry, err ? (const char*)err->GetBufferPointer() : "?");
            rel(err);
            return false;
        }
        rel(err);
        const HRESULT hr = vs ? dev->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, (ID3D11VertexShader**)out)
                              : dev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, (ID3D11PixelShader**)out);
        blob->Release();
        if (FAILED(hr)) { say(why, cap, "%s create failed 0x%08lX", entry, (unsigned long)hr); return false; }
        return true;
    };
    auto build_cs = [&](const char* entry, ID3D11ComputeShader** out) -> bool {
        ID3DBlob *blob = nullptr, *err = nullptr;
        if (FAILED(compile(kSrc, strlen(kSrc), "dlss_guides", nullptr, nullptr, entry, "cs_5_0", kOptimizationLevel3, 0, &blob, &err))) {
            say(why, cap, "%s compile failed: %s", entry, err ? (const char*)err->GetBufferPointer() : "?");
            rel(err);
            return false;
        }
        rel(err);
        const HRESULT hr = dev->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, out);
        blob->Release();
        if (FAILED(hr)) { say(why, cap, "%s create failed 0x%08lX", entry, (unsigned long)hr); return false; }
        return true;
    };
    if (!build("vsmain", "vs_4_0", (void**)&vs_, true) || !build("psmain", "ps_4_0", (void**)&ps_, false) ||
        !build("ps_mask", "ps_4_0", (void**)&psMask_, false) || !build("ps_audit", "ps_4_0", (void**)&psAudit_, false) ||
        !build("ps_flow", "ps_5_0", (void**)&psFlow_, false) || !build_cs("cs_objpre", &csObjPre_) || !build_cs("cs_objsearch", &csObjTile_) ||
        !build("ps_objfix", "ps_5_0", (void**)&psObjFix_, false)) {
        shutdown(); return false;
    }
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(g_cb); bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    dev->CreateBuffer(&bd, nullptr, &cb_);
    bd.ByteWidth = 32;
    dev->CreateBuffer(&bd, nullptr, &cbObj_);
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
    dev->CreateRasterizerState(&rd, &raster_);
    D3D11_BLEND_DESC bld = {};
    bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    bld.RenderTarget[1].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    bld.IndependentBlendEnable = TRUE;
    dev->CreateBlendState(&bld, &blend_);
    D3D11_DEPTH_STENCIL_DESC dd = {};
    dd.DepthEnable = FALSE;
    dev->CreateDepthStencilState(&dd, &ds_);
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    dev->CreateSamplerState(&sd, &linear_);
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = td.Height = kAuditGrid; td.MipLevels = 1; td.ArraySize = 1; td.SampleDesc.Count = 1;
    td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
    dev->CreateTexture2D(&td, nullptr, &auditTex_);
    if (auditTex_) dev->CreateRenderTargetView(auditTex_, nullptr, &auditRtv_);
    td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    for (auto& s : auditStage_) dev->CreateTexture2D(&td, nullptr, &s);
    for (int t = 0; t < 2; ++t) for (auto& s : flowStage_[t]) dev->CreateTexture2D(&td, nullptr, &s);
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET; td.CPUAccessFlags = 0;
    for (int t = 0; t < 2; ++t) {
        dev->CreateTexture2D(&td, nullptr, &flowTex_[t]);
        if (flowTex_[t]) dev->CreateRenderTargetView(flowTex_[t], nullptr, &flowRtv_[t]);
    }
    ready_ = cb_ && raster_ && blend_ && ds_ && linear_ && auditRtv_ && auditStage_[0] && auditStage_[1] &&
             flowRtv_[0] && flowRtv_[1] && flowStage_[0][0] && flowStage_[0][1] && flowStage_[1][0] && flowStage_[1][1];
    if (!ready_) { say(why, cap, "state objects failed"); shutdown(); }
    return ready_;
}

bool GuideGpu::ensure(ID3D11Device* dev, uint32_t w, uint32_t h, char* why, size_t cap) {
    if (depth_ && w_ == w && h_ == h) return true;
    rel(depthRtv_); rel(motionRtv_); rel(biasRtv_); rel(depthSrv_); rel(motionSrv_); rel(biasSrv_);
    rel(depth_); rel(motion_); rel(bias_);
    forget(0); forget(1);
    w_ = h_ = 0;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr = S_OK;
    auto make = [&](DXGI_FORMAT f, ID3D11Texture2D** t, ID3D11RenderTargetView** rtv, ID3D11ShaderResourceView** srv) {
        td.Format = f;
        if (SUCCEEDED(hr)) hr = dev->CreateTexture2D(&td, nullptr, t);
        if (SUCCEEDED(hr)) hr = dev->CreateRenderTargetView(*t, nullptr, rtv);
        if (SUCCEEDED(hr)) hr = dev->CreateShaderResourceView(*t, nullptr, srv);
    };
    make(DXGI_FORMAT_R32_FLOAT, &depth_, &depthRtv_, &depthSrv_);
    make(DXGI_FORMAT_R16G16_FLOAT, &motion_, &motionRtv_, &motionSrv_);
    make(DXGI_FORMAT_R8_UNORM, &bias_, &biasRtv_, &biasSrv_);
    if (FAILED(hr)) {
        say(why, cap, "guide targets %ux%u failed 0x%08lX", w, h, (unsigned long)hr);
        rel(depthRtv_); rel(motionRtv_); rel(biasRtv_); rel(depthSrv_); rel(motionSrv_); rel(biasSrv_);
        rel(depth_); rel(motion_); rel(bias_);
        return false;
    }
    w_ = w; h_ = h;
    return true;
}

static void full_screen(ID3D11DeviceContext* ctx, ID3D11RasterizerState* raster, ID3D11BlendState* blend,
                        ID3D11DepthStencilState* ds, ID3D11VertexShader* vs, ID3D11PixelShader* ps,
                        ID3D11Buffer* cb, uint32_t w, uint32_t h) {
    D3D11_VIEWPORT vp = {0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f};
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(raster);
    const float bf[4] = {0, 0, 0, 0};
    ctx->OMSetBlendState(blend, bf, 0xffffffff);
    ctx->OMSetDepthStencilState(ds, 0);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vs, nullptr, 0);
    ctx->PSSetShader(ps, nullptr, 0);
    ctx->PSSetConstantBuffers(0, 1, &cb);
}

bool GuideGpu::run(ID3D11Device* dev, ID3D11DeviceContext* ctx, const GuideParams& p, char* why, size_t cap) {
    if (!ready_ && !init(dev, why, cap)) return false;
    if (!p.w || !p.h) { say(why, cap, "zero size"); return false; }
    if (!ensure(dev, p.w, p.h, why, cap)) return false;
    const bool useDepth = p.sceneDepth && p.depthW && p.depthH && p.depthScale > 0 && isfinite(p.depthScale);
    float* cb = g_cb;
    for (int j = 0; j < 3; ++j) { cb[j] = p.prevFromCur.m[0][j]; cb[4 + j] = p.prevFromCur.m[1][j]; cb[8 + j] = p.prevFromCur.m[2][j]; }
    cb[3] = 0; cb[7] = p.tanH; cb[11] = p.tanV;
    for (int j = 0; j < 3; ++j) cb[12 + j] = p.translation[j];
    cb[15] = p.depthScale;
    cb[16] = (float)p.w; cb[17] = (float)p.h; cb[18] = (float)p.depthW; cb[19] = (float)p.depthH;
    cb[20] = p.prevTanH > 0 ? p.prevTanH : p.tanH; cb[21] = p.prevTanV > 0 ? p.prevTanV : p.tanV;
    cb[22] = useDepth ? 1.0f : 0.0f; cb[23] = p.historyValid ? 1.0f : 0.0f;
    cb[28] = p.bodyDepth > 0 ? p.bodyDepth : 0.0f;
    cb[29] = p.useVp ? 1.0f : 0.0f;
    if (p.useVp) {
        // Row-vector matrices: clip_i = sum_j P_j * M[j][i] + M[3][i]; the linear form of clip_i
        // is column i. x, y and w (columns 0, 1, 3), current then previous.
        const float* ms[2] = {p.vpCur, p.vpPrev};
        for (int k = 0; k < 2; ++k) {
            const int cols[3] = {0, 1, 3};
            for (int f = 0; f < 3; ++f)
                for (int j = 0; j < 4; ++j) cb[32 + k * 12 + f * 4 + j] = ms[k][j * 4 + cols[f]];
        }
        for (int j = 0; j < 3; ++j) cb[56 + j] = p.camDelta[j];
    }
    ctx->UpdateSubresource(cb_, 0, nullptr, cb, 0, 0);
    full_screen(ctx, raster_, blend_, ds_, vs_, ps_, cb_, p.w, p.h);
    ID3D11RenderTargetView* rtvs[2] = {depthRtv_, motionRtv_};
    ctx->OMSetRenderTargets(2, rtvs, nullptr);
    ID3D11ShaderResourceView* srv = useDepth ? p.sceneDepth : nullptr;
    ctx->PSSetShaderResources(0, 1, &srv);
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* none = nullptr;
    ctx->PSSetShaderResources(0, 1, &none);
    ID3D11RenderTargetView* noRt[2] = {nullptr, nullptr};
    ctx->OMSetRenderTargets(2, noRt, nullptr);
    return true;
}

bool GuideGpu::mask(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11ShaderResourceView* color,
                    bool historyValid, float lo, float hi, char* why, size_t cap, bool colourPart,
                    ID3D11ShaderResourceView* sceneDepth, ID3D11ShaderResourceView* preFg) {
    (void)dev;
    if (!ready_ || !bias_ || !color || (eye != 0 && eye != 1)) { say(why, cap, "mask: not ready"); return false; }
    const bool have = historyValid && prevOk_[eye] && prevSrv_[eye];
    g_cb[24] = lo; g_cb[25] = hi > lo ? hi : lo + 0.001f; g_cb[26] = have ? 1.0f : 0.0f; g_cb[27] = (float)kAuditGrid;
    g_cb[30] = (sceneDepth && preFg) ? 1.0f : 0.0f; g_cb[31] = colourPart ? 1.0f : 0.0f;
    ctx->UpdateSubresource(cb_, 0, nullptr, g_cb, 0, 0);
    full_screen(ctx, raster_, blend_, ds_, vs_, psMask_, cb_, w_, h_);
    ctx->OMSetRenderTargets(1, &biasRtv_, nullptr);
    ID3D11ShaderResourceView* srvs[7] = {color, have ? prevSrv_[eye] : nullptr, motionSrv_, nullptr, nullptr, sceneDepth, preFg};
    ctx->PSSetShaderResources(0, 7, srvs);
    ctx->PSSetSamplers(0, 1, &linear_);
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* none[7] = {};
    ctx->PSSetShaderResources(0, 7, none);
    ID3D11RenderTargetView* noRt = nullptr;
    ctx->OMSetRenderTargets(1, &noRt, nullptr);
    return true;
}

bool GuideGpu::keep(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11Texture2D* color) {
    if ((eye != 0 && eye != 1) || !color) return false;
    D3D11_TEXTURE2D_DESC cd = {};
    color->GetDesc(&cd);
    if (prev_[eye]) {
        D3D11_TEXTURE2D_DESC pd = {};
        prev_[eye]->GetDesc(&pd);
        if (pd.Width != cd.Width || pd.Height != cd.Height || pd.Format != cd.Format) {
            prevBytes_ -= (uint64_t)pd.Width * pd.Height * 4;
            rel(prevSrv_[eye]); rel(prev_[eye]);
        }
    }
    if (!prev_[eye]) {
        D3D11_TEXTURE2D_DESC td = cd;
        td.MipLevels = 1; td.ArraySize = 1; td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE; td.CPUAccessFlags = 0; td.MiscFlags = 0;
        if (FAILED(dev->CreateTexture2D(&td, nullptr, &prev_[eye])) ||
            FAILED(dev->CreateShaderResourceView(prev_[eye], nullptr, &prevSrv_[eye]))) {
            rel(prevSrv_[eye]); rel(prev_[eye]); prevOk_[eye] = false;
            return false;
        }
        prevBytes_ += (uint64_t)cd.Width * cd.Height * 4;
    }
    ctx->CopyResource(prev_[eye], color);
    prevOk_[eye] = true;
    return true;
}

void GuideGpu::forget(int eye) { if (eye == 0 || eye == 1) { prevOk_[eye] = false; tilePrevOk_[eye] = false; } }

void GuideGpu::audit(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11ShaderResourceView* color,
                     float expX, float expY) {
    (void)dev;
    if (!ready_ || !color || (eye != 0 && eye != 1)) return;
    // Fold in whichever staging copy the GPU has finished, without waiting.
    for (int s = 0; s < 2; ++s) {
        if (!auditPending_[s]) continue;
        D3D11_MAPPED_SUBRESOURCE m = {};
        if (ctx->Map(auditStage_[s], 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &m) != S_OK) continue;
        for (int y = 0; y < kAuditGrid; ++y) {
            const float* row = (const float*)((const uint8_t*)m.pData + (size_t)y * m.RowPitch);
            for (int x = 0; x < kAuditGrid; ++x) {
                const float* v = row + x * 4;
                if (v[0] < 0) continue;   // moved off screen
                const float z = v[2];
                const int b = z < 0 ? 7 : z < 0.1f ? 0 : z < 0.3f ? 1 : z < 1 ? 2 : z < 2 ? 3 : z < 10 ? 4 : z < 50 ? 5 : z < 1000 ? 6 : 7;
                bins[b].vecErr += v[0]; bins[b].zeroErr += v[1]; bins[b].masked += v[3]; ++bins[b].n;
            }
        }
        ctx->Unmap(auditStage_[s], 0);
        D3D11_MAPPED_SUBRESOURCE ma = {}, mb = {};
        if (ctx->Map(flowStage_[0][s], 0, D3D11_MAP_READ, 0, &ma) == S_OK) {
            if (ctx->Map(flowStage_[1][s], 0, D3D11_MAP_READ, 0, &mb) == S_OK) {
                double fx = 0, fy = 0; uint64_t fn = 0;
                const float jx = auditExp_[s][0], jy = auditExp_[s][1];   // the jitter's own shift
                for (int pass = 0; pass < 2; ++pass)
                for (int y = 0; y < kAuditGrid; ++y) {
                    const float* ra = (const float*)((const uint8_t*)ma.pData + (size_t)y * ma.RowPitch);
                    const float* rb = (const float*)((const uint8_t*)mb.pData + (size_t)y * mb.RowPitch);
                    for (int x = 0; x < kAuditGrid; ++x) {
                        const float* a = ra + x * 4; const float* b = rb + x * 4;
                        const float ex = a[0] - jx, ey = a[1] - jy, mx = a[2], my = a[3], z = b[1];
                        const bool arms = z >= 0 && z < bodyDepth;
                        if (pass == 1) {   // scatter around this image's mean shift
                            if (b[0] >= 0.5f && !arms && fn >= 50) {
                                const double dx = ex - fx / fn, dy = ey - fy / fn;
                                flow.frameScatter += sqrt(dx * dx + dy * dy) / fn;
                            }
                            continue;
                        }
                        if (b[0] < 0.5f) { ++flow.rejected; continue; }
                        const float eabs = sqrtf(ex * ex + ey * ey);
                        if (arms) { ++flow.armsN; flow.armsAbs += eabs; continue; }
                        fx += ex; fy += ey; ++fn;
                        const float tx = mx + ex, ty = my + ey;
                        ++flow.n; flow.ex += ex; flow.ey += ey; flow.eabs += eabs;
                        flow.mt[0] += mx * tx; flow.mm[0] += mx * mx; flow.mt[1] += my * ty; flow.mm[1] += my * my;
                        flow.pabs += sqrtf(mx * mx + my * my); flow.tabs += sqrtf(tx * tx + ty * ty);
                        if (eabs > 1.0f) ++flow.big;
                        {
                            const int bb = z < 0 ? 7 : z < 0.1f ? 0 : z < 0.3f ? 1 : z < 1 ? 2 : z < 2 ? 3 : z < 10 ? 4 : z < 50 ? 5 : z < 1000 ? 6 : 7;
                            flow.bandMt[bb] += mx * tx + my * ty; flow.bandMm[bb] += mx * mx + my * my;
                            flow.bandErr[bb] += eabs; ++flow.bandN[bb];
                        }
                    }
                }
                if (fn >= 50) {
                    const double cx = fx / fn, cy = fy / fn;   // after the jitter subtraction
                    ++flow.frames; flow.frameShift += sqrt(cx * cx + cy * cy);
                    const double rx = cx + jx, ry = cy + jy, en = (double)jx * jx + (double)jy * jy;
                    if (en > 1e-6) {
                        ++flow.jitFrames; flow.jitShiftRaw += sqrt(rx * rx + ry * ry); flow.jitExpect += sqrt(en);
                        flow.jitDot += rx * jx + ry * jy; flow.jitNorm += en;
                    }
                }
                ctx->Unmap(flowStage_[1][s], 0);
            }
            ctx->Unmap(flowStage_[0][s], 0);
        }
        auditPending_[s] = false;
    }
    if (!prevOk_[eye] || !prevSrv_[eye]) return;
    const int s = auditNext_;
    if (auditPending_[s]) return;   // both copies still in flight: skip this image
    auditNext_ = 1 - auditNext_;
    full_screen(ctx, raster_, blend_, ds_, vs_, psAudit_, cb_, kAuditGrid, kAuditGrid);
    ctx->OMSetRenderTargets(1, &auditRtv_, nullptr);
    ID3D11ShaderResourceView* srvs[5] = {color, prevSrv_[eye], motionSrv_, depthSrv_, biasSrv_};
    ctx->PSSetShaderResources(0, 5, srvs);
    ctx->PSSetSamplers(0, 1, &linear_);
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* none[5] = {};
    ctx->PSSetShaderResources(0, 5, none);
    ID3D11RenderTargetView* noRt = nullptr;
    ctx->OMSetRenderTargets(1, &noRt, nullptr);
    ctx->CopyResource(auditStage_[s], auditTex_);
    // The flow check on the same points (MRT: err + predicted vector, confidence + depth).
    full_screen(ctx, raster_, blend_, ds_, vs_, psFlow_, cb_, kAuditGrid, kAuditGrid);
    ctx->OMSetRenderTargets(2, flowRtv_, nullptr);
    ctx->PSSetShaderResources(0, 5, srvs);
    ctx->PSSetSamplers(0, 1, &linear_);
    ctx->Draw(3, 0);
    ctx->PSSetShaderResources(0, 5, none);
    ID3D11RenderTargetView* noRt2[2] = {nullptr, nullptr};
    ctx->OMSetRenderTargets(2, noRt2, nullptr);
    ctx->CopyResource(flowStage_[0][s], flowTex_[0]);
    ctx->CopyResource(flowStage_[1][s], flowTex_[1]);
    auditExp_[s][0] = expX; auditExp_[s][1] = expY;
    auditPending_[s] = true;
}

bool GuideGpu::ensure_obj(ID3D11Device* dev) {
    const uint32_t tw = (w_ + 7) / 8, th = (h_ + 7) / 8;
    if (tiles_ && motion2_ && tilesW_ == tw && tilesH_ == th && obj2W_ == w_ && obj2H_ == h_) return true;
    rel(tilesUav_); rel(listUav_); rel(listSrv_); rel(list_); rel(listArgs_); rel(tilesSrv_); rel(tiles_); rel(motion2Rtv_); rel(motion2_);
    for (int e = 0; e < 2; ++e) { rel(tilePrevSrv_[e]); rel(tilePrev_[e]); tilePrevOk_[e] = false; }
    tilesW_ = tilesH_ = obj2W_ = obj2H_ = 0;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = tw; td.Height = th; td.MipLevels = 1; td.ArraySize = 1; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    td.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr = dev->CreateTexture2D(&td, nullptr, &tiles_);
    if (SUCCEEDED(hr)) hr = dev->CreateUnorderedAccessView(tiles_, nullptr, &tilesUav_);
    if (SUCCEEDED(hr)) hr = dev->CreateShaderResourceView(tiles_, nullptr, &tilesSrv_);
    {   // the list of tiles to search (an append buffer) and the indirect dispatch's arguments
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = tw * th * 4; bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
        bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; bd.StructureByteStride = 4;
        if (SUCCEEDED(hr)) hr = dev->CreateBuffer(&bd, nullptr, &list_);
        D3D11_UNORDERED_ACCESS_VIEW_DESC ud = {};
        ud.Format = DXGI_FORMAT_UNKNOWN; ud.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        ud.Buffer.NumElements = tw * th; ud.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
        if (SUCCEEDED(hr)) hr = dev->CreateUnorderedAccessView(list_, &ud, &listUav_);
        D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
        sd.Format = DXGI_FORMAT_UNKNOWN; sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        sd.Buffer.NumElements = tw * th;
        if (SUCCEEDED(hr)) hr = dev->CreateShaderResourceView(list_, &sd, &listSrv_);
        D3D11_BUFFER_DESC ad = {};
        ad.ByteWidth = 12; ad.Usage = D3D11_USAGE_DEFAULT; ad.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
        const UINT init[3] = {0, 1, 1};
        D3D11_SUBRESOURCE_DATA id = {init, 0, 0};
        if (SUCCEEDED(hr)) hr = dev->CreateBuffer(&ad, &id, &listArgs_);
    }
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    for (int e = 0; e < 2 && SUCCEEDED(hr); ++e) {
        hr = dev->CreateTexture2D(&td, nullptr, &tilePrev_[e]);
        if (SUCCEEDED(hr)) hr = dev->CreateShaderResourceView(tilePrev_[e], nullptr, &tilePrevSrv_[e]);
    }
    td.Width = w_; td.Height = h_; td.Format = DXGI_FORMAT_R16G16_FLOAT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (SUCCEEDED(hr)) hr = dev->CreateTexture2D(&td, nullptr, &motion2_);
    if (SUCCEEDED(hr)) hr = dev->CreateRenderTargetView(motion2_, nullptr, &motion2Rtv_);
    if (FAILED(hr)) {
        rel(tilesUav_); rel(listUav_); rel(listSrv_); rel(list_); rel(listArgs_); rel(tilesSrv_); rel(tiles_); rel(motion2Rtv_); rel(motion2_);
        for (int e = 0; e < 2; ++e) { rel(tilePrevSrv_[e]); rel(tilePrev_[e]); }
        return false;
    }
    tilesW_ = tw; tilesH_ = th; obj2W_ = w_; obj2H_ = h_;
    return true;
}

bool GuideGpu::objmotion(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11ShaderResourceView* color,
                         float jitShiftX, float jitShiftY, const ObjParams& op, char* why, size_t cap) {
    if (!ready_ || !motion_ || !color || (eye != 0 && eye != 1)) { say(why, cap, "objmotion: not ready"); return false; }
    if (!prevOk_[eye] || !prevSrv_[eye]) { say(why, cap, "objmotion: no previous image of this eye"); return false; }
    if (!ensure_obj(dev)) { say(why, cap, "objmotion: targets failed"); return false; }
    const float om[8] = {(float)tilesW_, (float)tilesH_, jitShiftX, jitShiftY,
                         op.ratio, op.minGain, op.minContrast, (op.temporal && tilePrevOk_[eye]) ? 1.0f : 0.0f};
    ctx->UpdateSubresource(cbObj_, 0, nullptr, om, 0, 0);
    ID3D11Buffer* cbs[2] = {cb_, cbObj_};
    // The tiles: the early exits and the list (one thread per tile), then the search over the listed tiles (one
    // 64-thread group each, dispatched indirectly with the list's count).
    ID3D11ShaderResourceView* srvs[6] = {color, prevSrv_[eye], motionSrv_, depthSrv_, tilePrevOk_[eye] ? tilePrevSrv_[eye] : nullptr, nullptr};
    ID3D11ShaderResourceView* none[6] = {};
    ctx->CSSetConstantBuffers(0, 2, cbs);
    ctx->CSSetShaderResources(0, 5, srvs);
    ctx->CSSetSamplers(0, 1, &linear_);
    ID3D11UnorderedAccessView* uavs[2] = {tilesUav_, listUav_};
    const UINT counts[2] = {0xffffffffu, 0u};   // the list starts empty
    ctx->CSSetUnorderedAccessViews(0, 2, uavs, counts);
    ctx->CSSetShader(csObjPre_, nullptr, 0);
    ctx->Dispatch((tilesW_ + 7) / 8, (tilesH_ + 7) / 8, 1);
    ctx->CopyStructureCount(listArgs_, 0, listUav_);
    ID3D11UnorderedAccessView* noUav[2] = {};
    ctx->CSSetUnorderedAccessViews(0, 2, noUav, nullptr);
    srvs[5] = listSrv_;
    ctx->CSSetShaderResources(0, 6, srvs);
    ctx->CSSetUnorderedAccessViews(0, 1, &tilesUav_, nullptr);
    ctx->CSSetShader(csObjTile_, nullptr, 0);
    ctx->DispatchIndirect(listArgs_, 0);
    ctx->CSSetUnorderedAccessViews(0, 1, noUav, nullptr);
    ctx->CSSetShaderResources(0, 6, none);
    ctx->CSSetShader(nullptr, nullptr, 0);
    ID3D11Buffer* noCs[2] = {};
    ctx->CSSetConstantBuffers(0, 2, noCs);
    ID3D11RenderTargetView* noRt = nullptr;
    // Per pixel, into motion2, then over the camera vectors (everything downstream reads motion()).
    full_screen(ctx, raster_, blend_, ds_, vs_, psObjFix_, cb_, w_, h_);
    ctx->PSSetConstantBuffers(0, 2, cbs);
    ctx->OMSetRenderTargets(1, &motion2Rtv_, nullptr);
    srvs[4] = tilesSrv_;
    ctx->PSSetShaderResources(0, 5, srvs);
    ctx->Draw(3, 0);
    ctx->PSSetShaderResources(0, 5, none);
    ctx->OMSetRenderTargets(1, &noRt, nullptr);
    ctx->CopyResource(motion_, motion2_);
    ctx->CopyResource(tilePrev_[eye], tiles_);
    tilePrevOk_[eye] = true;
    ID3D11Buffer* nocb[2] = {};
    ctx->PSSetConstantBuffers(0, 2, nocb);
    return true;
}

uint64_t GuideGpu::bytes() const { return (uint64_t)w_ * h_ * 9 + prevBytes_; }

void GuideGpu::shutdown() {
    rel(depthRtv_); rel(motionRtv_); rel(biasRtv_); rel(depthSrv_); rel(motionSrv_); rel(biasSrv_);
    rel(depth_); rel(motion_); rel(bias_);
    for (int e = 0; e < 2; ++e) { rel(prevSrv_[e]); rel(prev_[e]); prevOk_[e] = false; }
    prevBytes_ = 0;
    rel(auditRtv_); rel(auditTex_);
    for (int t = 0; t < 2; ++t) { rel(flowRtv_[t]); rel(flowTex_[t]); rel(flowStage_[t][0]); rel(flowStage_[t][1]); }
    rel(psFlow_);
    flow = FlowStats{};
    for (int s = 0; s < 2; ++s) { rel(auditStage_[s]); auditPending_[s] = false; }
    rel(linear_); rel(ds_); rel(blend_); rel(raster_); rel(cb_);
    rel(psAudit_); rel(psMask_); rel(ps_); rel(vs_);
    rel(tilesUav_); rel(listUav_); rel(listSrv_); rel(list_); rel(listArgs_); rel(tilesSrv_); rel(tiles_); rel(motion2Rtv_); rel(motion2_); rel(csObjTile_); rel(csObjPre_); rel(psObjFix_); rel(cbObj_);
    for (int e = 0; e < 2; ++e) { rel(tilePrevSrv_[e]); rel(tilePrev_[e]); tilePrevOk_[e] = false; }
    tilesW_ = tilesH_ = obj2W_ = obj2H_ = 0;
    ready_ = false; w_ = h_ = 0;
    for (auto& b : bins) b = AuditBin{};
}

} // namespace dvr::dlss
