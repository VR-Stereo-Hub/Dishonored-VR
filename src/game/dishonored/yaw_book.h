#pragma once
#include <cstdint>
// Shared production bookkeeping and host regression tests.
struct YawBook { int64_t headContrib; int32_t viewOut; bool have; };

// One FRESH view computation: the engine handed us `incomingView`, we injected
// `headDeltaU` (already clamped and signed). Call exactly once per fresh write.
static void YawBookFresh(YawBook* b, int32_t incomingView, int32_t headDeltaU, bool resume=false)
{
    if(resume) b->headContrib=0;
    b->viewOut = (int32_t)((uint32_t)incomingView + (uint32_t)headDeltaU);
    b->headContrib += headDeltaU;
    b->have = true;
}
// The body heading that view implies. int32 wrap keeps multiple revolutions
// continuous instead of saturating.
static int32_t YawBookBody(const YawBook* b)
{
    return (int32_t)((uint32_t)b->viewOut - (uint32_t)(int32_t)(b->headContrib & 0xffffffffLL));
}

inline bool YawTargetFresh(double published,double now) {
    return published>0 && now>=published && now-published<=150;
}
