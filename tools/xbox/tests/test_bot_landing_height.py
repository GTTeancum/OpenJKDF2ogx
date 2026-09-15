"""Check lift landing targets against production actor floor clearance."""
from pathlib import Path
import shutil, subprocess, tempfile
root=Path(__file__).resolve().parents[3]
bot=(root/'src/AI/sithBot.c').read_text()
a=bot.index('    exitFloorZ = exitTarget.z;',bot.index('static int sithBot_HandleLiftExit('))
b=bot.index('    exitRise =',a)
physics=(root/'src/Engine/sithPhysics.c').read_text()
x=physics.index('flex_t sithPhysics_ThingGetInsertOffsetZ(')
y=physics.index('// MOTS altered',x)
code=r'''
#include <assert.h>
#include <math.h>
typedef double flex_t; typedef double flex_d_t;
#define RD_THINGTYPE_MODEL 1
#define SITHBOT_NODE_PORTAL 4
#define SITHBOT_NODE_FLOOR 2
struct Model { struct { double z; } insertOffset; };
typedef struct { struct { double height; } physicsParams; struct { int type; struct Model *model3; } rdthing; double moveSize; } sithThing;
PHYSICS
static double target(sithThing *thing,int kind,double nodeZ) {
 struct { int kind; } node={kind}, *targetNode=&node;
 struct { double z; } exitTarget={nodeZ}; double exitFloorZ;
 TARGET
 return exitFloorZ;
}
int main(void) {
 struct Model m={{.12}};
 sithThing actor={{0},{1,&m},.05};
 double z=target(&actor,4,1.34);
 assert(fabs(z-1.42)<1e-6);
 assert(1.27+.02<z); /* Observed stalled actor must keep clearing the ledge. */
 assert(!(1.43+.02<z));
 actor.physicsParams.height=.18;
 assert(fabs(target(&actor,2,1.34)-1.48)<1e-6);
 assert(fabs(target(&actor,0,1.45)-1.45)<1e-6); /* Spawn origin is already authored. */
 actor.physicsParams.height=0; m.insertOffset.z=0;
 assert(fabs(target(&actor,4,1.34)-1.355)<1e-6);
}
'''.replace('PHYSICS',physics[x:y]).replace('TARGET',bot[a:b])
with tempfile.TemporaryDirectory() as td:
 c=Path(td)/'test.c'; exe=Path(td)/'test.exe'; c.write_text(code)
 subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: lift targets honor physics/model clearance and preserve authored spawn origins')
