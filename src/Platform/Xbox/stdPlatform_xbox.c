#include <xtl.h>
#include "stdPlatform.h"
#include "jk.h"

/* Timing */
uint32_t stdPlatform_GetTimeMsec(void)
{
    return (uint32_t)GetTickCount();
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
    OutputDebugStringA(buf);
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
    OutputDebugStringA(buf);
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