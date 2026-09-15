"""Exercise the production HUD rectangle transform in both pixel aspects."""
from pathlib import Path
import tempfile, subprocess, shutil
root=Path(__file__).resolve().parents[3]
s=(root/'src/Platform/Xbox/std3D.c').read_text()
a=s.index('static void std3D_XboxTransformViewportUiRect(')
b=s.index('static unsigned int std3D_xboxMenuSig8',a)
code=r"""
#include <assert.h>
#include <math.h>
#include <stdio.h>
int g_xboxUiViewportOverlay, g_xboxViewportX, g_xboxViewportY, g_xboxViewportW, g_xboxViewportH;
float par;
float xboxVideo_GetPixelAspectRatio(void) { return par; }
SOURCE
int main(void) {
 int mode,slot;
 for(mode=0;mode<2;mode++) {
  par=mode?4.0f/3.0f:1;
  { /* Full-height stock HUD: gauge and its digits must move together. */
   float x=0,y=395,w=85, digitX=17,digitY=442,digitW=24;
   g_xboxUiViewportOverlay=1;
   g_xboxViewportX=g_xboxViewportY=0;
   g_xboxViewportW=640;g_xboxViewportH=480;
   std3D_XboxTransformViewportUiRect(&x,&y,&w,85);
   std3D_XboxTransformViewportUiRect(&digitX,&digitY,&digitW,10);
   assert(x==0 && y==395);
   assert(fabsf(w*par-85)<.0001);
   assert(fabsf((digitX-x)*par-17)<.0001);
   x=555;y=395;w=85;
   std3D_XboxTransformViewportUiRect(&x,&y,&w,85);
   assert(fabsf(x+w-640)<.0001);
  }
  for(slot=0;slot<4;slot++) {
   float x=580,y=410,w=50;
   g_xboxUiViewportOverlay=1;
   g_xboxViewportX=(slot&1)*320; g_xboxViewportY=(slot&2)?0:240;
   g_xboxViewportW=320;g_xboxViewportH=240;
   std3D_XboxTransformViewportUiRect(&x,&y,&w,60);
   assert(fabsf(w*par-50)<.0001);
   assert(fabsf((g_xboxViewportX+320-x-w)*par-10)<.0001);
   assert(fabsf(y-(480-g_xboxViewportY-70))<.0001);
   x=10;y=410;w=50;
   std3D_XboxTransformViewportUiRect(&x,&y,&w,60);
   assert(fabsf((x-g_xboxViewportX)*par-10)<.0001);
  }
  { float x=0,y=0,w=640;
    g_xboxUiViewportOverlay=0;
    std3D_XboxTransformViewportUiRect(&x,&y,&w,480);
    assert(fabsf(x+w*.5f-320)<.0001);
    assert(fabsf(w*par-640)<.0001);
  }
 }
 puts("PASS: HUD physical sizes and edge anchors preserved in each viewport; menu centered");
}
""".replace('SOURCE',s[a:b])
with tempfile.TemporaryDirectory() as tmp:
 c=Path(tmp)/'test.c'; exe=Path(tmp)/'test.exe'; c.write_text(code)
 subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
