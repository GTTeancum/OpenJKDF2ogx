/*
 * wuRegistry_xbox.c
 * Persists the Win32 registry-shaped settings API in Xbox title storage.
 */
#include "Platform/wuRegistry.h"
#include "Platform/Xbox/xbox_debug.h"
#include "jk.h"

#include <string.h>
#include <wchar.h>

#define WUREGISTRY_MAGIC       0x52474B4Au /* "JKGR" */
#define WUREGISTRY_VERSION     1u
#define WUREGISTRY_MAX_ENTRIES 96
#define WUREGISTRY_NAME_SIZE   64
#define WUREGISTRY_VALUE_SIZE  512

typedef enum wuRegistryValueType
{
    WUREGISTRY_TYPE_INT = 1,
    WUREGISTRY_TYPE_FLOAT,
    WUREGISTRY_TYPE_BOOL,
    WUREGISTRY_TYPE_BYTES,
    WUREGISTRY_TYPE_STRING,
    WUREGISTRY_TYPE_WSTRING
} wuRegistryValueType;

typedef struct wuRegistryEntry
{
    char name[WUREGISTRY_NAME_SIZE];
    DWORD type;
    DWORD size;
    BYTE value[WUREGISTRY_VALUE_SIZE];
} wuRegistryEntry;

typedef struct wuRegistryFile
{
    DWORD magic;
    DWORD version;
    DWORD count;
    wuRegistryEntry entries[WUREGISTRY_MAX_ENTRIES];
} wuRegistryFile;

static const char *wuRegistry_aPaths[] =
{
    "T:\\openjkdf2_registry.bin",
    "U:\\openjkdf2_registry.bin",
    "E:\\openjkdf2_registry.bin",
    NULL
};

static wuRegistryFile wuRegistry_file;
static const char *wuRegistry_pActivePath = NULL;

static void wuRegistry_Reset(void)
{
    memset(&wuRegistry_file, 0, sizeof(wuRegistry_file));
    wuRegistry_file.magic = WUREGISTRY_MAGIC;
    wuRegistry_file.version = WUREGISTRY_VERSION;
}

static int wuRegistry_IsValid(void)
{
    DWORD i;

    if (wuRegistry_file.magic != WUREGISTRY_MAGIC ||
        wuRegistry_file.version != WUREGISTRY_VERSION ||
        wuRegistry_file.count > WUREGISTRY_MAX_ENTRIES)
    {
        return 0;
    }

    for (i = 0; i < wuRegistry_file.count; i++)
    {
        wuRegistryEntry *entry = &wuRegistry_file.entries[i];
        if (!memchr(entry->name, 0, sizeof(entry->name)) ||
            entry->size > WUREGISTRY_VALUE_SIZE)
        {
            return 0;
        }
    }

    return 1;
}

static int wuRegistry_ReadPath(const char *path)
{
    stdFile_t file;
    size_t bytesRead;

    if (!std_pHS || !std_pHS->fileOpen)
        return 0;

    file = std_pHS->fileOpen(path, "rb");
    if (!file)
        return 0;
    bytesRead = std_pHS->fileRead(file, &wuRegistry_file, sizeof(wuRegistry_file));
    std_pHS->fileClose(file);

    return bytesRead == sizeof(wuRegistry_file) && wuRegistry_IsValid();
}

static int wuRegistry_WritePath(const char *path)
{
    stdFile_t file;
    size_t bytesWritten;

    if (!std_pHS || !std_pHS->fileOpen)
        return 0;

    file = std_pHS->fileOpen(path, "wb");
    if (!file)
        return 0;
    bytesWritten = std_pHS->fileWrite(file, &wuRegistry_file, sizeof(wuRegistry_file));
    std_pHS->fileClose(file);

    return bytesWritten == sizeof(wuRegistry_file);
}

static int wuRegistry_SaveFile(void)
{
    const char **path;

    if (wuRegistry_pActivePath && wuRegistry_WritePath(wuRegistry_pActivePath))
        return 1;

    for (path = wuRegistry_aPaths; *path; path++)
    {
        if (wuRegistry_WritePath(*path))
        {
            wuRegistry_pActivePath = *path;
            XDBGF("XboxRegistry: storage path='%s' entries=%lu\n",
                  wuRegistry_pActivePath,
                  (unsigned long)wuRegistry_file.count);
            return 1;
        }
    }

    XDBG("XboxRegistry: no writable title-storage path\n");
    return 0;
}

static wuRegistryEntry *wuRegistry_Find(const char *name)
{
    DWORD i;

    if (!name)
        return NULL;

    for (i = 0; i < wuRegistry_file.count; i++)
    {
        if (!_stricmp(wuRegistry_file.entries[i].name, name))
            return &wuRegistry_file.entries[i];
    }

    return NULL;
}

static wuRegistryEntry *wuRegistry_Store(const char *name, DWORD type,
                                         const void *value, DWORD size)
{
    wuRegistryEntry *entry;

    if (!name || !name[0] || !value || size > WUREGISTRY_VALUE_SIZE)
        return NULL;

    entry = wuRegistry_Find(name);
    if (!entry)
    {
        if (wuRegistry_file.count >= WUREGISTRY_MAX_ENTRIES)
            return NULL;
        entry = &wuRegistry_file.entries[wuRegistry_file.count++];
        memset(entry, 0, sizeof(*entry));
        strncpy(entry->name, name, sizeof(entry->name) - 1);
    }

    entry->type = type;
    entry->size = size;
    memset(entry->value, 0, sizeof(entry->value));
    memcpy(entry->value, value, size);
    if (type == WUREGISTRY_TYPE_STRING)
        entry->value[WUREGISTRY_VALUE_SIZE - 1] = 0;
    else if (type == WUREGISTRY_TYPE_WSTRING)
        ((wchar_t *)entry->value)[(WUREGISTRY_VALUE_SIZE / sizeof(wchar_t)) - 1] = 0;
    return entry;
}

static int wuRegistry_SaveValue(const char *name, DWORD type,
                                const void *value, DWORD size)
{
    if (!wuRegistry_Store(name, type, value, size))
        return 0;
    return wuRegistry_SaveFile();
}

LSTATUS wuRegistry_Startup(HKEY hKey, LPCSTR lpSubKey, BYTE *lpData)
{
    const char **path;

    (void)hKey;
    (void)lpSubKey;
    (void)lpData;

    wuRegistry_Reset();
    wuRegistry_pActivePath = NULL;
    for (path = wuRegistry_aPaths; *path; path++)
    {
        if (wuRegistry_ReadPath(*path))
        {
            wuRegistry_pActivePath = *path;
            XDBG("XboxRegistry: loaded persisted settings\n");
            XDBGF("XboxRegistry: loaded path='%s' entries=%lu\n",
                  wuRegistry_pActivePath,
                  (unsigned long)wuRegistry_file.count);
            if (wuRegistry_Find("numBots"))
                XDBG("XboxRegistry: numBots setting present\n");
            break;
        }
        wuRegistry_Reset();
    }
    if (!wuRegistry_pActivePath)
        XDBG("XboxRegistry: starting with defaults\n");

    wuRegistry_bInitted = 1;
    return 0;
}

void wuRegistry_Shutdown(void)
{
    if (wuRegistry_bInitted)
        wuRegistry_SaveFile();
    wuRegistry_bInitted = 0;
}

int wuRegistry_SaveInt(LPCSTR name, int val)
{
    return wuRegistry_SaveValue(name, WUREGISTRY_TYPE_INT, &val, sizeof(val));
}

int wuRegistry_GetInt(LPCSTR name, int defaultVal)
{
    wuRegistryEntry *entry = wuRegistry_Find(name);
    if (!entry || entry->type != WUREGISTRY_TYPE_INT || entry->size != sizeof(int))
        return defaultVal;
    return *(int *)entry->value;
}

int wuRegistry_SaveFloat(LPCSTR name, flex_t val)
{
    return wuRegistry_SaveValue(name, WUREGISTRY_TYPE_FLOAT, &val, sizeof(val));
}

flex_t wuRegistry_GetFloat(LPCSTR name, flex_t defaultVal)
{
    wuRegistryEntry *entry = wuRegistry_Find(name);
    if (!entry || entry->type != WUREGISTRY_TYPE_FLOAT || entry->size != sizeof(flex_t))
        return defaultVal;
    return *(flex_t *)entry->value;
}

int wuRegistry_SaveBool(LPCSTR name, int val)
{
    val = !!val;
    return wuRegistry_SaveValue(name, WUREGISTRY_TYPE_BOOL, &val, sizeof(val));
}

int wuRegistry_GetBool(LPCSTR name, int defaultVal)
{
    wuRegistryEntry *entry = wuRegistry_Find(name);
    if (!entry || entry->type != WUREGISTRY_TYPE_BOOL || entry->size != sizeof(int))
        return defaultVal;
    return !!*(int *)entry->value;
}

int wuRegistry_SaveBytes(LPCSTR name, BYTE *data, DWORD size)
{
    return wuRegistry_SaveValue(name, WUREGISTRY_TYPE_BYTES, data, size);
}

int wuRegistry_GetBytes(LPCSTR name, BYTE *data, DWORD size)
{
    wuRegistryEntry *entry = wuRegistry_Find(name);
    if (!data || !entry || entry->type != WUREGISTRY_TYPE_BYTES || entry->size != size)
        return 0;
    memcpy(data, entry->value, size);
    return 1;
}

int wuRegistry_SetString(LPCSTR name, const char *val)
{
    DWORD size;
    if (!val)
        return 0;
    size = (DWORD)strlen(val) + 1;
    if (size > WUREGISTRY_VALUE_SIZE)
        size = WUREGISTRY_VALUE_SIZE;
    return wuRegistry_SaveValue(name, WUREGISTRY_TYPE_STRING, val, size);
}

int wuRegistry_GetString(LPCSTR name, char *out, int maxLen, const char *defaultVal)
{
    wuRegistryEntry *entry = wuRegistry_Find(name);
    const char *value = defaultVal ? defaultVal : "";
    int found = 0;

    if (!out || maxLen <= 0)
        return 0;
    if (entry && entry->type == WUREGISTRY_TYPE_STRING && entry->size > 0)
    {
        value = (const char *)entry->value;
        found = 1;
    }
    strncpy(out, value, maxLen - 1);
    out[maxLen - 1] = 0;
    return found;
}

int wuRegistry_SetWString(LPCSTR name, const wchar_t *val)
{
    DWORD size;
    if (!val)
        return 0;
    size = ((DWORD)wcslen(val) + 1) * sizeof(wchar_t);
    if (size > WUREGISTRY_VALUE_SIZE)
        size = WUREGISTRY_VALUE_SIZE;
    return wuRegistry_SaveValue(name, WUREGISTRY_TYPE_WSTRING, val, size);
}

int wuRegistry_GetWString(LPCSTR name, wchar_t *out, int maxLen,
                          const wchar_t *defaultVal)
{
    wuRegistryEntry *entry = wuRegistry_Find(name);
    const wchar_t *value = defaultVal ? defaultVal : L"";
    int found = 0;

    if (!out || maxLen <= 0)
        return 0;
    if (entry && entry->type == WUREGISTRY_TYPE_WSTRING &&
        entry->size >= sizeof(wchar_t))
    {
        value = (const wchar_t *)entry->value;
        found = 1;
    }
    wcsncpy(out, value, maxLen - 1);
    out[maxLen - 1] = 0;
    return found;
}
