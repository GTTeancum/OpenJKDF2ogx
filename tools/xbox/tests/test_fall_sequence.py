"""Exercise production fall detection/fade/prompt logic without an emulator."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
source = (ROOT / 'src/Gameplay/sithPlayer.c').read_text()
body = source.split('        if ( !v3->attach_flags', 1)[1].split(
    'void sithPlayer_debug_loadauto', 1)[0]
body = 'if ( !v3->attach_flags' + body.rsplit('}', 2)[0]
program = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#undef assert
#define assert(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
#define SITH_AF_FALLING_TO_DEATH 1
#define SITH_TF_DEAD 2
#define SITH_MT_PHYSICS 3
#define SITH_SECTOR_FALLDEATH 0x40
#define DEBUGFLAG_NOCLIP 4
typedef struct { int flags; } Sector;
typedef struct {
    int attach_flags,moveType,thingflags;
    struct { int typeflags; } actorParams;
    struct { struct { float z; } vel; } physicsParams;
    Sector *sector;
} Thing;
typedef struct { float fade; } Effect;
static Thing player, *sithPlayer_pLocalPlayerThing = &player;
static int g_debugmodeFlags, sithNet_isMulti, sithCamera_cameras[2];
static int *camera, focusCount, stops, deathPackets;
static unsigned int sithTime_curMs, sithControl_death_msgtimer;
static void sithCamera_SetCameraFocus(int *cam, Thing *thing, int unused) { ++focusCount; }
static void sithCamera_SetCurrentCamera(int *cam) { camera = cam; }
static void sithPhysics_ThingStop(Thing *thing) { ++stops; }
static void sithPlayer_HandleSentDeathPkt(Thing *thing) { ++deathPackets; }
static void tick(Thing *v3, Effect *pPalEffect, float a2) { int v14; BODY }
int main(void) {
    Sector pit = {SITH_SECTOR_FALLDEATH};
    Effect effect = {1};
    player.moveType = SITH_MT_PHYSICS;
    player.sector = &pit;
    player.physicsParams.vel.z = -4;
    g_debugmodeFlags = DEBUGFLAG_NOCLIP;
    tick(&player, &effect, 0.02f);
    assert(player.thingflags == 0 && effect.fade == 1);
    g_debugmodeFlags = 0;
    for (int i = 0; i < 72; i++) {
        sithTime_curMs += 20;
        tick(&player, &effect, 0.02f);
        if (i < 71) assert(camera == &sithCamera_cameras[1]);
    }
    assert(effect.fade <= 0 && player.thingflags & SITH_TF_DEAD);
    assert(!(player.actorParams.typeflags & SITH_AF_FALLING_TO_DEATH));
    assert(camera == &sithCamera_cameras[0] && stops == 1 && focusCount == 3);
    assert(sithControl_death_msgtimer == 4440 && deathPackets == 0);
    /* A corpse can keep descending through the pit after its fade ends.
     * That must not restart the camera sequence or postpone the prompt. */
    for (int i = 0; i < 200; i++) {
        player.physicsParams.vel.z = -4;
        sithTime_curMs += 20;
        tick(&player, &effect, 0.02f);
    }
    assert(stops == 1 && focusCount == 3);
    assert(sithControl_death_msgtimer == 4440);
    assert(!(player.actorParams.typeflags & SITH_AF_FALLING_TO_DEATH));
    sithNet_isMulti = 1;
    effect.fade = 0.01f;
    player.actorParams.typeflags = SITH_AF_FALLING_TO_DEATH;
    tick(&player, &effect, 0.02f);
    assert(deathPackets == 1 && stops == 1 && sithControl_death_msgtimer == 4440);
    puts("PASS: authored pit detection, noclip exclusion, camera change, 1.44s fade, prompt delay, multiplayer death path");
}
'''.replace('BODY', body)
with tempfile.TemporaryDirectory() as temp:
    c = Path(temp) / 'test.c'
    exe = Path(temp) / 'test.exe'
    c.write_text(program)
    subprocess.run([shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe',
                    str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
