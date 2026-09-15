"""The GPU overlay must fully cover the stock background digits at fractional scales."""
from pathlib import Path
import shutil, subprocess, tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/Main/jkHud.c').read_text()
a=s.index('static void jkHud_ClearStockReadout(');b=s.index('\n#endif',a)
block=s[a:b]
program=r'''
#include <math.h>
#include <assert.h>
typedef float flex_t;
typedef struct {int x,y,width,height;} rdRect;
static float scale;
static rdRect result;
float jkHud_GetRenderScale(void) {return scale;}
void std3D_DrawUIClearedRectRGBA(int r,int g,int b,int a,rdRect *rect) {
 assert(r==0 && g==0 && b==0 && a==255); result=*rect;
}
BLOCK
int main(void) {
 for(int i=25;i<=300;i++) {
  scale=i/100.0f;
  for(int n=0;n<2;n++) {
   int x=n?23:13,y=n?43:35;
   jkHud_ClearStockReadout(0,400,x,y,15,7);
   assert(result.x<=x*scale);
   assert(result.y<=400+y*scale);
   assert(result.x+result.width>=(x+15)*scale);
   assert(result.y+result.height>=400+(y+7)*scale);
  }
 }
}
'''.replace('BLOCK',block)
with tempfile.TemporaryDirectory() as temp:
 c,exe=Path(temp)/'test.c',Path(temp)/'test.exe'
 c.write_text(program)
 subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: stock background digits fully covered at fractional scales')
