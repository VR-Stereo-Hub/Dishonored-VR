// State chunk 40 for game/dishonored/hands/arms_hide - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// 30.14: the +0x288 byte poke was wrong - that array turned out to be some
// per-bone animation control (arms froze to the view and rode the head).
// Do it the engine's way instead: UE3 ships native HideBoneByName /
// UnHideBoneByName on skeletal components. Call them through the engine's
// own ProcessEvent - the engine allocates BoneVisibilityStates itself, so
// the "never point engine arrays at our memory" trap never comes up.
typedef void (__fastcall *PeCall_t)(void* self, void* edxDummy,
                                    void* fn, void* parms, void* result);
