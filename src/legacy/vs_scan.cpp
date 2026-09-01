














static void DumpVSConstScan()
{
    g_scanDumps++;
    Log("scan: ---- VS-constant scan dump %d (frame %lu) ----", g_scanDumps,
        (unsigned long)g_frame);
    // rank registers by non-affine count (VP-matrix candidates)
    for (int pick = 0; pick < 8; pick++) {
        uint32_t best = 0, bestR = 0xffffffff;
        for (uint32_t r = 0; r < VS_REGS - 3; r++) {
            if (g_vsNonAffine[r] > best) { best = g_vsNonAffine[r]; bestR = r; }
        }
        if (bestR == 0xffffffff || best == 0) break;
        float* m = g_vsLast[bestR];
        Log("scan: c%-3u nonAffine=%u/%u changes=%u", bestR,
            g_vsNonAffine[bestR], g_vsCallCount[bestR], g_vsChanged[bestR]);
        Log("scan:      [% .3f % .3f % .3f % .3f]", m[0], m[1], m[2], m[3]);
        Log("scan:      [% .3f % .3f % .3f % .3f]", m[4], m[5], m[6], m[7]);
        Log("scan:      [% .3f % .3f % .3f % .3f]", m[8], m[9], m[10], m[11]);
        Log("scan:      [% .3f % .3f % .3f % .3f]", m[12], m[13], m[14], m[15]);
        g_vsNonAffine[bestR] = 0; // so the next pick finds the next register
    }
    // reset tallies for the next window
    memset(g_vsCallCount, 0, sizeof(g_vsCallCount));
    memset(g_vsNonAffine, 0, sizeof(g_vsNonAffine));
    memset(g_vsChanged, 0, sizeof(g_vsChanged));
    Log("scan: ---- end dump ----");
}
