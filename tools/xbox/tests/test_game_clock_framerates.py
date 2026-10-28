"""Exercise production Xbox game-clock and step scheduling across 60 FPS."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
clock = (root / "src/Gameplay/sithTime.c").read_text()
clock = "\n".join(line for line in clock.splitlines() if not line.startswith("#include"))
config = (root / "src/engine_config.h").read_text()
# Xbox builds define QOL_IMPROVEMENTS and use the non-TWL physics rates.
limits = re.findall(r"^#define SITHTIME_(?:MIN|MAX)DELTA \([^\n]+", config, re.M)[-2:]
rates = re.findall(r"^#define (?:TARGET_PHYSTICK_FPS|DELTA_PHYSTICK_FPS) [^\n]+", config, re.M)[-2:]
main = (root / "src/Main/sithMain.c").read_text()
step = re.search(r"flex_d_t rolloverCombine = sithTime_deltaSeconds.*?"
                 r"sithTime_physicsRolloverFrames = rolloverCombine -[^;]+;", main, re.S).group()
program = r'''
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
typedef double flex_d_t;
typedef float flex32_t;
#define DEBUGFLAG_SLOWMO 1
LIMITS
RATES
static uint32_t now, sithTime_curMs, sithTime_curMsAbsolute, sithTime_pauseTimeMs;
static int sithTime_deltaMs, sithTime_bRunning, g_debugmodeFlags;
static int sithNet_isMulti, sithNet_tickrate;
static double sithTime_deltaSeconds, sithTime_TickHz;
static float sithTime_curSeconds;
static uint32_t stdPlatform_GetTimeMsec(void) { return now; }
void sithTime_SetDelta(int);
CLOCK
static unsigned steps(void) { STEP return wholeFramesToApply; }
int main(void) {
 const unsigned fps[] = {30,59,60,61,90,120,144,240};
 for (unsigned mode=0;mode<3;++mode) {
  sithNet_isMulti=mode!=0;sithNet_tickrate=mode==1?50:100;
  for (unsigned rate=0;rate<sizeof(fps)/sizeof(*fps);++rate) {
   unsigned totalSteps=0;
   now=1000;sithTime_Startup();
   for (unsigned frame=1;frame<=fps[rate]*180;++frame) {
    now=1000+(uint32_t)((uint64_t)frame*1000/fps[rate]);
    sithTime_Tick();totalSteps+=steps();
   }
   assert(sithTime_curMs==180000);
   assert(totalSteps==(unsigned)(TARGET_PHYSTICK_FPS*180));
  }
  // Alternate fast and slow rendering without resetting the game timeline.
  unsigned totalSteps=0;
  now=1000;sithTime_Startup();
  for (unsigned block=0;block<24;++block) {
   unsigned rate=fps[block%8], start=now;
   for (unsigned frame=1;frame<=rate*5;++frame) {
    now=start+(uint32_t)((uint64_t)frame*1000/rate);
    sithTime_Tick();totalSteps+=steps();
   }
  }
  assert(sithTime_curMs==120000);
  assert(totalSteps==(unsigned)(TARGET_PHYSTICK_FPS*120));
 }
 puts("PASS: production clock and fixed-step scheduling stay at real-time rates from 30 to 240 FPS, including transitions");
}
'''.replace("LIMITS", "\n".join(limits)).replace("RATES", "\n".join(rates)).replace("CLOCK", clock).replace("STEP", step)
with tempfile.TemporaryDirectory(prefix="jk-clock-rates-") as temp:
    source = Path(temp) / "clock.c"
    binary = Path(temp) / "clock.exe"
    source.write_text(program)
    subprocess.run([shutil.which("clang") or r"C:\Program Files\LLVM\bin\clang.exe",
                    str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
