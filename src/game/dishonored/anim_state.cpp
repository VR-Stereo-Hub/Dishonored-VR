// VR-88: included after reflect.cpp in the unity TU. Engine reads only.
#include "game/dishonored/anim_state.h"
#include "game/dishonored/anim_policy.h"
namespace dvr::anim {
namespace {
SRWLOCK lock = SRWLOCK_INIT;
SRWLOCK sampleLock = SRWLOCK_INIT;
Snapshot published;
Handoff handoff;
Handoff classifier;
bool watch = true, handback = true;
unsigned releaseMs = 250, blendMs = 150;
char masterRules[1024] = "StatePlayerMasterAssassinate,StatePlayerMasterChoke,StatePlayerMasterMantle,StatePlayerMasterClimb,StatePlayerMasterStunned,StatePlayerMasterDead,StatePlayerMasterPrePossess,StatePlayerMasterPossess,StatePlayerMasterMinigame";
char upperRules[512] = "StatePlayerGenericFatality,StatePlayerGrabCorpse";
uint32_t pawnFsm[3], currentOff, idOff, pendingOff, bodyOff, compOff;
uint32_t historyOff, newestOff, seqOff, pickerOff, controllerPawnOff;
bool resolved = false;
bool sequenceLayout = false;
uint8_t* lastPawn = nullptr;
uint8_t* lastState[3] = {}, *lastClass[3] = {};
uint8_t* lastPending = nullptr, *lastComp = nullptr;
uint32_t lastSequence[3] = {};
int lastSequenceIndex = -1;
unsigned long long nextRead = 0, nextBeat = 0;
unsigned weightFrame = ~0u;
float frameWeight = 1;
void text(char* dst, size_t n, const char* src) { _snprintf_s(dst,n,_TRUNCATE,"%s",src ? src : "unknown"); }
bool read(uint8_t* obj, uint32_t off, void* dst, size_t n) {
    if (!obj || !off || !RangeReadable(obj+off,n)) return false;
    memcpy(dst,obj+off,n); return true;
}
uint8_t* object(uint8_t* obj,uint32_t off) {
    uint8_t* out = nullptr;
    return read(obj,off,&out,sizeof(out)) && IsLiveObject(out) && RangeReadable(out,kNameOff+8) ? out : nullptr;
}
const char* objectName(uint8_t* obj) {
    return obj && RangeReadable(obj+kNameOff,8) ? RealName(*(uint32_t*)(obj+kNameOff)) : nullptr;
}
void resolve() {
    if (resolved || !RflNamesReady()) return;
    const char* props[] = {"m_pPlayerMasterFSM","m_pPlayerUpperFSM","m_pPlayerLeftArmFSM"};
    for (int i=0;i<3;++i) pawnFsm[i]=RflOffsetOf("DishonoredPlayerPawn",props[i]);
    controllerPawnOff=RflOffsetOf("Controller","Pawn");
    currentOff=RflOffsetOf("DishonoredNativeStateMachine","m_pCurrentState");
    idOff=RflOffsetOf("DishonoredNativeStateMachine","m_pCurrentStateID");
    pendingOff=RflOffsetOf("DishonoredNativeStateMachine","m_pPendingStateID");
    bodyOff=RflOffsetOf("DishonoredPlayerPawn","m_BodyMode");
    compOff=RflOffsetOf("DishonoredPlayerPawn","m_pAnimStateComp");
    historyOff=RflOffsetOf("DisAnimStateComponent","m_AnimSeqHistory");
    newestOff=RflOffsetOf("DisAnimStateComponent","m_iCurNewestSeqInHistory");
    // Struct member offset zero is valid; FindPropOffset uses zero for a miss.
    // Verify the struct layout through its reflected picker offset before use.
    sequenceLayout=FindPropOffsetChecked("DisAnimStateSeqHistory","m_SeqName",&seqOff) &&
        FindPropOffsetChecked("DisAnimStateSeqHistory","m_StatePicker",&pickerOff) &&
        seqOff==0 && pickerOff==sizeof(uint32_t)*2;
    resolved=true;
    Log("anim: resolved FSM offsets %x/%x/%x current=%x id=%x; history=%x picker=%x (optional)",pawnFsm[0],pawnFsm[1],pawnFsm[2],currentOff,idOff,historyOff,pickerOff);
}
void report(const Snapshot& s) {
    Log("anim: gen=%u %s master=%s upper=%s left=%s pending=%s body=%d seq=%s picker=%d reason=%s age=%llu ms",
        s.generation,!s.valid?"UNKNOWN":s.game?"GAME":"PLAYER",s.state[0],s.state[1],s.state[2],s.pending,s.bodyMode,s.sequence,s.picker,s.reason,GetTickCount64()-s.stamp);
}
}
Snapshot snapshot() {
    AcquireSRWLockShared(&lock); Snapshot s=published;
    if (!watch || !fresh(s.stamp,GetTickCount64())) { s.valid=false; s.game=false; text(s.reason,sizeof(s.reason),"disabled or stale"); }
    ReleaseSRWLockShared(&lock); return s;
}
bool enabled() { AcquireSRWLockShared(&lock); bool on=handback; ReleaseSRWLockShared(&lock); return on; }
void set_enabled(bool on) {
    AcquireSRWLockExclusive(&lock); handback=on; handoff=Handoff{}; ReleaseSRWLockExclusive(&lock);
    Log("anim: HandBack=%d (live)",on?1:0);
}
float weight() {
    AcquireSRWLockExclusive(&lock);
    const auto now=GetTickCount64();
    const bool valid=watch && handback && published.valid && fresh(published.stamp,now);
    const unsigned frame=(unsigned)dvr::frame::count();
    if (!valid) { frameWeight=1; weightFrame=~0u; }
    else if (weightFrame!=frame) { frameWeight=handoff.value(now,blendMs); weightFrame=frame; }
    const float w=frameWeight;
    ReleaseSRWLockExclusive(&lock); return w;
}
bool active() { const Snapshot s=snapshot(); return enabled() && s.valid && s.game; }
bool native_draw() { return weight()<=0.0001f; }
void tick() {
    if (!TryAcquireSRWLockExclusive(&sampleLock)) return;
    struct Unlock { ~Unlock() { ReleaseSRWLockExclusive(&sampleLock); } } unlock;
    const auto now=GetTickCount64();
    if (now<nextRead) return;
    nextRead=now+10; // bounded script-lane sample, not a per-property scan
    AcquireSRWLockShared(&lock); bool on=watch; Snapshot s=published; ReleaseSRWLockShared(&lock);
    if (!on) return;
    resolve();
    Snapshot previous=s;
    s.stamp=now; ++s.generation; s.valid=false; s.game=false;
    text(s.reason,sizeof(s.reason),"unresolved or pawn unavailable");
    uint8_t* pawn=IsLiveObject(g_peCtrl)?object(g_peCtrl,controllerPawnOff):nullptr;
    const bool pawnChanged=pawn!=lastPawn;
    if (pawnChanged) {
        lastPawn=pawn; memset(lastState,0,sizeof(lastState)); memset(lastClass,0,sizeof(lastClass));
        lastPending=nullptr; lastComp=nullptr; lastSequenceIndex=-1;
        s=Snapshot{}; s.stamp=now; s.generation=previous.generation+1;
    }
    if (pawn && currentOff && idOff && pawnFsm[0] && pawnFsm[1] && pawnFsm[2]) {
        s.valid=true;
        for (int i=0;i<3;++i) {
            uint8_t* fsm=object(pawn,pawnFsm[i]);
            uint8_t* state=fsm?object(fsm,currentOff):nullptr;
            uint8_t* cls=fsm?object(fsm,idOff):nullptr;
            s.stateAddress[i]=(unsigned long long)(uintptr_t)state;
            if (!state || !cls) { s.valid=false; text(s.state[i],sizeof(s.state[i]),"unknown"); continue; }
            // Verify every current class identity; resolve its text only on change.
            uint8_t* actual=object(state,kClassOff);
            if (actual!=cls) { s.valid=false; text(s.reason,sizeof(s.reason),"state/class ID mismatch"); continue; }
            if (state!=lastState[i] || cls!=lastClass[i] || !previous.valid) {
                const char* name=objectName(cls);
                if (!name) { s.valid=false; continue; }
                text(s.state[i],sizeof(s.state[i]),name); s.entered[i]=now;
                lastState[i]=state; lastClass[i]=cls;
            }
            if (i==0) {
                uint8_t* pending=object(fsm,pendingOff);
                if (pending!=lastPending || !previous.valid) text(s.pending,sizeof(s.pending),pending?objectName(pending):"none");
                lastPending=pending;
            }
        }
        unsigned char body=0;
        s.bodyMode=read(pawn,bodyOff,&body,1) && body<=2 ? body : -1;
        uint8_t* comp=object(pawn,compOff); int index=-1;
        if (comp!=lastComp) { lastComp=comp; lastSequenceIndex=-1; }
        // UE3 FName is two dwords; this reflected struct is FName + int.
        if (comp && sequenceLayout && historyOff && read(comp,newestOff,&index,4) && index>=0 && index<12) {
            uint32_t entry[3];
            if (read(comp,historyOff+index*(pickerOff+sizeof(int32_t)),entry,sizeof(entry))) {
                if (index!=lastSequenceIndex || memcmp(entry,lastSequence,sizeof(entry)) || !previous.valid) {
                    const char* n=RealName(entry[0]);
                    _snprintf_s(s.sequence,sizeof(s.sequence),_TRUNCATE,"%s#%u",n?n:"unknown",entry[1]);
                    memcpy(lastSequence,entry,sizeof(entry)); lastSequenceIndex=index;
                    s.sequenceAt=now;
                }
                s.picker=(int)entry[2];
            }
        } else { s.picker=-1; text(s.sequence,sizeof(s.sequence),"unavailable"); }
    }
    AcquireSRWLockExclusive(&lock);
    if (pawnChanged || !previous.valid || !fresh(previous.stamp,now)) { handoff=Handoff{}; classifier=Handoff{}; }
    const bool match=listed(masterRules,s.state[0]) || listed(upperRules,s.state[1]);
    classifier.update(s.valid,match,watch,now,releaseMs,0);
    handoff.update(s.valid,classifier.game,watch && handback,now,0,blendMs);
    // StateWatch still reports the classifier with HandBack disabled.
    s.game=s.valid && classifier.game;
    if (s.valid) text(s.reason,sizeof(s.reason),listed(masterRules,s.state[0])?"master rule":listed(upperRules,s.state[1])?"upper rule":classifier.game?"release hysteresis":"unlisted state");
    published=s;
    ReleaseSRWLockExclusive(&lock);
    if (s.valid!=previous.valid || s.game!=previous.game || memcmp(s.state,previous.state,sizeof(s.state)) || s.bodyMode!=previous.bodyMode || strcmp(s.sequence,previous.sequence) || now>=nextBeat) {
        report(s); nextBeat=now+5000;
    }
}
void configure(const char* ini) {
    AcquireSRWLockExclusive(&lock);
    const int watchSetting=GetPrivateProfileIntA("Anim","StateWatch",-1,ini);
    const int backSetting=GetPrivateProfileIntA("Anim","HandBack",-1,ini);
    watch=watchSetting!=0;
    handback=backSetting!=0;
    releaseMs=(unsigned)GetPrivateProfileIntA("Anim","ReleaseMs",250,ini); if(releaseMs>5000) releaseMs=5000;
    blendMs=(unsigned)GetPrivateProfileIntA("Anim","HandBackBlendMs",150,ini); if(blendMs>2000) blendMs=2000;
    char buf[1024];
    GetPrivateProfileStringA("Anim","HandBackMaster",masterRules,buf,sizeof(buf),ini); text(masterRules,sizeof(masterRules),buf);
    GetPrivateProfileStringA("Anim","HandBackUpper",upperRules,buf,sizeof(buf),ini); text(upperRules,sizeof(upperRules),buf);
    handoff=Handoff{};
    Log("config: [Anim] StateWatch=%d (%s) HandBack=%d (%s) ReleaseMs=%u HandBackBlendMs=%u",watch,watchSetting<0?"shipped default":"ini",handback,backSetting<0?"shipped default":"ini",releaseMs,blendMs);
    Log("config: [Anim] master rules=%s | upper rules=%s",masterRules,upperRules);
    ReleaseSRWLockExclusive(&lock);
}
bool command(const char* args) {
    char sub[24]={}, value[24]={}; sscanf(args,"%23s %23s",sub,value);
    if (!strcmp(sub,"handback") && (!strcmp(value,"on") || !strcmp(value,"off"))) set_enabled(!strcmp(value,"on"));
    else if (!strcmp(sub,"watch") && (!strcmp(value,"on") || !strcmp(value,"off"))) {
        AcquireSRWLockExclusive(&lock); watch=!strcmp(value,"on"); published.valid=false; handoff=Handoff{}; ReleaseSRWLockExclusive(&lock);
    } else if (*sub && strcmp(sub,"status")) Log("anim: status | watch on|off | handback on|off");
    report(snapshot()); return true;
}
void status(dvr::status::Writer& w) {
    const Snapshot s=snapshot(); w.obj("anim"); w.kv("valid",s.valid); w.kv("gameOwnsBody",s.game); w.kv("handBack",enabled());
    w.kv("master",s.state[0]); w.kv("upper",s.state[1]); w.kv("left",s.state[2]); w.kv("sequence",s.sequence);
    w.kv("reason",s.reason); w.kv("bodyMode",s.bodyMode); w.kv("controllerWeight",(double)weight()); w.end_obj();
}
// Rotation interpolation is implemented separately in the pure math helper.
hf::Xform blend(const hf::Xform& transform) { return blend_transform(transform,weight()); }
}
