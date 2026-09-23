// VR-136: the camera's rain box, measured and (on request) hidden.
// Included after cinematic_trace.cpp (it reuses that module's reflected
// camera-cache layout) and after uobject/reflect.
//
// DishonoredPlayerCamera owns one Emitter actor, m_pRainBoxEmitter, whose
// particle module (DisParticleModuleRainDrops) spawns drops inside
// m_RainBoxExtent around the view. In the headset it reads as a pane in front
// of the eyes. The separate looping lens sheet is also handled (VR-199). Two parts:
//
//  1. MEASURE, always while [Rain] Trace=1: the box extent, the drop count and
//     the emitter's location in the camera's own frame (forward/right/up, uu),
//     logged on change. This is the number a near-eye placement has to be
//     designed from; nothing about it is guessed here.
//  2. HIDE, only with [Rain] Hide=1 (code default off, live `rainhide on|off`,
//     F10 Basic checkbox): native PrimitiveComponent.SetHidden on rain-named
//     looping camera lens particle components only. The native propagates to the
//     render proxy, which a raw HiddenGame write would not. The rain impacts
//     and camera rain box are untouched.
//
// LANE: the script lane (ProcessEvent), 250 ms cadence. Every sample re-reads
// the chain from the controller and re-checks liveness; the only retained
// identities are hidden components, revalidated through current owners before restore.
#include <atomic>
#include <cmath>

static std::atomic<bool> g_rainHide{false}, g_rainTrace{true};
// VR-202: opt-in native recovery correction; see ENGINE_NOTES timing derivation.
static std::atomic<bool> g_rainRecovery{false}, g_rainRecoveryPending{false};
static void RainRecoverySet(bool on) {
    g_rainRecovery.store(on);
    g_rainRecoveryPending.store(true);
    Log("rain: recovery=%d (native uncovered recovery ceiling; shelter remains native)", on ? 1 : 0);
}
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
    Log("rain: hide=%d (live; close rain lens particles only; sky rain box untouched)", on ? 1 : 0);
}
static bool RainHideEnabled() { return g_rainHide.load(); }
static bool RainTraceEnabled() { return g_rainTrace.load(); }
static void RainConfigure(const char* ini) {
    g_rainTrace.store(GetPrivateProfileIntA("Rain", "Trace", 1, ini) != 0);
    RainHideSet(GetPrivateProfileIntA("Rain", "Hide", 0, ini) != 0);
    RainDistanceSet(GetPrivateProfileIntA("Rain", "Distance", -1, ini));
    RainRecoverySet(GetPrivateProfileIntA("Rain", "Recovery", 0, ini) != 0);
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

// VR-202: read-only native weather decisions. Never force rain through shelter.
// Called at RainTick's 250 ms cadence, emits at most one summary per second.
static void RainWeatherTrace(uint8_t* cam, uint8_t* psc, const float* pos, int hidden, int drops, float kill)
{
    static RflWant w[] = {
        {"DishonoredPlayerCamera", "m_bWasUncovered", true},
        {"DishonoredPlayerCamera", "m_NumRequestedRainImpacts", false},
        {"DishonoredPlayerCamera", "m_NumAvailableRainImpacts", false},
        {"ParticleSystemComponent", "bIsActive", true},
        {"ParticleSystemComponent", "bSuppressSpawning", true},
        {"ParticleSystemComponent", "InstanceParameters", false},
        {"ParticleSysParam", "Name", false},
        {"ParticleSysParam", "ParamType", false},
        {"ParticleSysParam", "Scalar", false},
        {"ParticleSysParam", "Material", false}
    };
    static bool resolved = false;
    static uint32_t maxName = 0xffffffffu, rateName = 0xffffffffu;
    if (!resolved) {
        resolved = true;
        const int got = RflResolveBatch(w, (int)(sizeof(w) / sizeof(w[0])));
        maxName = FindNameIdx("MaxParticles"); rateName = FindNameIdx("SpawnKillRate");
        Log("rain/weather: layout %d/10 fields; uncovered=+0x%x/0x%x params=+0x%x "
            "param name/type/scalar/material=+0x%x/+0x%x/+0x%x/+0x%x stride=%u; unresolved reads=-1",
            got, w[0].off, w[0].mask, w[5].off, w[6].off, w[7].off, w[8].off, w[9].off,
            kRainParticleParamStride);
    }
    auto readInt = [&](uint8_t* obj, int k) -> int {
        if (!IsLiveObject(obj) || !w[k].found || !RangeReadable(obj + w[k].off, 4)) return -1;
        uint32_t v = 0; memcpy(&v, obj + w[k].off, 4);
        return w[k].isBool ? ((v & w[k].mask) ? 1 : 0) : (int)v;
    };
    const int uncovered = readInt(cam, 0), requested = readInt(cam, 1), available = readInt(cam, 2);
    const int active = readInt(psc, 3), suppressed = readInt(psc, 4);
    float effective = -1, rate = -1;
    const bool paramLayout = w[5].found && w[6].found && w[7].found && w[8].found && w[9].found &&
        w[6].off + 8 <= kRainParticleParamStride && w[7].off < kRainParticleParamStride &&
        w[8].off + 4 <= kRainParticleParamStride && w[9].off + sizeof(void*) == kRainParticleParamStride;
    uint8_t* data = nullptr; int32_t num = 0;
    if (paramLayout && IsLiveObject(psc) && RflArrayAt(psc, w[5].off, &data, &num) &&
        num > 0 && num <= 128 && RangeReadable(data, num * kRainParticleParamStride)) {
        for (int i = 0; i < num; ++i) {
            uint8_t* row = data + i * kRainParticleParamStride;
            uint32_t name[2]; memcpy(name, row + w[6].off, sizeof(name));
            if (name[1] || row[w[7].off] != 1) continue; // PSPT_Scalar, declared in the script
            float value; memcpy(&value, row + w[8].off, sizeof(value));
            if (!std::isfinite(value)) continue;
            if (name[0] == maxName) effective = value;
            if (name[0] == rateName) rate = value;
        }
    }
    static unsigned samples = 0, covered = 0, open = 0, zero = 0, positive = 0, unknown = 0;
    static unsigned long long nextLog = 0;
    ++samples;
    if (uncovered == 0) ++covered; else if (uncovered == 1) ++open;
    if (effective == 0) ++zero; else if (effective > 0) ++positive; else ++unknown;
    const auto now = GetTickCount64();
    if (now < nextLog) return;
    nextLog = now + 1000;
    Log("rain/weather: cam=%p psc=%p pos=(%.1f %.1f %.1f) uncovered=%d "
        "configuredDrops=%d MaxParticles=%.1f SpawnKillRate=%.2f cameraKillRate=%.2f "
        "hidden=%d active=%d suppressSpawn=%d impacts requested=%d available=%d "
        "| samples=%u open=%u covered=%u maxZero=%u maxPositive=%u maxUnknown=%u "
        "(MaxParticles=0 is native suppression; positive is not proof of drawn rain)",
        (void*)cam, (void*)psc, pos[0], pos[1], pos[2], uncovered, drops, effective, rate, kill,
        hidden, active, suppressed, requested, available, samples, open, covered, zero, positive, unknown);
    samples = covered = open = zero = positive = unknown = 0;
}

// Identify the actual update modules and their rain volume, rather than assuming
// every emitter instance in the component is a falling-rain layer.
static void RainModuleTrace(uint8_t* psc, uint8_t* instance, int index)
{
    static RflWant w[] = {
        {"ParticleLODLevel", "UpdateModules", false},
        {"DisParticleModuleRainDrops", "m_Extent", false}
    };
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        Log("rain/modules: layout %d/2", RflResolveBatch(w, 2));
    }
    uint8_t* lod = RainPtr(instance, kRainInstanceLodOff);
    uint8_t* data = nullptr; int32_t n = 0;
    if (!IsLiveObject(psc) || !IsLiveObject(lod) || !w[0].found ||
        !RflArrayAt(lod, w[0].off, &data, &n) || n < 0 || n > 64 ||
        (n && !RangeReadable(data, n * sizeof(void*)))) {
        Log("rain/modules: psc=%p layer=%d current LOD/update modules unavailable", (void*)psc, index);
        return;
    }
    for (int j = 0; j < n; ++j) {
        uint8_t* module = nullptr; memcpy(&module, data + j * sizeof(void*), sizeof(module));
        if (!IsLiveObject(module)) continue;
        const char* cls = ObjClassName(module);
        if (!cls) continue;
        if (!strcmp(cls, "DisParticleModuleRainDrops") && w[1].found &&
            RangeReadable(module + w[1].off, 12)) {
            float extent[3]; memcpy(extent, module + w[1].off, sizeof(extent));
            Log("rain/modules: psc=%p layer=%d lod=%p module=%p class=%s extent=(%.1f %.1f %.1f)",
                (void*)psc, index, (void*)lod, (void*)module, cls, extent[0], extent[1], extent[2]);
        }
    }
}

// VR-202: distinguish requested rain from live particles and moving render bounds.
// Native instances are not UObjects: validate the current live PSC's array and
// the instance's back-pointer on every read. No retained pointers and no writes.
static void RainParticleTrace(uint8_t* psc, const float* camera, const float* emitter, const int32_t* rot)
{
    static unsigned long long next = 0;
    const auto now = GetTickCount64();
    if (now < next) return;
    next = now + 1000;
    static RflWant w[] = {
        {"ParticleSystemComponent", "EmitterInstances", false},
        {"PrimitiveComponent", "Bounds", false},
        {"BoxSphereBounds", "Origin", false},
        {"BoxSphereBounds", "BoxExtent", false},
        {"PrimitiveComponent", "LastRenderTime", false},
        {"ParticleSystemComponent", "bForcedInActive", true},
        {"ParticleSystemComponent", "Template", false},
        {"ParticleSystem", "bUseFixedRelativeBoundingBox", true}
    };
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        Log("rain/particles: layout %d/8 fields (unresolved counts/flags=-1; boundsValid=0 means unavailable)",
            RflResolveBatch(w, (int)(sizeof(w) / sizeof(w[0]))));
    }
    if (!IsLiveObject(psc)) return;
    int particles = 0, valid = 0, count = -1;
    uint8_t* data = nullptr; int32_t n = 0;
    if (w[0].found && RflArrayAt(psc, w[0].off, &data, &n) && n >= 0 && n <= 32 &&
        (!n || RangeReadable(data, n * sizeof(void*)))) {
        count = n;
        for (int i = 0; i < n; ++i) {
            uint8_t* instance = nullptr;
            memcpy(&instance, data + i * sizeof(void*), sizeof(instance));
            if (!instance || ((uintptr_t)instance & 3) ||
                !RangeReadable(instance, kRainInstanceActiveCountOff + sizeof(int))) continue;
            uint8_t* owner = nullptr; int active = -1;
            memcpy(&owner, instance + kRainInstanceComponentOff, sizeof(owner));
            memcpy(&active, instance + kRainInstanceActiveCountOff, sizeof(active));
            if (owner != psc || active < 0 || active > 100000) continue;
            ++valid; particles += active;
            // Active slots can remain allocated with zero alpha. Inspect each
            // emitter separately so another layer cannot conceal that transition.
            uint8_t* records = nullptr; uint16_t* indices = nullptr; int stride = 0;
            memcpy(&records, instance + kRainInstanceDataOff, sizeof(records));
            memcpy(&indices, instance + kRainInstanceIndicesOff, sizeof(indices));
            memcpy(&stride, instance + kRainInstanceStrideOff, sizeof(stride));
            int read = 0, transparent = 0, baseZero = 0, fadingIn = 0, fadingOut = 0;
            float zMin = 1e30f, zMax = -1e30f;
            float sum = 0, minAlpha = 1e30f, maxAlpha = -1e30f;
            if (active <= 512 && stride >= (int)kRainParticleBaseAlphaOff + 4 && stride <= 65536 &&
                (!active || (records && indices && RangeReadable(indices, active * sizeof(uint16_t))))) {
                for (int j = 0; j < active; ++j) {
                    uint16_t slot = 0; memcpy(&slot, indices + j, sizeof(slot));
                    const uint64_t address = (uintptr_t)records + (uint64_t)slot * stride;
                    if (address > UINT32_MAX - kRainParticleBaseAlphaOff - 4) continue;
                    const uint8_t* particle = (const uint8_t*)(uintptr_t)address;
                    if (!RangeReadable(particle, kRainParticleBaseAlphaOff + 4)) continue;
                    float alpha = 0, base = 0, fade = 0, z = 0;
                    memcpy(&fade, particle + kRainParticleFadeOff, 4);
                    memcpy(&z, particle + kRainParticlePositionOff + 2 * sizeof(float), 4);
                    memcpy(&alpha, particle + kRainParticleAlphaOff, 4);
                    memcpy(&base, particle + kRainParticleBaseAlphaOff, 4);
                    if (!std::isfinite(alpha) || !std::isfinite(base)) continue;
                    ++read; sum += alpha;
                    if (std::isfinite(fade)) { if (fade > 0) ++fadingIn; else if (fade < 0) ++fadingOut; }
                    if (std::isfinite(z)) { if (z < zMin) zMin = z; if (z > zMax) zMax = z; }
                    if (alpha <= 0.001f) ++transparent;
                    if (base <= 0.001f) ++baseZero;
                    if (alpha < minAlpha) minAlpha = alpha;
                    if (alpha > maxAlpha) maxAlpha = alpha;
                }
            }
            Log("rain/layer: psc=%p index=%d instance=%p active=%d alphaRead=%d "
                "transparent=%d baseZero=%d alphaMin=%.4f alphaMax=%.4f alphaMean=%.4f "
                "fadeIn=%d fadeOut=%d particleZ=(%.1f %.1f) emitterZ=%.1f "
                "(CPU alpha only; read<active is incomplete, material visibility unmeasured)",
                (void*)psc, i, (void*)instance, active, read, transparent, baseZero,
                read ? minAlpha : -1, read ? maxAlpha : -1, read ? sum / read : -1,
                fadingIn, fadingOut, zMin < 1e30f ? zMin : -1, zMax > -1e30f ? zMax : -1, emitter[2]);
            RainModuleTrace(psc, instance, i);

        }
    }
    if (count < 0 || valid != count) particles = -1;
    float origin[3] = {}, extent[3] = {}, lastRender = -1;
    const bool bounds = w[1].found && w[2].found && w[3].found &&
        RangeReadable(psc + w[1].off + w[2].off, sizeof(origin)) &&
        RangeReadable(psc + w[1].off + w[3].off, sizeof(extent));
    if (bounds) {
        memcpy(origin, psc + w[1].off + w[2].off, sizeof(origin));
        memcpy(extent, psc + w[1].off + w[3].off, sizeof(extent));
    }
    if (w[4].found && RangeReadable(psc + w[4].off, 4)) memcpy(&lastRender, psc + w[4].off, 4);
    auto flag = [&](uint8_t* obj, int k) -> int {
        if (!IsLiveObject(obj) || !w[k].found || !RangeReadable(obj + w[k].off, 4)) return -1;
        uint32_t value = 0; memcpy(&value, obj + w[k].off, 4);
        return (value & w[k].mask) ? 1 : 0;
    };
    uint8_t* templ = w[6].found ? RainPtr(psc, w[6].off) : nullptr;
    Log("rain/particles: psc=%p camera=(%.1f %.1f %.1f) pitch=%.2f yaw=%.2f "
        "emitter=(%.1f %.1f %.1f) instances=%d valid=%d liveParticles=%d "
        "boundsValid=%d origin=(%.1f %.1f %.1f) extent=(%.1f %.1f %.1f) "
        "lastRenderTime=%.3f forcedInactive=%d fixedBounds=%d "
        "(live count is not proof of drawing; compare bounds and render time across pitch)",
        (void*)psc, camera[0], camera[1], camera[2], rot[0] * (360.0 / 65536.0), rot[1] * (360.0 / 65536.0),
        emitter[0], emitter[1], emitter[2], count, valid, particles, bounds ? 1 : 0,
        origin[0], origin[1], origin[2], extent[0], extent[1], extent[2], lastRender, flag(psc, 5), flag(templ, 7));
}

// The stock rate recurrence is rate *= 100 * dt until it reaches 10000.
// Above 100 Hz it decays instead of growing, starving transparent-slot recovery.
// Use the native terminal rate only while the current camera reports uncovered.
static void RainRecoveryTick(uint8_t* cam) {
    static RflWant w[] = {
        {"DishonoredPlayerCamera", "m_bWasUncovered", true},
        {"DishonoredPlayerCamera", "m_fRainSpawnKillRate", false},
        {"DishonoredPlayerCamera", "m_NumRainDrops", false}
    };
    static bool resolved = false, wasEnabled = false;
    const bool enabled = g_rainRecovery.load();
    if (!enabled && !wasEnabled) { g_rainRecoveryPending.store(false); return; }
    if (!resolved) {
        resolved = true;
        Log("rain/recovery: layout %d/3", RflResolveBatch(w, 3));
    }
    if (!IsLiveObject(cam) || !w[0].found || !w[1].found || !w[2].found ||
        !RangeReadable(cam + w[0].off, 4) || !RangeReadable(cam + w[1].off, 4) ||
        !RangeReadable(cam + w[2].off, 4)) return;
    uint32_t bits = 0; int drops = 0; float rate = 0;
    memcpy(&bits, cam + w[0].off, 4); memcpy(&rate, cam + w[1].off, 4);
    memcpy(&drops, cam + w[2].off, 4);
    if (!std::isfinite(rate) || rate < 0) return;
    if (!enabled) {
        // Re-enter the engine's normal transition recurrence on the CURRENT live
        // camera. No saved pointer/value crosses a menu or level transition.
        if (rate == kRainRecoveryCeiling) {
            const float seed = kRainRecoverySeed;
            memcpy(cam + w[1].off, &seed, 4);
            Log("rain/recovery: disabled, current camera=%p native seed restored", (void*)cam);
        }
        wasEnabled = false;
        g_rainRecoveryPending.store(false);
        return;
    }
    wasEnabled = true;
    g_rainRecoveryPending.store(false);
    if (!(bits & w[0].mask) || drops <= 0 || rate >= kRainRecoveryCeiling) return;
    const float corrected = kRainRecoveryCeiling;
    memcpy(cam + w[1].off, &corrected, 4);
    DVR_LOG_EVERY_MS(dvr::log::Cat::script, dvr::log::Level::Info, 1000,
        "rain/recovery: cam=%p uncovered=1 requested=%d rate %.6f -> %.1f (native recovery ceiling)",
        (void*)cam, drops, rate, corrected);
}

static void RainTick() {
    const bool hide = g_rainHide.load(), trace = g_rainTrace.load();
    struct HiddenRain { uint8_t* comp; uint8_t* owner; uint8_t* templ; uint32_t name[2]; };
    static HiddenRain hidden[17] = {};
    static int hiddenN = 0;
    static uint8_t* extCam = nullptr;           // the camera whose extent WE wrote
    static float extOrig[3] = {};
    const int wantDist = g_rainDistUu.load();
    if (!hide && !trace && !hiddenN && wantDist < 0 && !extCam &&
        !g_rainRecovery.load() && !g_rainRecoveryPending.load()) return;
    static unsigned long long next = 0, nextRebuild = 0;
    const unsigned long long now = GetTickCount64();
    if (now < next) return;
    next = now + 250;
    if (!RflNamesReady()) return;
    // A menu/load transition invalidates the old population, even if pointers match.
    static unsigned liveEpoch = ~0u;
    const unsigned epoch = UiSurfaceEpoch();
    if (liveEpoch != epoch) {
        if (!BuildLiveSet()) return;
        liveEpoch = epoch;
    } else if (!RefreshLiveSet(2000)) return;

    static bool resolved = false;
    static uint32_t pcCamOff = 0, emitterOff = 0, extentOff = 0, dropsOff = 0, killOff = 0, dirOff = 0,
                    pscOff = 0, locOff = 0, hiddenOff = 0, hiddenMask = 0, lensOff = 0, templateOff = 0;
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
        lensOff    = RflOffsetOf("Camera", "CameraLensEffects");
        templateOff = RflOffsetOf("ParticleSystemComponent", "Template");
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
        if (now >= nextRebuild) { nextRebuild = now + 2000; BuildLiveSet(); }   // VR-160
        emitter = nullptr;
    }
    RainRecoveryTick(cam);
    const char* ec = emitter ? ObjClassName(emitter) : nullptr;
    if (emitter && (!ec || !strstr(ec, "Emitter"))) emitter = nullptr;
    uint8_t* psc = RainPtr(emitter, pscOff);
    const char* pc = IsLiveObject(psc) ? ObjClassName(psc) : nullptr;
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

    // VR-199: the camera rain box is not the lens sheet (ENGINE_NOTES run470).
    // Enumerate the CURRENT camera array; never restore through a retained pointer alone.
    HiddenRain current[17] = {}; int currentN = 0;
    auto add = [&](uint8_t* owner, uint8_t* comp, uint8_t* templ) {
        if (!IsLiveObject(owner) || !IsLiveObject(comp) || !RangeReadable(comp + kNameOff, 8)) return;
        auto& c = current[currentN++]; c = { comp, owner, templ, {} };
        memcpy(c.name, comp + kNameOff, sizeof(c.name));
    };
    // VR-199 headset scope: preserve the native sky-rain box and its impact system.
    // Only the separately identified camera lens component belongs to this toggle.
    uint8_t* lensData = nullptr; int32_t lensN = 0;
    if (cam && lensOff && RflArrayAt(cam, lensOff, &lensData, &lensN)) {
        if (lensN > 16) lensN = 16;
        for (int i = 0; i < lensN; ++i) {
            uint8_t* fx = ((uint8_t**)lensData)[i];
            uint8_t* comp = nullptr; uint8_t* templ = nullptr;
            if (fx && !IsLiveObject(fx)) {
                // Objects created after a load must enter the live table before use.
                if (now >= nextRebuild) { nextRebuild = now + 2000; BuildLiveSet(); }
                continue;
            }
            if (!IsLiveObject(fx)) continue;
            const char* cls = ObjClassName(fx);
            if (!cls || strcmp(cls, "DisEmitterCameraLensEffect_Looping")) continue;
            comp = RainPtr(fx, pscOff);
            if (!IsLiveObject(comp)) continue;
            templ = RainPtr(comp, templateOff);
            if (!IsLiveObject(templ) || !RangeReadable(templ + kNameOff, 4)) continue;
            const char* name = RealName(*(uint32_t*)(templ + kNameOff));
            // The looping class is shared: require a rain-named particle asset too.
            bool rain = false;
            if (name) for (const char* c = name; *c; ++c) if (!_strnicmp(c, "rain", 4)) { rain = true; break; }
            if (!rain) {
                DVR_LOG_EVERY_MS(dvr::log::Cat::script, dvr::log::Level::Info, 10000,
                    "rain/lens: skipped looping effect %p template=%s (not identified as rain)",
                    (void*)fx, name ? name : "unresolved");
                continue;
            }
            if (hide && hiddenNow(comp) == 0)
                Log("rain/lens: matched template=%s owner=%p component=%p", name, (void*)fx, (void*)comp);
            add(fx, comp, templ);
        }
    }
    HiddenRain nextHidden[17] = {}; int nextN = 0;
    for (int i = 0; i < currentN; ++i) {
        const auto& c = current[i];
        bool ours = false;
        for (int j = 0; j < hiddenN; ++j) {
            const auto& h = hidden[j];
            if (h.comp == c.comp && h.owner == c.owner && h.templ == c.templ &&
                h.name[0] == c.name[0] && h.name[1] == c.name[1]) { ours = true; break; }
        }
        if (!IsLiveObject(c.comp) || !IsLiveObject(fnSetHidden) || !hiddenOff) continue;
        const int before = hiddenNow(c.comp);
        if (hide && before == 0) {
            callSetHidden(c.comp, true);
            ours = hiddenNow(c.comp) == 1;
            Log("rain: HID %s owner=%p component=%p HiddenGame=%d -> %d (visual result needs headset)",
                "rain lens", (void*)c.owner,
                (void*)c.comp, before, hiddenNow(c.comp));
        } else if (!hide && ours) {
            callSetHidden(c.comp, false);
            Log("rain: RESTORED component=%p HiddenGame=%d -> %d", (void*)c.comp, before, hiddenNow(c.comp));
            ours = hiddenNow(c.comp) != 0;
        }
        if (ours) nextHidden[nextN++] = c;
    }
    memcpy(hidden, nextHidden, sizeof(hidden)); hiddenN = nextN;
    if (hide && !currentN) DVR_LOG_EVERY_MS(dvr::log::Cat::script, dvr::log::Level::Info, 10000,
        "rain: hide armed, no live camera rain targets (camera=%p lensCount=%d)", (void*)cam, lensN);

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
    if (cam && cOk) RainWeatherTrace(cam, psc, cloc, hid, drops, kill);
    if (cOk && eOk) RainParticleTrace(psc, cloc, eloc, crot);
    const bool changed = emitter != lastEmitter || drops != lastDrops || hid != lastHidden ||
                         fabsf(extent[0] - lastExtent[0]) > 0.5f || fabsf(extent[1] - lastExtent[1]) > 0.5f ||
                         fabsf(extent[2] - lastExtent[2]) > 0.5f;
    if (changed) {
        lastEmitter = emitter; lastDrops = drops; lastHidden = hid; memcpy(lastExtent, extent, sizeof(extent));
        Log("rain/box: emitter=%p (%s) psc=%p hidden=%d drops=%d extent=(%.1f %.1f %.1f) uu dir=(%.2f %.2f %.2f) "
            "killRate=%.2f | emitter in camera frame fwd=%.1f right=%.1f up=%.1f dist=%.1f uu (%s) | 100 uu = 1 m; "
            "box visibility untouched; extent writable only with [Rain] Distance>=0",
            (void*)emitter, ec ? ec : "none", (void*)psc, hid, drops, extent[0], extent[1], extent[2],
            dir[0], dir[1], dir[2], kill, f, r, u, dist,
            eOk && cOk ? "measured" : "unavailable: -1 dist means no emitter or no camera cache");
    } else if (emitter && eOk && cOk) {
        DVR_LOG_EVERY_MS(dvr::log::Cat::script, dvr::log::Level::Info, 5000,
            "rain/box: steady - emitter in camera frame fwd=%.1f right=%.1f up=%.1f dist=%.1f uu, extent=(%.1f %.1f %.1f) drops=%d hidden=%d",
            f, r, u, dist, extent[0], extent[1], extent[2], drops, hid);
    }
}
