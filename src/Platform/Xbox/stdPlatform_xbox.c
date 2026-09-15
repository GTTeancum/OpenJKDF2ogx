#include <xtl.h>
#include "stdPlatform.h"
#include "jk.h"
#include "xbox_debug.h"

/* Use the performance counter for elapsed time. In measured XEMU runs,
 * GetTickCount advanced substantially slower than both QPC and host time.
 * Keep a shared epoch for the millisecond and microsecond engine APIs. */
static int xbox_clockInitialized;
static int xbox_clockUseCounter;
static LARGE_INTEGER xbox_clockFrequency, xbox_clockBase;
static uint64_t xbox_clockBaseUs, xbox_clockLastUs;

static uint64_t xbox_clockTimeUs(void)
{
    LARGE_INTEGER now;
    if (!xbox_clockInitialized) {
        xbox_clockBaseUs = (uint64_t)GetTickCount() * 1000ULL;
        xbox_clockLastUs = xbox_clockBaseUs;
        xbox_clockUseCounter = QueryPerformanceFrequency(&xbox_clockFrequency)
            && xbox_clockFrequency.QuadPart > 0
            && QueryPerformanceCounter(&xbox_clockBase);
        xbox_clockInitialized = 1;
    }
    if (!xbox_clockUseCounter)
        return (uint64_t)GetTickCount() * 1000ULL;
    if (QueryPerformanceCounter(&now) && now.QuadPart >= xbox_clockBase.QuadPart) {
        uint64_t ticks = (uint64_t)(now.QuadPart - xbox_clockBase.QuadPart);
        uint64_t frequency = (uint64_t)xbox_clockFrequency.QuadPart;
        /* Split the quotient to avoid overflowing ticks * 1000000 on long runs. */
        uint64_t value = xbox_clockBaseUs + (ticks / frequency) * 1000000ULL
            + ((ticks % frequency) * 1000000ULL) / frequency;
        if (value > xbox_clockLastUs) xbox_clockLastUs = value;
    }
    return xbox_clockLastUs;
}

uint32_t stdPlatform_GetTimeMsec(void)
{
    return (uint32_t)(xbox_clockTimeUs() / 1000ULL);
}

uint64_t Linux_TimeUs(void)
{
    return xbox_clockTimeUs();
}

/* Printf — routes to debug output visible in Xbox Neighborhood */
int stdPlatform_Printf(const char *fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    _vsnprintf(buf, sizeof(buf)-1, fmt, args);
    buf[sizeof(buf)-1] = 0;
    va_end(args);
    xbox_debug_Print(buf);
    return 0;
}

int stdPrintf(int (*printfn)(const char*,...),
              const char *file, int line, const char *fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    _vsnprintf(buf, sizeof(buf)-1, fmt, args);
    buf[sizeof(buf)-1] = 0;
    va_end(args);
    xbox_debug_Print(buf);
    return 0;
}

/* stdConsolePrintf is provided by std.c — do not duplicate here */

/* Memory — delegate to Xbox CRT */
int stdPlatform_Startup(void) { return 1; }

void stdPlatform_InitServices(HostServices *handlers)
{
    /* Xbox HostServices are set up by stdFile_xbox.c and preserved
       by stdStartup. Nothing to do here. */
    (void)handlers;
}
