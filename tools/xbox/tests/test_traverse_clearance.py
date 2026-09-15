"""Exercise the production teleport clearance test against a closed room."""
from pathlib import Path
import subprocess, tempfile, shutil
root=Path(__file__).resolve().parents[3]
s=(root/'src/Main/jkMain.c').read_text()
s=s[s.index('static int jkMain_XboxSmokeClearance('):s.index('static void jkMain_XboxSmokeTraverseTick(')]
program=r'''
#include <assert.h>
#include <stdio.h>
typedef struct { float x,y,z; } rdVector3;
typedef struct { int numVertices; int *vertexPosIdx; rdVector3 normal; } rdFace;
typedef struct { struct { rdFace face; } surfaceInfo; } Surface;
typedef struct { int numSurfaces,flags; Surface *surfaces; } sithSector;
typedef struct { rdVector3 *vertices; } sithWorld;
#define SITH_SECTOR_FALLDEATH 1
SOURCE
int main(void) {
    rdVector3 vertices[6]={{0,0,0},{1,0,0},{0,0,0},{0,1,0},{0,0,0},{0,0,1}};
    rdVector3 normals[6]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    int indices[6]={0,1,2,3,4,5},i;
    Surface surfaces[6]; sithWorld world={vertices}; sithSector sector={6,0,surfaces};
    rdVector3 p={.5f,.5f,.5f};
    for(i=0;i<6;++i) {surfaces[i].surfaceInfo.face.numVertices=3;surfaces[i].surfaceInfo.face.vertexPosIdx=&indices[i];surfaces[i].surfaceInfo.face.normal=normals[i];}
    assert(jkMain_XboxSmokeClearance(&world,&sector,&p,.1f));
    p.x=2; assert(!jkMain_XboxSmokeClearance(&world,&sector,&p,.1f));
    p.x=.05f; assert(!jkMain_XboxSmokeClearance(&world,&sector,&p,.1f));
    p.x=.5f;p.z=.98f;assert(!jkMain_XboxSmokeClearance(&world,&sector,&p,.04f));
    p.z=.5f;sector.flags=1;assert(!jkMain_XboxSmokeClearance(&world,&sector,&p,.1f));
    sector.flags=0;sector.numSurfaces=0;assert(!jkMain_XboxSmokeClearance(&world,&sector,&p,.1f));
    puts("PASS: interior accepted; void, wall/body overlap, eye/ceiling overlap, pit and empty sectors rejected");
}
'''.replace('SOURCE',s)
with tempfile.TemporaryDirectory() as tmp:
    c=Path(tmp)/'test.c';exe=Path(tmp)/'test.exe';c.write_text(program)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
