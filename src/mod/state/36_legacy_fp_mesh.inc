// State chunk 36 for legacy/fp_mesh - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// The raw dump pinned the component's relative-transform block exactly:
//
//   +0x190  Translation  FVector   (0,0,0)
//   +0x19c  Rotation     FRotator  (0,0,0)   <- three int32, pitch/yaw/roll
//   +0x1a8  Scale        float     1.0
//   +0x1ac  Scale3D      FVector   (1,1,1)
//   +0x1b8  BoundsScale  float     1.0
//   +0x1bc  MaxDrawDistance / +0x1c0 CachedMaxDrawDistance  ~30017
//
// Scale=1 followed by Scale3D=(1,1,1) is the fingerprint that locks the block
// down, and the two draw distances immediately after confirm it. These are the
// INPUTS the engine composes LocalToWorld from, so writing Rotation makes the
// engine apply our rotation itself, every frame, instead of us losing a race
// against its own recompute - which is what "engine-left-it-alone=0" meant.
//
// Note both dumped world matrices are yaw-only (row1 is (0,0,-1) flat): the
// arms' pitch comes from animation, not from this transform. So a relative
// pitch here is ours to use for hand aiming.
