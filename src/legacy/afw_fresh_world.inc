// Retired 2026-10-04: build 34 improved the boat slightly but regressed world ghosting.
// Offline reproduction only, included in the shader literal when DVR_WITH_LEGACY is enabled.
    // Tangential object/vehicle motion can change colour without changing depth.
    // The depth-only stale test cannot detect it. This opt-in uses current-time
    // stereo samples where available; unseen regions retain the established fill.
    "    if (prm9.w > 0.5 && okF) return shadeF(sF, 2, tF);\n"
