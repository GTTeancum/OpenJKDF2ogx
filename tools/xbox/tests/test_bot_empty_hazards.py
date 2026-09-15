"""Empty authored hazard sets skip searches; nonempty sets preserve route behavior."""
from pathlib import Path
import shutil, subprocess, tempfile
s=(Path(__file__).resolve().parents[3]/'src/AI/sithBot.c').read_text()
a=s.index('static int sithBot_HandleControlledHazardRoute(');b=s.index('static int sithBot_RunControlledHazardEscape',a)
fn=s[a:b]
old=fn.replace('    if (!sithBot_numControlledHazards)\n        return 0;\n','').replace('sithBot_HandleControlledHazardRoute','oldRoute')
code=r'''
#include <assert.h>
#include <string.h>
#include <math.h>
typedef float flex_t;
typedef struct {float x,y,z;} rdVector3;
typedef struct {int id;} sithSector;
typedef struct {rdVector3 position;sithSector *sector;struct {rdVector3 acceleration;} physicsParams;} sithThing;
typedef struct {int goalNode,nextNode,routeGoalNode,routeCommitUntilMs,nextGoalMs;} SithBotState;
int sithBot_numControlledHazards,sithBot_numNodes=2,sithTime_curMs=100,traceCalls,syncs;
struct {rdVector3 pos; sithSector *sector;} sithBot_nodes[2];
struct {sithSector *sector;} sithBot_controlledHazards[2];
sithSector sectors[3]={{0},{1},{2}};
int sithBot_FindControlledHazard(sithSector *s) {return sithBot_numControlledHazards && s->id==2 ? 0 : -1;}
float rdVector_Dist3(rdVector3 *a,rdVector3 *b){return fabsf(b->x-a->x);}
void rdVector_Copy3(rdVector3 *a,rdVector3 *b){*a=*b;}
void rdVector_Zero3(rdVector3 *a){memset(a,0,sizeof(*a));}
sithSector *sithCollision_GetSectorLookAt(sithSector *s,rdVector3 *a,rdVector3 *b,float r){traceCalls++;return &sectors[b->x>1?2:0];}
void sithBot_DampHorizontalVelocity(SithBotState *s,sithThing *b,float f){}
void sithBot_SyncPositionIfNeeded(SithBotState *s,sithThing *b){syncs++;}
FUNCTIONS
int main(void){
 sithBot_controlledHazards[0].sector=&sectors[2];
 for(int count=0;count<2;count++) for(int dist=0;dist<100;dist++) for(int sec=0;sec<3;sec++) {
  sithBot_numControlledHazards=count;
  sithBot_nodes[0].pos.x=dist*.2f;sithBot_nodes[0].sector=&sectors[sec];
  SithBotState a={3,4,5,6,7},b=a;
  sithThing aa={{0,0,0},&sectors[0],{{1,2,3}}},bb=aa;
  traceCalls=syncs=0;int before=oldRoute(&a,&aa,0),oldTraces=traceCalls,oldSyncs=syncs;
  traceCalls=syncs=0;int after=sithBot_HandleControlledHazardRoute(&b,&bb,0);
  assert(before==after && memcmp(&a,&b,sizeof(a))==0 && memcmp(&aa,&bb,sizeof(aa))==0 && oldSyncs==syncs);
  if(count) assert(traceCalls==oldTraces); else assert(traceCalls==0);
 }
}
'''.replace('FUNCTIONS',old+fn)
with tempfile.TemporaryDirectory() as temp:
 c,exe=Path(temp)/'test.c',Path(temp)/'test.exe';c.write_text(code)
 subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: 600 hazard route cases preserve behavior; empty trap sets execute zero segment traces')
