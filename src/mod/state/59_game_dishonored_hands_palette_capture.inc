// Live reflection state; packet capture is an optional legacy diagnostic.
enum { PC_MAX_BYTECODE = 64 * 1024 };
// THE LIVE LAYOUT, for placement rather than capture. Keyed on the shader
// pointer, which is cheap to compare per draw, and re-reflected whenever it
// changes. A recycled pointer could in principle hand back a stale layout;
// that is caught downstream, where the placement validates that LocalToWorld
// is rigid and the camera basis orthonormal with a unit forward row. A wrong
// layout does not satisfy those by accident, so it refuses rather than placing
// a hand on numbers from the wrong registers.
static void*    g_pcLayShader = NULL;
static int      g_pcLayVp   = -1;
static int      g_pcLayL2W  = -1;
static int      g_pcLayBones = -1;
static int      g_pcLayBonesN = 0;
// The PARTIAL reflection: BoneMatrices from a shader that declares no
// LocalToWorld. Such a shader cannot be identified by transform, but its
// geometry can be identified by a sibling pass that was, and it can then be
// corrected. See the ghost-pass note in state chunk 57b.
static int      g_pcLayBonesPartial  = -1;
static int      g_pcLayBonesNPartial = 0;
static LONG     g_pcLayFail = 0;

#if DVR_WITH_LEGACY
#include "legacy/vr33/palette_packet_capture_state.inc"
#endif
