"""Exercise the production split-player tick wrapper across player contexts."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
source = (ROOT / 'src/Platform/Xbox/xbox_splitscreen.c').read_text()
wrapper = 'void xboxSplitScreen_TickPlayer' + source.split(
    'void xboxSplitScreen_TickPlayer', 1)[1].split(
    'void xboxSplitScreen_GetViewport', 1)[0]
program = r'''
#include <assert.h>
#include <stdio.h>
typedef float flex_t;
typedef struct { int ticks; } sithPlayerInfo;
static sithPlayerInfo jkPlayer_playerInfos[4];
static int g_xboxSplitScreenCurrentSlot, g_xboxSplitScreenEnabled = 1;
static int g_xboxSplitScreenLocalCount = 2;
static int liveTimer, savedTimers[2] = {10, 100};
static int xboxSplitScreen_GetPlayerIndexForSlot(int slot) { return slot + 1; }
static void xboxSplitScreen_SetContextForLocalSlot(int slot) { g_xboxSplitScreenCurrentSlot = slot; }
static void xboxSplitScreen_ApplyTransientStateForSlot(int slot) { liveTimer = savedTimers[slot]; }
static void xboxSplitScreen_SaveTransientStateForSlot(int slot) { savedTimers[slot] = liveTimer; }
static void sithPlayer_Tick(sithPlayerInfo *info, flex_t delta) {
    ++info->ticks;
    if (info == &jkPlayer_playerInfos[xboxSplitScreen_GetPlayerIndexForSlot(g_xboxSplitScreenCurrentSlot)])
        liveTimer += (int)delta;
}
WRAPPER
int main(void) {
    liveTimer = savedTimers[0];
    xboxSplitScreen_TickPlayer(&jkPlayer_playerInfos[1], 3);
    assert(savedTimers[0] == 13 && savedTimers[1] == 100 && liveTimer == 13);
    xboxSplitScreen_TickPlayer(&jkPlayer_playerInfos[2], 7);
    assert(savedTimers[0] == 13 && savedTimers[1] == 107 && liveTimer == 13);
    assert(g_xboxSplitScreenCurrentSlot == 0);
    /* A remote player must not borrow either local player's context. */
    xboxSplitScreen_TickPlayer(&jkPlayer_playerInfos[3], 99);
    assert(jkPlayer_playerInfos[3].ticks == 1 && liveTimer == 13);
    /* Rendering/context restore must retain the completed update. */
    xboxSplitScreen_ApplyTransientStateForSlot(1);
    assert(liveTimer == 107);
    g_xboxSplitScreenEnabled = 0;
    xboxSplitScreen_TickPlayer(&jkPlayer_playerInfos[1], 2);
    assert(liveTimer == 109 && savedTimers[0] == 13 && savedTimers[1] == 107);
    puts("PASS: local player ticks persist independently; remote and non-split ticks retain behavior");
}
'''.replace('WRAPPER', wrapper)
with tempfile.TemporaryDirectory() as temp:
    c = Path(temp) / 'test.c'
    exe = Path(temp) / 'test.exe'
    c.write_text(program)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',
                    str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
