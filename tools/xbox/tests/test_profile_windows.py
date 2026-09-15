"""Exercise production profiler windows with independent synthetic clocks."""
from pathlib import Path
import shutil, subprocess, tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/Platform/Xbox/xbox_debug.c').read_text()
s=s[s.index('volatile unsigned int g_XboxClockProbe'):]
a=s.index('unsigned int xbox_debug_ProfileClock(void)');b=s.index('void xbox_debug_ProfileAdd',a)
s=s[:a]+s[b:]
program=r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#define XPROF_COUNT 13
#define XPROF_SUBMIT 12
static unsigned int tick, counter, reports, spanMs, spanUs, frames;
unsigned int GetTickCount(void) { return tick; }
unsigned int xbox_debug_ProfileClock(void) { return counter; }
static void report(const char *fmt, ...) {
    va_list ap; va_start(ap,fmt); (void)va_arg(ap,int);
    frames=va_arg(ap,unsigned int);spanMs=va_arg(ap,unsigned int);spanUs=va_arg(ap,unsigned int);
    va_end(ap); reports++;
}
#define XPERF report
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"failed line %d\n",__LINE__);return 1; } } while(0)
SOURCE
int main(void) {
    int i;
    tick=100; counter=100000;g_profileUs[0]=999999;
    xbox_debug_ProfileFrame(4, tick);CHECK(g_profileUs[0]==0 && g_profileFrames==0);
    CHECK(g_XboxClockProbe[0]==g_XboxClockProbe[5] && !(g_XboxClockProbe[0]&1) && g_XboxClockProbe[3]==tick);
    for(i=0;i<100;i++) { tick+=100;counter+=200000;xbox_debug_ProfileFrame(4, tick); }
    CHECK(reports==1 && frames==100 && spanMs==10000 && spanUs==20000000);
    g_profileUs[0]=42;tick+=3000;counter+=6000000;xbox_debug_ProfileFrame(4, tick);
    CHECK(reports==1 && g_profileUs[0]==0 && g_profileFrames==0);
    counter=4;xbox_debug_ProfileAdd(0,0xFFFFFFFEU);CHECK(g_profileUs[0]==6);
    puts("PASS: startup/pause exclusion, independent clock spans and counter wrap");return 0;
}
'''.replace('SOURCE',s)
with tempfile.TemporaryDirectory(prefix='jk-profile-') as temp:
    path=Path(temp)/'test.c';binary=Path(temp)/'test.exe';path.write_text(program)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(path),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
