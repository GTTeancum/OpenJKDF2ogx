"""Exercise exact-key floor caching and per-lookup lifetime in production helpers."""
from pathlib import Path
import subprocess, shutil, tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/AI/sithBot.c').read_text()
a=s.index('typedef struct SithBotFloorQuery');b=s.index('static int sithBot_PositionHasWalkableFloorWithRiseImpl',a)
c=s.index('static int sithBot_PositionHasWalkableFloorWithRise(');d=s.index('\n}',c)+2
e=s.index('static int sithBot_FindNearestNodeAt(');f=s.index('\n}',e)+2
code=r'''
#include <assert.h>
#include <stdint.h>
typedef double flex_t;
typedef struct {double x,y,z;} rdVector3;
typedef struct {int id;} sithThing;
typedef struct {int id;} sithSector;
int Main_botProfile=0, queries=0, answer=1;
uint64_t sithBot_profileUs[4];
uint64_t sithBot_ProfileTimeUs(void) {return 0;}
HELPERS
int sithBot_PositionHasWalkableFloorWithRiseImpl(sithThing *t,sithSector *s,const rdVector3 *p,flex_t h) {++queries;return answer;}
FLOOR
int sithBot_FindNearestNodeAtImpl(sithSector *s,const rdVector3 *p) {
 int a=sithBot_PositionHasWalkableFloorWithRise(0,s,p,.35);
 int b=sithBot_PositionHasWalkableFloorWithRise(0,s,p,.35);
 assert(a==b); return a;
}
NEAREST
int main(void) {
 sithSector s={1},other={2}; rdVector3 p={1,2,3},p2={1,2,3.001};
 SithBotFloorQueryCache cache; cache.count=0;
 assert(sithBot_FindNearestNodeAt(&s,&p)==1 && queries==1);
 assert(sithBot_floorQueryCache==0);
 answer=0;
 assert(sithBot_FindNearestNodeAt(&s,&p)==0 && queries==2);
 assert(sithBot_floorQueryCache==0);
 sithBot_floorQueryCache=&cache;
 sithBot_StoreFloorQuery(0,&s,&p,.35,0);
 assert(sithBot_CachedFloorQuery(0,&s,&p,.35)==0);
 assert(sithBot_CachedFloorQuery(0,&other,&p,.35)==-1);
 assert(sithBot_CachedFloorQuery(0,&s,&p2,.35)==-1);
 assert(sithBot_CachedFloorQuery(0,&s,&p,.36)==-1);
 assert(sithBot_CachedFloorQuery((sithThing*)&other,&s,&p,.35)==-1);
 for(int i=0;i<40;i++) {p2.x=i;sithBot_StoreFloorQuery(0,&s,&p2,.35,1);}
 assert(cache.count==16);
 assert(sithBot_CachedFloorQuery(0,&s,&p,.35)==0);
 sithBot_FindNearestNodeAt(&s,&p);
 assert(sithBot_floorQueryCache==&cache);
}
'''.replace('HELPERS',s[a:b]).replace('FLOOR',s[c:d]).replace('NEAREST',s[e:f])
with tempfile.TemporaryDirectory() as td:
 c=Path(td)/'test.c';e=Path(td)/'test.exe';c.write_text(code)
 subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(c),'-o',str(e)],check=True)
 subprocess.run([str(e)],check=True)
print('PASS: floor cache keys, negative results, capacity, nested context, and query lifetime')
