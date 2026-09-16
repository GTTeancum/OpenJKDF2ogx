"""Compile the production setup focus helpers and check drill-in boundaries."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
source = (ROOT / 'src/Gui/jkGUISetup.c').read_text()

def function(signature):
    start = source.index(signature)
    brace = source.index('{', start)
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

program = r'''
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#define TARGET_XBOX
enum { ELEMENT_END, ELEMENT_TEXT, ELEMENT_TEXTBUTTON, ELEMENT_CHECKBOX, ELEMENT_SLIDER };
enum { FOCUS_NONE, FOCUS_LEFT, FOCUS_RIGHT, FOCUS_UP, FOCUS_DOWN };
typedef struct { int x, y; } Rect;
typedef struct { int type, hoverId, enableHover, bIsVisible; wchar_t *wstr; Rect rect; } jkGuiElement;
typedef struct { jkGuiElement *paElements, *lastMouseOverClickable, *focusedElement; int lastClicked; } jkGuiMenu;
static jkGuiElement jkGuiSetup_buttons[10];
static jkGuiMenu jkGuiSetup_menu = {jkGuiSetup_buttons};
static jkGuiMenu *jkGuiSetupXbox_submenus[8];
static int jkGuiSetupXbox_tab = 100;
static jkGuiElement *initial;
#define ANALOG_THRESHOLD 30
#define XB_BTN_LT 6
#define XB_BTN_RT 7
typedef struct { int connected; unsigned char prevAnalog[8]; } XboxControllerState;
static XboxControllerState g_pads[4];
static int g_activeController;
TRIGGER_READER
int jkGuiXboxControls_IsMenu(jkGuiMenu *menu) { return 0; }
void jkGuiRend_ClickableMouseover(jkGuiMenu *menu, jkGuiElement *e) { menu->lastMouseOverClickable=e; }
void jkGuiRend_XboxSetInitialFocus(jkGuiMenu *menu, jkGuiElement *e) { initial=e; }
FUNCTIONS
int main(void) {
    int i;
    assert(stdControl_XboxMenuTriggers()==0);
    g_pads[0].connected=1;g_pads[0].prevAnalog[6]=31;
    assert(stdControl_XboxMenuTriggers()==1);
    g_pads[0].prevAnalog[7]=255;assert(stdControl_XboxMenuTriggers()==3);
    g_pads[0].prevAnalog[6]=30;assert(stdControl_XboxMenuTriggers()==2);
    g_pads[0].connected=0;assert(stdControl_XboxMenuTriggers()==0);
    jkGuiElement rows[12] = {0}, controls[12] = {0};
    jkGuiMenu sub={rows}, accepted={controls};
    for(i=2;i<=6;i++) {jkGuiSetup_buttons[i].hoverId=i+98;rows[i].type=ELEMENT_TEXTBUTTON;controls[i].type=ELEMENT_TEXTBUTTON;}
    assert(jkGuiSetup_XboxFocus(&jkGuiSetup_menu,FOCUS_NONE));
    assert(jkGuiSetup_menu.lastMouseOverClickable==&jkGuiSetup_buttons[2]);
    for(i=0;i<10;i++) jkGuiSetup_XboxFocus(&jkGuiSetup_menu,FOCUS_RIGHT);
    assert(jkGuiSetupXbox_tab==104);
    jkGuiSetup_XboxFocus(&jkGuiSetup_menu,FOCUS_DOWN);
    assert(jkGuiSetupXbox_tab==104); /* Arrows never enter the page. */
    for(i=0;i<10;i++) jkGuiSetup_XboxFocus(&jkGuiSetup_menu,FOCUS_LEFT);
    assert(jkGuiSetupXbox_tab==100);
    assert(!jkGuiSetup_XboxFocus(&sub,FOCUS_RIGHT));
    rows[2].type=ELEMENT_TEXT;
    rows[7].type=ELEMENT_CHECKBOX;rows[7].rect.y=150; /* Hidden option. */
    rows[8].type=ELEMENT_SLIDER;rows[8].rect.y=200;rows[8].bIsVisible=1;
    jkGuiSetup_sub_412EF0(&sub,0);
    assert(jkGuiSetup_XboxIsSubmenu(&sub));
    assert(initial==&rows[8]);
    for(i=2;i<=6;i++) assert(rows[i].enableHover); /* Tabs cannot steal item focus. */
    rows[7].bIsVisible=1;rows[7].rect.x=20;rows[7].rect.y=150;
    rows[8].rect.x=330;rows[8].rect.y=150;
    rows[9]=rows[7];rows[9].rect.y=190;
    rows[10]=rows[8];rows[10].rect.y=190;
    sub.lastMouseOverClickable=&rows[7];
    jkGuiSetup_XboxFocus(&sub,FOCUS_DOWN);assert(sub.lastMouseOverClickable==&rows[9]);
    jkGuiSetup_XboxFocus(&sub,FOCUS_DOWN);assert(sub.lastMouseOverClickable==&rows[8]);
    jkGuiSetup_XboxFocus(&sub,FOCUS_DOWN);assert(sub.lastMouseOverClickable==&rows[10]);
    jkGuiSetup_XboxFocus(&sub,FOCUS_DOWN);assert(sub.lastMouseOverClickable==&rows[10]);
    jkGuiSetup_XboxFocus(&sub,FOCUS_UP);assert(sub.lastMouseOverClickable==&rows[8]);
    assert(!jkGuiSetup_XboxFocus(&sub,FOCUS_RIGHT)); /* Slider adjustment belongs to renderer. */
    jkGuiSetup_XboxFocus(&sub,FOCUS_UP);assert(sub.lastMouseOverClickable==&rows[9]);
    rows[2].hoverId=100;
    jkGuiSetup_XboxFocus(&sub,FOCUS_RIGHT);assert(sub.lastClicked==0);
    jkGuiSetup_XboxChangeMenu(&sub,1);assert(sub.lastClicked==101);
    sub.lastClicked=0;rows[2].hoverId=101;
    rows[7].rect.x=320;rows[8].rect.x=360;rows[8].type=ELEMENT_CHECKBOX;
    rows[9].rect.x=320;rows[10].rect.x=360;rows[10].type=ELEMENT_CHECKBOX;
    sub.lastMouseOverClickable=&rows[7];
    jkGuiSetup_XboxFocus(&sub,FOCUS_DOWN);assert(sub.lastMouseOverClickable==&rows[8]);
    jkGuiSetup_XboxFocus(&sub,FOCUS_DOWN);assert(sub.lastMouseOverClickable==&rows[9]);
    jkGuiSetup_XboxFocus(&sub,FOCUS_DOWN);assert(sub.lastMouseOverClickable==&rows[10]);
    jkGuiSetup_XboxFocus(&sub,FOCUS_UP);assert(sub.lastMouseOverClickable==&rows[9]);
    controls[6].type=ELEMENT_TEXT;
    initial=NULL;
    jkGuiSetup_sub_412EF0(&accepted,0);
    assert(!jkGuiSetup_XboxIsSubmenu(&accepted) && !initial);
    for(i=2;i<=6;i++) assert(!controls[i].enableHover);
    puts("PASS: setup tab limits, drill-in focus, disabled tabs, accepted Controls unchanged");
}
'''
program = program.replace('FUNCTIONS', '\n'.join(function(s) for s in (
    'int jkGuiSetup_XboxIsSubmenu(', 'int jkGuiSetup_XboxMenuTab(',
    'void jkGuiSetup_XboxChangeMenu(', 'static int jkGuiSetupXbox_OrderBefore(', 'int jkGuiSetup_XboxFocus(',
    'void jkGuiSetup_sub_412EF0(')))
control_source = (ROOT / 'src/Platform/Xbox/stdControl_xbox.c').read_text()
trigger_reader = control_source.split('int stdControl_XboxMenuTriggers(void)', 1)[1].split('void stdControl_XboxGetButtonMap', 1)[0]
program = program.replace('TRIGGER_READER', 'int stdControl_XboxMenuTriggers(void)' + trigger_reader)
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / 'test.c').write_text(program)
    compiler = shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe'
    subprocess.run([compiler, str(path / 'test.c'), '-o', str(path / 'test.exe')], check=True)
    subprocess.run([str(path / 'test.exe')], check=True)
