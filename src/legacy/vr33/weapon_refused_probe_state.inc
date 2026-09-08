// ---- THE PROBE --------------------------------------------------------------
//
// The first ghost detector reported ZERO and that reading is trustworthy: no
// refused draw shares a matched contract's exact geometry. It also exposed its
// own flaw. Its cheap pre-filter was the PRIMITIVE COUNT, so a pass drawing a
// different number of triangles from the same mesh - a shadow LOD, a separate
// section, a different index range - was rejected before anything looked at
// it. A filter that can only find what it already assumed is not evidence.
//
// The probe drops that assumption. On a refused draw it compares the VERTEX
// BUFFER against the contracts, which is the loosest useful key, and reports
// what DIFFERS about anything that hits: index buffer, range, shader,
// primitive count. That distinguishes the three live possibilities - the same
// buffers at another range, another mesh entirely, or a pass that never
// reaches this router at all - which have so far been indistinguishable.
//
// Bounded per Present so it cannot cost the frame, and it reports the budget
// it used so a truncated probe is never read as an absence.
static bool g_waProbe       = true;   // [Hands] AttachProbe
static int  g_waProbeBudget = 400;    // device reads per Present
static LONG g_waProbeRan    = 0;
static LONG g_waProbeVbHit  = 0;   // shares a contract's vertex buffer
static LONG g_waProbeIbHit  = 0;   // ... and its index buffer
static LONG g_waProbeMiss   = 0;   // not our geometry at all
static LONG g_waProbeCapped = 0;   // refused draws the budget did not reach

static LONG g_waProbeIbOnly = 0;
