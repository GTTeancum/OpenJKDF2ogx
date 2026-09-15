"""Compare optimized production fills with a pixel oracle, including padding."""
from pathlib import Path
import shutil, subprocess, tempfile
root = Path(__file__).resolve().parents[3]
s = (root / "src/Platform/Xbox/stdDisplay_xbox.c").read_text()
fill = s[s.index("int stdDisplay_VBufferFill("):s.index("void stdDisplay_VBufferFree")]
program = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define __int64 long long
typedef struct { int x,y,width,height; } rdRect;
typedef struct { void *surface_lock_alloc; struct { int width,height,width_in_bytes; struct { int bpp,is16bit; } format; } format; } stdVBuffer;
FILL
static int oracle(stdVBuffer *vbuf, int color, rdRect *r) {
    int x,y,b = vbuf->format.format.bpp == 16 || vbuf->format.format.is16bit ? 2 : 1;
    unsigned char *p = vbuf->surface_lock_alloc;
    if (!p) return 0;
    for(y=0;y<vbuf->format.height;y++) for(x=0;x<vbuf->format.width;x++) {
        if(r && (x<r->x || y<r->y || (long long)x >= (long long)r->x+r->width || (long long)y >= (long long)r->y+r->height)) continue;
        p[y*vbuf->format.width_in_bytes+x*b]=(unsigned char)color;
        if(b==2) p[y*vbuf->format.width_in_bytes+x*b+1]=(unsigned char)((unsigned)color>>8);
    }
    return 1;
}
int main(void) {
    unsigned char actual[4096], expected[4096];
    stdVBuffer a, e;
    int i,b,pad;
    srand(7);
    for(b=1;b<=2;b++) for(pad=0;pad<4;pad++) for(i=0;i<1000;i++) {
        rdRect r={rand()%80-40,rand()%60-30,rand()%90-10,rand()%70-10};
        int color=rand();
        memset(actual,0xCD,sizeof actual); memset(expected,0xCD,sizeof expected);
        a.surface_lock_alloc=actual+17; a.format.width=31; a.format.height=23;
        a.format.width_in_bytes=31*b+pad; a.format.format.bpp=b*8; a.format.format.is16bit=0;
        e=a;e.surface_lock_alloc=expected+17;
        stdDisplay_VBufferFill(&a,color,i%11 ? &r : NULL); oracle(&e,color,i%11 ? &r : NULL);
        if(memcmp(actual,expected,sizeof actual)) { fprintf(stderr,"fill mismatch %d %d %d\n",b,pad,i); return 1; }
    }
    puts("PASS: 8000 clipped 8/16-bit fills preserve pixels, row padding and guards");
    return 0;
}
'''.replace("FILL", fill)
with tempfile.TemporaryDirectory(prefix="jk-fill-") as temp:
    source=Path(temp)/"fill.c"; binary=Path(temp)/"fill.exe"
    source.write_text(program)
    subprocess.run([shutil.which("clang") or r"C:\Program Files\LLVM\bin\clang.exe", "-O2", str(source), "-o", str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
