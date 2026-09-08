static void WiTick(void);
static bool WiOwns(int sig, int comp, uint32_t* h0, uint32_t* s0,
                   uint32_t* h1, uint32_t* s1);
static void WiFinish(void);
static void WiFinishTick(void);
static void WiNoteDraw(IDirect3DDevice9* dev, INT baseVertex, UINT minIndex,
                       UINT numVertices, UINT startIndex, UINT primCount);
