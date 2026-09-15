"""Compare Xbox face submission metadata with the retained software-bounds path."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "src/Raster/rdCache.c").read_text()
start = source.index("int rdCache_AddProcFace(")
brace = source.index("{", start)
depth = 1
end = brace + 1
while depth:
    depth += (source[end] == "{") - (source[end] == "}")
    end += 1
function = source[start:end]
reference = function.replace("rdCache_AddProcFace", "reference", 1).replace(
    "#if defined(TARGET_XBOX) && defined(SDL2_RENDER)", "#if 0", 1)
code = r'''
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#define TARGET_XBOX 1
#define SDL2_RENDER 1
#define RDCACHE_MAX_TRIS 8
using flex_t = float;
using flex_d_t = double;
struct rdVector3 { float x,y,z; };
struct Extent { int x,y; } rdCache_ulcExtent, rdCache_lrcExtent;
struct rdProcEntry {
 rdVector3* vertices;
 int extraData;
 unsigned numVertices;
 int vertexColorMode;
 int x_min,x_max,y_min,y_max,y_min_related,y_max_related;
 float z_min,z_max;
 void* colormap;
};
rdProcEntry rdCache_aProcFaces[RDCACHE_MAX_TRIS];
unsigned rdCache_numProcFaces, rdCache_numUsedVertices;
unsigned rdCache_numUsedTexVertices, rdCache_numUsedIntensities;
int rdroid_curProcFaceUserData;
void* rdColormap_pCurMap;
int ceilCalls;
float stdMath_Ceil(float f) { ++ceilCalls; return std::ceil(f); }
FUNCTION
REFERENCE
struct Result {
 rdProcEntry faces[RDCACHE_MAX_TRIS];
 unsigned facesUsed, vertices, uvs, lights;
 int returned;
};
uint32_t state=0x74a398f1;
uint32_t random32() { state^=state<<13; state^=state>>17; state^=state<<5; return state; }
int main() {
 rdVector3 vertices[32];
 for (unsigned trial=0;trial<40000;++trial) {
  unsigned count=3+trial%30, slot=(trial/30)%9;
  char flags=(char)(trial%256);
  for (unsigned i=0;i<32;++i) {
   vertices[i].x=(int32_t)(random32()%200000)-100000;
   vertices[i].y=(trial%4==0)?-0.0f:((int32_t)(random32()%200000)-100000)*0.0078125f;
   vertices[i].z=(int32_t)(random32()%200000)-100000;
  }
  Result result[2];
  for (int path=0;path<2;++path) {
   memset(rdCache_aProcFaces,0x5a,sizeof(rdCache_aProcFaces));
   for (auto &face:rdCache_aProcFaces) face.vertices=vertices;
   rdCache_numProcFaces=slot;
   rdCache_numUsedVertices=17;rdCache_numUsedTexVertices=21;rdCache_numUsedIntensities=33;
   rdroid_curProcFaceUserData=trial%5;rdColormap_pCurMap=vertices;
   rdCache_ulcExtent={0x7fffffff,0x7fffffff};rdCache_lrcExtent={0,0};ceilCalls=0;
   int returned=path?reference(11,count,flags):rdCache_AddProcFace(11,count,flags);
   assert(ceilCalls==(path && slot<RDCACHE_MAX_TRIS ? 4:0));
   memset(&result[path],0,sizeof(Result));
   memcpy(result[path].faces,rdCache_aProcFaces,sizeof(rdCache_aProcFaces));
   // Only software-only fields are intentionally different. Compare every
   // remaining byte, including depth, metadata, pointers and untouched slots.
   if (slot<RDCACHE_MAX_TRIS) {
    auto &face=result[path].faces[slot];
    face.x_min=face.x_max=face.y_min=face.y_max=0;
    face.y_min_related=face.y_max_related=0;
   }
   result[path].facesUsed=rdCache_numProcFaces;result[path].vertices=rdCache_numUsedVertices;
   result[path].uvs=rdCache_numUsedTexVertices;result[path].lights=rdCache_numUsedIntensities;
   result[path].returned=returned;
  }
  assert(memcmp(&result[0],&result[1],sizeof(Result))==0);
 }
 puts("PASS: 40,000 cases preserve depth bounds, metadata, counters, flags and capacity rejection; zero Xbox ceiling calls");
}
'''.replace("FUNCTION", function).replace("REFERENCE", reference)
with tempfile.TemporaryDirectory() as temp:
    cpp = Path(temp) / "test.cpp"
    exe = Path(temp) / "test.exe"
    cpp.write_text(code)
    subprocess.run([shutil.which("clang++") or r"C:\Program Files\LLVM\bin\clang++.exe",
                    "-O2", str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
