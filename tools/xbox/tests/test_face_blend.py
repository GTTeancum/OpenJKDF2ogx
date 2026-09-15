"""Verify production face blending against the PC translucent shader equation."""
from pathlib import Path
import shutil, subprocess, tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/Platform/Xbox/std3D.c').read_text()
s=s[s.index('static void std3D_XboxApplyFaceBlend('):s.index('void std3D_DrawRenderList(void)')]
program=r'''
#include <assert.h>
#include <stdio.h>
#include <math.h>
#define GL_BLEND 1
#define GL_SRC_ALPHA 2
#define GL_ONE_MINUS_SRC_ALPHA 3
#define GL_ALPHA_TEST 4
#define GL_GREATER 5
static int enabled, src, dst;
static int alphaTest;
static float cutoff;
static void glEnable(int x) { if(x==GL_ALPHA_TEST) alphaTest=1; else {assert(x==GL_BLEND);enabled=1;} }
static void glDisable(int x) { assert(x==GL_BLEND);enabled=0; }
static void glAlphaFunc(int x,float threshold) {assert(x==GL_GREATER);cutoff=threshold;}
static void glBlendFunc(int a,int b) {src=a;dst=b;}
SOURCE
int main(void) {
    int textured, flags, a;
    for(textured=0;textured<2;textured++) for(flags=0;flags<=0x600;flags+=0x200) {
        std3D_XboxApplyFaceBlend(flags,textured);
        assert(alphaTest && cutoff>0.0f);
        assert(cutoff==((flags&0x200) ? .01f : .5f));
        assert(enabled==!!(textured || flags));
        if(!enabled) continue;
        for(a=0;a<=255;a++) {
            double alpha=a/255.0, foreground=.9, background=.2;
            double sf=src==GL_SRC_ALPHA ? alpha : 1-alpha;
            double df=dst==GL_SRC_ALPHA ? alpha : 1-alpha;
            double opacity=(flags&0x200) ? 1-alpha : alpha;
            assert(fabs(foreground*sf+background*df - (foreground*opacity+background*(1-opacity)))<1e-12);
        }
    }
    puts("PASS: opaque, cutout and inverse-alpha translucent face blend weights");
}
'''.replace('SOURCE',s)
with tempfile.TemporaryDirectory() as tmp:
    c=Path(tmp)/'test.c'; exe=Path(tmp)/'test.exe';c.write_text(program)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
