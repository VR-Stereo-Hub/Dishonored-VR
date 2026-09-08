        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
            "ms/palette: %s | %d range(s) | delta (%.1f %.1f %.1f) uu on class "
            "%s | c6 x%u (%u bones) | %ld per-class draw(s), %ld wanted with no "
            "c6 cached. If BOTH hands move, the per-class scoping failed and "
            "the delta is reaching the shared upload; if NEITHER moves, the "
            "palette is not what skins this mesh.",
            perClass ? "PER-CLASS" : "merged (backend off, wrong mode, or no c6 yet)",
            nrng,
            (g_mpAxis == 0) ? g_mpAmount : 0.0f,
            (g_mpAxis == 1) ? g_mpAmount : 0.0f,
            (g_mpAxis == 2) ? g_mpAmount : 0.0f,
            g_mpHand == 0 ? "A" : g_mpHand == 1 ? "B" : "BOTH",
            g_mpCacheN, g_mpCacheN / 3,
            g_mpDraws, g_mpNoCache);
