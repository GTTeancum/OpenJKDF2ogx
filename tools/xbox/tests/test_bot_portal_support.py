"""Exercise production portal-support resolution, including clipped probe starts."""
from pathlib import Path
import shutil, subprocess, tempfile

s=(Path(__file__).resolve().parents[3]/'src/AI/sithBot.c').read_text()
a=s.index('static void sithBot_ResolvePortalRouteFloors(')
b=s.index('static flex_t sithBot_GetRouteStandingRise(',a)
code=r'''
#include <assert.h>
#include <math.h>
#define SITHBOT_NODE_PORTAL 4
#define SITHCOLLISION_WORLD 1
#define RAYCAST_1 1
#define RAYCAST_2 2
#define RAYCAST_2000 0x2000
typedef float flex_t;
typedef struct {float x,y,z;} rdVector3;
typedef struct {int valid;} sithSector;
typedef struct {rdVector3 pos;sithSector *sector;int kind;float routeFloorZ;int routeFloorKnown;} SithBotNode;
typedef struct {int hitType;void *surface;float distance;} sithCollisionSearchEntry;
SithBotNode sithBot_nodes[2];int sithBot_numNodes=2;
rdVector3 rdroid_zVector3={0,0,1};
static int queries,closed,enumerated,walkable=1,clipped;
static float floorZ=.50f;static sithCollisionSearchEntry hit;
void rdVector_Copy3(rdVector3 *a,const rdVector3 *b){*a=*b;}
void rdVector_Neg3(rdVector3 *a,const rdVector3 *b){a->x=-b->x;a->y=-b->y;a->z=-b->z;}
sithSector *sithCollision_GetSectorLookAt(sithSector *s,const rdVector3 *a,rdVector3 *b,float r) {
 if(clipped)b->z=a->z+.02f;return s;
}
int sithBot_IsNavSectorUsableForBot(sithSector *s){return s&&s->valid;}
void sithCollision_SearchRadiusForThings(sithSector *s,void *t,rdVector3 *p,rdVector3 *d,float dist,float radius,int flags) {
 assert(radius==0 && d->z==-1 && (flags&RAYCAST_1));
 ++queries;enumerated=0;hit.distance=p->z-floorZ;hit.hitType=SITHCOLLISION_WORLD;
}
sithCollisionSearchEntry *sithCollision_NextSearchResult(void){return enumerated++<2?&hit:0;}
int sithBot_IsSurfaceWalkableForBot(void *s){return walkable;}
void sithCollision_SearchClose(void){++closed;}
#define sithBot_Logf(...) ((void)0)
BODY
int main(void) {
 sithSector sector={1};
 sithBot_nodes[0]=(SithBotNode){{2,-.26,.59},&sector,4,0,0};
 sithBot_nodes[1]=(SithBotNode){{0,0,.12},&sector,0,123,1};
 sithBot_ResolvePortalRouteFloors();
 assert(sithBot_nodes[0].routeFloorKnown && fabsf(sithBot_nodes[0].routeFloorZ-.50f)<1e-6);
 assert(!sithBot_nodes[1].routeFloorKnown && queries==1 && closed==1);
 clipped=1;floorZ=.1094f;sithBot_nodes[0].pos.z=.2597f;
 sithBot_ResolvePortalRouteFloors();
 assert(fabsf(sithBot_nodes[0].routeFloorZ-floorZ)<1e-6);
 walkable=0;sithBot_ResolvePortalRouteFloors();
 assert(!sithBot_nodes[0].routeFloorKnown && enumerated==1);
 sector.valid=0;sithBot_ResolvePortalRouteFloors();
 assert(!sithBot_nodes[0].routeFloorKnown && queries==3 && closed==3);
}
'''.replace('BODY',s[a:b])
with tempfile.TemporaryDirectory() as d:
    c=Path(d)/'test.c';e=Path(d)/'test.exe';c.write_text(code)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(c),'-o',str(e)],check=True)
    subprocess.run([str(e)],check=True)
print('PASS: static portal support uses actual floor and clipped start; unsafe support stays rejected')
