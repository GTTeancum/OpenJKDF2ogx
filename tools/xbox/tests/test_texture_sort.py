"""Check production texture comparators as strict orderings over separate allocations."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
s = (root / 'src/Raster/rdCache.c').read_text()
tri = s[s.index('int rdCache_TriCompare('):s.index('\n#else\nint rdCache_NGonCompare(')]
ngon = s[s.index('int rdCache_NGonCompare('):s.index('\n#endif\n\nint rdCache_ProcFaceCompare(')]
program = r'''
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
typedef struct { int is_16bit; char padding[129]; } rdDDrawSurface;
typedef struct { rdDDrawSurface *texture; } rdTri;
typedef rdTri rdNGon;
TRI
NGON
int main(void) {
    rdTri tris[33], sorted[33];
    int i,j,k;
    tris[0].texture=NULL;
    for(i=1;i<33;i++) {
        tris[i].texture=malloc(sizeof(rdDDrawSurface)); assert(tris[i].texture);
        tris[i].texture->is_16bit=i%3; /* Any nonzero value denotes 16-bit. */
    }
    for(i=0;i<33;i++) for(j=0;j<33;j++) {
        int cmp=rdCache_TriCompare(&tris[i],&tris[j]);
        assert(cmp == -rdCache_TriCompare(&tris[j],&tris[i]));
        assert((cmp==0)==(i==j));
        assert(cmp==rdCache_NGonCompare(&tris[i],&tris[j]));
        for(k=0;k<33;k++)
            if(cmp<0 && rdCache_TriCompare(&tris[j],&tris[k])<0)
                assert(rdCache_TriCompare(&tris[i],&tris[k])<0);
    }
    for(i=0;i<33;i++) sorted[i]=tris[32-i];
    qsort(sorted,33,sizeof(rdTri),rdCache_TriCompare);
    assert(sorted[0].texture==NULL);
    for(i=1;i<33;i++) {
        assert(rdCache_TriCompare(&sorted[i-1],&sorted[i])<0);
        if(i>1) assert(!!sorted[i-1].texture->is_16bit <= !!sorted[i].texture->is_16bit);
    }
    for(i=1;i<33;i++) free(tris[i].texture);
    puts("PASS: triangle/ngon null handling, distinct keys, antisymmetry, transitivity, and format grouping");
}
'''.replace('TRI',tri).replace('NGON',ngon)
with tempfile.TemporaryDirectory() as tmp:
    c=Path(tmp)/'sort.c'; exe=Path(tmp)/'sort.exe'
    c.write_text(program)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe', str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
