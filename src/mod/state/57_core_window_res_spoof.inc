// State chunk 57 for core/window/res_spoof - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static PFN_GetAdapterDisplayMode g_origGADM = NULL;

static UINT g_realRefresh = 0;   // the desktop's true Hz - see hkEnumAdapterModes

// 32.82: THE GAME REJECTS THE RESOLUTION, IT DOES NOT IGNORE IT.
// DishonoredEngine.ini says ResX=3200 ResY=1800 Fullscreen=False, and the game
// asks D3D for 1920x1080 FULLSCREEN - its fallback, the desktop mode. That is
// what a game does when the resolution it wants is not in the adapter's list
// of supported modes, and 3200x1800 is not a real mode on a 1080p monitor.
// GetAdapterDisplayMode was already spoofed; the MODE LIST never was, and the
// mode list is what a resolution gets validated against.
// So advertise one extra mode - ours - and the resolution the ini asks for
// becomes a resolution the game believes it can have. Nothing else has to
// pretend: it then drives its own viewport, its own render targets and its own
// backbuffer to that size, which is the whole reason this route is worth more
// than any amount of resizing after the fact.
typedef UINT    (__stdcall *PFN_GetAdapterModeCount)(IDirect3D9*, UINT, D3DFORMAT);
typedef HRESULT (__stdcall *PFN_EnumAdapterModes)(IDirect3D9*, UINT, D3DFORMAT,
                                                  UINT, D3DDISPLAYMODE*);
static PFN_GetAdapterModeCount g_origGAMC = NULL;
static PFN_EnumAdapterModes    g_origEAM  = NULL;
