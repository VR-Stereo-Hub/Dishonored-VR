// State chunk 52 for game/dishonored/head_track - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// ----------------------------------------------------------------------------
// Stage 6.0 — Index controllers as a virtual Xbox-360 gamepad
// ----------------------------------------------------------------------------
// Dishonored.exe statically imports XINPUT1_3.dll ordinal 2 (XInputGetState)
// and ordinal 3 (XInputSetState) — nothing else. So the game decides "a pad is
// plugged in" purely from XInputGetState succeeding. The exe has a fixed base
// (0x400000, no ASLR — verified by the UE3 probe), so we patch its two IAT
// slots directly and serve a synthesized pad built from the Index controllers
// each frame. Real analog movement, and the game's rumble comes back out as
// controller haptics. A real 360 pad still works whenever the VR controllers
// are off (we pass through unless both sides are live).
static const uintptr_t kXIGetSlot = 0x00f946c4; // IAT slot: xinput1_3 ord 2
