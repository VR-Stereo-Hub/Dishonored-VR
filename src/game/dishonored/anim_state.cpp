#include "core/framework/render_profile.h"
#include <algorithm>
// VR-88/VR-134: reads player states; optionally rejects native action requests before entry.
#include "game/dishonored/anim_state.h"
#include "game/dishonored/anim_policy.h"
#include "game/dishonored/stereo_state_policy.h"
namespace dvr::anim {
namespace {
SRWLOCK lock = SRWLOCK_INIT;
SRWLOCK sampleLock = SRWLOCK_INIT;
Snapshot published;
Handoff handoff;
Handoff classifier;
Handoff cameraClassifier;
bool watch = true, handback = true, cinematicHandback = false, mantleHandback = false;
char rulesIni[MAX_PATH]={};
struct ArmOverrides { int values[armRuleCount]; ArmOverrides(){for(int& v:values)v=-1;} } armOverrides;
bool actionAllowed[armRuleCount];
std::atomic<unsigned> disabledActions{0};
float viewRightCm=0;
dvr::hooks::Detour actionDetour;
uintptr_t actionResume=kAnimRequestState+sizeof(kAnimRequestStateBytes);
unsigned releaseMs = 250, blendMs = 150;
// Mantle is controlled independently by MantleHandBack. The previous controller
// preference remains the default; the current cinematic-comfort test enables it.
char masterRules[1024] = "StatePlayerMasterAssassinate,StatePlayerMasterChoke,StatePlayerMasterClimb,StatePlayerMasterStunned,StatePlayerMasterDead,StatePlayerMasterPrePossess,StatePlayerMasterPossess,StatePlayerMasterMinigame";
char upperRules[512] = "StatePlayerGenericFatality,StatePlayerGrabCorpse";
uint32_t pawnFsm[3], currentOff, idOff, pendingOff, bodyOff, compOff;
uint32_t historyOff, newestOff, seqOff, pickerOff, controllerPawnOff;
uint32_t dialogOff = 0;
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
bool frameArms = false;
void text(char* dst, size_t n, const char* src) { _snprintf_s(dst,n,_TRUNCATE,"%s",src ? src : "unknown"); }
bool read(uint8_t* obj, uint32_t off, void* dst, size_t n) {
    if (!obj || !off || !RangeReadable(obj+off,n)) return false;
    memcpy(dst,obj+off,n); return true;
}
// The live-object table is a SNAPSHOT, rebuilt only by the controller scan, and
// that scan usually runs at the main menu, before the level's pawn exists. So a
// real pawn, machine or state can be missing from it for the whole level, and
// every read here would fail inert without anything being wrong. A plausible
// object absent from the table marks the table stale; tick() rebuilds it.
bool staleTable = false;
uint32_t tableRebuilds = 0;
unsigned long long nextRebuild = 0;
uint8_t* object(uint8_t* obj,uint32_t off) {
    uint8_t* out = nullptr;
    if (!read(obj,off,&out,sizeof(out)) || !out) return nullptr;
    if (IsLiveObject(out) && RangeReadable(out,kNameOff+8)) return out;
    if (LooksLikeObj(out)) staleTable = true;
    return nullptr;
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
    dialogOff=RflOffsetOf("StatePlayerMasterChoice_Base","m_DialogState");
    Log("anim: reflected choice-state enum offset=%x (0=unresolved)",dialogOff);
    resolved=true;
    Log("anim: resolved FSM offsets %x/%x/%x current=%x id=%x; history=%x picker=%x (optional)",pawnFsm[0],pawnFsm[1],pawnFsm[2],currentOff,idOff,historyOff,pickerOff);
}
// VR-111: read the native drop decision without altering combat eligibility.
// This diagnostic is opt-in and bounded; cached addresses must still occupy
// their current object slot and belong to this sample's live player pawn.
bool dropWatch=false;
void drop_sample(uint8_t* pawn,const Snapshot& s) {
    if (!dropWatch || !s.valid || !pawn) return;
    static uint32_t ownerOff=0,statusOff=0,typeOff=0,targetOff=0,tagOff=0,velocityOff=0;
    static uint32_t scan=0,slot=0;
    static uint8_t* context=nullptr;
    static unsigned long long next=0,beat=0;
    const auto now=GetTickCount64();
    if (now<next) return;
    next=now+20;
    if (!ownerOff) {
        ownerOff=RflOffsetOf("DisItemContext_DropAssassinate","m_pPlayerOwner");
        statusOff=RflOffsetOf("DisItemContext","m_ContextStatus");
        typeOff=RflOffsetOf("DisItemContext_DropAssassinate","m_CachedDropType");
        targetOff=RflOffsetOf("DisItemContext_DropAssassinate","m_pCachedTarget");
        tagOff=RflOffsetOf("DisItemContext_DropAssassinate","m_TickTagAtWhichCacheIsValid");
        velocityOff=RflOffsetOf("Actor","Velocity");
    }
    if (!ownerOff || !statusOff || !typeOff || !targetOff || !tagOff || !velocityOff ||
        !RangeReadable((void*)kGObjHdr,12)) return;
    auto** objects=*(uint8_t***)(kGObjHdr);
    const uint32_t count=*(uint32_t*)(kGObjHdr+4);
    if (!objects || !count || count>4000000) return;
    if (context && (slot>=count || !RangeReadable(objects+slot,sizeof(void*)) ||
        objects[slot]!=context || !IsLiveObject(context) || object(context,ownerOff)!=pawn)) context=nullptr;
    for (unsigned budget=0;!context && budget<1024;++budget) {
        if (scan>=count) {scan=0;break;}
        const uint32_t i=scan++;
        if (!RangeReadable(objects+i,sizeof(void*))) break;
        auto* candidate=objects[i];
        if (!IsLiveObject(candidate)) continue;
        const char* cls=ObjClassName(candidate);
        if (!cls || strcmp(cls,"DisItemContext_DropAssassinate")) continue;
        if (object(candidate,ownerOff)!=pawn) continue;
        context=candidate;slot=i;
        Log("drop/watch: current player context discovered slot=%u",slot);
    }
    unsigned char status=255,type=255; int tag=-1; uint8_t* target=nullptr;float velocity[3]={};
    bool known=context && read(context,statusOff,&status,1) && status<4 &&
        read(context,typeOff,&type,1) && type<3 && read(context,tagOff,&tag,4) &&
        read(context,targetOff,&target,sizeof(target)) && read(pawn,velocityOff,velocity,sizeof(velocity));
    static int previous=-1;
    const int key=known?status*8+type:-1;
    const bool falling=!strcmp(s.state[0],"StatePlayerMasterFalling");
    if (key!=previous || now>=beat) {
        Log("drop/watch: known=%d status=%u (0 idle 1 failed 2 active 3 finished) type=%u "
            "(0 no-drop 1 too-high 2 do-now) cacheTick=%d targetLive=%d velocity=%.1f/%.1f/%.1f "
            "master=%s upper=%s; cached native decision, not a forced attack",
            known,(unsigned)status,(unsigned)type,tag,target && IsLiveObject(target),
            velocity[0],velocity[1],velocity[2],s.state[0],s.state[1]);
        previous=key;beat=now+(falling?100:1000);
    }
}
bool default_arm_rule(int lane,const char* state) {
    return (lane==0 && ((mantleHandback && !strcmp(state,"StatePlayerMasterMantle")) ||
        (cinematicHandback && dvr::scene_state::cinematic(state)) || listed(masterRules,state))) ||
        (lane==1 && listed(upperRules,state));
}
bool resolve_arm_rule(int lane,const char* state) {
    const int i=arm_rule_index(lane,state);
    return arm_rule_value(i>=0?armOverrides.values[i]:-1,default_arm_rule(lane,state));
}
void arm_rule_key(int i,char* key,size_t n) {
    _snprintf_s(key,n,_TRUNCATE,"Arms.%d.%s",armRules[i].lane,armRules[i].state);
}
// Reject at the request boundary, before pending state, entry callbacks or locks change.
// Unknown/stale ownership always runs the original request. Fresh slot membership is
// checked only for a disabled player action, not for the ordinary engine request path.
bool __cdecl reject_action(uint8_t* fsm,uint8_t* request) {
    if(!disabledActions.load() || !request || !RangeReadable(request,kAnimRequestClassOff+sizeof(void*)))return false;
    const Snapshot s=snapshot();
    if(!s.valid || !fresh(s.stamp,GetTickCount64()) || !IsLiveObject(g_peCtrl) || !IsLiveObject(fsm))return false;
    auto* pawn=object(g_peCtrl,controllerPawnOff);
    if(!pawn || pawn!=lastPawn)return false;
    int lane=-1;
    for(int n=0;n<3;++n)if(object(pawn,pawnFsm[n])==fsm){lane=n;break;}
    if(lane<0)return false;
    auto* cls=*(uint8_t**)(request+kAnimRequestClassOff);
    if(!IsLiveObject(cls))return false;
    const char* name=objectName(cls);
    if(!name)return false;
    const int rule=arm_rule_index(lane,name);
    if(!cancellable_action(rule) || action_enabled(rule))return false;
    // Do not prevent an action already underway from completing/re-entering.
    if(object(fsm,idOff)==cls)return false;
    if(!RangeReadable((void*)kGObjHdr,12))return false;
    auto** objects=*(uint8_t***)kGObjHdr;
    const uint32_t count=*(uint32_t*)(kGObjHdr+4);
    if(!objects || count>4000000 || !RangeReadable(objects,count*sizeof(void*)))return false;
    unsigned found=0;
    for(uint32_t n=0;n<count && found!=15;++n){
        auto* p=objects[n];if(p==g_peCtrl)found|=1;if(p==pawn)found|=2;if(p==fsm)found|=4;if(p==cls)found|=8;
    }
    if(found!=15)return false;
    DVR_LOG_EVERY_MS(DVR_CAT,dvr::log::Level::Info,500,"anim/action: rejected lane=%d state=%s before native entry",lane,name);
    return true;
}
__declspec(naked) void action_stub() {
    __asm {
        pushfd
        pushad
        mov eax,[esp+40]
        push eax
        push ecx
        call reject_action
        add esp,8
        test al,al
        jnz denied
        popad
        popfd
        push ebp
        mov ebp,esp
        push -1
        jmp dword ptr [actionResume]
    denied:
        popad
        popfd
        xor eax,eax
        ret 12
    }
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
bool mantle_enabled() { AcquireSRWLockShared(&lock); bool on=mantleHandback; ReleaseSRWLockShared(&lock); return on; }
void set_mantle(bool on) {
    AcquireSRWLockExclusive(&lock); mantleHandback=on; ReleaseSRWLockExclusive(&lock);
    Log("anim: MantleHandBack=%d (live)",on?1:0);
}
bool cinematic_enabled() { AcquireSRWLockShared(&lock); bool on=cinematicHandback; ReleaseSRWLockShared(&lock); return on; }
void set_cinematic(bool on) {
    AcquireSRWLockExclusive(&lock); cinematicHandback=on; ReleaseSRWLockExclusive(&lock);
    Log("anim: CinematicHandBack=%d (live; native hands/arms for cinematic states)",on?1:0);
}
bool enabled() { AcquireSRWLockShared(&lock); bool on=handback; ReleaseSRWLockShared(&lock); return on; }
void set_enabled(bool on) {
    AcquireSRWLockExclusive(&lock); handback=on; handoff=Handoff{};
    char ini[MAX_PATH];text(ini,sizeof(ini),rulesIni);ReleaseSRWLockExclusive(&lock);
    if(*ini)WritePrivateProfileStringA("Anim","HandBack",on?"1":"0",ini);
    Log("anim: HandBack=%d (live, saved)",on?1:0);
}
bool arm_rule_enabled(int i) {
    if(i<0 || i>=armRuleCount)return false;
    AcquireSRWLockShared(&lock);
    const bool on=resolve_arm_rule(armRules[i].lane,armRules[i].state);
    ReleaseSRWLockShared(&lock);return on;
}
void set_arm_rule(int i,bool on) {
    if(i<0 || i>=armRuleCount)return;
    AcquireSRWLockExclusive(&lock);armOverrides.values[i]=on?1:0;
    char ini[MAX_PATH];text(ini,sizeof(ini),rulesIni);ReleaseSRWLockExclusive(&lock);
    char key[128];arm_rule_key(i,key,sizeof(key));
    if(*ini)WritePrivateProfileStringA("Anim",key,on?"1":"0",ini);
    Log("anim: %s=%d (live, saved)",key,int(on));
}
void reset_arm_rules() {
    AcquireSRWLockExclusive(&lock);armOverrides=ArmOverrides{};
    char ini[MAX_PATH];text(ini,sizeof(ini),rulesIni);ReleaseSRWLockExclusive(&lock);
    for(int i=0;i<armRuleCount;++i){char key[128];arm_rule_key(i,key,sizeof(key));
        if(*ini)WritePrivateProfileStringA("Anim",key,nullptr,ini);}
    Log("anim: per-state arm overrides reset to inherited defaults");
}
bool action_gate_ready(){return actionDetour.on;}
bool action_enabled(int i) {
    if(!cancellable_action(i))return true;
    AcquireSRWLockShared(&lock);bool on=actionAllowed[i];ReleaseSRWLockShared(&lock);return on;
}
void set_action_enabled(int i,bool on) {
    if(!cancellable_action(i))return;
    AcquireSRWLockExclusive(&lock);actionAllowed[i]=on;
    unsigned count=0;for(int n=0;n<armRuleCount;++n)if(cancellable_action(n) && !actionAllowed[n])++count;
    disabledActions.store(count);
    char ini[MAX_PATH];text(ini,sizeof(ini),rulesIni);ReleaseSRWLockExclusive(&lock);
    char key[128];_snprintf_s(key,sizeof(key),_TRUNCATE,"Action.%d.%s",armRules[i].lane,armRules[i].state);
    if(*ini)WritePrivateProfileStringA("Anim",key,on?"1":"0",ini);
    Log("anim/action: %s=%d (next request; current action is allowed to finish)",key,int(on));
}
float view_right_cm(){AcquireSRWLockShared(&lock);float v=viewRightCm;ReleaseSRWLockShared(&lock);return v;}
void set_view_right_cm(float cm){
    if(!std::isfinite(cm))return;cm=std::clamp(cm,-20.0f,20.0f);
    AcquireSRWLockExclusive(&lock);viewRightCm=cm;
    char ini[MAX_PATH];text(ini,sizeof(ini),rulesIni);ReleaseSRWLockExclusive(&lock);
    char v[32];_snprintf_s(v,sizeof(v),_TRUNCATE,"%.3f",cm);
    if(*ini)WritePrivateProfileStringA("Anim","ViewRightCm",v,ini);
    Log("anim/alignment: native animation view right=%.2f cm (manual trim, not a measured correction)",cm);
}
float view_right_metres(){
    if(g_menuOpen || g_inMenu || g_mainMenu || UiSurfaceBlocks())return 0;
    return active()?view_right_cm()*.01f*(1.0f-weight()):0.0f;
}
float weight() {
    dvr::render_profile::Scope profile(dvr::render_profile::AnimationWeight);
    AcquireSRWLockExclusive(&lock);
    const auto now=GetTickCount64();
    const bool valid=watch && handback && published.valid && fresh(published.stamp,now);
    const unsigned frame=(unsigned)dvr::frame::count();
    if (!valid) { frameWeight=1; frameArms=false; weightFrame=~0u; }
    else if (weightFrame!=frame) { frameWeight=handoff.value(now,blendMs); frameArms=published.showArms; weightFrame=frame; }
    const float w=frameWeight;
    ReleaseSRWLockExclusive(&lock); return w;
}
bool active() { const Snapshot s=snapshot(); return enabled() && s.valid && s.game; }
bool native_draw() { return weight()<=0.0001f; }
bool native_full_arms() {
    if(!native_draw())return false;
    AcquireSRWLockShared(&lock);const bool full=frameArms;ReleaseSRWLockShared(&lock);return full;
}
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
    s.stamp=now; ++s.generation; s.valid=false; s.game=false; s.dialogState=-1;
    text(s.reason,sizeof(s.reason),"unresolved or pawn unavailable");
    staleTable=false;
    const bool ctrlLive=IsLiveObject(g_peCtrl);
    if (!ctrlLive && g_peCtrl && LooksLikeObj(g_peCtrl)) staleTable=true;
    uint8_t* pawn=ctrlLive?object(g_peCtrl,controllerPawnOff):nullptr;
    // The FSM offsets belong to DishonoredPlayerPawn. While possessing, the pawn is
    // another class, and reading those offsets would chase unrelated fields.
    if (pawn) { const char* pc=ObjClassName(pawn); if (!pc || !strstr(pc,"PlayerPawn")) pawn=nullptr; }
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
                if (!strcmp(s.state[0],"StatePlayerMasterInDialog") ||
                    !strcmp(s.state[0],"StatePlayerMasterInScriptedChoice")) {
                    unsigned char value=255;
                    if (read(state,dialogOff,&value,1) && value<=1) s.dialogState=value;
                }
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
    drop_sample(pawn,s);
    if (!s.valid && staleTable && now>=nextRebuild) {
        nextRebuild=now+1000;   // a rebuild is a full GObjects copy and sort: at most once a second
        const bool built=BuildLiveSet(); ++tableRebuilds;
        Log("anim: live-object table rebuilt (#%u, %s) - a real %s was missing from it, so it predated this "
            "level; the next sample reads against the new table",tableRebuilds,built?"ok":"refused: object table busy",
            ctrlLive?"pawn, state machine or state":"player controller");
        text(s.reason,sizeof(s.reason),"live-object table was stale; rebuilt");
    }
    AcquireSRWLockExclusive(&lock);
    if (pawnChanged || !previous.valid || !fresh(previous.stamp,now)) { handoff=Handoff{}; classifier=Handoff{}; cameraClassifier=Handoff{}; }
    const bool cinematic=cinematicHandback && dvr::scene_state::cinematic(s.state[0]);
    const bool mantle=mantleHandback && !strcmp(s.state[0],"StatePlayerMasterMantle");
    cameraClassifier.update(s.valid,mantle || cinematic || listed(masterRules,s.state[0]) || listed(upperRules,s.state[1]),watch,now,releaseMs,0);
    s.cameraAction=s.valid && cameraClassifier.game;
    bool match=false;
    // Whole-body actions own visibility before upper/left actions. Idle or walking
    // with hidden arms does not mask a real upper-body animation.
    for(int lane=0;lane<3;++lane) {
        const bool full=resolve_arm_rule(lane,s.state[lane]);
        if(native_pose_requested(arm_rule_index(lane,s.state[lane]),default_arm_rule(lane,s.state[lane]),full)) {
            match=true;s.showArms=full;break;
        }
    }
    // Keep the last action's geometry through the same release interval as its pose.
    if(!match)s.showArms=previous.showArms;
    classifier.update(s.valid,match,watch,now,releaseMs,0);
    handoff.update(s.valid,classifier.game,watch && handback,now,0,blendMs);
    // StateWatch still reports the classifier with HandBack disabled.
    s.game=s.valid && classifier.game;
    if (s.valid) text(s.reason,sizeof(s.reason),match?(s.showArms?"native pose with full arms":"native pose with split hands"):classifier.game?"release hysteresis":"no active native action");
    published=s;
    ReleaseSRWLockExclusive(&lock);
    if (s.valid!=previous.valid || s.game!=previous.game || s.showArms!=previous.showArms || memcmp(s.state,previous.state,sizeof(s.state)) || s.bodyMode!=previous.bodyMode || strcmp(s.sequence,previous.sequence) || now>=nextBeat) {
        report(s); nextBeat=now+5000;
    }
}
void configure(const char* ini) {
    if(!actionDetour.on) {
        if(RangeReadable((void*)kAnimRequestState,sizeof(kAnimRequestStatePrefix)) &&
           !memcmp((void*)kAnimRequestState,kAnimRequestStatePrefix,sizeof(kAnimRequestStatePrefix)))
            dvr::hooks::detour_install(actionDetour,"anim/action",kAnimRequestState,kAnimRequestStateBytes,sizeof(kAnimRequestStateBytes),action_stub);
        else Log("anim/action: request gate refused: native entry prefix mismatch at %p",(void*)kAnimRequestState);
    }
    AcquireSRWLockExclusive(&lock);
    text(rulesIni,sizeof(rulesIni),ini);
    for(int i=0;i<armRuleCount;++i){char key[128];arm_rule_key(i,key,sizeof(key));
        int v=GetPrivateProfileIntA("Anim",key,-1,ini);armOverrides.values[i]=(v==0 || v==1)?v:-1;}
    unsigned disabled=0;
    for(int i=0;i<armRuleCount;++i){char key[128];
        _snprintf_s(key,sizeof(key),_TRUNCATE,"Action.%d.%s",armRules[i].lane,armRules[i].state);
        actionAllowed[i]=!cancellable_action(i) || GetPrivateProfileIntA("Anim",key,1,ini)!=0;
        if(!actionAllowed[i])++disabled;
    }
    disabledActions.store(disabled);
    char alignment[32];GetPrivateProfileStringA("Anim","ViewRightCm","0",alignment,sizeof(alignment),ini);
    viewRightCm=(float)atof(alignment);if(!std::isfinite(viewRightCm))viewRightCm=0;
    viewRightCm=std::clamp(viewRightCm,-20.0f,20.0f);
    Log("anim/action: request gate=%d disabled=%u; native view right=%.2f cm",int(actionDetour.on),disabled,viewRightCm);
    dropWatch=GetPrivateProfileIntA("Anim","DropWatch",1,ini)!=0;
    Log("config: [Anim] DropWatch=%d (read-only native drop eligibility)",dropWatch);
    const int watchSetting=GetPrivateProfileIntA("Anim","StateWatch",-1,ini);
    const int backSetting=GetPrivateProfileIntA("Anim","HandBack",-1,ini);
    watch=watchSetting!=0;
    handback=backSetting!=0;
    cinematicHandback=GetPrivateProfileIntA("Anim","CinematicHandBack",1,ini)!=0;
    Log("config: [Anim] CinematicHandBack=%d",cinematicHandback);
    mantleHandback=GetPrivateProfileIntA("Anim","MantleHandBack",1,ini)!=0;
    Log("config: [Anim] MantleHandBack=%d",mantleHandback);
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
// The state lists are deliberately NOT written: once materialised in an ini they
// beat every later compiled default (TRAPS section 1), and the default list is
// still being tuned by headset runs. An edited list in the ini is kept as is.
void save(const char* ini) {
    AcquireSRWLockShared(&lock);
    const bool w=watch, b=handback, c=cinematicHandback, mantle=mantleHandback; const unsigned r=releaseMs, m=blendMs;
    ReleaseSRWLockShared(&lock);
    char v[16];
    WritePrivateProfileStringA("Anim","StateWatch",w?"1":"0",ini);
    WritePrivateProfileStringA("Anim","HandBack",b?"1":"0",ini);
    WritePrivateProfileStringA("Anim","CinematicHandBack",c?"1":"0",ini);
    WritePrivateProfileStringA("Anim","MantleHandBack",mantle?"1":"0",ini);
    _snprintf_s(v,sizeof(v),_TRUNCATE,"%u",r); WritePrivateProfileStringA("Anim","ReleaseMs",v,ini);
    _snprintf_s(v,sizeof(v),_TRUNCATE,"%u",m); WritePrivateProfileStringA("Anim","HandBackBlendMs",v,ini);
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
