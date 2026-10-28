"""Check production route pricing for raised portals and ordinary edges."""
from pathlib import Path
import shutil
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[3] / 'src/AI/sithBot.c').read_text()
start = source.index('static flex_t sithBot_GetRouteEdgeCost(')
end = source.index('static int sithBot_FindPathNextWeightedImpl(', start)
code = r'''
#include <math.h>
#include <stdio.h>
#define SITHBOT_NODE_SPAWN 0
#define SITHBOT_NODE_FLOOR 2
#define SITHBOT_NODE_PORTAL 4
#define SITHBOT_NODE_JUMPPAD 5
typedef double flex_t;
typedef struct {double x,y,z;} V;
typedef struct {V pos; int kind,routeFloorKnown; double routeFloorZ;} Node;
static Node sithBot_nodes[3];
double rdVector_Dist3(const V *a,const V *b) {
 return sqrt((a->x-b->x)*(a->x-b->x)+(a->y-b->y)*(a->y-b->y)+(a->z-b->z)*(a->z-b->z));
}
#define CHECK(x) do {if (!(x)) {fprintf(stderr,"failure at %d\n",__LINE__);return 1;}} while(0)
BODY
int main(void) {
 /* c1 ramp-side 307 -> upper-corridor 308. Equal opening Z is not level support. */
 sithBot_nodes[0]=(Node){{2.96,0,.54},4,1,.06};
 sithBot_nodes[1]=(Node){{2.84,0,.54},4,1,.50};
 CHECK(fabs(sithBot_GetRouteEdgeCost(0,1)-(.12+.44*2.25+1.10))<1e-9);
 CHECK(fabs(sithBot_GetRouteEdgeCost(1,0)-.12)<1e-9);
 /* Prefer a 1.2-unit level detour over that unnecessary climb. */
 CHECK(sithBot_GetRouteEdgeCost(0,1)>1.2);
 /* A high opening over level support must not acquire a fictitious climb. */
 sithBot_nodes[0]=(Node){{0,0,.04},2,0,0};
 sithBot_nodes[1]=(Node){{1,0,.54},4,1,0};
 CHECK(fabs(sithBot_GetRouteEdgeCost(0,1)-sqrt(1.25))<1e-9);
 /* Unknown support, ordinary nodes and jump pads retain existing pricing. */
 for(int kind=0;kind<6;kind++) for(int n=-40;n<=40;n++) {
   double rise=n*.031, distance=sqrt(1+rise*rise), expected=distance;
   sithBot_nodes[0]=(Node){{0,0,.04},kind,0,0};
   sithBot_nodes[1]=(Node){{1,0,.04+rise},kind,0,0};
   if(rise>.20 && kind!=SITHBOT_NODE_JUMPPAD) {
     expected+=rise*2.25; if(rise>.30) expected+=1.10;
   }
   if(kind==SITHBOT_NODE_SPAWN) expected+=.12;
   CHECK(fabs(sithBot_GetRouteEdgeCost(0,1)-expected)<1e-9);
 }
 return 0;
}
'''.replace('BODY', source[start:end])
with tempfile.TemporaryDirectory() as directory:
    c = Path(directory) / 'test.c'
    executable = Path(directory) / 'test.exe'
    c.write_text(code)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',
                    str(c), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print('PASS: portal support prices real climbs; 486 ordinary/unknown-support cases unchanged')
