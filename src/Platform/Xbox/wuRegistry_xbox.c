/*
 * wuRegistry_xbox.c
 * Small file-backed replacement for the Win32 registry.
 */
#include "Platform/wuRegistry.h"
#include "jk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

int  wuRegistry_bInitted   = 0;
const char *wuRegistry_lpSubKey = "";

#define WUREG_XBOX_MAX_ENTRIES 96
#define WUREG_XBOX_KEY_LEN 64
#define WUREG_XBOX_VAL_LEN 128
#define WUREG_XBOX_FNAME "xbox_registry.cfg"

typedef struct XboxRegistryEntry
{
    char key[WUREG_XBOX_KEY_LEN];
    char value[WUREG_XBOX_VAL_LEN];
} XboxRegistryEntry;

static XboxRegistryEntry wuRegistry_xboxEntries[WUREG_XBOX_MAX_ENTRIES];
static int wuRegistry_xboxNumEntries = 0;
static int wuRegistry_xboxLoaded = 0;

static void wuRegistry_XboxTrimLine(char *line)
{
    size_t len;
    if (!line)
        return;
    len = strlen(line);
    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n' || line[len - 1] == ' ' || line[len - 1] == '\t'))
    {
        line[len - 1] = 0;
        len--;
    }
}

static int wuRegistry_XboxFind(const char *name)
{
    int i;
    if (!name)
        return -1;
    for (i = 0; i < wuRegistry_xboxNumEntries; i++)
    {
        if (!_stricmp(wuRegistry_xboxEntries[i].key, name))
            return i;
    }
    return -1;
}

static void wuRegistry_XboxLoad(void)
{
    FILE *f;
    char line[256];

    if (wuRegistry_xboxLoaded)
        return;

    wuRegistry_xboxLoaded = 1;
    wuRegistry_xboxNumEntries = 0;

    f = fopen(WUREG_XBOX_FNAME, "r");
    if (!f)
        return;

    while (fgets(line, sizeof(line), f) && wuRegistry_xboxNumEntries < WUREG_XBOX_MAX_ENTRIES)
    {
        char *eq;
        wuRegistry_XboxTrimLine(line);
        if (!line[0] || line[0] == '#')
            continue;
        eq = strchr(line, '=');
        if (!eq || eq == line)
            continue;
        *eq++ = 0;
        wuRegistry_XboxTrimLine(line);
        wuRegistry_XboxTrimLine(eq);
        strncpy(wuRegistry_xboxEntries[wuRegistry_xboxNumEntries].key, line, WUREG_XBOX_KEY_LEN - 1);
        wuRegistry_xboxEntries[wuRegistry_xboxNumEntries].key[WUREG_XBOX_KEY_LEN - 1] = 0;
        strncpy(wuRegistry_xboxEntries[wuRegistry_xboxNumEntries].value, eq, WUREG_XBOX_VAL_LEN - 1);
        wuRegistry_xboxEntries[wuRegistry_xboxNumEntries].value[WUREG_XBOX_VAL_LEN - 1] = 0;
        wuRegistry_xboxNumEntries++;
    }

    fclose(f);
}

static void wuRegistry_XboxFlush(void)
{
    FILE *f;
    int i;

    wuRegistry_XboxLoad();

    f = fopen(WUREG_XBOX_FNAME, "w");
    if (!f)
        return;

    for (i = 0; i < wuRegistry_xboxNumEntries; i++)
        fprintf(f, "%s=%s\n", wuRegistry_xboxEntries[i].key, wuRegistry_xboxEntries[i].value);

    fclose(f);
}

static int wuRegistry_XboxSet(const char *name, const char *value)
{
    int idx;

    if (!name || !name[0] || !value)
        return 0;

    wuRegistry_XboxLoad();
    idx = wuRegistry_XboxFind(name);
    if (idx < 0)
    {
        if (wuRegistry_xboxNumEntries >= WUREG_XBOX_MAX_ENTRIES)
            return 0;
        idx = wuRegistry_xboxNumEntries++;
        strncpy(wuRegistry_xboxEntries[idx].key, name, WUREG_XBOX_KEY_LEN - 1);
        wuRegistry_xboxEntries[idx].key[WUREG_XBOX_KEY_LEN - 1] = 0;
    }

    strncpy(wuRegistry_xboxEntries[idx].value, value, WUREG_XBOX_VAL_LEN - 1);
    wuRegistry_xboxEntries[idx].value[WUREG_XBOX_VAL_LEN - 1] = 0;
    wuRegistry_XboxFlush();
    return 1;
}

static const char *wuRegistry_XboxGet(const char *name)
{
    int idx;

    wuRegistry_XboxLoad();
    idx = wuRegistry_XboxFind(name);
    if (idx < 0)
        return NULL;
    return wuRegistry_xboxEntries[idx].value;
}

LSTATUS wuRegistry_Startup(void *hKey, const char *lpSubKey, unsigned char *lpData)
{
    (void)hKey; (void)lpSubKey; (void)lpData;
    wuRegistry_XboxLoad();
    wuRegistry_bInitted = 1;
    return 0;
}

void wuRegistry_Shutdown(void)
{
    wuRegistry_XboxFlush();
    wuRegistry_bInitted = 0;
}

int  wuRegistry_SaveInt(const char *name, int val)
{
    char tmp[32];
    sprintf(tmp, "%d", val);
    return wuRegistry_XboxSet(name, tmp);
}
int  wuRegistry_LoadInt(const char *name, int def)
{
    const char *value = wuRegistry_XboxGet(name);
    return value ? atoi(value) : def;
}
int  wuRegistry_GetInt(const char *name, int def)
{
    return wuRegistry_LoadInt(name, def);
}
int  wuRegistry_SaveBool(const char *name, int val)
{
    return wuRegistry_SaveInt(name, val ? 1 : 0);
}
int  wuRegistry_LoadBool(const char *name, int def)
{
    return wuRegistry_LoadInt(name, def ? 1 : 0) ? 1 : 0;
}
int  wuRegistry_GetBool(const char *name, int def)
{
    return wuRegistry_LoadBool(name, def);
}
int  wuRegistry_SaveFloat(const char *name, float val)
{
    char tmp[32];
    sprintf(tmp, "%.6f", val);
    return wuRegistry_XboxSet(name, tmp);
}
float wuRegistry_LoadFloat(const char *name, float def)
{
    const char *value = wuRegistry_XboxGet(name);
    return value ? (float)atof(value) : def;
}
float wuRegistry_GetFloat(const char *name, float def)
{
    return wuRegistry_LoadFloat(name, def);
}
int  wuRegistry_SaveString(const char *name, const char *val)
{
    return wuRegistry_XboxSet(name, val ? val : "");
}
int  wuRegistry_GetString(const char *name, char *out,
                           int maxLen, const char *def)
{
    const char *value = wuRegistry_XboxGet(name);
    if (!value)
        value = def;
    if (value && out && maxLen > 0) {
        strncpy(out, value, maxLen-1);
        out[maxLen-1] = 0;
    }
    return value ? 1 : 0;
}
int  wuRegistry_SetString(const char *name, const char *val)
{
    return wuRegistry_SaveString(name, val);
}

int  wuRegistry_SetWString(const char *name, const wchar_t *val)
{
    char tmp[WUREG_XBOX_VAL_LEN];
    if (!val)
        return wuRegistry_XboxSet(name, "");
    wcstombs(tmp, val, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = 0;
    return wuRegistry_XboxSet(name, tmp);
}

int  wuRegistry_GetWString(const char *name, wchar_t *out,
                           int maxLen, const wchar_t *def)
{
    const char *value = wuRegistry_XboxGet(name);
    if (value && out && maxLen > 0) {
        mbstowcs(out, value, maxLen - 1);
        out[maxLen - 1] = 0;
        return 1;
    }
    if (def && out && maxLen > 0) {
        wcsncpy(out, def, maxLen - 1);
        out[maxLen - 1] = 0;
    }
    return def ? 1 : 0;
}
