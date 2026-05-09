/*
 * wuRegistry_xbox.c
 * Replaces Win32 registry with hardcoded defaults.
 * All settings that would normally persist are just
 * set to sane values once at startup.
 */
#include "Platform/wuRegistry.h"
#include "jk.h"

int  wuRegistry_bInitted   = 0;
const char *wuRegistry_lpSubKey = "";

LSTATUS wuRegistry_Startup(void *hKey, const char *lpSubKey, unsigned char *lpData)
{
    (void)hKey; (void)lpSubKey; (void)lpData;
    wuRegistry_bInitted = 1;
    return 0;
}

void wuRegistry_Shutdown(void)
{
    wuRegistry_bInitted = 0;
}

int  wuRegistry_SaveInt(const char *name, int val)
         { (void)name; (void)val; return 0; }
int  wuRegistry_LoadInt(const char *name, int def)
         { (void)name; return def; }
int  wuRegistry_SaveBool(const char *name, int val)
         { (void)name; (void)val; return 0; }
int  wuRegistry_LoadBool(const char *name, int def)
         { (void)name; return def; }
int  wuRegistry_SaveFloat(const char *name, float val)
         { (void)name; (void)val; return 0; }
float wuRegistry_LoadFloat(const char *name, float def)
         { (void)name; return def; }
int  wuRegistry_SaveString(const char *name, const char *val)
         { (void)name; (void)val; return 0; }
int  wuRegistry_GetString(const char *name, char *out,
                           int maxLen, const char *def)
{
    (void)name;
    if (def && out && maxLen > 0) {
        strncpy(out, def, maxLen-1);
        out[maxLen-1] = 0;
    }
    return 0;
}
int  wuRegistry_SetString(const char *name, const char *val)
         { (void)name; (void)val; return 0; }