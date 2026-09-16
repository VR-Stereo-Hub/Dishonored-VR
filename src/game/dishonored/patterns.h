// game/dishonored/patterns.h - EVERY fixed address and engine layout number
// the mod relies on, in one place.
//
// Dishonored.exe (Steam, patch 1.4, UE3 build 9099) is a 32-bit image with no
// ASLR: it always loads at 0x400000, so the mod uses absolute addresses. Each
// code hook verifies the bytes it expects before patching and refuses on a
// mismatch - that refusal is the only build check there is, so a different exe
// build degrades to "feature off" instead of a crash. Every entry is documented
// with its derivation in docs/dishonored/ENGINE_NOTES.md; never add a number
// anywhere else, and never copy one from another game.
#pragma once
#include <stdint.h>

static_assert(sizeof(void*) == 4, "Dishonored is a 32-bit game; the addresses here are 32-bit");

// ---- Module image (no ASLR: base 0x400000) ----
static const uintptr_t kModBase    = 0x400000;
static const uintptr_t kModEnd     = 0x400000 + 0x1206A0C; // end of .reloc
static const uintptr_t kDataStart  = 0x400000 + 0xE69000;  // .data VA
static const uintptr_t kDataEnd    = kDataStart + 0x21B3BC;

// VR-125: D3D9 query-read helper, thiscall + four stack args, ret16.
// Complete polling loop preserved by diagnostic; ENGINE_NOTES derivation.
static const uintptr_t kD3D9QueryRead = 0x009bcf50;
static const uint8_t kD3D9QueryReadPrefix[15] = {0x55,0x8b,0xec,0x83,0xec,0x1c,0x56,0x8b,0x75,0x08,0x89,0x4d,0xfc,0x85,0xf6};

// ---- GC fault capture (read-only, no hook or engine-memory write) ----
// Offline image verification and first-fault registers: ENGINE_NOTES,
// "GC reference crash recurrence, 2026-09-13". Reads referenced object flags.
static const uintptr_t kGcReferenceReadFault = 0x00465894;
static const uint8_t kGcReferenceReadBytes[] = {0x8B,0x50,0x08,0x8B,0x48,0x0C};

// ---- UE3 globals ----
// Existing FpComputePivots component matrix reads, centralized for VR-33.
// Native row-vector FMatrix: basis rows followed by translation row.
static const uint32_t kWaComponentLocalToWorld = 0x60;
static const uint32_t kWaComponentTranslation = 0x90;
// UE3 reflection: every UProperty is itself a UObject whose Outer is the class
// that declares it, which is what FindPropOffset uses. These two were derived by
// the 38.x skelcontrol property dump (the Offset column identifies itself: small,
// distinct, ascending in declaration order and below the class instance size)
// and have been in production use since. Promoted here from inline literals in
// uobject.cpp so one place owns them; ENGINE_NOTES records the derivation.
static const uint32_t kUPropOffset  = 0x5c;   // UProperty::Offset
static const uint32_t kUBoolBitMask = 0x6c;   // UBoolProperty::BitMask
static const uintptr_t kGObjHdr = 0x1423630; // TArray<UObject*> {Data,Num,Max}
static const uintptr_t kGNamesData = 0x1435674;
static const uintptr_t kGNamesNum  = 0x1435678;

// ---- UObject layout ----
static const uint32_t  kNameOff  = 0x28;
static const uint32_t  kClassOff = 0x30;
static const uint32_t  kOuterOff = 0x24;

// ---- SkeletalMeshComponent layout (VR-31, resolved from UE3 reflection at
// runtime; these are the values measured on this build 2026-09-06, kept here so
// a log line can be read without a live game). This build declares NO
// BoneVisibilityStates, which is why per-bone hiding has nothing to drive.
static const uint32_t  kSkcIndexOff = 0x288;  // SkelControlIndex (0xFF = none)
static const uint32_t  kReqBonesOff = 0x23c;  // RequiredBones

// ---- Camera object ----
// A row-major basis at +0x50..+0x7F: forward, right, up (the original
// author's FindPovRotators matched +0x50 against the POV rotator's forward;
// 41.1 validates the three rows orthonormal before writing along them).
static const uint32_t  kCamFwd   = 0x50;   // basis X (forward) row
static const uint32_t  kCamRight = 0x60;   // basis Y (right) row
static const uint32_t  kCamUp    = 0x70;   // basis Z (up) row
static const uint32_t  kCamLoc0  = 0x80;   // matrix translation row
static const uint32_t  kCamLoc1  = 0x90;   // cached POV loc
static const uint32_t  kCamLoc2  = 0xC4;   // cached POV loc 2
// DO NOT USE kPcRotBase ON A PlayerController. Measured 2026-09-05: 0x9c on the
// controller is a FLOAT (0xbcc8cea0 = -0.0245), not a rotator - it was copied from
// kCamRotBase below, which is the CAMERA's, and the two being one literal is the
// tell. Reading it fed -1127690592 into the body-yaw hold, which then pinned the
// pawn's yaw to that constant: the arms froze and the stick could not turn. The
// controller's rotation is Actor.Rotation, resolved by FindPropOffset like the
// pawn's. Left here only because the camera entry below still needs the values.
static const uint32_t kPcRotBase[]  = { 0x9c, 0xd0 };   // RETIRED - see above
static const uint32_t kCamRotBase[] = { 0x9c, 0xd0 };   // camera POV + its cache

// VR-57: FireCrossbow's final spawn arguments, after both spawn-location
// branches converge, before vector->rotator, SpawnActor and velocity init.
// Derived offline from the class constructor/vtable and the initializer's
// direction*speed stores. See ENGINE_NOTES, VR-57 native crossbow fire seam.
static const uintptr_t kCrossbowSpawnAim = 0x00C38BBB;
static const uint8_t kCrossbowSpawnAimBytes[] = {0x8B,0x4D,0xAC,0x57,0x57,0x51};
static const int kCrossbowSourceLocal = -0x54; // native source pawn
static const int kCrossbowSpawnLocal = -0xB8;  // float3, chosen spawn position
static const int kCrossbowDirectionLocal = -0xAC; // float3, used for spawn and init
static const uintptr_t kCrossbowContextVtable = 0x01172C80;
static const uintptr_t kCrossbowInitCall = 0x00C38DB6;
static const uint8_t kCrossbowInitCallBytes[] = {0xFF,0xD0,0xF6,0x86,0xD4,0,0,0,1};

// VR-82: the pistol's equivalent seam, derived offline by re-walking the route
// above and reproducing every published crossbow number first. See
// docs/dishonored/VR-82-PISTOL-FIRE-SEAM.md and ENGINE_NOTES.
//
// The join sits AFTER 0x00BFFBA0, which receives the direction local by address
// and can still write it. A hook placed at the natural-looking spot - right
// after the aim cache returns - would be overwritten and would change nothing
// while its counter moved. Do not move this address earlier.
static const uintptr_t kPistolSpawnAim = 0x00C2A53C;
static const uint8_t kPistolSpawnAimBytes[] = {0x8D,0x4D,0x94,0x51,0x8D,0x4D,0xB8};
static const int kPistolSourceLocal = -0x1C;    // native source pawn
static const int kPistolSpawnLocal = -0x54;     // float3, chosen spawn position
static const int kPistolDirectionLocal = -0x48; // float3, used for spawn and init
static const int kPistolAimDirLocal = -0x60;    // float3, the ORIGINAL unit aim dir
static const int kPistolTweaksLocal = -0x18;    // the DisTweaks_FirePistol object
static const uintptr_t kPistolContextVtable = 0x01172E60;
static const uintptr_t kPistolInitCall = 0x00C2A611;
static const uint8_t kPistolInitCallBytes[] = {0xFF,0xD2,0x8B,0x06,0x8B,0x90,0x48,1,0,0};
// DisTweaks_FirePistol::m_fBulletSpawnDistance. The routine multiplies the aim
// direction by this and adds it to the origin to get the spawn position, which
// is the whole reason the pistol needs its own solver entry point.
static const uint32_t kPistolSpawnDistOff = 0x420;
static const uint32_t kPovOffs[3] = {0x330, 0x350, 0x374};
static const uint32_t kFovCands[4] = {0x53c, 0x540, 0x564, 0x254};
static const uint32_t kLevCtrl[3] = {0x3ac, 0x3b0, 0x3b4};   // FOVAngle/Desired/Default
static const uint32_t kLevCam[7]  = {0x254, 0x348, 0x368, 0x38c, 0x53c, 0x540, 0x564};

// ---- Engine code hooks (byte-verified before patching) ----
// UDishonoredPlayerPawn::FaceRotation - the operation that faces the body.
// DERIVED 2026-09-05, statically, from a runtime-resolved starting point:
//   1. armfollow/nfp resolved the FaceRotation UFunction and reported its exec
//      thunk at RVA 0x1DAF30 - the SAME thunk for Pawn and DishonoredPlayerPawn.
//   2. The thunk ends `mov edx,[edi]; mov edx,[edx+0x3f0]; mov ecx,edi; call edx`,
//      so FaceRotation is VIRTUAL at vtable offset +0x3F0 (slot 252).
//   3. The live pawn's vtable (RVA 0xD1A3B0) slot 252 holds this address.
//   4. It ends in `ret 0x10` - 16 bytes of stack args - and has ZERO static
//      E8/E9 callers, i.e. it is only ever reached through the vtable, which is
//      why gameplay never dispatched it through ProcessEvent (measured: 0 hits
//      across a whole run) and why hooking the script exec wrapper catches
//      nothing.
// __thiscall: this in ecx; stack +0 Pitch, +4 Yaw, +8 Roll, +0xC DeltaTime.
static const uintptr_t kFaceRotation = 0x00AB0D40;
static const uint8_t   kFaceRotationBytes[5] = { 0x55, 0x8B, 0xEC, 0x51, 0x53 };

static const uintptr_t kProcessEvent = 0x00470640;
static const uintptr_t kCamHookAt = 0x56dd36; // epilogue (5 bytes: 5E 8B E5 5D C3)
static const uintptr_t kBlkAimHook = 0x00bf595f;   // the first movss, 5 bytes
static const uintptr_t kBlkAimBack = 0x00bf5964;   // resume at the second
static const uint8_t   kBlkAimOrig[5] = { 0xf3, 0x0f, 0x10, 0x45, 0xf4 };
static const uintptr_t kBlkDstHook = 0x00bf5e4f;
static const uintptr_t kBlkDstBack = 0x00bf5e55;
static const uint8_t   kBlkDstOrig[6] = { 0x8d, 0x85, 0x30, 0xff, 0xff, 0xff };
static const uintptr_t kBlkDirHook = 0x00bf55a3;
static const uintptr_t kBlkDirBack = 0x00bf55a8;
static const uint8_t   kBlkDirOrig[5] = { 0x8b, 0x08, 0x89, 0x4d, 0xb4 };
static const uintptr_t kBlkTrcHook = 0x00bf5d1a;
static const uintptr_t kBlkTrcBack = 0x00bf5d1f;
static const uint8_t   kBlkTrcOrig[5] = { 0xf3, 0x0f, 0x11, 0x55, 0xd8 };

// ---- The scene-draw root (41.1, derived live 2026-09-03; ENGINE_NOTES "The
// scene-draw root, derived live") ----
// FViewport::Draw's analog: __thiscall on the viewport, ONE stack arg
// (bShouldPresent; `ret 4` at +0x1fc), SEH prologue. Its body builds a stack
// canvas, calls the viewport client's Draw through [viewport+0x1c] -> vtable
// slot 2, tears the canvas down; the present is enqueued to the render thread
// from its tail. UGameEngine::Tick (0x632860, reached through the engine
// vtable's Tick at +0x124 from the main loop) calls it ONCE per tick at
// 0x6330dc as `push 1; call` with ecx = GameViewport->Viewport - the gameplay
// dispatcher, and the ONLY site the re-entry patches (3 static E8 callers
// exist; the other two are not gameplay). Byte-verified before patching.
static const uintptr_t kViewportDraw = 0x005fc5b0;
static const uint8_t   kViewportDrawPrologue[16] = { 0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0xa3, 0x97,
                                                     0xf2, 0x00, 0x64, 0xa1, 0x00, 0x00, 0x00, 0x00 };
static const uint32_t  kViewportDrawRetImm = 4;                 // one stack arg
static const uintptr_t kViewportDrawCallSite = 0x006330da;      // push 1; call rel32 (7 bytes)
static const uint8_t   kViewportDrawCallSiteOrig[7] = { 0x6a, 0x01, 0xe8, 0xcf, 0x94, 0xfc, 0xff };
static const uintptr_t kViewportDrawGameplayRet = 0x006330e1;   // the return address of that call
// VR-80: the root's OTHER static callers (tools/disasm-rva.py calls 0x1fc5b0 lists
// exactly four E8/E9 sites; 0x6330dc is the gameplay one above). A draw from any
// of these does not pass the stub, so it presents with no eye tag. Diagnostic
// retarget only ([Stereo] DrawCallerTrace): each site's bytes are verified, the
// stub counts and calls the root with the same argument. ENGINE_NOTES, "the
// viewport draw root's callers".
static const uintptr_t kViewportDrawCallerA = 0x004dba66;       // push 1; call root
static const uint8_t   kViewportDrawCallerAOrig[7] = { 0x6a, 0x01, 0xe8, 0x43, 0x0b, 0x12, 0x00 };
static const uintptr_t kViewportDrawCallerB = 0x0061236a;       // jmp root (tail call)
static const uint8_t   kViewportDrawCallerBOrig[5] = { 0xe9, 0x41, 0xa2, 0xfe, 0xff };
static const uintptr_t kViewportDrawCallerC = 0x00641d85;       // push 0; call root
static const uint8_t   kViewportDrawCallerCOrig[7] = { 0x6a, 0x00, 0xe8, 0x24, 0xa8, 0xfb, 0xff };
static const uintptr_t kGameEngineTick = 0x00632860;            // UGameEngine::Tick (derivation only)
static const uint32_t  kViewportClientOff = 0x1c;               // FViewport -> its client (derivation only)

// VR-43: only the GFx SetBlackStripes visibility query. Verified native
// caller and ret4 helper are documented in ENGINE_NOTES, 2026-09-13.
static const uintptr_t kLetterboxQuerySetup = 0x00b96115;
static const uint8_t kLetterboxQueryOrig[7] = {0x6a,0x10,0xe8,0x14,0x40,0xe5,0xff};
static const uintptr_t kLetterboxAnyMask = 0x009ea130;
static const uintptr_t kLetterboxQueryReturn = 0x00b9611c;
static const uint32_t kLetterboxMask = 0x10;

// ---- Import table slots ----
static const uintptr_t kXIGetSlot = 0x00f946c4; // IAT slot: xinput1_3 ord 2
static const uintptr_t kXISetSlot = 0x00f946c0; // IAT slot: xinput1_3 ord 3

// ---- SkelControl (AnimTree bone-override node) fields ----
static const uint32_t kSkcName    = 0x5c;   // FName ControlName
static const uint32_t kSkcStr     = 0x64;   // float ControlStrength
static const uint32_t kSkcBools   = 0xb8;   // bApply/bAdd/bRemove bitfield
static const uint32_t kSkcTrans   = 0xbc;   // FVector BoneTranslation
static const uint32_t kSkcTSpace  = 0xc8;   // BYTE  BoneTranslationSpace
static const uint32_t kSkcRSpace  = 0xc9;   // BYTE  BoneRotationSpace
static const uint32_t kSkcRot = 0xd4;      // FRotator BoneRotation
static const uint32_t kSkcScaleProp = 0xa0;
static const uint32_t kSkcApplyTrans = 0x01;
static const uint32_t kSkcApplyRot   = 0x02;
static const uint32_t kSkcAddTrans   = 0x04;
static const uint32_t kSkcAddRot     = 0x08;

// ---- SkeletalMeshComponent fields ----
static const uint32_t kMeshTrans = 0x190;
static const uint32_t kMeshRot   = 0x19c;
static const uint32_t kMeshScale = 0x1a8;
static const uint32_t kMeshScl3D = 0x1ac;

// VR-104: existing arms cull pair, measured transform-input layout (ENGINE_NOTES).
static constexpr uint32_t kArmDrawDistancePair = 0x1bc;

// VR-108: movie-service presentation query, derived from overlay draw gate.
// See ENGINE_NOTES. Read fields only after matching vtable/query bytes.
static constexpr uintptr_t kBinkServiceVtable = 0x010a4610;
static constexpr uintptr_t kNullMovieServiceVtable = 0x00fcde60;
static constexpr uintptr_t kBinkPresentQuery = 0x00932cc0;
static constexpr uintptr_t kNullMoviePresentQuery = 0x00722980;
static constexpr uint32_t kMoviePresentSlot = 0x1c;
static constexpr uint32_t kBinkPresentActive = 0x130;
static constexpr unsigned char kBinkPresentQueryBytes[] = {0x8b,0x81,0x30,0x01,0x00,0x00,0xc3};
static constexpr unsigned char kNullMoviePresentQueryBytes[] = {0x33,0xc0,0xc3};

// VR-108: Engine.WaitMovie -> service+1c manual-reset completion event.
static constexpr uint32_t kMovieCompletionEvent = 0x1c;
static constexpr uintptr_t kMovieEventVtable = 0x00fb98a8;
static constexpr uint32_t kMovieEventHandle = 4;
static constexpr uintptr_t kMovieEventWait = 0x00416670;
static constexpr uint32_t kMovieEventWaitSlot = 0x14;
static constexpr unsigned char kMovieEventWaitBytes[] = {0x55,0x8b,0xec,0x8b,0x45,0x08,0x8b,0x49,0x04,0x50,0x51,0xff,0x15,0xd0,0x41,0xf9,0x00,0xf7,0xd8,0x1b,0xc0,0x40,0x5d,0xc2,0x04,0x00};
static constexpr uintptr_t kMovieEventCreate = 0x00500cfe;
static constexpr unsigned char kMovieEventCreateBytes[] = {0x8b,0x0d,0x94,0x34,0x42,0x01,0x8b,0x01,0x8b,0x50,0x04,0x53,0x6a,0x01,0xc6,0x45,0xfc,0x0b,0xff,0xd2,0x89,0x46,0x1c};
// ---- The Scaleform HUD draw class (41.2, measured live 2026-09-04, session 10;
// ENGINE_NOTES "The Scaleform HUD draw class, measured") ----
// Dishonored draws its world into an OFFSCREEN scene target the size of the
// render (2496x2688 when measured; 2750x2850 shipped since) and paints the whole HUD onto
// the BACKBUFFER at the tail of the frame; the scene is resolved to the
// backbuffer with StretchRect, not a draw. So the render target alone separates
// the HUD from the world, with no overlap - a simpler discriminator than the
// DXVK fork needed, because on the fork's frame the two shared one target.
//
// Measured (run 46-02, the sewers, `stereo reentry`, 2496x2688): 1205 draws per
// present, of which the backbuffer takes 5 buckets and 15.0 draws at ordinals
// 1177..1221; in the pause menu 10 buckets and 95.9 draws at 1126..1223. EVERY
// backbuffer bucket carries a full-viewport draw with depth disabled. Proven by
// picture, not by counter: `draws kill` on those buckets removed the health and
// blood indicator and left the world pixel-identical, and killing the whole
// population removed the HUD entirely (captures kill-a..kill-d).
//
// The two terms the fork used that do NOT hold here, both measured:
//   - portrait targets: the fork rejected them because its side-by-side frame
//     was landscape. Our per-eye render is 2496x2688 and portrait.
//   - user-pointer draws only: DrawIndexedPrimitiveUP carries 810 WORLD draws
//     per present on this path, so the entry point discriminates nothing.
// And one that holds but must not be used as a gate: texture stage 0. Most HUD
// draws are untextured fills, so requiring a texture keeps 1 draw of 15.
//
// The MENU is drawn by the same class (measured above), so a redirect must be
// gated on the game state, not on the draw: that is the original's inherited
// bug (HANDOFF 8.4, the main menu on the wrist), and DvrGameplayVerdict's own
// !mainMenu / !menuOpen terms are the positive signal it lacked.
//
// THE FOURTH TERM, and the reason it exists (run 46-04, found by picture): the
// backbuffer's population is not all HUD. The SCENE RESOLVE is a draw too - one
// full-screen textured quad, two primitives, that copies the finished world
// over - and it is the single draw that would carry the entire world onto the
// panel. It did: with the first three terms the panel held the whole frame, and
// with the same class killed the panel was empty, so one of the fifteen was
// painting the world. ALPHA BLENDING tells them apart, and not arbitrarily:
// something drawn ONTO a finished frame must blend to sit over it, while the
// frame itself is written opaquely. The resolve is the only opaque draw in the
// population; every HUD element blends. 14 draws of the 15 survive.
//
// (The fork's samples-rt term would not have caught it here: this game's
// resolve samples a plain texture copy rather than a surface flagged as a
// render target, which is also why a tonemap detector keyed on that read 0 for
// a whole run.)
static const bool     kHudFingerprintMeasured = true;
static const uint32_t kHudSceneTargetIsOffscreen = 1;   // the world never draws to the backbuffer
static const uint32_t kHudRequiresFullViewport   = 1;   // every measured HUD draw covers the target
static const uint32_t kHudRequiresDepthOff       = 1;   // D3DRS_ZENABLE == D3DZB_FALSE on all of them
static const uint32_t kHudRequiresAlphaBlend     = 1;   // excludes the opaque scene resolve
// The tail of the frame the HUD occupied, as a FRACTION of the present's draws.
// Diagnostic only: nothing gates on it, because a bucket's ordinal moves with
// what is on screen. 1177/1205 in gameplay, 1126/1221 in the pause menu.
static const float    kHudTailFractionSeen = 0.92f;

// Engine-labelled InitViews: two direct callers; thiscall, no stack arguments.
// First six whole non-relative bytes are sufficient for the trampoline.
static constexpr uintptr_t kSceneInitViews = 0x008662A0;
static const uint8_t kSceneInitViewsPrefix[] = {0x53,0x8B,0xDC,0x83,0xEC,0x08,0x83,0xE4,0xF0,0x83,0xC4,0x04,0x55,0x8B,0x6B,0x04};

// ProcessViewFrustumCulling: cdecl one renderer argument, caller cleans stack.
static constexpr uintptr_t kSceneFrustumCull = 0x00864AD0;
static const uint8_t kSceneFrustumCullPrefix[] = {0x53,0x8B,0xDC,0x83,0xEC,0x08,0x83,0xE4,0xF0,0x83,0xC4,0x04,0x55,0x8B,0x6B,0x04};
// Exact reflection-culling selector at VA00864CCA..00864CE4.
// Read only during the borrowed renderer invocation; no retained identity.
static constexpr size_t kSceneRendererFamilyPointer = 0x60;
static constexpr size_t kSceneFamilyReflectionBranch = 0x48;

// VR-50 live resize: F11 at RVA5C8E7D..5C8F0A calls the primary viewport
// vtable slot1 with SIX stack args. Constructor RVA5C6810 sets both tables.
// FViewport base is native WindowsViewport+4; GameViewportClient.Viewport
// comes from the existing gameplay Draw call site at RVA2330D3.
static const uintptr_t kWindowsViewportVtable=0x010c1870;
static const uintptr_t kWindowsFViewportVtable=0x010c17d8;
static const uintptr_t kWindowsViewportResize=0x009c5b30;
static const uint8_t kWindowsViewportResizePrologue[]={0x55,0x8b,0xec,0x83,0xec,0x40,0x53,0x56,0x8b,0xd9,0x33,0xf6};
static const uintptr_t kWindowsViewportResizeReturn=0x009c6096;
static const uint32_t kWindowsFViewportBase=4;
static const uint32_t kGameViewportNativeViewport=0x40;
static const uint32_t kWindowsViewportHwnd=0x68;
static const uint32_t kFViewportFlags=0x5c;
static const uint32_t kWindowsViewportPosX=0x4e4;
static const uint32_t kWindowsViewportPosY=0x4e8;
