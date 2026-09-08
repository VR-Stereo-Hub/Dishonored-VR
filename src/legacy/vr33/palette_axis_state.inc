static int      g_mpAxis    = 1;       // [Hands] PaletteAxis  0/1/2
static float    g_mpAmount  = 12.0f;   // [Hands] PaletteAmount, unreal units
static int      g_mpHand    = 0;       // [Hands] PaletteHand  0=A 1=B 2=both
static bool     g_mpStep      = false;  // [Hands] PaletteStep
static int      g_mpStepAxis  = -1;     // -1 = resting, 0/1/2 = that axis
static volatile LONG g_mpStepReq = 0;   // hotkey -> MsTick, present thread
