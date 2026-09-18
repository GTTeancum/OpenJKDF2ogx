"""Check the production calibration curve and split viewport boundaries."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
program = r'''
#include <assert.h>
#include <stdio.h>
#include "Platform/Xbox/xbox_display_settings.h"
static int g_displayGamma=100, g_displayContrast=100, g_displaySafeX=100, g_displaySafeY=100;
static bool g_displayRampDirty=false;
struct FakeGL { int changes; void DisplaySettingsChanged() { ++changes; } } device;
static FakeGL *gFakeGL=0;
SETTINGS_SETTER
int main(void) {
    int g, c, i, s;
    for (i=0; i<256; ++i) assert(xboxDisplay_RampValue(i,100,100)==i);
    for (g=50; g<=150; ++g) for(c=50; c<=150; ++c) {
        for(i=1;i<256;++i)
            assert(xboxDisplay_RampValue(i,g,c)>=xboxDisplay_RampValue(i-1,g,c));
    }
    assert(xboxDisplay_RampValue(64,125,100)>64);
    assert(xboxDisplay_RampValue(64,100,125)<64);
    assert(xboxDisplay_RampValue(192,100,125)>192);
    for(s=80;s<=100;++s) {
        int l=xboxDisplay_SafeEdge(0,640,s), r=xboxDisplay_SafeEdge(640,640,s);
        int m=xboxDisplay_SafeEdge(320,640,s);
        int t=xboxDisplay_SafeEdge(0,480,s), b=xboxDisplay_SafeEdge(480,480,s);
        assert(l==640-r && t==480-b);
        assert(m==320 && (m-l)+(r-m)==r-l);
        assert(xboxDisplay_SafeEdge(240,480,s)==240);
    }
    assert(xboxDisplay_SafeEdge(0,640,90)==32);
    assert(xboxDisplay_SafeEdge(0,480,90)==24);
    assert(xboxDisplay_Clamp(-100,50,150)==50);
    assert(xboxDisplay_Clamp(200,80,100)==100);
    xboxVideo_SetDisplaySettings(100,100,80,100);
    assert(g_displaySafeX==80 && g_displaySafeY==100 && !g_displayRampDirty);
    gFakeGL=&device;
    xboxVideo_SetDisplaySettings(150,50,100,85);
    assert(g_displayGamma==150 && g_displayContrast==50 && g_displayRampDirty);
    assert(g_displaySafeX==100 && g_displaySafeY==85 && device.changes==1);
    xboxVideo_SetDisplaySettings(-1,999,500,-1);
    assert(g_displayGamma==50 && g_displayContrast==150);
    assert(g_displaySafeX==100 && g_displaySafeY==80);
    puts("PASS: identity, monotone gamma/contrast, safe-zone edges and clamps");
}
'''
source = (root/'src/Platform/Xbox/fakeglx.cpp').read_text()
start = source.index('extern "C" void xboxVideo_SetDisplaySettings(')
end = source.index('\n}', start)+2
program = program.replace('SETTINGS_SETTER', source[start:end])
with tempfile.TemporaryDirectory(prefix='jk-display-') as tmp:
    path = Path(tmp)/'test.cpp'
    binary = Path(tmp)/'test.exe'
    path.write_text(program)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',
                    str(path), '-I'+str(root/'src'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
