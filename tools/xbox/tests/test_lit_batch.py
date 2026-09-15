"""Exercise the actual Xbox batch class against a recording device."""
from pathlib import Path
import shutil, subprocess, tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/Platform/Xbox/fakeglx.cpp').read_text()
header=(root/'src/Platform/Xbox/xbox_vertex_submit.h').read_text()
a=s.index('class OGLPrimitiveVertexBuffer',s.index('#ifdef USE_BEGINEND'));b=s.index('#endif // USE_BEGINEND',a)
m=s.index('#define D3DRGBFAST(_r');n=s.index('#define RGBA_MAKE',m)
h=s.index('inline long Truncate(float f)');j=s.index('\n}',h)+2
code=r"""
#include <cassert>
#include <xmmintrin.h>
#include <cstdint>
#include <vector>
#include <cstring>
typedef uint32_t DWORD; typedef int HRESULT, DX_DIRECT3D, D3DPRIMITIVETYPE;
typedef unsigned char GLubyte;typedef unsigned GLenum,GLuint;
#define S_OK 0
#define D3DFVF_XYZ 2
#define D3DFVF_DIFFUSE 64
#define D3DFVF_TEX1 256
#define D3DFVF_TEX2 512
void xbox_debug_PerfPrintf(const char*,...){}
#define D3DVSDE_DIFFUSE 3
enum {GL_POINTS,GL_LINES,GL_LINE_STRIP,GL_TRIANGLES,GL_TRIANGLE_STRIP,GL_TRIANGLE_FAN};
enum {D3DPT_POINTLIST,D3DPT_LINELIST,D3DPT_LINESTRIP,D3DPT_TRIANGLELIST,D3DPT_TRIANGLESTRIP,D3DPT_TRIANGLEFAN};
CLAMP
CONVERSION
MACRO
struct Vertex {float x,y,z;DWORD color;float u,v,u1,v1;};
struct Device {
 int begins=0,ends=0,immediate=0,colors=0;std::vector<int> counts;std::vector<Vertex> vertices,immediateVertices;Vertex current={};
 void SetVertexShader(DWORD){}
 void SetVertexData4ub(int,unsigned char r,unsigned char g,unsigned char b,unsigned char a){++colors;current.color=(DWORD)a<<24|(DWORD)r<<16|(DWORD)g<<8|b;}
 void SetVertexData4f(int reg,float x,float y,float z,float w){if(reg==-1){++immediate;current.x=x;current.y=y;current.z=z;immediateVertices.push_back(current);}else{++colors;current.color=D3DRGBFAST(Clamp(x),Clamp(y),Clamp(z),Clamp(w));}}
 void SetVertexData2f(int reg,float u,float v){if(reg==9){current.u=u;current.v=v;}else{assert(reg==10);current.u1=u;current.v1=v;}}
 void Begin(int){++begins;}void End(){++ends;}
 void DrawVerticesUP(int type,int count,void *data,int stride){
  assert(type==D3DPT_TRIANGLELIST && count%3==0 && count<=768 && stride==32);
  counts.push_back(count);Vertex *v=(Vertex*)data;vertices.insert(vertices.end(),v,v+count);
 }
};
typedef Device *LPDIRECT3DDEVICE;
CLASS
int main(){
 Device d; OGLPrimitiveVertexBuffer b;b.Initialize(&d,0,false,D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
 b.SetTextureCoord(1,.125f,.875f);b.Begin(GL_TRIANGLES);b.BeginLitBatch();
 for(int i=0;i<1541;++i){
  float light=(i%256)/255.f;
  b.SetColor(light,light,light,.5f);b.SetTextureCoord0(i*.25f,-.5f);b.SetVertex((float)i,2,3);
 }
 b.End();assert(d.immediate==0 && d.vertices.size()==1539);
 assert(d.counts.size()==3 && d.counts[0]==768 && d.counts[1]==768 && d.counts[2]==3);
 for(int i=0;i<1539;++i){
  Vertex v=d.vertices[i];assert(v.x==i && v.y==2 && v.z==3 && v.u==i*.25f && v.v==-.5f);
  assert(v.u1==.125f && v.v1==.875f);
  assert((v.color>>24)==127);
  assert((v.color&255)==((v.color>>8)&255));
  assert((int)(v.color&255)>=(i%256)-1 && (int)(v.color&255)<=(i%256));
 }
 b.Initialize(&d,0,false,D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX2);
 b.Begin(GL_TRIANGLES);b.BeginLitBatch();b.SetColor(1.f,1.f,1.f,1.f);
 for(int i=0;i<3;++i)b.SetVertex(0,0,0);b.End();assert(d.vertices.size()==1542);
 int previous=d.colors;b.Begin(GL_LINES);b.SetColor(0.f,0.f,0.f,.5f);b.SetVertex(0,0,0);b.End();
 assert(d.immediate==1 && d.colors==previous+1);
 b.Initialize(&d,0,false,D3DFVF_XYZ);b.Begin(GL_TRIANGLES);b.BeginLitBatch();b.SetVertex(1,2,3);b.End();
 assert(d.immediate==2);b.Release();
 struct Triangle {int indices[3];unsigned flags,texture;};
 XboxEngineVertex input[600]={};Triangle triangles[1025];
 for(int i=0;i<600;++i) {
  input[i].x=i*.125f;input[i].y=-i*.0625f;input[i].z=i*.03125f;
  input[i].tu=(i%11)*.25f;input[i].tv=-(i%7)*.5f;
  input[i].color=(unsigned)(i%256)<<24;
  input[i].lightLevel=(i%290-10)/255.f;
 }
 for(int i=0;i<1025;++i){triangles[i].indices[0]=i%600;triangles[i].indices[1]=(i*7)%600;triangles[i].indices[2]=(i*13)%600;triangles[i].flags=0xA5A5;triangles[i].texture=0xFFFF;}
 int lengths[]={0,1,2,255,256,257,513,1025};
 for(int mode=0;mode<2;++mode)for(int buffered=0;buffered<2;++buffered)for(int l=0;l<8;++l){
  Device reference,bulk;OGLPrimitiveVertexBuffer oldPath,newPath;
  DWORD layout=D3DFVF_XYZ|(buffered?(D3DFVF_DIFFUSE|D3DFVF_TEX2):0);
  oldPath.Initialize(&reference,0,false,layout);newPath.Initialize(&bulk,0,false,layout);
  oldPath.SetTextureCoord(1,.25f,-.125f);newPath.SetTextureCoord(1,.25f,-.125f);
  oldPath.Begin(GL_TRIANGLES);newPath.Begin(GL_TRIANGLES);oldPath.BeginLitBatch();newPath.BeginLitBatch();
  // A prior non-gray or byte color must not make the scalar cache hit.
  oldPath.SetColor((GLubyte)0,(GLubyte)64,(GLubyte)128,(GLubyte)192);
  newPath.SetColor((GLubyte)0,(GLubyte)64,(GLubyte)128,(GLubyte)192);
  oldPath.SetColor(input[0].lightLevel,.25f,.5f,1.f);
  newPath.SetColor(input[0].lightLevel,.25f,.5f,1.f);
  for(int t=0;t<lengths[l];++t)for(int j=0;j<3;++j){
   XboxEngineVertex *v=&input[triangles[t].indices[j]];unsigned a=v->color>>24;
   oldPath.SetColor(v->lightLevel,v->lightLevel,v->lightLevel,(a?a:255)/255.f);
   oldPath.SetTextureCoord0(v->tu,v->tv);
   oldPath.SetVertex(v->x,mode?v->y:v->z,mode?-v->z:-v->y);
  }
  int split=lengths[l]/2;
  newPath.SetLitTriangles(input,triangles,sizeof(Triangle),split,mode!=0);
  newPath.SetLitTriangles(input,triangles+split,sizeof(Triangle),lengths[l]-split,mode!=0);
  if(lengths[l])for(int i=0;i<3;++i){oldPath.SetVertex(1,2,3);newPath.SetVertex(1,2,3);}
  // Scalar -> colored -> scalar transitions must retain exact attribute state.
  oldPath.SetColor(.75f,.25f,.5f,.5f);newPath.SetColor(.75f,.25f,.5f,.5f);
  for(int j=0;j<3;++j){
   XboxEngineVertex *v=&input[triangles[0].indices[j]];unsigned a=v->color>>24;
   oldPath.SetColor(v->lightLevel,v->lightLevel,v->lightLevel,(a?a:255)/255.f);
   oldPath.SetTextureCoord0(v->tu,v->tv);
   oldPath.SetVertex(v->x,mode?v->y:v->z,mode?-v->z:-v->y);
  }
  newPath.SetLitTriangles(input,triangles,sizeof(Triangle),1,mode!=0);
  oldPath.End();newPath.End();
  assert(reference.counts==bulk.counts && reference.vertices.size()==bulk.vertices.size());
  assert(reference.immediateVertices.size()==bulk.immediateVertices.size());
  if(reference.vertices.size())assert(!memcmp(reference.vertices.data(),bulk.vertices.data(),reference.vertices.size()*sizeof(Vertex)));
  if(reference.immediateVertices.size())assert(!memcmp(reference.immediateVertices.data(),bulk.immediateVertices.data(),reference.immediateVertices.size()*sizeof(Vertex)));
  assert(reference.colors==bulk.colors);
 }
}
""".replace('CONVERSION',s[h:j]).replace('MACRO',s[m:n]).replace('CLASS',header+'\n'+s[a:b]).replace('CLAMP',s[s.index('#if 1\n#define Clamp(x)'):s.index('// Converts a floating point',s.index('#if 1\n#define Clamp(x)'))])
with tempfile.TemporaryDirectory() as td:
 c=Path(td)/'test.cpp';e=Path(td)/'test.exe';c.write_text(code)
 subprocess.run([shutil.which('clang++') or r'C:\Program Files\LLVM\bin\clang++.exe',str(c),'-o',str(e)],check=True)
 subprocess.run([str(e)],check=True)
print('PASS: batch boundaries and bulk/scalar byte-identical streams, alpha/light bounds, UV persistence, both projections and immediate fallback')
