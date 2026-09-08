    // VR-33 step 2: capture this qualified draw BEFORE any per-hand palette
    // modification, so the packet records the state the GAME asked for. The
    // qualifier above has already run and its contract is in `con` - the
    // capture never calls MsDraw to find out whether it should fire, because
    // MsDraw draws.
    if (g_pcOn && g_pcWant > 0 && g_mpPalN && g_mpCacheN == g_mpPalN) {
        float ourQ[3];
        const int cls = MS_CLS_HAND_A;
        if (MpAnchorPos(cls, g_mpCache, g_mpCacheN, ourQ)) {
            if (PcCapture(dev, &con, primCount, cls, ourQ)) g_pcWant--;
        } else {
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 3000,
                "pcap: the anchor REFUSED at a qualified draw, so no packet was "
                "taken - a capture whose own claimed palm position could not be "
                "computed would be constants with nothing to check them against.");
        }
    }

