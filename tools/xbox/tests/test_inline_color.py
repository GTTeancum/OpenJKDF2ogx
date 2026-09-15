"""Run production inline submission against a device recording per-vertex color."""
from pathlib import Path
import shutil, subprocess, tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/Platform/Xbox/fakeglx.cpp').read_text().split('#ifdef USE_BEGINEND\nclass OGLPrimitiveVertexBuffer {',1)[1]
s='class OGLPrimitiveVertexBuffer {'+s.split('#endif // USE_BEGINEND',1)[0]
program=r'''
#include <cassert>
#include <cstdio>
#include <cmath>
typedef unsigned int DWORD, GLuint, GLenum;
typedef unsigned char GLubyte;
typedef int HRESULT, D3DPRIMITIVETYPE, DX_DIRECT3D;
#define S_OK 0
#define D3DVSDE_DIFFUSE 3
enum {GL_POINTS,GL_LINES,GL_LINE_STRIP,GL_TRIANGLES,GL_TRIANGLE_STRIP,GL_TRIANGLE_FAN};
enum {D3DPT_POINTLIST,D3DPT_LINELIST,D3DPT_LINESTRIP,D3DPT_TRIANGLELIST,D3DPT_TRIANGLESTRIP,D3DPT_TRIANGLEFAN};
struct Device {
    float color[4]={}; int colors=0,vertices=0;
    void SetVertexShader(DWORD) {}
    void SetVertexData4f(int attr,float r,float g,float b,float a) {
        if(attr==D3DVSDE_DIFFUSE) { color[0]=r;color[1]=g;color[2]=b;color[3]=a;colors++; }
        else vertices++;
    }
    void SetVertexData4ub(int attr,GLubyte r,GLubyte g,GLubyte b,GLubyte a) { SetVertexData4f(attr,r/255.f,g/255.f,b/255.f,a/255.f); }
    void SetVertexData2f(int,float,float) {}
    void Begin(int) {} void End() {}
};
typedef Device* LPDIRECT3DDEVICE;
SOURCE
int main() {
    Device d; OGLPrimitiveVertexBuffer v; v.Initialize(&d,nullptr,true,0);
    v.Begin(GL_TRIANGLES);
    for(int i=0;i<300;i++) {
        float c=(i/30)%3*.25f;
        v.SetColor(c,c,c,.35f); v.SetVertex(i,0,1);
        assert(d.color[0]==c && d.color[1]==c && d.color[2]==c && d.color[3]==.35f);
    }
    assert(d.vertices==300 && d.colors==10);
    v.End(); v.Begin(GL_TRIANGLES); v.SetColor(0.f,0.f,0.f,.35f); assert(d.colors==11);
    v.SetColor((GLubyte)255,(GLubyte)0,(GLubyte)0,(GLubyte)255);
    v.SetColor(0.f,0.f,0.f,.35f); assert(d.colors==13 && d.color[0]==0.f);
    v.SetColor(0.f,0.f,0.f); assert(d.color[3]==1.f);
    int before=d.colors; v.SetColor(0.f,0.f,0.f,1.f); assert(d.colors==before);
    v.Release(); v.Initialize(&d,nullptr,true,0); v.SetColor(0.f,0.f,0.f,1.f); assert(d.colors==before+1);
    float nan=std::nanf(""); v.SetColor(nan,0.f,0.f,1.f); before=d.colors;
    v.SetColor(nan,0.f,0.f,1.f); assert(d.colors==before+1);
    puts("PASS: 300 vertex colors preserved with 10 writes; batch, byte-color, alpha and reinitialization boundaries preserved");
}
'''.replace('SOURCE',s)
with tempfile.TemporaryDirectory() as tmp:
    c=Path(tmp)/'test.cpp'; exe=Path(tmp)/'test.exe';c.write_text(program)
    subprocess.run([shutil.which('clang++') or r'C:\Program Files\LLVM\bin\clang++.exe',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
