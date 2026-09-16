"""Exercise production button mapping and the scrolling menu state on the host."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
compiler = shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe'
source = (ROOT / 'src/Gui/jkGUIXboxControls.c').read_text()

def function(name):
    start = source.index(name)
    start = source.rfind('\n', 0, start) + 1
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

program = r'''
#include <assert.h>
#include <stdio.h>
#include "Platform/Xbox/xbox_control_map.h"
#define VISIBLE_ROWS 6
#define OPTION_FIRST 18
#define ROW_COUNT 23
#define ROW_FIRST 10
static int editing=-1,selected,top,savedSelection,savedTop;
static int buttonMap[8],options[5];
static const int logicalButtons[8]={7,6,0,2,4,5,3,1};
typedef struct Element {int unused;} Element;
static Element elements[17];
static struct {Element *focusedElement,*lastMouseOverClickable,*lastMouseDownClickable;} controls;
FUNCTIONS
int main(void) {
    int a,b,i,map[8];
    unsigned char raw[8]={11,22,33,44,55,66,77,88},out[8],previous[8];
    for(a=0;a<8;a++) for(b=0;b<8;b++) {
        xboxControlMap_Defaults(map);
        assert(xboxControlMap_Assign(map,a,b));
        assert(xboxControlMap_Valid(map));
        assert(map[a]==b && map[b]==a);
        xboxControlMap_Apply(map,raw,out,1);
        assert(out[a]==raw[b]);
        xboxControlMap_Apply(map,raw,previous,1);
        for(i=0;i<8;i++) assert(out[i]==previous[i]);
        /* Menus always see the physical buttons, including after reassignment. */
        xboxControlMap_Apply(map,raw,out,0);
        for(i=0;i<8;i++) assert(out[i]==raw[i]);
    }
    map[0]=map[1];assert(!xboxControlMap_Valid(map));
    xboxControlMap_Defaults(map);map[0]=8;assert(!xboxControlMap_Valid(map));
    map[0]=-1;assert(!xboxControlMap_Valid(map));
    defaults();
    for(i=0;i<ROW_COUNT;i++) {selected=i;focusRow();assert(selected>=top && selected<top+6);}
    assert(top==ROW_COUNT-6);
    selected=100;focusRow();assert(selected==22 && top==17);
    selected=-1;focusRow();assert(selected==0 && top==0);
    edit();assert(editing==7 && selected==7 && top==2);
    selected=0;edit();assert(editing==-1 && selected==0 && top==0);
    assert(buttonMap[7]==0 && buttonMap[0]==7);
    selected=18;options[0]=100;changeOption(1);assert(options[0]==100);
    options[0]=1;changeOption(-1);assert(options[0]==1);
    selected=20;options[2]=30;changeOption(1);assert(options[2]==30);
    selected=21;changeOption(1);assert(options[3]==1);changeOption(-1);assert(options[3]==0);
    defaults();assert(buttonMap[7]==7 && buttonMap[0]==0 && options[0]==50 && options[1]==75 && options[2]==12 && options[4]==1);
    puts("PASS: all button swaps, physical menu input, scrolling bounds, editor restoration and option limits");
}
'''.replace('FUNCTIONS', '\n'.join(function(name) for name in [
    'static int count(', 'static void clampSelection(', 'static void focusRow(',
    'static void changeOption(', 'static void defaults(', 'static void edit('
]))
with tempfile.TemporaryDirectory(prefix='jk-controls-') as temp:
    path = Path(temp) / 'controls.c'
    exe = Path(temp) / 'controls.exe'
    path.write_text(program)
    subprocess.run([compiler, str(path), '-I', str(ROOT / 'src'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
