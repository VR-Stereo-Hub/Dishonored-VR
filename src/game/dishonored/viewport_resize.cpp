// VR-50: apply explicit resolution requests through the engine's F11 resize path.
// Included before scene_draw.cpp. No retained viewport/UObject pointer crosses frames.
namespace {
std::atomic<int> g_resLiveState{0}; // idle,queued,calling,confirmed,awaiting capture; negative=refused
std::atomic<uint64_t> g_resLiveSize{0};
std::atomic<unsigned long long> g_resLiveStamp{0};
void ResLiveRefuse(const char* why) {
    g_resLiveState.store(-1);
    Log("res/live: REFUSED: %s; current render remains authoritative",why);
}
}
static int ResLiveState() { return g_resLiveState.load(); }
static const char* ResLiveStatus() {
    switch (ResLiveState()) {
    case 1: return "Queued for the next game-thread draw.";
    case 2: return "Engine resize in progress.";
    case 3: return "Applied: capture confirms the requested resolution.";
    case 4: return "Engine called; waiting for capture to confirm resolution.";
    case -1: return "Resize refused; see log. The displayed current size is authoritative.";
    case -2: return "Requested size not confirmed; see log before retrying.";
    default: return "Drag to preview; Set applies the selected resolution.";
    }
}
static void ResLiveQueue(uint32_t w,uint32_t h) {
    if (w<640 || h<480 || w>16384 || h>16384) { ResLiveRefuse("invalid dimensions"); return; }
    const int state=ResLiveState();
    if (state==1 || state==2 || state==4) return;
    g_resLiveSize.store(((uint64_t)w<<32)|h);
    g_resLiveStamp.store(GetTickCount64());
    g_resLiveState.store(1);
    Log("res/live: queued %ux%u; no D3D reset on overlay/render lane",w,h);
}
static void ResLivePoll() {
    int state=ResLiveState();
    if (state!=1 && state!=4) return;
    const uint64_t packed=g_resLiveSize.load();
    const uint32_t w=(uint32_t)(packed>>32),h=(uint32_t)packed;
    if (state==4 && dvr::capture::width()==w && dvr::capture::height()==h) {
        if (!g_resLiveState.compare_exchange_strong(state,3)) return;
        Log("res/live: CONFIRMED capture %ux%u; inspect XR swapchain and FOV for full acceptance",w,h);
    } else if (GetTickCount64()-g_resLiveStamp.load()>10000) {
        if (!g_resLiveState.compare_exchange_strong(state,-2)) return;
        Log("res/live: NOT CONFIRMED state=%d requested=%ux%u capture=%ux%u after10s; no automatic retry",state,w,h,dvr::capture::width(),dvr::capture::height());
    }
}
static void ResLiveApply(void* viewport) {
    int queued=1;
    if (!g_resLiveState.compare_exchange_strong(queued,2)) return;
    const uint64_t packed=g_resLiveSize.load();
    const uint32_t w=(uint32_t)(packed>>32),h=(uint32_t)packed;
    if (!g_gameWnd || GetWindowThreadProcessId(g_gameWnd,nullptr)!=GetCurrentThreadId()) {
        ResLiveRefuse("draw is not on the game window thread"); return;
    }
    auto* view=(uint8_t*)viewport;
    if ((uintptr_t)view<kWindowsFViewportBase || !RangeReadable(view-kWindowsFViewportBase,kWindowsViewportPosY+4)) {
        ResLiveRefuse("native viewport unreadable"); return;
    }
    auto* native=view-kWindowsFViewportBase;
    if (*(uintptr_t*)view!=kWindowsFViewportVtable || *(uintptr_t*)native!=kWindowsViewportVtable ||
        *(HWND*)(native+kWindowsViewportHwnd)!=g_gameWnd ||
        !RangeReadable((void*)kWindowsViewportVtable,8) ||
        *(uintptr_t*)(kWindowsViewportVtable+sizeof(void*))!=kWindowsViewportResize ||
        !RangeReadable((void*)kWindowsViewportResize,sizeof(kWindowsViewportResizePrologue)) ||
        memcmp((void*)kWindowsViewportResize,kWindowsViewportResizePrologue,sizeof(kWindowsViewportResizePrologue)) ||
        !RangeReadable((void*)kWindowsViewportResizeReturn,3) ||
        memcmp((void*)kWindowsViewportResizeReturn,"\xc2\x18\x00",3)) {
        ResLiveRefuse("viewport type/window or resize ABI bytes mismatch"); return;
    }
    // FViewport is native, not a UObject. Validate its current owning UObject
    // against a freshly rebuilt live table and the exact Viewport field.
    if (!BuildLiveSet() || !RangeReadable((void*)kGObjHdr,12)) {
        ResLiveRefuse("live-object table refresh failed"); return;
    }
    auto** objects=*(uint8_t***)kGObjHdr;
    const uint32_t count=*(uint32_t*)(kGObjHdr+4);
    if (count<2000 || count>4000000 || !RangeReadable(objects,(size_t)count*sizeof(void*))) {
        ResLiveRefuse("current object array invalid"); return;
    }
    uint8_t* owner=nullptr;
    for (uint32_t i=0;i<count;++i) {
        auto* object=objects[i];
        if (!object || !RangeReadable(object,kGameViewportNativeViewport+sizeof(void*)) ||
            *(void**)(object+kGameViewportNativeViewport)!=viewport || !IsLiveObject(object)) continue;
        const char* name=ObjClassName(object);
        if (name && strstr(name,"ViewportClient")) {
            if (owner) { ResLiveRefuse("ambiguous live viewport owners"); return; }
            owner=object;
        }
    }
    if (!owner || !IsLiveObject(owner)) { ResLiveRefuse("no live GameViewportClient owner"); return; }
    // Persist/advertise before resize, so the engine can validate the new mode.
    g_resVirtual=true;
    ResRequest(w,h,true,"F10 live total-pixel scale");
    if (!IsLiveObject(owner) || *(void**)(owner+kGameViewportNativeViewport)!=viewport ||
        *(uintptr_t*)view!=kWindowsFViewportVtable || *(HWND*)(native+kWindowsViewportHwnd)!=g_gameWnd) {
        ResLiveRefuse("viewport ownership changed before call"); return;
    }
    const int option=(*(uint32_t*)(view+kFViewportFlags)>>1)&1;
    const int x=*(int*)(native+kWindowsViewportPosX),y=*(int*)(native+kWindowsViewportPosY);
    Log("res/live: engine resize %ux%u owner=%p viewport=%p thread=%lu option=%d; before stereo tags",w,h,owner,viewport,GetCurrentThreadId(),option);
    // Six stack args, ret24. The native routine owns window/RHI synchronization.
    using ResizeFn=void(__fastcall*)(void*,void*,uint32_t,uint32_t,int,int,int,int);
    ((ResizeFn)kWindowsViewportResize)(native,nullptr,w,h,1,option,x,y);
    g_resLiveStamp.store(GetTickCount64());
    g_resLiveState.store(4);
    Log("res/live: engine returned; awaiting capture %ux%u (return alone is not acceptance)",w,h);
}
