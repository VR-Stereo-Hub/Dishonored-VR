static bool PcCapture(IDirect3DDevice9* dev, const MsContract* con, UINT primCount,
                      int cls, const float* ourQ);
static void PcTick(void);
static void PcStart(void);
static void PcStop(void);
static bool PcCommand(const char* args);
