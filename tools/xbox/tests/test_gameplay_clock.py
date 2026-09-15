"""Run the real clock code and gameplay-entry clock calls without an Xbox.

Only platform time and engine globals are stubbed. Requires a host C compiler.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
compiler = shutil.which("clang") or r"C:\Program Files\LLVM\bin\clang.exe"
clock = (ROOT / "src/Gameplay/sithTime.c").read_text()
clock = "\n".join(line for line in clock.splitlines() if not line.startswith("#include"))
main = (ROOT / "src/Main/jkMain.c").read_text()
entry = main.split('JKTRACE("GameplayShow: thing_eight=1')[0]
# Take the actual clock operations between the warmup trace and frame timestamp.
entry = entry.split('JKTRACE("GameplayShow: refresh frame clock after load/display warmup\\n");')[1]
entry = entry.split("jkMain_lastTickMs =")[0].replace("#endif", "")
shim = r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef double flex_d_t;
typedef float flex32_t;
#define SITHTIME_MINDELTA 1
#define SITHTIME_MAXDELTA 200
#define DEBUGFLAG_SLOWMO 1
static uint32_t now, sithTime_curMs, sithTime_curMsAbsolute, sithTime_pauseTimeMs;
static int sithTime_deltaMs, sithTime_bRunning, g_debugmodeFlags;
static double sithTime_deltaSeconds, sithTime_TickHz;
static float sithTime_curSeconds;
static uint32_t stdPlatform_GetTimeMsec(void) { return now; }
void sithTime_SetDelta(int);
'''
tests = r'''
static void enter_gameplay(void) {
ENTRY
}
int main(void) {
    unsigned int saved;
    now = 100;
    sithTime_Startup();
    sithTime_SetMs(120000);
    /* Menu pause plus display warmup must not rewind active weapon/COG timers. */
    sithTime_Pause();
    now += 10000;
    sithTime_Resume();
    now += 2500;
    sithTime_physicsRolloverFrames = 0.005;
    enter_gameplay();
    assert(sithTime_curMs == 120000);
    assert(sithTime_curSeconds > 119.99f);
    assert(sithTime_physicsRolloverFrames == 0.0);
    now += 16;
    sithTime_Tick();
    assert(sithTime_deltaMs == 16);
    assert(sithTime_curMs == 120016);
    /* A loaded save retains its restored timeline after display setup. */
    saved = 654321;
    sithTime_SetMs(saved);
    now += 30000;
    enter_gameplay();
    assert(sithTime_curMs == saved);
    now += 20;
    sithTime_Tick();
    assert(sithTime_curMs == saved + 20);
    assert(sithTime_deltaMs == 20);
    puts("PASS: menu and save timelines survive gameplay entry; warmup is excluded");
    return 0;
}
'''.replace("ENTRY", entry)
with tempfile.TemporaryDirectory(prefix="jk-clock-") as temp:
    source = Path(temp) / "clock_test.c"
    binary = Path(temp) / ("clock_test.exe" if os.name == "nt" else "clock_test")
    source.write_text(shim + clock + tests)
    subprocess.run([compiler, str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
