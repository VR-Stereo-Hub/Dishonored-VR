// State chunk 29 for game/dishonored/motion_aim - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// Full catch of a fresh (or pool-reused) projectile.
// The catch works, but "vel-rewrites=0" says we found no velocity to rotate:
// at the moment the engine announces the arrow, its velocity has not been set
// yet. So we hold on to the object and keep steering it for a few frames,
// which is when the engine fills the velocity in. No scanning - we already
// have the exact pointer.
static bool g_aimProbe = false;   // Shift+F6 re-runs the velocity-field dump
struct SteerObj { uint8_t* ptr; double until; float dir[3]; int probed; bool steered; };
static SteerObj g_steer[8];
static int      g_steerN = 0;
