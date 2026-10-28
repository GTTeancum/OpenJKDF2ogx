"""Optional pickups must not lead into a one-way navigation sink."""
from pathlib import Path
import shutil
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[3] / 'src/AI/sithBot.c').read_text()
start = source.index('static void sithBot_MarkReachableNodes(')
end = source.index('static int sithBot_CanSkipRouteNode(', start)
code = r'''
#include <stdio.h>
#include <string.h>
#define SITHBOT_MAX_NODES 8
#define SITHBOT_NODE_ITEM 1
typedef struct {int hazard;} sithSector;
typedef struct {int kind,edgeCount,edges[8];sithSector *sector;} Node;
static Node sithBot_nodes[8];
static int sithBot_numNodes=8;
static int sithBot_IsDynamicHazardSector(sithSector *sector) {return sector && sector->hazard;}
#define CHECK(x) do {if (!(x)) {fprintf(stderr,"failure at %d\n",__LINE__);return 1;}} while(0)
BODY
int main(void) {
 unsigned char reachable[8];
 /* Main route 0 <-> 1, safe pickup 2, one-way drop 1 -> 3 -> pickup 4. */
 sithBot_nodes[0]=(Node){0,1,{1},0};
 sithBot_nodes[1]=(Node){0,3,{0,2,3},0};
 sithBot_nodes[2]=(Node){1,1,{1},0};
 sithBot_nodes[3]=(Node){0,1,{4},0};
 sithBot_nodes[4]=(Node){1,1,{3},0};
 sithBot_MarkReachableNodes(0,0,reachable);
 CHECK(reachable[2] && reachable[4]); /* Both pickups are reachable inbound. */
 CHECK(sithBot_IsOptionalPickupReturnable(0,2));
 CHECK(!sithBot_IsOptionalPickupReturnable(0,4));
 /* A separate safe return corridor makes the one-way drop usable. */
 sithBot_nodes[3].edges[sithBot_nodes[3].edgeCount++]=5;
 sithBot_nodes[5]=(Node){0,1,{0},0};
 CHECK(sithBot_IsOptionalPickupReturnable(0,4));
 /* Do not invent a route through another terminal pickup or a live hazard. */
 sithBot_nodes[5].kind=1;
 CHECK(!sithBot_IsOptionalPickupReturnable(0,4));
 sithBot_nodes[5].kind=0;
 sithSector hazard={1}; sithBot_nodes[5].sector=&hazard;
 CHECK(!sithBot_IsOptionalPickupReturnable(0,4));
 CHECK(!sithBot_IsOptionalPickupReturnable(-1,2));
 CHECK(!sithBot_IsOptionalPickupReturnable(0,8));
 CHECK(sithBot_IsOptionalPickupReturnable(2,2));
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
print('PASS: safe pickups and alternate exits accepted; one-way sinks and hazardous returns rejected')
