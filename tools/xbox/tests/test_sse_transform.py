"""Validate SSE transforms against a double reference and buffer boundaries."""
from pathlib import Path
import shutil, subprocess, tempfile
s=(Path(__file__).resolve().parents[3]/'src/Primitives/rdMatrix.c').read_text()
a=s.index('void rdMatrix_TransformPointLst34('); b=s.index('\n}',a)+2
code=r'''
#include <xmmintrin.h>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#define TARGET_XBOX
struct rdVector3 {float x,y,z;};
struct rdMatrix34 {rdVector3 rvec,lvec,uvec,scale;};
FUNCTION
uint32_t seed=1234567;
float value() {seed=seed*1664525u+1013904223u;return ((int)(seed>>12)-524288)/8192.0f;}
int main() {
 rdMatrix_TransformPointLst34(0,0,0,0);
 rdMatrix_TransformPointLst34(0,0,0,-1);
 for(int n=0;n<100000;++n) {
  rdMatrix34 m; float *mf=(float*)&m;
  for(int k=0;k<12;++k) mf[k]=value();
  rdVector3 in[3],out[5]; memset(out,0x55,sizeof out);
  for(int i=0;i<3;++i) in[i]={value(),value(),value()};
  rdVector3 guard=out[0];
  rdMatrix_TransformPointLst34(&m,in,out+1,3);
  assert(!memcmp(out,&guard,sizeof guard) && !memcmp(out+4,&guard,sizeof guard));
  for(int i=0;i<3;++i) for(int j=0;j<3;++j) {
   double p0=(double)mf[j]*in[i].x,p1=(double)mf[j+3]*in[i].y,p2=(double)mf[j+6]*in[i].z;
   double expected=p0+p1+p2+mf[j+9];
   double tolerance=5e-7*(fabs(p0)+fabs(p1)+fabs(p2)+fabs(mf[j+9])+1);
   assert(fabs(((float*)&out[i+1])[j]-expected)<=tolerance);
  }
  rdMatrix_TransformPointLst34(&m,in,in,3);
  assert(!memcmp(in,out+1,sizeof in));
 }
}
'''.replace('FUNCTION',s[a:b])
with tempfile.TemporaryDirectory() as d:
 c=Path(d)/'test.cpp';e=Path(d)/'test.exe';c.write_text(code)
 subprocess.run([shutil.which('clang++') or r'C:\Program Files\LLVM\bin\clang++.exe',str(c),'-O2','-o',str(e)],check=True)
 subprocess.run([str(e)],check=True)
print('PASS: 300000 transformed vertices, numerical bounds, guards, empty and in-place lists')
