"""Verify the production bitmap quad preserves topology, UVs and tint."""
from pathlib import Path
import shutil, subprocess, tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/Platform/Xbox/std3D.c').read_text()
a=s.index('    /* FakeGL now maps GL_TRIANGLES');b=s.index('    glDisable(GL_TEXTURE_2D);',a)
block=s[a:b]
program=r'''
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define GL_TRIANGLES 4
static int begins, ends, n;static float color[4],uv[2],out[6][9];
void glBegin(int mode) { if(mode!=4) exit(2);begins++; }
void glEnd(void) { ends++; }
void glColor4f(float r,float g,float b,float a) { color[0]=r;color[1]=g;color[2]=b;color[3]=a; }
void glTexCoord2f(float u,float v) { uv[0]=u;uv[1]=v; }
void glVertex3f(float x,float y,float z) {
 int i;if(n>=6)exit(3);out[n][0]=x;out[n][1]=y;out[n][2]=z;
 out[n][3]=uv[0];out[n][4]=uv[1];for(i=0;i<4;i++)out[n][5+i]=color[i];n++;
}
int main(void) {
 float dstX=10,dstY=20,dstW=30,dstH=40,u1=.125f,v1=.25f,u2=.75f,v2=.875f;
 int cr=255,cg=128,cb=0,ca=64,i,j;
 float expected[6][5]={{10,20,0,.125f,.25f},{40,20,0,.75f,.25f},{40,60,0,.75f,.875f},{10,20,0,.125f,.25f},{40,60,0,.75f,.875f},{10,60,0,.125f,.875f}};
 BLOCK
 if(begins!=1||ends!=1||n!=6)return 1;
 for(i=0;i<6;i++) {
  for(j=0;j<5;j++)if(out[i][j]!=expected[i][j])return 1;
  if(out[i][5]!=1||fabsf(out[i][6]-128.0f/255)>1e-6f||out[i][7]!=0||fabsf(out[i][8]-64.0f/255)>1e-6f)return 1;
 }
 puts("PASS: one batch preserves both triangles, UVs and per-vertex tint");return 0;
}
'''.replace('BLOCK',block)
with tempfile.TemporaryDirectory(prefix='jk-ui-') as temp:
 path=Path(temp)/'test.c';binary=Path(temp)/'test.exe';path.write_text(program)
 subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(path),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
