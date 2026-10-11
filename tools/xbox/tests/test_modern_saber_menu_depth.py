"""Static guards for Modern saber menu placement, POV depth and RGB lighting."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[3]

setup = (root / "src/Gui/jkGUISetup.c").read_text()
gameplay = (root / "src/Gui/jkGUIGameplay.c").read_text()
player = (root / "src/World/jkPlayer.c").read_text()
strings = (root / "src/Main/jkStrings.c").read_text()
uni = (root / "resource/ui/openjkdf2.uni").read_text()
smoke = (root / "tools/xbox_xemu_smoke.ps1").read_text()
main = (root / "src/Main/jkMain.c").read_text()

# The user-facing toggle belongs in Setup > Display with the exact requested text.
assert '"GUI_MODERN_SABER_RENDERING"' in setup
assert '"GUI_MODERN_SABER_RENDERING"           0 "Modern saber rendering"' in uni
assert 'JK_XBOX_SETUP_STRING("GUI_MODERN_SABER_RENDERING", L"Modern saber rendering")' in strings
assert '"GUI_MODERN_SABER"' not in gameplay

# Display save/load owns modernSaber now; Gameplay should not save or reload it.
show_display = re.search(r"static int jkGuiSetupXbox_ShowDisplay\(void\).*?\n}\n\n#endif", setup, re.S).group(0)
assert 'jkPlayer_bModernSaber = jkGuiSetupXboxDisplay_buttons[15].selectedTextEntry != 0;' in show_display
assert 'wuRegistry_SaveInt("modernSaber", jkPlayer_bModernSaber != 0);' in show_display
assert 'jkGuiSetupXboxDisplay_buttons[15].selectedTextEntry = jkPlayer_bModernSaber != 0;' in show_display
assert 'modernSaber' not in gameplay

# The native menu probe should target Display, not Gameplay, and look for the new key.
assert 'ModernSaberMenuProbe' in smoke
assert '"7 102"' in smoke
assert 'strcmp(target->origStr, "GUI_MODERN_SABER_RENDERING")' in setup

# First-person modern saber is deferred until after the POV model has written
# hand depth. Then the saber reads both world depth and hand depth without
# writing depth, so hands no longer punch through the blade visually.
assert 'int xboxModernPovSaber = 0;' in player
assert re.search(
    r"if \(jkPlayer_bModernSaber\)\s*\{\s*xboxModernPovSaber = 1;\s*\}\s*else\s*#endif\s*jkSaber_Draw\(&viewMat\);",
    player,
    re.S,
)
order = [
    'if (xboxModernPovSaber)\n            rdSetZBufferMethod(RD_ZBUFFER_NOREAD_WRITE);',
    'rdThing_Draw(&playerThings[playerThingIdx].povModel, &viewMat);',
    'rdCache_Flush();',
    'if (xboxModernPovSaber)\n        {\n            rdSetZBufferMethod(RD_ZBUFFER_READ_NOWRITE);',
    'jkSaber_Draw(&viewMat);',
    'rdCache_Flush();',
    'rdSetZBufferMethod(RD_ZBUFFER_NOREAD_NOWRITE);',
]
pos = -1
for token in order:
    nxt = player.index(token, pos + 1)
    assert nxt > pos
    pos = nxt

# Proof fixtures keep ordinary ModernSaberProbe behavior unchanged, provide a
# dark third-person lighting proof, and provide a first-person POV depth proof.
assert 'ModernSaberProof' in smoke
assert 'ModernSaberPovProof' in smoke
assert '"1 1"' in smoke
assert '"1 2"' in smoke
assert 'int proofMode = 0;' in main
assert 'jkPlayer_setSaberCam = proofMode == 2 ? 0 : 1;' in main
assert 'proofMode == 1' in main
assert 'proofMode != 2' in main
assert 'proof=%d requested saber camera=%s' in main
assert 'ambientLight = 0.015f;' in main

# MotS/RGB lighting is globally compiled and modern saber dynamic light uses RGB
# for both world vertices and 3DO/model vertices.
engine_config = (root / 'src/engine_config.h').read_text()
sith_render = (root / 'src/Engine/sithRender.c').read_text()
rd_model3 = (root / 'src/Primitives/rdModel3.c').read_text()
assert '#define JKM_LIGHTING' in engine_config
assert 'if (jkPlayer_bModernSaber) mode = 1;' in sith_render
assert 'sithRender_SetDynamicLightColor(light, thing ? thing->actorParams.field_1A8 : 0x40FF40);' in sith_render
assert 'verticesDynamicLightR[idx] += contribution * (flex_t)((tmpLights[i]->color >> 16) & 0xFF) / 255.0;' in sith_render
assert 'verticesDynamicLightG[idx] += contribution * (flex_t)((tmpLights[i]->color >> 8) & 0xFF) / 255.0;' in sith_render
assert 'verticesDynamicLightB[idx] += contribution * (flex_t)(tmpLights[i]->color & 0xFF) / 255.0;' in sith_render
assert 'rdModel3_CalcModernDynamicRGB' in rd_model3
assert 'vertexSrc.paDynamicLightR = rdModel3_modernDynamicR;' in rd_model3

# Third-person/world model sabers are rendered by the weapon render hook, not
# jkPlayer_DrawPov. The player model and hand/hilt mesh must flush into depth
# before Modern Saber draws read-only against them.
third_person_order = [
    'ret = rdThing_Draw(&pThing->rdthing, &pThing->lookOrientation);',
    'if (jkPlayer_bModernSaber\n        && sithRender_weaponRenderHandle',
    'rdCache_Flush();',
    'sithRender_weaponRenderHandle(pThing);',
]
pos = -1
for token in third_person_order:
    nxt = sith_render.index(token, pos + 1)
    assert nxt > pos
    pos = nxt
assert player.count('rdSetZBufferMethod(RD_ZBUFFER_READ_NOWRITE);') >= 2
assert player.count('rdSetZBufferMethod(RD_ZBUFFER_READ_WRITE);') >= 1
print('PASS: Modern saber toggle, POV hand depth, explicit proof modes, and RGB dynamic light paths are guarded')
