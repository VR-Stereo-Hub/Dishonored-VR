// game/dishonored/block_state.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static float BlockFingerprint(const float* blk, uint32_t count)
{
    uint32_t bones = count / 3; if (bones > 64) bones = 64;
    float s = 0;
    for (uint32_t b = 0; b < bones; b++) {
        const float* m = blk + b * 12;
        float t0 = m[3], t1 = m[7], t2 = m[11];
        if (t0 != t0 || t1 != t1 || t2 != t2) return 0.0f;
        s += fabsf(t0) + fabsf(t1) + fabsf(t2);
    }
    return s;
}


static void BlockCfgLoad()
{
    char ini[MAX_PATH];
    _snprintf(ini, MAX_PATH, "%s\\dishonored_vr.ini", g_dir);
    static char buf[4096];
    DWORD n = GetPrivateProfileSectionA("WeaponBlocks", buf, sizeof(buf), ini);
    if (!n) return;
    g_savedN = 0;
    for (const char* p = buf; *p && g_savedN < 32; p += strlen(p) + 1) {
        unsigned reg = 0, cnt = 0;
        char mode[16] = {0};
        unsigned hi = 0, lo = 0;
        float fp = 0.0f;
        if (sscanf(p, "c%ux%u=%15[^,],0x%8x%8x,%f", &reg, &cnt, mode, &hi, &lo, &fp) < 3)
            continue;
        SavedBlk* s = &g_saved[g_savedN++];
        s->reg = reg; s->count = cnt; s->fp = fp;
        s->mode = (!strcmp(mode,"left")) ? BM_LEFT : (!strcmp(mode,"right")) ? BM_RIGHT
                : (!strcmp(mode,"hidden")) ? BM_HIDE : BM_OFF;
        s->hideMask = ((uint64_t)hi << 32) | (uint64_t)lo;
    }
    Log("wpnblk: loaded %d saved block assignment(s)", g_savedN);
}


static void BlockCfgSave()
{
    char ini[MAX_PATH];
    _snprintf(ini, MAX_PATH, "%s\\dishonored_vr.ini", g_dir);
    static char buf[4096];
    size_t off = 0;
    for (int i = 0; i < g_catN && off < sizeof(buf) - 96; i++) {
        if (g_blk[i].mode == BM_OFF && !g_blk[i].hideMask) continue;
        const char* mn = g_blk[i].mode == BM_LEFT ? "left" :
                         g_blk[i].mode == BM_RIGHT ? "right" :
                         g_blk[i].mode == BM_HIDE ? "hidden" : "off";
        off += (size_t)_snprintf(buf + off, sizeof(buf) - off - 2,
                    "c%ux%u=%s,0x%08x%08x,%.1f", g_cat[i].reg, g_cat[i].count, mn,
                    (unsigned)(g_blk[i].hideMask >> 32),
                    (unsigned)(g_blk[i].hideMask & 0xffffffffu), g_blk[i].fp);
        buf[off++] = '\0';
    }
    buf[off++] = '\0';
    WritePrivateProfileSectionA("WeaponBlocks", buf, ini);
    Log("wpnblk: assignments saved to the ini");
}


// 38.23 THE CROUCH WALL - root cause and fix. The full evidence chain:
// keyboard flights (hand system inactive) crawl clean; HOME (writers
// zeroed) crawls clean; hands on = wedging, hit-or-miss by hand position;
// heartbeats during stuck runs show the BONE drive idle (0 writes, stale
// target) - the live writer is the FIRST-PERSON MESH TRACKING, which
// translates the pawn's own mesh COMPONENTS (pMesh/Skm_Player body, the
// sword) to follow the controllers. A translated collision-bearing
// component is a wall we drag around with our own hands: the game's
// movement sweeps (non-zero-extent) hit it under furniture. The flat game
// never moves these meshes, so it never has the problem. Fix: clear
// BlockNonZeroExtent + BlockActors on exactly the driven components, once
// per collect - they can then never block movement, standing or crouched,
// with hands looking and behaving IDENTICALLY. Zero-extent traces (hit
// detection) stay untouched. If the flags turn out already clear, the log
// says so on every candidate and this build changed nothing - the evidence
// speaks either way.
static void FpNoBlock(uint8_t* comp, const char* nm)
{
    static int resolved = 0;
    static uint32_t oNZ = 0, mNZ = 0, oBA = 0, mBA = 0;
    if (!resolved) {
        resolved = 1;
        FindBoolProp("PrimitiveComponent", "BlockNonZeroExtent", &oNZ, &mNZ);
        FindBoolProp("PrimitiveComponent", "BlockActors", &oBA, &mBA);
        Log("collision: BlockNonZeroExtent %s (+0x%x mask %08x), "
            "BlockActors %s (+0x%x mask %08x)",
            oNZ ? "found" : "NOT FOUND", oNZ, mNZ,
            oBA ? "found" : "NOT FOUND", oBA, mBA);
    }
    if (!comp || ((uintptr_t)comp & 3)) return;
    struct { uint32_t off, mask; const char* what; } f[2] = {
        { oNZ, mNZ, "BlockNonZeroExtent" },
        { oBA, mBA, "BlockActors" },
    };
    for (int k = 0; k < 2; k++) {
        if (!f[k].off || !RangeReadable(comp + f[k].off, 4)) continue;
        uint32_t v = *(uint32_t*)(comp + f[k].off);
        if (v & f[k].mask) {
            *(uint32_t*)(comp + f[k].off) = v & ~f[k].mask;
            Log("collision: %s CLEARED on '%s' - a hand-driven mesh can "
                "no longer block movement", f[k].what, nm);
        } else {
            static int told = 0;
            if (told++ < 8)
                Log("collision: %s already clear on '%s'", f[k].what, nm);
        }
    }
}


// ==== 34.7: hunt the BLOCK state property ================================
// Blocking fires no named script event (measured: a session of standing and
// crouched blocks logged zero Versus/block events), so the trim fix needs
// the pawn's own state instead: walk GObjects once and log every Bool/Byte
// property whose name smells like blocking, declared on a combat-ish class.
// The next build polls the winner and keys a standing-block trim off it.
static void BlockPropHunt()
{
    if (!RangeReadable((void*)kGObjHdr, 12)) return;
    void** objs = *(void***)kGObjHdr;
    uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) return;
    int logged = 0;
    Log("blockhunt: walking %u objects for block-ish properties", onum);
    for (uint32_t i = 0; i < onum && logged < 60; i++) {
        if ((i & 1023) == 0) {
            uint32_t left = onum - i;
            if (left > 1024) left = 1024;
            if (!RangeReadable(objs + i, left * sizeof(void*))) break;
        }
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x80)) continue;
        const char* pc = ObjClassName(o);
        if (!pc) continue;
        bool isBool = !strcmp(pc, "BoolProperty");
        if (!isBool && strcmp(pc, "ByteProperty") && strcmp(pc, "IntProperty"))
            continue;
        const char* pn = NameFromIndex(*(uint32_t*)(o + kNameOff));
        if (!pn) continue;
        if (!(strstr(pn, "Block") || strstr(pn, "block") ||
              strstr(pn, "Parry") || strstr(pn, "Guard") ||
              strstr(pn, "Defen") || strstr(pn, "Riposte") ||
              strstr(pn, "Versus")))
            continue;
        uint8_t* ou = *(uint8_t**)(o + kOuterOff);
        if (!ou || ((uintptr_t)ou & 3) || !RangeReadable(ou, kNameOff + 4))
            continue;
        const char* on = NameFromIndex(*(uint32_t*)(ou + kNameOff));
        if (!on) continue;
        // combat-ish outers only; the plain "Block*" collision flags on
        // engine classes are noise
        if (!(strstr(on, "Dis") || strstr(on, "Pawn") || strstr(on, "Player") ||
              strstr(on, "Weapon") || strstr(on, "Sword") ||
              strstr(on, "Melee") || strstr(on, "Combat") ||
              strstr(on, "Fight")))
            continue;
        if (isBool)
            Log("blockhunt: %s.%s (Bool) off=0x%x mask=0x%08x",
                on, pn, *(uint32_t*)(o + 0x5c), *(uint32_t*)(o + 0x6c));
        else
            Log("blockhunt: %s.%s (%s) off=0x%x", on, pn, pc,
                *(uint32_t*)(o + 0x5c));
        logged++;
    }
    Log("blockhunt: done, %d candidate(s)", logged);
}


// 34.9: poll the player's block button (10 Hz - it's a held state). Offset
// resolved from reflection once; blockhunt measured 0x62e on
// DishonoredPlayerController.m_bBlockButton.
static void BlockStateTick()
{
    static double nextMs = 0.0;
    double bnow = MaimNowMs();
    if (bnow < nextMs) return;
    nextMs = bnow + 100.0;
    static uint32_t off = 0;
    static int      tried = 0;
    if (!tried) {
        tried = 1;
        off = FindPropOffset("DishonoredPlayerController", "m_bBlockButton");
        Log("block: m_bBlockButton at controller+0x%x%s", off,
            off ? " (blockhunt measured 0x62e)"
                : "  <-- NOT FOUND, block trim disabled");
    }
    if (!off) return;
    bool held = false;
    if (FindPlayerController() && g_pcObj && RangeReadable(g_pcObj + off, 1))
        held = *(uint8_t*)(g_pcObj + off) != 0;
    if (held != g_blockHeld) {
        g_blockHeld = held;
        Log("block: %s (crouched=%d -> %s trim)", held ? "HELD" : "released",
            (int)g_pawnCrouched,
            !held ? "stand" : (g_pawnCrouched ? "crouch (no block offset)"
                                              : "stand+BLOCK offset"));
    }
}
