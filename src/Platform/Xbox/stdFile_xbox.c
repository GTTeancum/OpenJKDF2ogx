/*
 * stdFile_xbox.c  -  OpenJKDF2 Xbox filesystem
 * Location:  src/Platform/Xbox/stdFile_xbox.c
 *
 * Uses XTL CreateFileA/ReadFile/WriteFile/CloseHandle instead of CRT
 * fopen/fread/fwrite/fclose. This ensures file calls go through
 * NtCreateFile (xboxkrnl.exe) which CXBX-Reloaded intercepts.
 * D:\ is the virtual disc drive (\Device\CdRom0) — the XBE's directory.
 */

#include "platform_xbox.h"
#include "xbox_debug.h"
#include <xtl.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

/* =========================================================================
 * Xbox kernel file API — bypass CreateFileA, talk to NtCreateFile directly.
 * CXBX-R intercepts NtCreateFile from xboxkrnl.exe.
 * Xbox uses ANSI strings (not UNICODE) for object attributes.
 * ====================================================================== */
typedef struct _XB_ANSI_STRING {
    unsigned short Length;
    unsigned short MaximumLength;
    char          *Buffer;
} XB_ANSI_STRING;

typedef struct _XB_OBJECT_ATTRIBUTES {
    HANDLE           RootDirectory;
    XB_ANSI_STRING  *ObjectName;
    unsigned long    Attributes;
} XB_OBJECT_ATTRIBUTES;

typedef struct _XB_IO_STATUS_BLOCK {
    union { long Status; void *Pointer; };
    unsigned long Information;
} XB_IO_STATUS_BLOCK;

/* Constants — verified against nxdk xboxkrnl.h and ReactOS iotypes.h.
 * Source: https://github.com/XboxDev/nxdk/blob/master/lib/xboxkrnl/xboxkrnl.h */
#define XB_OBJ_CASE_INSENSITIVE       0x00000040  /* OBJ_CASE_INSENSITIVE */
#define XB_FILE_SYNCHRONOUS_IO_NONALERT 0x00000020 /* FILE_SYNCHRONOUS_IO_NONALERT */
#define XB_FILE_DIRECTORY_FILE         0x00000001  /* FILE_DIRECTORY_FILE */
#define XB_FILE_NON_DIRECTORY          0x00000040  /* FILE_NON_DIRECTORY_FILE */

/* NT NtCreateFile CreateDisposition values (ntdef.h / wdm.h).
 * Source: Windows NT DDK, consistent across all NT kernels including Xbox.
 * See also: reactos/include/ndk/iotypes.h */
#define XB_FILE_SUPERSEDE     0x00000000  /* If exists: replace. If not: create. */
#define XB_FILE_OPEN          0x00000001  /* If exists: open.    If not: fail.   */
#define XB_FILE_CREATE        0x00000002  /* If exists: fail.    If not: create. */
#define XB_FILE_OPEN_IF       0x00000003  /* If exists: open.    If not: create. */
#define XB_FILE_OVERWRITE     0x00000004  /* If exists: replace. If not: fail.   */
#define XB_FILE_OVERWRITE_IF  0x00000005  /* If exists: replace. If not: create. */

/* Import from xboxkrnl.lib — must use C linkage to match xboxkrnl.lib */
extern "C" long __stdcall NtCreateFile(
    HANDLE *FileHandle,
    unsigned long DesiredAccess,
    XB_OBJECT_ATTRIBUTES *ObjectAttributes,
    XB_IO_STATUS_BLOCK *IoStatusBlock,
    LARGE_INTEGER *AllocationSize,
    unsigned long FileAttributes,
    unsigned long ShareAccess,
    unsigned long CreateDisposition,
    unsigned long CreateOptions);

extern "C" long __stdcall NtClose(HANDLE Handle);

/* NT file I/O — bypass ReadFile/WriteFile (xapilib) which need Win32 handles.
 * These work with NT handles from NtCreateFile. Verified against nxdk xboxkrnl.h. */
extern "C" long __stdcall NtReadFile(
    HANDLE FileHandle, HANDLE Event, void *ApcRoutine, void *ApcContext,
    XB_IO_STATUS_BLOCK *IoStatusBlock, void *Buffer, unsigned long Length,
    LARGE_INTEGER *ByteOffset);

extern "C" long __stdcall NtWriteFile(
    HANDLE FileHandle, HANDLE Event, void *ApcRoutine, void *ApcContext,
    XB_IO_STATUS_BLOCK *IoStatusBlock, void *Buffer, unsigned long Length,
    LARGE_INTEGER *ByteOffset);

extern "C" long __stdcall NtQueryInformationFile(
    HANDLE FileHandle, XB_IO_STATUS_BLOCK *IoStatusBlock,
    void *FileInformation, unsigned long Length, int FileInformationClass);

extern "C" long __stdcall NtSetInformationFile(
    HANDLE FileHandle, XB_IO_STATUS_BLOCK *IoStatusBlock,
    void *FileInformation, unsigned long Length, int FileInformationClass);

/* FileInformationClass values — verified against nxdk/ReactOS */
#define XB_FileStandardInformation  5   /* FILE_STANDARD_INFORMATION */
#define XB_FilePositionInformation  14  /* FILE_POSITION_INFORMATION */

/* Helper to open a file via NtCreateFile. Returns NTSTATUS. */
static long xbox_NtOpen(const char *devicePath, HANDLE *outHandle)
{
    XB_ANSI_STRING name;
    XB_OBJECT_ATTRIBUTES oa;
    XB_IO_STATUS_BLOCK iosb;

    name.Buffer        = (char *)devicePath;
    name.Length         = (unsigned short)strlen(devicePath);
    name.MaximumLength = name.Length + 1;

    oa.RootDirectory = NULL;
    oa.ObjectName    = &name;
    oa.Attributes    = XB_OBJ_CASE_INSENSITIVE;

    return NtCreateFile(
        outHandle,
        GENERIC_READ | 0x00100000,  /* GENERIC_READ | SYNCHRONIZE */
        &oa,
        &iosb,
        NULL,
        0,
        FILE_SHARE_READ,
        XB_FILE_OPEN,
        XB_FILE_SYNCHRONOUS_IO_NONALERT | XB_FILE_NON_DIRECTORY);
}

/* General-purpose open: supports read, write, create modes. */
static long xbox_NtOpenEx(const char *devicePath, HANDLE *outHandle,
                          unsigned long access, unsigned long share,
                          unsigned long createDisp)
{
    XB_ANSI_STRING name;
    XB_OBJECT_ATTRIBUTES oa;
    XB_IO_STATUS_BLOCK iosb;
    unsigned long ntDisp;

    /* Map Win32 CreateDisposition (WinBase.h) to NT NtCreateFile disposition.
     * Win32 constants verified from C:\XDK\xbox\include\WinBase.h:105-109.
     * NT constants from ntdef.h / wdm.h (standard across all NT kernels). */
    switch (createDisp) {
        case 1: /* CREATE_NEW */       ntDisp = XB_FILE_CREATE;       break;
        case 2: /* CREATE_ALWAYS */    ntDisp = XB_FILE_OVERWRITE_IF; break;
        case 3: /* OPEN_EXISTING */    ntDisp = XB_FILE_OPEN;         break;
        case 4: /* OPEN_ALWAYS */      ntDisp = XB_FILE_OPEN_IF;      break;
        case 5: /* TRUNCATE_EXISTING */ntDisp = XB_FILE_OVERWRITE;    break;
        default: ntDisp = XB_FILE_OPEN; break;
    }

    name.Buffer        = (char *)devicePath;
    name.Length         = (unsigned short)strlen(devicePath);
    name.MaximumLength = name.Length + 1;

    oa.RootDirectory = NULL;
    oa.ObjectName    = &name;
    oa.Attributes    = XB_OBJ_CASE_INSENSITIVE;

    return NtCreateFile(
        outHandle,
        access | 0x00100000,  /* | SYNCHRONIZE */
        &oa,
        &iosb,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        share,
        ntDisp,
        XB_FILE_SYNCHRONOUS_IO_NONALERT | XB_FILE_NON_DIRECTORY);
}

/* =========================================================================
 * XboxFile - thin wrapper around a Win32/XTL HANDLE
 * ====================================================================== */
typedef struct
{
    HANDLE  hFile;
    int     bEof;
    int     bIsNtHandle;    /* 1 = NtCreateFile handle, use Nt* APIs; 0 = CreateFileA, use Win32 */
    char    lineBuf[256];   /* for FileGets emulation */
} XboxFile;

#define MAX_OPEN_FILES 64
static XboxFile g_filePool[MAX_OPEN_FILES];
static int      g_filePoolInited = 0;
static int      g_fileOpenLogCount = 0;
#define FILEOPEN_LOG_LIMIT 0

static void xbox_InitPool(void)
{
    int i;
    if (g_filePoolInited) return;
    for (i = 0; i < MAX_OPEN_FILES; i++)
        g_filePool[i].hFile = INVALID_HANDLE_VALUE;
    g_filePoolInited = 1;
}

static XboxFile *xbox_AllocFile(void)
{
    int i;
    xbox_InitPool();
    for (i = 0; i < MAX_OPEN_FILES; i++)
        if (g_filePool[i].hFile == INVALID_HANDLE_VALUE)
        {
            g_filePool[i].bEof = 0;
            return &g_filePool[i];
        }
    return NULL;
}

#define FHAND_TO_XFILE(h)  (&g_filePool[(intptr_t)(h) - 1])
#define XFILE_TO_FHAND(xf) ((intptr_t)((xf) - g_filePool + 1))

/* =========================================================================
 * Path translation
 * D:\ on Xbox is the disc drive (\Device\CdRom0) — where the XBE lives.
 * On real hardware, installed games use E:\ or F:\ (HDD partitions).
 * CXBX-R maps D:\ to the folder containing the XBE automatically.
 * ======================================================================== */
typedef struct { const char *from; const char *to; } GobRemap;

static const GobRemap g_gobRemaps[] =
{
    /* Episode GOB remaps */
    { "jediknight.gob", "Episode\\JK1.GOB"    },
    { "jk1ctf.gob",     "Episode\\JK1CTF.GOB" },
    { "jk1mp.gob",      "Episode\\JK1MP.GOB"  },
    { NULL, NULL }
};

static char g_gameRoot[64] = "";   /* set by probe; empty = relative paths */

static void xbox_TranslatePath(const char *in, char *out, int outSize)
{
    char lower[256];
    int i, fromLen, lowerLen;
    const GobRemap *r;
    const char *src;

    strncpy(lower, in, sizeof(lower) - 1);
    lower[sizeof(lower) - 1] = '\0';
    for (i = 0; lower[i]; i++)
        if (lower[i] >= 'A' && lower[i] <= 'Z') lower[i] += 32;

    for (r = g_gobRemaps; r->from != NULL; r++)
    {
        fromLen  = (int)strlen(r->from);
        lowerLen = (int)strlen(lower);
        if (lowerLen >= fromLen &&
            _stricmp(lower + lowerLen - fromLen, r->from) == 0)
        {
            if (g_gameRoot[0])
                _snprintf(out, outSize, "%s\\%s", g_gameRoot, r->to);
            else
                _snprintf(out, outSize, "%s", r->to);
            return;
        }
    }

    if (in[1] == ':')
    {
        strncpy(out, in, outSize - 1);
        out[outSize - 1] = '\0';
        return;
    }

    src = in;
    if (src[0] == '.' && (src[1] == '/' || src[1] == '\\')) src += 2;
    if (g_gameRoot[0])
        _snprintf(out, outSize, "%s\\%s", g_gameRoot, src);
    else
        _snprintf(out, outSize, "%s", src);

    for (i = 0; out[i]; i++) if (out[i] == '/') out[i] = '\\';

    {
        char *p;
        for (p = out; *p; p++)
        {
            if (_strnicmp(p, "\\resource", 9) == 0)
            { p[1]='R';p[2]='e';p[3]='s';p[4]='o';p[5]='u';p[6]='r';p[7]='c';p[8]='e'; }
            if (_strnicmp(p, "\\episode", 8) == 0)
            { p[1]='E';p[2]='p';p[3]='i';p[4]='s';p[5]='o';p[6]='d';p[7]='e'; }
        }
    }
}

static void xbox_ProbeGameRoot(void)
{
    /*
     * \??\D: is a kernel symlink the dashboard sets to the XBE's directory.
     * Works on both real hardware and CXBX-R. Try it first.
     * Fall back to device paths for disc/partition root installs.
     */
    typedef struct { const char *path; const char *root; } NtProbe;
    static const NtProbe ntProbes[] = {
        { "\\??\\D:\\Resource\\JK_.CD",                        "\\??\\D:" },
        { "\\Device\\CdRom0\\Resource\\JK_.CD",                "\\Device\\CdRom0" },
        { "\\Device\\Harddisk0\\Partition1\\Resource\\JK_.CD", "\\Device\\Harddisk0\\Partition1" },
        { "\\Device\\Harddisk0\\Partition6\\Resource\\JK_.CD", "\\Device\\Harddisk0\\Partition6" },
        { "\\Device\\Harddisk0\\Partition7\\Resource\\JK_.CD", "\\Device\\Harddisk0\\Partition7" },
        { NULL, NULL }
    };
    int i;
    HANDLE h;
    long status;

    /* Probe for game data */
    for (i = 0; ntProbes[i].path; i++)
    {
        status = xbox_NtOpen(ntProbes[i].path, &h);
        XDBGF("NtOpen '%s': 0x%08X\n", ntProbes[i].path, status);
        if (status >= 0)
        {
            NtClose(h);
            strncpy(g_gameRoot, ntProbes[i].root, sizeof(g_gameRoot) - 1);
            XDBGF("stdFile: game root = '%s'\n", g_gameRoot);
            return;
        }
    }

    /* Fallback: try CreateFileA with drive letters */
    {
        static const char *caProbes[] = { "D:\\Resource\\JK_.CD", "Resource\\JK_.CD", NULL };
        static const char *caRoots[]  = { "D:",                   "",                  NULL };
        for (i = 0; caProbes[i]; i++)
        {
            h = CreateFileA(caProbes[i], GENERIC_READ, FILE_SHARE_READ,
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            XDBGF("CreateFileA '%s': %s (err=%lu)\n", caProbes[i],
                   h != INVALID_HANDLE_VALUE ? "OK" : "FAIL",
                   h == INVALID_HANDLE_VALUE ? GetLastError() : 0UL);
            if (h != INVALID_HANDLE_VALUE)
            {
                CloseHandle(h);
                strncpy(g_gameRoot, caRoots[i], sizeof(g_gameRoot) - 1);
                XDBGF("stdFile: game root = '%s'\n", g_gameRoot);
                return;
            }
        }
    }

    XDBG("stdFile: all probes failed, no game data found\n");
}

/* Forward declaration */
static const char* xbox_StripNtPrefix(const char *path);

/* =========================================================================
 * File I/O using Win32/XTL handles
 * ====================================================================== */
static intptr_t xbox_fileOpen(const char *path, const char *mode)
{
    char xbPath[260];
    DWORD access, share, disposition;
    HANDLE h;
    XboxFile *xf;

    xbox_TranslatePath(path, xbPath, sizeof(xbPath));
    if (g_fileOpenLogCount < FILEOPEN_LOG_LIMIT) {
        XDBGF("fileOpen('%s' -> '%s', '%s')\n", path, xbPath, mode);
    }

    /* Parse mode — Win32 disposition constants from WinBase.h:105-109.
     * CREATE_NEW=1, CREATE_ALWAYS=2, OPEN_EXISTING=3, OPEN_ALWAYS=4, TRUNCATE_EXISTING=5.
     * Mode strings: "r"/"rb", "w"/"wb", "a"/"ab", and +variants like "r+b"/"rb+"/"w+b"/"wb+" */
    if (mode[0] == 'r')
    {
        access      = GENERIC_READ;
        share       = FILE_SHARE_READ;
        disposition = 3; /* OPEN_EXISTING */
    }
    else if (mode[0] == 'w')
    {
        access      = GENERIC_WRITE;
        share       = 0;
        disposition = 2; /* CREATE_ALWAYS */
    }
    else /* 'a' or anything else */
    {
        access      = GENERIC_READ | GENERIC_WRITE;
        share       = 0;
        disposition = 4; /* OPEN_ALWAYS */
    }

    /* Any '+' in the mode string means read+write (e.g. "r+b", "rb+", "w+b", "wb+") */
    if (strchr(mode, '+'))
    {
        access = GENERIC_READ | GENERIC_WRITE;
    }

    /* Use CreateFileA with drive-letter path (strip \??\ prefix).
     * ReadFile/WriteFile from xapilib require Win32 handles, not
     * raw NT handles from NtCreateFile. */
    {
        const char *cleanPath = xbox_StripNtPrefix(xbPath);
        h = CreateFileA(cleanPath, access, share, NULL, disposition,
                        FILE_ATTRIBUTE_NORMAL, NULL);
        if (g_fileOpenLogCount < FILEOPEN_LOG_LIMIT) {
            XDBGF("CreateFileA('%s', disp=%lu): %s\n", cleanPath,
                   (unsigned long)disposition,
                   h != INVALID_HANDLE_VALUE ? "OK" : "FAIL");
        }
        if (h == INVALID_HANDLE_VALUE)
        {
            /* Fallback: NtCreateFile. Use Nt* APIs for I/O on this handle. */
            long status = xbox_NtOpenEx(xbPath, &h, access, share, disposition);
            if (status < 0)
            {
                if (g_fileOpenLogCount < FILEOPEN_LOG_LIMIT)
                    XDBGF("fileOpen: FAIL 0x%08X\n", status);
                g_fileOpenLogCount++;
                return (intptr_t)0;
            }
            if (g_fileOpenLogCount < FILEOPEN_LOG_LIMIT)
                XDBG("fileOpen: OK (NtOpen, using NtReadFile)\n");
            xf = xbox_AllocFile();
            if (!xf) { NtClose(h); return (intptr_t)0; }
            xf->hFile       = h;
            xf->bEof        = 0;
            xf->bIsNtHandle = 1;
            return XFILE_TO_FHAND(xf);
        }
        else
        {
            if (g_fileOpenLogCount < FILEOPEN_LOG_LIMIT)
                XDBG("fileOpen: OK (CreateFileA)\n");
        }
    }

    g_fileOpenLogCount++;

    xf = xbox_AllocFile();
    if (!xf) { CloseHandle(h); return (intptr_t)0; }

    xf->hFile       = h;
    xf->bEof        = 0;
    xf->bIsNtHandle = 0;
    return XFILE_TO_FHAND(xf);
}

static int xbox_fileClose(intptr_t fhand)
{
    XboxFile *xf;
    if (!fhand) return 0;
    xf = FHAND_TO_XFILE(fhand);
    if (xf->hFile != INVALID_HANDLE_VALUE)
    {
        if (xf->bIsNtHandle)
            NtClose(xf->hFile);
        else
            CloseHandle(xf->hFile);
        xf->hFile = INVALID_HANDLE_VALUE;
    }
    return 0;
}

static size_t xbox_fileRead(intptr_t fhand, void *buf, size_t len)
{
    XboxFile *xf;
    DWORD read = 0;
    static int s_readCalls = 0;
    int isFirstFew = (s_readCalls < 8);
    s_readCalls++;

    if (isFirstFew)
        XDBGF("xbox_fileRead#%d: enter fhand=%p buf=%p len=%u\n",
              s_readCalls, (void*)fhand, buf, (unsigned)len);

    if (!fhand || !buf) {
        if (isFirstFew) XDBG("xbox_fileRead: bad args, returning 0\n");
        return 0;
    }
    xf = FHAND_TO_XFILE(fhand);
    if (xf->hFile == INVALID_HANDLE_VALUE) {
        if (isFirstFew) XDBG("xbox_fileRead: invalid handle, returning 0\n");
        return 0;
    }

    if (isFirstFew)
        XDBGF("xbox_fileRead#%d: bIsNtHandle=%d hFile=%p\n",
              s_readCalls, xf->bIsNtHandle, xf->hFile);

    if (xf->bIsNtHandle)
    {
        XB_IO_STATUS_BLOCK iosb;
        long status;
        iosb.Information = 0;
        status = NtReadFile(xf->hFile, NULL, NULL, NULL, &iosb, buf, (unsigned long)len, NULL);
        read = (DWORD)iosb.Information;
        if (isFirstFew)
            XDBGF("xbox_fileRead#%d: NtReadFile read=%u st=0x%08X\n",
                  s_readCalls, (unsigned)read, status);
    }
    else
    {
        BOOL ok = ReadFile(xf->hFile, buf, (DWORD)len, &read, NULL);
        if (isFirstFew)
            XDBGF("xbox_fileRead#%d: ReadFile ok=%d read=%u\n",
                  s_readCalls, ok, (unsigned)read);
    }

    if (read < len) xf->bEof = 1;
    return (size_t)read;
}

static const char *xbox_fileGets(intptr_t fhand, char *buf, size_t len)
{
    XboxFile *xf;
    size_t i;
    char c;
    DWORD read;

    if (!fhand || !buf || len == 0) return NULL;
    xf = FHAND_TO_XFILE(fhand);
    if (xf->hFile == INVALID_HANDLE_VALUE || xf->bEof) return NULL;

    for (i = 0; i < len - 1; i++)
    {
        read = 0;
        if (xf->bIsNtHandle)
        {
            XB_IO_STATUS_BLOCK iosb;
            iosb.Information = 0;
            NtReadFile(xf->hFile, NULL, NULL, NULL, &iosb, &c, 1, NULL);
            read = (DWORD)iosb.Information;
        }
        else
        {
            ReadFile(xf->hFile, &c, 1, &read, NULL);
        }
        if (read == 0) { xf->bEof = 1; break; }
        buf[i] = c;
        if (c == '\n') { i++; break; }
    }
    buf[i] = '\0';
    return (i > 0) ? buf : NULL;
}

static size_t xbox_fileWrite(intptr_t fhand, void *buf, size_t len)
{
    XboxFile *xf;
    DWORD written = 0;
    if (!fhand || !buf) return 0;
    xf = FHAND_TO_XFILE(fhand);
    if (xf->hFile == INVALID_HANDLE_VALUE) return 0;

    if (xf->bIsNtHandle)
    {
        XB_IO_STATUS_BLOCK iosb;
        iosb.Information = 0;
        NtWriteFile(xf->hFile, NULL, NULL, NULL, &iosb, buf, (unsigned long)len, NULL);
        written = (DWORD)iosb.Information;
    }
    else
    {
        WriteFile(xf->hFile, buf, (DWORD)len, &written, NULL);
    }
    return (size_t)written;
}

static int xbox_fileEof(intptr_t fhand)
{
    XboxFile *xf;
    if (!fhand) return 1;
    xf = FHAND_TO_XFILE(fhand);
    return xf->bEof;
}

static int xbox_ftell(intptr_t fhand)
{
    XboxFile *xf;
    if (!fhand) return -1;
    xf = FHAND_TO_XFILE(fhand);
    if (xf->hFile == INVALID_HANDLE_VALUE) return -1;

    if (xf->bIsNtHandle)
    {
        XB_IO_STATUS_BLOCK iosb;
        LARGE_INTEGER pos;
        pos.QuadPart = 0;
        if (NtQueryInformationFile(xf->hFile, &iosb, &pos, sizeof(pos),
                                    XB_FilePositionInformation) < 0)
            return -1;
        return (int)pos.LowPart;
    }
    return (int)SetFilePointer(xf->hFile, 0, NULL, FILE_CURRENT);
}

static int xbox_fseek(intptr_t fhand, int offset, int whence)
{
    XboxFile *xf;
    if (!fhand) return -1;
    xf = FHAND_TO_XFILE(fhand);
    if (xf->hFile == INVALID_HANDLE_VALUE) return -1;

    if (xf->bIsNtHandle)
    {
        /* For NT handles: compute absolute position, then set it.
         * SEEK_SET=0: absolute. SEEK_CUR=1: relative to current. SEEK_END=2: relative to end. */
        XB_IO_STATUS_BLOCK iosb;
        LARGE_INTEGER pos;
        long curPos, fileSize;

        if (whence == 0) /* SEEK_SET */
        {
            pos.QuadPart = offset;
        }
        else if (whence == 1) /* SEEK_CUR */
        {
            pos.QuadPart = 0;
            NtQueryInformationFile(xf->hFile, &iosb, &pos, sizeof(pos), XB_FilePositionInformation);
            curPos = (long)pos.LowPart;
            pos.QuadPart = curPos + offset;
        }
        else if (whence == 2) /* SEEK_END */
        {
            /* Query file size via FILE_STANDARD_INFORMATION */
            struct { LARGE_INTEGER AllocSize; LARGE_INTEGER EndOfFile;
                     unsigned long NumLinks; unsigned char DeletePending; unsigned char Dir; } stdInfo;
            NtQueryInformationFile(xf->hFile, &iosb, &stdInfo, sizeof(stdInfo), XB_FileStandardInformation);
            fileSize = (long)stdInfo.EndOfFile.LowPart;
            pos.QuadPart = fileSize + offset;
        }
        else
            return -1;

        NtSetInformationFile(xf->hFile, &iosb, &pos, sizeof(pos), XB_FilePositionInformation);
        xf->bEof = 0;
        return 0;
    }

    /* Win32 handle path */
    {
        DWORD method;
        switch (whence) {
            case 0: method = FILE_BEGIN;   break;
            case 1: method = FILE_CURRENT; break;
            case 2: method = FILE_END;     break;
            default: return -1;
        }
        SetFilePointer(xf->hFile, offset, NULL, method);
        xf->bEof = 0;
        return 0;
    }
}

static int xbox_fileSize(intptr_t fhand)
{
    XboxFile *xf;
    if (!fhand) return 0;
    xf = FHAND_TO_XFILE(fhand);
    if (xf->hFile == INVALID_HANDLE_VALUE) return 0;

    if (xf->bIsNtHandle)
    {
        XB_IO_STATUS_BLOCK iosb;
        struct { LARGE_INTEGER AllocSize; LARGE_INTEGER EndOfFile;
                 unsigned long NumLinks; unsigned char DeletePending; unsigned char Dir; } stdInfo;
        if (NtQueryInformationFile(xf->hFile, &iosb, &stdInfo, sizeof(stdInfo),
                                    XB_FileStandardInformation) < 0)
            return 0;
        return (int)stdInfo.EndOfFile.LowPart;
    }
    return (int)GetFileSize(xf->hFile, NULL);
}

static int xbox_filePrintf(intptr_t fhand, const char *fmt, ...)
{
    char buf[512];
    va_list a;
    XboxFile *xf;
    int len;
    if (!fhand) return 0;
    xf = FHAND_TO_XFILE(fhand);
    if (xf->hFile == INVALID_HANDLE_VALUE) return 0;
    va_start(a, fmt);
    len = _vsnprintf(buf, sizeof(buf) - 1, fmt, a);
    va_end(a);
    if (len > 0)
    {
        if (xf->bIsNtHandle)
        {
            XB_IO_STATUS_BLOCK iosb;
            NtWriteFile(xf->hFile, NULL, NULL, NULL, &iosb, buf, (unsigned long)len, NULL);
        }
        else
        {
            DWORD written;
            WriteFile(xf->hFile, buf, (DWORD)len, &written, NULL);
        }
    }
    return len;
}

static const wchar_t *xbox_fileGetws(intptr_t fhand, wchar_t *buf, size_t len)
{
    /* Read UTF-16 chars one at a time */
    XboxFile *xf;
    size_t i;
    wchar_t c;
    DWORD read;

    if (!fhand || !buf || len == 0) return NULL;
    xf = FHAND_TO_XFILE(fhand);
    if (xf->hFile == INVALID_HANDLE_VALUE || xf->bEof) return NULL;

    for (i = 0; i < len - 1; i++)
    {
        read = 0;
        if (xf->bIsNtHandle)
        {
            XB_IO_STATUS_BLOCK iosb;
            iosb.Information = 0;
            NtReadFile(xf->hFile, NULL, NULL, NULL, &iosb, &c, sizeof(wchar_t), NULL);
            read = (DWORD)iosb.Information;
        }
        else
        {
            ReadFile(xf->hFile, &c, sizeof(wchar_t), &read, NULL);
        }
        if (read < sizeof(wchar_t)) { xf->bEof = 1; break; }
        buf[i] = c;
        if (c == L'\n') { i++; break; }
    }
    buf[i] = L'\0';
    return (i > 0) ? buf : NULL;
}

/* =========================================================================
 * util_FileExists — also uses CreateFileA so CXBX-R intercepts it
 * ====================================================================== */
int util_FileExists(const char *path)
{
    char xbPath[260];
    HANDLE h;
    long status;
    xbox_TranslatePath(path, xbPath, sizeof(xbPath));
    status = xbox_NtOpen(xbPath, &h);
    XDBGF("util_FileExists('%s' -> '%s'): 0x%08X\n", path, xbPath, status);
    if (status >= 0) { NtClose(h); return 1; }
    return 0;
}

int util_FileExistsLowLevel(const char *path) { return util_FileExists(path); }

/* =========================================================================
 * Memory / misc wrappers
 * ====================================================================== */
static int  xbox_printStub(const char *fmt, ...)       { (void)fmt; return 0; }
static void xbox_assertStub(const char *a, const char *b, int c) { (void)a;(void)b;(void)c; }
static void *xbox_alloc(unsigned int sz)               { return malloc(sz); }
static void  xbox_free(void *p)                        { free(p); }
static void *xbox_realloc(void *p, unsigned int sz)    { return realloc(p, sz); }
static unsigned int xbox_getTimerTick(void)            { return (unsigned int)GetTickCount(); }
static void *xbox_allocHandle(size_t sz)               { return malloc(sz); }
static void  xbox_freeHandle(void *p)                  { free(p); }
static void *xbox_reallocHandle(void *p, size_t sz)    { return realloc(p, sz); }
static unsigned int xbox_lockHandle(unsigned int h)    { (void)h; return 0; }
static void xbox_unlockHandle(unsigned int h)          { (void)h; }

static void xbox_errorPrint(const char *fmt, ...)
{
    char buf[512]; va_list a;
    va_start(a, fmt); _vsnprintf(buf, sizeof(buf)-1, fmt, a); va_end(a);
    XDBG(buf);
}

/* =========================================================================
 * HostServices table
 * ====================================================================== */
struct HostServices;
extern HostServices *std_pHS;

static void *g_xboxHS[] =
{
    (void*)0,                       /* some_float      */
    (void*)xbox_printStub,          /* messagePrint    */
    (void*)xbox_printStub,          /* statusPrint     */
    (void*)xbox_printStub,          /* warningPrint    */
    (void*)xbox_errorPrint,         /* errorPrint      */
    (void*)xbox_printStub,          /* debugPrint      */
    (void*)xbox_assertStub,         /* assert          */
    (void*)0,                       /* unk_0           */
    (void*)xbox_alloc,              /* alloc           */
    (void*)xbox_free,               /* free            */
    (void*)xbox_realloc,            /* realloc         */
    (void*)xbox_getTimerTick,       /* getTimerTick    */
    (void*)xbox_fileOpen,           /* fileOpen        */
    (void*)xbox_fileClose,          /* fileClose       */
    (void*)xbox_fileRead,           /* fileRead        */
    (void*)xbox_fileGets,           /* fileGets        */
    (void*)xbox_fileWrite,          /* fileWrite       */
    (void*)xbox_fileEof,            /* fileEof         */
    (void*)xbox_ftell,              /* ftell           */
    (void*)xbox_fseek,              /* fseek           */
    (void*)xbox_fileSize,           /* fileSize        */
    (void*)xbox_filePrintf,         /* filePrintf      */
    (void*)xbox_fileGetws,          /* fileGetws       */
    (void*)0,                       /* suggestHeap (STDPLATFORM_HEAP_SUGGESTIONS) */
    (void*)xbox_allocHandle,        /* allocHandle     */
    (void*)xbox_freeHandle,         /* freeHandle      */
    (void*)xbox_reallocHandle,      /* reallocHandle   */
    (void*)xbox_lockHandle,         /* lockHandle      */
    (void*)xbox_unlockHandle,       /* unlockHandle    */
};

/* =========================================================================
 * stdFile_xbox_Startup
 * ====================================================================== */
const char *stdFile_xbox_GetGameRoot(void) { return g_gameRoot; }

void stdFile_xbox_Startup(void)
{
    std_pHS = (HostServices*)g_xboxHS;
    XDBG("stdFile_xbox_Startup: HostServices wired\n");
    xbox_ProbeGameRoot();
    XDBGF("stdFile_xbox_Startup: game root = %s\\\n", g_gameRoot);
}

/* =========================================================================
 * stdFileUtil — Xbox implementation of directory scanning
 *
 * Uses FindFirstFileA/FindNextFileA/FindClose (Win32 API) — the same path
 * Re-Volt (Content.cpp:705-812) and UC2 (FFileManagerXbox.h:928-946) use on
 * Xbox.  The XDK CRT _findfirst/_findnext path was flaky in our testing
 * and hung at variable points; FindFirstFileA is the canonical retail-game
 * approach.
 *
 * Paths are translated through xbox_TranslatePath, then the NT device
 * prefix "\??\" is stripped because FindFirstFileA expects drive letter
 * paths.
 * ====================================================================== */
#include "General/stdFileUtil.h"

/* Strip "\??\" prefix from NT device paths for CRT _findfirst.
 * "\??\D:\foo" -> "D:\foo" */
static const char* xbox_StripNtPrefix(const char *path)
{
    if (path[0] == '\\' && path[1] == '?' && path[2] == '?' && path[3] == '\\')
        return path + 4;
    return path;
}

stdFileSearch* stdFileUtil_NewFind(const char *path, int a2, const char *extension)
{
    stdFileSearch *s;
    char xlat[256];
    const char *clean;

    XDBGF("NewFind: enter path='%s' a2=%d ext='%s'\n",
          path ? path : "(null)", a2, extension ? extension : "(null)");

    s = (stdFileSearch *)malloc(sizeof(stdFileSearch));
    if (!s) { XDBG("NewFind: malloc failed\n"); return 0; }
    XDBG("NewFind: malloc ok\n");

    memset(s, 0, sizeof(stdFileSearch));
    s->isNotFirst = 0;
    s->field_88   = -1;

    if (a2 < 0)
        return s;

    xbox_TranslatePath(path, xlat, sizeof(xlat));
    clean = xbox_StripNtPrefix(xlat);

    if (a2 <= 2)
        _snprintf(s->path, sizeof(s->path) - 1, "%s\\*.*", clean);
    else if (a2 == 3 && extension)
        _snprintf(s->path, sizeof(s->path) - 1, "%s\\*.%s", clean, extension);
    else
        _snprintf(s->path, sizeof(s->path) - 1, "%s\\*.*", clean);

    s->path[sizeof(s->path) - 1] = '\0';

    XDBGF("stdFileUtil_NewFind: pattern='%s'\n", s->path);
    return s;
}

int stdFileUtil_FindNext(stdFileSearch *s, stdFileSearchResult *result)
{
    WIN32_FIND_DATAA fd;
    BOOL ok;

    if (!s) return 0;

    memset(result, 0, sizeof(stdFileSearchResult));

    if (!s->isNotFirst)
    {
        /* First call — initiate search via Win32 API */
        HANDLE h = FindFirstFileA(s->path, &fd);
        if (h == INVALID_HANDLE_VALUE)
            return 0;
        /* Stash HANDLE in field_88 (32-bit on Xbox, fits) */
        s->field_88 = (int)(intptr_t)h;
        s->isNotFirst = 1;
        ok = TRUE;
    }
    else
    {
        ok = FindNextFileA((HANDLE)(intptr_t)s->field_88, &fd);
        if (!ok)
            return 0;
    }

    strncpy(result->fpath, fd.cFileName, sizeof(result->fpath) - 1);
    result->fpath[sizeof(result->fpath) - 1] = '\0';
    /* time_write left at 0 — engine doesn't use it for GOB scan */
    result->is_subdirectory =
        (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 0x10 : 0;

    return 1;
}

void stdFileUtil_DisposeFind(stdFileSearch *s)
{
    if (!s) return;
    if (s->isNotFirst && s->field_88 != -1 &&
        (HANDLE)(intptr_t)s->field_88 != INVALID_HANDLE_VALUE)
        FindClose((HANDLE)(intptr_t)s->field_88);
    free(s);
}

BOOL stdFileUtil_MkDir(LPCSTR lpPathName)
{
    char xlat[260];
    const char *clean;

    if (!lpPathName || !lpPathName[0])
        return FALSE;

    xbox_TranslatePath(lpPathName, xlat, sizeof(xlat));
    clean = xbox_StripNtPrefix(xlat);
    if (CreateDirectoryA(clean, NULL))
        return TRUE;

    return GetLastError() == ERROR_ALREADY_EXISTS;
}

int stdFileUtil_DelFile(char *lpFileName)
{
    char xlat[260];
    const char *clean;

    if (!lpFileName || !lpFileName[0])
        return 0;

    xbox_TranslatePath(lpFileName, xlat, sizeof(xlat));
    clean = xbox_StripNtPrefix(xlat);
    return DeleteFileA(clean) ? 1 : 0;
}
