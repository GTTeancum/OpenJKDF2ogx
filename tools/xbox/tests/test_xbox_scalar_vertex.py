"""Verify the GPU-visible light/alpha inputs of the Xbox textured fast path."""
from pathlib import Path
import subprocess,shutil,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/Raster/rdCache.c').read_text();a=s.index('static void rdCache_XboxBuildScalarVertex(');b=s.index('\n}',a)+2
r=(root/'src/Platform/Xbox/std3D.c').read_text();c=r.index('int std3D_XboxUsesScalarTextureLighting(');d=r.index('\n}',c)+2
code=r'''
#include <assert.h>
#include <stdint.h>
#include <math.h>
typedef struct {float lightLevel; uint32_t color;} D3DVERTEX;
typedef struct {float vertexIntensities[2],light_level_static;} rdProcEntry;
typedef struct {int texture_loaded,texture_id;} rdDDrawSurface;
int g_pfnBindTexture;
SOURCE
GATE
int main(void) {
 D3DVERTEX v;rdProcEntry f; rdDDrawSurface t={1,7};
 assert(!std3D_XboxUsesScalarTextureLighting(&t));g_pfnBindTexture=1;
 assert(std3D_XboxUsesScalarTextureLighting(&t));
 assert(!std3D_XboxUsesScalarTextureLighting(0));
 t.texture_loaded=0;assert(!std3D_XboxUsesScalarTextureLighting(&t));
 t.texture_loaded=1;t.texture_id=0;assert(!std3D_XboxUsesScalarTextureLighting(&t));
 for(int mode=0;mode<4;mode++)for(int light=0;light<256;light++)for(int alpha=0;alpha<256;alpha++) {
  f.light_level_static=(float)light;f.vertexIntensities[0]=255-light;f.vertexIntensities[1]=light;
  for(int index=0;index<2;index++) {
   float expected=mode==0 ? 1.f : (mode==3 ? f.vertexIntensities[index] : f.light_level_static)/255.f;
   rdCache_XboxBuildScalarVertex(&v,&f,index,mode,(uint32_t)alpha<<24);
   assert(fabsf(v.lightLevel-expected)<1e-7f);
   assert((v.color>>24)==(unsigned)alpha);
  }
 }
}
'''.replace('SOURCE',s[a:b]).replace('GATE',r[c:d])
with tempfile.TemporaryDirectory() as td:
 c=Path(td)/'test.c';e=Path(td)/'test.exe';c.write_text(code)
 subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(c),'-o',str(e)],check=True)
 subprocess.run([str(e)],check=True)
print('PASS: 524288 light/alpha combinations and unavailable-texture fallback gate')
