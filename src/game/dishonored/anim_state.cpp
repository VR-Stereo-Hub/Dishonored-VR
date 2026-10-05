#include "core/framework/render_profile.h"
#include <algorithm>
// VR-88/VR-134: reads player states; optionally rejects native action requests before entry.
#include "game/dishonored/anim_state.h"
#include "game/dishonored/anim_policy.h"
#include "game/dishonored/stereo_state_policy.h"
#include "game/dishonored/move_speed.h"
namespace dvr::anim {
namespace {
SRWLOCK lock = SRWLOCK_INIT;
SRWLOCK sampleLock = SRWLOCK_INIT;
Snapshot published;
Handoff handoff;
Handoff classifier;
Handoff cameraClassifier;
bool watch = true, handback = true, cinematicHandback = false, mantleHandback = false;
// VR-283: takedowns (assassinations on the ground and from the air, chokes) and combat
// fatalities draw the game-animated HANDS through the arm/hand split, forearms hidden, the
// way mantle does. Hiding the arms by bone visibility froze the hands (the earlier attempt);
// the split keeps the game's clip on the hands and drops only the arm triangles, at the
// player's F10 sleeve length. Off = the game's full arms, as before.
bool hideTakedownArms = false;
bool takedown_state(const char* master, const char* upper) {
    return !strcmp(master,"StatePlayerMasterAssassinate") || !strcmp(master,"StatePlayerMasterChoke") ||
           !strcmp(upper,"StatePlayerGenericFatality");
}
// Game animation on the tracked hands with the arms hidden, like mantling: the
// sword swing (StatePlayerMeleeAttack) and the shot (a *Fire* clip inside
// StatePlayerAction, where run483 measured Pistol_Fire). New levers: default off.
bool handAnimMelee = false, handAnimFire = false;
// VR-220: the melee hand-back is for TRIGGER attacks. A physical swing (the motion sword,
// VR-37) presses the same trigger, but the player's own arm is that animation; pinning it
// to the game's clip would yank a moving arm. HandAnimMeleeSwing=1 hands swings back too.
bool handAnimMeleeSwing = false;
// VR-220: the trigger clip is right-handed, so by default only the right hand follows it and
// the left stays on the controller (free to point, Blink, hold an item). 1 = both hands.
bool handAnimMeleeBoth = false;
unsigned char frameMask = 0;         // the mask the frame's weight was cached with
std::atomic<unsigned char> ownedMask{0};
// The choke owns the arms, from its state until its hand-back has fully returned (the IK draw
// keeps the game's arm exactly as drawn for it: ArmIKGameArmShoulder=1). Latched so the rule
// cannot change under a blend that is still running when the state has already moved on.
std::atomic<bool> chokeArms{false};   // hands the game owns right now (tick() publishes it; hand_owned reads it)
unsigned long long meleeKey = 0;     // the attack classified: max(entered[1], a combo clip's sequenceAt)
bool meleeTrigger = false;           // its verdict, latched: true = TRIGGER, false = SWING
char meleeSource[8] = "none";        // for status.json
long long meleeFireDt = -1;          // ms from the last swing fire to the attack; -1 = no fire before it
char rulesIni[MAX_PATH]={};
struct ArmOverrides { int values[armRuleCount]; ArmOverrides(){for(int& v:values)v=-1;} } armOverrides;
bool actionAllowed[armRuleCount];
std::atomic<unsigned> disabledActions{0};
float viewRightCm=0;
dvr::hooks::Detour actionDetour;
uintptr_t actionResume=kAnimRequestState+sizeof(kAnimRequestStateBytes);
unsigned releaseMs = 250, blendMs = 150;
// [Anim] SmoothBlend: the hand-back eases in and out (smootherstep) over its own entry and
// return durations, the palm travels a straight line, and the hands stay owned until the
// return reaches the controller. Off = the original linear HandBackBlendMs ramp, exactly.
std::atomic<bool> smoothBlend{false};
unsigned blendInMs = 250, blendOutMs = 350;
// [Anim] CinematicArms: in a cinematic the player keeps tracked arms; the game takes them
// (with the same blend) only while it animates them - an upper/left action or the matinee
// pose blend. A hide-player cinematic (Kismet bHidePlayer) also has the pawn unhidden while
// the body mode is arms-only, through the game's own setter. Off = CinematicHandBack rules.
bool cinematicArms = false;
void apply_shape() { handoff.smooth=smoothBlend.load(); handoff.inMs=blendInMs; handoff.outMs=blendOutMs; }
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
bool frameMantleSplit = false;
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
// ---- CinematicArms ---------------------------------------------------------------------
// Names resolved once, in one object-table walk (~100 ms), the first time a cinematic is seen.
// Actor.bHidden must land where the static reading put it (+0x120, bit 1: patterns.h), or the
// pawn is never touched: a route that cannot reproduce a known answer is not evidence.
struct CineProps {
    bool tried=false, hiddenAgrees=false, setterOk=false;
    uint32_t matineeOff=0, enabledOff=0, enabledMask=0, hiddenOff=0, hiddenMask=0, modeOff=0, modeMask=0;
    // Instrument only (which matinee field marks an authored arm clip): ActiveChildIndex,
    // BlendTimeToGo, m_bDoBlend. found* distinguishes a real offset 0 from a miss.
    uint32_t childOff=0, togoOff=0, doBlendOff=0, doBlendMask=0; bool childFound=false, togoFound=false;
    // Instrument only, the BioShock Infinite mod's two cutscene signals: the input-lock counters
    // (bytes) and the view target (a cutscene camera vs the pawn).
    uint32_t ignoreMoveOff=0, ignoreLookOff=0, viewTargetOff=0;
} cineProps;
void cine_resolve() {
    if (cineProps.tried || !RflNamesReady()) return;
    cineProps.tried=true;
    RflWant w[10]={{"DishonoredPlayerPawn","m_pMatineeBlender",false,0,0,false},{"ArkAnimNodeBlendPose","m_bEnabled",true,0,0,false},
                  {"Actor","bHidden",true,0,0,false},{"PlayerController","bCinematicMode",true,0,0,false},
                  {"ArkAnimNodeBlendPose","ActiveChildIndex",false,0,0,false},{"ArkAnimNodeBlendPose","BlendTimeToGo",false,0,0,false},
                  {"ArkAnimNodeBlendPose","m_bDoBlend",true,0,0,false},{"PlayerController","bIgnoreMoveInput",false,0,0,false},
                  {"PlayerController","bIgnoreLookInput",false,0,0,false},{"Controller","ViewTarget",false,0,0,false}};
    RflResolveBatch(w,10);
    if (w[7].found) cineProps.ignoreMoveOff=w[7].off;
    if (w[8].found) cineProps.ignoreLookOff=w[8].off;
    if (w[9].found) cineProps.viewTargetOff=w[9].off;
    Log("cine/arms: input locks bIgnoreMoveInput=%s+0x%x bIgnoreLookInput=%s+0x%x, ViewTarget=%s+0x%x",
        w[7].found?"":"MISSING ",w[7].off,w[8].found?"":"MISSING ",w[8].off,w[9].found?"":"MISSING ",w[9].off);
    if (w[0].found) cineProps.matineeOff=w[0].off;
    if (w[1].found) { cineProps.enabledOff=w[1].off; cineProps.enabledMask=w[1].mask; }
    if (w[2].found) { cineProps.hiddenOff=w[2].off; cineProps.hiddenMask=w[2].mask; }
    if (w[3].found) { cineProps.modeOff=w[3].off; cineProps.modeMask=w[3].mask; }
    if (w[4].found) { cineProps.childOff=w[4].off; cineProps.childFound=true; }
    if (w[5].found) { cineProps.togoOff=w[5].off; cineProps.togoFound=true; }
    if (w[6].found) { cineProps.doBlendOff=w[6].off; cineProps.doBlendMask=w[6].mask; }
    Log("cine/arms: matinee instrument fields ActiveChildIndex=%s+0x%x BlendTimeToGo=%s+0x%x m_bDoBlend=%s+0x%x/0x%x",
        w[4].found?"":"MISSING ",w[4].off,w[5].found?"":"MISSING ",w[5].off,w[6].found?"":"MISSING ",w[6].off,w[6].mask);
    cineProps.hiddenAgrees=w[2].found && w[2].off==kActorHiddenOff && w[2].mask==kActorHiddenMask;
    cineProps.setterOk=RangeReadable((void*)kActorSetHidden,sizeof(kActorSetHiddenBytes)) &&
        !memcmp((void*)kActorSetHidden,kActorSetHiddenBytes,sizeof(kActorSetHiddenBytes));
    Log("cine/arms: resolved m_pMatineeBlender=%s+0x%x m_bEnabled=%s+0x%x/0x%x bCinematicMode=%s+0x%x/0x%x | "
        "Actor.bHidden=%s+0x%x/0x%x vs static +0x%x/0x%x: %s | SetHidden 0x%08X bytes %s",
        w[0].found?"":"MISSING ",w[0].off,w[1].found?"":"MISSING ",w[1].off,w[1].mask,w[3].found?"":"MISSING ",w[3].off,w[3].mask,
        w[2].found?"":"MISSING ",w[2].off,w[2].mask,kActorHiddenOff,kActorHiddenMask,
        cineProps.hiddenAgrees?"AGREE":"DISAGREE - the pawn will not be unhidden",(unsigned)kActorSetHidden,
        cineProps.setterOk?"verified":"MISMATCH - the pawn will not be unhidden");
}
bool lane_idle(const char* st) { return !strcmp(st,"StatePlayerUpperIdle") || !strcmp(st,"StatePlayerUpperNav"); }
int read_flag(uint8_t* obj,uint32_t off,uint32_t mask) {
    uint32_t bits=0;
    return obj && mask && read(obj,off,&bits,4) ? ((bits & mask)?1:0) : -1;
}
// The game animates the arms inside a cinematic: an upper or left-arm action (the unequip at
// a conversation's start, an item use). NOT the matinee pose blend: run 2026-10-04 measured
// m_bEnabled=1 for the whole of every conversation, so it took the hands for the entire scene.
// Its finer fields are logged by cine_visibility until a run shows which marks a real clip.
// Run 2 (2026-10-04): a scripted arm clip (picking Emily up) showed no arm action, no change of
// the matinee node and no distinct state, so the game's own arm MOTION is the signal, as in the
// BioShock Remastered mod (M7-S4): the speed of its upper-arm/forearm bones (MsSampleArmSpeed),
// bones the mod never writes, through a gate with hysteresis. The hold is a setting because a
// scripted clip can freeze a pose for seconds mid-scene (BRVR measured 2.5 and 4.5 s).
// Run 3 (2026-10-04): the first instrument (the fastest arm-bone point) followed the PLAYER's
// controller, because the mod's hand control moves palette bones behind the wrist: in a
// conversation any hand movement above 0.2 m/s opened the gate, the game's arms turned out to be
// a still stance (0.0 uu/s in every second the game owned them), and the gate closed on the hold:
// all 15 openings lasted 1.8 to 2.2 s. The speed is now measured between the bones of one arm with
// the mod's single rigid write excluded (arm_motion, anim_policy.h); an opening also needs
// cineMinSamples different measurements (a pose snap is one), and a pose within cineRefPoseUu of
// the reference pose is never the game animating (the default stance of an unposed arm mesh).
// The thresholds are in the units of the NEW instrument and are not yet set from a run; the hold
// is 5 s because the sibling mod measured scripted poses frozen for 2.5 and 4.5 s mid-clip.
MotionGate cineMotion;
float cineStartSpeed=20.0f, cineStopSpeed=8.0f, cineRefPoseUu=1.0f;
unsigned cineStartMs=120, cineHoldMs=5000, cineMinSamples=3;
std::atomic<bool> cineGateOpen{false}, cineRefVeto{false};
struct CineMotionStats {
    unsigned long long since=0; float max=0, sum=0, fastest=0, refMin=-1, refMax=-1;
    unsigned n=0, on=0, veto=0, stale=0, openings=0; int bones=0;
} cineStats;
bool cine_animating(const Snapshot& s,uint8_t*) {
    const unsigned long long now=GetTickCount64();
    const float raw=g_msArmSpeed.load();
    const bool fresh=raw>=0 && now-g_msArmSpeedMs<=200;
    const float speed=fresh ? raw : 0.0f;   // no arm draw (hidden, culled): nothing to measure, read as still
    const float ref=fresh ? g_msArmRefPose.load() : -1.0f;
    // The reference pose is what an arm mesh shows when nothing poses it. Never the game's clip.
    const bool veto=cineRefPoseUu>0 && ref>=0 && ref<cineRefPoseUu;
    const bool was=cineMotion.on;
    if (veto) cineMotion=MotionGate{};
    const bool moving=!veto && cineMotion.update(speed,now,cineStartSpeed,cineStopSpeed,cineStartMs,cineHoldMs,
                                                 1ull+g_msArmSampleGen.load(),cineMinSamples);
    cineGateOpen.store(moving); cineRefVeto.store(veto);
    if (!cineStats.since) cineStats.since=now;
    cineStats.max=fmaxf(cineStats.max,speed); cineStats.sum+=speed; ++cineStats.n; cineStats.on+=moving?1:0;
    cineStats.fastest=fmaxf(cineStats.fastest,fresh?g_msArmFastest.load():0.0f);
    cineStats.veto+=veto?1:0; cineStats.stale+=fresh?0:1; cineStats.openings+=(moving && !was)?1:0;
    cineStats.bones=g_msArmBones.load();
    if (ref>=0) { cineStats.refMin=cineStats.refMin<0?ref:fminf(cineStats.refMin,ref); cineStats.refMax=fmaxf(cineStats.refMax,ref); }
    if (moving!=was) Log("cine/gate: %s - game arm motion %.1f uu/s between bones (palette's fastest point %.1f, which also follows your "
        "controller), reference-pose distance %.2f uu%s | opens above %.0f for %u ms over %u measurements, closes below %.0f after %u ms",
        moving?"OPEN, the game takes the arms":veto?"CLOSED at once, the arms are in the reference pose":"CLOSED, the hold ran out",
        speed,g_msArmFastest.load(),ref,ref<0?" (not measurable)":"",cineStartSpeed,cineStartMs,cineMinSamples,cineStopSpeed,cineHoldMs);
    return moving || !lane_idle(s.state[1]) || !lane_idle(s.state[2]);
}
struct CineVis {
    uint8_t* unhid=nullptr; unsigned long long unhidAt=0; bool honourLogged=true;
    unsigned unhides=0, rehides=0; int lastMode=-2, lastHidden=-2, lastMatinee=-2; bool lastCine=false;
    int lastChild=-2, lastDoBlend=-2, lastTogo=-2, lastMatineeI=-2;
} cineVis;
// Kept trivial: __try cannot share a frame with objects that need unwinding.
bool cine_set_hidden(uint8_t* pawn,int hide) {
    __try { ((void(__thiscall*)(void*,int))kActorSetHidden)(pawn,hide); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// Script lane, after the snapshot is published and outside the lock. With the lever on it logs
// the facts of every cinematic transition (the instrument) and unhides a hidden arms-only or full-body pawn
// for the cinematic, re-hiding it if the lever goes off or the body leaves arms-only while the
// cinematic still runs. The game's own PreSetCinematicMode(false) unhides it at the end, so
// nothing is restored then.
void cine_visibility(uint8_t* pawn,const Snapshot& s,bool leverOn,uint8_t* ctrl) {
    const bool cineState=s.valid && dvr::scene_state::cinematic(s.state[0]);
    // Lever off: nothing at all (no name walk, no reads), unless a pawn we unhid must be put back.
    // Lever on: one name walk (~100 ms, once per session, on the game thread), then plain reads.
    if (!leverOn && !cineVis.unhid) return;
    cine_resolve();
    const int mode=read_flag(ctrl,cineProps.modeOff,cineProps.modeMask);
    const int hidden=read_flag(pawn,cineProps.hiddenOff,cineProps.hiddenMask);
    const int matinee=pawn && cineProps.matineeOff ? read_flag(object(pawn,cineProps.matineeOff),cineProps.enabledOff,cineProps.enabledMask) : -1;
    const unsigned long long now=GetTickCount64(), drawn=g_msLastArmDrawMs;
    if (pawn!=cineVis.unhid) cineVis.unhid=nullptr;   // a new pawn or none: nothing of ours to undo
    if (mode!=cineVis.lastMode || hidden!=cineVis.lastHidden || cineState!=cineVis.lastCine || matinee!=cineVis.lastMatinee) {
        char ago[32]; if (!drawn) text(ago,sizeof(ago),"never"); else _snprintf_s(ago,sizeof(ago),_TRUNCATE,"%llu ms ago",now-drawn);
        Log("cine/arms: master=%s cinematicMode=%d pawnHidden=%d body=%d matineeBlend=%d upper=%s left=%s | arm mesh last drawn %s | "
            "CinematicArms=%d unhid-by-us=%d (-1 = unreadable; the arms draw only while the pawn is visible and arms-only)",
            s.state[0],mode,hidden,s.bodyMode,matinee,s.state[1],s.state[2],ago,(int)leverOn,cineVis.unhid?1:0);
        cineVis.lastMode=mode; cineVis.lastHidden=hidden; cineVis.lastCine=cineState; cineVis.lastMatinee=matinee;
    }
    // Instrument: inside a cinematic, every change of the matinee node's child/blend fields,
    // beside the arm actions and the newest sequence, so a run shows which one marks a clip.
    if (cineState || mode==1) {
        uint8_t* blender=pawn && cineProps.matineeOff ? object(pawn,cineProps.matineeOff) : nullptr;
        int child=-1, doBlend=read_flag(blender,cineProps.doBlendOff,cineProps.doBlendMask); float togo=-1;
        if (blender && cineProps.childFound) read(blender,cineProps.childOff,&child,4);
        if (blender && cineProps.togoFound) read(blender,cineProps.togoOff,&togo,4);
        const int togoBucket=togo>0 ? 1 : togo==0 ? 0 : -1;   // moving vs settled, not every float step
        if (child!=cineVis.lastChild || doBlend!=cineVis.lastDoBlend || togoBucket!=cineVis.lastTogo || matinee!=cineVis.lastMatineeI) {
            DVR_LOG_EVERY_MS(DVR_CAT,dvr::log::Level::Info,250,"cine/matinee: enabled=%d ActiveChildIndex=%d doBlend=%d BlendTimeToGo=%.3f | upper=%s left=%s seq=%s | owner=%s",
                matinee,child,doBlend,togo,s.state[1],s.state[2],s.sequence,s.handMask?"GAME":"PLAYER");
            cineVis.lastChild=child; cineVis.lastDoBlend=doBlend; cineVis.lastTogo=togoBucket; cineVis.lastMatineeI=matinee;
        }
    }
    // Once a second in a cutscene: the motion the gate saw (for setting its thresholds from a
    // run, as BRVR did), with the two signals the BioShock Infinite mod uses beside it.
    if (cineStats.n && now-cineStats.since>=1000) {
        unsigned char lockMove=255, lockLook=255;
        if (ctrl && cineProps.ignoreMoveOff) read(ctrl,cineProps.ignoreMoveOff,&lockMove,1);
        if (ctrl && cineProps.ignoreLookOff) read(ctrl,cineProps.ignoreLookOff,&lockLook,1);
        uint8_t* vt=ctrl && cineProps.viewTargetOff ? object(ctrl,cineProps.viewTargetOff) : nullptr;
        const char* vtc=vt ? ObjClassName(vt) : nullptr;
        Log("cine/motion: game arm motion max=%.1f mean=%.1f uu/s between bones (%d bones an arm; 0 with the fastest point high = your own "
            "hand, or one joint only) | palette fastest point max=%.1f uu/s (the run-3 instrument, follows your controller) | reference-pose "
            "distance %.2f..%.2f uu (-1 = not measurable; under %.2f is vetoed: %u of %u samples) | gate open %u/%u samples, %u opening(s), "
            "%u sample(s) with no fresh measurement (opens above %.0f for %u ms over %u measurements, closes below %.0f after %u ms) | "
            "input locks move=%d look=%d | view target %s%s | master=%s upper=%s left=%s owner=%s",
            cineStats.max,cineStats.sum/cineStats.n,cineStats.bones,cineStats.fastest,cineStats.refMin,cineStats.refMax,cineRefPoseUu,
            cineStats.veto,cineStats.n,cineStats.on,cineStats.n,cineStats.openings,cineStats.stale,cineStartSpeed,cineStartMs,cineMinSamples,
            cineStopSpeed,cineHoldMs,
            (int)lockMove,(int)lockLook,vtc?vtc:"?",vt && vt==pawn?" (the pawn)":"",s.state[0],s.state[1],s.state[2],s.handMask?"GAME":"PLAYER");
        cineStats=CineMotionStats{}; cineStats.since=now;
    }
    if (cineVis.unhid && !cineVis.honourLogged && now-cineVis.unhidAt>=1000) {
        cineVis.honourLogged=true;
        const bool honoured=drawn>=cineVis.unhidAt;
        Log("cine/arms: unhide %s - the arm mesh %s in the second after it (a verified write is not an honoured one)",
            honoured?"HONOURED":"NOT honoured",honoured?"drew":"did NOT draw");
    }
    if (mode!=1) { cineVis.unhid=nullptr; return; }   // no cinematic, or it ended: the game unhides its own pawn
    // Arms-only or full body: the game itself shows the full-body pawn in conversations
    // (run 1: body=1, pawn visible, arms drawn). The boat ride is body=1 with the pawn hidden;
    // the first build refused it there and the arms stayed away. HIDDEN (2) is never touched.
    if (!leverOn || (s.bodyMode!=0 && s.bodyMode!=1)) {
        if (cineVis.unhid && hidden==0 && cine_set_hidden(cineVis.unhid,1)) {
            ++cineVis.rehides;
            Log("cine/arms: pawn hidden again (CinematicArms=%d body=%d) - the cinematic still runs and wanted it hidden",(int)leverOn,s.bodyMode);
        }
        cineVis.unhid=nullptr;
        return;
    }
    if (hidden!=1) return;
    if (!cineProps.hiddenAgrees || !cineProps.setterOk) {
        DVR_LOG_EVERY_MS(DVR_CAT,dvr::log::Level::Warn,10000,"cine/arms: hidden pawn left hidden - %s",
            !cineProps.hiddenAgrees?"Actor.bHidden did not resolve to the statically read +0x120/0x2":"the SetHidden prologue does not match patterns.h");
        return;
    }
    uint8_t* global=RangeReadable((void*)kPlayerPawnGlobal,4) ? *(uint8_t**)kPlayerPawnGlobal : nullptr;
    if (!pawn || pawn!=global || !IsLiveObject(pawn)) {
        DVR_LOG_EVERY_MS(DVR_CAT,dvr::log::Level::Warn,10000,"cine/arms: hidden pawn left hidden - the controller's pawn %p is not the live player pawn %p",(void*)pawn,(void*)global);
        return;
    }
    if (!cine_set_hidden(pawn,0)) {
        Log("cine/arms: SetHidden(0) FAULTED - CinematicArms switched off for this session");
        AcquireSRWLockExclusive(&lock); cinematicArms=false; ReleaseSRWLockExclusive(&lock); return;
    }
    ++cineVis.unhides; cineVis.unhid=pawn; cineVis.unhidAt=now; cineVis.honourLogged=false;
    if (cineVis.unhides>5) DVR_LOG_EVERY_MS(DVR_CAT,dvr::log::Level::Info,5000,"cine/arms: the game hid the pawn again and it was unhidden again (#%u)",cineVis.unhides);
    else Log("cine/arms: pawn UNHIDDEN for the cinematic (#%u; bHidden now %d, master=%s body=%d) - tracked arms visible; the game takes them while it animates",
        cineVis.unhides,read_flag(pawn,cineProps.hiddenOff,cineProps.hiddenMask),s.state[0],s.bodyMode);
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
bool takedown_arms_hidden() { AcquireSRWLockShared(&lock); bool on=hideTakedownArms; ReleaseSRWLockShared(&lock); return on; }
void set_takedown_arms_hidden(bool on) {   // VR-283
    AcquireSRWLockExclusive(&lock); hideTakedownArms=on; ReleaseSRWLockExclusive(&lock);
    Log("anim: HideTakedownArms=%d (live; %s)",on?1:0,
        on?"takedowns and fatalities draw the game-animated hands only, forearms hidden at the F10 sleeve length"
          :"takedowns and fatalities draw the game's full arms");
}
void set_mantle(bool on) {
    AcquireSRWLockExclusive(&lock); mantleHandback=on; ReleaseSRWLockExclusive(&lock);
    Log("anim: MantleHandBack=%d (live)",on?1:0);
}
// VR-220: who pressed the attack the game just started. T is the attack's key (the state
// entry, or a combo clip's start), F the last swing FIRE, C when that press stopped (0 while
// it is still held). Script lane, under the exclusive lock; the swing record is atomic.
static bool classify_melee_source(unsigned long long T,bool combo,const Snapshot& s) {
    const unsigned long long F=dvr::swing::last_fire_tick(), C=dvr::swing::last_pulse_close_tick();
    const bool realTrig=dvr::swing::last_fire_real_trigger();
    bool swingSrc=false; long long dt=-1;
    if (F && T>=F) {
        dt=(long long)(T-F);
        // A state entry: the press was still held, or had stopped within 80 ms (one pad poll
        // gap, the measured 15-16 ms to state entry and the 10 ms sample stride, doubled for
        // a hitch). A combo clip inside a running attack: the game queues a press it got
        // mid-attack and starts the next clip when the current one ends, long after the
        // pulse closed, so the honour window (600 ms) is the bound there.
        if (combo) swingSrc = dt<=600;
        else swingSrc = (C==0 || C<F) || T<=C+80;
    }
    const bool trigger=!swingSrc;
    meleeFireDt=dt;
    text(meleeSource,sizeof(meleeSource),trigger?"trigger":"swing");
    char pulse[40];
    if (!F) text(pulse,sizeof(pulse),"none");
    else if (C==0 || C<F) text(pulse,sizeof(pulse),"open");
    else _snprintf_s(pulse,sizeof(pulse),_TRUNCATE,"closed %lld ms before",(long long)(T>C?T-C:0));
    char dtText[24]; if (dt<0) text(dtText,sizeof(dtText),"none"); else _snprintf_s(dtText,sizeof(dtText),_TRUNCATE,"%lld ms",dt);
    if (!handAnimMelee)
        Log("anim/melee: attack source=%s (entry=%s, last swing #%u fire dt=%s) - [Anim] HandAnimMelee=0, the hand stays on the controller "
            "(F10 Hands > Game arms during actions, or 'anim melee on')",trigger?"TRIGGER":"SWING",combo?"combo":"state",dvr::swing::fires(),dtText);
    else if (trigger)
        Log("anim/melee: attack source=TRIGGER -> hand-back ON (%s): the game's swing plays on the tracked hand and returns "
            "(entry=%s, last swing #%u fire dt=%s, pulse=%s, realTrig=%d, seq=%s)",handAnimMeleeBoth?"both hands":"right hand, the left stays on the controller",
            combo?"combo":"state",dvr::swing::fires(),dtText,pulse,(int)realTrig,s.sequence);
    else
        Log("anim/melee: attack source=SWING (swing #%u) -> hand-back %s (entry=%s, fire dt=%s, pulse=%s, realTrig=%d, seq=%s)",
            dvr::swing::fires(),handAnimMeleeSwing?"ON (HandAnimMeleeSwing=1)":"off: your arm is the animation",combo?"combo":"state",
            dtText,pulse,(int)realTrig,s.sequence);
    return trigger;
}
static bool has_fire_clip(const char* seq) {   // "Pistol_Fire#0", "Crossbow_Fire..." in any case
    for (const char* p=seq; *p; ++p) if (!_strnicmp(p,"fire",4)) return true;
    return false;
}
bool hand_anim_melee() { AcquireSRWLockShared(&lock); bool on=handAnimMelee; ReleaseSRWLockShared(&lock); return on; }
bool hand_anim_fire() { AcquireSRWLockShared(&lock); bool on=handAnimFire; ReleaseSRWLockShared(&lock); return on; }
static void set_hand_anim(bool fire,bool on) {
    AcquireSRWLockExclusive(&lock); (fire?handAnimFire:handAnimMelee)=on;
    char ini[MAX_PATH];text(ini,sizeof(ini),rulesIni);ReleaseSRWLockExclusive(&lock);
    const char* key=fire?"HandAnimFire":"HandAnimMelee";
    if(*ini)WritePrivateProfileStringA("Anim",key,on?"1":"0",ini);
    Log("anim: %s=%d (live, saved; game animation on the tracked hands, arms hidden unless that state shows game arms)",key,on?1:0);
}
void set_hand_anim_melee(bool on) { set_hand_anim(false,on); }
void set_hand_anim_fire(bool on) { set_hand_anim(true,on); }
bool hand_anim_melee_both() { AcquireSRWLockShared(&lock); bool on=handAnimMeleeBoth; ReleaseSRWLockShared(&lock); return on; }
void set_hand_anim_melee_both(bool on) {   // VR-220
    AcquireSRWLockExclusive(&lock); handAnimMeleeBoth=on;
    char ini[MAX_PATH];text(ini,sizeof(ini),rulesIni);ReleaseSRWLockExclusive(&lock);
    if(*ini)WritePrivateProfileStringA("Anim","HandAnimMeleeBothHands",on?"1":"0",ini);
    Log("anim: HandAnimMeleeBothHands=%d (live, saved; %s)",on?1:0,
        on?"both hands follow the game's swing clip":"only the right hand follows it; the left stays on the controller");
}
bool hand_anim_melee_swing() { AcquireSRWLockShared(&lock); bool on=handAnimMeleeSwing; ReleaseSRWLockShared(&lock); return on; }
void set_hand_anim_melee_swing(bool on) {   // VR-220
    AcquireSRWLockExclusive(&lock); handAnimMeleeSwing=on;
    char ini[MAX_PATH];text(ini,sizeof(ini),rulesIni);ReleaseSRWLockExclusive(&lock);
    if(*ini)WritePrivateProfileStringA("Anim","HandAnimMeleeSwing",on?"1":"0",ini);
    Log("anim: HandAnimMeleeSwing=%d (live, saved; %s)",on?1:0,
        on?"a physical swing hands the hand to the game's clip as well":"only a trigger attack does; a physical swing keeps your arm");
}
bool cinematic_enabled() { AcquireSRWLockShared(&lock); bool on=cinematicHandback; ReleaseSRWLockShared(&lock); return on; }
void set_cinematic(bool on) {
    AcquireSRWLockExclusive(&lock); cinematicHandback=on; ReleaseSRWLockExclusive(&lock);
    Log("anim: CinematicHandBack=%d (live; native hands/arms for cinematic states)",on?1:0);
}
bool smooth_blend() { return smoothBlend.load(); }
unsigned blend_in_ms() { AcquireSRWLockShared(&lock); unsigned v=blendInMs; ReleaseSRWLockShared(&lock); return v; }
unsigned blend_out_ms() { AcquireSRWLockShared(&lock); unsigned v=blendOutMs; ReleaseSRWLockShared(&lock); return v; }
void set_smooth_blend(bool on) {
    AcquireSRWLockExclusive(&lock); smoothBlend.store(on); apply_shape();
    char ini[MAX_PATH];text(ini,sizeof(ini),rulesIni);const unsigned i=blendInMs,o=blendOutMs;ReleaseSRWLockExclusive(&lock);
    if(*ini)WritePrivateProfileStringA("Anim","SmoothBlend",on?"1":"0",ini);
    Log("anim: SmoothBlend=%d (live, saved; %s)",on?1:0,on?"eased entry/return, palm on a straight path, hands held through the return":
        "original linear HandBackBlendMs ramp, instant return");
    if(on)Log("anim: SmoothBlend durations: entry %u ms, return %u ms",i,o);
}
void set_blend_ms(unsigned in,unsigned out,bool save) {
    in=in>2000?2000:in; out=out>2000?2000:out;
    AcquireSRWLockExclusive(&lock); blendInMs=in; blendOutMs=out; apply_shape();
    char ini[MAX_PATH];text(ini,sizeof(ini),rulesIni);ReleaseSRWLockExclusive(&lock);
    if (!save) return;   // a slider being dragged: live only
    char v[16];
    _snprintf_s(v,sizeof(v),_TRUNCATE,"%u",in); if(*ini)WritePrivateProfileStringA("Anim","HandBackBlendInMs",v,ini);
    _snprintf_s(v,sizeof(v),_TRUNCATE,"%u",out); if(*ini)WritePrivateProfileStringA("Anim","HandBackBlendOutMs",v,ini);
    Log("anim: SmoothBlend durations entry=%u ms return=%u ms (live, saved; used only while SmoothBlend=1)",in,out);
}
bool cinematic_arms() { AcquireSRWLockShared(&lock); bool on=cinematicArms; ReleaseSRWLockShared(&lock); return on; }
void set_cinematic_arms(bool on) {
    AcquireSRWLockExclusive(&lock); cinematicArms=on;
    char ini[MAX_PATH];text(ini,sizeof(ini),rulesIni);ReleaseSRWLockExclusive(&lock);
    if(*ini)WritePrivateProfileStringA("Anim","CinematicArms",on?"1":"0",ini);
    Log("anim: CinematicArms=%d (live, saved; %s)",on?1:0,on?"tracked arms in cinematics, the game takes them while it animates them; "
        "a hidden arms-only or full-body pawn is unhidden":"cinematics follow CinematicHandBack and the per-state rules; the game's hiding is left alone");
}
CineGate cine_gate() {
    AcquireSRWLockShared(&lock);
    CineGate g{cineStartSpeed,cineStopSpeed,cineRefPoseUu,cineStartMs,cineHoldMs,cineMinSamples};
    ReleaseSRWLockShared(&lock); return g;
}
void set_cine_gate(CineGate g,bool save) {
    if(!std::isfinite(g.start)||!std::isfinite(g.stop)||!std::isfinite(g.refPoseUu))return;
    g.start=std::clamp(g.start,0.1f,500.0f); g.stop=std::clamp(g.stop,0.0f,g.start);
    g.refPoseUu=std::clamp(g.refPoseUu,0.0f,50.0f);
    g.startMs=std::min(g.startMs,5000u); g.holdMs=std::min(g.holdMs,30000u); g.samples=std::clamp(g.samples,1u,20u);
    AcquireSRWLockExclusive(&lock);
    cineStartSpeed=g.start; cineStopSpeed=g.stop; cineRefPoseUu=g.refPoseUu; cineStartMs=g.startMs; cineHoldMs=g.holdMs; cineMinSamples=g.samples;
    char ini[MAX_PATH];text(ini,sizeof(ini),rulesIni);ReleaseSRWLockExclusive(&lock);
    if (!save) return;   // a slider being dragged: live only
    char v[32];
    if(*ini){
        _snprintf_s(v,sizeof(v),_TRUNCATE,"%.1f",g.start); WritePrivateProfileStringA("Anim","CinematicMotionStart",v,ini);
        _snprintf_s(v,sizeof(v),_TRUNCATE,"%.1f",g.stop); WritePrivateProfileStringA("Anim","CinematicMotionStop",v,ini);
        _snprintf_s(v,sizeof(v),_TRUNCATE,"%u",g.startMs); WritePrivateProfileStringA("Anim","CinematicMotionStartMs",v,ini);
        _snprintf_s(v,sizeof(v),_TRUNCATE,"%u",g.holdMs); WritePrivateProfileStringA("Anim","CinematicMotionHoldMs",v,ini);
        _snprintf_s(v,sizeof(v),_TRUNCATE,"%u",g.samples); WritePrivateProfileStringA("Anim","CinematicMotionSamples",v,ini);
        _snprintf_s(v,sizeof(v),_TRUNCATE,"%.2f",g.refPoseUu); WritePrivateProfileStringA("Anim","CinematicRefPoseUu",v,ini);
    }
    Log("anim: cutscene arm gate start=%.1f stop=%.1f uu/s startMs=%u samples=%u holdMs=%u refPoseUu=%.2f (live, saved)",
        g.start,g.stop,g.startMs,g.samples,g.holdMs,g.refPoseUu);
}
CineGateLive cine_gate_live() {
    const unsigned long long now=GetTickCount64();
    CineGateLive l; const float raw=g_msArmSpeed.load();
    l.fresh=raw>=0 && now-g_msArmSpeedMs<=200;
    l.joint=l.fresh?raw:0.0f; l.fastest=l.fresh?g_msArmFastest.load():0.0f; l.refPose=l.fresh?g_msArmRefPose.load():-1.0f;
    l.open=cineGateOpen.load(); l.veto=cineRefVeto.load();
    return l;
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
    if (!valid) { frameWeight=1; frameMantleSplit=false; frameMask=0; weightFrame=~0u; }
    else if (weightFrame!=frame) { frameWeight=handoff.value(now,blendMs); frameMantleSplit=published.mantleSplit; frameMask=published.handMask; weightFrame=frame; }
    const float w=frameWeight;
    ReleaseSRWLockExclusive(&lock); return w;
}
// VR-220: a hand outside the mask keeps its full controller correction through the hand-back.
float weight_for(int hand) {
    const float w=weight();
    AcquireSRWLockShared(&lock); const unsigned char m=frameMask; ReleaseSRWLockShared(&lock);
    return (hand>=0 && hand<2 && (m & (1u<<hand))) ? w : 1.0f;
}
// Ownership including the release hysteresis and, with SmoothBlend, the return blend: the
// mask is non-zero exactly while the game (or a return from it) owns a hand.
bool active() { const Snapshot s=snapshot(); return enabled() && s.valid && s.handMask!=0; }
// Cheap on purpose: SkcRotApply asks per control on every ProcessEvent dispatch. The mask
// is published by tick() as (handback && valid && game) ? handMask : 0, the same test active() makes.
bool hand_owned(int hand) { return hand>=0 && hand<2 && (ownedMask.load() & (1u<<hand)); }
bool choke_owns_arms() { return chokeArms.load(); }
// The whole-draw native path (the split draws the game's own pose, the weapons fall back to
// their native draw) is for a hand-back that owns BOTH hands. A right-hand-only hand-back keeps
// the normal per-hand path: the right hand's correction blends to identity, the left keeps its own.
bool native_draw() {
    if (weight()>0.0001f) return false;
    AcquireSRWLockShared(&lock); const unsigned char m=frameMask; ReleaseSRWLockShared(&lock);
    return m==3;
}
bool native_full_arms() {
    if(!native_draw())return false;
    AcquireSRWLockShared(&lock);const bool full=!frameMantleSplit;ReleaseSRWLockShared(&lock);return full;
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
    dvr::drop::sample(pawn,s);   // VR-111: the drop takedown decision (drop_assist.cpp)
    dvr::movespeed::sample(ctrlLive?g_peCtrl:nullptr,s.valid?pawn:nullptr);   // VR-204: read-only
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
    apply_shape();
    const bool smooth=smoothBlend.load();
    const bool cinematic=cinematicHandback && dvr::scene_state::cinematic(s.state[0]);
    const bool mantle=mantle_pose_requested(mantleHandback,s.state[0]);
    cameraClassifier.update(s.valid,mantle || cinematic || listed(masterRules,s.state[0]) || listed(upperRules,s.state[1]),watch,now,releaseMs,0);
    s.cameraAction=s.valid && cameraClassifier.game;
    // Swing / shot: the same split-native path as mantle. The shot is matched on
    // the clip, not the state, because StatePlayerAction also carries reloads and
    // the sword sneak in/out.
    // VR-220: the swing hand-back is for TRIGGER attacks. Both attack sources reach the
    // game as the same trigger press, so the source is read from the motion sword's own
    // record: an attack state entered while a swing's pulse was open (or within one poll
    // gap of its close) is the player's arm, and the hand stays on the controller. One
    // verdict per attack, latched so it cannot flip mid-clip: a new state entry, or a new
    // clip more than 100 ms after the entry (the first clip lands in the entry's sample; a
    // later one is a combo press). Every ambiguity resolves toward SWING, because pinning a
    // moving arm to the game's clip is the fault this exists to remove, and the cost of the
    // other error is one un-animated trigger hit.
    const bool inMelee=!strcmp(s.state[1],"StatePlayerMeleeAttack");
    if (!inMelee) meleeKey=0;
    else {
        const unsigned long long key=(s.sequenceAt>s.entered[1]+100)?s.sequenceAt:s.entered[1];
        if (key!=meleeKey) {
            meleeKey=key;
            meleeTrigger=classify_melee_source(key,!strcmp(previous.state[1],"StatePlayerMeleeAttack"),s);
        }
    }
    const bool swing=handAnimMelee && inMelee && (meleeTrigger || handAnimMeleeSwing);
    const int fireLane=!strcmp(s.state[1],"StatePlayerAction")?1:!strcmp(s.state[2],"StatePlayerAction")?2:-1;
    const bool fire=handAnimFire && fireLane>0 && has_fire_clip(s.sequence);
    const bool handPose=swing || fire;
    // CinematicArms: inside a cinematic the master state no longer hands the arms back by
    // itself; only the game animating them does (cine_animating: an upper/left action, or
    // the matinee pose blend enabled). Its lane-0 rule and Arms.0.<cinematic> are bypassed.
    // A cutscene is a cinematic FSM state OR the controller's bCinematicMode: Kismet cutscenes
    // also run in ordinary states (the intro's walk-in with Emily was StatePlayerMasterWalk).
    const bool cineMode=cinematicArms && ctrlLive && read_flag(g_peCtrl,cineProps.modeOff,cineProps.modeMask)==1;
    const bool cineArms=cinematicArms && (dvr::scene_state::cinematic(s.state[0]) || cineMode);
    if (!cineArms) { cineMotion=MotionGate{}; cineStats=CineMotionStats{}; cineGateOpen.store(false); cineRefVeto.store(false); }
    const bool cineAnim=cineArms && s.valid && cine_animating(s,pawn);
    const bool rules=(!cineArms && resolve_arm_rule(0,s.state[0])) || resolve_arm_rule(1,s.state[1]) || resolve_arm_rule(2,s.state[2]);
    const bool match=mantle || handPose || rules || cineAnim;
    // VR-283: a takedown the rules hand back draws split hands, not full arms, while the
    // toggle is on. Only when the rule would hand it back at all: an Arms.<lane>.<state>=0
    // override (controller hands) still wins.
    const bool takedownSplit=hideTakedownArms && rules && takedown_state(s.state[0],s.state[1]);
    classifier.update(s.valid,match,watch,now,releaseMs,0);
    // VR-220: which hands this hand-back owns. A trigger sword attack alone owns the right
    // hand (the clip is right-handed); anything else owns both. Held through the release
    // hysteresis so the blend out finishes on the same hands it blended in on.
    handoff.update(s.valid,classifier.game,watch && handback,now,0,blendMs);
    const unsigned char matchedMask=match ? ((swing && !mantle && !fire && !rules && !cineAnim && !handAnimMeleeBoth) ? 2 : 3) : 0;
    // SmoothBlend holds the hands through the return (render_hand_mask); off, the mask
    // drops with the release and the return is instant (the original behaviour).
    s.handMask=smooth ? render_hand_mask(s.valid,watch && handback,matchedMask,previous.handMask,classifier.game,handoff.value(now,blendMs))
                      : !s.valid ? 0 : match ? matchedMask : classifier.game ? previous.handMask : 0;
    s.mantleSplit=s.valid && (mantle ? !resolve_arm_rule(0,s.state[0]) :
        handPose ? !(swing ? resolve_arm_rule(1,s.state[1]) : resolve_arm_rule(fireLane,s.state[fireLane])) :
        takedownSplit ? true :
        (!match && (smooth ? s.handMask!=0 : classifier.game) && previous.mantleSplit));
    // StateWatch still reports the classifier with HandBack disabled.
    s.game=s.valid && classifier.game;
    // VR-220: what hand_owned() answers. SmoothBlend keeps the game's pose under the hand until
    // the return completes, so the return starts from exactly the pose the game left.
    ownedMask.store(smooth ? s.handMask : (handback && s.valid && s.game) ? s.handMask : 0);
    if (s.valid && !strcmp(s.state[0],"StatePlayerMasterChoke")) chokeArms.store(true);
    else if (!s.valid || !s.handMask) chokeArms.store(false);
    if (s.valid) text(s.reason,sizeof(s.reason),match?(cineAnim?"cinematic: the game animates the arms":s.mantleSplit?(swing?"swing native pose with split hands (trigger attack)":fire?"shot native pose with split hands":takedownSplit?"takedown native pose with split hands":"mantle native pose with split hands"):"selected animation arms"):classifier.game?"release hysteresis":s.handMask?"returning to tracked hands":cineArms?"cinematic: tracked arms (CinematicArms)":"no selected active action");
    published=s;
    const bool cineArmsOn=cinematicArms;
    ReleaseSRWLockExclusive(&lock);
    cine_visibility(pawn,s,cineArmsOn,ctrlLive?g_peCtrl:nullptr);   // outside the lock: it calls into the engine
    if (s.valid!=previous.valid || s.game!=previous.game || memcmp(s.state,previous.state,sizeof(s.state)) || s.bodyMode!=previous.bodyMode || strcmp(s.sequence,previous.sequence) || now>=nextBeat) {
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
    dvr::movespeed::configure(ini);
    const int watchSetting=GetPrivateProfileIntA("Anim","StateWatch",-1,ini);
    const int backSetting=GetPrivateProfileIntA("Anim","HandBack",-1,ini);
    watch=watchSetting!=0;
    handback=backSetting!=0;
    cinematicHandback=GetPrivateProfileIntA("Anim","CinematicHandBack",1,ini)!=0;
    Log("config: [Anim] CinematicHandBack=%d",cinematicHandback);
    mantleHandback=GetPrivateProfileIntA("Anim","MantleHandBack",1,ini)!=0;
    Log("config: [Anim] MantleHandBack=%d",mantleHandback);
    hideTakedownArms=GetPrivateProfileIntA("Anim","HideTakedownArms",0,ini)!=0;   // VR-283
    Log("config: [Anim] HideTakedownArms=%d (%s)",hideTakedownArms,
        hideTakedownArms?"takedowns and fatalities: game-animated hands, forearms hidden at the sleeve length":"takedowns and fatalities: full game arms");
    handAnimMelee=GetPrivateProfileIntA("Anim","HandAnimMelee",1,ini)!=0;
    // VR-220: the shipped default moves 0 -> 1 (a trigger attack now plays the game's swing
    // on the tracked hand). The old default is WRITTEN in every installed ini, so a compiled
    // default alone reaches nobody, and a config version bump would drop the machine's tuning
    // (the EdgeSpeed precedent, VR-170). Once per ini: a stored 0 becomes 1 and HandAnimMeleeRev=1
    // is written; after that a 0 is this machine's own choice and stays. A stored 0 is taken as
    // the old default and not as a choice because the lever had never been judged in a headset
    // before this (STATUS 2026-09-19: unverified), so nobody had chosen it on purpose.
    if (GetPrivateProfileIntA("Anim","HandAnimMeleeRev",0,ini)<1) {
        if (!handAnimMelee) {
            handAnimMelee=true;
            WritePrivateProfileStringA("Anim","HandAnimMelee","1",ini);
            Log("config: [Anim] HandAnimMelee 0 -> 1 (one-time: a TRIGGER sword attack now plays the game's swing on the tracked hand; "
                "the stored 0 was the old shipped default, HandAnimMeleeRev=1 written; set it to 0 in F10 or the ini and it stays)");
        } else Log("config: [Anim] HandAnimMelee=1 kept (HandAnimMeleeRev=1 written)");
        WritePrivateProfileStringA("Anim","HandAnimMeleeRev","1",ini);
    }
    handAnimMeleeSwing=GetPrivateProfileIntA("Anim","HandAnimMeleeSwing",0,ini)!=0;   // VR-220
    handAnimMeleeBoth=GetPrivateProfileIntA("Anim","HandAnimMeleeBothHands",0,ini)!=0;
    Log("config: [Anim] HandAnimMeleeSwing=%d HandAnimMeleeBothHands=%d - 0/0 = only a TRIGGER sword attack plays the game's swing, on the RIGHT hand "
        "only (the left stays on the controller); a physical swing keeps your arm. Swing=1: physical swings hand back too. BothHands=1: the left "
        "hand follows the clip as well. 'anim/melee:' names the source and the hands of every attack",(int)handAnimMeleeSwing,(int)handAnimMeleeBoth);
    handAnimFire=GetPrivateProfileIntA("Anim","HandAnimFire",0,ini)!=0;
    Log("config: [Anim] HandAnimMelee=%d HandAnimFire=%d (game animation on the tracked hands, arms hidden)",handAnimMelee,handAnimFire);
    releaseMs=(unsigned)GetPrivateProfileIntA("Anim","ReleaseMs",250,ini); if(releaseMs>5000) releaseMs=5000;
    blendMs=(unsigned)GetPrivateProfileIntA("Anim","HandBackBlendMs",150,ini); if(blendMs>2000) blendMs=2000;
    smoothBlend.store(GetPrivateProfileIntA("Anim","SmoothBlend",0,ini)!=0);
    blendInMs=(unsigned)GetPrivateProfileIntA("Anim","HandBackBlendInMs",250,ini); if(blendInMs>2000) blendInMs=2000;
    blendOutMs=(unsigned)GetPrivateProfileIntA("Anim","HandBackBlendOutMs",350,ini); if(blendOutMs>2000) blendOutMs=2000;
    cinematicArms=GetPrivateProfileIntA("Anim","CinematicArms",0,ini)!=0;
    {   // CinematicArms motion gate: first guesses, set from the cine/motion lines of a run
        char v[32];
        GetPrivateProfileStringA("Anim","CinematicMotionStart","20",v,sizeof(v),ini); cineStartSpeed=(float)atof(v);
        GetPrivateProfileStringA("Anim","CinematicMotionStop","8",v,sizeof(v),ini); cineStopSpeed=(float)atof(v);
        if(!std::isfinite(cineStartSpeed)||cineStartSpeed<0.1f) cineStartSpeed=20.0f;
        if(!std::isfinite(cineStopSpeed)||cineStopSpeed<0||cineStopSpeed>cineStartSpeed) cineStopSpeed=cineStartSpeed*0.4f;
        cineStartMs=(unsigned)std::clamp((int)GetPrivateProfileIntA("Anim","CinematicMotionStartMs",120,ini),0,5000);
        cineHoldMs=(unsigned)std::clamp((int)GetPrivateProfileIntA("Anim","CinematicMotionHoldMs",5000,ini),0,30000);
        cineMinSamples=(unsigned)std::clamp((int)GetPrivateProfileIntA("Anim","CinematicMotionSamples",3,ini),1,20);
        GetPrivateProfileStringA("Anim","CinematicRefPoseUu","1",v,sizeof(v),ini); cineRefPoseUu=(float)atof(v);
        if(!std::isfinite(cineRefPoseUu)||cineRefPoseUu<0||cineRefPoseUu>50) cineRefPoseUu=1.0f;
        Log("config: [Anim] CinematicMotionStart=%.1f Stop=%.1f uu/s StartMs=%u Samples=%u HoldMs=%u CinematicRefPoseUu=%.2f (the game's own arm "
            "motion, measured between the bones of an arm so your own hand cannot open it; a pose within RefPoseUu of the reference pose is "
            "never handed over, 0 = off; F10 Hands > Your arms in cutscenes)",
            cineStartSpeed,cineStopSpeed,cineStartMs,cineMinSamples,cineHoldMs,cineRefPoseUu);
    }
    Log("config: [Anim] SmoothBlend=%d HandBackBlendInMs=%u HandBackBlendOutMs=%u (%s) CinematicArms=%d",(int)smoothBlend.load(),blendInMs,blendOutMs,
        smoothBlend.load()?"eased entry/return, palm on a straight path, hands held through the return":"off: linear HandBackBlendMs, instant return",(int)cinematicArms);
    char buf[1024];
    GetPrivateProfileStringA("Anim","HandBackMaster",masterRules,buf,sizeof(buf),ini); text(masterRules,sizeof(masterRules),buf);
    GetPrivateProfileStringA("Anim","HandBackUpper",upperRules,buf,sizeof(buf),ini); text(upperRules,sizeof(upperRules),buf);
    handoff=Handoff{}; apply_shape();
    Log("config: [Anim] StateWatch=%d (%s) HandBack=%d (%s) ReleaseMs=%u HandBackBlendMs=%u",watch,watchSetting<0?"shipped default":"ini",handback,backSetting<0?"shipped default":"ini",releaseMs,blendMs);
    Log("config: [Anim] master rules=%s | upper rules=%s",masterRules,upperRules);
    ReleaseSRWLockExclusive(&lock);
}
// The state lists are deliberately NOT written: once materialised in an ini they
// beat every later compiled default (TRAPS section 1), and the default list is
// still being tuned by headset runs. An edited list in the ini is kept as is.
void save(const char* ini) {
    AcquireSRWLockShared(&lock);
    const bool w=watch, b=handback, c=cinematicHandback, mantle=mantleHandback, hm=handAnimMelee, hf=handAnimFire, hs=handAnimMeleeSwing, hb=handAnimMeleeBoth; const unsigned r=releaseMs, m=blendMs;
    ReleaseSRWLockShared(&lock);
    char v[16];
    WritePrivateProfileStringA("Anim","StateWatch",w?"1":"0",ini);
    WritePrivateProfileStringA("Anim","HandBack",b?"1":"0",ini);
    WritePrivateProfileStringA("Anim","CinematicHandBack",c?"1":"0",ini);
    WritePrivateProfileStringA("Anim","MantleHandBack",mantle?"1":"0",ini);
    WritePrivateProfileStringA("Anim","HideTakedownArms",takedown_arms_hidden()?"1":"0",ini);   // VR-283
    WritePrivateProfileStringA("Anim","HandAnimMelee",hm?"1":"0",ini);
    WritePrivateProfileStringA("Anim","HandAnimMeleeRev","1",ini);            // VR-220: a saved value is this machine's choice
    WritePrivateProfileStringA("Anim","HandAnimMeleeSwing",hs?"1":"0",ini);   // VR-220
    WritePrivateProfileStringA("Anim","HandAnimMeleeBothHands",hb?"1":"0",ini);
    WritePrivateProfileStringA("Anim","HandAnimFire",hf?"1":"0",ini);
    _snprintf_s(v,sizeof(v),_TRUNCATE,"%u",r); WritePrivateProfileStringA("Anim","ReleaseMs",v,ini);
    _snprintf_s(v,sizeof(v),_TRUNCATE,"%u",m); WritePrivateProfileStringA("Anim","HandBackBlendMs",v,ini);
    WritePrivateProfileStringA("Anim","SmoothBlend",smooth_blend()?"1":"0",ini);
    _snprintf_s(v,sizeof(v),_TRUNCATE,"%u",blend_in_ms()); WritePrivateProfileStringA("Anim","HandBackBlendInMs",v,ini);
    _snprintf_s(v,sizeof(v),_TRUNCATE,"%u",blend_out_ms()); WritePrivateProfileStringA("Anim","HandBackBlendOutMs",v,ini);
    WritePrivateProfileStringA("Anim","CinematicArms",cinematic_arms()?"1":"0",ini);
}
bool command(const char* args) {
    char sub[24]={}, value[24]={}, extra[24]={}; sscanf(args,"%23s %23s %23s",sub,value,extra);
    if (!strcmp(sub,"handback") && (!strcmp(value,"on") || !strcmp(value,"off"))) set_enabled(!strcmp(value,"on"));
    else if (!strcmp(sub,"smooth") && (!strcmp(value,"on") || !strcmp(value,"off"))) set_smooth_blend(!strcmp(value,"on"));
    else if (!strcmp(sub,"blendms") && *value && *extra) set_blend_ms((unsigned)atoi(value),(unsigned)atoi(extra));
    else if (!strcmp(sub,"cinearms") && (!strcmp(value,"on") || !strcmp(value,"off"))) set_cinematic_arms(!strcmp(value,"on"));
    else if (!strcmp(sub,"cinegate")) {   // anim cinegate <start> <stop> <startMs> <holdMs> [samples] [refPoseUu]
        CineGate g=cine_gate(); float a=0,b=0,r=-1; unsigned c=0,d=0,e=0;
        const int n=sscanf(args,"%*s %f %f %u %u %u %f",&a,&b,&c,&d,&e,&r);
        if (n>=4) { g.start=a; g.stop=b; g.startMs=c; g.holdMs=d; if(n>=5)g.samples=e; if(n>=6)g.refPoseUu=r; set_cine_gate(g,true); }
        const CineGateLive l=cine_gate_live(); g=cine_gate();
        Log("anim: cinegate start=%.1f stop=%.1f startMs=%u holdMs=%u samples=%u refPoseUu=%.2f | now: motion %.1f uu/s, fastest point %.1f, "
            "reference-pose distance %.2f, gate %s%s%s",g.start,g.stop,g.startMs,g.holdMs,g.samples,g.refPoseUu,l.joint,l.fastest,l.refPose,
            l.open?"OPEN":"closed",l.veto?" (reference-pose veto)":"",l.fresh?"":" (no fresh measurement)");
        return true;
    }
    else if (!strcmp(sub,"watch") && (!strcmp(value,"on") || !strcmp(value,"off"))) {
        AcquireSRWLockExclusive(&lock); watch=!strcmp(value,"on"); published.valid=false; handoff=Handoff{}; ReleaseSRWLockExclusive(&lock);
    } else if (!strcmp(sub,"melee")) {   // VR-220: the sword hand-back and its source rule
        if (!strcmp(value,"on") || !strcmp(value,"off")) set_hand_anim_melee(!strcmp(value,"on"));
        else if (!strcmp(value,"swing") && (!strcmp(extra,"on") || !strcmp(extra,"off"))) set_hand_anim_melee_swing(!strcmp(extra,"on"));
        else if (!strcmp(value,"both") && (!strcmp(extra,"on") || !strcmp(extra,"off"))) set_hand_anim_melee_both(!strcmp(extra,"on"));
        else if (*value && strcmp(value,"status")) Log("anim: melee on|off | melee swing on|off | melee both on|off | melee status");
        AcquireSRWLockShared(&lock);
        Log("anim/melee: HandAnimMelee=%d HandAnimMeleeSwing=%d HandAnimMeleeBothHands=%d | last attack source=%s fire dt=%lld ms hands=%s | hand-back plays "
            "the game's swing on the %s for a TRIGGER attack%s",(int)handAnimMelee,(int)handAnimMeleeSwing,(int)handAnimMeleeBoth,meleeSource,meleeFireDt,
            published.handMask==3?"both":published.handMask==2?"right":published.handMask==1?"left":"none",
            handAnimMeleeBoth?"tracked hands":"RIGHT hand (the left stays on the controller)",
            handAnimMeleeSwing?" and for a physical swing":"; a physical swing keeps your arm");
        ReleaseSRWLockShared(&lock);
        return true;
    } else if (*sub && strcmp(sub,"status")) Log("anim: status | watch on|off | handback on|off | smooth on|off | blendms <entry> <return> | cinearms on|off | cinegate [<start> <stop> <startMs> <holdMs> [samples] [refPoseUu]] | "
                                                 "melee on|off | melee swing on|off | melee both on|off | melee status");
    report(snapshot()); return true;
}
void status(dvr::status::Writer& w) {
    const Snapshot s=snapshot(); w.obj("anim"); w.kv("valid",s.valid); w.kv("gameOwnsBody",s.game); w.kv("handBack",enabled());
    w.kv("master",s.state[0]); w.kv("upper",s.state[1]); w.kv("left",s.state[2]); w.kv("sequence",s.sequence);
    w.kv("reason",s.reason); w.kv("bodyMode",s.bodyMode); w.kv("controllerWeight",(double)weight());
    const float wl=weight_for(0), wr=weight_for(1);   // before the lock: weight() takes it exclusively, and SRW locks do not nest
    AcquireSRWLockShared(&lock);   // VR-220: the sword hand-back's levers and the last attack's source
    w.kv("handAnimMelee",handAnimMelee); w.kv("handAnimMeleeSwing",handAnimMeleeSwing); w.kv("handAnimMeleeBothHands",handAnimMeleeBoth);
    w.kv("meleeSource",meleeSource); w.kv("meleeFireDtMs",(int)meleeFireDt);
    w.kv("handMask",(int)s.handMask); w.kv("weightLeft",(double)wl); w.kv("weightRight",(double)wr);
    w.kv("smoothBlend",smoothBlend.load()); w.kv("blendInMs",(int)blendInMs); w.kv("blendOutMs",(int)blendOutMs);
    w.kv("cinematicArms",cinematicArms); w.kv("cineUnhides",(int)cineVis.unhides); w.kv("cineRehides",(int)cineVis.rehides);
    ReleaseSRWLockShared(&lock);
    w.end_obj();
}
// Rotation interpolation is implemented separately in the pure math helper.
hf::Xform blend(const hf::Xform& transform) { return blend_transform(transform,weight()); }
hf::Xform blend(const hf::Xform& transform,int hand) { return blend_transform(transform,weight_for(hand)); }   // VR-220
// SmoothBlend: the palm (draw-local) travels a straight line; off, exactly the form above.
hf::Xform blend(const hf::Xform& transform,int hand,const float* palm) {
    const float w=weight_for(hand);
    return smoothBlend.load() && palm ? blend_transform_palm(transform,w,palm) : blend_transform(transform,w);
}
}
