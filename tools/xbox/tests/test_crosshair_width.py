"""Check actual crosshair rectangle construction for fractional widths."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'src/Main/jkHud.c').read_text()
start = source.index('        flex_t line_len = v24 - v25 + 1;')
end = source.index('        std3D_DrawUIClearedRect', start)
block = source[start:end]
program = r'''
#include <assert.h>
#include <stdio.h>
#define TARGET_XBOX 1
typedef float flex_t;
typedef struct { int x,y,width,height; } rdRect;
static void check(float jkPlayer_crosshairLineWidth, int expected) {
    int v22=160, v23=120, v24=18, v25=6;
    BLOCK
    assert(rect1.width==13 && rect2.width==13);
    assert(rect3.height==13 && rect4.height==13);
    assert(rect1.height==expected && rect2.height==expected);
    assert(rect3.width==expected && rect4.width==expected);
}
int main(void) {
    check(.25f,1); check(.99f,1); check(0,1); check(-1,1);
    check(1,1); check(2,2); check(3.5f,3);
    puts("PASS: enabled fractional-width crosshairs retain nonzero rectangles; normal widths preserved");
}
'''.replace('BLOCK', block)
with tempfile.TemporaryDirectory() as tmp:
    c = Path(tmp) / 'test.c'
    exe = Path(tmp) / 'test.exe'
    c.write_text(program)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',
                    str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
