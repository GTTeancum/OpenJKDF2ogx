"""Exercise production sequential writes against an exact-byte sink and I/O faults."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'src/General/stdConffile.c').read_text()
buffer = source[source.index('#ifdef TARGET_XBOX'):source.index('int stdConffile_OpenRead(')]
opening = source[source.index('int stdConffile_OpenWrite('):source.index('// Added: Helper')]
closing = source[source.index('void stdConffile_CloseWrite('):source.index('int stdConffile_WriteLine(')]
writing = source[source.index('int stdConffile_Write(const'):source.index('int stdConffile_Printf(')]
program = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define TARGET_XBOX
static intptr_t writeFile;
static char stdConffile_aWriteFilename[128];
static unsigned char sink[200000], expected[200000];
static int used, calls, closed, failCall, openFailure;
static intptr_t open_sink(char *p, const char *m) { return openFailure ? 0 : 1; }
static size_t put(intptr_t h, void *p, size_t n) {
    ++calls;
    if (calls == failCall) { if (n) --n; }
    memcpy(sink + used, p, n); used += (int)n; return n;
}
static int close_sink(intptr_t h) { ++closed; return 0; }
struct HostServices {
    intptr_t (*fileOpen)(char*, const char*);
    size_t (*fileWrite)(intptr_t, void*, size_t);
    int (*fileClose)(intptr_t);
};
static struct HostServices hs = {open_sink, put, close_sink};
static struct HostServices *stdConffile_pHS = &hs, *pLowLevelHS = &hs;
#define stdString_SafeStrCopy(d,s,n) snprintf(d,n,"%s",s)
BUFFER
OPEN
CLOSE
WRITE
static void begin(void) {
    used = calls = closed = failCall = openFailure = 0;
    assert(stdConffile_OpenWrite("test"));
}
int main(void) {
    int i, cursor = 0;
    for (i=0;i<sizeof(expected);++i) expected[i]=(unsigned char)(i*37);
    begin(); stdConffile_BufferWrite();
    /* Packet headers, empty payloads, odd payloads, and a payload larger than
       the buffer must concatenate exactly, with a partial tail on close. */
    while(cursor < 100000) {
        int n = cursor % 151 + 1;
        assert(stdConffile_Write((char*)expected + cursor, n)); cursor += n;
        assert(stdConffile_Write("", 0));
    }
    assert(stdConffile_Write((char*)expected+cursor, 50000)); cursor+=50000;
    stdConffile_CloseWrite();
    assert(closed==1 && used==cursor && !memcmp(sink,expected,cursor));
    assert(calls == (cursor+16383)/16384);
    begin(); /* Non-save callers retain immediate writes. */
    assert(stdConffile_Write("abc",3) && calls==1 && used==3);
    stdConffile_CloseWrite();
    begin(); stdConffile_BufferWrite(); failCall=1;
    assert(!stdConffile_Write((char*)expected, 40000));
    assert(!stdConffile_Write("x",1) && !stdConffile_FlushWrite());
    stdConffile_CloseWrite(); assert(calls==1 && closed==1);
    begin(); stdConffile_BufferWrite(); failCall=1;
    assert(stdConffile_Write("tail",4));
    assert(!stdConffile_FlushWrite());
    stdConffile_CloseWrite(); assert(calls==1);
    begin(); stdConffile_BufferWrite();
    assert(stdConffile_Write("fresh",5) && stdConffile_FlushWrite());
    assert(used==5 && !memcmp(sink,"fresh",5));
    stdConffile_CloseWrite(); assert(calls==1);
    openFailure=1; assert(!stdConffile_OpenWrite("missing"));
    assert(!stdConffile_Write("x",1) && !stdConffile_FlushWrite());
    puts("PASS: save bytes, batching, tail flush, short-write failure, reopen, and immediate writes");
}
'''.replace('BUFFER', buffer).replace('OPEN', opening).replace('CLOSE', closing).replace('WRITE', writing)
with tempfile.TemporaryDirectory() as tmp:
    c = Path(tmp) / 'test.c'
    exe = Path(tmp) / 'test.exe'
    c.write_text(program)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe', str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
