"""Compare actual Xbox vertex-lighting function against its scalar fallback."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "src/Engine/rdLight.c").read_text()
start = source.index("flex_t rdLight_CalcVertexIntensities(")
end = source.index("flex_t rdLight_CalcFaceIntensity(", start)
body = source[start:end]
reference = body.replace("rdLight_CalcVertexIntensities", "reference")
optimized = body.replace("rdLight_CalcVertexIntensities", "optimized")
code = r'''
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef float flex_t;
typedef struct { float x,y,z; } rdVector3;
typedef struct { float falloffMin,intensity,cosAngleX,cosAngleY,lux; int type; } rdLight;
int Main_bMotsCompat=0;
static unsigned sqrtCalls;
void rdVector_Sub3(rdVector3 *o,const rdVector3 *a,const rdVector3 *b) {
 o->x=a->x-b->x;o->y=a->y-b->y;o->z=a->z-b->z;
}
float rdVector_Dot3(const rdVector3 *a,const rdVector3 *b) {
 return a->x*b->x+a->y*b->y+a->z*b->z;
}
float rdVector_Len3(const rdVector3 *v) { ++sqrtCalls;return sqrtf(rdVector_Dot3(v,v)); }
float rdVector_Normalize3Acc(rdVector3 *v) {
 float len=rdVector_Len3(v);
 if(len!=0.0){v->x/=len;v->y/=len;v->z/=len;}return len;
}
REFERENCE
#define TARGET_XBOX
OPTIMIZED
static uint32_t seed=4387;
static float value(void) {seed=seed*1664525+1013904223;return ((int)(seed>>16)-32768)/4096.0f;}
int main(void) {
 rdLight lights[8],*ptr[8];rdVector3 positions[8],dirs[8],vertices[32],normals[32];
 float base[32],before[32],after[32];unsigned long long saved=0;
 for(int scenario=0;scenario<12000;++scenario) {
  int count=scenario%9,verts=scenario%33;
  float scalar=fabsf(value())/4;
  for(int j=0;j<8;++j) {
   ptr[j]=&lights[j];lights[j].intensity=value();lights[j].falloffMin=value()+4;
   lights[j].type=2+j%2;lights[j].cosAngleX=.8f;lights[j].cosAngleY=.2f;lights[j].lux=1.6666666f;
   positions[j]=(rdVector3){value(),value(),value()};dirs[j]=(rdVector3){0,0,1};
  }
  for(int j=0;j<32;++j) {
   vertices[j]=(rdVector3){value(),value(),value()};normals[j]=(rdVector3){value(),value(),value()};
   rdVector_Normalize3Acc(&normals[j]);base[j]=value()/8;
  }
  /* Coincident light/vertex and exact falloff boundaries are intentional. */
  positions[0]=vertices[0];
  if(scenario%5==0)lights[0].falloffMin=0;
  if(scenario%7==0){positions[1]=vertices[0];positions[1].x+=1;lights[1].falloffMin=1;}
  memset(before,0xA5,sizeof(before));memset(after,0xA5,sizeof(after));
  sqrtCalls=0;
#ifdef JKM_LIGHTING
  Main_bMotsCompat=scenario%3==0;
  float a=reference(ptr,positions,dirs,count,normals,vertices,base,before,verts,scalar);
#else
  float a=reference(ptr,positions,count,normals,vertices,base,before,verts,scalar);
#endif
  unsigned oldCalls=sqrtCalls;sqrtCalls=0;
#ifdef JKM_LIGHTING
  float b=optimized(ptr,positions,dirs,count,normals,vertices,base,after,verts,scalar);
#else
  float b=optimized(ptr,positions,count,normals,vertices,base,after,verts,scalar);
#endif
  assert(memcmp(&a,&b,sizeof(a))==0);
  assert(memcmp(before,after,sizeof(before))==0);
  assert(sqrtCalls<=oldCalls);saved+=oldCalls-sqrtCalls;
#ifdef JKM_LIGHTING
  if(Main_bMotsCompat)assert(sqrtCalls==oldCalls);
#endif
 }
 assert(saved>1000);
 printf("PASS: 12000 vertex-light cases, bit-identical output, %llu redundant square roots removed\n",saved);
}
'''.replace("REFERENCE", reference).replace("OPTIMIZED", optimized)
with tempfile.TemporaryDirectory() as temporary:
    cfile = Path(temporary) / "test.c"
    executable = Path(temporary) / "test.exe"
    cfile.write_text(code)
    for defines in ([], ["-DJKM_LIGHTING"]):
        subprocess.run([shutil.which("clang") or r"C:\Program Files\LLVM\bin\clang.exe",
                        "-O2", "-ffp-contract=off", *defines, str(cfile), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
