"""Exercise modern saber RGB dynamic lighting on model vertices."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'src/Primitives/rdModel3.c').read_text()
start = source.index('#if defined(TARGET_XBOX) && defined(QOL_IMPROVEMENTS) && defined(JKM_LIGHTING)\n#define RDMODEL3_MODERN_LIGHT_VERTS')
end = source.index('\nmodel3Loader_t rdModel3_RegisterLoader', start)
code = r'''
#include <assert.h>
#include <math.h>
#define TARGET_XBOX
#define QOL_IMPROVEMENTS
#define JKM_LIGHTING
typedef float flex_t;
typedef struct { float x,y,z; } rdVector3;
typedef struct { float intensity, falloffMin, falloffMax; unsigned color; } rdLight;
static int jkPlayer_bModernSaber;
static int colorMode = 1;
static int rdGetVertexColorMode(void){return colorMode;}
static void rdVector_Sub3(rdVector3 *o,const rdVector3*a,const rdVector3*b){o->x=a->x-b->x;o->y=a->y-b->y;o->z=a->z-b->z;}
static float rdVector_Len3(const rdVector3 *v){return sqrtf(v->x*v->x+v->y*v->y+v->z*v->z);}
static float rdVector_Dot3(const rdVector3 *a,const rdVector3*b){return a->x*b->x+a->y*b->y+a->z*b->z;}
'''+source[start:end]+r'''
int main(void) {
 rdLight green = {.intensity=.58f,.falloffMin=.72f,.falloffMax=.72f,.color=0x40ff40};
 rdLight *lights[1] = {&green};
 rdVector3 lightPos[1] = {{0,0,0}};
 rdVector3 verts[3] = {{0,.10f,0},{0,.42f,0},{0,.74f,0}};
 rdVector3 normals[3] = {{0,-1,0},{0,-1,0},{0,-1,0}};
 assert(!rdModel3_CalcModernDynamicRGB(lights,lightPos,1,normals,verts,3));
 jkPlayer_bModernSaber=1;
 assert(rdModel3_CalcModernDynamicRGB(lights,lightPos,1,normals,verts,3));
 assert(rdModel3_modernDynamicG[0] > rdModel3_modernDynamicR[0]);
 assert(rdModel3_modernDynamicG[0] > rdModel3_modernDynamicB[0]);
 assert(rdModel3_modernDynamicG[0] > rdModel3_modernDynamicG[1]);
 assert(rdModel3_modernDynamicG[1] > rdModel3_modernDynamicG[2]);
 assert(rdModel3_modernDynamicR[2] == 0 && rdModel3_modernDynamicG[2] == 0 && rdModel3_modernDynamicB[2] == 0);
 colorMode=0;
 assert(!rdModel3_CalcModernDynamicRGB(lights,lightPos,1,normals,verts,3));
 colorMode=1;
 assert(!rdModel3_CalcModernDynamicRGB(lights,lightPos,1,normals,verts,513));
 return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path/'test.c').write_text(code)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',
                    str(path/'test.c'), '-o', str(path/'test.exe')], check=True)
    subprocess.run([str(path/'test.exe')], check=True)
print('PASS: modern saber model RGB light is colored, local, smoothly falling off and disabled for stock paths')

primit = (root / 'src/Primitives/rdPrimit3.c').read_text()
assert 'RDPRIMIT3_DYNAMIC_R(_vertexSrc, *piVar14)' in primit
assert 'RDPRIMIT3_DYNAMIC_G(_vertexSrc, *piVar14)' in primit
assert 'RDPRIMIT3_DYNAMIC_B(_vertexSrc, *piVar14)' in primit
assert 'RDPRIMIT3_DYNAMIC_R(idxInfo, iVar12)' in primit
assert 'RDPRIMIT3_DYNAMIC_G(idxInfo, iVar12)' in primit
assert 'RDPRIMIT3_DYNAMIC_B(idxInfo, iVar12)' in primit
assert 'pfVar5[iVar12] + pfVar6[iVar12]' not in primit
assert 'pfVar7[iVar12] + pfVar6[iVar12]' not in primit
assert 'pfVar8[iVar12] + pfVar6[iVar12]' not in primit
assert 'pfVar6[iVar12] + pfVar5[iVar12]' not in primit
assert 'pfVar7[iVar12] + pfVar5[iVar12]' not in primit
assert 'pfVar8[iVar12] + pfVar5[iVar12]' not in primit
print('PASS: RGB primitive paths consume separate dynamic R/G/B channels')
