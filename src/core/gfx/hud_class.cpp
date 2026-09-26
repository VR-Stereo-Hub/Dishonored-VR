#include "core/framework/native_profile.h"
// core/gfx/hud_class.cpp - see hud_class.h.
#define DVR_CAT ::dvr::log::Cat::d3d
#include "core/gfx/hud_class.h"

#include "core/framework/frame_hooks.h"
#include "core/framework/status.h"
#include "core/gfx/hud_capture.h"
#include "core/gfx/hud_layout.h"
#include "core/gfx/hud_native_icon.h"
#include "core/hooks/vtable.h"
#include "core/util/log.h"
#include "core/util/paths.h"

#include <d3dcommon.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <atomic>
#include "core/gfx/hud_owner.h"
namespace dvr::hudclass {
namespace { std::atomic<bool> g_ownerTrace{false}; }
void set_owner_trace(bool on) {
    g_ownerTrace.store(on, std::memory_order_relaxed);
    if(on) DVR_INFO("hud/identity: ARMED finite read-only capture; at most 16 renderer stacks and 4 native attempts per family; no pixel readback, no budget reset on menus or loads");
}
bool owner_trace_enabled() { return g_ownerTrace.load(std::memory_order_relaxed); }
namespace {

typedef HRESULT (__stdcall *PFN_DrawPrimitiveUP)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT,
                                                 const void*, UINT);
typedef HRESULT (__stdcall *PFN_DrawIndexedPrimitiveUP)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT,
                                                        UINT, UINT, const void*, D3DFORMAT,
                                                        const void*, UINT);
typedef HRESULT (__stdcall *PFN_SetViewport)(IDirect3DDevice9*, const D3DVIEWPORT9*);
typedef HRESULT (__stdcall *PFN_SetRenderState)(IDirect3DDevice9*, D3DRENDERSTATETYPE, DWORD);
typedef HRESULT (__stdcall *PFN_SetTexture)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*);
typedef HRESULT (__stdcall *PFN_SetVertexDeclaration)(IDirect3DDevice9*,
                                                      IDirect3DVertexDeclaration9*);
typedef HRESULT (__stdcall *PFN_SetVertexShader)(IDirect3DDevice9*, IDirect3DVertexShader9*);
typedef HRESULT (__stdcall *PFN_SetPixelShader)(IDirect3DDevice9*, IDirect3DPixelShader9*);
typedef HRESULT (__stdcall *PFN_SetStreamSource)(IDirect3DDevice9*, UINT, IDirect3DVertexBuffer9*,
                                                 UINT, UINT);
typedef HRESULT (__stdcall *PFN_CreateStateBlock)(IDirect3DDevice9*, D3DSTATEBLOCKTYPE,
                                                  IDirect3DStateBlock9**);
typedef HRESULT (__stdcall *PFN_EndStateBlock)(IDirect3DDevice9*, IDirect3DStateBlock9**);
typedef HRESULT (__stdcall *PFN_SetTransform)(IDirect3DDevice9*, D3DTRANSFORMSTATETYPE, const D3DMATRIX*);

PFN_SetTransform            g_origSetXf = nullptr;
PFN_DrawPrimitiveUP         g_origDpUp = nullptr;
PFN_DrawIndexedPrimitiveUP  g_origDipUp = nullptr;
PFN_SetViewport             g_origSetVp = nullptr;
PFN_SetRenderState          g_origSetRs = nullptr;
PFN_SetTexture              g_origSetTex = nullptr;
PFN_SetVertexDeclaration    g_origSetDecl = nullptr;
PFN_SetVertexShader         g_origSetVs = nullptr;
PFN_SetPixelShader          g_origSetPs = nullptr;
PFN_SetStreamSource         g_origSetSs = nullptr;
PFN_CreateStateBlock        g_origCreateSb = nullptr;
PFN_EndStateBlock           g_origEndSb = nullptr;

bool g_hooksOk = false;

// The levers. g_wanted* is what the ini or the seam asked for, which can be
// BEFORE the device exists (the config is read at DllMain); it is applied when
// the hooks install.
bool g_track = false;        // the census
bool g_wanted = false;
bool g_regions = false;      // the region probe (and the routing it feeds)

// The render-thread assumption, measured rather than assumed. Draws and
// Present normally share the game's render thread; when the game loses focus
// (OnLostFocusPause) UE3 parks that thread and presents from the game thread
// until focus returns, and the two threads HAND OFF rather than race. So a
// mismatch is logged as a topology change with both ids and counted, but does
// not refuse: the first build latched a permanent refusal on that hand-off
// and switched the redirect off for the run on the tester's first alt-tab.
DWORD g_drawTid = 0;
DWORD g_lastDrawTid = 0;
bool  g_threadMismatch = false;
uint32_t g_threadMismatchPresents = 0;

// State blocks bypass the Set* hooks: a tracked value after an Apply would be
// stale. Counting them says whether the Apply slot has to be hooked too; a
// zero here is the licence not to (it read 0 for a whole run in session 10).
uint32_t g_stateBlocksCreated = 0;

// ---- the shadowed device state -------------------------------------------
IDirect3DSurface9*  g_rt0 = nullptr;        // pointer VALUE only, never a reference
IDirect3DSurface9*  g_bbPtr = nullptr;
uint32_t            g_bbW = 0, g_bbH = 0;
D3DVIEWPORT9        g_vp = {};
bool                g_vpKnown = false;
DWORD               g_zEnable = D3DZB_TRUE;
DWORD               g_zWrite = TRUE;
DWORD               g_alphaBlend = FALSE;
// VR-119: the blend equation as the game left it (D3D9's defaults until set),
// so the coverage equation forced on a redirected draw can be put back exactly.
DWORD               g_srcBlend = D3DBLEND_ONE, g_dstBlend = D3DBLEND_ZERO, g_blendOp = D3DBLENDOP_ADD;
DWORD               g_sepAlpha = FALSE, g_srcBlendA = D3DBLEND_ONE, g_dstBlendA = D3DBLEND_ZERO,
                    g_blendOpA = D3DBLENDOP_ADD;
uint32_t            g_winAlphaForced = 0;   // redirected draws that ran under the forced equation
void*               g_ps = nullptr;
void*               g_vs = nullptr;
void*               g_vdecl = nullptr;
void*               g_tex0 = nullptr;
IDirect3DVertexBuffer9* g_vb0 = nullptr;    // stream 0, pointer value only
UINT                g_vb0Offset = 0, g_vb0Stride = 0;

// VR-118: the fixed-function transform, shadowed from SetTransform (slot 44).
// The hypothesis this answers: a HUD draw with NO vertex shader bound takes
// its 2D transform from D3DTS_WORLD/VIEW/PROJECTION, and the c0..c3 shadow
// read the same values on every HUD draw because nothing had uploaded them
// since the last shader draw. Row-vector convention: out = v * W * V * P.
float    g_xfWorld[16] = {}, g_xfView[16] = {}, g_xfProj[16] = {};
uint8_t  g_xfKnown = 0;             // bit 0 world, 1 view, 2 projection (set since the device came up)
uint32_t g_xfCallsPresent = 0;      // SetTransform calls this present
uint32_t g_winXfCalls = 0;          // and per window
uint32_t g_winVsNull = 0, g_winVsSet = 0;   // HUD-class draws with no / a vertex shader bound
bool     g_vsDumpArmed = false;     // `draws vsdump`: disassemble the next distinct HUD vertex shader

// ---- caches keyed on pointer VALUE ---------------------------------------
template <int N> struct PtrMap {
    void*    key[N];
    uint32_t val[N];
    void clear() { memset(key, 0, sizeof(key)); memset(val, 0, sizeof(val)); }
    uint32_t* find_or_add(void* k) {
        uint32_t h = (uint32_t)(((uintptr_t)k >> 4) * 2654435761u) & (N - 1);
        for (int i = 0; i < 16; i++) {
            const uint32_t s = (h + i) & (N - 1);
            if (key[s] == k) return &val[s];
            if (key[s] == nullptr) { key[s] = k; val[s] = 0xffffffffu; return &val[s]; }
        }
        return nullptr;   // full: this pointer goes uncached this window
    }
};
PtrMap<512>  g_psHash;     // pixel shader -> FNV-1a of its bytecode
PtrMap<2048> g_texIsRt;    // texture -> 1 when it carries D3DUSAGE_RENDERTARGET
PtrMap<256>  g_surfSize;   // surface -> (w << 16) | h
PtrMap<128>  g_declPos;    // vertex declaration -> its position element, packed
PtrMap<256>  g_vbUsage;    // vertex buffer -> D3DUSAGE flags
PtrMap<64>   g_vsDumped;   // vertex shader -> 1 once `draws vsdump` wrote it

uint32_t g_psDistinct = 0;

uint32_t fnv32(const uint8_t* p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

uint32_t ps_hash(void* ps) {
    if (!ps) return 0;
    uint32_t* slot = g_psHash.find_or_add(ps);
    if (!slot) return 0;
    if (*slot != 0xffffffffu) return *slot;
    uint32_t h = 0;
    UINT size = 0;
    IDirect3DPixelShader9* s = (IDirect3DPixelShader9*)ps;
    if (SUCCEEDED(s->GetFunction(nullptr, &size)) && size && size <= 16384) {
        uint8_t buf[16384];
        if (SUCCEEDED(s->GetFunction(buf, &size))) h = fnv32(buf, size);
    }
    *slot = h;
    ++g_psDistinct;
    DVR_LOG(DVR_CAT, ::dvr::log::Level::Debug, "draws: ps %08x first seen (%u bytes)",
            (unsigned)h, (unsigned)size);
    return h;
}

// VR-118: the HUD vertex shader's transform, READ FROM THE SHADER. Every HUD
// draw on this build binds a vs_3_0 shader whose position output is
//     o = c[K+0]*v.x + c[K+1]*v.y + c[K+2]*v.z + c[K+3]*v.w
// (a float4x4 'Transform' held as COLUMNS at K=6 in all four shaders measured
// 2026-09-15; c0..c3 are whatever the last non-HUD shader left, which is why
// VR-117's rectangles were nonsense). The register numbers are per shader and
// never hard-coded (ENGINE_NOTES, the view-model's vertex path): at the first
// HUD-class draw with a new shader its bytecode is fetched (GetFunction),
// disassembled by d3dcompiler_47 (it reads D3D9 bytecode) and parsed: the
// instruction that writes the position output names the w column and the
// temp it sums into; the instructions before it that write that temp name the
// x, y and z columns. A shader that does not parse routes nothing (the probe
// refuses with "no transform map") and says so once. `draws vsdump` writes the
// disassembly under <data_dir>\dumps (game-derived, never in the tree) and
// logs the register lines.
typedef HRESULT (WINAPI *PFN_D3DDisassemble)(LPCVOID, SIZE_T, UINT, LPCSTR, ID3DBlob**);

struct VsXform {
    void*    vs = nullptr;
    uint32_t hash = 0;
    bool     parsed = false;     // the parse ran (ok or not)
    bool     ok = false;
    int      col[4] = {-1, -1, -1, -1};   // the constant register of the x, y, z, w column
};
const int kVsXforms = 16;
VsXform  g_vsXform[kVsXforms];
int      g_vsXformN = 0;

// One disassembly line: "op dst, s0, s1[, s2]" split on commas, trimmed.
struct DisLine { char op[16]; char arg[4][24]; int nargs; };

bool dis_split(const char* p, DisLine& L) {
    memset(&L, 0, sizeof(L));
    while (*p == ' ' || *p == '\t') ++p;
    if (!*p || *p == '/' || !strncmp(p, "dcl", 3) || !strncmp(p, "def", 3) || !strncmp(p, "vs_", 3)) return false;
    size_t k = 0;
    while (*p && *p != ' ' && k < sizeof(L.op) - 1) L.op[k++] = *p++;
    L.op[k] = 0;
    while (*p == ' ') ++p;
    while (*p && L.nargs < 4) {
        char* a = L.arg[L.nargs];
        k = 0;
        while (*p && *p != ',' && k < sizeof(L.arg[0]) - 1) { if (*p != ' ') a[k++] = *p; ++p; }
        a[k] = 0;
        ++L.nargs;
        if (*p == ',') ++p;
    }
    return L.nargs >= 2;
}

// "r0.xy" -> "r0": the register without its write mask / swizzle.
void reg_base(const char* a, char* out, size_t n) {
    size_t k = 0;
    while (a[k] && a[k] != '.' && k < n - 1) { out[k] = a[k]; ++k; }
    out[k] = 0;
}

// Parses the disassembly into `x`; logs the outcome once per shader.
void vs_xform_parse(const char* text, VsXform& x) {
    char posIn[8] = "", posOut[8] = "";
    // Pass 0: the declarations. vs_3_0 declares the output position as
    // dcl_position oN; vs_2_x writes oPos.
    const char* t = text;
    while (*t) {
        const char* e = strchr(t, '\n');
        const size_t n = e ? (size_t)(e - t) : strlen(t);
        char line[200];
        const size_t c = n < sizeof(line) - 1 ? n : sizeof(line) - 1;
        memcpy(line, t, c); line[c] = 0;
        const char* p = line;
        while (*p == ' ' || *p == '\t') ++p;
        if (!strncmp(p, "dcl_position", 12)) {
            const char* sp = strchr(p, ' ');
            if (sp) {
                ++sp;
                if (*sp == 'v' && !posIn[0]) reg_base(sp, posIn, sizeof(posIn));
                else if (*sp == 'o' && !posOut[0]) reg_base(sp, posOut, sizeof(posOut));
            }
        }
        if (!e) break;
        t = e + 1;
    }
    if (!posOut[0]) strcpy_s(posOut, "oPos");
    if (!posIn[0]) strcpy_s(posIn, "v0");
    // Pass 1: the instruction writing the output position. Its c# is the w
    // column (the swizzle on the position input says which), its r# the sum.
    DisLine lines[64];
    int nl = 0;
    t = text;
    while (*t && nl < 64) {
        const char* e = strchr(t, '\n');
        const size_t n = e ? (size_t)(e - t) : strlen(t);
        char line[200];
        const size_t c = n < sizeof(line) - 1 ? n : sizeof(line) - 1;
        memcpy(line, t, c); line[c] = 0;
        if (dis_split(line, lines[nl])) ++nl;
        if (!e) break;
        t = e + 1;
    }
    int outLine = -1;
    char temp[8] = "";
    const size_t inLen = strlen(posIn);
    auto column_of = [&](const DisLine& L, int* reg, int* comp) -> bool {
        // one c# argument and one posIn.<swizzle> argument
        *reg = -1; *comp = -1;
        for (int i = 1; i < L.nargs; ++i) {
            if (L.arg[i][0] == 'c' && L.arg[i][1] >= '0' && L.arg[i][1] <= '9') *reg = atoi(L.arg[i] + 1);
            else if (!strncmp(L.arg[i], posIn, inLen) && L.arg[i][inLen] == '.') {
                const char s = L.arg[i][inLen + 1];
                *comp = s == 'x' ? 0 : s == 'y' ? 1 : s == 'z' ? 2 : s == 'w' ? 3 : -1;
            }
        }
        return *reg >= 0 && *comp >= 0;
    };
    for (int i = 0; i < nl; ++i) {
        char dst[8]; reg_base(lines[i].arg[0], dst, sizeof(dst));
        if (strcmp(dst, posOut)) continue;
        int reg, comp;
        if (column_of(lines[i], &reg, &comp)) {
            x.col[comp] = reg;
            for (int a = 1; a < lines[i].nargs; ++a)
                if (lines[i].arg[a][0] == 'r') reg_base(lines[i].arg[a], temp, sizeof(temp));
            outLine = i;
        }
        break;
    }
    // Pass 2: the instructions BEFORE it that write the temp (the texgen after
    // it reuses r0 with other constants, so the order matters).
    if (outLine >= 0 && temp[0]) {
        for (int i = 0; i < outLine; ++i) {
            char dst[8]; reg_base(lines[i].arg[0], dst, sizeof(dst));
            if (strcmp(dst, temp)) continue;
            int reg, comp;
            if (column_of(lines[i], &reg, &comp) && x.col[comp] < 0) x.col[comp] = reg;
        }
    }
    x.parsed = true;
    x.ok = x.col[0] >= 0 && x.col[1] >= 0 && x.col[3] >= 0 &&
           x.col[0] < dvr::frame::vs_const_shadow_rows() && x.col[1] < dvr::frame::vs_const_shadow_rows() &&
           x.col[3] < dvr::frame::vs_const_shadow_rows() &&
           (x.col[2] < 0 || x.col[2] < dvr::frame::vs_const_shadow_rows());
    if (x.ok)
        DVR_INFO("draws/xform: vs %08x: position %s -> %s = c%d*x + c%d*y + c%d*z + c%d*w (read from the "
                 "shader; the probe applies these columns from the constant shadow)",
                 (unsigned)x.hash, posIn, posOut, x.col[0], x.col[1], x.col[2], x.col[3]);
    else
        DVR_WARN("draws/xform: vs %08x: the transform could not be read from the shader (position in %s, out %s, "
                 "columns x=c%d y=c%d z=c%d w=c%d, shadow rows %d) - its draws get NO rectangle and route by "
                 "the fallback; `draws vsdump` writes the disassembly",
                 (unsigned)x.hash, posIn, posOut, x.col[0], x.col[1], x.col[2], x.col[3],
                 dvr::frame::vs_const_shadow_rows());
}

// The shader's bytecode and disassembly (null blob when the compiler refused).
bool vs_disassemble(void* vs, uint8_t* buf, UINT* size, uint32_t* hash, ID3DBlob** blob) {
    *blob = nullptr; *size = 0; *hash = 0;
    IDirect3DVertexShader9* s = (IDirect3DVertexShader9*)vs;
    if (FAILED(s->GetFunction(nullptr, size)) || !*size || *size > 16384) return false;
    if (FAILED(s->GetFunction(buf, size))) return false;
    *hash = fnv32(buf, *size);
    static HMODULE compiler = LoadLibraryA("d3dcompiler_47.dll");
    static PFN_D3DDisassemble dis = compiler ? (PFN_D3DDisassemble)GetProcAddress(compiler, "D3DDisassemble") : nullptr;
    if (dis && FAILED(dis(buf, *size, 0, nullptr, blob))) *blob = nullptr;
    return true;
}

// The transform map of the bound vertex shader, parsed at first sight.
const VsXform* vs_xform_for(void* vs) {
    if (!vs) return nullptr;
    for (int i = 0; i < g_vsXformN; ++i) if (g_vsXform[i].vs == vs) return &g_vsXform[i];
    if (g_vsXformN >= kVsXforms) return nullptr;
    VsXform& x = g_vsXform[g_vsXformN++];
    x = VsXform();
    x.vs = vs;
    uint8_t buf[16384];
    UINT size = 0;
    ID3DBlob* blob = nullptr;
    if (vs_disassemble(vs, buf, &size, &x.hash, &blob) && blob) {
        vs_xform_parse((const char*)blob->GetBufferPointer(), x);
        blob->Release();
    } else {
        x.parsed = true;
        DVR_WARN("draws/xform: vertex shader %p: no bytecode or no disassembler (d3dcompiler_47) - its draws get "
                 "NO rectangle", vs);
    }
    return &x;
}

void vs_dump(void* vs, uint32_t psHash) {
    if (!vs) return;
    uint32_t* slot = g_vsDumped.find_or_add(vs);
    if (!slot || *slot == 1) return;
    *slot = 1;
    uint8_t buf[16384];
    UINT size = 0;
    uint32_t h = 0;
    ID3DBlob* blob = nullptr;
    if (!vs_disassemble(vs, buf, &size, &h, &blob)) {
        DVR_WARN("draws/vsdump: vertex shader %p would not give its function (%u bytes)", vs, (unsigned)size);
        return;
    }
    if (!blob)
        DVR_WARN("draws/vsdump: vs %08x (%u bytes, version dword %08x) - D3DDisassemble unavailable or refused; "
                 "the raw bytecode is written instead", (unsigned)h, (unsigned)size, (unsigned)*(uint32_t*)buf);
    char path[MAX_PATH];
    _snprintf(path, sizeof(path), "%s\\vs_%08x_ps_%08x.%s", dvr::paths::dumps_dir(), (unsigned)h, (unsigned)psHash,
              blob ? "txt" : "bin");
    path[sizeof(path) - 1] = 0;
    FILE* f = fopen(path, "wb");
    if (f) {
        if (blob) fwrite(blob->GetBufferPointer(), 1, blob->GetBufferSize() ? blob->GetBufferSize() - 1 : 0, f);
        else fwrite(buf, 1, size, f);
        fclose(f);
    }
    DVR_INFO("draws/vsdump: vs %08x (%u bytes, version dword %08x, the partner of ps %08x) -> %s%s",
             (unsigned)h, (unsigned)size, (unsigned)*(uint32_t*)buf, (unsigned)psHash, path,
             f ? "" : " (the file could not be written)");
    if (blob) {
        // The register lines: declarations, constants defined in the shader,
        // and every instruction reading the position input register.
        const char* t = (const char*)blob->GetBufferPointer();
        char posReg[8] = "";
        int lines = 0;
        while (*t && lines < 40) {
            const char* e = strchr(t, '\n');
            const size_t n = e ? (size_t)(e - t) : strlen(t);
            char line[200];
            const size_t c = n < sizeof(line) - 1 ? n : sizeof(line) - 1;
            memcpy(line, t, c); line[c] = 0;
            if (c && line[c - 1] == '\r') line[c - 1] = 0;
            const char* p = line;
            while (*p == ' ' || *p == '\t') ++p;
            bool show = !strncmp(p, "dcl", 3) || !strncmp(p, "def", 3) || !strncmp(p, "vs_", 3);
            if (!strncmp(p, "dcl_position", 12) && !posReg[0]) {
                const char* v = strchr(p, 'v');
                if (v) { size_t k = 0; while (v[k] && v[k] != ' ' && v[k] != '\n' && k < 6) { posReg[k] = v[k]; ++k; } posReg[k] = 0; }
            }
            if (!show && posReg[0]) {
                const char* at = strstr(p, posReg);
                // "v0" inside "v0." or "v0," or end: the input register, not v01
                if (at && (at[strlen(posReg)] == 0 || at[strlen(posReg)] == '.' || at[strlen(posReg)] == ',' || at[strlen(posReg)] == ' ')) show = true;
            }
            if (show) { DVR_INFO("draws/vsdump:   %s", p); ++lines; }
            if (!e) break;
            t = e + 1;
        }
        blob->Release();
    }
}

// 0 = no texture at stage 0, 1 = a plain texture, 2 = a render target, 3 = unknown.
uint8_t tex0_class(void* t) {
    if (!t) return 0;
    uint32_t* slot = g_texIsRt.find_or_add(t);
    if (!slot) return 3;
    if (*slot != 0xffffffffu) return (uint8_t)*slot;
    uint32_t isRt = 0;
    IDirect3DBaseTexture9* b = (IDirect3DBaseTexture9*)t;
    if (b->GetType() == D3DRTYPE_TEXTURE) {
        D3DSURFACE_DESC d;
        if (SUCCEEDED(((IDirect3DTexture9*)b)->GetLevelDesc(0, &d)) &&
            (d.Usage & D3DUSAGE_RENDERTARGET))
            isRt = 1;
    }
    *slot = isRt;
    return isRt ? 2 : 1;
}

void surf_size(IDirect3DSurface9* s, uint16_t* w, uint16_t* h) {
    *w = 0; *h = 0;
    if (!s) return;
    uint32_t* slot = g_surfSize.find_or_add(s);
    if (!slot) return;
    if (*slot == 0xffffffffu) {
        D3DSURFACE_DESC d;
        *slot = SUCCEEDED(s->GetDesc(&d)) ? ((d.Width & 0xffff) << 16) | (d.Height & 0xffff) : 0;
    }
    *w = (uint16_t)(*slot >> 16); *h = (uint16_t)(*slot & 0xffff);
}

// ---- the region probe -----------------------------------------------------
// The position element of the bound declaration: packed as
//   bit 31 known, bit 30 pre-transformed (POSITIONT), bits 16..23 type,
//   bits 12..15 stream, bits 0..11 offset. 0xff in the type = no position.
uint32_t decl_pos(void* decl) {
    if (!decl) return 0;
    uint32_t* slot = g_declPos.find_or_add(decl);
    if (!slot) return 0;
    if (*slot != 0xffffffffu) return *slot;
    uint32_t packed = 0x80000000u | (0xffu << 16);
    D3DVERTEXELEMENT9 el[64];
    UINT n = 64;
    if (SUCCEEDED(((IDirect3DVertexDeclaration9*)decl)->GetDeclaration(el, &n))) {
        for (UINT i = 0; i < n && i < 64; ++i) {
            if (el[i].Stream == 0xff) break;
            if (el[i].UsageIndex != 0) continue;
            if (el[i].Usage != D3DDECLUSAGE_POSITION && el[i].Usage != D3DDECLUSAGE_POSITIONT) continue;
            packed = 0x80000000u | (el[i].Usage == D3DDECLUSAGE_POSITIONT ? 0x40000000u : 0) |
                     ((uint32_t)el[i].Type << 16) | ((uint32_t)(el[i].Stream & 0xf) << 12) |
                     (el[i].Offset & 0xfff);
            break;
        }
    }
    *slot = packed;
    return packed;
}

const char* decl_type_name(uint32_t type) {
    switch (type) {
        case D3DDECLTYPE_FLOAT1: return "f1"; case D3DDECLTYPE_FLOAT2: return "f2";
        case D3DDECLTYPE_FLOAT3: return "f3"; case D3DDECLTYPE_FLOAT4: return "f4";
        case D3DDECLTYPE_SHORT2: return "s2"; case D3DDECLTYPE_SHORT4: return "s4";
        case D3DDECLTYPE_FLOAT16_2: return "h2"; case D3DDECLTYPE_FLOAT16_4: return "h4";
        case D3DDECLTYPE_SHORT2N: return "s2n"; case D3DDECLTYPE_SHORT4N: return "s4n";
        case 0xff: return "none";
        default: return "other";
    }
}

float half_to_float(uint16_t h) {
    const uint32_t s = (h >> 15) & 1, e = (h >> 10) & 0x1f, m = h & 0x3ff;
    float v;
    if (e == 0) v = ldexpf((float)m, -24);
    else if (e == 31) v = m ? 0.0f : 65504.0f;
    else v = ldexpf((float)(m | 0x400), (int)e - 25);
    return s ? -v : v;
}

// Reads one vertex's position (x, y, z) by declaration type. False = a type
// the probe does not read.
bool read_pos(const uint8_t* p, uint32_t type, float out[3]) {
    out[0] = out[1] = out[2] = 0.0f;
    switch (type) {
        case D3DDECLTYPE_FLOAT1: out[0] = ((const float*)p)[0]; return true;
        case D3DDECLTYPE_FLOAT2: out[0] = ((const float*)p)[0]; out[1] = ((const float*)p)[1]; return true;
        case D3DDECLTYPE_FLOAT3: case D3DDECLTYPE_FLOAT4:
            out[0] = ((const float*)p)[0]; out[1] = ((const float*)p)[1]; out[2] = ((const float*)p)[2]; return true;
        case D3DDECLTYPE_SHORT2: out[0] = ((const int16_t*)p)[0]; out[1] = ((const int16_t*)p)[1]; return true;
        case D3DDECLTYPE_SHORT4:
            out[0] = ((const int16_t*)p)[0]; out[1] = ((const int16_t*)p)[1]; out[2] = ((const int16_t*)p)[2]; return true;
        case D3DDECLTYPE_SHORT2N: out[0] = ((const int16_t*)p)[0] / 32767.0f; out[1] = ((const int16_t*)p)[1] / 32767.0f; return true;
        case D3DDECLTYPE_SHORT4N:
            out[0] = ((const int16_t*)p)[0] / 32767.0f; out[1] = ((const int16_t*)p)[1] / 32767.0f;
            out[2] = ((const int16_t*)p)[2] / 32767.0f; return true;
        case D3DDECLTYPE_FLOAT16_2:
            out[0] = half_to_float(((const uint16_t*)p)[0]); out[1] = half_to_float(((const uint16_t*)p)[1]); return true;
        case D3DDECLTYPE_FLOAT16_4:
            out[0] = half_to_float(((const uint16_t*)p)[0]); out[1] = half_to_float(((const uint16_t*)p)[1]);
            out[2] = half_to_float(((const uint16_t*)p)[2]); return true;
        default: return false;
    }
}

uint32_t verts_for(D3DPRIMITIVETYPE t, UINT prims) {
    switch (t) {
        case D3DPT_POINTLIST: return prims;
        case D3DPT_LINELIST: return prims * 2;
        case D3DPT_LINESTRIP: return prims + 1;
        case D3DPT_TRIANGLELIST: return prims * 3;
        case D3DPT_TRIANGLESTRIP: case D3DPT_TRIANGLEFAN: return prims + 2;
        default: return prims * 3;
    }
}

// The probe's answer for one draw, kept for the table and the route.
struct Probe {
    uint64_t drawKey=0;
    unsigned vertices=0,primitives=0;
    bool     ok = false;         // bbox[] is a screen rectangle (normalised, y down)
    bool     transformed = false;
    uint8_t  type = 0xff;
    uint8_t  why = 0;            // 0 ok, 1 no decl/position, 2 type unread, 3 no data, 4 write-only VB, 5 lock failed, 6 not finite, 7 off screen, 8 no transform map
    float    bbox[4] = {};
    float    nativePivot[4] = {};
    float    raw[4] = {};        // the vertices' own x/y range, before any transform
    // The transform columns applied: x, y and w (c0/c1/c3 by the old names;
    // now the shader's own registers, xcol[] says which).
    float    c0[4] = {}, c1[4] = {}, c3[4] = {};
    int      xcol[4] = {-1, -1, -1, -1};
    // VR-118: what else was bound at the draw. vs = the vertex shader (null =
    // fixed function); m0/m1/m3 = columns 0, 1 and 3 of W*V*P as shadowed from
    // SetTransform, in the same "row applied to (x,y,z,1)" shape as c0/c1/c3.
    void*    vs = nullptr;
    uint8_t  xfKnown = 0;
    float    m0[4] = {}, m1[4] = {}, m3[4] = {};
};
uint32_t g_probeSamples = 0, g_probeReads = 0, g_probeFails = 0;
long long g_probeQpc = 0;      // time spent in the probe this window

const char* probe_why(uint8_t w) {
    static const char* const k[] = { "ok", "no position element", "unread type", "no data", "write-only VB",
                                     "lock failed", "not finite", "off screen", "no transform map" };
    return k[w < 9 ? w : 0];
}

long long qpc_now() { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; }

// out = a * b for two row-major D3DMATRIX blocks (row-vector convention).
void mat_mul(const float* a, const float* b, float* out) {
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            out[r * 4 + c] = a[r * 4 + 0] * b[0 * 4 + c] + a[r * 4 + 1] * b[1 * 4 + c] +
                             a[r * 4 + 2] * b[2 * 4 + c] + a[r * 4 + 3] * b[3 * 4 + c];
}

// The shadowed fixed-function transform as three "rows applied to (x,y,z,1)":
// column c of M = W*V*P is (M[0][c], M[1][c], M[2][c], M[3][c]).
void xf_columns(float m0[4], float m1[4], float m3[4]) {
    float wv[16], m[16];
    static const float kIdentity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    mat_mul((g_xfKnown & 1) ? g_xfWorld : kIdentity, (g_xfKnown & 2) ? g_xfView : kIdentity, wv);
    mat_mul(wv, (g_xfKnown & 4) ? g_xfProj : kIdentity, m);
    for (int r = 0; r < 4; ++r) { m0[r] = m[r * 4 + 0]; m1[r] = m[r * 4 + 1]; m3[r] = m[r * 4 + 3]; }
}

// Walks the vertex range of a draw and returns its screen rectangle. `verts`
// is the user pointer for the UP entries; for the buffer entries the bound
// stream 0 is locked READONLY (a buffer created write-only is refused by D3D,
// so it is not asked). Vertices are sampled strided, at most 64 per draw.
void probe_draw(uint8_t entry, D3DPRIMITIVETYPE type, UINT prims, const void* verts, UINT stride,
                UINT firstVertex, UINT vertexCount, Probe& out) {
    const long long t0 = qpc_now();
    out = Probe();
    ++g_probeSamples;
    const uint32_t pk = decl_pos(g_vdecl);
    if (!(pk & 0x80000000u) || ((pk >> 16) & 0xff) == 0xff) { out.why = 1; ++g_probeFails; g_probeQpc += qpc_now() - t0; return; }
    out.type = (uint8_t)((pk >> 16) & 0xff);
    out.transformed = (pk & 0x40000000u) != 0;
    const uint32_t posOff = pk & 0xfff;
    const uint32_t posStream = (pk >> 12) & 0xf;
    out.vs = g_vs;
    out.xfKnown = g_xfKnown;
    xf_columns(out.m0, out.m1, out.m3);
    if (g_vs) ++g_winVsSet; else ++g_winVsNull;
    if (g_vsDumpArmed && g_vs) vs_dump(g_vs, ps_hash(g_ps));
    // The transform: the shader's own columns from the constant shadow. A
    // pre-transformed position (POSITIONT) needs none; anything else without
    // a parsed map gets no rectangle, and says so.
    float cz[4] = {0, 0, 0, 0};
    bool haveZ = false;
    if (!out.transformed) {
        const VsXform* xf = vs_xform_for(g_vs);
        if (!xf || !xf->ok) { out.why = 8; ++g_probeFails; g_probeQpc += qpc_now() - t0; return; }
        memcpy(out.xcol, xf->col, sizeof(out.xcol));
        memcpy(out.c0, dvr::frame::vs_const_shadow_row(xf->col[0]), sizeof(out.c0));
        memcpy(out.c1, dvr::frame::vs_const_shadow_row(xf->col[1]), sizeof(out.c1));
        memcpy(out.c3, dvr::frame::vs_const_shadow_row(xf->col[3]), sizeof(out.c3));
        if (xf->col[2] >= 0) { memcpy(cz, dvr::frame::vs_const_shadow_row(xf->col[2]), sizeof(cz)); haveZ = true; }
    }
    if (vertexCount == 0) vertexCount = verts_for(type, prims);
    out.vertices=vertexCount;out.primitives=prims;
    if (!vertexCount) { out.why = 3; ++g_probeFails; g_probeQpc += qpc_now() - t0; return; }

    const uint8_t* base = nullptr;
    IDirect3DVertexBuffer9* locked = nullptr;
    if (entry >= 2) {
        if (!verts || !stride) { out.why = 3; ++g_probeFails; g_probeQpc += qpc_now() - t0; return; }
        base = (const uint8_t*)verts + firstVertex * stride;
    } else {
        if (posStream != 0 || !g_vb0 || !g_vb0Stride) { out.why = 3; ++g_probeFails; g_probeQpc += qpc_now() - t0; return; }
        stride = g_vb0Stride;
        uint32_t* us = g_vbUsage.find_or_add(g_vb0);
        uint32_t usage = 0;
        if (us && *us != 0xffffffffu) usage = *us;
        else {
            D3DVERTEXBUFFER_DESC d;
            usage = SUCCEEDED(g_vb0->GetDesc(&d)) ? d.Usage : D3DUSAGE_WRITEONLY;
            if (us) *us = usage;
            DVR_LOG(DVR_CAT, ::dvr::log::Level::Debug, "draws: vertex buffer %p usage 0x%x stride %u first seen%s",
                    (void*)g_vb0, usage, stride, (usage & D3DUSAGE_WRITEONLY) ? " (write-only: its positions cannot be read)" : "");
        }
        if (usage & D3DUSAGE_WRITEONLY) { out.why = 4; ++g_probeFails; g_probeQpc += qpc_now() - t0; return; }
        void* p = nullptr;
        const UINT off = g_vb0Offset + firstVertex * stride;
        if (FAILED(g_vb0->Lock(off, vertexCount * stride, &p, D3DLOCK_READONLY)) || !p) {
            out.why = 5; ++g_probeFails; g_probeQpc += qpc_now() - t0; return;
        }
        base = (const uint8_t*)p;
        locked = g_vb0;
    }

    const uint32_t step = vertexCount > 64 ? (vertexCount+63) / 64 : 1;
    uint64_t content=1469598103934665603ull;
    auto hashBytes=[&](const void* data,size_t len) {
        const auto* bytes=(const uint8_t*)data;
        for(size_t j=0;j<len;++j) {content^=bytes[j];content*=1099511628211ull;}
    };
    hashBytes(&g_tex0,sizeof(g_tex0));hashBytes(&g_vs,sizeof(g_vs));
    hashBytes(&g_ps,sizeof(g_ps));hashBytes(&g_vdecl,sizeof(g_vdecl));
    hashBytes(&vertexCount,sizeof(vertexCount));hashBytes(&type,sizeof(type));
    hashBytes(&stride,sizeof(stride));
    // Local geometry/UV/colour, excluding the shader's screen transform.
    // No extra buffer lock or resource read; only the already-probed vertices.
    const bool keyable=!out.transformed && stride<=64 && vertexCount<=512 && vertexCount*stride<=8192;
    if(keyable) hashBytes(base,vertexCount*stride);
    float rawMin[2] = {1e30f, 1e30f}, rawMax[2] = {-1e30f, -1e30f};
    float scrMin[2] = {1e30f, 1e30f}, scrMax[2] = {-1e30f, -1e30f};
    bool finite = true, typeOk = true;
    uint32_t n = 0;
    for (uint32_t i = 0; i < vertexCount; i += step) {
        float v[3];
        if (!read_pos(base + i * stride + posOff, out.type, v)) { typeOk = false; break; }
        ++n;
        if (v[0] < rawMin[0]) rawMin[0] = v[0]; if (v[0] > rawMax[0]) rawMax[0] = v[0];
        if (v[1] < rawMin[1]) rawMin[1] = v[1]; if (v[1] > rawMax[1]) rawMax[1] = v[1];
        float sx, sy;
        if (out.transformed) {
            // POSITIONT: screen pixels of the bound target.
            sx = g_bbW ? v[0] / (float)g_bbW : 0.0f;
            sy = g_bbH ? v[1] / (float)g_bbH : 0.0f;
        } else {
            // The shader's own transform: o = X*x + Y*y + Z*z + W*w with the
            // columns read from the constant shadow (c0 = X, c1 = Y, cz = Z,
            // c3 = W here). A SHORT2/FLOAT2 position expands to (x, y, 0, 1).
            const float x = v[0], y = v[1], z = haveZ ? v[2] : 0.0f;
            const float ox = out.c0[0] * x + out.c1[0] * y + cz[0] * z + out.c3[0];
            const float oy = out.c0[1] * x + out.c1[1] * y + cz[1] * z + out.c3[1];
            float ow = out.c0[3] * x + out.c1[3] * y + cz[3] * z + out.c3[3];
            if (fabsf(ow) < 1e-6f) ow = 1.0f;
            sx = (ox / ow + 1.0f) * 0.5f;
            sy = (1.0f - oy / ow) * 0.5f;
        }
        if (!(sx == sx) || !(sy == sy) || fabsf(sx) > 1e6f || fabsf(sy) > 1e6f) { finite = false; break; }
        if (sx < scrMin[0]) scrMin[0] = sx; if (sx > scrMax[0]) scrMax[0] = sx;
        if (sy < scrMin[1]) scrMin[1] = sy; if (sy > scrMax[1]) scrMax[1] = sy;
    }
    if (locked) locked->Unlock();
    g_probeReads += n;
    if (n) { out.raw[0] = rawMin[0]; out.raw[1] = rawMin[1]; out.raw[2] = rawMax[0]; out.raw[3] = rawMax[1]; }
    if (!typeOk) { out.why = 2; ++g_probeFails; }
    else if (!finite || !n) { out.why = 6; ++g_probeFails; }
    else {
        out.bbox[0] = scrMin[0]; out.bbox[1] = scrMin[1]; out.bbox[2] = scrMax[0]; out.bbox[3] = scrMax[1];
        // A rectangle entirely outside the screen is a transform hypothesis
        // that failed for this draw, not a HUD element; say so.
        if (scrMax[0] < -0.5f || scrMin[0] > 1.5f || scrMax[1] < -0.5f || scrMin[1] > 1.5f) { out.why = 7; ++g_probeFails; }
        else { out.ok = true; if(keyable) out.drawKey=content ? content : 1; }
    }
    g_probeQpc += qpc_now() - t0;
}

// ---- the bucket signature -------------------------------------------------
#pragma pack(push, 1)
struct Sig {
    uint32_t psHash;
    uint32_t vdecl;      // low bits of the declaration pointer: an identity, not an address
    uint16_t rtW, rtH;
    uint8_t  entry;      // 0 DrawPrimitive, 1 Indexed, 2 UP, 3 IndexedUP
    uint8_t  rtClass;    // 0 the backbuffer, 1 another target, 2 unknown
    uint8_t  vpFull;
    uint8_t  zEnable;
    uint8_t  alphaBlend;
    uint8_t  tex0;       // 0 none, 1 plain, 2 render target
    uint8_t  afterTm;
    uint8_t  primBand;   // 0: <=2, 1: <=16, 2: <=256, 3: more
    uint32_t vs;         // VR-118: low bits of the vertex shader pointer; 0 = fixed function
};
#pragma pack(pop)

uint64_t sig_key(const Sig& s) {
    const uint8_t* p = (const uint8_t*)&s;
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < sizeof(Sig); i++) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

// The rule as run 46-01/46-04 measured this engine (ENGINE_NOTES, "The
// Scaleform HUD draw class, measured"): the target, the viewport, depth, and
// ALPHA BLENDING - the last is what leaves the scene resolve, the one opaque
// full-frame draw to the backbuffer, writing the world where it belongs.
enum { kTermRt = 0, kTermVp, kTermZ, kTermBlend, kTermCount };
const char* const kTermName[kTermCount] = { "rt=backbuffer", "viewport=full", "z=off", "blend=on" };

void sig_terms(const Sig& s, bool* t) {
    t[kTermRt]    = (s.rtClass == 0);
    t[kTermVp]    = (s.vpFull != 0);
    t[kTermZ]     = (s.zEnable == D3DZB_FALSE);
    t[kTermBlend] = (s.alphaBlend != 0);
}

bool is_candidate(const Sig& s) {
    bool t[kTermCount];
    sig_terms(s, t);
    for (int i = 0; i < kTermCount; i++) if (!t[i]) return false;
    return true;
}

void sig_text(const Sig& s, char* out, size_t n) {
    static const char* kEntry[4] = { "DP", "DIP", "UP", "IUP" };
    static const char* kTex[4]   = { "tex=none", "tex=plain", "tex=RT", "tex=?" };
    static const char* kPrim[4]  = { "p<=2", "p<=16", "p<=256", "p>256" };
    char rt[32];
    if (s.rtClass == 0) _snprintf(rt, sizeof(rt), "bb");
    else if (s.rtClass == 1) _snprintf(rt, sizeof(rt), "rt%ux%u", (unsigned)s.rtW, (unsigned)s.rtH);
    else _snprintf(rt, sizeof(rt), "rt?");
    rt[sizeof(rt) - 1] = 0;
    _snprintf(out, n, "%-3s %-10s %s z%u %s %s %s ps=%08x vs=%08x vd=%08x %s",
              kEntry[s.entry & 3], rt, s.vpFull ? "vpF" : "vpP", (unsigned)s.zEnable,
              s.alphaBlend ? "blend" : "opaque", kTex[s.tex0 & 3],
              s.afterTm ? "aTM" : "bTM", (unsigned)s.psHash, (unsigned)s.vs, (unsigned)s.vdecl,
              kPrim[s.primBand & 3]);
    out[n - 1] = 0;
}

// ---- the window's table ---------------------------------------------------
struct Row {
    uint64_t key;
    Sig      sig;
    uint32_t draws;
    uint32_t presents;
    uint32_t lastPresent;
    uint32_t minOrd, maxOrd;
    // The region probe's union for this bucket (HUD-class buckets only).
    uint32_t bbN, bbFail;
    uint8_t  posType, posT, lastWhy;
    float    bb[4], raw[4], c0[4], c1[4], c3[4];
    int      xcol[4];                 // VR-118: the shader's transform columns (registers) at the last draw
    uint8_t  xfKnown;                 // the fixed-function shadow at the last draw (0 on this build: measured)
    float    m0[4], m1[4], m3[4];
    int      lastElement;
};
const int kRows = 512;
Row      g_row[kRows];
uint32_t g_rowsUsed = 0;
uint32_t g_rowOverflow = 0;

// VR-118/120: the element census. A bucket is a draw CLASS (shader, decl,
// state) and its rectangle union spans every element drawn with it; the
// elements are the CLUSTERS of individual draw rectangles inside a bucket.
// Each probed draw is keyed by (bucket, its rectangle quantised to 1/40 of
// the screen) so a bar that fills or empties still lands in one cluster and
// two elements 3 % apart land in two. `draws regions` prints them by
// frequency: that list IS the element list the layout table names.
struct Cluster {
    uint64_t key;
    uint32_t bucket;          // short_key of the bucket
    uint8_t  q[4];            // the quantised rectangle
    uint8_t  tex0;
    uint32_t draws, presents, lastPresent;
    float    bb[4];           // the union of the member rectangles
    int      lastElement;
};
const int kClusters = 1024;
const int kClusterQ = 40;
Cluster  g_cluster[kClusters];
uint32_t g_clustersUsed = 0, g_clusterOverflow = 0;

Cluster* cluster_for(uint32_t bucket, const uint8_t q[4]) {
    uint64_t k = 1469598103934665603ull;
    const uint8_t* b = (const uint8_t*)&bucket;
    for (int i = 0; i < 4; ++i) { k ^= b[i]; k *= 1099511628211ull; }
    for (int i = 0; i < 4; ++i) { k ^= q[i]; k *= 1099511628211ull; }
    uint32_t h = (uint32_t)(k ^ (k >> 32)) & (kClusters - 1);
    for (int i = 0; i < 24; i++) {
        const uint32_t slot = (h + i) & (kClusters - 1);
        Cluster& c = g_cluster[slot];
        if (c.key == k && c.draws) return &c;
        if (!c.draws) {
            memset(&c, 0, sizeof(c));
            c.key = k; c.bucket = bucket; memcpy(c.q, q, 4);
            c.bb[0] = c.bb[1] = 1e30f; c.bb[2] = c.bb[3] = -1e30f;
            c.lastElement = -1;
            ++g_clustersUsed;
            return &c;
        }
    }
    ++g_clusterOverflow;
    return nullptr;
}

uint8_t quant(float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return (uint8_t)(v * kClusterQ + 0.5f);
}

Row* row_for(const Sig& s) {
    const uint64_t k = sig_key(s);
    uint32_t h = (uint32_t)(k ^ (k >> 32)) & (kRows - 1);
    for (int i = 0; i < 24; i++) {
        const uint32_t slot = (h + i) & (kRows - 1);
        Row& r = g_row[slot];
        if (r.key == k && r.draws) return &r;
        if (!r.draws) {
            memset(&r, 0, sizeof(r));
            r.key = k; r.sig = s; r.minOrd = 0xffffffffu;
            r.bb[0] = r.bb[1] = 1e30f; r.bb[2] = r.bb[3] = -1e30f;
            r.raw[0] = r.raw[1] = 1e30f; r.raw[2] = r.raw[3] = -1e30f;
            r.lastElement = -1;
            ++g_rowsUsed;
            return &r;
        }
    }
    ++g_rowOverflow;
    return nullptr;
}

// ---- window counters ------------------------------------------------------
uint32_t g_winPresents = 0;
uint32_t g_winDraws = 0;
uint32_t g_winByEntry[4] = {};
uint32_t g_winTonemapDraws = 0;
uint32_t g_winTmFirstOrdSum = 0, g_winTmLastOrdSum = 0, g_winTmPresents = 0;
uint32_t g_winOrdSum = 0;
uint32_t g_winRtSampleAfterCand = 0;
unsigned long g_winStartMs = 0;
unsigned long g_regWinStartMs = 0;

// ---- per-present state ----------------------------------------------------
uint32_t g_ord = 0;
uint32_t g_presentNo = 0;
bool     g_tmSeen = false;
uint32_t g_tmFirstOrd = 0, g_tmLastOrd = 0;
bool     g_candSeen = false;

// `draws kill`: the project's rule for identifying a render pass is to make it
// MOVE. A killed bucket's draws are dropped, so a capture before and after says
// by PICTURE whether the class is the HUD.
const int kKills = 8;
uint32_t g_kill[kKills] = {};
int      g_killN = 0;
bool     g_killHud = false;
uint32_t g_winKilled = 0;

uint32_t (*g_viewportDraws)() = nullptr;
uint32_t (*g_postRender)() = nullptr;
uint32_t g_prAtWinStart = 0, g_vdAtWinStart = 0;
uint32_t g_prLast = 0, g_prDeltaLast = 0;

inline uint32_t short_key(uint64_t k) { return (uint32_t)(k >> 32); }

char     g_verdict[320] = "not measured yet";
uint32_t g_candBuckets = 0;
double   g_candPerPresent = 0.0;

void note_draw() {
    g_lastDrawTid = GetCurrentThreadId();
}

// The rule, without the census's bookkeeping: four compares and no lookups.
inline bool hud_class() {
    if (g_rt0 && g_rt0 != g_bbPtr) return false;
    if (!g_vpKnown || !g_bbW) return false;
    if (g_vp.X != 0 || g_vp.Y != 0 || g_vp.Width != g_bbW || g_vp.Height != g_bbH) return false;
    if (g_zEnable != D3DZB_FALSE) return false;
    return g_alphaBlend != FALSE;
}

// Returns true when this draw is to be DROPPED (the kill lever).
bool record(uint8_t entry, UINT prims, const Probe* probe, int element) {
    ++g_ord;
    Sig s;
    memset(&s, 0, sizeof(s));
    s.entry = entry;
    s.psHash = ps_hash(g_ps);
    s.vdecl = (uint32_t)(uintptr_t)g_vdecl;
    s.zEnable = (uint8_t)g_zEnable;
    s.alphaBlend = g_alphaBlend ? 1 : 0;
    s.tex0 = tex0_class(g_tex0);
    s.primBand = prims <= 2 ? 0 : prims <= 16 ? 1 : prims <= 256 ? 2 : 3;
    s.vs = (uint32_t)(uintptr_t)g_vs;

    IDirect3DSurface9* rt = g_rt0;
    uint16_t rw = 0, rh = 0;
    if (!rt || rt == g_bbPtr) {
        s.rtClass = 0;
        rw = (uint16_t)g_bbW; rh = (uint16_t)g_bbH;
    } else {
        s.rtClass = 1;
        surf_size(rt, &rw, &rh);
        if (!rw) s.rtClass = 2;
    }
    s.rtW = rw; s.rtH = rh;
    s.vpFull = (g_vpKnown && rw && g_vp.X == 0 && g_vp.Y == 0 &&
                g_vp.Width == rw && g_vp.Height == rh) ? 1 : 0;

    const bool isTonemap = (s.rtClass == 0 && s.vpFull && !s.alphaBlend);
    if (isTonemap) {
        ++g_winTonemapDraws;
        if (!g_tmSeen) { g_tmSeen = true; g_tmFirstOrd = g_ord; }
        g_tmLastOrd = g_ord;
        if (g_candSeen) ++g_winRtSampleAfterCand;
    }
    s.afterTm = g_tmSeen ? 1 : 0;

    if (is_candidate(s)) g_candSeen = true;

    ++g_winDraws;
    ++g_winByEntry[entry & 3];
    Row* r = row_for(s);
    if (r) {
        ++r->draws;
        if (r->lastPresent != g_presentNo) { r->lastPresent = g_presentNo; ++r->presents; }
        if (g_ord < r->minOrd) r->minOrd = g_ord;
        if (g_ord > r->maxOrd) r->maxOrd = g_ord;
        if (probe) {
            r->posType = probe->type; r->posT = probe->transformed ? 1 : 0; r->lastWhy = probe->why;
            memcpy(r->c0, probe->c0, sizeof(r->c0)); memcpy(r->c1, probe->c1, sizeof(r->c1));
            memcpy(r->c3, probe->c3, sizeof(r->c3)); memcpy(r->xcol, probe->xcol, sizeof(r->xcol));
            r->xfKnown = probe->xfKnown;
            memcpy(r->m0, probe->m0, sizeof(r->m0)); memcpy(r->m1, probe->m1, sizeof(r->m1));
            memcpy(r->m3, probe->m3, sizeof(r->m3));
            r->lastElement = element;
            if (probe->ok) {
                const uint8_t q[4] = { quant(probe->bbox[0]), quant(probe->bbox[1]),
                                       quant(probe->bbox[2]), quant(probe->bbox[3]) };
                // A riding screen routes by its context, not by rectangle, and
                // its text alone overflowed the table (1372 over in the pause
                // menu): the clusters are the gameplay HUD's element census.
                if (Cluster* c = dvr::hudlayout::menu_riding() ? nullptr : cluster_for(short_key(r->key), q)) {
                    ++c->draws;
                    if (c->lastPresent != g_presentNo) { c->lastPresent = g_presentNo; ++c->presents; }
                    c->tex0 = s.tex0;
                    c->lastElement = element;
                    for (int k = 0; k < 2; ++k) {
                        if (probe->bbox[k] < c->bb[k]) c->bb[k] = probe->bbox[k];
                        if (probe->bbox[k + 2] > c->bb[k + 2]) c->bb[k + 2] = probe->bbox[k + 2];
                    }
                }
                ++r->bbN;
                for (int k = 0; k < 2; ++k) {
                    if (probe->bbox[k] < r->bb[k]) r->bb[k] = probe->bbox[k];
                    if (probe->bbox[k + 2] > r->bb[k + 2]) r->bb[k + 2] = probe->bbox[k + 2];
                    if (probe->raw[k] < r->raw[k]) r->raw[k] = probe->raw[k];
                    if (probe->raw[k + 2] > r->raw[k + 2]) r->raw[k + 2] = probe->raw[k + 2];
                }
            } else ++r->bbFail;
        }
    }

    if (!g_killN && !g_killHud) return false;
    if (g_killHud && is_candidate(s)) { ++g_winKilled; return true; }
    const uint32_t sk = short_key(sig_key(s));
    for (int i = 0; i < g_killN; i++)
        if (g_kill[i] == sk) { ++g_winKilled; return true; }
    return false;
}

// ---- VR-119: the coverage equation --------------------------------------------
// A sink is cleared to transparent black and the game's HUD draws land on it
// with the game's own blend state: SRCALPHA/INVSRCALPHA on colour, and
// without separate alpha blending the SAME equation on alpha, which yields
// srcA*srcA + dstA*(1-srcA): too transparent for every semi-transparent pixel
// and exactly zero for a black stroke. With the alpha mode off "repair" every
// redirected draw runs under SEPARATEALPHABLENDENABLE with ONE/INVSRCALPHA
// (ADD) on alpha, which accumulates the "over" coverage dstA = srcA +
// dstA*(1-srcA); the colour equation is untouched, so the colour stays
// premultiplied. Additive colour draws (ONE/ONE) get the same alpha equation.
// The shadowed values go back after the draw through the ORIGINAL setter (the
// shadow must not see our own writes). Eight calls per draw, about 170 per
// present in gameplay. State blocks would bypass this: g_stateBlocksCreated
// reads 0 for a whole run (`draws status` prints it).
inline bool alpha_force_wanted(int sink) { return dvr::hudlayout::force_capture_alpha(sink); }

void alpha_force_begin(IDirect3DDevice9* dev) {
    g_origSetRs(dev, D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
    g_origSetRs(dev, D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
    g_origSetRs(dev, D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
    g_origSetRs(dev, D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
    ++g_winAlphaForced;
    DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Info,
                 "draws/alpha: forcing the coverage equation on redirected HUD draws (separate alpha "
                 "ONE/INVSRCALPHA add; the game's colour equation untouched) - eight SetRenderState calls "
                 "per draw; `hud alpha mode repair` stops it");
}

void alpha_force_end(IDirect3DDevice9* dev) {
    g_origSetRs(dev, D3DRS_SEPARATEALPHABLENDENABLE, g_sepAlpha);
    g_origSetRs(dev, D3DRS_SRCBLENDALPHA, g_srcBlendA);
    g_origSetRs(dev, D3DRS_DESTBLENDALPHA, g_dstBlendA);
    g_origSetRs(dev, D3DRS_BLENDOPALPHA, g_blendOpA);
}

// The first HUD-class draw of each colour blend tuple, so the census can say
// which equations the HUD uses (SRCALPHA/INVSRCALPHA, and whether any glow is
// additive).
void note_blend_tuple() {
    static uint32_t seen[8];
    static int n = 0;
    const uint32_t t = (g_srcBlend & 0xff) | ((g_dstBlend & 0xff) << 8) | ((g_blendOp & 0xff) << 16) |
                       ((g_sepAlpha ? 1u : 0u) << 24);
    for (int i = 0; i < n; ++i) if (seen[i] == t) return;
    if (n < 8) seen[n++] = t;
    DVR_INFO("draws/blend: HUD-class draw with colour src=%lu dst=%lu op=%lu separateAlpha=%lu (alpha src=%lu dst=%lu) "
             "first seen (D3DBLEND: 2 ONE 5 SRCALPHA 6 INVSRCALPHA; a black stroke under SRCALPHA/INVSRCALPHA on "
             "alpha writes srcA*srcA, which is why 'repair' loses it)",
             (unsigned long)g_srcBlend, (unsigned long)g_dstBlend, (unsigned long)g_blendOp, (unsigned long)g_sepAlpha,
             (unsigned long)g_srcBlendA, (unsigned long)g_dstBlendA);
}

thread_local float g_nativeCo=1,g_nativeSi=0,g_nativeAspect=1;
thread_local uint32_t g_nativeFrame=0;
thread_local bool g_nativeBasis=false;
bool native_basis(float& co,float& si,float& aspect) {
    if(!g_nativeBasis || g_nativeFrame!=(uint32_t)dvr::frame::count()) return false;
    co=g_nativeCo;si=g_nativeSi;aspect=g_nativeAspect;return true;
}

// Native icon sizing changes only its own shader transform, then restores every
// touched row through the original setter. Never changes the shadow or capture.
struct NativeIconScope {
    IDirect3DDevice9* dev;int rows[4]{},count=0;float saved[4][4]{};
    NativeIconScope(IDirect3DDevice9* device,const Probe& p,int element):dev(device) {
        if(dvr::hudowner::active() && !dvr::hudlayout::menu_riding()) {
            const auto owner=dvr::hudowner::current();
            if(!owner || !owner.marker || !owner.pivotValid) return;
        }
        const float scale=dvr::hudlayout::native_objective_scale(element);
        const bool wanted=dvr::hudlayout::native_objective_upright(element);
        float co=1,si=0,aspect=1;
        const bool upright=wanted && native_basis(co,si,aspect);
        if(wanted) DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,1000,
            "hud/native-upright: renderedBasis=%d angle=%.2f aspect=%.3f frame=%u; center unchanged",
            (int)upright,std::atan2(si,co)*57.29578f,aspect,(unsigned)dvr::frame::count());
        if((scale>=1 && !upright) || !p.ok || p.transformed) return;
        float changed[4][4]{};int n=0;
        for(int i=0;i<4;++i) {
            const int row=p.xcol[i];if(row<0) continue;
            if(row>=256) return;
            for(int j=0;j<n;++j) if(rows[j]==row) return;
            rows[n]=row;memcpy(saved[n],dvr::frame::vs_const_shadow_row(row),sizeof(saved[n]));
            if(!dvr::hudnative::transform_column(saved[n],p.nativePivot,scale,co,si,aspect,changed[n])) return;
            ++n;
        }
        if(n<3) return;
        for(int i=0;i<n;++i) {
            count=i+1;
            if(FAILED(dvr::frame::orig_set_vs_const(dev,rows[i],changed[i],1))) {restore();return;}
        }
        DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,2000,
            "hud/native-icon: scale=%.3f rect=%.3f/%.3f/%.3f/%.3f; game target/color retained, routing owns identity",
            scale,p.bbox[0],p.bbox[1],p.bbox[2],p.bbox[3]);
    }
    void restore() {for(int i=0;i<count;++i) dvr::frame::orig_set_vs_const(dev,rows[i],saved[i],1);count=0;}
    ~NativeIconScope(){restore();}
};

// Capture the missing renderer boundary once per route family, at most one
// sample per Present and 16 per process. Route changes never rearm it.
// This tests whether native clip Display still encloses the D3D draw.
void owner_trace(const Probe& p, int element, int sink) {
    if (!owner_trace_enabled() || !::dvr::log::enabled(DVR_CAT,::dvr::log::Level::Info)) return;
    static bool seen[64]{};
    static unsigned samples=0;
    static uint32_t lastFrame=~0u;
    if (samples>=16 || !p.ok || element<0 || element>=32) return;
    const unsigned family=unsigned(element)*2+(sink<0?1:0);
    const uint32_t frame=(uint32_t)dvr::frame::count();
    if (seen[family] || lastFrame==frame) return;
    seen[family]=true;lastFrame=frame;++samples;
    const long long start=qpc_now();
    void* stack[24]{};
    const unsigned count=CaptureStackBackTrace(0,24,stack,nullptr);
    char frames[640]{};size_t used=0;
    for(unsigned i=0;i<count && used+24<sizeof(frames);++i) {
        const int n=_snprintf(frames+used,sizeof(frames)-used," %p",stack[i]);
        if(n<=0) break;
        used+=(size_t)n;
    }
    DVR_INFO("hud/identity-render: sample=%u/16 tid=%lu P%u element=%d sink=%d key=%016llx verts=%u prims=%u "
        "rect=%.5f/%.5f/%.5f/%.5f raw=%.7g/%.7g/%.7g/%.7g "
        "X=%.7g/%.7g/%.7g/%.7g Y=%.7g/%.7g/%.7g/%.7g W=%.7g/%.7g/%.7g/%.7g stack=%s",
        samples,GetCurrentThreadId(),frame,element,sink,p.drawKey,p.vertices,p.primitives,
        p.bbox[0],p.bbox[1],p.bbox[2],p.bbox[3],p.raw[0],p.raw[1],p.raw[2],p.raw[3],
        p.c0[0],p.c0[1],p.c0[2],p.c0[3],p.c1[0],p.c1[1],p.c1[2],p.c1[3],
        p.c3[0],p.c3[1],p.c3[2],p.c3[3],frames);
    LARGE_INTEGER freq;QueryPerformanceFrequency(&freq);
    DVR_INFO("hud/identity-cost: sample=%u elapsedUs=%.3f; finite capture includes stack and first log",
        samples,(qpc_now()-start)*1e6/(double)freq.QuadPart);
}

// ---- the hooks ------------------------------------------------------------
// Every draw hook: note the thread, classify once, probe once (if asked),
// record (the census), then route (the redirect) or forward.

#define HUD_DRAW_PROLOGUE(ENTRY, PRIMTYPE, PRIMS, VERTS, STRIDE, FIRST, COUNT)                     \
    note_draw();                                                                                  \
    const bool hudNow = (g_track || dvr::hudcap::armed()) && hud_class();                         \
    Probe probe; const float* pbb = nullptr; int element = -1;                                    \
    if (hudNow && g_regions) {                                                                    \
        probe_draw(ENTRY, PRIMTYPE, PRIMS, VERTS, STRIDE, FIRST, COUNT, probe);                   \
        if (probe.ok) pbb = probe.bbox;                                                           \
    }                                                                                             \
    int sink = -1;                                                                                \
    if (hudNow) note_blend_tuple();                                                               \
    if (hudNow && dvr::hudcap::armed()) sink = dvr::hudlayout::sink_for(g_regions ? pbb : nullptr, &element, probe.drawKey, probe.vertices, probe.primitives, probe.nativePivot); \
    if (g_track && record(ENTRY, PRIMS, hudNow && g_regions ? &probe : nullptr, element)) return D3D_OK;      \
    if (hudNow) owner_trace(probe,element,sink); \
    NativeIconScope nativeIcon(self,probe,element); \
    const bool forceAlpha = sink >= 0 && alpha_force_wanted(sink);

HRESULT __stdcall hkDrawPrimInner(IDirect3DDevice9* self, D3DPRIMITIVETYPE type, UINT start,
                                  UINT prims) {
    HUD_DRAW_PROLOGUE(0, type, prims, nullptr, 0, start, verts_for(type, prims))
    if (sink >= 0) {
        IDirect3DSurface9* gameRt = g_rt0;
        const D3DVIEWPORT9 vp = g_vp;
        if (dvr::hudcap::begin(self, vp, sink)) {
            if (forceAlpha) alpha_force_begin(self);
            const HRESULT r = dvr::frame::raw_draw_prim(self, type, start, prims);
            if (forceAlpha) alpha_force_end(self);
            if(SUCCEEDED(r) && element==dvr::hudlayout::ElObjective) dvr::hudcap::note_marker(sink,pbb);
            dvr::hudcap::end(self, gameRt, vp);
            return r;
        }
    }
    const HRESULT result=dvr::frame::raw_draw_prim(self, type, start, prims);
    if(hudNow && dvr::hudcap::armed()) dvr::hudcap::note_native_reference(result);
    return result;
}

HRESULT __stdcall hkDrawIndexedInner(IDirect3DDevice9* self, D3DPRIMITIVETYPE type, INT base,
                                     UINT minIdx, UINT numVerts, UINT startIdx, UINT prims) {
    HUD_DRAW_PROLOGUE(1, type, prims, nullptr, 0, (UINT)(base + (INT)minIdx), numVerts)
    if (sink >= 0) {
        IDirect3DSurface9* gameRt = g_rt0;
        const D3DVIEWPORT9 vp = g_vp;
        if (dvr::hudcap::begin(self, vp, sink)) {
            if (forceAlpha) alpha_force_begin(self);
            const HRESULT r = dvr::frame::raw_draw_indexed(self, type, base, minIdx, numVerts, startIdx, prims);
            if (forceAlpha) alpha_force_end(self);
            if(SUCCEEDED(r) && element==dvr::hudlayout::ElObjective) dvr::hudcap::note_marker(sink,pbb);
            dvr::hudcap::end(self, gameRt, vp);
            return r;
        }
    }
    const HRESULT result=dvr::frame::raw_draw_indexed(self, type, base, minIdx, numVerts, startIdx, prims);
    if(hudNow && dvr::hudcap::armed()) dvr::hudcap::note_native_reference(result);
    return result;
}

HRESULT __stdcall hkDrawPrimitiveUP(IDirect3DDevice9* self, D3DPRIMITIVETYPE type, UINT prims,
                                    const void* verts, UINT stride) {
    dvr::native_profile::Scope timing(dvr::native_profile::DrawPrimitiveUPInclusive);
    HUD_DRAW_PROLOGUE(2, type, prims, verts, stride, 0, verts_for(type, prims))
    if (sink >= 0) {
        IDirect3DSurface9* gameRt = g_rt0;
        const D3DVIEWPORT9 vp = g_vp;
        if (dvr::hudcap::begin(self, vp, sink)) {
            if (forceAlpha) alpha_force_begin(self);
            const HRESULT r = g_origDpUp(self, type, prims, verts, stride);
            if (forceAlpha) alpha_force_end(self);
            if(SUCCEEDED(r) && element==dvr::hudlayout::ElObjective) dvr::hudcap::note_marker(sink,pbb);
            dvr::hudcap::end(self, gameRt, vp);
            return r;
        }
    }
    const HRESULT result=g_origDpUp(self, type, prims, verts, stride);
    if(hudNow && dvr::hudcap::armed()) dvr::hudcap::note_native_reference(result);
    return result;
}

HRESULT __stdcall hkDrawIndexedPrimitiveUP(IDirect3DDevice9* self, D3DPRIMITIVETYPE type,
                                           UINT minIdx, UINT numVerts, UINT prims,
                                           const void* idxData, D3DFORMAT idxFmt,
                                           const void* verts, UINT stride) {
    dvr::native_profile::Scope timing(dvr::native_profile::DrawIndexedPrimitiveUPInclusive);
    HUD_DRAW_PROLOGUE(3, type, prims, verts, stride, minIdx, numVerts)
    if (sink >= 0) {
        IDirect3DSurface9* gameRt = g_rt0;
        const D3DVIEWPORT9 vp = g_vp;
        if (dvr::hudcap::begin(self, vp, sink)) {
            if (forceAlpha) alpha_force_begin(self);
            const HRESULT r = g_origDipUp(self, type, minIdx, numVerts, prims, idxData, idxFmt, verts, stride);
            if (forceAlpha) alpha_force_end(self);
            if(SUCCEEDED(r) && element==dvr::hudlayout::ElObjective) dvr::hudcap::note_marker(sink,pbb);
            dvr::hudcap::end(self, gameRt, vp);
            return r;
        }
    }
    const HRESULT result=g_origDipUp(self, type, minIdx, numVerts, prims, idxData, idxFmt, verts, stride);
    if(hudNow && dvr::hudcap::armed()) dvr::hudcap::note_native_reference(result);
    return result;
}

#undef HUD_DRAW_PROLOGUE

inline bool shadowing() { return g_track || g_regions || dvr::hudcap::armed() || dvr::hudcap::enabled(); }

HRESULT __stdcall hkSetViewport(IDirect3DDevice9* self, const D3DVIEWPORT9* vp) {
    dvr::native_profile::Scope timing(dvr::native_profile::SetViewportInclusive);
    if (vp && shadowing()) { g_vp = *vp; g_vpKnown = true; }
    return g_origSetVp(self, vp);
}

HRESULT __stdcall hkSetRenderState(IDirect3DDevice9* self, D3DRENDERSTATETYPE state, DWORD value) {
    dvr::native_profile::Scope timing(dvr::native_profile::SetRenderStateInclusive);
    if (shadowing()) {
        if (state == D3DRS_ZENABLE) g_zEnable = value;
        else if (state == D3DRS_ZWRITEENABLE) g_zWrite = value;
        else if (state == D3DRS_ALPHABLENDENABLE) g_alphaBlend = value;
        else if (state == D3DRS_SRCBLEND) g_srcBlend = value;              // VR-119: the blend equation
        else if (state == D3DRS_DESTBLEND) g_dstBlend = value;
        else if (state == D3DRS_BLENDOP) g_blendOp = value;
        else if (state == D3DRS_SEPARATEALPHABLENDENABLE) g_sepAlpha = value;
        else if (state == D3DRS_SRCBLENDALPHA) g_srcBlendA = value;
        else if (state == D3DRS_DESTBLENDALPHA) g_dstBlendA = value;
        else if (state == D3DRS_BLENDOPALPHA) g_blendOpA = value;
    }
    return g_origSetRs(self, state, value);
}

HRESULT __stdcall hkSetTexture(IDirect3DDevice9* self, DWORD stage, IDirect3DBaseTexture9* tex) {
    dvr::native_profile::Scope timing(dvr::native_profile::SetTextureInclusive);
    if (g_track && stage == 0) g_tex0 = tex;
    return g_origSetTex(self, stage, tex);
}

HRESULT __stdcall hkSetVertexDeclaration(IDirect3DDevice9* self,
                                         IDirect3DVertexDeclaration9* decl) {
    dvr::native_profile::Scope timing(dvr::native_profile::SetVertexDeclarationInclusive);
    if (g_track || g_regions) g_vdecl = decl;
    return g_origSetDecl(self, decl);
}

HRESULT __stdcall hkSetVertexShader(IDirect3DDevice9* self, IDirect3DVertexShader9* vs) {
    dvr::native_profile::Scope timing(dvr::native_profile::SetVertexShaderInclusive);
    if (shadowing()) g_vs = vs;   // VR-118: the probe needs it too (null = fixed function)
    return g_origSetVs(self, vs);
}

// VR-118: the fixed-function transform. Shadowed only while a lever wants it;
// counted always (one increment) so the 3 s line can say how many arrive.
HRESULT __stdcall hkSetTransform(IDirect3DDevice9* self, D3DTRANSFORMSTATETYPE state, const D3DMATRIX* m) {
    dvr::native_profile::Scope timing(dvr::native_profile::SetTransformInclusive);
    ++g_xfCallsPresent;
    if (m && shadowing()) {
        if (state == D3DTS_WORLD) { memcpy(g_xfWorld, m, sizeof(g_xfWorld)); g_xfKnown |= 1; }
        else if (state == D3DTS_VIEW) { memcpy(g_xfView, m, sizeof(g_xfView)); g_xfKnown |= 2; }
        else if (state == D3DTS_PROJECTION) { memcpy(g_xfProj, m, sizeof(g_xfProj)); g_xfKnown |= 4; }
    }
    return g_origSetXf(self, state, m);
}

HRESULT __stdcall hkSetPixelShader(IDirect3DDevice9* self, IDirect3DPixelShader9* ps) {
    dvr::native_profile::Scope timing(dvr::native_profile::SetPixelShaderInclusive);
    if (g_track) g_ps = ps;
    return g_origSetPs(self, ps);
}

HRESULT __stdcall hkSetStreamSource(IDirect3DDevice9* self, UINT stream, IDirect3DVertexBuffer9* vb,
                                    UINT offset, UINT stride) {
    dvr::native_profile::Scope timing(dvr::native_profile::SetStreamSourceInclusive);
    if (g_regions && stream == 0) { g_vb0 = vb; g_vb0Offset = offset; g_vb0Stride = stride; }
    return g_origSetSs(self, stream, vb, offset, stride);
}

HRESULT __stdcall hkCreateStateBlock(IDirect3DDevice9* self, D3DSTATEBLOCKTYPE type,
                                     IDirect3DStateBlock9** out) {
    ++g_stateBlocksCreated;
    return g_origCreateSb(self, type, out);
}

HRESULT __stdcall hkEndStateBlock(IDirect3DDevice9* self, IDirect3DStateBlock9** out) {
    ++g_stateBlocksCreated;
    return g_origEndSb(self, out);
}

// ---- the separator search -------------------------------------------------
struct ValSet {
    uint32_t v[24];
    int      n = 0;
    bool     over = false;
    void add(uint32_t x) {
        for (int i = 0; i < n; i++) if (v[i] == x) return;
        if (n >= 24) { over = true; return; }
        v[n++] = x;
    }
    bool disjoint(const ValSet& o) const {
        if (over || o.over) return false;
        for (int i = 0; i < n; i++)
            for (int j = 0; j < o.n; j++) if (v[i] == o.v[j]) return false;
        return true;
    }
};

uint32_t column(const Sig& s, int c) {
    switch (c) {
        case 0: return s.entry;
        case 1: return s.rtClass;
        case 2: return s.vpFull;
        case 3: return s.zEnable;
        case 4: return s.alphaBlend;
        case 5: return s.tex0;
        case 6: return s.afterTm;
        case 7: return s.psHash;
        case 8: return s.vdecl;
        default: return s.primBand;
    }
}
const char* const kColName[10] = { "entry", "rt", "viewport", "z", "blend",
                                   "tex0", "afterTonemap", "ps", "vdecl", "prims" };

void window_reset() {
    memset(g_row, 0, sizeof(g_row));
    g_rowsUsed = 0; g_rowOverflow = 0;
    memset(g_cluster, 0, sizeof(g_cluster));
    g_clustersUsed = 0; g_clusterOverflow = 0;
    g_winPresents = 0; g_winDraws = 0; g_winTonemapDraws = 0;
    g_winTmFirstOrdSum = g_winTmLastOrdSum = g_winTmPresents = 0;
    g_winOrdSum = 0; g_winRtSampleAfterCand = 0; g_winKilled = 0;
    g_prAtWinStart = g_postRender ? g_postRender() : 0;
    g_vdAtWinStart = g_viewportDraws ? g_viewportDraws() : 0;
    memset(g_winByEntry, 0, sizeof(g_winByEntry));
    g_psHash.clear(); g_texIsRt.clear(); g_surfSize.clear();
    g_psDistinct = 0;
    g_winStartMs = GetTickCount();
}

int cmp_rows(const void* a, const void* b) {
    const Row* x = (const Row*)a; const Row* y = (const Row*)b;
    return x->draws == y->draws ? 0 : (x->draws < y->draws ? 1 : -1);
}

void apply_wanted(const char* why) {
    if (g_wanted == g_track) return;
    if (g_wanted) {
        if (!g_hooksOk) {
            DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Info,
                         "draws: census ARMED (%s) - it starts when the device is created and the "
                         "draw hooks install", why);
            return;
        }
        window_reset();
    }
    g_track = g_wanted;
    DVR_INFO("draws: census %s (%s)%s",
             g_track ? "ON - a summary and a VERDICT every 3 s" : "off", why,
             g_track ? " - this costs one bucket lookup per draw, so turn it off when you are done"
                     : "");
}

} // namespace

// ---------------------------------------------------------------------------

void install(IDirect3DDevice9* dev) {
    if (!dev) return;
    dvr::frame::set_inner_draw_hooks(hkDrawIndexedInner, hkDrawPrimInner);
    void* old = PatchVtable(dev, 83, (void*)hkDrawPrimitiveUP);
    if (old && !g_origDpUp) g_origDpUp = (PFN_DrawPrimitiveUP)old;
    old = PatchVtable(dev, 84, (void*)hkDrawIndexedPrimitiveUP);
    if (old && !g_origDipUp) g_origDipUp = (PFN_DrawIndexedPrimitiveUP)old;
    old = PatchVtable(dev, 47, (void*)hkSetViewport);
    if (old && !g_origSetVp) g_origSetVp = (PFN_SetViewport)old;
    old = PatchVtable(dev, 57, (void*)hkSetRenderState);
    if (old && !g_origSetRs) g_origSetRs = (PFN_SetRenderState)old;
    old = PatchVtable(dev, 65, (void*)hkSetTexture);
    if (old && !g_origSetTex) g_origSetTex = (PFN_SetTexture)old;
    old = PatchVtable(dev, 87, (void*)hkSetVertexDeclaration);
    if (old && !g_origSetDecl) g_origSetDecl = (PFN_SetVertexDeclaration)old;
    old = PatchVtable(dev, 92, (void*)hkSetVertexShader);
    if (old && !g_origSetVs) g_origSetVs = (PFN_SetVertexShader)old;
    old = PatchVtable(dev, 107, (void*)hkSetPixelShader);
    if (old && !g_origSetPs) g_origSetPs = (PFN_SetPixelShader)old;
    old = PatchVtable(dev, 100, (void*)hkSetStreamSource);
    if (old && !g_origSetSs) g_origSetSs = (PFN_SetStreamSource)old;
    old = PatchVtable(dev, 59, (void*)hkCreateStateBlock);
    if (old && !g_origCreateSb) g_origCreateSb = (PFN_CreateStateBlock)old;
    old = PatchVtable(dev, 61, (void*)hkEndStateBlock);
    if (old && !g_origEndSb) g_origEndSb = (PFN_EndStateBlock)old;
    old = PatchVtable(dev, 44, (void*)hkSetTransform);   // VR-118: the fixed-function transform
    if (old && !g_origSetXf) g_origSetXf = (PFN_SetTransform)old;
    g_hooksOk = g_origDpUp && g_origDipUp && g_origSetVp && g_origSetRs && g_origSetTex &&
                g_origSetDecl && g_origSetVs && g_origSetPs && g_origSetSs && g_origSetXf;
    DVR_INFO("draws: hooks %s on device %p (the two buffer draws through frame_hooks' inner seam, "
             "innermost of the chain; DrawPrimitiveUP, DrawIndexedPrimitiveUP, SetViewport, SetRenderState, "
             "SetTexture, SetVertexDeclaration, SetVertexShader, SetPixelShader, SetStreamSource, SetTransform, state "
             "blocks patched here; SetRenderTarget is observed through frame_hooks). Forward-only until "
             "a lever is on",
             g_hooksOk ? "installed" : "PARTIAL - the census and the redirect will refuse", dev);
    apply_wanted("armed by the ini");
}

void set_game_counters(uint32_t (*viewportDraws)(), uint32_t (*postRenderDispatches)()) {
    g_viewportDraws = viewportDraws;
    g_postRender = postRenderDispatches;
    g_prLast = g_postRender ? g_postRender() : 0;
}

void note_world_view(const float* vp) {
    g_nativeFrame=(uint32_t)dvr::frame::count();
    g_nativeBasis=dvr::hudlayout::native_objective_upright(dvr::hudlayout::ElObjective) && vp &&
        dvr::hudnative::upright_basis(vp,g_nativeCo,g_nativeSi,g_nativeAspect);
}

void on_set_render_target(DWORD idx, IDirect3DSurface9* rt) {
    if (idx == 0) g_rt0 = rt;   // pointer VALUE; no reference is taken
}

void present_tick(IDirect3DDevice9* dev) {
    const DWORD tid = GetCurrentThreadId();
    const DWORD drew = g_lastDrawTid;
    const bool mismatch = drew && drew != tid;
    if (mismatch) ++g_threadMismatchPresents;
    if (mismatch != g_threadMismatch) {
        g_threadMismatch = mismatch;
        if (mismatch)
            DVR_WARN("draws: the last D3D draw came from thread %lu and this Present from thread %lu "
                     "(the game parks its render thread on a focus loss and presents from the game "
                     "thread; the two hand off, they do not race). The census and the redirect keep "
                     "running; this line and status.json's threadMismatchPresents say how often",
                     (unsigned long)drew, (unsigned long)tid);
        else
            DVR_INFO("draws: the draw thread and the present thread agree again (%lu) after %u "
                     "mismatched present(s)", (unsigned long)tid, g_threadMismatchPresents);
    } else if (drew && !g_drawTid) {
        DVR_INFO("draws: the draw thread IS the present thread (%lu) - the state may stay unlocked",
                 (unsigned long)tid);
    }
    g_drawTid = drew;

    // The backbuffer's identity and the shadowed viewport are what the HUD
    // redirect classifies on, so they are kept whenever any lever wants them.
    if (!shadowing()) return;
    if (dev) {
        IDirect3DSurface9* bb = nullptr;
        if (SUCCEEDED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) && bb) {
            D3DSURFACE_DESC d;
            if (SUCCEEDED(bb->GetDesc(&d))) { g_bbW = d.Width; g_bbH = d.Height; }
            g_bbPtr = bb;
            bb->Release();
        }
    }
    g_winXfCalls += g_xfCallsPresent;
    g_xfCallsPresent = 0;
    if (g_regions && !g_regWinStartMs) g_regWinStartMs = GetTickCount();
    if (g_regions && GetTickCount() - g_regWinStartMs >= 3000 && !g_track) {
        // The probe's own beat when the census is off: how much it costs and
        // how often it could not read. The table itself needs the census.
        const double us = g_probeQpc ? (double)g_probeQpc * 1e6 / (double)[]{ LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f.QuadPart; }() : 0.0;
        DVR_INFO("draws/regions: %u probes, %u vertices read, %u refused, %.0f us total (%.1f us/probe); "
                 "HUD draws with a vertex shader %u, without %u (fixed function); SetTransform calls %u "
                 "(W/V/P seen: %d/%d/%d) - `draws on` for the per-bucket rectangles",
                 g_probeSamples, g_probeReads, g_probeFails, us, g_probeSamples ? us / g_probeSamples : 0.0,
                 g_winVsSet, g_winVsNull, g_winXfCalls, (int)(g_xfKnown & 1), (int)((g_xfKnown >> 1) & 1),
                 (int)((g_xfKnown >> 2) & 1));
        g_probeSamples = g_probeReads = g_probeFails = 0; g_probeQpc = 0;
        g_winVsSet = g_winVsNull = g_winXfCalls = 0;
        g_regWinStartMs = GetTickCount();
    }
    if (!g_track) return;

    if (g_ord) {
        ++g_winPresents;
        g_winOrdSum += g_ord;
        if (g_tmSeen) { ++g_winTmPresents; g_winTmFirstOrdSum += g_tmFirstOrd; g_winTmLastOrdSum += g_tmLastOrd; }
    }
    ++g_presentNo;
    g_ord = 0; g_tmSeen = false; g_tmFirstOrd = g_tmLastOrd = 0; g_candSeen = false;
    if (g_postRender) {
        const uint32_t pr = g_postRender();
        g_prDeltaLast = pr - g_prLast;
        g_prLast = pr;
    }
    if (GetTickCount() - g_winStartMs >= 3000) {
        if (g_regions) log_regions("3s");
        log_summary("3s");
    }
}

void on_reset() {
    dvr::hudlayout::forget_draw_owners();
    g_stateBlocksCreated = 0;
    g_lastDrawTid = 0;
    g_rt0 = nullptr; g_bbPtr = nullptr; g_bbW = g_bbH = 0;
    g_vpKnown = false;
    g_ps = g_vs = g_vdecl = g_tex0 = nullptr;
    g_vb0 = nullptr; g_vb0Offset = g_vb0Stride = 0;
    g_xfKnown = 0;
    g_vsXformN = 0;   // shader objects may be recreated after a Reset
    g_declPos.clear(); g_vbUsage.clear(); g_vsDumped.clear();
    window_reset();
}

void shutdown() {
    g_track = false;
    g_regions = false;
    g_killN = 0; g_killHud = false;
}

bool census_enabled() { return g_track; }
void set_census_enabled(bool on) { g_wanted = on; apply_wanted("asked"); }

bool regions_enabled() { return g_regions; }
void set_regions_enabled(bool on) {
    dvr::hudlayout::forget_draw_owners();
    if (on == g_regions) return;
    g_regions = on;
    g_declPos.clear(); g_vbUsage.clear();
    g_probeSamples = g_probeReads = g_probeFails = 0; g_probeQpc = 0; g_regWinStartMs = 0;
    DVR_INFO("draws: the region probe is %s - %s", on ? "ON" : "off",
             on ? "every HUD-class draw's screen rectangle is read from its vertices and routes it to an element "
                  "(core/gfx/hud_layout); `draws status` prints the rectangles per bucket"
                : "every HUD-class draw routes to the element 'all'");
}

void log_regions(const char* why) {
    if (!g_track) { DVR_INFO("draws/regions: the table needs the census (`draws on`)"); return; }
    static Row sorted[kRows];
    memcpy(sorted, g_row, sizeof(sorted));
    qsort(sorted, kRows, sizeof(Row), cmp_rows);
    const uint32_t presents = g_winPresents ? g_winPresents : 1;
    const double us = g_probeQpc ? (double)g_probeQpc * 1e6 / (double)[]{ LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f.QuadPart; }() : 0.0;
    DVR_INFO("draws/regions: %s: %u probes, %u vertices read, %u refused, %.1f us/probe; HUD draws with a vertex "
             "shader %u, without %u; SetTransform calls %u (W/V/P seen %d/%d/%d); HUD-class buckets with their "
             "screen rectangles (normalised, y down; raw = the vertices' own range; xf = the shader's transform "
             "columns x/y/z/w as registers, X/Y/W = those columns' values at the last draw):",
             why, g_probeSamples, g_probeReads, g_probeFails, g_probeSamples ? us / g_probeSamples : 0.0,
             g_winVsSet, g_winVsNull, g_winXfCalls, (int)(g_xfKnown & 1), (int)((g_xfKnown >> 1) & 1),
             (int)((g_xfKnown >> 2) & 1));
    char txt[160];
    for (int i = 0, shown = 0; i < kRows && shown < 24; i++) {
        const Row& r = sorted[i];
        if (!r.draws || !is_candidate(r.sig)) continue;
        ++shown;
        sig_text(r.sig, txt, sizeof(txt));
        if (r.bbN)
            DVR_INFO("draws/regions:   k=%08x %s n=%.1f/present pos=%s%s bbox=[%.3f,%.3f - %.3f,%.3f] raw=[%.0f,%.0f - %.0f,%.0f] "
                     "xf=c%d/c%d/c%d/c%d X=(%.5f %.5f %.3f %.3f) Y=(%.5f %.5f %.3f %.3f) W=(%.3f %.3f %.3f %.3f) "
                     "ok=%u fail=%u(%s) -> %s",
                     (unsigned)short_key(r.key), txt, (double)r.draws / presents, decl_type_name(r.posType),
                     r.posT ? "T" : "", r.bb[0], r.bb[1], r.bb[2], r.bb[3], r.raw[0], r.raw[1], r.raw[2], r.raw[3],
                     r.xcol[0], r.xcol[1], r.xcol[2], r.xcol[3],
                     r.c0[0], r.c0[1], r.c0[2], r.c0[3], r.c1[0], r.c1[1], r.c1[2], r.c1[3],
                     r.c3[0], r.c3[1], r.c3[2], r.c3[3], r.bbN, r.bbFail,
                     probe_why(r.lastWhy), r.lastElement >= 0 ? dvr::hudlayout::element_name(r.lastElement) : "-");
        else
            DVR_INFO("draws/regions:   k=%08x %s n=%.1f/present pos=%s%s NO rectangle: %s (x%u) xf=c%d/c%d/c%d/c%d "
                     "X=(%.5f %.5f %.3f %.3f) Y=(%.5f %.5f %.3f %.3f) W=(%.3f %.3f %.3f %.3f) raw=[%.0f,%.0f - %.0f,%.0f] -> %s",
                     (unsigned)short_key(r.key), txt, (double)r.draws / presents, decl_type_name(r.posType),
                     r.posT ? "T" : "", probe_why(r.lastWhy), r.bbFail, r.xcol[0], r.xcol[1], r.xcol[2], r.xcol[3],
                     r.c0[0], r.c0[1], r.c0[2], r.c0[3], r.c1[0], r.c1[1], r.c1[2], r.c1[3],
                     r.c3[0], r.c3[1], r.c3[2], r.c3[3], r.raw[0], r.raw[1], r.raw[2], r.raw[3],
                     r.lastElement >= 0 ? dvr::hudlayout::element_name(r.lastElement) : "-");
        if (r.xfKnown)
            DVR_INFO("draws/regions:     fixed-function at the last draw (W/V/P %d/%d/%d): m0=(%.6f %.6f %.6f %.6f) "
                     "m1=(%.6f %.6f %.6f %.6f) m3=(%.6f %.6f %.6f %.6f)",
                     (int)(r.xfKnown & 1), (int)((r.xfKnown >> 1) & 1), (int)((r.xfKnown >> 2) & 1),
                     r.m0[0], r.m0[1], r.m0[2], r.m0[3], r.m1[0], r.m1[1], r.m1[2], r.m1[3],
                     r.m3[0], r.m3[1], r.m3[2], r.m3[3]);
    }
    // The clusters: the element list, by frequency.
    {
        static Cluster sortedC[kClusters];
        memcpy(sortedC, g_cluster, sizeof(sortedC));
        qsort(sortedC, kClusters, sizeof(Cluster), [](const void* a, const void* b) {
            const Cluster* x = (const Cluster*)a; const Cluster* y = (const Cluster*)b;
            return x->draws == y->draws ? 0 : (x->draws < y->draws ? 1 : -1);
        });
        DVR_INFO("draws/cluster: %u clusters (overflow %u) of draw rectangles quantised to 1/%d: each line is one "
                 "ELEMENT candidate (bucket, rectangle union, draws and presents per window):",
                 g_clustersUsed, g_clusterOverflow, kClusterQ);
        for (int i = 0, shown = 0; i < kClusters && shown < 48; i++) {
            const Cluster& c = sortedC[i];
            if (!c.draws) continue;
            ++shown;
            DVR_INFO("draws/cluster:   b=%08x rect=[%.3f,%.3f - %.3f,%.3f] (%.3f x %.3f, centre %.3f,%.3f) n=%.1f/present "
                     "in %u of %u presents tex=%s -> %s",
                     (unsigned)c.bucket, c.bb[0], c.bb[1], c.bb[2], c.bb[3], c.bb[2] - c.bb[0], c.bb[3] - c.bb[1],
                     (c.bb[0] + c.bb[2]) * 0.5f, (c.bb[1] + c.bb[3]) * 0.5f, (double)c.draws / presents,
                     c.presents, (unsigned)g_winPresents, c.tex0 == 0 ? "none" : c.tex0 == 1 ? "plain" : "RT",
                     c.lastElement >= 0 ? dvr::hudlayout::element_name(c.lastElement) : "-");
        }
    }
    g_probeSamples = g_probeReads = g_probeFails = 0; g_probeQpc = 0;
    g_winVsSet = g_winVsNull = g_winXfCalls = 0;
}

void log_summary(const char* why) {
    if (!g_track) { DVR_INFO("draws: census is off ([Draws] Census=1 or `draws on`)"); return; }
    const uint32_t presents = g_winPresents ? g_winPresents : 1;
    const double perPresent = (double)g_winDraws / (double)presents;
    DVR_INFO("draws: %s: presents=%u draws/present=%.0f (DP %.0f, DIP %.0f, UP %.0f, IUP %.0f) "
             "buckets=%u (overflow %u) ps distinct=%u stateBlocks=%u resolve draws/present=%.1f "
             "at ord %u..%u of %.0f",
             why, (unsigned)g_winPresents, perPresent,
             (double)g_winByEntry[0] / presents, (double)g_winByEntry[1] / presents,
             (double)g_winByEntry[2] / presents, (double)g_winByEntry[3] / presents,
             (unsigned)g_rowsUsed, (unsigned)g_rowOverflow, (unsigned)g_psDistinct,
             (unsigned)g_stateBlocksCreated, (double)g_winTonemapDraws / presents,
             (unsigned)(g_winTmPresents ? g_winTmFirstOrdSum / g_winTmPresents : 0),
             (unsigned)(g_winTmPresents ? g_winTmLastOrdSum / g_winTmPresents : 0),
             (double)g_winOrdSum / presents);
    if (!g_winTonemapDraws)
        DVR_INFO("draws: NO scene-resolve draw in the window - nothing opaque covered the whole "
                 "backbuffer, so either the frame arrives there by StretchRect or the classifier "
                 "is not seeing the resolve");
    {
        const uint32_t pr = g_postRender ? g_postRender() - g_prAtWinStart : 0;
        const uint32_t vd = g_viewportDraws ? g_viewportDraws() - g_vdAtWinStart : 0;
        if (!g_postRender)
            DVR_INFO("draws: postRender: no counter registered (the game side handed none over)");
        else if (!pr)
            DVR_INFO("draws: postRender: 0 dispatches in the window - the event never fired, so "
                     "this column says nothing about whether the HUD was drawn");
        else
            DVR_INFO("draws: postRender %u dispatches over %u viewport draws = %.2f per pass "
                     "(the name is dispatched on several objects per pass; what matters is that "
                     "it scales with the passes). Last present's delta %u is a game-thread count "
                     "read a frame ahead of these draws and is NOT aligned to this present",
                     (unsigned)pr, (unsigned)vd, vd ? (double)pr / (double)vd : 0.0,
                     (unsigned)g_prDeltaLast);
    }
    if (g_killN || g_killHud)
        DVR_INFO("draws: KILLING %s%s%d key(s): %.1f draws/present dropped - the picture is NOT "
                 "what the game drew", g_killHud ? "the HUD candidates" : "", g_killHud && g_killN ? " and " : "",
                 g_killN, (double)g_winKilled / presents);

    if (!g_winDraws) {
        DVR_INFO("draws: no draws in the window - the census sees nothing (is the game rendering?)");
        window_reset();
        return;
    }

    static Row sorted[kRows];
    memcpy(sorted, g_row, sizeof(sorted));
    qsort(sorted, kRows, sizeof(Row), cmp_rows);

    char txt[160];
    uint32_t candBuckets = 0, candDraws = 0;
    for (int i = 0; i < kRows; i++)
        if (sorted[i].draws && is_candidate(sorted[i].sig)) { ++candBuckets; candDraws += sorted[i].draws; }

    for (int i = 0; i < kRows && i < 12; i++) {
        const Row& r = sorted[i];
        if (!r.draws) break;
        sig_text(r.sig, txt, sizeof(txt));
        DVR_INFO("draws:   %s k=%08x %s n=%.1f/present ord %u..%u in %u presents",
                 is_candidate(r.sig) ? "HUD?" : "    ", (unsigned)short_key(r.key), txt,
                 (double)r.draws / presents, (unsigned)r.minOrd, (unsigned)r.maxOrd,
                 (unsigned)r.presents);
    }

    {
        uint32_t bbBuckets = 0, bbDraws = 0;
        for (int i = 0; i < kRows; i++)
            if (g_row[i].draws && g_row[i].sig.rtClass == 0) { ++bbBuckets; bbDraws += g_row[i].draws; }
        DVR_INFO("draws: the BACKBUFFER population: %u buckets, %.1f draws/present of %.0f (the "
                 "rest goes to the offscreen scene target)",
                 (unsigned)bbBuckets, (double)bbDraws / presents, perPresent);
        for (int i = 0, shown = 0; i < kRows && shown < 16; i++) {
            const Row& r = sorted[i];
            if (!r.draws || r.sig.rtClass != 0) continue;
            ++shown;
            sig_text(r.sig, txt, sizeof(txt));
            DVR_INFO("draws:   %s k=%08x %s n=%.1f/present ord %u..%u in %u presents",
                     is_candidate(r.sig) ? "HUD?" : "    ", (unsigned)short_key(r.key), txt,
                     (double)r.draws / presents, (unsigned)r.minOrd, (unsigned)r.maxOrd,
                     (unsigned)r.presents);
        }
    }

    ValSet cand[10], rest[10];
    uint32_t restBuckets = 0;
    for (int i = 0; i < kRows; i++) {
        const Row& r = g_row[i];
        if (!r.draws) continue;
        const bool c = is_candidate(r.sig);
        if (!c) ++restBuckets;
        for (int col = 0; col < 10; col++) (c ? cand[col] : rest[col]).add(column(r.sig, col));
    }
    char seps[192] = "";
    for (int col = 0; col < 10; col++) {
        if (!cand[col].disjoint(rest[col])) continue;
        if (seps[0]) strncat(seps, ", ", sizeof(seps) - strlen(seps) - 1);
        strncat(seps, kColName[col], sizeof(seps) - strlen(seps) - 1);
    }

    if (!candBuckets) {
        _snprintf(g_verdict, sizeof(g_verdict),
                  "NO HUD-CLASS DRAWS: nothing in %u buckets draws to the whole backbuffer with "
                  "depth off and blending", (unsigned)g_rowsUsed);
    } else if (!restBuckets) {
        _snprintf(g_verdict, sizeof(g_verdict),
                  "NO CLEAN SEPARATOR: EVERY bucket in the window is a candidate (%u), so the rule "
                  "does not separate anything - it would put the world on the panel",
                  (unsigned)candBuckets);
    } else if (!seps[0]) {
        _snprintf(g_verdict, sizeof(g_verdict),
                  "NO CLEAN SEPARATOR: %u candidate buckets, and NO column tells them from the "
                  "other %u - every value a candidate takes, some non-candidate takes too, so any "
                  "redirect would carry world draws with it",
                  (unsigned)candBuckets, (unsigned)restBuckets);
    } else {
        _snprintf(g_verdict, sizeof(g_verdict),
                  "HUD candidates %u buckets, %.1f draws/present of %.0f; separators with NO "
                  "overlap: %s", (unsigned)candBuckets, (double)candDraws / presents, perPresent, seps);
    }
    g_verdict[sizeof(g_verdict) - 1] = 0;
    g_candBuckets = candBuckets;
    g_candPerPresent = (double)candDraws / presents;
    DVR_INFO("draws: VERDICT: %s | rtSampleAfterCandidate=%u (0 = the resolve never runs after a "
             "candidate, so after-first equals after-last)",
             g_verdict, (unsigned)g_winRtSampleAfterCand);

    for (int i = 0, shown = 0; i < kRows && shown < 5; i++) {
        const Row& r = sorted[i];
        if (!r.draws || is_candidate(r.sig)) continue;
        bool t[kTermCount]; sig_terms(r.sig, t);
        int missing = -1, misses = 0;
        for (int k = 0; k < kTermCount; k++) if (!t[k]) { missing = k; ++misses; }
        if (misses != 1) continue;
        ++shown;
        sig_text(r.sig, txt, sizeof(txt));
        DVR_INFO("draws:   NEAR MISS (fails only %s): k=%08x %s n=%.1f/present",
                 kTermName[missing], (unsigned)short_key(r.key), txt,
                 (double)r.draws / presents);
    }
    window_reset();
}

void status(dvr::status::Writer& w) {
    w.kv("on", g_track);
    w.kv("regions", g_regions);
    w.kv("hooks", g_hooksOk);
    w.kv("threadMismatchPresents", (unsigned long)g_threadMismatchPresents);
    w.kv("candBuckets", (unsigned long)g_candBuckets);
    w.kv("candPerPresent", g_candPerPresent);
    w.kv("stateBlocks", (unsigned long)g_stateBlocksCreated);
    w.kv("alphaForced", (unsigned long)g_winAlphaForced);
    w.kv("killHud", g_killHud);
    w.kv("killKeys", (int)g_killN);
    w.kv("postRender", (unsigned long)(g_postRender ? g_postRender() : 0));
    w.kv("probeFails", (unsigned long)g_probeFails);
    w.kv("verdict", g_verdict);
}

bool command(const char* args) {
    if (!strcmp(args, "on"))  { set_census_enabled(true);  return true; }
    if (!strcmp(args, "off")) { set_census_enabled(false); return true; }
    if (!strcmp(args, "regions")) { log_regions("asked"); return true; }
    if (!strcmp(args, "vsdump")) {   // VR-118
        if (!g_regions) {
            DVR_WARN("draws: vsdump needs the region probe on (`hud regions on`) - it dumps the vertex shader "
                     "bound at the next HUD-class draw the probe sees");
            return true;
        }
        g_vsDumpArmed = true;
        g_vsDumped.clear();
        DVR_INFO("draws: vsdump armed - the next distinct vertex shader bound at a HUD-class draw is disassembled "
                 "into %s (a HUD draw with NO shader bound prints nothing here: that is the fixed-function "
                 "answer, and the `draws/regions` line counts it)", dvr::paths::dumps_dir());
        return true;
    }
    if (!strcmp(args, "unkill")) {
        g_killN = 0; g_killHud = false;
        DVR_INFO("draws: kill list cleared - the game's own draws again");
        return true;
    }
    if (!strncmp(args, "kill", 4)) {
        const char* a = args + 4;
        while (*a == ' ') ++a;
        if (!g_track) {
            DVR_WARN("draws: kill needs the census on (the buckets are what it kills) - `draws on`");
            return true;
        }
        if (!strcmp(a, "hud")) {
            g_killHud = true;
            DVR_INFO("draws: kill HUD armed - every candidate draw is dropped. Take `dump capture` "
                     "before and after: if exactly the HUD went and the world did not, the rule "
                     "found the HUD; if any world geometry went, it did not");
            return true;
        }
        const uint32_t k = (uint32_t)strtoul(a, nullptr, 16);
        if (!k) {
            DVR_WARN("draws: kill wants a bucket key from the table (`draws kill 1a2b3c4d`) or "
                     "`draws kill hud`; got '%s'", a);
            return true;
        }
        if (g_killN >= kKills) {
            DVR_WARN("draws: kill list is full (%d) - `draws unkill` first", kKills);
            return true;
        }
        g_kill[g_killN++] = k;
        DVR_INFO("draws: kill %08x armed (%d in the list) - dump a capture before and after",
                 (unsigned)k, g_killN);
        return true;
    }
    if (g_regions) log_regions("status");
    log_summary("status");
    return true;
}

} // namespace dvr::hudclass
