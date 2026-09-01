// core/util/log.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).

static void Log(const char* fmt, ...)
{
    if (!g_log) return;
    EnterCriticalSection(&g_logLock);
    DWORD now = GetTickCount();
    fprintf(g_log, "[%10lu] ", (unsigned long)now);
    va_list ap; va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fprintf(g_log, "\n");
    if (now - g_logFlushTick >= 200) { fflush(g_log); g_logFlushTick = now; }
    LeaveCriticalSection(&g_logLock);
}

static void LogFlush(void)
{
    if (!g_log) return;
    EnterCriticalSection(&g_logLock);
    fflush(g_log);
    g_logFlushTick = GetTickCount();
    LeaveCriticalSection(&g_logLock);
}
