// core/util/clock.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static double MaimNowMs()
{
    LARGE_INTEGER q; QueryPerformanceCounter(&q);
    return g_qpcFreq ? (double)q.QuadPart * 1000.0 / (double)g_qpcFreq : 0.0;
}
