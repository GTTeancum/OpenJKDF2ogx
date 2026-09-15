"""Combined textured submission preserves attribute order and values."""
from pathlib import Path
import shutil, subprocess, tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/Platform/Xbox/fakeglx.cpp').read_text()
a=s.index('extern "C" void APIENTRY glXboxLitVertex'); b=s.index('\n}',a)+2
t=(root/'src/Platform/Xbox/std3D.c').read_text()
c=t.index('#define V_COLOR_A(v)');d=t.index('            {',c)
code=r'''
#include <cassert>
#include <cstring>
#define APIENTRY
typedef float GLfloat;
struct Recorder {
 float values[9]; int count=0;
 void glColor4f(float r,float g,float b,float a) { assert(count==0);values[count++]=r;values[count++]=g;values[count++]=b;values[count++]=a; }
 void glTexCoord2f(float u,float v) { assert(count==4);values[count++]=u;values[count++]=v; }
 void glVertex3f(float x,float y,float z) { assert(count==6);values[count++]=x;values[count++]=y;values[count++]=z; }
};
Recorder recorder; Recorder *gFakeGL=&recorder;
FUNCTION
struct Vertex {float x,y,z,tu,tv,lightLevel;unsigned color;};
int g_xboxScreenSpaceRenderList;
MACROS
int main() {
 for(int mode=0;mode<2;++mode) for(int alpha=0;alpha<256;++alpha) for(int l=0;l<256;++l) {
  g_xboxScreenSpaceRenderList=mode;
  Vertex v={-2.25f,3.5f,-.125f,-.75f,2.0f,l/255.0f,(unsigned)alpha<<24};
  float expected[]={v.lightLevel,v.lightLevel,v.lightLevel,(alpha?alpha:255)/255.0f,v.tu,v.tv,v.x,mode?v.y:v.z,mode?-v.z:-v.y};
  recorder.count=0; V_LIT_TEXTURED(&v);
  assert(recorder.count==9);
  assert(!memcmp(expected,recorder.values,sizeof expected));
 }
}
'''.replace('FUNCTION',s[a:b]).replace('MACROS',t[c:d])
with tempfile.TemporaryDirectory() as td:
 c=Path(td)/'test.cpp'; e=Path(td)/'test.exe';c.write_text(code)
 subprocess.run([shutil.which('clang++') or r'C:\Program Files\LLVM\bin\clang++.exe',str(c),'-o',str(e)],check=True)
 subprocess.run([str(e)],check=True)
print('PASS: 131072 lighting/alpha/projection combinations preserve ordered vertex attributes')
