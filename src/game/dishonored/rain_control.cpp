// VR-136: the camera's rain box, measured and (on request) hidden.
// Included after cinematic_trace.cpp (it reuses that module's reflected
// camera-cache layout) and after uobject/reflect.
//
// DishonoredPlayerCamera owns one Emitter actor, m_pRainBoxEmitter, whose
// particle module (DisParticleModuleRainDrops) spawns drops inside
// m_RainBoxExtent around the view. In the headset it reads as a pane in front
// of the eyes. Two parts:
//
//  1. MEASURE, always while [Rain] Trace=1: the box extent, the drop count and
//     the emitter's location in the camera's own frame (forward/right/up, uu),
//     logged on change. This is the number a near-eye placement has to be
//     designed from; nothing about it is guessed here.
//  2. HIDE, only with [Rain] Hide=1 (code default off, live `rainhide on|off`,
//     F10 checkbox): the engine's own native PrimitiveComponent.SetHidden on
//     that one emitter's ParticleSystemComponent. The native propagates to the
//     render proxy, which a raw HiddenGame write would not. The rain impacts
//     and every other particle system are untouched.
//
// LANE: the script lane (ProcessEvent), 250 ms cadence. Every sample re-reads
// the chain from the controller and re-checks liveness; the only retained
// pointer is the component we hid, and it is revalidated before a restore.
#include <atomic>
#include <cmath>

static std::atomic<bool> g_rainHide{false}, g_rainTrace{true};
// VR-136: the rain slab's distance. The camera re-places m_pRainBoxEmitter every
// frame at camLoc + viewForward * t, t = min over axes of m_RainBoxExtent / |f|
// (the view ray's exit from that box; build458 steady samples: fwd 500..660 uu,
// right ~0, up ~0). The extent's only other read is the debug box draw. So the
// extent IS the slab's distance: writing it moves the rain toward the eyes, and
// 0 centres it on the head. < 0 = native, untouched.
static std::atomic<int> g_rainDistUu{-1};
static void RainDistanceSet(int uu) {
    if (uu > 2000) uu = 2000;
    g_rainDistUu.store(uu < 0 ? -1 : uu);
    Log("rain: distance=%d uu (%s; the emitter is placed this far ahead along the view)",
        uu < 0 ? -1 : uu, uu < 0 ? "native m_RainBoxExtent, untouched" : "written to m_RainBoxExtent each sample");
}
static int RainDistance() { return g_rainDistUu.load(); }

static void RainHideSet(bool on) {
    g_rainHide.store(on);
    Log("rain: hide=%d (live; only the camera's rain box emitter, via native SetHidden)", on ? 1 : 0);
}
static bool RainHideEnabled() { return g_rainHide.load(); }
static bool RainTraceEnabled() { return g_rainTrace.load(); }
static void RainConfigure(const char* ini) {
    g_rainTrace.store(GetPrivateProfileIntA("Rain", "Trace", 1, ini) != 0);
    RainHideSet(GetPrivateProfileIntA("Rain", "Hide", 0, ini) != 0);
    RainDistanceSet(GetPrivateProfileIntA("Rain", "Distance", -1, ini));
}

// A UFunction by (declaring class, name). FindFunctionObj matches the name
// alone and returns the first; SetHidden exists on Actor AND PrimitiveComponent
// with different parameter blocks, so the outer is checked.
static uint8_t* RainFindClassFunction(const char* clsName, const char* fnName) {
    const uint32_t ci = FindNameIdx(clsName), fi = FindNameIdx(fnName);
    if (ci == 0xffffffffu || fi == 0xffffffffu || !RangeReadable((void*)kGObjHdr, 12)) return nullptr;
    void** objs = *(void***)kGObjHdr;
    const uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) return nullptr;
    for (uint32_t i = 0; i < onum; i++) {
        if ((i & 1023) == 0) {
            uint32_t left = onum - i;
            if (left > 1024) left = 1024;
            if (!RangeReadable(objs + i, left * sizeof(void*))) break;
        }
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x80)) continue;
        if (*(uint32_t*)(o + kNameOff) != fi) continue;
        uint8_t* ou = *(uint8_t**)(o + kOuterOff);
        if (!ou || ((uintptr_t)ou & 3) || !RangeReadable(ou, kNameOff + 4)) continue;
        if (*(uint32_t*)(ou + kNameOff) != ci) continue;
        const char* cn = ObjClassName(o);
        if (cn && !strcmp(cn, "Function")) return o;
    }
    return nullptr;
}

static uint8_t* RainPtr(uint8_t* o, uint32_t off) {
    uint8_t* v = nullptr;
    if (!o || !off || !RangeReadable(o + off, sizeof(v))) return nullptr;
    memcpy(&v, o + off, sizeof(v));
    return v;
}

static void RainTick() {
    const bool hide = g_rainHide.load(), trace = g_rainTrace.load();
    static uint8_t* hidComp = nullptr;          // the one component WE hid
    static uint8_t* extCam = nullptr;           // the camera whose extent WE wrote
    static float extOrig[3] = {};
    const int wantDist = g_rainDistUu.load();
    if (!hide && !trace && !hidComp && wantDist < 0 && !extCam) return;
    static unsigned long long next = 0, nextRebuild = 0;
    const unsigned long long now = GetTickCount64();
    if (now < next) return;
    next = now + 250;
    if (!RflNamesReady()) return;

    static bool resolved = false;
    static uint32_t pcCamOff = 0, emitterOff = 0, extentOff = 0, dropsOff = 0, killOff = 0, dirOff = 0,
                    pscOff = 0, locOff = 0, hiddenOff = 0, hiddenMask = 0;
    static uint8_t* fnSetHidden = nullptr;
    if (!resolved) {
        resolved = true;
        pcCamOff   = RflOffsetOf("PlayerController", "PlayerCamera");
        emitterOff = RflOffsetOf("DishonoredPlayerCamera", "m_pRainBoxEmitter");
        extentOff  = RflOffsetOf("DishonoredPlayerCamera", "m_RainBoxExtent");
        dropsOff   = RflOffsetOf("DishonoredPlayerCamera", "m_NumRainDrops");
        killOff    = RflOffsetOf("DishonoredPlayerCamera", "m_fRainSpawnKillRate");
        dirOff     = RflOffsetOf("DishonoredPlayerCamera", "m_RainDirection");
        pscOff     = RflOffsetOf("Emitter", "ParticleSystemComponent");
        locOff     = RflOffsetOf("Actor", "Location");
        FindBoolProp("PrimitiveComponent", "HiddenGame", &hiddenOff, &hiddenMask);
        fnSetHidden = RainFindClassFunction("PrimitiveComponent", "SetHidden");
        Log("rain: layout pcCamera=+0x%x emitter=+0x%x extent=+0x%x drops=+0x%x killRate=+0x%x dir=+0x%x "
            "psc=+0x%x location=+0x%x HiddenGame=+0x%x/0x%x SetHidden=%p%s",
            pcCamOff, emitterOff, extentOff, dropsOff, killOff, dirOff, pscOff, locOff, hiddenOff, hiddenMask,
            (void*)fnSetHidden,
            (pcCamOff && emitterOff && pscOff && hiddenOff && fnSetHidden) ? "" : "  <-- a hide needs every field; it will refuse");
    }

    uint8_t* ctrl = IsLiveObject(g_peCtrl) ? g_peCtrl : nullptr;
    uint8_t* cam = RainPtr(ctrl, pcCamOff);
    if (cam && (!IsLiveObject(cam) || !ObjClassName(cam) || !strstr(ObjClassName(cam), "PlayerCamera"))) cam = nullptr;
    uint8_t* emitter = RainPtr(cam, emitterOff);
    if (emitter && !IsLiveObject(emitter)) {
        // An emitter spawned after the table was built. Bounded rebuild, then
        // the next sample decides; never trust it this sample.
        if (now >= nextRebuild) { nextRebuild = now + 2000; BuildLiveSet(); }
        emitter = nullptr;
    }
    const char* ec = emitter ? ObjClassName(emitter) : nullptr;
    if (emitter && (!ec || !strstr(ec, "Emitter"))) emitter = nullptr;
    uint8_t* psc = RainPtr(emitter, pscOff);
    const char* pc = psc ? ObjClassName(psc) : nullptr;
    if (psc && (!IsLiveObject(psc) || !pc || !strstr(pc, "ParticleSystemComponent"))) psc = nullptr;

    auto hiddenNow = [&](uint8_t* comp) -> int {
        if (!comp || !hiddenOff || !RangeReadable(comp + hiddenOff, 4)) return -1;
        return (*(uint32_t*)(comp + hiddenOff) & hiddenMask) ? 1 : 0;
    };
    auto callSetHidden = [&](uint8_t* comp, bool v) {
        struct { uint32_t NewHidden; } parms = { v ? 1u : 0u };
        g_peReentry = true;
        ((PFN_ProcessEventCall)kProcessEvent)(comp, fnSetHidden, &parms, NULL);
        g_peReentry = false;
    };

    // Restore first: the lever went off, or the emitter we hid is no longer
    // the camera's. A component that is no longer live is dropped, never written.
    // A restore writes ONLY to the component this sample reached through the
    // live controller -> camera -> emitter chain. Anything else (a level change,
    // a new emitter) is dropped without a write: the table can outlive a level,
    // so IsLiveObject alone does not prove a retained pointer still exists.
    if (hidComp && (!hide || hidComp != psc)) {
        if (hidComp == psc && fnSetHidden) {
            callSetHidden(psc, false);
            Log("rain: RESTORED component %p (hidden now %d; hide turned off)", (void*)psc, hiddenNow(psc));
        } else {
            Log("rain: released component %p without a write - it is no longer the camera's rain emitter", (void*)hidComp);
        }
        hidComp = nullptr;
    }
    if (hide && psc && fnSetHidden && hiddenOff) {
        const int before = hiddenNow(psc);
        if (before == 0) {
            callSetHidden(psc, true);
            const int after = hiddenNow(psc);
            hidComp = psc;
            Log("rain: HID the camera's rain box %p component %p via native SetHidden: HiddenGame %d -> %d "
                "(a verified write, not yet an honoured one: the headset says whether the pane is gone)",
                (void*)emitter, (void*)psc, before, after);
        }   // already hidden and not by us: the engine's state, never claimed or restored
    } else if (hide) {
        static const char* lastRefusal = nullptr;
        const char* why = !fnSetHidden || !hiddenOff ? "SetHidden/HiddenGame unresolved"
                        : !cam ? "no live player camera" : !emitter ? "no live rain emitter (no rain here)"
                        : "no live particle component";
        if (why != lastRefusal) { Log("rain: hide armed, nothing hidden: %s", why); lastRefusal = why; }
    }

    // The distance lever. The native value is captured from the camera the
    // first time it is written and restored only to that same camera, reached
    // through the live chain this sample; a changed camera is released unwritten.
    if (extCam && extCam != cam) {
        Log("rain: released camera %p without restoring its extent - it is no longer the live player camera", (void*)extCam);
        extCam = nullptr;
    }
    if (cam && extentOff && RangeReadable(cam + extentOff, 12)) {
        float* ext = (float*)(cam + extentOff);
        if (wantDist >= 0) {
            if (!extCam) {
                memcpy(extOrig, ext, sizeof(extOrig)); extCam = cam;
                Log("rain: distance lever took camera %p: native extent (%.1f %.1f %.1f) -> %d uu", (void*)cam,
                    extOrig[0], extOrig[1], extOrig[2], wantDist);
            }
            const float d = (float)wantDist;
            if (ext[0] != d || ext[1] != d || ext[2] != d) {
                static unsigned long long rewrites = 0;
                // The first write is expected; later ones mean the engine reset it.
                if (++rewrites > 1) DVR_LOG_EVERY_MS(dvr::log::Cat::script, dvr::log::Level::Info, 10000,
                    "rain: extent was reset by the engine to (%.1f %.1f %.1f); rewritten to %d uu (%llu writes)",
                    ext[0], ext[1], ext[2], wantDist, rewrites);
                ext[0] = ext[1] = ext[2] = d;
            }
        } else if (extCam == cam) {
            memcpy(ext, extOrig, sizeof(extOrig));
            Log("rain: distance lever off - restored native extent (%.1f %.1f %.1f)", extOrig[0], extOrig[1], extOrig[2]);
            extCam = nullptr;
        }
    }

    if (!trace) return;
    // The measurement: rain box geometry in the camera's own frame.
    float extent[3] = {-1, -1, -1}, dir[3] = {}, kill = -1, eloc[3] = {}, cloc[3] = {};
    int drops = -1; int32_t crot[3] = {};
    if (cam) {
        if (extentOff && RangeReadable(cam + extentOff, 12)) memcpy(extent, cam + extentOff, 12);
        if (dirOff && RangeReadable(cam + dirOff, 12)) memcpy(dir, cam + dirOff, 12);
        if (dropsOff && RangeReadable(cam + dropsOff, 4)) memcpy(&drops, cam + dropsOff, 4);
        if (killOff && RangeReadable(cam + killOff, 4)) memcpy(&kill, cam + killOff, 4);
    }
    const bool eOk = emitter && locOff && RangeReadable(emitter + locOff, 12) && (memcpy(eloc, emitter + locOff, 12), true);
    const bool cOk = cam && g_ctLayout && g_ctCache &&
        RangeReadable(cam + g_ctCache + g_ctPov + g_ctLoc, 12) && RangeReadable(cam + g_ctCache + g_ctPov + g_ctRot, 12) &&
        (memcpy(cloc, cam + g_ctCache + g_ctPov + g_ctLoc, 12), memcpy(crot, cam + g_ctCache + g_ctPov + g_ctRot, 12), true);
    float f = 0, r = 0, u = 0, dist = -1;
    if (eOk && cOk) {
        const double k = 3.14159265358979 / 32768.0, p = crot[0] * k, y = crot[1] * k;
        const double d[3] = { eloc[0] - cloc[0], eloc[1] - cloc[1], eloc[2] - cloc[2] };
        const double fw[3] = { cos(p) * cos(y), cos(p) * sin(y), sin(p) };
        const double rt[3] = { -sin(y), cos(y), 0 };
        const double up[3] = { fw[1] * rt[2] - fw[2] * rt[1], fw[2] * rt[0] - fw[0] * rt[2], fw[0] * rt[1] - fw[1] * rt[0] };
        f = (float)(d[0] * fw[0] + d[1] * fw[1] + d[2] * fw[2]);
        r = (float)(d[0] * rt[0] + d[1] * rt[1] + d[2] * rt[2]);
        u = (float)(d[0] * up[0] + d[1] * up[1] + d[2] * up[2]);   // fw x rt is +Z at zero pitch (X fwd, Y right, Z up)
        dist = (float)sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    }
    static uint8_t* lastEmitter = (uint8_t*)1;
    static float lastExtent[3] = {}; static int lastDrops = -2, lastHidden = -2;
    const int hid = hiddenNow(psc);
    const bool changed = emitter != lastEmitter || drops != lastDrops || hid != lastHidden ||
                         fabsf(extent[0] - lastExtent[0]) > 0.5f || fabsf(extent[1] - lastExtent[1]) > 0.5f ||
                         fabsf(extent[2] - lastExtent[2]) > 0.5f;
    if (changed) {
        lastEmitter = emitter; lastDrops = drops; lastHidden = hid; memcpy(lastExtent, extent, sizeof(extent));
        Log("rain/box: emitter=%p (%s) psc=%p hidden=%d drops=%d extent=(%.1f %.1f %.1f) uu dir=(%.2f %.2f %.2f) "
            "killRate=%.2f | emitter in camera frame fwd=%.1f right=%.1f up=%.1f dist=%.1f uu (%s) | 100 uu = 1 m; "
            "read-only unless [Rain] Hide=1",
            (void*)emitter, ec ? ec : "none", (void*)psc, hid, drops, extent[0], extent[1], extent[2],
            dir[0], dir[1], dir[2], kill, f, r, u, dist,
            eOk && cOk ? "measured" : "unavailable: -1 dist means no emitter or no camera cache");
    } else if (emitter && eOk && cOk) {
        DVR_LOG_EVERY_MS(dvr::log::Cat::script, dvr::log::Level::Info, 5000,
            "rain/box: steady - emitter in camera frame fwd=%.1f right=%.1f up=%.1f dist=%.1f uu, extent=(%.1f %.1f %.1f) drops=%d hidden=%d",
            f, r, u, dist, extent[0], extent[1], extent[2], drops, hid);
    }
}
