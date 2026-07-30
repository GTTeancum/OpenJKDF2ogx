#ifndef _MAINMAIN_H
#define _MAINMAIN_H

#include "types.h"
#include "globals.h"

#define Main_Startup_ADDR (0x00401000)
#define Main_Shutdown_ADDR (0x00401370)
#define Main_ParseCmdLine_ADDR (0x004014B0)
#define Main_FPrintf_ADDR (0x00401870)

#ifdef QOL_IMPROVEMENTS
extern int32_t Main_bDedicatedServer;
extern int32_t Main_bAutostart;
extern int32_t Main_bHeadless;
extern int32_t Main_bVerboseNetworking;
extern int32_t Main_bMotsCompat;
extern int32_t Main_bDwCompat;
extern int32_t Main_bEnhancedCogVerbs;
extern int32_t Main_numBots;
extern int32_t Main_botMatchSeconds;
extern int32_t Main_botCamPlayer;
extern int32_t Main_localPlayers;
extern char Main_strEpisode[129];
extern char Main_strMap[132];
#endif

int Main_ShouldUseMotsForcePowers(void);

int Main_Startup(const char *cmdline);
void Main_Shutdown();
void Main_ParseCmdLine(char *cmdline);
int Main_FPrintf(const char* fmt, ...);

#endif // _MAINMAIN_H
