"""Exercise the production saber light policy, including stock compatibility."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'src/Engine/sithRender.c').read_text()
start = source.index('static int sithRender_AddSaberLight(')
end = source.index('\nvoid sithRender_RenderDebugLight', start)
code = r'''
#include <assert.h>
#include <math.h>
#define JKM_LIGHTING
#define TARGET_XBOX
#define QOL_IMPROVEMENTS
#define SITHRENDER_SABER_LIGHT_MAX .42
#define SITHRENDER_MAX_SABER_LIGHTS 2
#define SITHRENDER_LIGHT_MIN_VISIBLE .08
#define SITHRENDER_MODERN_SABER_LIGHT_INTENSITY .68
#define SITHRENDER_MODERN_SABER_LIGHT_REACH .95
typedef float flex_t;
typedef struct { float x,y,z; } rdVector3;
typedef struct { rdVector3 rvec,lvec,uvec,scale; } rdMatrix34;
typedef struct { float length; } rdPolyLine;
typedef struct jkPlayerInfo { int bHasLastJointMat; rdMatrix34 lastSaberJointMat; rdPolyLine polyline; } jkPlayerInfo;
typedef struct { float intensity, falloffMin, falloffMax; unsigned color, id; } rdLight;
typedef struct { struct { rdVector3 saberBladePos; float timeLeftLengthChange; unsigned field_1A8; } actorParams; jkPlayerInfo *playerInfo; } sithThing;
static int jkPlayer_bModernSaber, sithRender_saberLightsThisFrame, calls;
static void *rdCamera_pCurCamera;
static rdVector3 addedPositions[8];
static void rdVector_Copy3(rdVector3 *dst,const rdVector3 *src){*dst=*src;}
static void rdVector_MultAcc3(rdVector3 *dst,const rdVector3 *vec,float scale){dst->x+=vec->x*scale;dst->y+=vec->y*scale;dst->z+=vec->z*scale;}
static void sithRender_SetDynamicLightColor(rdLight *l,unsigned c){l->color=c;}
static int rdCamera_AddLight(void *cam,rdLight *l,rdVector3 *p){addedPositions[calls]=*p;calls++;l->falloffMin=l->intensity/.4f;l->falloffMax=l->intensity/.8f;return 1;}
'''+source[start:end]+r'''
int main(void) {
 rdLight l; rdVector3 p={1,2,3}; jkPlayerInfo info; sithThing t;
 t.actorParams.saberBladePos = p; t.actorParams.timeLeftLengthChange = .42f; t.actorParams.field_1A8 = 0x4070ff; t.playerInfo = 0;
 l.intensity=.42f; assert(sithRender_AddSaberLight(&t,&l,&p));
 assert(fabs(l.intensity-.42)<.00001 && l.color==0x4070ff && addedPositions[0].x==1);
 l.intensity=.42f; assert(sithRender_AddSaberLight(&t,&l,&p));
 l.intensity=.42f; assert(!sithRender_AddSaberLight(&t,&l,&p));
 jkPlayer_bModernSaber=1; sithRender_saberLightsThisFrame=0;
 for(int i=0;i<4;i++){l.intensity=.42f;assert(sithRender_AddSaberLight(&t,&l,&p));assert(fabs(l.intensity-.68)<.00001);assert(fabs(l.falloffMin-.95)<.00001);assert(fabs(l.falloffMax-.95)<.00001);}
 l.intensity=.42f;assert(!sithRender_AddSaberLight(&t,&l,&p));
 sithRender_saberLightsThisFrame=0;
 l.intensity=.21f;assert(sithRender_AddSaberLight(&t,&l,&p));assert(fabs(l.intensity-.34)<.00001);
 l.intensity=0;assert(!sithRender_AddSaberLight(&t,&l,&p));
 l.intensity=10;assert(sithRender_AddSaberLight(&t,&l,&p));assert(fabs(l.intensity-.68)<.00001);
 assert(.68f/.4f > 1.0f && fabs(l.falloffMin-.95f)<.00001f);

 calls=0; sithRender_saberLightsThisFrame=0;
 info.bHasLastJointMat=1; info.polyline.length=1.0f;
 info.lastSaberJointMat.scale=(rdVector3){10,20,30};
 info.lastSaberJointMat.lvec=(rdVector3){0,1,0};
 t.playerInfo=&info;
 rdLight lights[4];
 int next=sithRender_AddSaberLights(&t,lights,0,4);
 assert(next==3 && calls==3);
 assert(fabs(addedPositions[0].y-20.18f)<.00001f);
 assert(fabs(addedPositions[1].y-20.50f)<.00001f);
 assert(fabs(addedPositions[2].y-20.82f)<.00001f);
 assert(lights[0].color==0x4070ff && lights[1].color==0x4070ff && lights[2].color==0x4070ff);
 next=sithRender_AddSaberLights(&t,lights,4,4);
 assert(next==4);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path/'test.c').write_text(code)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',
                    str(path/'test.c'), '-o', str(path/'test.exe')], check=True)
    subprocess.run([str(path/'test.exe')], check=True)
print('PASS: stock light unchanged; modern short reach, hue, four-light cap, ignition/retraction ramp and blade-line samples')
