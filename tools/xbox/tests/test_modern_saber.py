"""Exercise the production saber geometry/history and near-plane RGBA clipping."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
code = r'''
#include <assert.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "Platform/Xbox/xbox_vertex_submit.h"
#define QOL_IMPROVEMENTS
#define SDL2_RENDER
#define TARGET_XBOX
#define JKSABER_UNIT_TEST
#define RD_GEOMODE_SOLIDCOLOR 3
#define RD_LIGHTMODE_FULLYLIT 0
#define RD_FF_DOUBLE_SIDED 1
#define RD_FF_TEX_TRANSLUCENT 2
#define RD_FF_ZWRITE_DISABLED 32
#define JKFLAG_SABERDAMAGE 1
#define JKFLAG_SABERRETRACT 8
#define SITH_AF_BOSS 0x200
#define SITH_TF_DEAD 0x200
#define SITH_THING_CORPSE 5
typedef struct {float x,y,z;} rdVector3;
typedef struct {rdVector3 scale; float angle;} rdMatrix34;
typedef int rdMaterial;
typedef struct {float length,baseRadius; struct {rdMaterial *material;} edgeFace;} Line;
typedef struct {Line *polyline;} rdThing;
typedef struct {unsigned numMarkers; struct {float marker_float[8];int marker_int[8];} markers;} rdKeyframe;
typedef struct {int status;rdKeyframe *keyframe;float field_120;} rdPuppetTrack;
typedef struct {rdPuppetTrack tracks[4];} rdPuppet;
typedef struct {unsigned thingIdx,signature,jkFlags; struct {uint32_t field_1A8,typeflags;} actorParams;unsigned thingflags,type;struct {rdPuppet *puppet;} rdthing;} sithThing;
typedef struct {int geometryMode,lightingMode,textureMode,wallCel,type,light_flags,hasEffectColors;
float extralight,ambientLight,light_level_static;rdMaterial *material;rdVector3 vertices[4];uint32_t effectColors[4];} rdProcEntry;
typedef struct {float zNear;} Frustum;
typedef struct {Frustum *pClipFrustum;rdMatrix34 view_matrix;void (*fnProjectLst)(rdVector3*,rdVector3*,int);} Camera;
static void project(rdVector3 *d,rdVector3 *s,int n){memcpy(d,s,n*sizeof(*d));}
static Frustum frustum={.01f};
static Camera camera={&frustum,{{0,0,0},0},project};
static Camera *rdCamera_pCurCamera=&camera;
static unsigned sithTime_curMs;
static int playerThingIdx,jkPlayer_bModernSaber,stockCalls,count;
static rdProcEntry faces[512];
static rdProcEntry *rdCache_GetProcEntry(void){assert(count<512);return &faces[count];}
static void rdCache_AddProcFace(int unused,int n,int flags){int i;assert(n>=3&&n<=4);for(i=0;i<n;i++){assert(isfinite(faces[count].vertices[i].x));assert(faces[count].vertices[i].y>=frustum.zNear);}count++;}
static void rdThing_Draw(rdThing *t,rdMatrix34 *m){stockCalls++;}
static float rdVector_Len3(const rdVector3 *v){return sqrtf(v->x*v->x+v->y*v->y+v->z*v->z);}
static void rdVector_Sub3(rdVector3 *o,const rdVector3*a,const rdVector3*b){o->x=a->x-b->x;o->y=a->y-b->y;o->z=a->z-b->z;}
static float rdVector_Dist3(const rdVector3*a,const rdVector3*b){rdVector3 v;rdVector_Sub3(&v,a,b);return rdVector_Len3(&v);}
static void rdMatrix_TransformPoint34(rdVector3 *d,const rdVector3*s,const rdMatrix34*m){d->x=m->scale.x+s->x*cosf(m->angle)-s->y*sinf(m->angle);d->y=m->scale.y+s->x*sinf(m->angle)+s->y*cosf(m->angle);d->z=m->scale.z+s->z;}
#include "Gameplay/jkSaberModern.inc"
#include "Gameplay/jkSaberLegacy.h"
enum {GL_ALPHA_TEST,GL_BLEND,GL_GREATER,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA};
static int srcBlend,dstBlend,alphaTest;
static void glEnable(int v){if(v==GL_ALPHA_TEST)alphaTest=1;}
static void glDisable(int v){if(v==GL_ALPHA_TEST)alphaTest=0;}
static void glAlphaFunc(int f,float a){}
static void glBlendFunc(int a,int b){srcBlend=a;dstBlend=b;}
BLEND_SOURCE
int main(void){
 assert(jkSaber_IsLegacyGlowFace("SABFAN-G1.3DO","mat/DGREEN.MAT"));
 assert(jkSaber_IsLegacyGlowFace("sabv-b.3do","white.mat"));
 assert(!jkSaber_IsLegacyGlowFace("sabg-g.3do","kysaber.mat"));
 assert(!jkSaber_IsLegacyGlowFace("sabg-g.3do","kyhand.mat"));
 assert(!jkSaber_IsLegacyGlowFace("wall.3do","white.mat"));
 assert(!jkSaber_IsLegacyGlowFace("sabg.3do","white.mat"));
 {unsigned a;for(a=0;a<256;a++) assert(fabsf(xboxVertex_EffectAlpha(a<<24,1)-a/255.f)<.00001f);}
 assert(xboxVertex_EffectAlpha(0,0)==1.f);
 std3D_XboxApplyFaceBlend(0x200000|0x200,0);
 assert(!alphaTest&&srcBlend==GL_SRC_ALPHA&&dstBlend==GL_ONE_MINUS_SRC_ALPHA);
 std3D_XboxApplyFaceBlend(0x200,1);
 assert(alphaTest&&srcBlend==GL_ONE_MINUS_SRC_ALPHA&&dstBlend==GL_SRC_ALPHA);
 rdMaterial mat=1;Line line={.12f,.0032f,{&mat}};rdThing t={&line};
 sithThing actor={1,1,0,{0x4070ff}};rdMatrix34 m={{0,1,0},0};
 jkSaber_DrawBlade(&actor,&t,&m,0,0);assert(stockCalls==1&&count==0);
 jkPlayer_bModernSaber=1;jkSaber_DrawBlade(&actor,&t,&m,0,0);assert(count==90);
 count=0;actor.jkFlags=JKFLAG_SABERDAMAGE;sithTime_curMs=1;jkSaber_DrawBlade(&actor,&t,&m,0,0);assert(count==90);
 count=0;sithTime_curMs=17;m.angle=1.0f;jkSaber_DrawBlade(&actor,&t,&m,0,0);assert(count==113);
 {int i,j,white=0,feather=0;for(i=0;i<count;i++)for(j=0;j<3;j++){
 uint32_t c=faces[i].effectColors[j];if(c==0xffffffff)white=1;
 if((c>>24)>0&&(c>>24)<255){assert((c&255)>((c>>16)&255));feather=1;}}
 assert(white&&feather);}
 count=0;sithTime_curMs=33;jkSaber_DrawBlade(&actor,&t,&m,0,0);assert(count==113);
 count=0;sithTime_curMs=51;jkSaber_DrawBlade(&actor,&t,&m,0,0);assert(count==50);
 /* The same actor rendered in another local view starts its own history. */
 count=0;playerThingIdx=1;m.angle=1.1f;jkSaber_DrawBlade(&actor,&t,&m,0,0);assert(count==50);
 count=0;playerThingIdx=0;m.angle=1.15f;jkSaber_DrawBlade(&actor,&t,&m,0,0);assert(count==113);
 count=0;m.angle=1.2f;jkSaber_DrawBlade(&actor,&t,&m,1,0);assert(count==50);
 count=0;actor.jkFlags=0;jkSaber_DrawBlade(&actor,&t,&m,0,0);assert(count==50);
 count=0;actor.jkFlags=1;jkSaber_DrawBlade(&actor,&t,&m,0,0);assert(count==50);
 count=0;{rdVector3 a={0,-1,0},b={1,1,0},c={-1,1,0};jkModern_Triangle(&a,&b,&c,0,0xffffffff,0xffffffff);assert(count==1);assert((faces[0].effectColors[0]>>24)>0);}
 count=0;{rdVector3 a={0,-1,0};jkModern_Triangle(&a,&a,&a,0,0,0);assert(count==0);}
 /* Rounded cap: semicircle points stay beyond the tip, with concentric
  * white core/colored feather rings and a transparent outer boundary. */
 {rdVector3 tip={0,1,0};int i,j,core=0,feather=0,edge=0;float furthest=0;
  count=0;jkModern_Tip(&tip,1,1,0,0,1,0x4070ff,0);assert(count==40);
  for(i=0;i<count;i++)for(j=0;j<3;j++){
   rdVector3 p=faces[i].vertices[j];uint32_t c=faces[i].effectColors[j];
   float radius=sqrtf(p.x*p.x+p.z*p.z);assert(p.z>=0&&radius<=1.75001f);
   if(p.z>furthest)furthest=p.z;
   if(c==0xffffffff){assert(radius<=.23001f);core++;}
   else if(c>>24){assert(fabsf(radius-.9f)<.00001f);feather++;}
   else {assert(fabsf(radius-1.75f)<.00001f);edge++;}
  }
  assert(core&&feather&&edge&&fabsf(furthest-1.75f)<.00001f);
  count=0;jkModern_Tip(&tip,1,1,0,0,1,0x4070ff,1);assert(count==80);
  assert(faces[60].vertices[1].z<0);
  tip.y=-1;count=0;jkModern_Tip(&tip,1,1,0,0,1,0x4070ff,0);assert(count==0);
 }
 /* Retail Yun attack1 markers (15 FPS): two separate damage windows. */
 {
  rdKeyframe key={5,{{6.5f,10,12,14.5f,17},{4,5,14,4,5}}};
  rdKeyframe *active=NULL;rdPuppet puppet={0};
  actor.actorParams.typeflags=SITH_AF_BOSS;actor.rdthing.puppet=&puppet;
  puppet.tracks[0].status=0x83;puppet.tracks[0].keyframe=&key;
  puppet.tracks[0].field_120=6.4f;assert(!jkModern_BossAttack(&actor,&active));
  puppet.tracks[0].field_120=6.5f;assert(jkModern_BossAttack(&actor,&active)==1&&active==&key);
  puppet.tracks[0].field_120=10;assert(!jkModern_BossAttack(&actor,&active));
  puppet.tracks[0].field_120=14.5f;assert(jkModern_BossAttack(&actor,&active)==4);
  puppet.tracks[0].status|=8;assert(!jkModern_BossAttack(&actor,&active));
  puppet.tracks[0].status=0x83;actor.thingflags=SITH_TF_DEAD;assert(!jkModern_BossAttack(&actor,&active));
  actor.thingflags=0;actor.jkFlags|=JKFLAG_SABERRETRACT;assert(!jkModern_BossAttack(&actor,&active));
  actor.jkFlags=JKFLAG_SABERDAMAGE;
  memset(jkModernHistories,0,sizeof(jkModernHistories));
  sithTime_curMs=1000;m.angle=0;count=0;jkSaber_DrawBlade(&actor,&t,&m,0,0);assert(count==90);
  sithTime_curMs+=16;m.angle=.06f;count=0;jkSaber_DrawBlade(&actor,&t,&m,0,0);assert(count==113);
  count=0;jkSaber_DrawBlade(&actor,&t,&m,1,0);assert(count==50); /* Boc slot isolation */
  sithTime_curMs+=16;m.angle=.12f;count=0;jkSaber_DrawBlade(&actor,&t,&m,1,0);assert(count==113);
  puppet.tracks[0].status=0;count=0;jkSaber_DrawBlade(&actor,&t,&m,0,0);assert(count==50); /* stale damage flag after interruption */
  puppet.tracks[0].status=0x83;count=0;jkSaber_DrawBlade(&actor,&t,&m,0,0);assert(count==50);
  actor.signature++;sithTime_curMs+=16;m.angle=.18f;count=0;jkSaber_DrawBlade(&actor,&t,&m,0,0);assert(count==50);
  sithTime_curMs+=101;m.angle=.24f;count=0;jkSaber_DrawBlade(&actor,&t,&m,0,0);assert(count==50);
 }
 /* Constant angular speed: the shutter stays ~50 ms wide at 30/60/120 FPS,
  * where a one-render-frame history would shrink by four times. */
 {int step;for(step=8;step<=32;step*=2){
  jkModernHistory h={0};rdVector3 b,p;unsigned now;
  for(now=0;now<=320;now+=step){b.x=sinf(now*.002f);b.y=cosf(now*.002f);b.z=0;p=jkModern_BossPrevious(&h,&b,now);}
  assert(rdVector_Dist3(&b,&p)>.095f&&rdVector_Dist3(&b,&p)<.105f);
 }}
 return 0;
}
'''
blend = (root/'src/Platform/Xbox/std3D.c').read_text()
a = blend.index('static void std3D_XboxApplyFaceBlend(')
b = blend.index('\n}', a) + 2
code = code.replace('BLEND_SOURCE', blend[a:b])
with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / 'test.c'
    exe = Path(directory) / 'test.exe'
    source.write_text(code)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe', str(source), '-I'+str(root/'src'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: stock fallback, blade/fan RGBA, short retention/expiry, separate views/blades, attack reset, near-plane clipping')
