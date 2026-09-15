"""Test production Xbox trigger polling and edge tracking without host input."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
compiler = shutil.which("clang") or r"C:\Program Files\LLVM\bin\clang.exe"
source = (ROOT / "src/Platform/Xbox/stdControl_xbox.c").read_text()
setter = "void stdControl_SetKeydown" + source.split("void stdControl_SetKeydown", 1)[1].split(
    "void stdControl_SetSDLKeydown", 1
)[0]
poll = source.split("    cur = gameplay && (pad->bAnalogButtons[XB_BTN_RT]", 1)[1]
poll = "    cur = gameplay && (pad->bAnalogButtons[XB_BTN_RT]" + poll.split("    /* B stays GUI cancel", 1)[0]
program = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define XBOX_NUM_KEYS 512
#define XB_BTN_RT 7
#define XB_BTN_LT 6
#define KEY_JOY1_B17 273
#define KEY_JOY1_B16 272
#define ANALOG_THRESHOLD 30
static unsigned char g_keyDown[XBOX_NUM_KEYS], g_keyPress[XBOX_NUM_KEYS];
static unsigned int g_keyTime[XBOX_NUM_KEYS];
SETTER
static void poll(int gameplay, int rt, int lt) {
    struct Pad { unsigned char bAnalogButtons[8]; } value = {{0}}, *pad = &value;
    int cur;
    unsigned int tick = 100;
    memset(g_keyPress, 0, sizeof(g_keyPress));
    pad->bAnalogButtons[XB_BTN_RT] = rt;
    pad->bAnalogButtons[XB_BTN_LT] = lt;
POLL
}
int main(void) {
    poll(1, 255, 255);
    assert(g_keyDown[KEY_JOY1_B17] && g_keyDown[KEY_JOY1_B16]);
    assert(g_keyPress[KEY_JOY1_B17] == 1);
    poll(1, 255, 255);
    assert(g_keyPress[KEY_JOY1_B17] == 0);
    /* Entering a menu releases both held triggers without a physical edge. */
    poll(0, 255, 255);
    assert(!g_keyDown[KEY_JOY1_B17] && !g_keyDown[KEY_JOY1_B16]);
    poll(0, 0, 0);
    poll(0, 255, 255);
    assert(!g_keyDown[KEY_JOY1_B17] && !g_keyPress[KEY_JOY1_B17]);
    /* Returning while held produces one fresh activation, not a stuck release. */
    poll(1, 255, 255);
    assert(g_keyDown[KEY_JOY1_B17] && g_keyPress[KEY_JOY1_B17] == 1);
    poll(1, 0, 0);
    assert(!g_keyDown[KEY_JOY1_B17] && !g_keyDown[KEY_JOY1_B16]);
    puts("PASS: menu triggers release, stay inactive, and resume with correct edges");
    return 0;
}
'''.replace("SETTER", setter).replace("POLL", poll)
with tempfile.TemporaryDirectory(prefix="jk-triggers-") as temp:
    path = Path(temp) / "test.c"
    binary = Path(temp) / "test.exe"
    path.write_text(program)
    subprocess.run([compiler, str(path), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
