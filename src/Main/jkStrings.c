#include "jkStrings.h"

#include "General/stdStrTable.h"
#include "Cog/jkCog.h"
#include "../jk.h"

static int jkStrings_bInitialized = 0;
static stdStrTable jkStrings_table;
#ifdef QOL_IMPROVEMENTS
static stdStrTable jkStrings_tableExt;
stdStrTable jkStrings_tableExtOver;
#endif // QOL_IMPROVEMENTS

#ifdef TARGET_XBOX
static wchar_t* jkStrings_GetXboxSetupString(const char *key)
{
    if (!key)
        return 0;

#define JK_XBOX_SETUP_STRING(name, value) if (!__strcmpi(key, name)) return (wchar_t*)value
    JK_XBOX_SETUP_STRING("GUI_FULLSUB", L"Full subtitles");
    JK_XBOX_SETUP_STRING("GUI_FULLSUB_HINT", L"Show all dialogue subtitles");
    JK_XBOX_SETUP_STRING("GUI_ROTATEOVERLAY", L"Rotate overlay map");
    JK_XBOX_SETUP_STRING("GUI_ROTATEOVERLAY_HINT", L"Rotate the overlay map with player view");
    JK_XBOX_SETUP_STRING("GUI_DISABLECUTSCENES", L"Disable cutscenes");
    JK_XBOX_SETUP_STRING("GUI_DISABLECUTSCENES_HINT", L"Skip cinematic video playback");
    JK_XBOX_SETUP_STRING("GUI_ADVANCED", L"Advanced");
    JK_XBOX_SETUP_STRING("GUI_ADVANCED_HINT", L"Advanced settings");
    JK_XBOX_SETUP_STRING("GUIEXT_FOV", L"FOV");
    JK_XBOX_SETUP_STRING("GUIEXT_FOV_HINT", L"Set field of view");
    JK_XBOX_SETUP_STRING("GUIEXT_FOV_VERTICAL", L"Vertical FOV");
    JK_XBOX_SETUP_STRING("GUIEXT_DISABLE_MISSION_CONFIRMATION", L"Skip mission confirm");
    JK_XBOX_SETUP_STRING("GUIEXT_DISABLE_MISSION_CONFIRMATION_HINT", L"Start missions without confirmation");
    JK_XBOX_SETUP_STRING("GUIEXT_INCONSISTENT_PHYS", L"Classic physics timing");
    JK_XBOX_SETUP_STRING("GUIEXT_INCONSISTENT_PHYS_HINT", L"Use original frame timing");
    JK_XBOX_SETUP_STRING("GUIEXT_CORPSE_DESPAWN", L"Keep corpses");
    JK_XBOX_SETUP_STRING("GUIEXT_CORPSE_DESPAWN_HINT", L"Prevent corpse despawn");
    JK_XBOX_SETUP_STRING("GUIEXT_50HZ_MIDAIR_PHYS", L"Classic midair physics");
    JK_XBOX_SETUP_STRING("GUIEXT_50HZ_MIDAIR_PHYS_HINT", L"Use 50Hz midair physics");
    JK_XBOX_SETUP_STRING("GUI_AUTOAIM", L"Auto aim");
    JK_XBOX_SETUP_STRING("GUI_AUTOAIM_HINT", L"Use assisted aiming");
    JK_XBOX_SETUP_STRING("GUI_CROSSHAIR", L"Crosshair");
    JK_XBOX_SETUP_STRING("GUI_CROSSHAIR_HINT", L"Show the aiming crosshair");
    JK_XBOX_SETUP_STRING("GUI_SABERCAM", L"Saber camera");
    JK_XBOX_SETUP_STRING("GUI_SABERCAM_HINT", L"Use lightsaber camera");
    JK_XBOX_SETUP_STRING("GUI_SINGLE", L"Solo");
    JK_XBOX_SETUP_STRING("GUI_MP", L"MP");
    JK_XBOX_SETUP_STRING("GUI_AUTOPICKUP", L"Auto pickup");
    JK_XBOX_SETUP_STRING("GUI_AUTOPICKUP_HINT", L"Automatically pick up items");
    JK_XBOX_SETUP_STRING("GUI_NODANGER", L"No danger");
    JK_XBOX_SETUP_STRING("GUI_NODANGER_HINT", L"Avoid dangerous weapon switches");
    JK_XBOX_SETUP_STRING("GUI_NOWEAKER", L"No weaker");
    JK_XBOX_SETUP_STRING("GUI_NOWEAKER_HINT", L"Avoid weaker weapon switches");
    JK_XBOX_SETUP_STRING("GUI_KEEPSABER", L"Keep saber");
    JK_XBOX_SETUP_STRING("GUI_KEEPSABER_HINT", L"Keep the lightsaber selected");
    JK_XBOX_SETUP_STRING("GUI_AUTOSWITCH", L"Auto switch");
    JK_XBOX_SETUP_STRING("GUI_AUTOSWITCH_HINT", L"Switch weapons automatically");
    JK_XBOX_SETUP_STRING("GUI_AUTORELOAD", L"Auto reload");
    JK_XBOX_SETUP_STRING("GUI_AUTORELOAD_HINT", L"Reload weapons automatically");
    JK_XBOX_SETUP_STRING("GUIEXT_SHOW_SABER_CROSSHAIR", L"Saber crosshair");
    JK_XBOX_SETUP_STRING("GUIEXT_SHOW_SABER_CROSSHAIR_HINT", L"Show crosshair with saber");
    JK_XBOX_SETUP_STRING("GUIEXT_SHOW_FIST_CROSSHAIR", L"Fist crosshair");
    JK_XBOX_SETUP_STRING("GUIEXT_SHOW_FIST_CROSSHAIR_HINT", L"Show crosshair with fists");
    JK_XBOX_SETUP_STRING("GUIEXT_DISABLE_WAGGLE", L"Disable weapon waggle");
    JK_XBOX_SETUP_STRING("GUIEXT_DISABLE_WAGGLE_HINT", L"Stop weapon waggle while moving");
    JK_XBOX_SETUP_STRING("GUIEXT_CROSSHAIR_SCALE", L"Crosshair scale");
    JK_XBOX_SETUP_STRING("GUIEXT_CROSSHAIR_SCALE_HINT", L"Adjust crosshair size");
    JK_XBOX_SETUP_STRING("GUIEXT_CUTSCENE_VOLUME", L"Cutscene volume");
    JK_XBOX_SETUP_STRING("GUIEXT_CUTSCENE_VOLUME_HINT", L"Set cutscene audio volume");
#undef JK_XBOX_SETUP_STRING

    return 0;
}
#endif

int jkStrings_Startup()
{
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__);
    
    // Added: clean reset
    _memset(&jkStrings_table, 0, sizeof(jkStrings_table));

    int result = stdStrTable_Load(&jkStrings_table, "ui\\jkstrings.uni");

    // Added: OpenJKDF2 i8n
#ifdef QOL_IMPROVEMENTS
    _memset(&jkStrings_tableExtOver, 0, sizeof(jkStrings_tableExtOver));
    _memset(&jkStrings_tableExt, 0, sizeof(jkStrings_tableExt));

    stdStrTable_Load(&jkStrings_tableExtOver, "ui\\openjkdf2_i8n.uni");
    stdStrTable_Load(&jkStrings_tableExt, "ui\\openjkdf2.uni");
#endif // QOL_IMPROVEMENTS

    jkStrings_bInitialized = 1;
    return result;
}

void jkStrings_Shutdown()
{
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__);

    // Added: OpenJKDF2 i8n
#ifdef QOL_IMPROVEMENTS
    stdStrTable_Free(&jkStrings_tableExtOver);
    stdStrTable_Free(&jkStrings_tableExt);
    _memset(&jkStrings_tableExtOver, 0, sizeof(jkStrings_tableExtOver));
    _memset(&jkStrings_tableExt, 0, sizeof(jkStrings_tableExt));
#endif

    stdStrTable_Free(&jkStrings_table);
    jkStrings_bInitialized = 0;

    // Added: clean reset
    _memset(&jkStrings_table, 0, sizeof(jkStrings_table));
}

wchar_t* jkStrings_GetUniString(const char *key)
{
    wchar_t *result; // eax

#ifdef TARGET_XBOX
    result = jkStrings_GetXboxSetupString(key);
    if ( result )
        return result;
#endif

    // Added: Allow openjkdf2_i8n.uni to override everything
#ifdef QOL_IMPROVEMENTS
    result = stdStrTable_GetUniString(&jkStrings_tableExtOver, key);
    if ( !result )
#endif
    result = stdStrTable_GetUniString(&jkStrings_table, key);
    if ( !result )
        result = stdStrTable_GetUniString(&jkCog_strings, key);
#ifdef QOL_IMPROVEMENTS
    if ( !result )
        result = stdStrTable_GetUniString(&jkStrings_tableExt, key);
#endif
    return result;
}

wchar_t* jkStrings_GetUniStringWithFallback(const char *key)
{
    wchar_t *result; // eax

#ifdef TARGET_XBOX
    result = jkStrings_GetXboxSetupString(key);
    if ( result )
        return result;
#endif

    // Added: Allow openjkdf2_i8n.uni to override everything
#ifdef QOL_IMPROVEMENTS
    result = stdStrTable_GetUniString(&jkStrings_tableExtOver, key);
    if ( !result )
#endif
    result = stdStrTable_GetUniString(&jkStrings_table, key);

    // Added: OpenJKDF2 i8n -- stdStrTable_GetStringWithFallback must always be the last lookup
    // because it always succeeds.
#ifdef QOL_IMPROVEMENTS
    if ( !result )
        result = stdStrTable_GetUniString(&jkCog_strings, (char *)key);
    if ( !result )
        result = stdStrTable_GetStringWithFallback(&jkStrings_tableExt, key);
#else
        if ( !result )
        result = stdStrTable_GetStringWithFallback(&jkCog_strings, (char *)key);
#endif
    return result;
}

int jkStrings_unused_sub_40B490()
{
    return 1;
}
