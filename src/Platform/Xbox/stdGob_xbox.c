/*
 * stdGob_xbox.c  -- GOB loader for Xbox port (C89 clean)
 * Location: src/Platform/Xbox/stdGob_xbox.c
 *
 * Uses the REAL headers so struct layouts match the engine.
 * Only the function implementations are here (C89 compliant).
 */

#include "platform_xbox.h"
#include "xbox_debug.h"

/* Real type definitions so struct layouts match the engine */
#include "jk.h"
#include "Win95/stdGob.h"
#include "General/stdHashTable.h"
#include "General/stdString.h"
#include "General/stdFnames.h"
/* stdString — we provide our own implementations below */
/* stdFnames — we provide our own implementations below */
#include "stdPlatform.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef STDGOB_STANDALONE
/* =================================================================
 * stdHashTable -- C89 clean reimplementation (opaque struct)
 * ================================================================= */
#define HT_BUCKET_COUNT 2048

typedef struct {
    char  key[128];
    void *val;
    int   used;
} HT_Bucket;

struct stdHashTable {
    HT_Bucket *buckets;
    int        count;
};

static unsigned int ht_hash(const char *s)
{
    unsigned int h = 5381;
    unsigned char c;
    while (*s) {
        c = (unsigned char)*s;
        if (c >= 'A' && c <= 'Z') c += 32;
        h = ((h << 5) + h) + c;
        s++;
    }
    return h;
}
stdHashTable *stdHashTable_New(int sizeHint)
{
    stdHashTable *ht;
    (void)sizeHint;
    ht = (stdHashTable*)std_pHS->alloc(sizeof(stdHashTable));
    if (!ht) return 0;
    ht->buckets = (HT_Bucket*)std_pHS->alloc(sizeof(HT_Bucket) * HT_BUCKET_COUNT);
    if (!ht->buckets) { std_pHS->free(ht); return 0; }
    memset(ht->buckets, 0, sizeof(HT_Bucket) * HT_BUCKET_COUNT);
    ht->count = HT_BUCKET_COUNT;
    return ht;
}

void stdHashTable_Free(stdHashTable *ht)
{
    if (!ht) return;
    if (ht->buckets) std_pHS->free(ht->buckets);
    std_pHS->free(ht);
}

void stdHashTable_SetKeyVal(stdHashTable *ht, const char *key, void *val)
{
    unsigned int idx, slot;
    int i;
    if (!ht || !key) return;
    idx = ht_hash(key) % ht->count;
    for (i = 0; i < ht->count; i++) {
        slot = (idx + i) % ht->count;
        if (!ht->buckets[slot].used) {
            strncpy(ht->buckets[slot].key, key, 127);
            ht->buckets[slot].key[127] = 0;
            ht->buckets[slot].val = val;
            ht->buckets[slot].used = 1;
            return;
        }
        if (_stricmp(ht->buckets[slot].key, key) == 0) {
            ht->buckets[slot].val = val;
            return;
        }
    }
}

void *stdHashTable_GetKeyVal(stdHashTable *ht, const char *key)
{
    unsigned int idx, slot;
    int i;
    if (!ht || !key) return 0;
    idx = ht_hash(key) % ht->count;
    for (i = 0; i < ht->count; i++) {
        slot = (idx + i) % ht->count;
        if (!ht->buckets[slot].used) return 0;
        if (_stricmp(ht->buckets[slot].key, key) == 0)
            return ht->buckets[slot].val;
    }
    return 0;
}

#endif /* STDGOB_STANDALONE — stdHashTable */

/* =================================================================
 * stdString -- C89 clean (only for standalone builds)
 * ================================================================= */
#ifdef STDGOB_STANDALONE
char *stdString_SafeStrCopy(char *dst, const char *src, uint32_t n)
{
    if (dst && src && n > 0) { strncpy(dst, src, n - 1); dst[n - 1] = 0; }
    return dst;
}

void stdString_CStrToLower(char *s)
{
    if (!s) return;
    while (*s) { if (*s >= 'A' && *s <= 'Z') *s += 32; s++; }
}

int stdString_snprintf(char *buf, int n, const char *fmt, ...)
{
    va_list a; int r;
    va_start(a, fmt);
    r = _vsnprintf(buf, n, fmt, a);
    va_end(a);
    return r;
}

#endif /* STDGOB_STANDALONE — stdString */

#ifdef STDGOB_STANDALONE
char *stdFnames_MakePath(char *out, int outSz, const char *dir, const char *fname)
{
    int len;
    if (!out || outSz <= 0) return out;
    out[0] = 0;
    if (dir && dir[0]) {
        strncpy(out, dir, outSz - 1);
        out[outSz - 1] = 0;
        len = (int)strlen(out);
        if (len > 0 && out[len-1] != '\\' && out[len-1] != '/' && len < outSz-2) {
            out[len] = '\\'; out[len+1] = 0;
        }
    }
    if (fname) {
        len = (int)strlen(out);
        strncpy(out + len, fname, outSz - len - 1);
        out[outSz - 1] = 0;
    }
    return out;
}
#endif /* STDGOB_STANDALONE */

/* =================================================================
 * stdGob -- C89 clean, uses HostServices for file I/O
 * ================================================================= */
static HostServices gobHS;
static HostServices *pGobHS;
static int stdGob_bInit;
static char stdGob_fpath_buf[128];

int stdGob_Startup(HostServices *pHS_in)
{
    _memcpy(&gobHS, pHS_in, sizeof(gobHS));
    pGobHS = &gobHS;
    stdGob_bInit = 1;
    return 1;
}

void stdGob_Shutdown(void)
{
    stdGob_bInit = 0;
}

stdGob *stdGob_Load(char *fpath, int a2, int a3)
{
    stdGob *gob;
    XDBGF("stdGob_Load('%s', %d, %d)\n", fpath, a2, a3);
    gob = (stdGob*)std_pHS->alloc(sizeof(stdGob));
    if (!gob) return 0;
    _memset(gob, 0, sizeof(stdGob));
    if (!stdGob_LoadEntry(gob, fpath, a2, a3)) {
        std_pHS->free(gob);
        return 0;
    }
    return gob;
}

int stdGob_LoadEntry(stdGob *gob, char *fname, int a3, int a4)
{
    stdGobHeader header;
    uint32_t v4;

    (void)a4;

    stdString_SafeStrCopy(gob->fpath, fname, 128);
    gob->numFilesOpen = a3;
    gob->lastReadFile = 0;
    gob->viewMapped = 0;

    gob->fhand = pGobHS->fileOpen(gob->fpath, "rb");
    if (!gob->fhand) {
        XDBGF("stdGob: failed to open '%s'\n", gob->fpath);
        return 0;
    }
    XDBGF("stdGob: opened '%s'\n", gob->fpath);

    gob->openedFile = (stdGobFile*)std_pHS->alloc(sizeof(stdGobFile) * gob->numFilesOpen);
    if (!gob->openedFile) { XDBG("stdGob: alloc openedFile failed\n"); return 0; }
    _memset(gob->openedFile, 0, sizeof(stdGobFile) * gob->numFilesOpen);
    XDBG("stdGob: openedFile alloc ok\n");

    pGobHS->fileRead(gob->fhand, &header, sizeof(stdGobHeader));
    XDBGF("stdGob: header read magic=%c%c%c%c version=%u entryOff=%u\n",
          ((char*)&header)[0], ((char*)&header)[1], ((char*)&header)[2], ((char*)&header)[3],
          header.version, (unsigned)header.entryTable_offs);
    if (_memcmp((const char*)&header, "GOB ", 4u)) {
        XDBG("stdGob: bad GOB magic\n");
        return 0;
    }
    if (header.version != GOB_VERSION_LATEST) {
        XDBGF("stdGob: bad GOB version %u\n", header.version);
        return 0;
    }

    pGobHS->fseek(gob->fhand, header.entryTable_offs, 0);
    pGobHS->fileRead(gob->fhand, &gob->numFiles, sizeof(uint32_t));
    XDBGF("stdGob: numFiles=%u\n", (unsigned)gob->numFiles);

    gob->entries = (stdGobEntry*)std_pHS->alloc(sizeof(stdGobEntry) * gob->numFiles);
    if (!gob->entries) { XDBG("stdGob: alloc entries failed\n"); return 0; }
    _memset(gob->entries, 0, sizeof(stdGobEntry) * gob->numFiles);
    XDBG("stdGob: entries alloc+memset ok\n");

    XDBG("stdGob: about to call stdHashTable_New(1024)\n");
    gob->entriesHashtable = stdHashTable_New(1024);
    XDBGF("stdGob: hashtable=%p sizeof(stdGobEntry)=%u\n",
          (void*)gob->entriesHashtable, (unsigned)sizeof(stdGobEntry));

    XDBG("stdGob: entry loop about to start\n");
    for (v4 = 0; v4 < gob->numFiles; v4++) {
        if (v4 < 3 || (v4 & 0xFF) == 0)
            XDBGF("stdGob: loop iter %u start\n", (unsigned)v4);
        if (v4 < 3) XDBG("stdGob:  before fileRead\n");
        pGobHS->fileRead(gob->fhand, &gob->entries[v4], sizeof(stdGobEntry));
        if (v4 < 3) XDBGF("stdGob:  fileRead ok, fname='%s'\n", gob->entries[v4].fname);
        /* Normalize entry filenames: lowercase + forward-slash to backslash.
         * The real stdHashTable uses case-sensitive _strcmp, so both stored
         * keys and lookup keys must be in the same canonical form.
         * NOTE: inline tolower avoids jk.h __tolower function pointer (0x514550) crash */
        {
            char *p;
            for (p = gob->entries[v4].fname; *p; p++) {
                if (*p >= 'A' && *p <= 'Z') *p += 32;
                if (*p == '/') *p = '\\';
            }
        }
        if (v4 < 3) XDBG("stdGob:  before SetKeyVal\n");
        stdHashTable_SetKeyVal(gob->entriesHashtable, gob->entries[v4].fname, &gob->entries[v4]);
        if (v4 < 3) XDBG("stdGob:  SetKeyVal ok\n");
    }

    XDBGF("stdGob: loaded '%s' (%u files)\n", fname, gob->numFiles);
    return 1;
}

void stdGob_FreeEntry(stdGob *gob)
{
    if (!gob) return;
    if (gob->fhand) { pGobHS->fileClose(gob->fhand); gob->fhand = 0; }
    if (gob->openedFile) { std_pHS->free(gob->openedFile); gob->openedFile = 0; }
    if (gob->entries) { std_pHS->free(gob->entries); gob->entries = 0; }
    if (gob->entriesHashtable) { stdHashTable_Free(gob->entriesHashtable); gob->entriesHashtable = 0; }
}

void stdGob_Free(stdGob *gob)
{
    if (!gob) return;
    stdGob_FreeEntry(gob);
    std_pHS->free(gob);
}

stdGobFile *stdGob_FileOpen(stdGob *gob, const char *filepath)
{
    stdGobEntry *entry;
    stdGobFile *f;
    uint32_t i;

    if (!gob || !filepath) return 0;
    if (filepath[0] == '.' && (filepath[1] == '/' || filepath[1] == '\\'))
        filepath += 2;
    stdString_SafeStrCopy(stdGob_fpath_buf, filepath, 128);
    /* Inline tolower + slash-normalize (avoids jk.h __tolower crash) */
    {
        char *p;
        for (p = stdGob_fpath_buf; *p; p++) {
            if (*p >= 'A' && *p <= 'Z') *p += 32;
            if (*p == '/') *p = '\\';
        }
    }
    entry = (stdGobEntry*)stdHashTable_GetKeyVal(gob->entriesHashtable, stdGob_fpath_buf);
    if (!entry) return 0;

    f = gob->openedFile;
    for (i = 0; i < gob->numFilesOpen; i++) {
        if (!f[i].isOpen) {
#ifdef QOL_IMPROVEMENTS
            f[i].bIsMemoryMapped = 0;
            f[i].pMemory = (intptr_t)0;
            f[i].memorySz = 0;
#endif
            f[i].isOpen = 1;
            f[i].parent = gob;
            f[i].entry = entry;
            f[i].seekOffs = 0;
            return &f[i];
        }
    }
    return 0;
}

void stdGob_FileClose(stdGobFile *f)
{
    stdGob *gob;
    if (!f) return;
#ifdef QOL_IMPROVEMENTS
    if (f->pMemory) { free((void*)f->pMemory); f->pMemory = (intptr_t)0; }
    f->bIsMemoryMapped = 0;
#endif
    gob = f->parent;
    f->isOpen = 0;
    if (f == gob->lastReadFile) gob->lastReadFile = 0;
}

int stdGob_FSeek(stdGobFile *f, int pos, int whence)
{
    stdGob *gob;
    if (!f) return 0;
    switch (whence) {
        case 0: f->seekOffs = pos; break;
        case 1: f->seekOffs += pos; break;
        case 2: f->seekOffs = f->entry->fileSize + pos; break;
        default: return 0;
    }
    gob = f->parent;
    if (f == gob->lastReadFile) gob->lastReadFile = 0;
    return 1;
}

int32_t stdGob_FTell(stdGobFile *f) { return f ? f->seekOffs : 0; }

bool stdGob_FEof(stdGobFile *f)
{
    if (!f || !f->entry) return 1;
    return f->seekOffs >= f->entry->fileSize - 1;
}

size_t stdGob_FileRead(stdGobFile *f, void *out, uint32_t len)
{
    stdGob *gob;
    size_t result;

    if (!f || !out) return 0;
    gob = f->parent;
    if (gob->lastReadFile != f) {
        pGobHS->fseek(gob->fhand, f->seekOffs + f->entry->fileOffset, 0);
        gob->lastReadFile = f;
    }
    if (f->entry->fileSize - f->seekOffs < (int32_t)len)
        len = f->entry->fileSize - f->seekOffs;

    result = pGobHS->fileRead(gob->fhand, out, len);
    f->seekOffs += result;
    return result;
}

const char *stdGob_FileGets(stdGobFile *f, char *out, unsigned int len)
{
    stdGobEntry *entry;
    stdGob *gob;
    const char *result;

    if (!f || !out || !len) return 0;
    entry = f->entry;
    if (f->seekOffs >= entry->fileSize - 1) return 0;

    gob = f->parent;
    if (gob->lastReadFile != f) {
        pGobHS->fseek(gob->fhand, f->seekOffs + entry->fileOffset, 0);
        gob->lastReadFile = f;
    }
    if (entry->fileSize - f->seekOffs + 1 < (int)len)
        len = entry->fileSize - f->seekOffs + 1;

    result = pGobHS->fileGets(gob->fhand, out, len);
    if (result) f->seekOffs += (int)_strlen(result);
    return result;
}

const wchar_t *stdGob_FileGetws(stdGobFile *f, wchar_t *out, unsigned int len)
{
    stdGobEntry *entry;
    stdGob *gob;
    unsigned int len_wide;
    const wchar_t *ret;

    if (!f || !out || !len) return 0;
    entry = f->entry;
    if (f->seekOffs >= entry->fileSize - 1) return 0;

    gob = f->parent;
    if (gob->lastReadFile != f) {
        pGobHS->fseek(gob->fhand, f->seekOffs + entry->fileOffset, 0);
        gob->lastReadFile = f;
    }

    len_wide = len;
    if (((entry->fileSize - f->seekOffs) >> 1) + 1 < (int)len)
        len_wide = ((entry->fileSize - f->seekOffs) >> 1) + 1;

    ret = pGobHS->fileGetws(gob->fhand, out, len_wide);
    if (ret) f->seekOffs += (int)_wcslen(ret) * sizeof(wchar_t);
    return ret;
}

size_t stdGob_FileSize(stdGobFile *f)
{
    if (!f || !f->entry) return 0;
    return f->entry->fileSize;
}

/* =================================================================
 * stdEmbeddedRes -- stub (no embedded resources on Xbox)
 * ================================================================= */
void *stdEmbeddedRes_LoadOnlyInternal(const char *path, size_t *outSz)
{
    (void)path;
    if (outSz) *outSz = 0;
    return 0;
}
