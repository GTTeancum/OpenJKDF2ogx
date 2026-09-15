"""Exercise production lift standing-plane correction and reject remote contacts."""
from pathlib import Path
import subprocess,shutil,tempfile
s=(Path(__file__).resolve().parents[3]/'src/AI/sithBot.c').read_text()
a=s.index('        rdMatrix_TransformVector34(&surfacePoint,',s.index('static int sithBot_TryAttachToPathLift'))
b=s.index('        sithThing_LandThing(',a)
body=s[a:b]
code=r'''
#include <assert.h>
#include <math.h>
typedef float flex_t;
typedef struct {float x,y,z;} rdVector3;
typedef struct {int id;} sithSector;
typedef struct {rdVector3 position; int lookOrientation; float moveSize,height; sithSector *sector; int thingIdx;} sithThing;
struct Face {int *vertexPosIdx;};
struct Mesh {rdVector3 *vertices;};
struct Entry {struct Face *face;struct Mesh *sender;};
int Main_bAutostart=0,Main_botLiftProbe=0;
#define sithBot_Logf(...) ((void)0)
void rdMatrix_TransformVector34(rdVector3 *o,rdVector3 *i,int *m){*o=*i;}
void rdVector_Add3Acc(rdVector3 *o,rdVector3 *i){o->x+=i->x;o->y+=i->y;o->z+=i->z;}
void rdVector_Sub3(rdVector3 *o,rdVector3 *a,rdVector3 *b){o->x=a->x-b->x;o->y=a->y-b->y;o->z=a->z-b->z;}
float rdVector_Dot3(rdVector3 *a,rdVector3 *b){return a->x*b->x+a->y*b->y+a->z*b->z;}
void rdVector_Copy3(rdVector3 *a,rdVector3 *b){*a=*b;}
float sithPhysics_ThingGetInsertOffsetZ(sithThing *t){return t->height;}
int clipped=0;
sithSector *sithCollision_GetSectorLookAt(sithSector *s,rdVector3 *a,rdVector3 *b,float r){if(clipped)b->z=a->z;return s;}
int sithBot_IsNavSectorUsableForBot(sithSector *s){return s && s->id;}
void sithThing_MoveToSector(sithThing *t,sithSector *s,int n){t->sector=s;}
int place(sithThing *thing,sithThing *lift,float normalZ,float faceZ) {
 rdVector3 surfacePoint,standingPos,worldNormal={0,0,normalZ};float correction;
 sithSector *standingSector;int idx=0;
 rdVector3 vertices[1]={{0,0,faceZ}};struct Face face={&idx};struct Mesh mesh={vertices};
 struct Entry ent={&face,&mesh},*entry=&ent;
 for(int once=0;once<1;once++) {
 BODY
 return 1;
 }
 return 0;
}
int main(void){
 sithSector sector={1};sithThing lift={{0,0,.5},0,.2,0,&sector,48};
 sithThing bot={{0,0,.59},0,.05,.12,&sector,1};
 assert(place(&bot,&lift,1,.1));assert(fabsf(bot.position.z-.72f)<1e-6);
 assert(place(&bot,&lift,1,.1));assert(fabsf(bot.position.z-.72f)<1e-6);
 bot.position.z=1.0;assert(!place(&bot,&lift,1,.1));assert(bot.position.z==1.0);
 bot.position.z=.1;assert(!place(&bot,&lift,1,.1));assert(bot.position.z==.1f);
 bot.position.z=.59;sector.id=0;assert(!place(&bot,&lift,1,.1));assert(bot.position.z==.59f);
 sector.id=1;clipped=1;assert(!place(&bot,&lift,1,.1));assert(bot.position.z==.59f);
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory() as temp:
 c,exe=Path(temp)/'test.c',Path(temp)/'test.exe';c.write_text(code)
 subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: lift contact uses top plane plus standing clearance; remote/invalid contact leaves position unchanged')
