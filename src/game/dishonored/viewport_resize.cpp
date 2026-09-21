// VR-50: apply explicit resolution requests through the engine's F11 resize path.
// Included before scene_draw.cpp. No retained viewport/UObject pointer crosses frames.
namespace {
std::atomic<int> g_resLiveState{0}; // idle,queued,calling,confirmed,awaiting capture; negative=refused
std::atomic<uint64_t> g_resLiveSize{0};
std::atomic<unsigned long long> g_resLiveStamp{0};
// VR-158: the fullscreen flag the queued resize will ask for. The engine's
// Resize has always taken one; until now it was the literal 1, so nothing
// could switch a running game to windowed and back. Ships 1, the shipped
// behaviour. The device Reset this resize provokes is also what makes a
// vsync change take: present_tick.cpp's DvrBeforeReset runs UncapPresent,
// which reads g_forceNoVSync at that moment.
std::atomic<int> g_resLiveFull{1};
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
    Log("res/live: queued %ux%u %s; no D3D reset on overlay/render lane",
        w,h,g_resLiveFull.load()?"fullscreen":"windowed");
}

// VR-158, corrected 2026-09-20 by a measured run.
//
// `ResLiveFullscreen()` used to return `!g_gameWindowed` - the device's real
// state - on the argument that a control must show the device and not the wish.
// That reasoning was right and the reading was useless, because under
// VirtualMode the device is windowed BY DESIGN and can never read otherwise:
//
//   res: CreateDevice - VirtualMode: the game asked FULLSCREEN 2750x2850 (our
//   advertised mode); creating it WINDOWED with the backbuffer kept
//
// Two faults followed, both measured in one run. The F10 checkbox could never
// stay ticked (tick it, next frame it reads the device, snaps back). And
// `ResLiveSetVsync` fed that same reading into the resize as its fullscreen
// argument, so a vsync toggle asked for a WINDOWED 2750x2850 where VR-50 had
// always passed 1 - the engine clamped it to the desktop and the render
// collapsed to 1355x1405.
//
// So: the resize argument comes from what was ASKED (`g_resLiveFull`, still
// defaulting to 1), and the device's own state is reported separately and
// named for what it is.
static bool ResLiveWantFullscreen() { return g_resLiveFull.load()!=0; }
static bool ResLiveDeviceWindowed() { return g_gameWindowed; }
// True when the proxy is the reason the device is windowed, rather than the ask.
static bool ResLiveWindowedByVirtualMode() { return g_resVirtual && g_gameWindowed; }
static bool ResLiveFullscreen() { return ResLiveWantFullscreen(); }

// VR-158: ask for a live fullscreen change at the CURRENT render size. It
// rides the whole guarded path the resolution control already proved: same
// window-thread check, same viewport ABI byte verification, same live-owner
// validation, same refusal reasons. A refused switch leaves the running
// device exactly as it was and says why.
static void ResLiveSetFullscreen(bool full,const char* who) {
    const uint32_t w=dvr::capture::width(),h=dvr::capture::height();
    if (!w || !h) {
        Log("res/live: fullscreen %s REFUSED (%s) - capture reports no size yet (%ux%u)",
            full?"on":"off",who,w,h);
        return;
    }
    const int state=ResLiveState();
    if (state==1 || state==2 || state==4) {
        Log("res/live: fullscreen %s REFUSED (%s) - a resize is already in flight (state=%d)",
            full?"on":"off",who,state);
        return;
    }
    // MEASURED 2026-09-20: a windowed ask larger than the desktop is silently
    // clamped by the engine, and the render collapsed 2750x2850 -> 1355x1405
    // with only a NOT CONFIRMED line ten seconds later to show for it. Refuse
    // it up front and name both sizes, because losing the render resolution is
    // a worse outcome than not switching.
    if (!full) {
        const int dw=GetSystemMetrics(SM_CXSCREEN),dh=GetSystemMetrics(SM_CYSCREEN);
        if (dw>0 && dh>0 && ((int)w>dw || (int)h>dh)) {
            Log("res/live: fullscreen off REFUSED (%s) - %ux%u does not fit the desktop %dx%d, and a "
                "windowed ask is clamped to it (this is what collapsed the render to 1355x1405). "
                "Lower the render size first if you want the windowed leg.",who,w,h,dw,dh);
            return;
        }
    }
    Log("res/live: fullscreen %s asked by %s; asked state was %s, device is %s at %ux%u%s - "
        "re-entering the resize path",
        full?"on":"off",who,ResLiveWantFullscreen()?"fullscreen":"windowed",
        ResLiveDeviceWindowed()?"windowed":"fullscreen",w,h,
        ResLiveWindowedByVirtualMode()
            ? " (windowed BY VIRTUALMODE, not by the ask - the proxy creates the advertised "
              "fullscreen mode windowed, so the device can never read fullscreen while it is on)"
            : "");
    g_resLiveFull.store(full?1:0);
    ResLiveQueue(w,h);
}

// VR-158: vsync is NOT live on its own. UncapPresent runs only at CreateDevice
// and Reset, so the flag has to be followed by a device event.
//
// CORRECTED 2026-09-20 after the first run, which found two faults:
//
//  1. This used to pass the DEVICE's fullscreen state into the resize. Under
//     VirtualMode that reads windowed always, so a vsync toggle asked for a
//     windowed 2750x2850, the engine clamped it to the desktop, and the render
//     collapsed to 1355x1405. It now leaves the fullscreen ask alone entirely -
//     changing vsync must not change anything else.
//  2. Clearing ForceNoVSync did not turn vsync ON. This game asks for
//     IMMEDIATE itself, and the old UncapPresent returned at its first line
//     when the flag was off, so the ON leg was never forced and the device kept
//     running uncapped. `g_vsyncWant` now forces both directions.
static void ResLiveSetVsync(bool vsyncOn,const char* who) {
    const int want=vsyncOn?1:0;
    if (g_vsyncWant==want) {
        Log("perf: vsync %s asked by %s - already forced that way; no reset provoked",
            vsyncOn?"on":"off",who);
        return;
    }
    g_vsyncWant=want;
    g_forceNoVSync=!vsyncOn;          // kept in step so the ini and cfg dump agree
    const uint32_t w=dvr::capture::width(),h=dvr::capture::height();
    Log("perf: vsync %s asked by %s -> vsyncWant=%d ForceNoVSync=%d; provoking a device reset at "
        "%ux%u WITHOUT touching the fullscreen ask (%s). The reset's own present-interval line is "
        "the evidence; this line is only the request",
        vsyncOn?"on":"off",who,g_vsyncWant,(int)g_forceNoVSync,w,h,
        ResLiveWantFullscreen()?"fullscreen":"windowed");
    ResLiveSetFullscreen(ResLiveWantFullscreen(),"vsync change");
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
    const bool wantFull=g_resLiveFull.load()!=0;
    g_resVirtual=true;
    ResRequest(w,h,wantFull,"F10 live total-pixel scale");
    if (!IsLiveObject(owner) || *(void**)(owner+kGameViewportNativeViewport)!=viewport ||
        *(uintptr_t*)view!=kWindowsFViewportVtable || *(HWND*)(native+kWindowsViewportHwnd)!=g_gameWnd) {
        ResLiveRefuse("viewport ownership changed before call"); return;
    }
    const int option=(*(uint32_t*)(view+kFViewportFlags)>>1)&1;
    const int x=*(int*)(native+kWindowsViewportPosX),y=*(int*)(native+kWindowsViewportPosY);
    Log("res/live: engine resize %ux%u %s owner=%p viewport=%p thread=%lu option=%d; before stereo tags",
        w,h,wantFull?"fullscreen":"windowed",owner,viewport,GetCurrentThreadId(),option);
    // Six stack args, ret24. The native routine owns window/RHI synchronization.
    // VR-158: arg 3 is the fullscreen flag. It was the literal 1 from VR-50
    // until now, which is why nothing could switch a running game to windowed.
    using ResizeFn=void(__fastcall*)(void*,void*,uint32_t,uint32_t,int,int,int,int);
    ((ResizeFn)kWindowsViewportResize)(native,nullptr,w,h,wantFull?1:0,option,x,y);
    g_resLiveStamp.store(GetTickCount64());
    g_resLiveState.store(4);
    Log("res/live: engine returned; awaiting capture %ux%u (return alone is not acceptance; "
        "the device is %s after the call)",w,h,g_gameWindowed?"windowed":"fullscreen");
}
