"""Check actual viewport/projection helpers against physical display geometry."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / "src/Platform/Xbox/xbox_splitscreen.c").read_text()
start = source.index("float xboxVideo_GetProjectionPixelAspectRatio(void)")
end = source.index("void xboxSplitScreen_ApplyViewport", start)
code = r'''
#include <cassert>
#include <cmath>
static int g_xboxSplitScreenEnabled, g_xboxSplitScreenLocalCount;
static int g_xboxSplitScreenLoggedViewports, g_xboxSplitScreenCurrentSlot;
static int Main_splitFullWidth;
static float pixelAspect;
float xboxVideo_GetPixelAspectRatio() { return pixelAspect; }
#define XDBGF(...) ((void)0)
FUNCTIONS
int main() {
 for (int full=0;full<2;++full) for (int wide=0; wide<2; ++wide) for (int count=1; count<=4; ++count) {
  Main_splitFullWidth=full;
  bool boxed=wide && !full;
  pixelAspect = wide ? 4.0f/3.0f : 1.0f;
  g_xboxSplitScreenLocalCount=count; g_xboxSplitScreenEnabled=count>1;
  g_xboxSplitScreenLoggedViewports=0;
  int area=0;
  for (int slot=0; slot<count; ++slot) {
   int x,y,w,h; g_xboxSplitScreenCurrentSlot=slot;
   xboxSplitScreen_GetViewport(slot,&x,&y,&w,&h);
   assert(x>=0 && y>=0 && x+w<=640 && y+h<=480);
   area+=w*h;
   if (count==1) assert(x==0 && y==0 && w==640 && h==480);
   else if (count==2) assert(x==0 && y==(slot?0:240) && w==640 && h==240);
   else {
    assert(w==(boxed?240:320) && h==240);
    assert(x==((slot&1)?320:(boxed?80:0)) && y==((slot&2)?0:240));
    // Each physical view is 4:3 and its projection has matching tangents.
    assert(fabs(w*pixelAspect/h-xboxVideo_GetProjectionPixelAspectRatio()/0.75f)<0.00001f);
    assert(xboxVideo_GetProjectionPixelAspectRatio()==(full?pixelAspect:1.0f));
    assert(xboxSplitScreen_GetCurrentViewportAspect()==0.75f);
   }
  }
  if (count>=3) assert(area==count*(boxed?57600:76800));
  else assert(xboxVideo_GetProjectionPixelAspectRatio()==pixelAspect);
  // Leaving split-screen must restore the system aspect for SP/menus.
  g_xboxSplitScreenEnabled=0;
  assert(xboxVideo_GetProjectionPixelAspectRatio()==pixelAspect);
  assert(xboxSplitScreen_GetCurrentViewportAspect()==0.0f);
 }
}
'''.replace("FUNCTIONS", source[start:end])
with tempfile.TemporaryDirectory() as temp:
    cpp = Path(temp) / "test.cpp"
    exe = Path(temp) / "test.exe"
    cpp.write_text(code)
    subprocess.run([shutil.which("clang++") or r"C:\Program Files\LLVM\bin\clang++.exe",
                    str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("PASS: 1P/2P unchanged, centered 3P/4P physical 4:3 views, 25% fewer widescreen pixels, projection reset")
