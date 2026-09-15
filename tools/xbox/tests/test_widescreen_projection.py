"""Check production CPU/GPU anamorphic projection for all split sizes."""
from pathlib import Path
import subprocess, tempfile, shutil
root = Path(__file__).resolve().parents[3]
cpu = (root / 'src/Engine/rdCamera.c').read_text()
gpu = (root / 'src/Platform/Xbox/std3D.c').read_text()
a = cpu.index('camera->fovDx = project_width_half / tangent;')
b = cpu.index('// UBSAN fixes', a)
cpu_block = cpu[a:b]
a = gpu.index('tan_h  = std3D_XboxHalfFovTangent(cam_fov);')
b = gpu.index('std3D_XboxApplyViewport();', a)
gpu_block = gpu[a:b]
portal = (root / "src/Engine/sithRender.c").read_text()
portal_line = next(line.strip() for line in portal.splitlines() if "sithRender_aVerticesTmp_projected[_i].y = cy" in line)
code = r"""
#include <assert.h>
#include <math.h>
#include <stdio.h>
#define TARGET_XBOX 1
typedef float flex_t;
static float par;
float rdCamera_XboxPixelAspect(void) { return par; }
float xboxVideo_GetPixelAspectRatio(void) { return par; }
float std3D_XboxHalfFovTangent(float x) { return tanf(x * 0.00872664626f); }
void check(float w, float h, float aspect) {
 struct { float fovDx; } cam, *camera = &cam;
 float project_width_half = w * .5f;
 float tangent = 1.0f;
 float cam_fov = 90, cam_aspect = h/w, cam_znear = .0625f;
 float tan_h, tan_v, half_w, half_h;
 par = aspect;
 CPU
 GPU
 {
 struct { float x,y,z; } sithRender_aVerticesTmp[1], sithRender_aVerticesTmp_projected[1];
 int _i=0;
 float cy=h*.5f, s=fovDx/2;
 sithRender_aVerticesTmp[0].z=.5f;
 PORTAL_LINE
 assert(fabsf(sithRender_aVerticesTmp_projected[0].y-(cy-.5f*w*.25f))<.0001f);
 }
 assert(fabsf(fovDx * tan_h - project_width_half) < .0001f);
 assert(fabsf(fovDy * tan_v - h*.5f) < .0001f);
 /* A world-space square must have equal physical width/height. */
 assert(fabsf((w/tan_h)*par - h/tan_v) < .0001f);
 assert(fabsf(tan_v - h/w) < .00001f);
 assert(fabsf(tan_h - par) < .00001f);
}
int main(void) {
 float sizes[][2]={{640,480},{640,240},{320,240}};
 int i;
 for(i=0;i<3;i++) { check(sizes[i][0],sizes[i][1],1); check(sizes[i][0],sizes[i][1],4.0f/3); }
 puts("PASS: CPU/GPU agree; 4:3 unchanged; 16:9 expands horizontally for one, two and four views");
}
""".replace(' CPU\n', cpu_block, 1).replace(' GPU\n', gpu_block, 1).replace('PORTAL_LINE', portal_line)
with tempfile.TemporaryDirectory() as tmp:
 c=Path(tmp)/'test.c'; exe=Path(tmp)/'test.exe'; c.write_text(code)
 subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
