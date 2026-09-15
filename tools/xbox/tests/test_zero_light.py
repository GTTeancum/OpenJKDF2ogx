"""Regress dark-to-fullbright discontinuities using the production light conversion."""
from pathlib import Path
import subprocess, tempfile, shutil, sys
root=Path(__file__).resolve().parents[3]
s=(subprocess.check_output(['git','show','HEAD:src/Raster/rdCache.c'],cwd=root).decode() if '--baseline' in sys.argv else (root/'src/Raster/rdCache.c').read_text())
a=s.index('rdCache_aHWVertices[rdCache_totalVerts].lightLevel = light_level / 255.0;')
block=s[a:s.index('#endif',a)]
program=r'''
#include <assert.h>
#include <stdio.h>
#include <math.h>
struct { float lightLevel; } rdCache_aHWVertices[1];
int rdCache_totalVerts;
static float evaluate(int lighting_capability, float light_level) {
SOURCE
return rdCache_aHWVertices[0].lightLevel;
}
int main(void) {
 int mode,i;
 for(mode=1;mode<=3;++mode) {
  float previous=-1;
  for(i=0;i<=255;++i) {
   float value=evaluate(mode,(float)i);
   if(fabsf(value-i/255.0f)>.000001f || value<previous) {
    fprintf(stderr,"FAIL: mode=%d input=%d brightness=%f (zero must stay dark)\n",mode,i,value);return 1;
   }
   previous=value;
  }
 }
 puts("PASS: dark, diffuse and Gouraud brightness stay continuous through zero");
}
'''.replace('SOURCE',block)
with tempfile.TemporaryDirectory() as d:
 c=Path(d)/'test.c';exe=Path(d)/'test.exe';c.write_text(program)
 subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(c),'-o',str(exe)],check=True)
 sys.exit(subprocess.run([str(exe)]).returncode)
