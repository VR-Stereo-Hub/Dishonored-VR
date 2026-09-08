    else if (g_mpOn && g_mpStep)
        Log("config: [Hands] Palette=1 PaletteStep=1 - the palette's STEPPED "
            "axis probe is armed and starts at REST, so nothing moves until "
            "you ask. F6 advances rest -> axis 0 -> axis 1 -> axis 2 "
            "-> rest, %+.1f uu on hand class %s each time, with the other class "
            "never moving as the reference. Every press prints an "
            "ms/palette/step: line, so which axis was live is never inferred.",
            g_mpAmount,
            g_mpHand == 0 ? "A (left)" : g_mpHand == 1 ? "B (right)" : "BOTH");
    else if (g_mpOn)
        Log("config: [Hands] Palette=1 - the draw-scoped bone palette is ARMED. "
            "Hand class %s takes a %.1f uu delta on axis %d and the other class "
            "takes none, so the OTHER HAND IS THE CONTROL. Read the "
            "ms/palette: lines.",
            g_mpHand == 0 ? "A" : g_mpHand == 1 ? "B" : "BOTH (no control)",
            g_mpAmount, g_mpAxis);
