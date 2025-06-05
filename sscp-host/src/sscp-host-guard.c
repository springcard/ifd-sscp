#include "sscp-host_i.h"

void SSCP_InitGuardTime(SSCP_CTX_ST* ctx, DWORD guardTimeMs)
{
#ifdef _WIN32    
    QueryPerformanceFrequency(&ctx->guardFreq);
    QueryPerformanceCounter(&ctx->guardStart);
#else
    clock_gettime(CLOCK_MONOTONIC, &ctx->guardStart);
#endif
    ctx->guardValue = guardTimeMs;
    ctx->guardRunning = TRUE;
}

void SSCP_WaitGuardTime(SSCP_CTX_ST* ctx)
{
#ifdef _WIN32    
    LARGE_INTEGER now;
    LONGLONG ticksToWait, ticksElapsed;
#else
    struct timespec now;
    DWORD elapsedMs;
#endif

    if (!ctx->guardRunning)
        return;
    ctx->guardRunning = FALSE;

#ifdef _WIN32    
    QueryPerformanceCounter(&now);
    ticksToWait = (ctx->guardValue * ctx->guardFreq.QuadPart) / 1000;
    ticksElapsed = now.QuadPart - ctx->guardStart.QuadPart;
    if (ticksElapsed < ticksToWait)
    {
        DWORD remainingMs = (DWORD)((ticksToWait - ticksElapsed) * 1000 / ctx->guardFreq.QuadPart);
        if (remainingMs > 1)
            Sleep(remainingMs);
        else
            Sleep(1);
    }
#else
    clock_gettime(CLOCK_MONOTONIC, &now);
    elapsedMs = 1000UL * (now.tv_sec - ctx->guardStart.tv_sec) + (now.tv_nsec - ctx->guardStart.tv_nsec) * 1000000UL;
    if (elapsedMs < ctx->guardValue)
    {
        struct timespec ts;
        DWORD remainingMs = ctx->guardValue - elapsedMs;
        ts.tv_sec = remainingMs / 1000;
        ts.tv_nsec  = (remainingMs % 1000UL) * 1000000UL;
        nanosleep(&ts, NULL);
    }
#endif
}

void SSCP_GuardTime(SSCP_CTX_ST* ctx, DWORD guardTimeMs)
{
    if (ctx->guardRunning)
        SSCP_WaitGuardTime(ctx);
    SSCP_InitGuardTime(ctx, guardTimeMs);
}
