/*
 * Window_xbox.c  —  OpenJKDF2 Xbox window/message-loop wrapper
 * Location:  src/Platform/Xbox/Window_xbox.c
 */

#include "platform_xbox.h"
#include "xbox_debug.h"

/* Forward declarations from std3D */
extern "C" {
int  std3D_Startup(void);
void std3D_Shutdown(void);
}

int Window_xbox_Startup(void)
{
    XDBG("Window_xbox_Startup: initialising D3D8...\n");
    if (!std3D_Startup())
    {
        XDBG("Window_xbox_Startup: std3D_Startup FAILED\n");
        return 0;
    }
    XDBG("Window_xbox_Startup: D3D8 ready\n");
    return 1;
}

void Window_xbox_Shutdown(void)
{
    std3D_Shutdown();
    XDBG("Window_xbox_Shutdown: done\n");
}

int Window_xbox_PumpMessages(void)
{
    return 1;
}

void Window_xbox_Present(void)
{
    /* std3D_EndScene already calls Present — nothing to do here */
}
