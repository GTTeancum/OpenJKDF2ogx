/*
 * xbox_debug.c  —  OpenJKDF2 Xbox debug logging
 * Location:  src/Platform/Xbox/xbox_debug.c
 *
 * Strategy 1: NtCreateFile next to XBE (via XeImageFileName) — real hardware
 * Strategy 2: NtCreateFile to HDD partitions — real hardware fallback
 * Strategy 3: CreateFileA with drive letters — dashboard-mapped drives
 */

#include "platform_xbox.h"
#include "xbox_debug.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

typedef struct { unsigned short Length; unsigned short MaximumLength; char *Buffer; } XDB_STR;
typedef struct { HANDLE RootDirectory; XDB_STR *ObjectName; unsigned long Attributes; } XDB_OA;
typedef struct { union { long Status; void *Pointer; }; unsigned long Information; } XDB_IOSB;

extern long __stdcall NtCreateFile(HANDLE*, unsigned long, XDB_OA*, XDB_IOSB*,
    LARGE_INTEGER*, unsigned long, unsigned long, unsigned long, unsigned long);
extern long __stdcall NtClose(HANDLE);
extern long __stdcall NtWriteFile(HANDLE, HANDLE, void*, void*, XDB_IOSB*,
    void*, unsigned long, LARGE_INTEGER*);

#define XDBG_BUF_SIZE 512
static HANDLE g_hLogFile = INVALID_HANDLE_VALUE;
static int g_logIsNtHandle = 0;  /* track which API created the handle */

static long xdbg_NtCreate(const char *path, HANDLE *out)
{
    XDB_STR name; XDB_OA oa; XDB_IOSB iosb;
    name.Buffer = (char*)path;
    name.Length = (unsigned short)strlen(path);
    name.MaximumLength = name.Length + 1;
    oa.RootDirectory = NULL; oa.ObjectName = &name; oa.Attributes = 0x40;
    return NtCreateFile(out, GENERIC_WRITE | 0x00100000, &oa, &iosb, NULL,
        FILE_ATTRIBUTE_NORMAL, 0,
        5,      /* FILE_OVERWRITE_IF */
        0x20 | 0x02 | 0x40); /* SYNCHRONOUS_IO_NONALERT | WRITE_THROUGH | NON_DIRECTORY */
}

void xbox_debug_Startup(void)
{
    int i;
    long status;

    /* Strategy 1: NtCreateFile to HDD partition roots (works on real hw + CXBX-R) */
    {
        static const char *ntPaths[] = {
            "\\Device\\Harddisk0\\Partition1\\debug_openjkdf2.txt",
            "\\Device\\Harddisk0\\Partition6\\debug_openjkdf2.txt",
            "\\Device\\Harddisk0\\Partition7\\debug_openjkdf2.txt",
            NULL
        };
        for (i = 0; ntPaths[i]; i++) {
            status = xdbg_NtCreate(ntPaths[i], &g_hLogFile);
            if (status >= 0) {
                g_logIsNtHandle = 1;
                xbox_debug_Print("=== OpenJKDF2 Xbox debug log ===\n");
                return;
            }
        }
    }

    /* Strategy 2: CreateFileA with drive letters (dashboard-mapped) */
    {
        static const char *caPaths[] = {
            "D:\\debug_openjkdf2.txt",
            "T:\\debug_openjkdf2.txt",
            "E:\\debug_openjkdf2.txt",
            "debug_openjkdf2.txt",
            NULL
        };
        for (i = 0; caPaths[i]; i++) {
            g_hLogFile = CreateFileA(caPaths[i], GENERIC_WRITE, FILE_SHARE_READ,
                NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
            if (g_hLogFile != INVALID_HANDLE_VALUE) {
                g_logIsNtHandle = 0;
                xbox_debug_Print("=== OpenJKDF2 Xbox debug log ===\n");
                return;
            }
        }
    }

    g_hLogFile = INVALID_HANDLE_VALUE;
    OutputDebugStringA("xbox_debug: all log paths failed\n");
}

void xbox_debug_Shutdown(void)
{
    xbox_debug_Print("=== log end ===\n");
    if (g_hLogFile != INVALID_HANDLE_VALUE) {
        if (g_logIsNtHandle)
            NtClose(g_hLogFile);
        else
            CloseHandle(g_hLogFile);
        g_hLogFile = INVALID_HANDLE_VALUE;
    }
}

void xbox_debug_Print(const char *msg)
{
    DWORD len;
    if (!msg) return;
    OutputDebugStringA(msg);
    if (g_hLogFile != INVALID_HANDLE_VALUE) {
        len = (DWORD)strlen(msg);
        if (g_logIsNtHandle) {
            XDB_IOSB iosb;
            NtWriteFile(g_hLogFile, NULL, NULL, NULL, &iosb, (void*)msg, len, NULL);
        } else {
            DWORD written;
            WriteFile(g_hLogFile, msg, len, &written, NULL);
        }
        /* Flush after every write so log survives mid-execution hangs.
         * Without this, entries near a hang sit in the OS write buffer
         * and never reach disk, hiding which line was last reached. */
        FlushFileBuffers(g_hLogFile);
    }
}

void xbox_debug_Printf(const char *fmt, ...)
{
    char buf[XDBG_BUF_SIZE];
    va_list args;
    va_start(args, fmt);
    _vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);
    buf[sizeof(buf) - 1] = '\0';
    xbox_debug_Print(buf);
}
