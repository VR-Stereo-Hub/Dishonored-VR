// tools/frame_test - the VR-33 rotation/grip frame math, on the desk.
//
// Runs hand_frame_test.h's suite and exits non-zero if any case fails. NO mod
// code, no D3D, no OpenXR - it includes exactly the header the proxy includes,
// so a pass here is a statement about the shipped arithmetic and not about a
// re-derivation of it.
//
//   .\build\src\RelWithDebInfo\frame_test.exe
//
// With capture packets as arguments it also REPLAYS them through the same
// decomposition the draw path uses, which is the only offline way to ask "is
// the dominant slot of a real palette actually a rotation?" of real data:
//
//   .\build\src\RelWithDebInfo\frame_test.exe D:\dvr-data\dumps\pcap_*.txt
//
// The proxy runs the synthetic suite at init and writes the result to the log,
// so the tester never has to run any of this.

#include "../../game/dishonored/hands/hand_frame.h"
#include "../../game/dishonored/hands/hand_frame_test.h"
#include "../../game/dishonored/hands/weapon_frame_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void report(void* ctx, const char* name, bool pass, const char* detail)
{
    (void)ctx;
    printf("%-32s %s  %s\n", name, pass ? "PASS" : "**FAIL**", detail);
}

// ---- packet replay ----------------------------------------------------------

enum { PK_MAXC = 512, PK_MAXA = 32 };

struct Packet {
    float c[PK_MAXC][4];
    bool  have[PK_MAXC];
    int   an;
    int   abi[PK_MAXA][4];
    float abw[PK_MAXA][4];
    bool  ok;
};

static bool pk_load(const char* path, Packet* p)
{
    memset(p, 0, sizeof(*p));
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == 'c' && line[1] >= '0' && line[1] <= '9') {
            int idx; float a, b, cc, d;
            if (sscanf(line, "c%d %f %f %f %f", &idx, &a, &b, &cc, &d) == 5 &&
                idx >= 0 && idx < PK_MAXC) {
                p->c[idx][0] = a; p->c[idx][1] = b; p->c[idx][2] = cc; p->c[idx][3] = d;
                p->have[idx] = true;
            }
        } else if (strncmp(line, "anchorV", 7) == 0 && p->an < PK_MAXA) {
            int vi, bi[4]; float pos[3], bw[4];
            // anchorV <i> pos x y z bi a b c d bw w x y z
            if (sscanf(line, "anchorV %d pos %f %f %f bi %d %d %d %d bw %f %f %f %f",
                       &vi, &pos[0], &pos[1], &pos[2], &bi[0], &bi[1], &bi[2], &bi[3],
                       &bw[0], &bw[1], &bw[2], &bw[3]) == 12) {
                for (int k = 0; k < 4; k++) {
                    p->abi[p->an][k] = bi[k];
                    p->abw[p->an][k] = bw[k];
                }
                p->an++;
            }
        }
    }
    fclose(f);
    p->ok = (p->an > 0 && p->have[6]);
    return p->ok;
}

// The same choice the split freezes at build time: the slot carrying the most
// weight across the anchor patch.
static int pk_dominant(const Packet* p, float* fracOut)
{
    float w[256]; memset(w, 0, sizeof(w));
    float tot = 0.0f;
    for (int a = 0; a < p->an; a++)
        for (int k = 0; k < 4; k++) {
            const int b = p->abi[a][k];
            if (p->abw[a][k] > 0.0f && b >= 0 && b < 256)
                { w[b] += p->abw[a][k]; tot += p->abw[a][k]; }
        }
    int best = -1; float bw = 0.0f;
    for (int b = 0; b < 256; b++) if (w[b] > bw) { bw = w[b]; best = b; }
    if (fracOut) *fracOut = (tot > 0.0f) ? bw / tot : 0.0f;
    return best;
}

static int replay(int argc, char** argv)
{
    printf("\npacket replay - the dominant slot through the SHIPPED decomposition\n");
    printf("---------------------------------------------------------------\n");
    int files = 0, ok = 0, refused = 0, slotVaried = 0, firstSlot = -1;
    float loScale = 1e9f, hiScale = -1e9f, worstAniso = 0.0f, worstOrtho = 0.0f;
    dvr::hf::Mat3 firstR; bool haveFirst = false;
    float worstMove = 0.0f;
    for (int i = 1; i < argc; i++) {
        Packet p;
        if (!pk_load(argv[i], &p)) { printf("%-28s could not read\n", argv[i]); continue; }
        files++;
        float frac = 0.0f;
        const int slot = pk_dominant(&p, &frac);
        if (firstSlot < 0) firstSlot = slot; else if (slot != firstSlot) slotVaried++;
        const int base = 6 + slot * 3;
        if (slot < 0 || base + 2 >= PK_MAXC || !p.have[base + 2]) {
            printf("%-28s slot %d not in the captured constants\n", argv[i], slot);
            refused++; continue;
        }
        dvr::hf::Mat3 m;
        for (int c = 0; c < 3; c++) {
            m.m[0*3+c] = p.c[base+0][c];
            m.m[1*3+c] = p.c[base+1][c];
            m.m[2*3+c] = p.c[base+2][c];
        }
        dvr::hf::ScaledRot sr;
        if (!dvr::hf::decompose_scaled_rotation(m, 0.02f, 0.02f, &sr)) {
            printf("%-28s slot %d REFUSED by the decomposition\n", argv[i], slot);
            refused++; continue;
        }
        ok++;
        if (sr.scale < loScale) loScale = sr.scale;
        if (sr.scale > hiScale) hiScale = sr.scale;
        if (sr.aniso > worstAniso) worstAniso = sr.aniso;
        if (sr.ortho > worstOrtho) worstOrtho = sr.ortho;
        if (!haveFirst) { firstR = sr.r; haveFirst = true; }
        else {
            const float d = dvr::hf::rotation_diff_deg(firstR, sr.r);
            if (d > worstMove) worstMove = d;
        }
    }
    printf("---------------------------------------------------------------\n");
    printf("%d packet(s): %d decomposed, %d refused. dominant slot %d",
           files, ok, refused, firstSlot);
    printf(slotVaried ? " (VARIED in %d packet(s))\n" : " (the same in all)\n", slotVaried);
    if (ok) {
        printf("uniform scale %.9f .. %.9f | worst anisotropy %.3e | worst "
               "orthonormality residual %.3e\n", loScale, hiScale, worstAniso, worstOrtho);
        printf("the frame moved at most %.4f deg across these packets\n", worstMove);
        if (worstMove < 0.01f)
            printf("NOTE: near-zero movement means these captures are all one pose. "
                   "It does NOT show the frame tracks the palm - only a run in "
                   "which the game animates the hand can show that.\n");
    }
    return (files && refused == files) ? 1 : 0;
}

int main(int argc, char** argv)
{
    printf("VR-33 hand frame math\n");
    printf("---------------------------------------------------------------\n");
    const int failed = dvr::hf::test::run_all(report, NULL);
    printf("---------------------------------------------------------------\n");
    printf("%s\n", failed ? "FAILURES" : "all cases passed");
    int rc = (failed + WeaponFrameTests()) ? 1 : 0;
    if (argc > 1) rc |= replay(argc, argv);
    return rc;
}
