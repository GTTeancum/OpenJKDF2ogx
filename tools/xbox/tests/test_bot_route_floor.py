"""Regression: do not consume raised route nodes before landing on them."""
from pathlib import Path
import subprocess,tempfile,shutil
root=Path(__file__).resolve().parents[3];s=(root/'src/AI/sithBot.c').read_text()
a=s.index('static flex_t sithBot_GetRouteStandingRise(');b=s.index('static int sithBot_AddPortalApproachNodes',a)
code=r'''
#include <assert.h>
#include <math.h>
#define SITHBOT_NODE_FLOOR 2
#define SITHBOT_NODE_PORTAL 4
typedef double flex_t;
typedef struct { double x,y,z; } V;
typedef struct { V pos; int kind; void *sector; double routeFloorZ; int routeFloorKnown; } SithBotNode;
typedef struct { V position; void *sector; int attach_flags; } sithThing;
double sithPhysics_ThingGetInsertOffsetZ(sithThing *t) { return .12; }
double sithBot_AbsFlex(double x) { return fabs(x); }
double sithBot_GetRouteNodeReachRadius(const SithBotNode *n,double r) { return r; }
double sithBot_GetRouteNodeCrossRadius(const SithBotNode *n) { return .15; }
double sithBot_DistSq(const V *a,const V *b) { return (a->x-b->x)*(a->x-b->x)+(a->y-b->y)*(a->y-b->y)+(a->z-b->z)*(a->z-b->z); }
int sithBot_CanSeePosition(void *a,V *b,void *c,const V *d) { return 1; }
SOURCE
int main(void) {
 int lower,upper;
 SithBotNode node={{0,0,.19},2,&upper};
 sithThing actor={{0,0,.12},&lower,1};
 assert(fabs(sithBot_GetRouteStandingRise(&actor,&node)-.15)<1e-6);
 assert(!sithBot_IsRouteNodeReached(&actor,&node,.34));
 actor.sector=&upper; actor.position.z=.27;
 assert(sithBot_IsRouteNodeReached(&actor,&node,.34));
 node.kind=4; actor.position.z=.12;
 assert(!sithBot_IsRouteNodeReached(&actor,&node,.34));
 actor.position.z=.27;
 assert(sithBot_IsRouteNodeReached(&actor,&node,.34));
 node.pos.z=.04; actor.position.z=-.18;
 assert(fabs(sithBot_GetRouteStandingRise(&actor,&node)-.30)<1e-6);
 actor.position.z=.12;
 assert(fabs(sithBot_GetRouteStandingRise(&actor,&node))<1e-6);
 node.kind=0; node.pos.z=.12; actor.position.z=.12;
 assert(sithBot_IsRouteNodeReached(&actor,&node,.34));
 node.kind=4;node.pos.z=.59;node.routeFloorKnown=1;node.routeFloorZ=.50;actor.position.z=.62;
 assert(sithBot_IsRouteNodeReached(&actor,&node,.34));
 node.routeFloorZ=.70;
 assert(!sithBot_IsRouteNodeReached(&actor,&node,.34));
 node.routeFloorZ=.1094;node.pos.z=.2597;actor.position.z=.24;
 assert(sithBot_IsRouteNodeReached(&actor,&node,.34));
 node.routeFloorKnown=0;
 assert(!sithBot_IsRouteNodeReached(&actor,&node,.34));
}
'''.replace('SOURCE',s[a:b])
with tempfile.TemporaryDirectory() as td:
 c=Path(td)/'test.c';e=Path(td)/'test.exe';c.write_text(code)
 subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(c),'-o',str(e)],check=True)
 subprocess.run([str(e)],check=True)
print('PASS: raised floor/portal nodes require actor clearance; authored spawn nodes remain unchanged')
