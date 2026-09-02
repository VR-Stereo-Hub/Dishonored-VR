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

