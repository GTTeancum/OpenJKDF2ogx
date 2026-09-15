"""Exercise the production Xbox clock with a slow tick source and counter edges."""
from pathlib import Path
import shutil,subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/Platform/Xbox/stdPlatform_xbox.c').read_text();s=s[s.index('static int xbox_clockInitialized'):s.index('/* Printf')]
program=r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define QueryPerformanceCounter mockCounter
#define QueryPerformanceFrequency mockFrequency
#define GetTickCount mockTick
typedef struct { int64_t QuadPart; } LARGE_INTEGER;
static uint32_t tick=100;static int64_t counter=0,frequency=3375000;static int available=1;
uint32_t GetTickCount(void) { return tick; }
int QueryPerformanceFrequency(LARGE_INTEGER *v) { v->QuadPart=frequency;return available; }
int QueryPerformanceCounter(LARGE_INTEGER *v) { v->QuadPart=counter;return available; }
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"failed line %d\n",__LINE__);return 1; } } while(0)
SOURCE
int main(void) {
 CHECK(stdPlatform_GetTimeMsec()==100);
 counter=43*frequency;tick+=24000;
 CHECK(stdPlatform_GetTimeMsec()==43100 && Linux_TimeUs()==43100000);
 counter-=frequency;CHECK(stdPlatform_GetTimeMsec()==43100);
 counter=44*frequency;CHECK(stdPlatform_GetTimeMsec()==44100);
 available=0;CHECK(stdPlatform_GetTimeMsec()==44100);available=1;
 /* Long uptime must not overflow the intermediate tick-to-us conversion. */
 counter=frequency*10000000LL;
 CHECK(Linux_TimeUs()==10000000100000ULL);
 CHECK(stdPlatform_GetTimeMsec()==(uint32_t)10000000100ULL);
 /* Fractional milliseconds retain precision in the microsecond API. */
 counter+=frequency/2;CHECK(Linux_TimeUs()==10000000600000ULL);
 /* A platform without QPC consistently retains the original fallback. */
 xbox_clockInitialized=0;available=0;tick=55;
 CHECK(stdPlatform_GetTimeMsec()==55);tick=80;CHECK(Linux_TimeUs()==80000);
 puts("PASS: real elapsed pacing, shared epoch, monotonicity, long uptime and fallback");return 0;
}
'''.replace('SOURCE',s)
with tempfile.TemporaryDirectory(prefix='jk-clock-') as temp:
 path=Path(temp)/'test.c';binary=Path(temp)/'test.exe';path.write_text(program)
 subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(path),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
