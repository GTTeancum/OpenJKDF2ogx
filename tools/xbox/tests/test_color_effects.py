"""Run production effect submission with a software model of GL blending."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
compiler = shutil.which("clang") or r"C:\Program Files\LLVM\bin\clang.exe"
source = (ROOT / "src/Platform/Xbox/std3D.c").read_text()
functions = "static void xbox_draw_effect_quad" + source.split(
    "static void xbox_draw_effect_quad", 1
)[1].split("static void std3D_XboxTransformViewportUiRect", 1)[0]
program = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#define GL_TRIANGLES 4
#define GL_TEXTURE_2D 0x0DE1
#define GL_ONE 1
#define GL_ZERO 0
#define GL_SRC_COLOR 0x0300
#define GL_DST_COLOR 0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
static int g_initialized = 1, g_sceneOpen = 1, src, dst, draws, viewport;
static float pixel[3], color[3], tintValue[3], fadeValue = 1;
static int filterValue[3], addValue[3];
static void xbox_get_color_effects(float *t, int *f, float *fade, int *add) {
    int i; for(i=0;i<3;++i) { t[i]=tintValue[i]; f[i]=filterValue[i]; add[i]=addValue[i]; }
    *fade=fadeValue;
}
static void xbox_set_ui_state(int blend) { assert(blend); }
static void std3D_XboxApplyViewport(void) { ++viewport; }
static void glDisable(int flag) { assert(flag == GL_TEXTURE_2D); }
static void glBlendFunc(int s, int d) { src=s; dst=d; }
static void glBegin(int mode) { assert(mode == GL_TRIANGLES); }
static void glColor4f(float r,float g,float b,float a) {
    color[0]=r; color[1]=g; color[2]=b; assert(a==1);
}
static void glVertex3f(float x,float y,float z) { assert(z==0); }
static void glEnd(void) {
    int i; ++draws;
    for(i=0;i<3;++i) {
        float result;
        if(src==0 && dst==0x0300) result=pixel[i]*color[i];
        else if(src==GL_ONE && dst==GL_ONE) result=pixel[i]+color[i];
        else if(src==GL_ONE_MINUS_DST_COLOR && dst==GL_ZERO) result=(1-pixel[i])*color[i];
        else { assert(src==0x0306 && dst==1); result=pixel[i]*(1+color[i]); }
        pixel[i]=fminf(1,fmaxf(0,result));
    }
}
FUNCTIONS
static void expect(float r,float g,float b) {
    assert(fabsf(pixel[0]-r)<0.00001f);
    assert(fabsf(pixel[1]-g)<0.00001f);
    assert(fabsf(pixel[2]-b)<0.00001f);
}
static void reset(void) {
    int i; for(i=0;i<3;++i) { tintValue[i]=0; filterValue[i]=0; addValue[i]=0; }
    pixel[0]=0.3f; pixel[1]=0.6f; pixel[2]=0.9f;
    fadeValue=1; draws=viewport=0;
}
int main(void) {
    int value, offset;
    for(value=0;value<256;++value) for(offset=-300;offset<=300;++offset) {
        float expected;
        reset(); pixel[0]=value/255.0f; addValue[0]=offset; addValue[1]=-offset;
        fadeValue=.5f;
        std3D_XboxDrawColorEffects();
        expected=fminf(1,fmaxf(0,(value+offset)/255.0f))*.5f;
        expect(expected, fminf(1,fmaxf(0,.6f-offset/255.0f))*.5f, .45f);
        assert(src==GL_SRC_ALPHA && dst==GL_ONE_MINUS_SRC_ALPHA);
    }
    reset(); std3D_XboxDrawColorEffects(); expect(.3f,.6f,.9f); assert(draws==0);
    reset(); tintValue[0]=1; std3D_XboxDrawColorEffects();
    expect(.6f,.3f,.45f); assert(draws==2 && viewport==1);
    reset(); tintValue[0]=1; fadeValue=.5f; pixel[0]=.8f;
    std3D_XboxDrawColorEffects(); expect(.5f,.15f,.225f);
    reset(); filterValue[1]=1; std3D_XboxDrawColorEffects(); expect(.075f,.6f,.225f);
    reset(); fadeValue=0; std3D_XboxDrawColorEffects(); expect(0,0,0);
    reset(); tintValue[2]=.5f; std3D_XboxDrawColorEffects(); expect(.225f,.45f,1);
    reset(); g_sceneOpen=0; std3D_XboxDrawColorEffects(); assert(draws==0);
    puts("PASS: neutral, damage tint, saturation-before-fade, filter, death fade, blue tint");
    return 0;
}
'''.replace("FUNCTIONS", functions)
with tempfile.TemporaryDirectory(prefix="jk-effects-") as temp:
    path = Path(temp) / "test.c"
    binary = Path(temp) / "test.exe"
    path.write_text(program)
    subprocess.run([compiler, str(path), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
