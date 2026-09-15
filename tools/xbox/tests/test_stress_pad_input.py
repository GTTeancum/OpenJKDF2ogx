"""Check four raw synthetic gamepads through the production trigger gate."""
from pathlib import Path
import shutil
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[3]
source = (ROOT / 'src/Platform/Xbox/stdControl_xbox.c').read_text()
body = 'static void stdControl_XboxFireProbeApply' + source.split(
    'static void stdControl_XboxFireProbeApply', 1)[1].split(
    'static void stdControl_XboxSmokeInputProbeLogPhase', 1)[0]
assert 'stdControl_SetKeydown' not in body
gate = '    cur = gameplay && (pad->bAnalogButtons[XB_BTN_RT]' + source.split(
    '    cur = gameplay && (pad->bAnalogButtons[XB_BTN_RT]', 1)[1].split('    /* B stays GUI cancel', 1)[0]
program = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
#define XPERF(...) ((void)0)
#define XB_BTN_RT 7
#define XB_BTN_LT 6
#define KEY_JOY1_B17 0
#define KEY_JOY1_B16 1
#define ANALOG_THRESHOLD 30
typedef unsigned long DWORD;
typedef struct { unsigned char bAnalogButtons[8]; short sThumbLX,sThumbLY,sThumbRX; } XINPUT_GAMEPAD;
typedef struct Thing sithThing;
typedef struct { sithThing *playerThing; int curWeapon; struct { float ammoAmt; } iteminfo[12]; } sithPlayerInfo;
struct Thing { struct { sithPlayerInfo *playerinfo; float health; } actorParams; struct { int id; } *sector; struct { float x,y,z; } position; };
static sithThing *sithPlayer_pLocalPlayerThing;
static int stdControl_XboxFourPlayerStressEnabled(void) { return 1; }
static DWORD GetFileAttributesA(const char *path) { return (DWORD)-1; }
static int keys[2];
static void stdControl_SetKeydown(int key,int down,unsigned int tick) { keys[key]=down; }
BODY
static void poll(XINPUT_GAMEPAD *pad, int gameplay) {
    int cur; unsigned int tick=4100;
GATE
}
int main(void) {
    for (int port=0;port<4;port++) {
        XINPUT_GAMEPAD pad={0};
        stdControl_XboxFireProbeApply(port,&pad,100,1);
        memset(&pad,0,sizeof(pad));
        stdControl_XboxFireProbeApply(port,&pad,4100,1);
        CHECK(pad.bAnalogButtons[(port&1)?XB_BTN_LT:XB_BTN_RT]==255);
        CHECK(pad.sThumbLY && pad.sThumbLX && pad.sThumbRX);
        poll(&pad,1);
        CHECK(keys[(port&1)?1:0] && !keys[(port&1)?0:1]);
        memset(&pad,0,sizeof(pad));
        stdControl_XboxFireProbeApply(port,&pad,4101,0);
        CHECK(pad.bAnalogButtons[(port&1)?XB_BTN_LT:XB_BTN_RT]==255);
        CHECK(!pad.sThumbLY && !pad.sThumbLX && !pad.sThumbRX);
        poll(&pad,0);
        CHECK(!keys[0] && !keys[1]);
    }
    puts("PASS: four raw pads move/fire; held primary/secondary triggers pass through normal menu suppression");
}
'''.replace('BODY',body).replace('GATE',gate)
with tempfile.TemporaryDirectory() as temp:
    c=Path(temp)/'test.c'; exe=Path(temp)/'test.exe'; c.write_text(program)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
