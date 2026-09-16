"""Run the production analog-button polling block with remapped physical pads."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
source = (ROOT / 'src/Platform/Xbox/stdControl_xbox.c').read_text()
setter = 'void stdControl_SetKeydown' + source.split('void stdControl_SetKeydown',1)[1].split('void stdControl_SetSDLKeydown',1)[0]
poll = source.split('    /* Analog buttons',1)[1].split('    /* Sticks',1)[0]
poll = poll[poll.index('    /* Do not carry'):]
program = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "Platform/Xbox/xbox_control_map.h"
#define XBOX_NUM_KEYS 512
#define XB_BTN_A 0
#define XB_BTN_B 1
#define XB_BTN_X 2
#define XB_BTN_Y 3
#define XB_BTN_BLACK 4
#define XB_BTN_WHITE 5
#define XB_BTN_LT 6
#define XB_BTN_RT 7
#define KEY_JOY1_B1 256
#define KEY_JOY1_B2 257
#define KEY_JOY1_B3 258
#define KEY_JOY1_B4 259
#define KEY_JOY1_B10 265
#define KEY_JOY1_B11 266
#define KEY_JOY1_B16 271
#define KEY_JOY1_B17 272
#define DIK_X 45
#define DIK_SPACE 57
#define ANALOG_THRESHOLD 30
static unsigned char g_keyDown[512],g_keyPress[512],g_prevAnalog[8];
static unsigned int g_keyTime[512];
static int map[8];
SETTER
static void readPad(int gameplay,int physical) {
    struct Pad {unsigned char bAnalogButtons[8];} value,*pad=&value;
    unsigned char physicalAnalog[8]={0},previousAnalog[8];
    unsigned int tick=100;
    int cur,prev,i;
    memset(g_keyPress,0,sizeof(g_keyPress));
    if(physical>=0) physicalAnalog[physical]=255;
    xboxControlMap_Apply(map,physicalAnalog,pad->bAnalogButtons,gameplay);
    xboxControlMap_Apply(map,g_prevAnalog,previousAnalog,gameplay);
POLL
}
int main(void) {
    xboxControlMap_Defaults(map);
    xboxControlMap_Assign(map,7,0);
    readPad(1,0); assert(g_keyDown[KEY_JOY1_B17] && !g_keyDown[DIK_X]);
    readPad(1,0); assert(!g_keyPress[KEY_JOY1_B17]);
    readPad(1,-1); readPad(1,7); assert(g_keyDown[DIK_X] && !g_keyDown[KEY_JOY1_B17]);
    /* Pausing with a remapped jump held must release gameplay keys. */
    readPad(0,7); assert(!g_keyDown[DIK_X] && !g_keyDown[KEY_JOY1_B17]);
    readPad(0,-1); readPad(0,0); assert(g_keyDown[KEY_JOY1_B1]);
    readPad(1,0); assert(!g_keyDown[KEY_JOY1_B1]);
    readPad(0,0); assert(!g_keyDown[KEY_JOY1_B1]);
    readPad(0,-1); readPad(0,0); assert(g_keyDown[KEY_JOY1_B1]);
    readPad(0,-1); readPad(0,1); assert(g_keyDown[KEY_JOY1_B2]);
    readPad(0,-1); readPad(0,2); assert(g_keyDown[KEY_JOY1_B3] && !g_keyDown[DIK_SPACE]);
    readPad(0,-1);
    xboxControlMap_Assign(map,2,1);
    readPad(1,1); assert(g_keyDown[DIK_SPACE]);
    readPad(0,1); assert(!g_keyDown[DIK_SPACE]);
    puts("PASS: remapped fire/jump/use, held edges, menu releases and physical A/B/X navigation");
}
'''.replace('SETTER',setter).replace('POLL',poll)
compiler = shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe'
with tempfile.TemporaryDirectory(prefix='jk-remap-') as temp:
    path=Path(temp)/'test.c'; exe=Path(temp)/'test.exe'
    path.write_text(program)
    subprocess.run([compiler,str(path),'-I',str(ROOT/'src'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
