"""Run the production damage-tint dispatcher with distinct local/remote players."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
source = (ROOT / 'src/Platform/Xbox/xbox_splitscreen.c').read_text()
body = 'void xboxSplitScreen_AddDamageTint' + source.split(
    'void xboxSplitScreen_AddDamageTint', 1)[1].split('void xboxSplitScreen_TickPlayer', 1)[0]
actor = (ROOT / 'src/World/sithActor.c').read_text()
assert 'xboxSplitScreen_AddDamageTint(sender, amount * 0.04);' in actor
program = r'''
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
typedef float flex_t;
typedef struct { int id; } sithThing;
typedef struct { sithThing *playerThing; int palEffectsIdx1; } sithPlayerInfo;
typedef struct { struct { float x,y,z; } tint; } stdPalEffect;
static sithThing things[5], *sithPlayer_pLocalPlayerThing = &things[0];
static sithPlayerInfo jkPlayer_playerInfos[5];
static stdPalEffect effects[8];
static int g_xboxSplitScreenEnabled = 1, g_xboxSplitScreenLocalCount = 2;
static int fallbackCalls;
static int xboxSplitScreen_GetPlayerIndexForSlot(int slot) { return slot + 1; }
static stdPalEffect *stdPalEffects_GetEffectPointer(int idx) { CHECK(idx>=0 && idx<8); return &effects[idx]; }
static float stdMath_Clamp(float x, float lo, float hi) { return x<lo?lo:(x>hi?hi:x); }
static void sithPlayer_AddDynamicTint(float r, float g, float b) { ++fallbackCalls; }
BODY
int main(void) {
    jkPlayer_playerInfos[1].playerThing = &things[0];
    jkPlayer_playerInfos[1].palEffectsIdx1 = 2;
    jkPlayer_playerInfos[2].playerThing = &things[1];
    jkPlayer_playerInfos[2].palEffectsIdx1 = 6;
    effects[6].tint.y = 0.25f;
    effects[6].tint.z = 0.5f;
    /* P2 is damaged while P1 remains the current gameplay context. */
    xboxSplitScreen_AddDamageTint(&things[1], 0.75f);
    CHECK(effects[6].tint.x == 0.75f && effects[2].tint.x == 0);
    CHECK(effects[6].tint.y == 0.25f && effects[6].tint.z == 0.5f);
    CHECK(sithPlayer_pLocalPlayerThing == &things[0] && fallbackCalls == 0);
    xboxSplitScreen_AddDamageTint(&things[1], 0.75f);
    CHECK(effects[6].tint.x == 1);
    sithPlayer_pLocalPlayerThing = &things[1];
    xboxSplitScreen_AddDamageTint(&things[0], 0.25f);
    CHECK(effects[2].tint.x == 0.25f && effects[6].tint.x == 1);
    xboxSplitScreen_AddDamageTint(&things[3], 1);
    CHECK(effects[2].tint.x == 0.25f && effects[6].tint.x == 1 && fallbackCalls == 0);
    jkPlayer_playerInfos[3].playerThing = &things[2];
    jkPlayer_playerInfos[3].palEffectsIdx1 = 3;
    jkPlayer_playerInfos[4].playerThing = &things[4];
    jkPlayer_playerInfos[4].palEffectsIdx1 = 5;
    g_xboxSplitScreenLocalCount = 4;
    xboxSplitScreen_AddDamageTint(&things[2], 0.5f);
    xboxSplitScreen_AddDamageTint(&things[4], 0.75f);
    CHECK(effects[3].tint.x == 0.5f && effects[5].tint.x == 0.75f);
    CHECK(effects[2].tint.x == 0.25f && effects[6].tint.x == 1);
    g_xboxSplitScreenEnabled = 0;
    xboxSplitScreen_AddDamageTint(&things[0], 1);
    CHECK(fallbackCalls == 0);
    xboxSplitScreen_AddDamageTint(&things[1], 1);
    CHECK(fallbackCalls == 1);
    puts("PASS: recipient-owned damage tint, clamping, remote exclusion, unchanged context and single-player fallback");
}
'''.replace('BODY', body)
with tempfile.TemporaryDirectory() as temp:
    c = Path(temp) / 'test.c'
    exe = Path(temp) / 'test.exe'
    c.write_text(program)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',
                    str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
