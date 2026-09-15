"""Run production palette aggregation with sparse per-view/global requests."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
source = (ROOT / 'src/General/stdPalEffects.c').read_text()
gather = 'void stdPalEffects_GatherEffects()' + source.split(
    'void stdPalEffects_GatherEffects()', 1)[1].split('// setunk', 1)[0]
program = r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#define _memset memset
#define _memcpy memcpy
typedef double flex_d_t;
typedef struct { float x,y,z; } Float3;
typedef struct { int x,y,z; } Int3;
typedef struct { Int3 filter; Float3 tint; Int3 add; float fade; } stdPalEffect;
typedef struct { int isValid, idx; stdPalEffect effect; } stdPalEffectRequest;
static stdPalEffectRequest stdPalEffects_aEffects[32];
static int stdPalEffects_numEffectRequests;
static struct { stdPalEffect effect; int bUseFilter,bUseTint,bUseFade,bUseAdd; } stdPalEffects_state;
void stdPalEffects_GatherEffectsMasked(uint32_t);
GATHER
int main(void) {
    stdPalEffects_aEffects[0].isValid = 1;
    stdPalEffects_aEffects[0].effect.tint.x = 1;
    stdPalEffects_aEffects[0].effect.fade = 1;
    stdPalEffects_aEffects[2].isValid = 1;
    stdPalEffects_aEffects[2].effect.tint.z = 0.75f;
    stdPalEffects_aEffects[2].effect.fade = 1;
    stdPalEffects_aEffects[31].isValid = 1;
    stdPalEffects_aEffects[31].effect.fade = 0.5f;
    stdPalEffects_aEffects[31].effect.add.y = 10;
    stdPalEffects_numEffectRequests = 3;
    stdPalEffects_GatherEffectsMasked(1U << 2);
    assert(stdPalEffects_state.effect.tint.x == 1 && stdPalEffects_state.effect.tint.z == 0);
    assert(stdPalEffects_state.effect.fade == 0.5f && stdPalEffects_state.effect.add.y == 10);
    stdPalEffects_GatherEffectsMasked(1U);
    assert(stdPalEffects_state.effect.tint.x == 0 && stdPalEffects_state.effect.tint.z == 0.75f);
    assert(stdPalEffects_state.effect.fade == 0.5f);
    stdPalEffects_GatherEffects();
    assert(stdPalEffects_state.effect.tint.x == 1 && stdPalEffects_state.effect.tint.z == 0.75f);
    stdPalEffects_GatherEffectsMasked(UINT32_MAX);
    assert(stdPalEffects_state.effect.tint.x == 0 && stdPalEffects_state.effect.fade == 1);
    assert(stdPalEffects_numEffectRequests == 3 && stdPalEffects_aEffects[31].isValid);
    puts("PASS: per-view effects are isolated, sparse global effects survive, unmasked behavior is retained");
}
'''.replace('GATHER', gather)
with tempfile.TemporaryDirectory() as temp:
    c = Path(temp) / 'test.c'
    exe = Path(temp) / 'test.exe'
    c.write_text(program)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',
                    str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
