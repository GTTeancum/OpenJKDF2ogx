"""Exercise actual HUD open/close scale state across mode changes."""
from pathlib import Path
import shutil,subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/Main/jkHud.c').read_text()
a=s.index('    jkHud_multiplayerAssets = (sithNet_isMulti != 0);')
b=s.index('    XDBGF("HudMode:',a)
c=s.index('    if (jkHud_multiplayerAssets) {',s.index('void jkHud_Close()'))
d=s.index('#endif',c)
scale=s[s.index('static flex_t jkHud_GetRenderScale(void)'):s.index('\n}',s.index('static flex_t jkHud_GetRenderScale(void)'))+2]
code='''#include <assert.h>
#include <math.h>
#define TARGET_XBOX 1
typedef float flex_t;
int sithNet_isMulti, jkHud_multiplayerAssets, localPlayers=4;
int xboxSplitScreen_IsRequested(void) {return localPlayers>0;}
int xboxSplitScreen_GetRequestedLocalPlayerCount(void) {return localPlayers;}
float jkHud_savedScale, jkPlayer_hudScale;
SCALE
void openHud(void) { OPEN }
void closeHud(void) { CLOSE }
int main(void) {
 jkPlayer_hudScale=1.25f;
 sithNet_isMulti=0; openHud(); assert(jkPlayer_hudScale==1.25f); closeHud();
 assert(fabsf(jkHud_GetRenderScale()-1.25f*4.0f/3.0f)<0.00001f);
 for(int i=0;i<3;i++) {openHud();closeHud();assert(jkPlayer_hudScale==1.25f);}
 for(localPlayers=0;localPlayers<=4;localPlayers++) {
  sithNet_isMulti=1;openHud();assert(fabsf(jkPlayer_hudScale-(localPlayers<=1 ? 2.0f/3.0f : 1.0f/3.0f))<0.00001f);
  assert(jkHud_GetRenderScale()==jkPlayer_hudScale);
  closeHud();assert(jkPlayer_hudScale==1.25f);
  closeHud();assert(jkPlayer_hudScale==1.25f);
 }
 sithNet_isMulti=0;openHud();assert(!jkHud_multiplayerAssets);assert(jkPlayer_hudScale==1.25f);
}
'''.replace('OPEN',s[a:b]).replace('CLOSE',s[c:d]).replace('SCALE',scale)
with tempfile.TemporaryDirectory() as t:
 p=Path(t)/'test.c';exe=Path(t)/'test.exe';p.write_text(code)
 subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: multiplayer scale and repeated open/close restore single-player preference')
