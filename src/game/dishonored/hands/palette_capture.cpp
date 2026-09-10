// Live shader reflection for hand and weapon placement.
// ---- shader reflection ------------------------------------------------------
//
// THE REGISTER NUMBERS ARE READ FROM THE SHADER, NEVER ASSUMED. Three shaders
// draw this mesh and they do NOT agree: MeshOrigin sits at c235 in one and
// c238 in the others, and one of them defines c4 as an immediate (3,1,0,0)
// while the device reports (0,0,0,1) for the same register. A layout guessed
// from one shader would be silently wrong on the next, and a device read of a
// def'd register is wrong by construction - the shader's own value wins.
//
// Shader Model 3 bytecode carries a CTAB comment block naming every constant
// and its register. Parsing it makes the capture self-describing.
struct PcLayout {
    int vp, bones, bonesN, localToWorld, worldToLocal, meshOrigin, meshExtension;
    bool ok;
};

static bool PcReflect(const uint8_t* code, UINT len, PcLayout* out)
{
    if (!code || len < 8 || !out) return false;
    memset(out, 0, sizeof(*out));
    out->vp = out->bones = out->localToWorld = out->worldToLocal = -1;
    out->meshOrigin = out->meshExtension = -1;

    UINT off = 4;                            // past the version token
    while (off + 4 <= len) {
        uint32_t tok;
        memcpy(&tok, code + off, 4);
        if ((tok & 0xFFFF) != 0xFFFE) { off += 4; continue; }
        const UINT n = (tok >> 16) & 0x7FFF;
        if (off + 8 > len) break;
        if (memcmp(code + off + 4, "CTAB", 4) != 0) { off += 4 * (n + 1); continue; }

        const UINT base = off + 8;
        if (base + 28 > len) return false;
        uint32_t hdr[7];
        memcpy(hdr, code + base, 28);
        const uint32_t nconst = hdr[3], cinfo = hdr[4];
        if (nconst > 256) return false;      // not a table we understand
        for (uint32_t k = 0; k < nconst; k++) {
            const UINT o = base + cinfo + k * 20;
            if (o + 20 > len) return false;
            uint32_t nameOff; uint16_t rset, ridx, rcnt;
            memcpy(&nameOff, code + o, 4);
            memcpy(&rset, code + o + 4, 2);
            memcpy(&ridx, code + o + 6, 2);
            memcpy(&rcnt, code + o + 8, 2);
            const UINT no = base + nameOff;
            if (no >= len) return false;
            const char* nm = (const char*)(code + no);
            UINT maxn = len - no, sl = 0;
            while (sl < maxn && nm[sl]) sl++;
            if (sl >= maxn) return false;     // unterminated
            if (rset != 2) continue;          // float bank only
            if (!strcmp(nm, "ViewProjectionMatrix")) out->vp = ridx;
            else if (!strcmp(nm, "BoneMatrices"))    { out->bones = ridx; out->bonesN = rcnt; }
            else if (!strcmp(nm, "LocalToWorld"))    out->localToWorld = ridx;
            else if (!strcmp(nm, "WorldToLocal"))    out->worldToLocal = ridx;
            else if (!strcmp(nm, "MeshOrigin"))      out->meshOrigin = ridx;
            else if (!strcmp(nm, "MeshExtension"))   out->meshExtension = ridx;
        }
        // Weapon placement only needs a component transform and its palette.
        // Depth/shadow passes need not declare the camera's projection.
        // The hand capture/placement callers separately require VP.
        out->ok = (out->bones >= 0 && out->localToWorld >= 0);
        return true;
    }
    return false;
}


// Keep the live layout current for the placement path. Called from the draw
// when the shader pointer changes; re-reads the bytecode and re-reflects, so
// the registers always come from the shader actually bound.
static void PcRefreshLayout(IDirect3DDevice9* dev)
{
    IDirect3DVertexShader9* vs = NULL;
    if (FAILED(dev->GetVertexShader(&vs)) || !vs) {
        if (vs) vs->Release();
        g_pcLayShader = NULL; g_pcLayVp = g_pcLayL2W = g_pcLayBones = -1;
        g_pcLayBonesN = 0;
        g_pcLayBonesPartial = -1; g_pcLayBonesNPartial = 0;
        return;
    }
    if ((void*)vs == g_pcLayShader) { vs->Release(); return; }

    UINT len = 0;
    static uint8_t code[PC_MAX_BYTECODE];
    bool ok = SUCCEEDED(vs->GetFunction(NULL, &len)) && len > 0 && len <= PC_MAX_BYTECODE;
    if (ok) ok = SUCCEEDED(vs->GetFunction(code, &len));
    void* key = (void*)vs;
    vs->Release();                        // released before anything can return

    PcLayout lay;
    const bool reflected = ok && PcReflect(code, len, &lay);
    // THE PARTIAL LAYOUT. A shader that declares BoneMatrices but no
    // LocalToWorld cannot be IDENTIFIED by transform - it has no transform to
    // compare - but it can still be CORRECTED once something else has
    // identified its geometry. That is exactly the ghost pass: the same weapon
    // mesh, drawn by a depth or shadow shader that carries the palette and
    // nothing else, left at the native position while the colour pass moved.
    // Published separately so the full-layout gate keeps its meaning.
    if (reflected && lay.bones >= 0) {
        g_pcLayBonesPartial  = lay.bones;
        g_pcLayBonesNPartial = lay.bonesN;
    } else {
        g_pcLayBonesPartial  = -1;
        g_pcLayBonesNPartial = 0;
    }
    if (!reflected || !lay.ok) {
        g_pcLayShader = key;              // remember the refusal, do not re-read every draw
        g_pcLayVp = g_pcLayL2W = g_pcLayBones = -1;
        g_pcLayBonesN = 0;
        InterlockedIncrement(&g_pcLayFail);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000,
            "pcap/layout: this shader declares no readable constant table, so "
            "placement has no register numbers and refuses. Guessing them is "
            "how a hand ends up positioned from the wrong constants.");
        return;
    }
    g_pcLayShader = key;
    g_pcLayVp = lay.vp; g_pcLayL2W = lay.localToWorld; g_pcLayBones = lay.bones;
    g_pcLayBonesN = lay.bonesN;
    // ONCE PER SHADER, not once per draw. Three or four shaders alternate
    // across the hand draws, so g_pcLayShader changes on nearly every one and
    // this line was re-reflecting and re-printing at draw rate: it produced a
    // 25 MB log in a single short run and buried the twenty lines that
    // mattered. The layout is identical every time it is read, so only a
    // shader nobody has reported yet earns a line. Nothing unbounded in a
    // per-draw path (CLAUDE.md).
    {
        static void* said[16];
        static int   saidN = 0;
        bool seen = false;
        for (int i = 0; i < saidN; i++) if (said[i] == key) { seen = true; break; }
        if (!seen) {
            if (saidN < 16) said[saidN++] = key;
            Log("pcap/layout: shader %p declares ViewProjectionMatrix c%d, "
                "BoneMatrices c%d, LocalToWorld c%d. Read from its own constant "
                "table - several shaders draw this mesh and they do not agree. "
                "Printed once per shader (%d distinct so far); the layout does "
                "not change between reads.",
                key, lay.vp, lay.bones, lay.localToWorld, saidN);
        }
    }
}


#if DVR_WITH_LEGACY
#include "legacy/vr33/palette_packet_capture.cpp"
#endif
