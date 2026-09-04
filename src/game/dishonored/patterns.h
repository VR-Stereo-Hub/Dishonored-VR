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

// ---- UE3 globals ----
static const uintptr_t kGObjHdr = 0x1423630; // TArray<UObject*> {Data,Num,Max}
static const uintptr_t kGNamesData = 0x1435674;
static const uintptr_t kGNamesNum  = 0x1435678;

// ---- UObject layout ----
static const uint32_t  kNameOff  = 0x28;
static const uint32_t  kClassOff = 0x30;
static const uint32_t  kOuterOff = 0x24;

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
static const uint32_t kPcRotBase[]  = { 0x9c, 0xd0 };   // controller: the source
static const uint32_t kCamRotBase[] = { 0x9c, 0xd0 };   // camera POV + its cache
static const uint32_t kPovOffs[3] = {0x330, 0x350, 0x374};
static const uint32_t kFovCands[4] = {0x53c, 0x540, 0x564, 0x254};
static const uint32_t kLevCtrl[3] = {0x3ac, 0x3b0, 0x3b4};   // FOVAngle/Desired/Default
static const uint32_t kLevCam[7]  = {0x254, 0x348, 0x368, 0x38c, 0x53c, 0x540, 0x564};

// ---- Engine code hooks (byte-verified before patching) ----
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
static const uintptr_t kGameEngineTick = 0x00632860;            // UGameEngine::Tick (derivation only)
static const uint32_t  kViewportClientOff = 0x1c;               // FViewport -> its client (derivation only)

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

// ---- The Scaleform HUD draw class (41.2, measured live 2026-09-04, session 10;
// ENGINE_NOTES "The Scaleform HUD draw class, measured") ----
// Dishonored draws its world into an OFFSCREEN scene target the size of the
// render (2496x2688 in the shipped configuration) and paints the whole HUD onto
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
static const bool     kHudFingerprintMeasured = true;
static const uint32_t kHudSceneTargetIsOffscreen = 1;   // the world never draws to the backbuffer
static const uint32_t kHudRequiresFullViewport   = 1;   // every measured HUD draw covers the target
static const uint32_t kHudRequiresDepthOff       = 1;   // D3DRS_ZENABLE == D3DZB_FALSE on all of them
// The tail of the frame the HUD occupied, as a FRACTION of the present's draws.
// Diagnostic only: nothing gates on it, because a bucket's ordinal moves with
// what is on screen. 1177/1205 in gameplay, 1126/1221 in the pause menu.
static const float    kHudTailFractionSeen = 0.92f;
