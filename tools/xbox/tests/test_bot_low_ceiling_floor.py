"""Production floor queries must start below a ceiling that clips the lift."""
from pathlib import Path
import shutil
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[3] / 'src/AI/sithBot.c').read_text()
a = source.index('static int sithBot_PositionHasWalkableFloorWithRiseImpl(')
b = source.index('static int sithBot_PositionHasWalkableFloorWithRise(', a)
c = source.index('static int sithBot_GetWalkableFloorDrop(')
d = source.index('static int sithBot_PositionHasWalkableFootprint(', c)
code = r'''
#include <math.h>
#include <stdio.h>
#define SITHCOLLISION_WORLD 1
#define SITHCOLLISION_THING 2
#define SITH_SURFACE_FLOOR 1
#define SITH_TF_STANDABLE 1
#define RAYCAST_2000 0x2000
#define RAYCAST_2 2
typedef double flex_t;
typedef struct {double x,y,z;} rdVector3;
typedef struct {int safe;} sithSector;
typedef struct {double moveSize;int thingflags;} sithThing;
typedef struct {int surfaceFlags;} sithSurface;
typedef struct {int hitType; sithSurface *surface;sithThing *receiver;double distance;} sithCollisionSearchEntry;
rdVector3 rdroid_zVector3={0,0,1};
static double ceiling=.35,floorZ=0,lastStart,lastDist;
static int pending,walkable=1,hazard,closed;
static sithSurface floorSurface={SITH_SURFACE_FLOOR};
static sithCollisionSearchEntry hit={SITHCOLLISION_WORLD,&floorSurface,0,0};
void rdVector_Copy3(rdVector3 *a,const rdVector3 *b){*a=*b;}
void rdVector_Neg3(rdVector3 *a,const rdVector3 *b){a->x=-b->x;a->y=-b->y;a->z=-b->z;}
int sithBot_IsSectorSafeForBot(sithSector *s){return s&&s->safe;}
int sithBot_HasControlledHazardBelow(sithSector *s,const rdVector3 *p){return hazard;}
double sithPhysics_ThingGetInsertOffsetZ(sithThing *t){return .12;}
int sithBot_IsSurfaceWalkableForBot(sithSurface *s){return walkable;}
sithSector *sithCollision_GetSectorLookAt(sithSector *s,const rdVector3 *a,rdVector3 *end,double radius){
 if(end->z>ceiling)end->z=ceiling-radius;return s;
}
void sithCollision_SearchRadiusForThings(sithSector *s,sithThing *t,rdVector3 *p,rdVector3 *v,double distance,double radius,int flags){
 lastStart=p->z;lastDist=distance;hit.distance=p->z-floorZ;
 pending=p->z+radius<=ceiling+1e-9&&hit.distance>=0&&hit.distance<=distance;
}
sithCollisionSearchEntry *sithCollision_NextSearchResult(void){if(!pending)return 0;pending=0;return &hit;}
void sithCollision_SearchClose(void){closed++;}
#define CHECK(x) do {if (!(x)) {fprintf(stderr,"failure at %d\n",__LINE__);return 1;}} while(0)
BODY
int main(void){
 sithSector sector={1};sithThing actor={.065,0};rdVector3 pos={0,0,.12};double drop=-1;
 /* c1 doorway ceiling .35: requested floor-query origin .47 is outside it. */
 CHECK(sithBot_PositionHasWalkableFloorWithRiseImpl(&actor,&sector,&pos,.35));
 CHECK(fabs(lastStart-.32)<1e-9 && fabs(lastDist-(.85+.20))<1e-9);
 CHECK(sithBot_GetWalkableFloorDrop(&actor,&sector,&pos,&drop));
 CHECK(fabs(lastStart-.32)<1e-9 && fabs(drop-.12)<1e-9);
 CHECK(fabs(lastDist-(1.20+.20))<1e-9);
 /* An open ceiling preserves the original origin and depth budget. */
 ceiling=2;
 CHECK(sithBot_PositionHasWalkableFloorWithRiseImpl(&actor,&sector,&pos,.35));
 CHECK(fabs(lastStart-.47)<1e-9 && fabs(lastDist-1.20)<1e-9);
 CHECK(sithBot_GetWalkableFloorDrop(&actor,&sector,&pos,&drop));
 CHECK(fabs(lastStart-.37)<1e-9 && fabs(lastDist-1.45)<1e-9 && fabs(drop-.12)<1e-9);
 /* Reject hazards, unsafe floors and floors outside the original drop budget. */
 hazard=1;CHECK(!sithBot_PositionHasWalkableFloorWithRiseImpl(&actor,&sector,&pos,.35));hazard=0;
 walkable=0;CHECK(!sithBot_PositionHasWalkableFloorWithRiseImpl(&actor,&sector,&pos,.35));
 CHECK(!sithBot_GetWalkableFloorDrop(&actor,&sector,&pos,&drop));walkable=1;
 floorZ=-2;CHECK(!sithBot_PositionHasWalkableFloorWithRiseImpl(&actor,&sector,&pos,.35));
 CHECK(!sithBot_GetWalkableFloorDrop(&actor,&sector,&pos,&drop));
 CHECK(closed==8);
 return 0;
}
'''.replace('BODY', source[a:b] + source[c:d])
with tempfile.TemporaryDirectory() as directory:
    c = Path(directory) / 'test.c'
    executable = Path(directory) / 'test.exe'
    c.write_text(code)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',
                    str(c), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print('PASS: low-ceiling queries use clipped origins and preserve physical floor/drop limits')
