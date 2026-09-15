"""Only relink a stale lift sector to a landing containing the existing position."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'src/AI/sithBot.c').read_text()
start = source.index('static void sithBot_RelinkLiftExitSector(')
end = source.index('static int sithBot_HandleLiftExit(', start)
code = r'''
#include <assert.h>
typedef struct { double x,y,z; } V;
typedef struct { double low,high; } sithSector;
typedef struct { V position; sithSector *sector; } sithThing;
int moves;
int sithIntersect_IsSphereInSector(const V *p,double r,sithSector *s) {
 return p->z >= s->low && p->z <= s->high;
}
int sithBot_GetSectorIndex(sithSector *s) { return 0; }
#define sithBot_Logf(...) ((void)0)
void sithThing_MoveToSector(sithThing *t,sithSector *s,int flags) { t->sector=s; moves++; }
SOURCE
int main(void) {
 sithSector shaft={.725,1.30}, upper={1.30,1.90};
 sithThing actor={{-8.41,-1.34,1.53},&shaft};
 sithBot_RelinkLiftExitSector(&actor,&upper);
 assert(moves==1 && actor.sector==&upper && actor.position.z==1.53);
 assert(actor.position.x==-8.41 && actor.position.y==-1.34);
 actor.sector=&shaft; actor.position.z=1.10;
 sithBot_RelinkLiftExitSector(&actor,&upper);
 assert(moves==1 && actor.sector==&shaft);
 actor.position.z=2.10;
 sithBot_RelinkLiftExitSector(&actor,&upper);
 assert(moves==1 && actor.sector==&shaft);
 actor.position.z=1.30;
 sithBot_RelinkLiftExitSector(&actor,&upper);
 assert(moves==1 && actor.sector==&shaft);
}
'''.replace('SOURCE', source[start:end])
with tempfile.TemporaryDirectory() as directory:
    c = Path(directory) / 'test.c'
    exe = Path(directory) / 'test.exe'
    c.write_text(code)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe', str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: stale-sector repair requires landing containment and preserves position')
