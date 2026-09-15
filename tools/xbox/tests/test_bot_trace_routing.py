"""Routine bot trace stays in RAM; startup messages retain disk logging."""
from pathlib import Path
import subprocess,shutil,tempfile
s=(Path(__file__).resolve().parents[3]/'src/AI/sithBot.c').read_text()
a=s.index('static void sithBot_Logf(const char *fmt, ...)\n{');b=s.index('\n}',a)+2
code=r'''
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define TARGET_XBOX 1
int disk,ram;
char last[512];
void xbox_debug_Trace(const char *s) {ram++;strcpy(last,s);}
void xbox_debug_Print(const char *s) {disk++;strcpy(last,s);}
SOURCE
int main(void) {
 sithBot_Logf("BotMatch: shot slot=%d\n",4);assert(ram==1 && disk==0);
 assert(!strcmp(last,"BotMatch: shot slot=4\n"));
 sithBot_Logf("BotPerf: ticks=250\n");sithBot_Logf("BotProfile: tickMs=1\n");sithBot_Logf("BotLiftProbe: reached\n");
 assert(ram==4 && disk==0);
 sithBot_Logf("BotNav: cache-write-failed\n");assert(ram==4 && disk==1);
 sithBot_Logf("Unexpected failure\n");assert(ram==4 && disk==2);
}
'''.replace('SOURCE',s[a:b])
with tempfile.TemporaryDirectory() as td:
 c=Path(td)/'test.c';e=Path(td)/'test.exe';c.write_text(code)
 subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(c),'-o',str(e)],check=True)
 subprocess.run([str(e)],check=True)
print('PASS: routine bot diagnostics preserve formatted RAM trace; other messages retain disk path')
