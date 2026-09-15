"""Playback queries must respect DirectSound errors, not output garbage."""
from pathlib import Path
import shutil,subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/Platform/Xbox/stdSound_xbox.c').read_text();s=s[s.index('int stdSound_IsPlaying('):s.index('int stdSound_BufferQueueAfterAnother')]
program=r'''
#include <stdint.h>
#include <stdio.h>
typedef uint32_t DWORD;typedef int stdSound_buffer_t;typedef int rdVector3;
typedef struct { void *pDS; } XboxDSEntry;
#define DSBSTATUS_PLAYING 1
#define FAILED(x) ((x)<0)
static XboxDSEntry entry={(void*)1};static int found=1,result;static DWORD flags;
XboxDSEntry *xbox_DSFind(stdSound_buffer_t *b) { return found?&entry:0; }
int IDirectSoundBuffer_GetStatus(void *p,DWORD *s) { *s=flags;return result; }
SOURCE
int main(void) {
 stdSound_buffer_t b=0;
 flags=1;result=0;if(!stdSound_IsPlaying(&b,0))return 1;
 result=-1;if(stdSound_IsPlaying(&b,0))return 1;
 result=0;flags=0;if(stdSound_IsPlaying(&b,0))return 1;
 flags=1;found=0;if(stdSound_IsPlaying(&b,0))return 1;
 found=1;entry.pDS=0;if(stdSound_IsPlaying(&b,0))return 1;
 if(stdSound_IsPlaying(0,0))return 1;
 puts("PASS: playing/stopped/error/missing sound statuses");return 0;
}
'''.replace('SOURCE',s)
with tempfile.TemporaryDirectory(prefix='jk-sound-') as temp:
 path=Path(temp)/'test.c';binary=Path(temp)/'test.exe';path.write_text(program)
 subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(path),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
