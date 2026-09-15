"""Compare path-search block masks with the existing per-edge predicate."""
from pathlib import Path
import shutil,subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/AI/sithBot.c').read_text()
a=s.index('static int sithBot_IsRouteEdgeBlocked(');b=s.index('static void sithBot_BlockRouteEdge(',a)
code=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define SITHBOT_MAX_BLOCKED_EDGES 128
#define SITHBOT_BLOCK_SHARED_OWNER -2
#define N 64
#define E 24
typedef struct { int edgeCount,edges[E]; } SithBotNode;
typedef struct { int ownerSlot,fromNode,toNode; uint32_t untilMs; } SithBotBlockedEdge;
SithBotNode sithBot_nodes[N];
SithBotBlockedEdge sithBot_blockedEdges[128];
int sithBot_numNodes=N;
uint32_t sithTime_curMs;
SOURCE
static uint32_t seed=12345;
static uint32_t rng(void) { seed=seed*1664525u+1013904223u; return seed; }
int main(void) {
 uint32_t masks[N]; int trial,owner,n,e,i; unsigned checks=0;
 for(trial=0;trial<100;trial++) {
  sithTime_curMs=1000+trial;
  for(n=0;n<N;n++) {
   sithBot_nodes[n].edgeCount=E;
   for(e=0;e<E;e++) sithBot_nodes[n].edges[e]=rng()%N;
  }
  for(i=0;i<128;i++) {
   SithBotBlockedEdge *p=&sithBot_blockedEdges[i];
   p->ownerSlot=(int)(rng()%11)-2;
   p->fromNode=(int)(rng()%(N+2))-1;
   p->toNode=rng()%N;
   p->untilMs=sithTime_curMs+(int)(rng()%3)-1;
  }
  for(owner=-2;owner<9;owner++) {
   sithBot_BuildRouteBlockedMasks(owner,masks);
   for(n=0;n<N;n++) for(e=0;e<E;e++) {
    assert(!!(masks[n]&(1u<<e))==sithBot_IsRouteEdgeBlocked(owner,n,sithBot_nodes[n].edges[e]));
    checks++;
   }
  }
 }
 printf("PASS: %u edge decisions match, including expiry, shared/private owners and invalid records\n",checks);
}
'''.replace('SOURCE',s[a:b])
with tempfile.TemporaryDirectory() as td:
 c=Path(td)/'test.c';exe=Path(td)/'test.exe';c.write_text(code)
 subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe','-O2',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
