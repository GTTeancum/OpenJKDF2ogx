"""Exercise production scalar cache against the original projection formula."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'src/Platform/Xbox/std3D.c').read_text()
source = source[source.index('static float std3D_XboxHalfFovTangent('):
                source.index('static void std3D_XboxApplyFaceBlend(')]
program = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
static int calls;
static double counted_tan(double value) { ++calls; return tan(value); }
#define tan counted_tan
SOURCE
#undef tan
int main(void) {
    int i, before;
    float fov, actual, expected;
    for(i=0;i<1000;++i) {
        actual=std3D_XboxHalfFovTangent(90.0f);
        assert(actual==(float)tan(90.0*(3.14159265358979/360.0)));
    }
    assert(calls==1);
    for(i=0;i<1800;++i) {
        fov=1.0f+i*.099f;
        expected=(float)tan((double)fov*(3.14159265358979/360.0));
        actual=std3D_XboxHalfFovTangent(fov);
        assert(actual==expected);
        before=calls;
        assert(std3D_XboxHalfFovTangent(fov)==expected && calls==before);
    }
    std3D_XboxHalfFovTangent(75.0f);
    std3D_XboxHalfFovTangent(110.0f);
    assert(std3D_XboxHalfFovTangent(75.0f)==(float)tan(75.0*(3.14159265358979/360.0)));
    before=calls;
    assert(isnan(std3D_XboxHalfFovTangent(NAN)));
    assert(isnan(std3D_XboxHalfFovTangent(NAN)) && calls==before+2);
    puts("PASS: exact formula, animated FOV, alternating cameras, repeated values, NaN invalidation");
}
'''.replace('SOURCE', source)
with tempfile.TemporaryDirectory() as tmp:
    c = Path(tmp) / 'test.c'
    exe = Path(tmp) / 'test.exe'
    c.write_text(program)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',
                    str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
